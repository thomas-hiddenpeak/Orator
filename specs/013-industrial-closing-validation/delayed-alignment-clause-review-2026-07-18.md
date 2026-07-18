# Delayed Alignment Clause Frozen Replay Review (2026-07-18)

## Scope and claim boundary

This record completes T108 and T109 for FR16ABN in the dirty-worktree
candidate based on source commit
`208f80c983590b858af1439da3a3c8fc0186fce9`. It covers implementation,
focused and full automated engineering tests, deterministic replay of the already
promoted full Run A and Run B typed tracks, and complete forward/reverse
contextual review of every changed conversational region.

No executable, script, query, formula, notebook, metric, or algorithm assigned
correctness, aggregated product accuracy, ranked the candidate, or issued the
retention decision. Tools exported immutable typed tracks, replayed the C++
production projector, checked deterministic and structural contracts, and
displayed unjudged context. The reviewer made the product judgment from the
complete conversation around the only changed source group, in chronological
and reverse order, while carrying forward the already completed 556-item
baseline review.

At this historical checkpoint, the review retained FR16ABN only as the frozen
candidate for real-WebSocket promotion; it was not a production-run result.
That promotion has since completed and is linked in the successor section.
T102 audible boundary review, speaker-time and fixed-block gates, T084, full
speaker closure, and industrial readiness remain open.

## Root cause and implementation

The common time base is stable around the residual previously associated with
`ref-0090`. Forced alignment leaves a `1.84 s` hole after the outgoing phrase,
then places a `0.16 s` short response at `569.26-569.42`, after activity and
primary have returned to the surrounding `spk_3`. Inside the hole, activity and
primary both carry one sustained `spk_2` island, and typed VAD exposes one
`0.264 s` boundary before the `spk_3` return. The short source group has no
isolating embedding; every containing embedding also includes the following
substantive `spk_3` speech.

FR16ABN therefore admits only a punctuation-delimited, source-contiguous,
subminimum aligned group in this source-free A-B-A evidence topology. It
requires corroborating activity and primary support for the intervening
identity, one typed VAD gap, an incumbent return at the delayed alignment onset,
uniform eligible current ownership, no exact phrase embedding, and no competing
native island. It rewrites only the exact source group and leaves all producer
tracks and timestamps immutable.

The implementation reuses the checked-in TOML punctuation, alignment pause,
alignment tolerance, minimum embedding duration, and minimum aligned-unit
count. It adds no threshold, lexical condition, speaker name, known timestamp,
or reference-derived value. The decision reason is
`sortformer_delayed_subminimum_clause_group_override`; its typed source is
`sortformer_activity+primary_top1+vad_boundary+forced_alignment`.

## Engineering verification

The focused C++ matrix covers the positive topology, evidence arrival order,
and abstention for a regular-duration group, one aligned unit, missing primary
support, an unseparated or competing island, an incumbent return outside the
existing tolerance, missing VAD gap, candidate activity on the delayed group,
and an available exact embedding. Exact source reconstruction and preservation
of the following phrase are also required.

The complete build succeeded without `warning:` or `error:` diagnostics. All
68 registered CTest entries passed. The frozen binaries were:

| Evidence | SHA-256 |
|---|---|
| `build/orator_ws` | `6812ba5f04ddc6df44a8eaf04654901b2237af98dffe430040d36e3dc058d8ab` |
| `build/business_speaker_replay_probe` | `8169f034a38df52d807055fd324797c75ca7bd490ae72dce9c77b5474e96bb82` |
| `orator.toml` | `5e3ab154a1d337361e099fe2907587820981ccc8e33d2082faeb790aa00e6218` |
| Sortformer v2.1 | `d036020b6b93977098929d417b1b106a952ec02cc38cafc9d3315ae0ec4d90b8` |
| `test.mp3` | `b7c25d1c349b02d654b6a406bc29039749e4240a4109dda4fcc905285b14b18b` |
| `test.txt` | `35e8695057be82f3028877f7dc159f10ecfa0ab7f06c444cefa9a079b0e24a86` |

## Frozen replay evidence

The source tracks came directly from the promoted FR16ABM full WebSocket
artifacts. Run A contains `755` diar segments, `1348` primary segments, `275`
ASR records, `13379` aligned units, and `16070` voiceprint records. Run B
contains `755`, `1348`, `275`, `13394`, and `16096`, respectively.

| Evidence | Run A | Run B |
|---|---|---|
| Source full timeline | `419379df5a2a49c1ca83e25c9a5606d10cbddad8080d4138e9df251e0e43c166` | `bf9c78e5b89eaf039fce860562938413422bd1fc5bb0da29e8188c56c7899584` |
| Export manifest | `4bf9e0464575ef848ca7bbcfde9ed9cff526b23ab6f4a0b9ee1d363ff36f9307` | `62c958dac79278efaf13ffc165c8b1a446629c758efdd6b2cd4eb6ec21b0ee6f` |
| Baseline replay | `5ae9f461e470d7b04641150ff43d1e6a6b05c60dfde289bfe30aff2d98bf27fe` | `f50aca636aa4a5b359e68e1948542056ec82f6f37825083803884f10949aad0d` |
| Candidate replay, all three repetitions | `bac384aa74e10dc634121f1aa9da55f804169bf11557c24fe8bab4793d9d1a80` | `d65e2d80d3944fc4ec6f8acfd8a5761d0876645ce7b5056ddc3a176edc380f68` |

Each path is byte-stable across three candidate replays. Each candidate has the
same `1753` business records as its baseline. Source text and record identities
are unchanged. JSON fixed-decimal serialization rounds two unrelated
half-microsecond values; this is display precision, not a timeline or speaker
change. The only speaker-sequence change on either path is:

- `569.26-569.42`, `呃对，对，嗯嗯，`: `spk_3` becomes `spk_2` under FR16ABN.

These statements are mechanical replay facts and carry no product verdict.

## Complete changed-context review

The reviewer read the complete `556-581 s` conversation in chronological and
reverse order for both paths. The reference conversation is Shi Yi's valuation
explanation, Xu Zijing's short confirmations, Shi Yi's substantive continuation
about the additional investment, Xu Zijing's short continuation, and Shi Yi's
calculation. In the business view, `spk_3` is Shi Yi and `spk_2` is Xu Zijing.

The changed short group is the confirmation contribution associated with
`ref-0090`, Xu Zijing's `嗯。对。对。`; assigning it to `spk_2` restores its
natural speaker. The subsequent substantive `实际上你又投了...` contribution
remains independently assigned to `spk_3`, preserving Shi Yi's `ref-0091` and
the surrounding valuation argument. Run A and Run B have the same local
conversation and the same result.

The worksheet's provisional timestamp join displays `ref-0091` because the
forced-aligned group starts at `569.26`. That mechanical row boundary cannot
decide the turn: complete conversational context identifies the short
confirmations as Xu Zijing's preceding contribution and the following
substantive clause as Shi Yi's contribution. The collapsed group may include a
brief Shi Yi filler; because audible review is not available here, this record
does not claim an exact acoustic boundary correction.

The complete contextual judgment retains the sole changed assignment and finds
no changed-context regression. Carrying this one repaired contribution forward
from the already signed full baseline review gives a frozen-replay natural-turn
state of 515 accepted / 41 incorrect for Run A and 516 / 40 for Run B. These
totals and the retention decision were derived manually from the complete
context and prior signed ledger; no executable mechanism produced them.

## Retention and next gate

FR16ABN is retained as the frozen candidate. T108 and T109 are complete. The
next gate is a clean transitional experimental commit followed by 120-second
and 600-second direct-end production WebSocket runs with unchanged TOML values.
Only after their mechanical contracts and complete changed-context review pass
may a new full A/B capture begin. Full-run promotion still requires complete
556-contribution forward and reverse contextual review; this frozen replay does
not substitute for it.

## Successor status

That promotion ladder is now complete at transitional experimental commit
`6b1cb79fa4f5`. T110 and T111 passed their real-WebSocket and complete
forward/reverse contextual gates. The authoritative full-run evidence is
`delayed-alignment-full-promotion-review-2026-07-18.md`; this document remains
the frozen-replay design and changed-context record only.
