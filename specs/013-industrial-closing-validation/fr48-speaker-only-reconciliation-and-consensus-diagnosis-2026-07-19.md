# FR48 Speaker-Only Reconciliation and Consensus Diagnosis (2026-07-19)

## Scope and authority

This record separates speaker ownership from ASR lexical fidelity on the two
exact full FR47 real-WebSocket artifacts. It also reviews the proposed
hierarchical-consensus protection using frozen typed evidence only. It changes
no model, speaker policy, time code, audio artifact, registry, or TOML value.

`test/data/reference/test.txt` is the authoritative human-listened reference.
No code, script, query, formula, notebook, metric, hash, structural filter, or
algorithm assigned correctness, counted an accepted result, ranked a product
candidate, or issued the decision below. Automation only preserved and
displayed immutable evidence, verified hashes and schemas, and replayed typed
inputs. Every product label, residual class, total, and stop decision was
manually transcribed and cross-checked after complete conversational reading.

## Frozen artifacts and review method

Both artifacts come from clean pushed commit
`70f1186d2b9e0b1b12808ebc644a164d1e21983c` and the checked-in
`orator.toml`:

| Path | Timeline SHA-256 | Review directions |
|---|---|---|
| Full Run A, empty isolated registry | `a43288a4145b1ad3f92a0599020a46346bde9bf81ad6f18572136b4bc380ff9e` | all 556 contributions chronological, then reverse fixed windows |
| Full Run B, restarted frozen registry | `c2d45c2e87e21f8ef9d98094f933f09badd593edf0c779968cab9289ee64f3a1` | all 556 contributions chronological, then reverse fixed windows |

The identity map is `spk_0 = 朱杰`, `spk_1 = 唐云峰`,
`spk_2 = 徐子景`, and `spk_3 = 石一`. The manual rubric is:

- a recognizable contribution displayed under its listener-verified speaker
  is speaker-correct even when the decoded words are wrong;
- absent material speech remains missing;
- speech owned materially by another speaker remains confident-wrong;
- genuinely indeterminate ownership remains uncertain;
- punctuation, lexical similarity, hashes, evidence scores, and interval
  overlap do not decide a label.

Run A and Run B were each read independently from start to end and again from
the final fixed window to the first. The two-direction review produced the
same manual row decisions in both artifacts. No new speaker residual was found.

## Speaker-only ledger correction

One prior row mixed an ASR defect into the speaker ledger:

| Reference | Final business evidence | Manual finding |
|---|---|---|
| `ref-0375` | `2635.068-2635.308`, `spk_2`, decoded as `不要` | The listener transcript is 徐子景 saying `可以啊`. The wording is wrong, but the displayed canonical speaker is 徐子景. This row is speaker-correct and remains an ASR defect. |

The complete speaker-only reconciliation therefore records `522/556`, with
28 confident-wrong, five missing, and one uncertain contribution. This is a
manual tally, not a code-produced score. The seven manually cross-checked
fixed-window ledgers are `88/93`, `79/84`, `76/80`, `75/80`, `119/129`,
`82/87`, and `3/3`. The per-speaker ledgers are 朱杰 `77/83`, 唐云峰
`176/189`, 徐子景 `70/73`, and 石一 `199/211`.

Twenty business-critical residuals remain. Nineteen are confident-wrong and
`ref-0442` is missing:

`0049`, `0058`, `0066`, `0099`, `0102`, `0118`, `0252`, `0313`, `0327`,
`0331`, `0333`, `0354`, `0390`, `0426`, `0442`, `0444`, `0461`, `0499`,
`0503`, and `0505`.

The noncritical residuals remain unchanged:

| Class | References |
|---|---|
| Confident-wrong | `0061`, `0135`, `0171`, `0221`, `0239`, `0241`, `0298`, `0457`, `0537` |
| Missing | `0063`, `0341`, `0409`, `0417` |
| Uncertain | `0506` |

Residuals still occur in every complete 600-second block. FR47 remains the
best repeatable real-path candidate, but critical-attribution-zero and
confident-wrong-zero remain open, so this correction is not a closing claim.

## Typed replay completeness repair

Reference-free replay inputs were exported from both full timelines into
`/tmp/orator-spec013/release-fr48-native-consensus/`. Each manifest records
755 diarization entries, 1,348 primary entries, 308 ASR finals, 13,327 aligned
units, and 16,104 voiceprint evidence records. Manifest SHA-256 values are
`a9472a90bdc7ae6eaebdacaa26fb32a0f8c505060b567e25b7af8b2171bcd488`
for A and
`7c4e18cdb41e7eff2f000d42750fbba9f0dc4a5c8b9f742d35f8faf604c084a0`
for B.

The existing direct-track exporter omitted the already typed
`session_gallery_complete` field from its voiceprint TSV. The C++ replay
parser consequently used its backward-compatible incomplete default, so a
direct replay contained 1,715 rather than all 1,716 business records and could
not faithfully exercise complete-gallery evidence. The exporter now writes
both session and robust completeness columns, and an exact regression covers
the asymmetric `true/false` case.

After that mechanical repair, both baseline replays contain all 1,716 business
records and are byte-identical at SHA-256
`75fc0b39fdf4530ec98a54f8e6ac113e8eef1aee00839c3d9c6577adafb8302e`.
This equality proves replay consistency only; it contributes no correctness
judgment.

## Complete direct-short conflict review

The reference-free display exposed 24 ordinary short direct-voiceprint
decisions that conflict with a uniform native identity. Every surrounding
conversation and accepted control was read forward and reverse in both A and
B. The manual findings fall into three groups:

- Existing direct identity is necessary for accepted speaker ownership at
  starts `317.868`, `2428.636`, `2520.844`, `2811.596`, `2813.196`,
  `2853.436`, `3015.052`, `3039.404`, `3299.724`, `3346.636`, and
  `3557.344`. A broad native-precedence rule would regress these contexts.
- Starts `175.116`, `1155.672`, `1375.728`, `1474.360`, `1580.364`,
  `2421.676`, `2759.724`, `2894.460`, `2895.580`, `2915.360`,
  `3016.732`, and `3381.884` are tiny fillers, overlap fragments, or internal
  pieces of otherwise correctly owned natural contributions. Rewriting them
  does not establish a reusable material speaker repair.
- `661.276-662.076` is the only context with a plausible strict hierarchy
  invariant, and complete context identifies it as the `ref-0099` residual.

At `661.276-662.076`, the final business view displays
`他们就啥都别说` under `spk_0` with reason `voiceprint_direct_short`, while
the surrounding governance statement belongs to 石一 (`spk_3`). Activity and
primary evidence cover the exact interval completely and uncontestedly as
`spk_3`. The one containing VAD query and the one same-text complete-source
query both have complete session and robust galleries and uniquely prefer
`spk_3` under the existing gates. The exact direct query instead prefers
`spk_0`; its session margin is `0.068`, while its robust margin is `0.015`,
below the existing `0.04` robust margin gate. Thus the lower-scope write lacks
eligible dual-gallery agreement and overwrites unanimous broader evidence.

This evidence explains `ref-0099`, but the complete source-free inventory
contains no second material context with the same full topology. The accepted
controls also demonstrate that direct evidence often repairs native speaker
labels. The proposed guard would therefore be justified by one product
context only.

## Engineering validation

The replay-export regression suite passes all three tests, including the new
asymmetric gallery-completeness case. A complete `cmake --build build -j`
finishes without warning or error diagnostics, and
`ctest --test-dir build --output-on-failure` passes all `70/70` entries in
`53.12 s`. These checks validate the exporter repair and existing mechanical
contracts only; they do not evaluate speaker correctness or alter the manual
contextual decision.

## Decision and next boundary

FR48 stops before product implementation. A single-context fit is explicitly
insufficient under the approved gate, so no
`direct_short_native_consensus_guard_enable` field, policy helper, candidate
replay, TOML change, or audio run is authorized. The current FR47 runtime and
checked-in configuration remain unchanged.

The next speaker-closure phase must seek a broader evidence family across
multiple remaining material residuals, especially source-absent, zero-duration,
overlap, and boundary cases. It must first establish a reusable reference-free
invariant from frozen typed evidence and complete forward/reverse context. It
must not tune a rule around `ref-0099` or start another full audio run merely
to search parameters.
