# FR55 Gallery-Independence Evidence Design (2026-07-23)

## Status

T248-T252 are complete. FR55 audited whether a final TitaNet query can be
scored against retained identity references that use the same audio. Four
complete contextual readings found one material occurrence at `ref-0503`, but
no second context with the same topology. The branch therefore stops without
authorizing a runtime behavior or parameter change.

## Evaluation Authority

Automation may extract frozen diar delivery snapshots, replay the production
identity stage, preserve reference provenance, validate exact identity equality,
and display raw model scores. It may not label a reference speaker, evaluate a
focus context, aggregate accuracy, rank a repair, select a candidate, or issue a
verdict. Only complete contextual-semantic reading against the human-listened
`test/data/reference/test.txt` may make those judgments.

## Frozen Sources

| Item | SHA-256 |
|---|---|
| Exact FR50 Run A artifact and diar events | `33482f741d2467f28a436a6ffd90bdd0dd708e0e93af7cf814ef29c62d4781da` |
| Exact FR50 Run B artifact and diar events | `7ba17a7caacbe39bafd747fefd8fade4600f509e6c6372a03d1af7c7d14be65c` |
| Exact WAV wrapper | `a60af82d57958ddfaf5d0358820adc7536544c97d59aab9f55a3b2f53694a16e` |
| Exact stream PCM | `17f0edda49989f3ceada60170885091023eeb9d67faae0d6dd67bb585b8857fe` |
| Checked-in `orator.toml` | `d00150ae376d802af0fcf8c0a89aa3fae1e0abb2bf5d10601c55cefd570a40db` |
| Sortformer v2.1 weights | `d036020b6b93977098929d417b1b106a952ec02cc38cafc9d3315ae0ec4d90b8` |
| TitaNet-Large weights | `59e1e3069a33bef8da8b9467a5a64a30a5de8c7ae2550b15d5323ef537acaad1` |
| Run A final registry / Run B input registry | `66461a77755984a08231d06306da7ce9e1eeac07be1927e91f8a772fc54c7b3f` |
| Run B final registry | `66461a77755984a08231d06306da7ce9e1eeac07be1927e91f8a772fc54c7b3f` |
| Human-listened `test.txt` | `35e8695057be82f3028877f7dc159f10ecfa0ab7f06c444cefa9a079b0e24a86` |
| FR55 context table | `4a81d06398d0af37597764484c1dffa880262036ec5b5b714a9c6e54de97efde` |

Run A identity replay starts with an empty registry. Run B replay loads Run A's
final frozen registry. The context table is
`fr55-gallery-independence-contexts-2026-07-23.tsv`; it contains only context
IDs, common-clock bounds, focus references, and named controls.

## Code-Level Observability Finding

Before T249, `SpeakerIdentityStage::AddReference` retained only
`(quality, embedding)`. The original diar span and the actual edge-trimmed,
window-capped embedding bounds were discarded.

`SpeakerIdentityStage::EvaluateSpan` builds its session view from
`global_centroid_` and its robust view from every retained epoch reference. It
did not know whether a retained reference's embedded samples intersect the
query's embedded samples. T249 closes that observability gap without changing
the scoring path. Sortformer-labelled reference selection and TitaNet scoring
remain separate models, but overlapping audio can make their final evidence
statistically dependent.

## Evidence-Only Implementation

1. Replace the private anonymous reference pair with a private typed record
   containing the unchanged quality and embedding plus original diar bounds and
   exact embedded sample bounds. Keep insertion order, quality-only sorting,
   `max_ref_segs`, centroid arithmetic, drift decisions, and identity assignment
   unchanged.
2. Add a read-only diagnostic snapshot of the final retained references. Stable
   evidence IDs encode local slot, epoch order, and retained-reference order.
   The output includes no expected identity or correctness field.
3. Extend the existing identity replay probe with an optional retained-reference
   TSV. Preserve its old invocations and outputs.
4. Add a mechanical artifact-to-snapshot exporter. It copies each frozen `diar`
   event in capture order into the existing strict snapshot TSV schema,
   including the captured `speaker_id` when present. It does not merge,
   summarize, or classify rows.
5. For supplied immutable voiceprint queries, keep the current inclusive score
   output and add a raw overlap-excluded view. Exclusion is sample-exact: a
   retained reference is omitted only when its actual embedded sample interval
   has positive intersection with the query's actual embedded sample interval.
   The view lists intersecting reference IDs and recomputes the same session and
   robust gallery formulas over the remaining references. It does not choose a
   speaker.

The overlap-excluded view is diagnostic only. No runtime consumer calls it, and
no TOML key is introduced by T249.

## Mechanical Gates

- Snapshot export is deterministic and preserves every diar event and segment
  row in capture order.
- Run A and Run B replay every frozen snapshot through the production identity
  stage and reproduce every captured global identity exactly.
- Existing inclusive query evidence remains unchanged at the established
  serialization tolerance.
- Retained reference bounds use the session `TimeBase`; all sample intervals are
  positive, ordered, and bounded by the exact audio extent.
- Repeated reference and query-evidence exports are byte-identical and source
  hashes are recorded.
- Warning-clean build, focused identity/evidence tests, and full CTest pass.

These are producer and provenance contracts only. Failure stops FR55 evidence
use; success does not establish a speaker result.

All mechanical gates pass. Run A reproduces all `1,254,063` captured identity
rows and Run B reproduces all `1,253,630` rows. Final A/B identity replay,
retained references, and query evidence are pairwise byte-identical. The
inclusive score view remains within the established serialization tolerance,
and both packet content manifests verify every payload. Exact hashes are
recorded in `fr55-gallery-independence-review-2026-07-23.md`.

## Semantic Gate

After T249/T250, all retained reference spans and all four complete focus/control
contexts must be read in Run A chronological and reverse order, followed by
independent Run B chronological and reverse reading. T252 may authorize a later
false-default TOML experiment only when at least two material contexts share one
non-overlap evidence topology and every accepted control establishes a clear
abstention. Otherwise FR55 stops without changing FR50.

The four readings are complete. Only `ref-0503` contains a materially wrong
retained reference drawn from the focus audio; the other three focus contexts
have no reference intersection, and coherent controls show that overlap alone
cannot safely discard a reference. T252 therefore stops FR55 at the producer
boundary. See `fr55-gallery-independence-review-2026-07-23.md`.
