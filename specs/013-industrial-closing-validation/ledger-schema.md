# Full Reference and Review Ledger

## Separation of evidence and judgment

The closing review uses two signed JSON documents:

1. `orator_reference_ledger`: immutable source fields plus manual reference
   adjudication completed before candidate output review.
2. `orator_run_review`: mechanically selected raw-track evidence plus two manual
   review passes and one reconciled final judgment for the exact run artifact.

`closing_ledger.py` may parse, align, display, validate, and total signed manual
judgments. It must never infer whether a speaker assignment is correct.

## Reference entry

Every one of the 556 source rows has a stable `ref-NNNN` ID, source line,
timestamp, speaker, text, provisional next-timestamp end, and source hash. These
fields are never edited. Its `adjudication` is completed while listening in
context and before viewing a candidate:

- exact audible start/end on the common audio clock;
- overlap participants;
- `critical` or `noncritical` classification;
- reference ambiguity: `none`, `timestamp`, `speaker`, `text`, `audio`, or
  `overlap`;
- acceptable speakers/semantic equivalents, context summary, notes, reviewer,
  and UTC signature.

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
- manually judged boundary offsets and confident-wrong state;
- contextual notes, reviewer, and UTC signature.

## Totals

Natural-turn accuracy counts only final `correct` rows over all 556 rows.
Speaker-time accuracy sums only manually selected correct intervals over all
adjudicated audible durations. Critical accuracy, confident-wrong count, and
fixed 600-second block totals are conjunctive gates. The summary command refuses
incomplete or hash-mismatched ledgers.

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

python3 tools/verify/py/closing_ledger.py summary \
  --ledger /path/reference-ledger.json \
  --review /path/full-run-review.json
```
