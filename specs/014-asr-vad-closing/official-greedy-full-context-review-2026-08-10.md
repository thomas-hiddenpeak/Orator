# Spec 014 Official-Greedy Full Context Review (2026-08-10)

## Scope and evaluation boundary

This report evaluates the exact committed Phase 3I candidate at
`5f98db18b1ed3e3aee74aad7f01e7a51a50ac214`. The candidate uses
`stream_mode = "accumulated_redecode"`, a 1000 ms streaming cadence, and
`ban_steps = 0`. Every VAD, prompt, segment, model, alignment, Sortformer v2.1,
FR50 speaker-fusion, time-base, and publication value remains fixed.

The production server received all 3615.120 seconds of canonical `test.mp3`
through the real WebSocket at source rate. The reference is the complete
human-listened `test.txt` with 556 contributions.

The reviewer read all 556 reference contributions beside the Final and
comprehensive evidence in chronological order, then reread all 556 in reverse
fixed-window order. The reviewer also read every one of the 2,664 Live ASR
partial events from the beginning through the terminal sample, then reread
them from terminal time to zero in reverse fixed windows and reconciled their
user-visible effect with the Web UI state and rendering code. No compiled code,
script, query, formula, notebook, metric, or algorithm labeled
a contribution, counted correctness, calculated accuracy, compared candidates,
or issued the verdict. Programs only ran and captured the product, checked
mechanical contracts, and displayed unjudged evidence.

The earlier 102-second Phase 3H result is used only as a diagnostic pointer.
It has no independent product-decision authority in this report.

## Evaluation correction

The prior evaluation mistake was methodological: a 102-second excerpt was
allowed to reject a whole-session candidate. That excerpt could identify a
local defect and guide inspection, but it could not establish global semantic
behavior, candidate ordering, or a product verdict. Repeating or optimizing
that excerpt would only select a local optimum.

This review corrects the boundary in four ways: the exact candidate processes
the complete source once; every human reference context is read in both
directions; every user-visible Live state is independently read in both
directions; and the whole-session judgment is made from conversation context,
not by averaging the fixed windows. The windows organize complete coverage and
expose where failures occur; they do not replace the global reading.

## Test summary

| Item | Content |
|---|---|
| Test type | Full real-WebSocket joint streaming candidate evaluation |
| Input audio | `test/data/audio/test.mp3`, 3615.120 seconds, 16 kHz mono |
| Reference text | `test/data/reference/test.txt`, human-listened, 556 contributions |
| Runtime | Clean candidate commit `5f98db18b1ed3e3aee74aad7f01e7a51a50ac214`; isolated paths/ports only |
| Diarizer | Streaming Sortformer v2.1, frozen FR50 `340/1/188/188` profile |
| Run result | Completed without crash, CUDA error, OOM, transport error, or server error |
| Wall time | 3643.083 seconds |
| Stream throughput | `0.992x` at `1.0x` source pacing |
| ASR compute | 1728.944 seconds; runtime-reported throughput `2.091x` |
| Diar compute | 50.197 seconds; runtime-reported throughput `72.019x` |
| Direct-end latency | 27.883 seconds |
| Subjective conclusion | The full discussion remains topic-level understandable, but critical meaning and Live presentation remain unsafe; the candidate is not an overall improvement |

## Frozen evidence

| Evidence | Value |
|---|---|
| Raw run | `artifacts/spec014/candidates/asr-official-greedy-no-eos-ban/full-a/run.json` |
| Raw run SHA-256 | `3bb41068ada63e6bb107f51263f65a1b2f3316aeb72999a2791c56fb0fd33fb1` |
| Run manifest SHA-256 | `cf49f15d2b225738d816d7728ced51530ddbcb804deaeaccf2b7e89f7daef0c6` |
| Pre-run manifest SHA-256 | `f4a9027657f28ead2ce126d853c257f31acadb30ed7a69d73cd6e34efd2eab13` |
| Candidate source TOML SHA-256 | `7a1398fd4b72dcf580695d786f344c204f85b0cdecda5ff28ae5a64f05de66e5` |
| Candidate checked-in TOML SHA-256 | `0e0f5c36818b32dc0d779d8315f2559e558dda12d599120d4d71c9cbb421bd8e` |
| Server binary SHA-256 | `b8f755b6a44e86b8da079acf2bd36cb3d77f56e44f58f07d6060135799a45f7c` |
| Source MP3 SHA-256 | `b7c25d1c349b02d654b6a406bc29039749e4240a4109dda4fcc905285b14b18b` |
| Stream PCM SHA-256 | `17f0edda49989f3ceada60170885091023eeb9d67faae0d6dd67bb585b8857fe` |
| Reference SHA-256 | `35e8695057be82f3028877f7dc159f10ecfa0ab7f06c444cefa9a079b0e24a86` |
| Forward worksheet SHA-256 | `28f346d3560e17b164e18d603aa51ad9e8c2c912e1dee15b2d4683ae6f8c0646` |
| Reverse worksheet SHA-256 | `dd4f1a40e2bcbede48872672753d5880a9766949322936f8158f6af50a9a6355` |
| Client log SHA-256 | `2af4a5d465eb58e3ac235f0b447a776ba9577d06f8e8ac18698b5da9fb614fcd` |
| Server log SHA-256 | `5b9dc66fe422d125c0e327a89234bf946334f963052e09e77ce18abbca69bcc7` |
| Empty-start registry after run SHA-256 | `66461a77755984a08231d06306da7ce9e1eeac07be1927e91f8a772fc54c7b3f` |

The run received exactly 57,841,920 samples. Source, VAD, diarization, ASR,
alignment, speaker, and comprehensive extents all end at that exact sample.
The terminal tracks contain 755 diarization segments, 1,348 primary-speaker
segments, 308 Final ASR records, 972 VAD segments, 308 alignment groups,
17,488 voiceprint spans, and 1,704 comprehensive entries. Every Final ASR ID
has one alignment group. Producer, early observer, late observer, persisted
terminal state, and exported terminal state reconcile mechanically.

Continuous telemetry covers the complete run with 3,615 runtime samples and
3,633 `tegrastats` samples. Runtime GPU utilization averages 53.06 percent and
reaches 98 percent; GPU memory reaches 37,814 MB; runtime system power reaches
82.32 W; and `tegrastats` system power reaches 83.784 W. These are mechanical
observations, not accuracy evidence.

## Segmented manual review

The bands below are holistic reviewer judgments after both complete reference
passes and both complete Live passes. They are not computed or averaged from
tokens, characters, rows, timestamps, or labels.

| Time span | Reference and system context | ASR semantic | Speaker evaluation | Material issues |
|---|---|---|---|---|
| 00:00-10:00 | Product ownership, Hangzhou/Chengdu scope, equity percentages, valuation, and governance remain broadly traceable | Approx. 70-79% | Mostly accurate | `拿出40%` becomes `拿不出40%`; RM1, names, arithmetic, `一致行动权`, `低于51`, and `不要再吃亏` are changed or reversed |
| 10:00-20:00 | Dilution, financing rounds, option pool, valuation, and negotiation intent remain visible | Approx. 70-79% | Mostly accurate | `3.14`, `8+6=14`, three-round limits, dilution, option-pool terminology, percentages, `80亿`, and currency/unit facts are repeatedly unsafe |
| 20:00-30:00 | The final 28/15/7/51 agreement and three-round non-dilution direction survive in context | Approx. 70-79% | Mostly accurate with short-turn confusion | Pricing polarity, product/company names, dates, legal structure, silence/signing questions, entity ownership, and amount scale fail; a transient amount changes before later recovery |
| 30:00-40:00 | Company separation, internationalization, consolidation, acquisition, reimbursement, and IP discussion retain their main topic | Approx. 70-79% | Mostly accurate with known omissions | Open-source provenance, product names, strategy, consolidation terms, `不一定全是重磅`, purchase-versus-transfer conditions, and reimbursement consequences are changed |
| 40:00-50:00 | Tax/IP, financing conditions, PMP structure, valuation, ownership, and governance remain topic-level readable | Approx. 60-69% | Mostly accurate with known residuals | First/second round, signing/no-signing, pre-closing deadline, B+C package, `3000万` versus `3000亿`, ownership, and five-to-six-million scale remain critically wrong |
| 50:00-60:00 | Consolidation, dividends, nominee holding, company separation, overseas structure, and account routing remain visible | Approx. 60-69% | Mostly accurate with the known FR50 tail cluster | Taxpayer/entity relationships, litigation, legal representative, nominee holding, the Hangzhou prohibition, Canadian/non-disclosure context, account structure, and names are unsafe |
| 60:00-60:15.12 | Agent/address visibility and the four-person reserve-account conclusion survive | Approx. 80-89% | Mostly accurate | The final name/colloquial wording is distorted and the closing `挺好` becomes `挺满` |

## Reconciled full-context findings

The reverse pass repaired a limited set of boundary-local readings where a
following contribution completed an interrupted phrase. For example, a
provisional missing `不一定` is recoverable when the next complete turn is
read. This confirms why the earlier 102-second decision boundary was invalid.

The same reverse pass confirms that the material failures below are not
repaired by later context:

- The opening block reverses effort, disadvantage, and governance statements;
  these are decisions and commitments, not harmless wording variants.
- The second block repeatedly changes equations, decimals, financing-round
  constraints, dilution commitments, percentages, and valuation units.
- The third block preserves the broad final allocation but loses legal terms,
  signing status, entity ownership, pricing polarity, names, and amount scale.
- The fourth block preserves the broad separation proposal but changes a
  negative qualifier, open-source and acquisition facts, and the condition
  under which prior expenses are repaid.
- The fifth block changes first round to second round, signing to no signing,
  loses the pre-closing deadline and B+C package, and asserts `3000亿` where
  the reference says `3000万`.
- The sixth block changes litigation to financing and does not reliably
  preserve nominee holding, legal-representative, Hangzhou, non-disclosure,
  Canada, and overseas-account conclusions.

The failure distribution is session-wide. The tail remains difficult, but it
is not the sole source of the result. Complete context cannot convert the
candidate into a globally correct transcript because unrepaired polarity,
number, identity, legal, and commitment errors occur in every complete block.

## Live, endpoint, Web UI, and speaker findings

Every one of the 2,664 `asr_partial` events was read chronologically and again
in reverse fixed windows. The reverse pass did not repair or overturn the Live
findings. The accumulated decoder frequently grows one `text_id` across 20 to
more than 40 seconds, joining multiple natural turns and speakers while
rewriting the complete provisional text each second. Final output repairs some
provisional errors but does not
erase their user-visible effect and does not repair all of them.

At about 1625 seconds, Live displays an unsupported statement that asks to let
`老刘投钱`; the next updates rewrite it toward the actual holding-percentage
discussion. Other Live states change material numbers and legal terms before
settling, including the amount that later resolves to 20 million and the
`3000万` valuation context that appears as `3000亿`. These are visible
hallucination/rewrite defects, not terminal-only artifacts.

The Web UI behavior confirms the product effect. `Model.applyAsr()` replaces
the single current Draft with every partial. `TranscriptView` displays that
whole Draft immediately, then creates one finalized row per `text_id`.
Speaker revisions do not split that row at comprehensive speaker boundaries;
the renderer chooses only the dominant overlapping speaker for the entire ASR
row. The observed Live hallucinations and cross-speaker long rows are therefore
what a microphone user sees.

Endpoint behavior remains open. The growing state does not establish natural
speaker-turn endpoints and does not prevent cross-turn semantic contamination.
No VAD or endpoint parameter change is justified by this candidate.

All four real speakers remain represented. The complete review finds no new
whole-session identity permutation or accumulating identity drift. Speaker
behavior therefore remains inside the frozen conditional FR50 boundary, with
the existing short-turn, overlap, omission, and tail residuals. This does not
advance canonical speaker closure.

## Full-baseline comparison

The immutable current-source baseline is recorded in
`full-baseline-context-review-2026-08-09.md`; it was itself read completely in
chronological and reverse context and manually judged at approximately 70-79
percent overall. Its first two complete blocks were 80-89 percent, its middle
two blocks were 70-79 percent, and its final two complete blocks were 60-69
percent. This candidate remains 70-79 percent overall, lowers both opening
blocks into 70-79 percent, leaves the middle and final complete blocks below
closure, and lowers the final 15.12-second context from 90-95 to 80-89 percent.

The accumulated state improves continuity for some long statements, but the
complete conversation does not show a compensating global gain. Instead, it
redistributes critical errors and leaves the Live user experience less stable.
The full review therefore does not support retaining the official-greedy
candidate.

## Conclusions and verdict

- **ASR semantic accuracy**: approximately 70-79% by complete contextual
  review. The gist is understandable, but business-critical facts remain
  unsafe throughout the session.
- **Diarization/business-speaker accuracy**: approximately 90-95% within the
  frozen conditional FR50 boundary. Known residuals and canonical closure
  remain open.
- **Critical meaning**: failed. Unrepaired critical errors exist in every
  complete 600-second block.
- **Fixed 600-second ASR blocks**: failed. All six complete blocks are manually
  judged below the required 90-percent floor.
- **Live hallucination and presentation**: failed. Unsupported provisional
  statements and cross-speaker long rows are user-visible.
- **Mechanical/realtime gate**: passed.
- **Test result**: failed for ASR closing; reject the Phase 3I candidate.
- **Configuration disposition**: restore the checked-in `kv_append`, dormant
  2000 ms accumulated cadence, and `ban_steps = 3` control. Preserve this run
  as immutable rejected-candidate evidence.
- **Proceed to another decoder parameter**: no. A new candidate requires a
  separately specified full-session hypothesis; shortened evidence cannot
  select it.
