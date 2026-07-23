# FR56 Speaker Producer-Boundary Evidence Design

## Authority and purpose

FR56 investigates the producer boundary between streaming Sortformer local
speaker slots and time-ordered TitaNet identity epochs. It starts from the ten
FR51 critical residuals whose complete contextual-semantic review already
found unusable or contradictory speaker evidence before final fusion. The
phase does not reopen ASR wording accuracy and does not assume that those ten
contexts share one cause.

No executable artifact may label a speaker, classify a context, rank an epoch
policy, aggregate an accuracy result, or issue a product verdict. Automation
may preserve, hash, validate, index, and display unjudged evidence only. Every
speaker and causal judgment requires complete conversational-semantic reading
against `test/data/reference/test.txt`, in both chronological and reverse
order, independently for Run A and Run B.

## Frozen baseline

FR56 keeps the exact FR50 real-path baseline fixed:

- code under evaluation: `a6f0d33730326b19a3831019b1aba21fd900f126`;
- full Run A artifact SHA-256:
  `33482f741d2467f28a436a6ffd90bdd0dd708e0e93af7cf814ef29c62d4781da`;
- full Run B artifact SHA-256:
  `7ba17a7caacbe39bafd747fefd8fade4600f509e6c6372a03d1af7c7d14be65c`;
- stream PCM SHA-256:
  `17f0edda49989f3ceada60170885091023eeb9d67faae0d6dd67bb585b8857fe`;
- checked-in TOML SHA-256:
  `d00150ae376d802af0fcf8c0a89aa3fae1e0abb2bf5d10601c55cefd570a40db`;
- Sortformer v2.1 SHA-256:
  `d036020b6b93977098929d417b1b106a952ec02cc38cafc9d3315ae0ec4d90b8`;
- TitaNet SHA-256:
  `59e1e3069a33bef8da8b9467a5a64a30a5de8c7ae2550b15d5323ef537acaad1`;
- human-listened reference SHA-256:
  `35e8695057be82f3028877f7dc159f10ecfa0ab7f06c444cefa9a079b0e24a86`;
- Run A starts with an empty registry and Run B starts with the exact frozen
  Run A registry, SHA-256
  `66461a77755984a08231d06306da7ce9e1eeac07be1927e91f8a772fc54c7b3f`.

The context-only table is
`fr56-speaker-producer-boundary-contexts-2026-07-23.tsv`, SHA-256
`19c9e53de90a74ccc937b2af821db96f4e929140daf6d24b4d1d2c3f93d5f38c`.
It contains only common-clock bounds and reference IDs. The ten rows come from
the prior human FR51 reading, not from an executable filter. `ref-0099` remains
the accepted final-only overwrite
control, and `ref-0118` remains the accepted alignment/native-boundary control;
both are already present in the surrounding FR51 evidence and must be reread
when their neighbouring FR56 pages are reviewed.

## Verified code path

The following statements are static producer-contract facts, not accuracy
judgments:

1. `DiarizationWorker` gives each Sortformer output channel one
   `local_speaker` value. With checked-in `diarizer.reset_period_sec=0`, the
   four channel numbers remain in one diarizer session for the full stream.
2. Every raw `DiarFrameBlock` is deposited on the common absolute-seconds time
   base before the segment view reaches `SpeakerIdentityStage`.
3. `SpeakerIdentityStage::Process` receives the accumulated onset/offset
   segment view at the configured delivery cadence. It assigns one global ID
   to each local slot's active time-ordered epoch and may start a later epoch
   after clean-span drift or competing-identity evidence.
4. The mapped diarization segments are then deposited and consumed by the
   comprehensive speaker pipeline. Final fusion can revise the business view,
   but it cannot recover a producer identity that is absent or contradictory
   in the deposited evidence.
5. FR55 exact capture-order replay now preserves every diar delivery and every
   retained-reference source/embedding bound. Existing packet output shows
   final epoch intervals, but it does not expose every pending, split, or
   abstention decision that formed those intervals.

## Evidence sequence

1. Reuse the exact FR51 and FR55 Run A/B packets. Read all ten rows and named
   controls from the raw posterior, activity, primary, identity-epoch,
   retained-reference, source/alignment, VAD, TitaNet, and business views.
   Manually record which producer evidence is present; do not assign a cause by
   query or script.
2. If the existing views cannot explain an epoch boundary, add a diagnostic-
   only transition trace to exact capture-order identity replay. Each record
   may contain event index, local slot, active epoch bounds and identity,
   candidate source/embedding bounds, raw own/competing scores, pending state,
   blocked identities, and the production decision name. It must not contain a
   reference speaker, correctness field, causal label, candidate ranking, or
   acceptance field.
3. Require the diagnostic replay to reproduce every captured Run A/B identity
   row exactly. Diagnostic output must have deterministic ordering, valid
   common-clock bounds, stable source hashes, and no runtime consumer or TOML
   key.
4. Arrange independent Run A/B review packets. Include complete local context,
   all raw producer tracks, exact identity event order, epoch transition trace
   when needed, retained-reference provenance, and current comprehensive
   output. Mechanical checks end at provenance and completeness.
5. Read Run A chronologically, Run B chronologically, Run A in reverse, and Run
   B in reverse. Reconcile the four readings manually.

## Authorization gate

A later false-default, TOML-controlled producer experiment requires at least
two independent material contexts with one reference-free local-slot or
identity-epoch topology, plus explicit abstentions demonstrated by every
accepted control. The topology must use only evidence available at runtime on
the common time base and must preserve raw Sortformer output and historical
epoch attribution.

If the complete readings instead show distinct producer errors, insufficient
identity evidence, or a Sortformer channel error that no identity contract can
resolve, FR56 stops without a runtime, TOML, model, product-run, ledger,
baseline, or closure change. No new `test.mp3` run is justified before this
manual gate because the exact producer evidence is already frozen.

## Outcome

T253-T257 complete the gate. The independent Run A chronological, Run B
chronological, Run A reverse, and Run B reverse readings identify only
`ref-0499` beside an identity epoch change. Its accepted preceding Tang Yunfeng
control rejects moving the boundary backward, and no independent Shi Yi query
supports the focus before the transition. The other nine contexts remain
distinct stable-epoch source or producer conflicts.

The existing final epoch output is sufficient for this decision, so the
conditional diagnostic transition trace is not implemented. FR56 stops with no
runtime, TOML, model, product-run, ledger, baseline, or closure change. The
complete manual record is
`fr56-speaker-producer-boundary-review-2026-07-23.md`.
