# Full Reference and Review Ledger

## Separation of evidence and judgment

The closing review may use two signed JSON documents to arrange evidence:

1. `orator_reference_ledger`: an immutable mirror of the human-audited
   `test.txt` source plus reference-only annotations.
2. `orator_run_review`: mechanically selected raw-track evidence plus two manual
   review passes and one reconciled final judgment for the exact run artifact.

`test.txt` itself is the reference authority. An absent or incomplete auxiliary
JSON document does not make its 556 human-listened rows unreviewed and does not
require the audio to be transcribed or timed again. The JSON documents are
optional evidence indexes; they cannot replace or refine the source truth.

`closing_ledger.py` may parse source data, select evidence by time for display,
and validate hashes/schema/signature presence. It must never infer whether a
speaker assignment is correct, total judgments, calculate a percentage, compare
a threshold, rank candidates, or emit an acceptance result. A structurally
valid ledger is not an evaluated result.

## Reference entry

Every one of the 556 source rows has a stable `ref-NNNN` ID, source line,
whole-second timestamp, speaker, text, next-timestamp display edge, line order,
and source hash. These fields are never edited. The source timestamp is used
only at its recorded precision. Duplicate timestamps and the one backward pair
remain in line order and are interpreted from adjacent conversational context;
the display edge is not an independently heard sub-second boundary.

Reference-only annotations may add:

- `critical` or `noncritical`, decided from the business meaning in `test.txt`
  without consulting the candidate evidence for that classification;
- reference ambiguity: `none`, `timestamp`, `speaker`, `text`, or `overlap`;
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
- manually judged source time blocks where the business speaker is correct;
- manually judged source-time offsets at `test.txt` precision, uncertain-output state, and
  confident-wrong state;
- a dedicated note whenever an offset changes which reference contribution the
  runtime span represents or otherwise affects business attribution;
- contextual notes, reviewer, and UTC signature.

When the chronological and reverse-block pass differ on any scored field, the
row also requires explicit reconciliation notes before the final judgment can
validate.

## Totals

The reviewer manually counts final `correct` rows over all 556 rows for
natural-turn accuracy. The reviewer manually reviews and totals the human source
time blocks in `test.txt` for speaker-time accuracy. Whole-second source marks,
line order, and conversation determine the applicable block; duplicate or
backward marks require an explicit contextual note rather than automated repair.
Time crossing a fixed 600-second boundary is manually assigned from context.
The final partial 15.12-second block is reported separately and is not treated
as a full 600-second gate.

The signed manual report records full-session, fixed-block, and
per-canonical-speaker turn and speaker-time recall; critical-turn accuracy;
critical and overall confident-wrong attribution; and all attribution-affecting
source-time offsets visible at the one-second reference precision. It does not
report median/P95 sub-second reference statistics that `test.txt` cannot
support. A second manual pass reproduces every total and gate decision. No
summary command, spreadsheet formula, query, script, or other code may produce
these values or acceptance booleans.

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
