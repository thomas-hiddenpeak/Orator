# Spec 014 Full Baseline Context Review (2026-08-09)

## Scope and evaluation boundary

This report signs the first persistent, current-source full baseline required by
Spec 014. The production server received the complete `test.mp3` through the
real WebSocket path at source rate. The reference is the complete human-listened
`test.txt`.

The product conclusions below were made only by reading all 556 reference
contributions and the corresponding raw ASR, VAD, forced-alignment, speaker,
business-view, and incremental-event evidence. The reviewer first read the
session chronologically, then reread the complete session in reverse fixed-
window order and reconciled the two readings from conversational context. No
compiled code, script, query, formula, notebook, metric, or algorithm labeled a
row, counted correctness, calculated accuracy, ranked defects, selected a
candidate, or issued the verdict.

Programs were used only to run the product, capture immutable evidence, verify
mechanical contracts, and arrange unjudged source-ordered review worksheets.

## Test summary

| Item | Content |
|---|---|
| Test type | Full real-WebSocket joint streaming baseline |
| Input audio | `test/data/audio/test.mp3`, 3615.120 seconds, 16 kHz mono |
| Reference text | `test/data/reference/test.txt`, human-listened, 556 contributions |
| Runtime | Clean commit `96b8347fb8287fda1ac2fbbf0c1d07920c5dfdf1`; checked-in behavior copied without parameter override |
| Diarizer | Streaming Sortformer v2.1, frozen FR50 `340/1/188/188` profile |
| Run result | Completed without crash, CUDA error, OOM, or server error |
| Wall time | 3641.765 seconds |
| Stream RTF | `0.993x` at `1.0x` source pacing |
| ASR / diar RTF | Not exposed as independent session-wide values; asynchronous stage evidence is retained in the raw run |
| Direct-end latency | 26.310 seconds |
| Subjective conclusion | Main topics remain understandable, but critical facts are too frequently changed or omitted for ASR closure; speaker behavior remains within the frozen conditional FR50 boundary |

## Frozen evidence

| Evidence | Value |
|---|---|
| Raw run | `artifacts/spec014/baseline-1417334/full-a/run.json` |
| Raw run SHA-256 | `5a6d4e1681d021fce59e96c15bbd27fb9c52571c3fd0f84ffa0962520187e3a4` |
| Run manifest SHA-256 | `9d056a3371433f0d7c7d4c320d8c62a2b019a2b2d14582fcaa8552bddcd17e7f` |
| Pre-run manifest SHA-256 | `588104ef3c06603920876b5cb26733544443820e31a2a0ca7455d7553519248d` |
| Empty-start registry SHA-256 after run | `66461a77755984a08231d06306da7ce9e1eeac07be1927e91f8a772fc54c7b3f` |
| Source MP3 SHA-256 | `b7c25d1c349b02d654b6a406bc29039749e4240a4109dda4fcc905285b14b18b` |
| Stream PCM SHA-256 | `17f0edda49989f3ceada60170885091023eeb9d67faae0d6dd67bb585b8857fe` |
| Config source SHA-256 | `6734acdc09a8ba8b818036d1fff61b5539464a3b552bd2693f013f0bd79e79cb` |
| Server binary SHA-256 | `222e5b55e6e5ea62a1ea7600d676616e044af93c0b22bdba0fd7b9d0a3cbdc84` |
| Forward worksheet SHA-256 | `1a33a899d218e1d24ce83056a5265f8da492281e4ab26554806cddfcb5eb2d57` |
| Reverse worksheet SHA-256 | `37df3d3b9c1117bd58f2b62ae13cba1c192537fa13a614897402283a2b7a73fd` |
| Reviewer notes SHA-256 | `590e0af51475fbaaf56306c84bc87c2344836272146bf35210103c3ff58bdfb0` |

The run received exactly 57,841,920 samples. All seven source, VAD, diar,
ASR, alignment, speaker, and comprehensive extents end at 3615.120 seconds.
The terminal tracks contain 755 diarization segments, 1,348 primary-speaker
segments, 308 final ASR records, 972 VAD segments, 308 alignment groups,
17,452 voiceprint spans, and 1,719 final business entries. Each final ASR ID is
represented exactly once by forced alignment. Producer, early observer, late
observer, persisted terminal document, and exported terminal document converge
mechanically.

Continuous telemetry contains the required GPU utilization, memory, system
power, and `tegrastats` fields at the required cadence. Runtime GPU memory
reached 37.75 GB, GPU utilization reached 30%, runtime power reached 82.8 W,
and `tegrastats` power reached 84.224 W. These are mechanical observations, not
product-quality evidence.

## Segmented manual review

The ranges below are holistic reviewer judgments made after both complete
passes. They are not calculated from token, character, timestamp, or row
matching.

| Time span | Reference and system context | ASR semantic | Speaker evaluation | Material issues |
|---|---|---|---|---|
| 00:00-10:00 | Company/product framing, Hangzhou/Chengdu relationship, equity allocation and valuation setup remain understandable | Approx. 80-89% | Mostly accurate | RM1 and Shi Yi names, allocation numbers, governance right, negation, decimal amounts, and Tang's separate concurrence fail; known short-turn speaker residuals remain |
| 10:00-20:00 | Dilution, financing rounds, valuation and negotiation rationale remain broadly traceable | Approx. 80-89% | Mostly accurate | `3.14` plan ownership, supplement commitment, two-round horizon, `80 亿` valuation, challenge polarity, and currency scale are not recoverable faithfully |
| 20:00-30:00 | Investor discussion, legal structure, product examples and decision flow remain visible | Approx. 70-79% | Mostly accurate with short-turn confusion | Financing-round identity, benchmark names, pricing polarity, dates, governance terms, option structure, unsupported investment text, and Chengdu entity fail |
| 30:00-40:00 | Product separation, internationalization, consolidation, acquisition and IP discussion retain their main topic | Approx. 70-79% | Mostly accurate with known omissions | Company/product names, open-source provenance, manufacturing condition, consolidation terminology, schedule polarity, prior-expense consequence, and patent scope fail |
| 40:00-50:00 | Tax/IP, trial narrative, financing conditions, PMP structure, valuation and governance remain topic-level readable | Approx. 60-69% | Mostly accurate, but several known short and long attribution residuals remain | Dense unrecoverable legal, entity, amount, consent, deadline, business-line, valuation, ownership, reimbursement and mutual-approval errors make details unsafe |
| 50:00-60:00 | Taxpayer, nominee holding, company separation, overseas structure and account routing remain visible | Approx. 60-69% | Mostly accurate, with the known FR50 tail cluster | Wrong taxpayer/entity/account, RM01 and company names, legal representative and nominee-holding conclusions, geographic negation, structure process, platform and allocation details fail |
| 60:00-60:15.12 | Agent/address and four-person reserve-account closure survive | Approx. 90-95% | Mostly accurate | Final colloquial wording is distorted, but the terminal decision is preserved by the immediately preceding complete context |

## Reconciled contextual findings

The reverse pass prevented local transcription errors from being treated as
business failures where the same conversation settled the point unambiguously.
Examples include the Hangzhou/Chengdu non-relationship, the `3.14` first
statement, the direct `50 亿` decision, `0.7`, `28.8`, the `7` non-dilution
condition, `80 倍 PE`, the no-disclosure answer, the acquisition conclusion,
the delayed-reimbursement rule, the final `B` business choice, symbolic
one-yuan pricing, and the shipping route. These remain wording defects or local
presentation defects, but the complete context preserves their operative
meaning.

The same reverse review confirms that the following critical failures are not
repaired by complete context. This ledger is written and checked manually; its
presence, not a program-generated count, fails the 100-percent critical-meaning
gate.

- `00:00-10:00`: `ref-0001`, `ref-0019`, `ref-0022`, `ref-0031`,
  `ref-0039`, `ref-0047`, `ref-0052`, `ref-0066`, `ref-0077`,
  `ref-0081`, `ref-0092`.
- `10:00-20:00`: `ref-0130`, `ref-0141`, `ref-0163`, `ref-0174`,
  `ref-0176`, `ref-0177`.
- `20:00-30:00`: `ref-0179`, `ref-0183`, `ref-0194`, `ref-0199`,
  `ref-0205`, `ref-0209`, `ref-0222`, `ref-0226`, `ref-0227`,
  `ref-0228`, `ref-0229`, `ref-0230`, `ref-0247`.
- `30:00-40:00`: `ref-0265`, `ref-0266`, `ref-0269`, `ref-0275`,
  `ref-0276`, `ref-0277`, `ref-0279`, `ref-0288`, `ref-0289`,
  `ref-0290`, `ref-0297`, `ref-0300`, `ref-0305`, `ref-0321`,
  `ref-0334`, `ref-0336`.
- `40:00-50:00`: `ref-0340`, `ref-0344`, `ref-0346`, `ref-0348`,
  `ref-0357`, `ref-0361`, `ref-0364`, `ref-0369`, `ref-0370`,
  `ref-0371`, `ref-0372`, `ref-0374`, `ref-0375`, `ref-0379`,
  `ref-0380`, `ref-0385`, `ref-0386`, `ref-0395`, `ref-0397`,
  `ref-0404`, `ref-0409`, `ref-0410`, `ref-0412`, `ref-0415`,
  `ref-0422`, `ref-0426`, `ref-0427`, `ref-0428`, `ref-0431`,
  `ref-0432`, `ref-0434`, `ref-0435`, `ref-0436`, `ref-0440`,
  `ref-0441`, `ref-0445`, `ref-0448`, `ref-0452`, `ref-0455`,
  `ref-0458`, `ref-0462`.
- `50:00-60:00`: `ref-0471`, `ref-0472`, `ref-0473`, `ref-0478`,
  `ref-0479`, `ref-0481`, `ref-0483`, `ref-0485`, `ref-0486`,
  `ref-0487`, `ref-0493`, `ref-0501`, `ref-0502`, `ref-0503`,
  `ref-0504`, `ref-0515`, `ref-0516`, `ref-0517`, `ref-0522`,
  `ref-0524`, `ref-0525`, `ref-0530`, `ref-0531`, `ref-0538`,
  `ref-0544`, `ref-0550`, `ref-0552`.

The failure pattern is not a single tail-only collapse. The final 20 minutes
are materially worse, but unrecoverable proper-name, number, negation, legal-
term, and commitment failures exist from the opening block onward. Long
24-second decoder sessions also cross natural speaker turns. Forced alignment
repairs many final ownership boundaries but cannot repair text meaning already
lost by ASR.

One unsupported substantive investment proposition remains at `ref-0230`.
Transient `5000` fragments around `ref-0393`/`ref-0394` are immediately and
repeatedly contradicted by the correct no-buyback/no-valuation-adjustment rule,
so complete context does not leave a plausible 5000 term. The three separate
digital-silence reviews remain free of substantive speech assertions.

## Live, endpoint, and speaker conclusions

- **Live publication**: unchanged partial text is repeatedly delivered over
  WebSocket at approximately the incremental polling cadence. Typed partial
  state already deduplicates it, so this is a publication-path defect rather
  than decoder instability. Final IDs and terminal content still converge.
- **Endpointing**: decoder intervals frequently span multiple natural turns.
  Alignment recovers many speaker boundaries, but endpoint construction does
  not prevent critical text loss, cross-turn joins, or absent short replies.
  The evidence does not justify a VAD threshold change before publication is
  corrected and endpoint ownership is traced on the common sample clock.
- **Speaker guard**: all four real speakers remain represented; no new whole-
  session identity permutation, accumulating drift, or new policy topology is
  found. Known FR50 short-response, overlap, omission, and tail residuals remain.
  This one baseline preserves the conditional FR50 comparison boundary but does
  not replace FR50's signed empty/frozen-registry A/B result.

## Conclusions and verdict

- **ASR semantic accuracy**: approximately 70-79% by complete contextual
  review. The discussion remains topic-level comprehensible, but critical
  names, numbers, polarity, legal structures, decisions, and commitments are
  often unsafe.
- **Diarization/business-speaker accuracy**: approximately 90-95% within the
  frozen conditional FR50 boundary. Main turns are usable; known short-turn,
  overlap, omission, and tail attribution residuals remain, so canonical
  speaker closure is still open.
- **Critical meaning**: failed. The complete manual ledger above is nonempty.
- **Fixed 600-second ASR blocks**: failed. Every complete block is manually
  judged below the required 90-percent semantic floor; the final 15.12 seconds
  is reported separately and does not repair them.
- **Silence hallucination**: the separate three-run digital-silence gate passes;
  room tone and physical microphone remain open.
- **Mechanical/realtime gate**: passed for this baseline.
- **Test result**: failed for ASR closing; accepted as the immutable diagnostic
  baseline.
- **Proceed to next optimization**: yes, beginning with the bounded Live
  publication defect. Final ASR meaning is the next independent defect class;
  speaker and diarizer behavior remain frozen.

## First frozen correction boundary

The first correction is limited to duplicate unchanged partial publication.
Its contract is reference-free:

1. one newly accepted non-empty text state may produce one WebSocket partial
   event;
2. feeding more audio while the same `text_id` retains the same text produces
   no additional external partial event; the final event supplies the terminal
   interval;
3. a changed partial, retract, or final remains externally visible exactly
   once in publication order; and
4. typed ASR records, final text, forced alignment, speaker policy, source
   sample extents, and checked-in TOML behavior do not change.

The accepted controls are the current full terminal document, the two
120-second seal runs, and the three silence runs. No endpoint, decoder, VAD,
speaker, diarizer, or fusion parameter is authorized by this report.
