# Full Reference and Review Ledger

## Separation of evidence and judgment

The closing review uses two signed JSON documents:

1. `orator_reference_ledger`: immutable source fields plus manual reference
   adjudication completed before candidate output review.
2. `orator_run_review`: mechanically selected raw-track evidence plus two manual
   review passes and one reconciled final judgment for the exact run artifact.

`closing_ledger.py` may parse source data, select evidence by time for display,
and validate hashes/schema/signature presence. It must never infer whether a
speaker assignment is correct, total judgments, calculate a percentage, compare
a threshold, rank candidates, or emit an acceptance result. A structurally
valid ledger is not an evaluated result.

## Reference entry

Every one of the 556 source rows has a stable `ref-NNNN` ID, source line,
timestamp, speaker, text, provisional next-timestamp end, and source hash. These
fields are never edited. Its `adjudication` is completed while listening in
context and before viewing a candidate:

- exact audible start/end on the common audio clock;
- overlap participants plus their exact intervals within the reference turn;
- `critical` or `noncritical` classification;
- reference ambiguity: `none`, `timestamp`, `speaker`, `text`, `audio`, or
  `overlap`;
- one canonical real speaker for per-speaker recall, plus acceptable speakers
  for genuinely ambiguous evidence;
- acceptable semantic equivalents, context summary, notes, reviewer, and UTC
  signature.

Ambiguous or duplicate rows remain in the 556-row denominator. They are
adjudicated rather than deleted.

## Run judgment

Each run row contains evidence from business-speaker, diarization, ASR, align,
and VAD tracks selected only by time overlap. It has three independent judgment
records: chronological pass, reversed-600-second-block pass, and final
reconciliation. A complete record requires:

- `result`: `correct`, `incorrect`, or `ambiguous`;
- protocol speaker rubric category;
- system speaker labels and evidence IDs actually read;
- manually selected intervals where the business speaker is correct;
- manually judged boundary offsets, uncertain-output state, and
  confident-wrong state;
- a dedicated boundary note whenever either absolute offset exceeds one second;
- contextual notes, reviewer, and UTC signature.

When the chronological and reverse-block pass differ on any scored field, the
row also requires explicit reconciliation notes before the final judgment can
validate.

## Totals

The reviewer manually counts final `correct` rows over all 556 rows for
natural-turn accuracy. The reviewer manually sums selected correct intervals
over all adjudicated audible durations for speaker-time accuracy. Speaker time
crossing a fixed 600-second boundary is manually clipped into both blocks; turn
accuracy uses the block containing the audible start. The final partial
15.12-second block is reported separately and is not treated as a full
600-second gate.

The signed manual report records full-session, fixed-block, and
per-canonical-speaker turn and speaker-time recall; critical-turn accuracy;
critical and overall confident-wrong attribution; and median/P95 absolute
boundary offsets. A second reviewer pass manually reproduces every total and
gate decision. No summary command, spreadsheet formula, query, script, or other
code may produce these values or acceptance booleans.

## Commands

```bash
python3 tools/verify/py/closing_ledger.py init-reference \
  --reference test/data/reference/test.txt \
  --audio test/data/audio/test.mp3 \
  --out /path/reference-ledger.json

python3 tools/verify/py/closing_ledger.py prepare-review \
  --ledger /path/reference-ledger.json \
  --timeline /path/full-run.json \
  --out /path/full-run-review.json

python3 tools/verify/py/closing_ledger.py validate \
  --ledger /path/reference-ledger.json \
  --review /path/full-run-review.json --require-complete

```

The commands above initialize/display evidence and validate structural
completeness only. They do not evaluate accuracy. There is intentionally no
authorized command for labels, totals, percentages, ranking, or acceptance.
