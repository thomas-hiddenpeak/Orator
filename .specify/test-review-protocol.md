# Test Review Protocol

## Overview

This document defines the default AI-conducted manual-style test evaluation protocol for Orator. It is not a feature spec but an engineering governance artifact (process) that applies across all feature specs (Specs 001–005+).

The protocol exists to ensure consistent, context-aware quality judgments for real streaming regressions, supplementing the automated gates (ctest, oracle metrics).

## Scope

Applies to:
- WebSocket real-time ASR + diarization joint testing
- Offline full-segment ASR testing
- Any regression test where reference text is known and semantic + speaker accuracy need human-style judgment

Does not replace:
- Unit test gates (`ctest`)
- Oracle numeric comparisons
- Runtime metric measurement

## 1. Required Review Inputs

Each review MUST read:

1. **Reference text** (gold)
   - Example: `test.txt`
2. **Model outputs**
   - timeline JSON
   - incremental ASR events
   - diarization track
3. **Run metadata**
   - Audio duration
   - Wall time
   - ASR/diar compute time
   - Real-time factor (RTF)
4. **Key run logs**
   - Crash/no-result indicators
   - Abnormal warnings
5. **AI manual comparison**
   - Read reference and model output
   - Perform context-aware per-segment comparison
   - Do NOT derive conclusions from script metrics alone
   - Do NOT substitute "recommend manual review" for actual review

## 2. Output Structure

Every test MUST produce 4 sections:

### 2.1 Test Summary Table

| Item | Content |
|---|---|
| Test type | Example: 120s WS joint streaming |
| Input audio | File / duration |
| Reference text | Gold text used |
| Run result | Success / Failure / Crash |
| Wall time | Measured seconds |
| Stream RTF | Total real-time factor |
| ASR RTF | ASR only RTF |
| Diar RTF | Diarization only RTF |
| Subjective conclusion | One-sentence summary |

### 2.2 Segmented Manual Review Table

Segment by natural speaker-turn or content change (AI-decided, not pre-cut).

| Time span | Reference summary | System output summary | ASR semantic | Speaker eval | Issues |
|---|---|---|---|---|---|

Field definitions:

- **Time span**: Example: 00:00–00:45
- **Reference summary**: Who spoke, what was said
- **System output summary**: Actual ASR + diar performance from timeline
- **ASR semantic**: Accurate / Mostly accurate / Partly distorted / Clearly wrong
- **Speaker eval**: Accurate / Mostly accurate / Minor confusion / Major confusion / Unusable
- **Issues**: Examples: missing words, reversed meaning, merging, missing speaker, speech overlap, lag

### 2.3 Two Core Conclusions

#### A. ASR Semantic Accuracy

Human-scored semantic fidelity, NOT character error rate substitute.

Rubric:

- **90–95%**: Semantic consistency high; minor colloquial variance; no fact-understanding impact
- **80–89%**: Main semantic correct; local phrase distortion; stable content understanding
- **70–79%**: Gist comprehensible; key sentences have clear errors or omissions
- **60–69%**: Topic visible; many detail errors
- **<60%**: Semantic not reliably usable

Focus on:

1. Is discussion topic preserved?
2. Are key numbers, negations, ratios, proper nouns correct?
3. Under multi-speaker interrupts, are sentences severely merged?
4. Does final text support real business understanding?

#### B. Diarization Accuracy

Human judgment on "who speaks, are cuts correct, speakers missed, multiple merged".

Rubric:

- **90–95%**: Speaker count, segmentation, attribution all mostly correct
- **80–89%**: Occasional short-phrase mislabeling; main turns reliable
- **70–79%**: Large spans usable; short phrases and interjections have clear confusion
- **60–69%**: Only rough turn visibility; not suitable for precise attribution
- **<60%**: Speaker track not operationally viable

Focus on:

1. Is speaker count complete?
2. Are main speaks stable?
3. Are overlaps/interjections merged?
4. Is any real speaker merged into another?
5. Is timeline speaker-turn count significantly lower than actual?

### 2.4 Final Verdict Block

- **ASR semantic accuracy**: Approx. X%–Y%, one-sentence reason
- **Diarization accuracy**: Approx. X%–Y%, one-sentence reason
- **Test result**: Pass / Conditional pass / Fail
- **Proceed to next optimization**: Yes / No

## 3. Default Judgment Order

1. **Stability first**: Crash / no result => Fail immediately
2. **Realtime metrics**: Record wall time + RTF; do not substitute accuracy
3. **ASR semantics**: Higher priority than literal match
4. **Diarization**: If speaker count wrong, score ≤ 69%
5. **Business usability**: Comprehensive judgment

## 4. Reviewer Responsibility

- **Default**: AI completes context ingestion, per-segment comparison, conclusion synthesis
- **Human involvement**: Only when explicitly requested
- **Constraint**: Never replace actual comparison with metric-only summary

## 5. Template

```
### Test Summary

| Item | Content |
|---|---|
| Test type | |
| Input audio | |
| Reference text | |
| Run result | |
| Wall time | |
| Stream RTF | |
| ASR RTF | |
| Diar RTF | |
| Subjective conclusion | |

### Segmented Review

| Time span | Reference summary | System output summary | ASR semantic | Speaker eval | Issues |
|---|---|---|---|---|---|
| 00:00–00:45 | | | | | |
| 00:45–01:20 | | | | | |
| 01:20–02:00 | | | | | |

### Conclusions

- ASR semantic accuracy:
- Diarization accuracy:
- Test result:
- Next step:
```

## 6. Project Convention

Starting from this document's adoption, any completed real test with known reference text defaults to this protocol's output format unless explicitly overridden.
