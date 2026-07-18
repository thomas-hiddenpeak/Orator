# Future-Epoch Phrase Frozen Replay Review (2026-07-18)

## Scope and claim boundary

This record completes T114 and T115 for FR16ABO in the dirty-worktree
candidate based on source commit `94ea937`. It covers the typed configuration,
production projection rule, focused abstention matrix, complete engineering
test suite, deterministic replay of both accepted T111 full-run typed-track
sets, and complete forward/reverse conversational review of every changed
region against the human-listened `test.txt`.

No executable, script, query, formula, notebook, metric, or algorithm assigned
speaker correctness, aggregated product accuracy, ranked this candidate, or
issued the retention decision. Tools only preserved and replayed typed
evidence, verified deterministic and structural contracts, and displayed raw
contexts. The reviewer made the product judgment from the complete surrounding
conversation in chronological and reverse order, carrying forward the already
signed 556-contribution T111 review.

FR16ABO is retained here only as a frozen-evidence candidate. It is not a new
real-WebSocket baseline and does not advance T102, T084, full speaker closure,
or industrial readiness. Those claims still require the promotion ladder and,
for any full capture, a new complete 556-contribution forward/reverse semantic
review.

## Root cause and implementation

The remaining tail error associated with `ref-0504` is not caused by a broken
session clock. At `3299.90-3300.62`, the business phrase inherits the current
identity of one local diarization slot even though the same slot later begins a
stable, differently identified epoch. The later epoch is useful retrospective
evidence because both the diarization activity and independent primary-speaker
track corroborate it, and the robust voiceprint galleries distinguish its
candidate from the incumbent. Rewriting the global identity stage or expanding
its backfill window was rejected because that changes unrelated historical
epoch boundaries.

FR16ABO therefore permits only one complete punctuation phrase to challenge
its current identity. The phrase must have uniform current ownership, exactly
one corroborating local slot, matching current ownership from both diarization
and primary tracks, no competing slot, and enough aligned units. The first
later differing epoch on that same slot must fall within the explicit
`[speaker_fusion].future_epoch_lookahead_sec` TOML window; both diarization and
primary support must be stable for at least the existing minimum embedding
duration. The robust gallery candidate must pass the existing duration score
and margin, the session candidate must remain in its top two, and neither
gallery may rank the incumbent first.

The rule changes only the exact business phrase. It leaves the common time
base, raw identity epochs, all producer tracks, text, and source boundaries
immutable. A zero lookahead disables the rule. The checked-in candidate uses
`120.0` seconds, and the decision reason is
`voiceprint_phrase_future_epoch_robust_override`. No reference wording,
speaker name, known timestamp, or command-line tuning is present.

## Engineering verification

Focused C++ coverage includes the positive topology and abstention for a zero
window, a future epoch outside the window, missing future primary support,
weak robust-gallery margin, a session candidate absent from the top two,
insufficient alignment, and competing activity. Configuration parsing,
runtime serialization, and replay-probe wiring are also covered.

The build completed without a `warning:` or `error:` diagnostic. All 69
registered CTest entries passed. The candidate binaries and fixed inputs are:

| Evidence | SHA-256 |
|---|---|
| `build/orator_ws` | `209b16f402f89c101aa76521f98bbae9a28f874487b2081197dbc90ab92cb858` |
| `build/business_speaker_replay_probe` | `ac6016298ba6210c545fca2f1763ce5170efac64d79da6efaf2d1f4187d8ed84` |
| `orator.toml` | `6cd61b9d79a931ccc64fd3ee51ab95050d34da4f8da67697039214b13eb0e875` |
| Sortformer v2.1 | `d036020b6b93977098929d417b1b106a952ec02cc38cafc9d3315ae0ec4d90b8` |
| `test.mp3` | `b7c25d1c349b02d654b6a406bc29039749e4240a4109dda4fcc905285b14b18b` |
| `test.txt` | `35e8695057be82f3028877f7dc159f10ecfa0ab7f06c444cefa9a079b0e24a86` |

## Frozen replay evidence

Run A is the accepted empty-registry direct-end capture; Run B is the accepted
frozen-registry controlled retry. Run A contributes 755 diarization segments,
1348 primary segments, 275 ASR records, 13382 aligned units, and 16083
voiceprint records. Run B contributes 755, 1348, 274, 13365, and 16086,
respectively.

| Evidence | Run A | Run B |
|---|---|---|
| Accepted T111 timeline | `d5c97db9ff91b41da4ccd5414d5f2bca4966592e60fb2717058fee2e600132e9` | `b5dfefc8c30ec9458bbe70a8f7e789a6997d082c9bcce7d834b1df12e6c725f4` |
| Export manifest | `04014b320c58813e22c9b932760d076aacfd7bdc1ba116935a1efde2c2e53f90` | `983b94eeabe3ddeaaea2d9564e1e4501c8bda8a760f1fbb11b28c7889e645d42` |
| Candidate replay, both repetitions | `5fef10c3fcf009b42ef5698d3bb3a44ea3f42d0dffd46b0f2d019bb1d6af8eac` | `d4d98149c15b37ebde27155d38ba44662080c987bd58cf60ff693988b0c061a2` |

Each path is byte-stable across both repetitions. Run A retains 1750 business
records and Run B retains 1748. In each path FR16ABO activates on exactly two
phrases. These are mechanical replay facts, not correctness judgments:

- `2991.16-2991.88`, `就分开嘛。`: remains `spk_0`; only the recorded decision
  reason changes.
- `3299.90-3300.62`, `这边才会长，`: changes from `spk_0` to `spk_1`.

No other business speaker assignment, source boundary, text, or producer-track
record changes.

## Complete changed-context review

The reviewer read both changed conversations in full chronological and reverse
order for Run A and Run B, then compared them with the corresponding complete
human-listened passages in `test.txt`. The business identity map is
`spk_0=朱杰`, `spk_1=唐云峰`, `spk_2=徐子景`, and `spk_3=石一`.

At `00:49:43-00:49:58`, Xu Zijing explains that the two people approve each
other, Tang Yunfeng acknowledges it, Zhu Jie says `就分开了嘛。`, and Tang
Yunfeng continues the joke about who approves whom. Both baseline and candidate
assign the changed phrase to `spk_0`, matching Zhu Jie. FR16ABO therefore makes
no product identity change in this context and introduces no semantic
regression.

At `00:54:28-00:55:18`, Zhu Jie first discusses temporarily retaining the
legal representative and says the other side's share is not large. Tang
Yunfeng then replies `他在这边占比大。` and continues at `00:55:06` about the
Hangzhou share. The runtime's imperfect text `这边才会长，` is the opening of
that Tang Yunfeng contribution. Assigning it to `spk_1` restores the speaker
continuity of Tang Yunfeng's reply; the baseline `spk_0` assignment incorrectly
extends Zhu Jie's preceding turn. This conclusion depends on conversational
meaning and turn response structure, not the ASR wording or a timestamp rule.
Run A and Run B present the same context and retain the same repair.

The complete changed-context judgment therefore retains FR16ABO. Carrying this
single repaired contribution forward from the signed T111 ledger gives the
frozen candidate 520 accepted and 36 incorrect contributions in each path.
This manual carry-forward is a design-stage result only; it does not replace a
new full-run review or advance the accepted 519/556 production baseline.

## Retention and next gate

T114 and T115 are complete. FR16ABO is retained for T116 as a transitional
experiment. The next gate is a clean commit followed by warning-clean build,
full CTest, and 120/600-second real-WebSocket runs with checked-in TOML values.
Those shorter runs validate runtime and lifecycle behavior; because neither
contains the late repaired context, they cannot establish the speaker benefit.
If they pass, a full A/B capture is justified, and each new full artifact must
receive complete 556-contribution forward and reverse contextual semantic
review before any accepted gate or baseline changes.

## T116 successor outcome

T116 subsequently passed the 120/600-second real-WebSocket ladder and completed
new full Run A and Run B captures at clean transitional commit `f49a8278e0d8`.
Both full artifacts received complete 556-contribution chronological and
tail-to-start contextual semantic review against the human-listened `test.txt`.
The manually checked result is `518/556` for each run, one contribution below
the accepted T111 baseline, and their error sets differ. FR16ABO still repairs
`ref-0504` in both runs, but the full promotion is rejected and the checked-in
TOML switch is returned to zero. See
`future-epoch-full-promotion-review-2026-07-18.md` for the frozen evidence,
manual ledgers, and decision. No executable mechanism produced that result or
verdict.
