# Equal-Overlap Confidence Review (2026-07-15)

## Governance

Tools replayed frozen tracks, checked byte determinism, and arranged unjudged
contexts only. No code, script, formula, query, notebook, metric, or algorithm
assigned correctness, aggregated accuracy, selected a policy, or issued the
verdict below.

## Manual finding

The two `higher_confidence` full-track replays are byte-identical and produce
188 displayed speaker-sequence changes against the same-source `shorter_span`
control. Manual reading stops after the first 20 changed contexts because the
candidate has already shown repeated categorical regressions. It repairs weak
micro-slot chatter, but it also absorbs genuine short replies including `对`,
`相差0.7`, and `你们俩可以` into a longer high-confidence speaker segment.

## Verdict

FR16ZZ is rejected from the closing candidate. The typed experiment remains
available for reproducibility, while `shorter_span` remains the runtime default.
The next topology uses sustained native frame top-1 evidence rather than whole-
segment mean confidence.
