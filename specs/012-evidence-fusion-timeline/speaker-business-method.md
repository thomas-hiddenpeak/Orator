# Speaker Business Accuracy Method

This spec evaluates speaker accuracy at the business-view level, not as an
isolated diarization-track metric.

## Scope

The target output is the final user-facing speaker transcript view: who said
which business-relevant statement in context. The review must consider the
combined evidence from all pipelines:

- ASR final text: used as the system's content claim and as a context anchor.
- Forced alignment: used for intra-ASR timing and phrase boundaries.
- VAD: used for speech/pause support and endpoint sanity.
- Diarization: used for local speaker ownership over time.
- Speaker identity: used for stable speaker labels across the full session.
- Comprehensive/fusion view: the actual business output under review.

ASR literal accuracy is not scored in this review. However, when judging speaker
business accuracy, the reviewer still reads the transcript content because
speaker ownership is only meaningful in the context of what was being said.

## Non-authoritative Diagnostics

Code may prepare review packets and report mechanical facts:

- track counts and coverage;
- align coverage and time-base issues;
- overlap summaries;
- candidate/current view diffs.

No code, script, test, notebook, formula, query, metric, or algorithm may assign
a speaker-business label, calculate an accuracy percentage, rank/select a
candidate, or issue a verdict. Historical code-derived percentages are retained
only as non-authoritative mechanical diagnostics. Result evaluation is valid
only when the reviewer reads every item against
`test/data/reference/test.txt` in its surrounding conversational context,
performs the required second pass, and manually derives/checks the report.

## Review Unit

The review unit is a natural business turn or short exchange, not a raw diar
segment and not an ASR segment. A valid review entry should answer:

1. What was the reference business context?
2. Which real speakers participated?
3. What did the final system view attribute to each speaker?
4. Did that attribution preserve the business meaning of who said what?

## Judgment

Use the Test Review Protocol's diarization rubric, but apply it to the final
business view:

- **90-95%**: speaker identity and turn ownership are reliable for business use.
- **80-89%**: long turns are reliable; short interjections rarely affect meaning.
- **70-79%**: large spans are usable, but important short turns can be wrong.
- **60-69%**: only rough speaker context is usable.
- **<60%**: not operationally usable.

When a window contains a clear ASR text error, do not penalize ASR semantics in
this score. Penalize speaker business accuracy only if the final view attributes
the relevant utterance or business position to the wrong speaker, misses a
speaker's role in the exchange, or collapses multiple participants in a way that
changes who held which position.

## Required Report

Every speaker-business review must include:

- input evidence package path;
- candidate view path and parameters;
- time-window table with reference context, final-view attribution, speaker
  business judgment, and issues;
- final speaker business accuracy band;
- explicit distinction between mechanical diagnostics and constitutional
  context-aware judgment;
- an explicit statement that no code generated labels, totals, percentages,
  candidate rankings, or the verdict.
