# Final ASR Prompt-Causality Review (2026-08-09)

## Status

- **Scope**: Spec 014 T045-T047, final ASR meaning only
- **Control**: clean full baseline `96b8347`, with the accepted Live publication
  correction frozen and FR50 speaker behavior unchanged
- **Result**: T045-T047 complete; the first reference-free TOML candidate was
  rejected by complete focused-context review and removed
- **Product gate**: unchanged and open

No executable code, script, query, metric, formula, or lexical rule assigned a
correctness label, selected the candidate, or produced a product verdict. Shell
and `jq` commands only displayed immutable raw records for direct reading. The
reviewer read the complete baseline notes, every retained critical context in
forward conversational order, the full final ASR track through the tail, and
the existing reverse-context reconciliation before making the judgment below.

## Evidence Read

The immutable evidence is under
`artifacts/spec014/baseline-1417334/full-a/`:

- `run.json`: raw runtime events and terminal typed tracks;
- `review/forward-by-reference.md`: complete source-order evidence beside the
  human-listened `test.txt` reference;
- `review/reverse-by-reference.md`: complete reverse-order evidence;
- `review/manual-review-notes.md`: the previously reconciled contextual
  judgments; and
- the captured `orator.toml`, manifest, server log, and client log.

The reread covered the opening name, entity, number, decimal, negation, and
allocation failures; the financing-round, valuation, commitment, legal-term,
date, and unsupported-insertion failures in the middle; and the IP, entity,
schedule, financing, governance, taxpayer, nominee-holding, overseas-structure,
and company-name failures through the terminal context. In particular, the
review returned to the critical clusters around `ref-0001`-`ref-0092`,
`ref-0107`-`ref-0176`, `ref-0177`-`ref-0247`, `ref-0265`-`ref-0336`,
`ref-0340`-`ref-0462`, and `ref-0471`-`ref-0552` rather than inspecting only a
tail sample.

Direct reading rejects a single long-session or tail-drift explanation. The
same semantic substitution class is already present in short, single-speaker
finals and continues in longer multi-speaker finals. Long decoder sessions can
increase the amount of affected conversation, but they are not the common
cause. The tail contains denser legal, tax, entity, and company-name language,
which makes the business impact more visible; it does not introduce a new
failure mode.

## Common-Time-Base Trace

`ref-0226` and `ref-0227` provide the clearest causal context because two
speakers repeat the same legal term:

- Human reference at `00:25:51`: `那这样其实也就不需要一致行动的人。`
- Human reference at `00:25:54`: `对。如果这样的肯定不行，对，也不需要一致行动的人。`
- Captured ASR configuration:
  `system_prompt = "你是一个专业的中文普通话语音识别系统，请准确识别并转录所有语音内容。"`

The signal path is coherent on one clock:

1. Typed VAD admits voiced intervals beginning at `1551.300s`, followed by
   voiced intervals across both repeated statements. The ASR session begins at
   `1551.100s`, exactly accounting for the configured 200ms VAD lead.
2. Decoder session `text_id=133` begins at `1551.100s`. Its Live partial first
   exposes `那这样其实也就不需要语音识别了。` at an end frontier of
   `1554.164s`. Later partials keep that wording and render the second repetition
   as `不需要语音`.
3. The immutable final ASR record at `1551.100-1575.972s` retains both
   substitutions. Therefore the text is already wrong before forced alignment
   or final speaker projection.
4. Forced alignment for `text_id=133` accepts the finalized transcript as its
   input and assigns units for `语`, `音`, `识`, and `别` at
   `1552.700-1554.540s`. It does not replace transcript characters.
5. The final business-speaker view copies `那这样其实也就不需要语音识别了`
   and `不需要语音` into its speaker-bounded entries. Its revisions affect
   ownership and boundaries, not the ASR text.

This trace separates endpoint ownership from decoder semantic loss. There is
admitted speech before and through the phrase, the error is visible in the
decoder's own provisional output, and downstream stages preserve it. The
substitution `一致行动的人 -> 语音识别` is also unusually informative because
`语音识别` appears verbatim in the system prompt supplied before every audio
block.

## Selected Defect Class

The selected class is **system-prompt-conditioned lexical substitution in the
ASR decoder**. This is a bounded causal hypothesis, not a claim that the prompt
explains every remaining proper-name, number, unit, or polarity error. It is
selected first because:

- the contaminating phrase is present before every streamed audio segment;
- the strongest repeated legal-term failure reproduces prompt text verbatim;
- the model's checked-in `chat_template.json` permits an empty system message;
- the model's local README transcription examples provide audio and an optional
  language hint without a custom system instruction; and
- removing the instruction is reference-free and does not encode any word,
  timestamp, person, company, or decision from `test.txt`.

The historical comment that this prompt was "proven to stabilise output" has no
linked complete contextual evidence. Commit `a7decbb` introduced the sentence;
commit `673dfef` only moved the existing default into typed TOML configuration.
That history does not establish a current product-quality benefit.

## T047 Candidate

The only behavioral change in historical candidate
`asr-empty-system-prompt` was:

```toml
[asr]
system_prompt = ""
```

All other ASR, VAD, alignment, speaker, timeline, model, transport, and GPU
values remain byte-for-byte unchanged. In particular, `language = "Chinese"`,
`segment_sec = 24.0`, `max_new_tokens = 32`, and the frozen Sortformer v2.1 /
FR50 settings are controls. No alternative instruction is added, because a
longer instruction could create a different conditioning bias and would mix two
hypotheses.

The focused engineering test asserted that the candidate TOML resolved an empty
ASR system prompt. The warning-clean build completed and all `74/74` registered
tests passed in `52.74s`. Existing model numerical gates remained unchanged
because no model weight, kernel, precision, feature extraction, encoder, or
decoder implementation changed.

The staged product review uses these direct contexts:

- **abstention controls**: three independent digital-silence sessions must make
  no substantive speech assertion;
- **focused causal context**: a real-WebSocket excerpt containing the complete
  `ref-0224`-`ref-0229` exchange, including both repetitions and neighboring
  legal context;
- **opening controls**: two independent canonical 120-second sessions reviewed
  completely in both directions, including names, entities, numbers,
  negations, and speaker boundaries;
- **duration controls**: one complete 360-second and one complete 600-second
  session, each reviewed chronologically and in reverse before any longer run;
  and
- **speaker guard**: every reviewed candidate artifact includes final
  business-speaker ownership under the frozen FR50 policy.

The focused excerpt can establish whether the selected prompt-conditioned
substitution changes, but it cannot promote the candidate. A fixed phrase
improvement does not outweigh a new name, number, negation, endpoint,
hallucination, or speaker regression elsewhere. The candidate is removed if a
staged control fails. No full run is authorized before T048 and T049 complete.

## Focused Candidate Review

The candidate was captured from exact clean commit `5accc5f` through the real
WebSocket path at `1.0x` with 100ms frames. The source is the continuous
`1536-1638s` portion of `test.mp3`, decoded without resampling drift to a
102-second, 16kHz mono PCM WAV so the review includes the complete
`ref-0223`-`ref-0231` conversation around both repeated legal terms.

Mechanical evidence only:

- artifact:
  `artifacts/spec014/candidates/asr-empty-system-prompt/focused-legal-context/`;
- WAV SHA-256:
  `5c806e0e3dd6839cf9657804b639381f07abe8a4f3c5a5befee9d565d88f0cdc`;
- run SHA-256:
  `e8834d3a4374433e165b4dadd34899532518e07a09b2a7172421edd981ac6865`;
- captured config SHA-256:
  `ce989ea4887ba9375be0c6df8150f595669f1aa51d70f0a31fad858e91deaec8`;
- `wall_clock_ok`, `timebase_ok`, and `timebase_reconciled` are true;
- all seven pipeline extents equal the exact 1,632,000 input samples;
- direct terminal return is 1.159 seconds after the final frame; and
- required telemetry coverage and observer terminal identity pass their
  mechanical contracts.

The reviewer then read every final ASR and business-speaker entry against the
complete human reference in chronological order and again from the terminal
context back to the start. Both readings reach the same decision:

- The first `一致行动的人` becomes `女性`; the second becomes `一个`. Removing
  the prompt phrase removes the exact `语音识别` echo but does not recover either
  legal statement.
- `雷总也不说话了` becomes `伟哥也说话了`. The baseline already lost the
  negation, but retained `雷总`; the candidate adds a material name regression.
- The repeated `十五`/`十够了` option-pool discussion becomes repeated
  `树多的`/`十度的`, whereas the frozen baseline retains enough of the repeated
  `十` conclusion to follow that exchange. This is a second material regression
  under the same decoder boundaries.
- `灵启疆界`, `有限合伙人直接持`, and `放期权里` remain unusable as
  `林启江界`, `有些货直接吃`, and `放弃权利`; the later unsupported
  `让劳务...投钱` insertion also remains.
- The later explanation that only work none of the four understand merits
  equity, and the final wish for the holding to exceed the boss's, remain
  contextually recoverable. These controls do not repair the failed critical
  terms or offset the new regressions.

The empty-system-prompt candidate therefore fails its own focused causal
context and is rejected before silence or canonical 120-second gates. This
result refines the diagnosis: the historical prompt can contaminate ambiguous
decoding, but removing all system conditioning also destabilizes other
ambiguous language and does not recover the business terms. Prompt conditioning
is an active decoder factor, not a sufficient standalone root-cause repair.
The checked-in TOML and its config-contract test are restored to the frozen
pre-candidate prompt after this review. The restored tree is warning-clean and
all `74/74` registered tests pass again in `52.79s`.

## Current Conclusion

T045-T047 are complete. The current baseline's ASR meaning loss is already
present at the decoder output, and the strongest directly traced instance is
consistent with prompt contamination rather than VAD, forced alignment, or
comprehensive-timeline rewriting. The first one-variable correction proves that
prompt conditioning changes ambiguous output, but it fails to restore the
target meaning and introduces new material regressions. It is rejected and
removed; no active final-ASR candidate remains. No ASR, speaker, microphone,
full-candidate, or industrial-closing gate advances from this experiment.
