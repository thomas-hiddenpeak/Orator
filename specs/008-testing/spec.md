# Spec 008 — System-Level Testing & Evaluation

- **Feature**: `008-testing`
- **Status**: Draft
- **Created**: 2026-06-22
- **Owner**: project owner
- **Constitution**: v1.4.0

> WHAT system-level testing means, HOW it is executed, and WHY the evaluation
> methodology is semantic comparison against a reference transcript rather than
> automated metrics.

---

## 1. Summary

Orator has 38 C++ unit tests and 1 Python integration test (`test_integration.py`)
that validate individual components and basic connectivity. These do **not**
substitute for **system-level evaluation**: running the full pipeline (Diarization
+ ASR + VAD) against real audio and assessing the business output.

This spec defines the system-level testing methodology:

- **Audio**: `test.mp3` — multi-speaker business meeting, 4 participants (朱杰,
  徐子景, 石一, 唐云峰), duration ~3615s
- **Reference**: `test.txt` — human-annotated transcript with speaker labels,
  timestamps, and semantic text
- **Speed**: 1× real-time (simulates microphone input — no burst pushing)
- **Scope**: Three durations — 120s (L1), 600s (L2), Full (L3)
- **Evaluation**: AI-conducted semantic comparison against reference
- **Criteria**: ASR accuracy, speaker attribution accuracy, timing precision,
  comprehensive view coherence

---

## 2. Testing Principles

### 2.1 Realistic Input

All system-level tests MUST push audio at **1× real-time** speed (1 second of
audio per 1 second of wall time). The server processes the stream the same way it
would process a live microphone input. No burst/batch pushing is permitted — this
was the root cause of false negatives in earlier testing (stale server processes
and buffer overruns).

**Rationale**: The system is designed for real-time streaming. Testing at higher
speeds or in batch mode introduces artifacts (buffer growth, GPU serialization
starvation, stale server connections) that do not occur in production.

### 2.2 Full Pipeline

All system-level tests MUST activate all three pipelines:

| Pipeline | Model | Required |
|---|---|---|
| Diarization | `models/sortformer_4spk_v2.safetensors` | Yes |
| ASR | `models/asr/Qwen/Qwen3-ASR-1.7B` | Yes |
| VAD | `models/vad/silero_vad.safetensors` | Yes |

Running with only a subset of pipelines is permitted for debugging but does **not**
constitute a system-level test.

### 2.3 Defined Test Levels

| Level | Duration | Audio Span | Reference Lines | Purpose |
|---|---|---|---|---|
| L1 | 120s | `test.mp3[0:00-2:00]` | 1–22 | Daily regression check |
| L2 | 600s | `test.mp3[0:00-10:00]` | 1–90 | Weekly quality gate |
| L3 | Full | `test.mp3[0:00-60:15]` | All | Release qualification |

A release MUST pass L1 and L2. L3 is required before any production deployment.

### 2.4 Clean Server State

Every test MUST start with a **fresh server process** on a **known-clean port**.
The test harness (`OratorTestHarness`) enforces this by:

1. Killing any process on the target port (`fuser -k`)
2. Starting a new `orator_ws` instance with all three model paths
3. Waiting for TCP readiness (socket connect with timeout)
4. Verifying the ready message contains expected model configuration

Connection to a stale or orphaned server process is a known false-negative vector
and MUST be guarded against.

---

## 3. Reference Document

The reference file `test.txt` is a manually transcribed, speaker-labeled,
timestamped transcript of `test.mp3`. Its format:

```
HH:MM:SS SpeakerName
transcript text...
```

- **Speakers**: 朱杰, 徐子景, 石一, 唐云峰
- **Content**: Business meeting about equity distribution, corporate structure,
  and fundraising strategy
- **Language**: Mandarin Chinese with code-switching to English (proper nouns
  like "PMP", "DD", "TS", "GitHub", "iPhone")
- **Duration**: ~3615s (60 min 15 sec)
- **Style**: Conversational — overlapping speech, interruptions, restarts,
  fillers (嗯, 额, 就是, 对吧)

The reference is NOT a verbatim dictation — it is an **editorial transcript**
that normalizes fillers while preserving speaker identity, semantic content,
and utterance timing. The ASR output is expected to match at the **semantic**
level, not the character level.

---

## 4. Evaluation Dimensions

### 4.1 ASR Text Accuracy

Comparison against reference at the **utterance level**:

| Criterion | Meaning |
|---|---|
| Semantic equivalence | Does the output convey the same meaning as the reference? |
| Entity preservation | Are proper names, numbers, and domain terms correct? |
| Hallucination | Are there fabricated words or phrases not in the audio? |
| Omission | Are there content gaps where the reference has speech? |

**Not evaluated**: exact character match, filler word presence, punctuation.

### 4.2 Speaker Attribution Accuracy

For each comprehensive view entry:

| Criterion | Meaning |
|---|---|
| Speaker match | Does the assigned speaker match the reference speaker? |
| Segment boundary | Does the turn boundary align with a reference speaker change? |
| Unknown rate | What fraction of audio is attributed to `speaker_-1` (unknown)? |

### 4.3 Timing Precision

For each ASR utterance with a corresponding reference timestamp:

| Criterion | Tolerance |
|---|---|
| Start time offset | ±3s from reference `HH:MM:SS` |
| End time offset | ±3s from reference |
| Utterance ordering | Must match reference order |

### 4.4 VAD Alignment

| Criterion | Meaning |
|---|---|
| Speech coverage | Do VAD segments cover the same regions as ASR utterances? |
| Silence detection | Are long pauses correctly identified as non-speech? |

### 4.5 System Health

| Criterion | Meaning |
|---|---|
| wall_clock_ok | Server clock drift detection must be True |
| ASR compute | Must be > 0 (ASR processed audio) |
| Diarization compute | Must be > 0 (Sortformer ran) |
| VAD compute | Must be > 0 (Silero ran) |

---

## 5. Execution Protocol

### 5.1 Setup

```bash
# Clean start
pkill -f orator_ws
./build/orator_ws 8765 \
  "models/sortformer_4spk_v2.safetensors" \
  "models/asr/Qwen/Qwen3-ASR-1.7B" &
sleep 15  # wait for model loading

# Verify ready
cat /tmp/orator_server.log | grep "pipelines ready"
```

### 5.2 Audio Push

Push audio at 1× real-time in 1-second chunks (16000 bytes each):

```python
t0 = time.time()
with open('test.mp3', 'rb') as f:
    pcm = convert_to_pcm(f, duration=120)  # L1
    sent = 0
    while sent < len(pcm):
        ws.send(pcm[sent:sent+16000], opcode=0x02)
        sent += 16000
        elapsed = time.time() - t0
        if elapsed < sent / 32000:
            time.sleep(sent / 32000 - elapsed)
```

### 5.3 Collection

Push flush + end, collect timeline within a generous timeout:

```python
ws.send(json.dumps({'end': True}))
timeout = max(duration * 1.5, 120)  # seconds
tl = collect_timeline(ws, timeout)
```

### 5.4 Evaluation

The evaluator (AI) reads `test.txt` and the timeline JSON, then produces a
structured report covering all dimensions in §4. The evaluation is **manual**
(conducted by the AI) — not scripted — because semantic comparison requires
understanding of context, reference normalization, and domain knowledge.

---

## 6. Reporting Format

Each test level produces a structured report:

```markdown
## L1: 120s Evaluation

### System Health
- wall_clock_ok: ✓
- ASR: N utterances, X.XXs compute
- Diarization: N segments, X.XXs compute
- VAD: N segments, X.XXs compute

### ASR Text Accuracy
- Semantic match rate: N/N utterances (XX%)
- Entities correct: ...
- Hallucinations: ...
- Omissions: ...

### Speaker Attribution
- Correct speaker: N/N entries
- Speaker_-1 fraction: XX%
- Notable boundary errors: ...

### Timing
- Average start offset: X.Xs
- Average end offset: X.Xs
- Ordering errors: N

### Verdict: PASS / CONDITIONAL / FAIL
```

---

## 7. Relationship to Existing Tests

| Test Type | Coverage | When |
|---|---|---|
| C++ unit tests (38) | Component-level | Every commit |
| `test_integration.py` | Connectivity + basic pipeline | Every commit (CI) |
| L1 (120s) | Core business quality | Weekly / before merge |
| L2 (600s) | Extended stability | Before release |
| L3 (full) | Production qualification | Before deployment |

The C++ tests and integration test are automated in CI. L1–L3 are
AI-conducted evaluations that require human-readable output and are triggered
on demand.

---

## 8. Glossary

| Term | Definition |
|---|---|
| Reference transcript | Human-annotated `test.txt` with speaker labels and timestamps |
| Semantic equivalence | Two texts convey the same meaning despite different wording |
| Comprehensive view | Speaker-attributed ASR text from `ComprehensiveTimeline::SplitTextByDiar()` |
| wall_clock_ok | Server flag indicating no clock drift between session start and end |
| 1× real-time | Pacing where 1 second of audio takes 1 second of wall time |
| Stale server | A surviving `orator_ws` process from a previous test that is still bound to the test port |
