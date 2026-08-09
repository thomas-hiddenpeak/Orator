# Final ASR Prompt-Causality Review (2026-08-09)

## Status

- **Scope**: Spec 014 T045-T047, final ASR meaning only
- **Control**: clean full baseline `96b8347`, with the accepted Live publication
  correction frozen and FR50 speaker behavior unchanged
- **Result**: T045-T047 complete; one reference-free TOML candidate is active
  for staged validation but is not accepted
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

The only behavioral change in candidate `asr-empty-system-prompt` is:

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

The focused engineering test asserts that the checked-in TOML resolves an empty
ASR system prompt. The warning-clean build completes and all `74/74` registered
tests pass in `52.74s`. Existing model numerical gates remain unchanged because
no model weight, kernel, precision, feature extraction, encoder, or decoder
implementation changes.

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

## Current Conclusion

T045-T047 are complete. The current baseline's ASR meaning loss is already
present at the decoder output, and the strongest directly traced instance is
consistent with prompt contamination rather than VAD, forced alignment, or
comprehensive-timeline rewriting. Candidate `asr-empty-system-prompt` is active
as a one-variable experiment and ready for staged contextual review. No ASR,
speaker, microphone, full-candidate, or industrial-closing gate advances from
this diagnosis alone.
