# Streaming Encoder Boundary Review (2026-08-09)

- **Scope**: Spec 014 T061-T064, native ASR integration and focused review
- **Product evaluation**: complete direct contextual semantic review only
- **Runtime baseline**: clean `master` at `04c564d`
- **Official source**: Qwen3-ASR commit
  `7c6daf77a2421100f5fb066495372c00129d39ff`
- **Canonical input SHA-256**:
  `b7c25d1c349b02d654b6a406bc29039749e4240a4109dda4fcc905285b14b18b`

The first sections record numerical tensors and source-contract inspection and
make no product claim. The final section records a separate complete human
contextual review. No code compares transcript meaning, calculates accuracy,
ranks output, or issues a product verdict.

## Oracle Repair and Provenance

`tools/reference/asr_oracle.py` previously resolved the model below the
nonexistent `tools/models/` tree, referred to a moved `tools/test.mp3`, assumed
an unverified `/tmp` checkout, imported Torch before path checks, and described
its transcript as ground truth. T061 repairs those contracts:

- project model, audio, TOML, and artifact paths resolve from the repository
  root;
- the official checkout location is explicit and its Git revision must match
  the pinned revision above;
- production language, system prompt, and token limit come from `orator.toml`;
- `--check-only` verifies provenance without loading Torch; and
- the tool explicitly limits its output to numerical and raw evidence.

The new standard-library contract test covers project defaults, required
official files, the accepted revision, and rejection of a mismatched revision.
The repaired check resolves the current model, canonical audio, checked-in
TOML, and exact official commit successfully. This host currently exposes only
CPU builds of Torch in both project tool environments, so a new official GPU
forward pass is unavailable. No new official transcript or tensor claim is
made from that environment.

## Encoder Locality Experiment

The existing T011 C++/CUDA probe was extended to compare any convolution-
aligned slice that divides or contains the model's 800-mel-frame attention
window. Both runs use the first 16 seconds of the same decoded audio, one full
1600-frame mel tensor, one full windowed encoder result, the same local weights,
and then independent slice encodes.

### Eight-second control

Command shape:

```text
asr_encoder_chunk_probe test.mp3 Qwen3-ASR-1.7B 16 8
```

The full encode contains 208 audio tokens. Each 800-frame standalone control
contains 104 tokens. Both complete attention windows match their corresponding
full-encode slices exactly; the overall maximum absolute tensor difference is
`0.000e+00`.

Artifact SHA-256:
`b420acc57d1679e70844d0af3a215587d5f35248cc616c25dfbbd51306aa9784`.

### One-second production append unit

Command shape:

```text
asr_encoder_chunk_probe test.mp3 Qwen3-ASR-1.7B 16 1
```

Each standalone production slice contains 100 mel frames and 13 audio tokens.
All 16 slices differ from their corresponding positions in the same complete
windowed encode. Per-slice maximum absolute differences range from
`6.163e-02` to `1.759e-01`; the overall maximum is `1.759e-01`, above T011's
`5.000e-02` numerical contract threshold.

Artifact SHA-256:
`a21140857d031be23b9c05de9904666e972da93333f931b338a4afde14396176`.

The probe therefore rejects only the implementation claim that a 100-frame
encode can be frozen and appended as though it were a complete trained
attention window. It says nothing about transcript correctness.

## Source Trace

History identifies commit `d7010a5218d2e83234e0a67c9949819ea3f1e59d` as
the change from `kStreamWindowMel=800` to `100`. The retained T011 probe still
requires complete 800-frame windows. The later Spec 003 statement that any
100-frame boundary remains equivalent was not backed by a corresponding
numerical run and is contradicted by the experiment above.

Pinned official and native source inspection shows:

| Boundary | Official implementation | Current native implementation | Finding |
|---|---|---|---|
| Prompt structure | system, user/audio, assistant, optional language + `<asr_text>` | same ordered structure | no demonstrated structural mismatch |
| Normal stream rollback | empty prefix for first N chunks, then drop K tokenizer IDs with invalid-character guard | same policy | no demonstrated broad mismatch |
| Decode | deterministic temperature-zero generation | greedy argmax | same deterministic intent; prior decoder fixture remains the numerical gate |
| Audio update | append PCM, then re-feed all accumulated audio to the model | independently encode and permanently append each completed 100-frame block | demonstrated encoder-context mismatch |
| Final tail | re-feed all accumulated audio including the short tail | append only a standalone residual encoder block | inherits the demonstrated context mismatch |

The official default uses two-second input chunks, five unfixed tokens, and
accumulated-audio re-encoding. The current source uses one-second acoustic
appends and five unfixed tokens; historical Spec 003 text claiming 15 runtime
tokens is stale.

## Bounded Correction

The first correction is `asr-final-full-context-decode`. It does not replace
Live streaming or alter its publication cadence. Instead:

1. `AsrWorker` retains exactly the PCM already admitted to each active decoder
   segment, bounded by the existing TOML segment cap;
2. Live continues to use the current incremental path as provisional text;
3. at an endpoint or safety cap, the worker closes the provisional stream and,
   when `[asr].final_full_context_decode=true`, invokes a model-agnostic final-
   segment interface on the retained PCM;
4. Qwen3-ASR runs its existing full mel, full encoder, empty-prefix decoder path
   with a separate TOML `final_max_new_tokens` limit;
5. the complete-context result becomes the only Final text supplied to forced
   alignment and the comprehensive business view; and
6. an empty complete-context result retracts an exposed provisional partial
   instead of leaving stale Live text.

The candidate inherits `384` final tokens from the original full-segment path;
it is not selected from transcript output. The Live limit remains `32`. VAD,
prompt, time boundaries, alignment, Sortformer v2.1, FR50 speaker fusion, and
all reference vocabulary remain unchanged.

Engineering tests must prove exact PCM retention, feature-disable baseline
behavior, full-context Final replacement, empty-Final retraction, TOML loading,
and resolved-config capture. Model and pipeline builds must remain warning-clean
and the complete CTest suite must pass before any product output is reviewed.

The implemented candidate satisfies that engineering gate. The complete build
contains no `warning:` or `error:` diagnostic, and all `75/75` registered tests
pass in `53.95` seconds. This includes the retained mel/encoder/decoder numerical
fixtures, the new oracle provenance contract, exact segment-PCM retention,
feature-disabled controls, full-context Final replacement, empty-Final Live
retraction, TOML loading, resolved-config capture, and the registered real-
WebSocket contract.

Build log SHA-256:
`aab4323edcade0a6b66ffdac96e01c7be65128528e14d9179b75a4cd75372cb8`.

CTest log SHA-256:
`7069a924d087fab77d0c6e28eb79741b0e87066b47a623c771b8648cf2cf5c80`.

After those gates, one focused real-WebSocket context and neighboring controls
receive complete chronological and reverse contextual semantic review. Only
that review may decide whether this correction returns to T048.

## Focused Real-WebSocket Capture

The clean candidate commit is
`5b6ba51efbdf01698bb9177637366ff1ebbc4dd6`. The exact continuous source span is
1536-1638 seconds of `test.mp3`, stored as `ref-0223-0231.wav`. It contains the
complete surrounding discussion used for the earlier empty-prompt control.

- audio duration: `102.000` seconds, `1,632,000` mono 16 kHz samples;
- WAV SHA-256:
  `5c806e0e3dd6839cf9657804b639381f07abe8a4f3c5a5befee9d565d88f0cdc`;
- streamed PCM SHA-256:
  `49b86debff7b300281886d0680e112d300a3cf8b287f5fe6039fe11620b23d10`;
- run SHA-256:
  `154c13dc8097e1b867613b825c46bbfdb1425b956effbf6df20ce0c7d400e222`;
- isolated TOML SHA-256:
  `f23f8fd71768f8636eaf098ce17e19d65ca753b8cd38402f31a26eeafb147c27`;
- server binary SHA-256:
  `0bbe367299fe662c742a5b858487d3e0fd7c46979ee8a0aa36cd0f6eb466bdf0`;
- pacing: `1.0x`, 100 ms frames, total wall `104.214` seconds, reported stream
  factor `0.979x`, and direct terminal wait `2.214` seconds; and
- terminal tracks: 22 diarization segments, eight Final ASR segments, and eight
  alignment groups, with no captured transport or terminal contract issue.

The focused run is below the full-session `0.98x` pacing threshold by `0.001x`.
It is not a duration-gate artifact, and no performance acceptance follows.
Raw evidence is retained under
`artifacts/spec014/candidates/asr-final-full-context-decode/focused-legal-context/`.

## Complete Contextual Semantic Review

The reviewer read all eight Final segments and every intervening Live state in
chronological order against the complete human-listened passage in `test.txt`,
then reread the same material from the last contribution to the first. The
following judgments are manually derived from that complete conversation.

1. The opening retains “扯鸡巴蛋” and the name “雷总”, but changes the human
   context “雷总也不说话了” to “雷总也是出汗了”. This loses the negation and
   invents a different action. It is a new critical assertion.
2. “他过完年之后再没找过我” is preserved.
3. The human question “他是知道我们签了独家吗” becomes “是知道我们签了总项目”.
   The agreement type and question structure are changed. This is a new critical
   business assertion; the following “没跟他说” is also degraded to “没他说”.
4. The candidate removes the literal system-prompt phrase from Final and recovers
   “百分之十出来放期权里” plus the following ten/fifteen discussion. However,
   “一致行动的人” remains “一致性”, the entity name remains incorrect, and
   “有限合伙人直接持” becomes “优先股我直接吃”. The legal meaning is still
   unusable even though parts of the option-pool passage improve.
5. Repeated “十够了” is recovered, but the opening fifteen discussion is partly
   replaced by “不要”, “员工的期权” remains “员工的资源”, and “3万4” remains
   “三二四”. These unresolved terms continue to alter the business meaning.
6. “才值得给” is repaired and the rest of the contribution remains coherent;
   omission of the final acknowledgment does not change its main meaning.
7. The previously exact “故事就是这么个故事” changes to “故事就是这么多故事”.
   This is a smaller but new semantic regression.
8. Final removes the unsupported Live statement about asking a labor-service
   entity to invest and preserves the main sentence about the holding ratio.
   Its first short clause remains imperfect, but this Final is materially better
   than the provisional state and the earlier control.

The reverse reading confirms that the late Final correction does not compensate
for the new agreement-type error or the invented action attributed to “雷总”. It
also confirms that the useful repairs are localized rather than a consistent
improvement across the passage.

Live remains provisionally unsafe in this candidate. It still displays the
literal configured phrase “语音识别” throughout the legal discussion and later
displays the unsupported labor-service investment statement until Final
replacement. Full-context Final removes both, but users can observe them for a
material interval in the Live region.

## Decision and Next Control

T064 requires rejection on any new critical regression. The two independently
confirmed critical assertions above satisfy that stop condition, so
`asr-final-full-context-decode` does not return to T048. No silence, 120-second,
360-second, 600-second, or full-session candidate gate is authorized.

The checked-in TOML switch is restored to false. The code remains explicitly
inactive as evidence that complete-segment full-context replacement is not a
uniformly safe correction. VAD, alignment, Sortformer v2.1, FR50 speaker policy,
and the common time base were unchanged, so no speaker baseline promotion or
regression claim is made from this isolated cold-start clip.

The restored configuration passes a warning-clean complete build and all
`75/75` registered tests in `53.18` seconds. The build-log SHA-256 is
`526c9d765cca4cf9a80066d886489fc8265ed44b948765323215891f3346f548`; the
CTest-log SHA-256 is
`e84ea77c74e3d4c9512ea9b7f9d3aac776c1699781a42e4875c25bd9a44a3c06`.

The next causal control changes only the acoustic append window from the current
100 mel frames to the model-defined 800 mel frames already covered by the exact
numerical locality control. It keeps the accepted streaming decoder and Final
policy. The same complete context will be reviewed before considering a
low-latency Live plus trained-window Final replay design.

## Phase 3D Engineering Control

The append unit is now a typed `[asr].stream_window_mel_frames` value carried in
the resolved configuration and into `Qwen3Asr`. The parser and model both reject
values other than the evidenced 100-frame control and 800-frame trained window.
The checked-in TOML remains at 100 for this engineering commit; no product
candidate output is produced from the parameterization work.

Focused configuration and model tests pass. The complete build contains no
`warning:` or `error:` diagnostic, and all `75/75` registered tests pass in
`52.85` seconds. Build-log SHA-256:
`fb033fb6e8ee66b477ed8d00fd8aa4ab40e344b653f49ade800fcbdce51f6d18`.
CTest-log SHA-256:
`7787994efbe8442cd4691803b720d15f1320fe8ca2011b500b9571ea785f771c`.

The 16-second 800-frame encoder probe is repeated after parameterization. Both
complete 104-token windows remain exactly equal to their full windowed-encoder
slices, with maximum absolute difference `0.000e+00`. The raw log SHA-256 is
again `b420acc57d1679e70844d0af3a215587d5f35248cc616c25dfbbd51306aa9784`,
identical to the prior control. This remains numerical implementation evidence,
not transcript evaluation. T067 is the first authorized product-output step.

For T067, the checked-in TOML changes only
`[asr].stream_window_mel_frames` from 100 to 800. The rejected full-context
Final switch stays false. Prompt, VAD, decoder rollback, segment cap, alignment,
Sortformer v2.1, FR50 speaker policy, and common time base remain fixed. Product
status stays open until the exact real-WebSocket artifact receives complete
forward and reverse contextual review.

The 800-frame checked-in candidate passes a second warning-clean build and all
`75/75` registered tests in `50.77` seconds. Build-log SHA-256:
`562e3e406e469efa00246bf85c1ae89539ae10cbbb2df28c91470f6cefb48692`.
CTest-log SHA-256:
`ee2905fefee04b5f3c71bde06cdd56fea2b20cc13aebdcb9175b30a46ea2c997`.
These are engineering gates only.

## Phase 3D Focused Capture

The isolated run uses clean commit
`291c63dd66c54d9f6ac6a2ede9d4cf1b70487fa5` and the same continuous 102-second
source span as T064. The worktree and config remain unchanged from client start
through terminal capture.

- WAV SHA-256:
  `5c806e0e3dd6839cf9657804b639381f07abe8a4f3c5a5befee9d565d88f0cdc`;
- streamed PCM SHA-256:
  `49b86debff7b300281886d0680e112d300a3cf8b287f5fe6039fe11620b23d10`;
- run SHA-256:
  `1414e5b2dd673018a07234fa563ef1426a6ceebcd183c3e6f501dd10b2bcec14`;
- isolated TOML SHA-256:
  `5978abd8ca5ea1f29bfa30f802ab43e8a2a72fbad7746b46489e5dc7a65f3ced`;
- server binary SHA-256:
  `f1c1788f0deb3a4551eff41da40a42604102219001b5902ac12f374aad623e99`;
- pacing: `1.0x`, 100 ms frames, total wall `103.06` seconds, reported stream
  factor `0.99x`, and direct terminal wait `1.061` seconds; and
- terminal tracks: 22 diarization segments, eight Final ASR segments, and eight
  alignment groups, with the required telemetry and observer paths present.

Raw evidence is retained under
`artifacts/spec014/candidates/asr-trained-window-800/focused-legal-context/`.
All server and client processes are stopped after capture.

## Phase 3D Complete Contextual Review

The reviewer read every Final and all six Live states chronologically against
the complete human-listened passage, then reread the terminal comprehensive
view from its last contribution to its first. No script labeled or compared any
text.

1. The opening repeats the rejected “雷总也是出汗了” assertion instead of the
   human “雷总也不说话了”. The negation is lost and a different action is
   invented.
2. “他过完年之后再没找过我” remains coherent.
3. The signed-agreement question again becomes “签了总项目” rather than the
   human “签了独家吗”, and “没跟他说” remains degraded. This repeats a critical
   business error.
4. The legal discussion's first Live state still displays “语音识别”. At the next
   trained-window update that changes to “一支新的”, not “一致行动的人”. Final
   stops immediately after “把零起量界的股权变过来，然后”, omitting the finite-
   partner holding structure, ten-percent option pool, and ten/fifteen decision.
   Those are critical missing business clauses.
5. The next contribution recovers “员工的期权” and changes the magnitude toward
   “三到四”, but invents the unit in “十五个月” and still omits “万” from “3万4”.
   This is useful acoustic evidence mixed with new critical numeric meaning.
6. The following contribution preserves “才值得给” and its purchasing rationale,
   then Final stops at “我不可能”. It omits “给股份啊，这他妈太致命了” and the
   acknowledgment, removing the central commitment from the comprehensive view.
7. “故事就是这么多故事” remains a smaller regression from the human “这么个故事”.
8. The last contribution removes the unsupported labor-service investment text
   and preserves the holding-ratio goal and “好嘞”; its opening disfluency is
   imperfect but does not add a new business assertion.

The reverse reading confirms that the improved final contribution cannot repair
the earlier omitted option-pool decision or no-equity commitment. It also
confirms that the short residual segments reproduce the same critical errors as
the rejected full-context Final candidate, while only some long trained-window
segments gain useful vocabulary.

Live is not acceptable as a product view. Segments shorter than eight seconds
have no textual Live state before Final. Longer segments update only at the
trained-window boundary, and the first visible legal state still contains the
literal prompt phrase. The later replacement does not make the earlier visible
assertion harmless.

The terminal comprehensive view assigns speakers and aligned time spans only to
the text supplied by ASR. Neither forced alignment nor the frozen speaker fusion
path contains the omitted clauses, so their joint evidence cannot restore those
business statements. No speaker-policy change or speaker-baseline conclusion is
made from this cold-start focused clip.

## Phase 3D Decision

T067 requires rejection for a new critical meaning or unusable Live behavior.
Both conditions occur. The 800-frame candidate is rejected before silence,
120-second, 360-second, 600-second, or full-session gates. T068's proposed dual-
path Final replay is not authorized because its prerequisite failed.

The checked-in TOML returns to the 100-frame control. The typed 100/800 boundary
and numerical evidence remain useful for diagnosis, but exact encoder locality
is not transcript acceptance. The next causal analysis must independently
address decoder continuation and short residual-tail behavior; it may not use
code to select the better transcript.

The restored configuration passes a build with no `warning:` or `error:`
diagnostic and all `75/75` registered tests in `52.75` seconds. Build-log
SHA-256:
`0a8dc167ccb4ca5c9a080ebe49b6de2559d1f6b5c2cc6a0634af4c32a90e160c`.
CTest-log SHA-256:
`2fa42e1c3f67f948014e3476d7eb44a69a62eb708cfb1ebc33f0494ed61fdf97`.
