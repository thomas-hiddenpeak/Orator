# Spec 010 — Plan (Speaker Identity)

> HOW the speaker-identity layer is built: architecture, data flow, threading,
> phasing, and risks. Requirements/WHY are in `spec.md`.

## 1. Architecture (where it lives)

Speaker-id is a **post-diarization stage inside the diarization pipeline**
(Constitution Art. III decision, see spec R8). The diar pipeline becomes:

```
audio ─► Sortformer ─► local speaker segments (spk0..3, times, conf)
                              │
                              ▼
                  [SpeakerIdentityStage]   (new, this spec)
        clean-segment gate ─► TitaNet Embed ─► per-local-speaker voiceprint
                              │
                              ▼
              SpeakerDatabase 1:N cosine match (persistent)
                  match ≥ τ → known global id      (discover / identify)
                  match <  τ → Enroll new global id (auto-register)
                  (Phase H option: later reset-session slots may remain
                   local-only when the match is not strong enough)
                              │
                              ▼
              local-label → global-id map (revisable)
                              │
                              ▼
        publish diar segments carrying the GLOBAL id (protocol topic)
                              ▼
        ComprehensiveTimeline maps speaker turns to the global id
```

ASR / VAD pipelines are untouched and remain independent. The model runs on the
diar worker thread (diar rt ≈ 40–55×: ample headroom; the ASR critical path is
unaffected). GPU scheduling follows the diar pipeline's existing stream/lock.

## 2. Components

- **`model::TitaNetEmbedder` (`ISpeakerEmbedder`)** — pure C++/CUDA TitaNet-L:
  mel front-end (reuse `feature::MelSpectrogram`) → 1D depthwise-separable conv
  encoder (QuartzNet/ContextNet blocks) → attentive statistics pooling → linear
  → L2-normalized 192-d embedding. Weights bf16 (load-time upcast as elsewhere).
- **`model::SpeakerDatabase` (existing, `ISpeakerRegistry`)** — wired in:
  Enroll / Match / Update / Save / Load. Add a `id -> display_name` sidecar +
  a `SetDisplayName(id, name)` hook (R6); no UI.
- **`pipeline::SpeakerIdentityStage`** — drives clean-segment gating, embedding
  accumulation per local speaker, matching/enrolling, the revisable
  local→global map, and publishing the enriched diar segments.

## 3. Data flow / threading

- Runs on the **diarization worker thread**; uses the diar pipeline's CUDA
  stream. No new cross-pipeline coupling (Art. III): it consumes diar's own
  in-pipeline segment output + the shared audio buffer, and publishes the
  enriched segments through the existing diar protocol topic.
- **Streaming embedding**: accumulate a local speaker's CLEAN audio; embed once
  ≥ min-duration; refine (moving-average `Update`) as more accumulates. The
  local→global decision is **tentative early, revised later** → the comprehensive
  timeline receives a revision when an identity firms up or flips.

## 4. Phasing

- **Phase A — Voiceprint model + oracle (largest).**
  A1 Acquire `titanet_large` (NeMo) + convert weights → safetensors (tools/).
  A2 NeMo Python oracle: reference embeddings for test spans (tools/reference/).
  A3 Implement `TitaNetEmbedder` in C++/CUDA (encoder + ASP pooling + linear).
  A4 `test_titanet`: C++ embeddings vs oracle within tolerance.
- **Phase B — Identity stage.**
  B1 Clean-segment gate (VAD + single-speaker + duration + conf).
  B2 Per-local-speaker accumulation + embed + DB match/enroll.
  B3 Revisable local→global map.
- **Phase C — Injection.** Publish global ids on the diar topic; map them into
  the comprehensive view (revisable). Naming hook (R6).
- **Phase D — Persistence + tuning.** Save/Load wiring; threshold τ + min-dur
  tuning; 600 s real-WS validation (re-id across runs; auto-register).
- **Phase H — Conservative cross-session experiment.** Keep the default open-set
  behaviour, but expose all speaker-id policy thresholds in TOML and add an
  opt-in mode where reset-session local slots need multiple clean references
  before re-identification; unmatched later-session slots can remain local-only
  instead of immediately creating a new global id.

Each phase gates on build clean + ctest + (A) oracle + (D) real-WS 600 s.
Phase H additionally gates on full-length real WebSocket validation and
complete context-semantic comparison under the Test Review Protocol before any
accuracy claim is accepted. No code, script, test, metric, formula, query, or
algorithm may assign judgments, total accuracy, rank/select a candidate, or
issue that verdict.

## 5. Risks / open details (refined while building)

- **Clean-segment definition** (R2): exact thresholds (min-dur, overlap rule,
  conf cutoff) tuned empirically in Phase B/D.
- **Threshold τ + enroll policy**: false-merge (two people → one id) vs
  false-split (one person → two ids); tune on 600 s; moving-average update.
- **Reset-session identity policy**: a conservative policy reduces stable wrong
  identities by allowing local-only labels when evidence is weak, but may leave
  real new speakers unresolved. It is therefore opt-in and must be evaluated on
  the full 60 min stream before becoming a default.
- **Cold start**: first speaker of a session has no prior — always auto-register.
- **Overlapped speech**: excluded from embedding (R2) — may leave some turns
  with a tentative/low-confidence id.
- **TitaNet reimplementation effort**: depthwise-separable 1D conv + ASP is the
  main new kernel work; reuse existing conv/GEMM where possible.
- **Privacy** (non-goal here but noted): voiceprints are biometric; storage and
  consent are a later governance item.

## 6. Weight layout (A1 done — blueprint for A3)

Source: `nvidia/speakerverification_en_titanet_large` (NeMo `.nemo`, 102 MB).
Converted to `models/speaker/titanet_large.safetensors` (108 tensors, F32, with
`tools/convert/convert_nemo_to_safetensors.py`; config extracted to
`models/speaker/titanet_large_config.yaml`). The `decoder.final.weight`
[16681, 192] classifier head is **inference-irrelevant** (we stop at the 192-d
embedding). Inference forward:

Reproduce (weights are gitignored under `models/`, regenerate locally):
```
python -c "from huggingface_hub import hf_hub_download as d; \
  d('nvidia/speakerverification_en_titanet_large', \
    'speakerverification_en_titanet_large.nemo', local_dir='models/speaker')"
python tools/convert/convert_nemo_to_safetensors.py \
  models/speaker/speakerverification_en_titanet_large.nemo \
  models/speaker/titanet_large.safetensors
```

Front-end: `AudioToMelSpectrogramPreprocessor` — 16 kHz, 80 mel, window 25 ms,
stride 10 ms, n_fft 512, Hann, `normalize: per_feature` (per-utterance per-mel
mean/std normalization — NOTE: differs from ASR mel; verify against oracle).

A3 mel detail (found while building): the existing `feature::MelSpectrogram`
(diar) is n_fft 512 / 16 kHz with configurable `n_mels`, so its FFT + filterbank
framework is reusable, but TitaNet's front-end additionally needs (vs the diar
mel): 80-mel **slaney** filterbank, **preemphasis 0.97**, log, and **per_feature
normalization** (subtract per-utterance per-mel mean, divide by std). These are
the concrete deltas to add for A3; validate the produced mel against
`models/reference/speaker/span*_mel.f32` before trusting the encoder.

Encoder `ConvASREncoder` (ContextNet, all `separable: true`, `se: true`,
`relu`), 5 blocks `encoder.encoder.{0..4}`:
- Each conv = depthwise (`mconv.N.conv.weight` shape `[C,1,k]`) + pointwise
  (`mconv.N+1.conv.weight` shape `[Cout,Cin,1]`) + BN (`mconv.N+2`).
- Block 0 (prolog): k=3, →1024, repeat 1, no residual, SE `mconv.3.fc.{0,2}`
  (squeeze 1024→128→1024).
- Blocks 1/2/3: 1024 ch, k=7/11/15, repeat 3 (three dw-sep conv+BN+relu sub-
  units at mconv idx 0-2 / 5-7 / 10-12), SE at `mconv.13`, residual
  `res.0.0`(pointwise) + `res.0.1`(BN) added before the final relu.
- Block 4 (epilog): pointwise k=1, →3072, repeat 1.

Decoder `SpeakerDecoder` attentive statistics pooling (`pool_mode: attention`):
- Attention: `_pooling.attention_layer.0.conv_layer` [128, 9216, 1] — input is
  per-frame encoder out (3072) concatenated with broadcast global mean(3072)+
  std(3072) = 9216 → 128, BN, tanh, then `attention_layer.2` [3072,128,1] →
  per-frame per-channel attention logits → softmax over time.
- Pool: attention-weighted mean(3072) ⊕ std(3072) = 6144.
- Embed: `emb_layers.0.0` BN(6144) → `emb_layers.0.1` linear [192,6144,1] →
  192-d. (NeMo returns the pre-final-BN embedding; **L2-normalize** for cosine.)
