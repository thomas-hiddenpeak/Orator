# Test Review Protocol

## Overview

This document defines the mandatory context-semantic test evaluation protocol
for Orator. It is not a feature spec but an engineering governance artifact
that applies across all feature specs.

The protocol exists to ensure consistent, context-aware quality judgments for
real streaming regressions. Automated gates such as CTest and numerical-oracle
comparisons establish engineering facts only; they never evaluate product
accuracy.

## Scope

Applies to:
- WebSocket real-time ASR + diarization joint testing
- Offline full-segment ASR testing
- Any regression test where reference text is known and semantic + speaker accuracy need human-style judgment

Does not replace:
- Unit test gates (`ctest`)
- Oracle numeric comparisons
- Runtime metric measurement

## Absolute Evaluation Boundary

Result evaluation is performed only by a human or AI reviewer reading the
reference and system evidence in complete conversational context. No form of
code may assign a semantic or speaker judgment, calculate or estimate an
accuracy result, rank candidates, choose parameters, or issue a pass/fail or
promotion decision. The prohibition covers compiled programs, C++/CUDA tests,
Python, shell and JavaScript scripts, notebooks, spreadsheet formulas, queries,
algorithms, and temporary command-line code.

Automation may execute and observe the system, capture immutable output, verify
hashes/schemas/time-base/transport/numerical parity, and arrange unjudged
evidence for reading. It must stop before correctness judgment and result
aggregation. CER, DER, lexical matching, timestamp overlap, duration mapping,
embedding scores, and similar mechanical values are diagnostics only and may
not be converted into an accuracy conclusion.

## 1. Required Review Inputs

Each review MUST read:

1. **Reference text** (gold)
   - Canonical Orator example: the human-listened `test.txt`. Its speaker, text,
     timestamp, line order, and recorded precision are reference truth. A review
     must not downgrade it to an unreviewed transcript, require a duplicate
     audition as a prerequisite, or invent finer timing truth than it records.
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
   - Assign every correctness judgment directly from conversational meaning
   - Do NOT use code or automated aggregation for any result conclusion
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

Every value and verdict in this block is manually derived from the reviewed
rows and manually cross-checked. A program-generated count, percentage, band,
ranking, or acceptance flag is invalid even when its inputs are manual labels.

## 3. Default Judgment Order

1. **Stability first**: Crash / no result => Fail immediately
2. **Realtime metrics**: Record wall time + RTF; do not substitute accuracy
3. **ASR semantics**: Higher priority than literal match
4. **Diarization**: If speaker count wrong, score ≤ 69%
5. **Business usability**: Comprehensive judgment

## 4. Reviewer Responsibility

- **Default**: AI completes context ingestion, per-segment comparison, conclusion synthesis
- **Human involvement**: Only when explicitly requested
- **Constraint**: Never replace actual comparison with code, metrics, rules, or
  automated summaries. The reviewer owns every label, total, and verdict.

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

Starting from this document's adoption, every result evaluation with known
reference text uses this protocol. It cannot be overridden by a feature spec,
tool, test, report template, or command-line option because Constitution 1.7.0
Article VI is authoritative.
