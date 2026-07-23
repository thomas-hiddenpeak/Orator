# FR57 Speaker-Time Evidence Design

## Authority and purpose

FR57 completes the still-unsigned T102 speaker-time, per-speaker-time, and
source-time-offset review for the accepted FR50 real-path baseline. It reuses
the four completed full-context speaker readings; it does not reopen ASR
accuracy, tune a model, compare candidates, or run `test.mp3` again.

No compiled code, script, notebook, spreadsheet formula, query, metric, or
algorithm may assign a speaker-time judgment, map a duration to correctness,
total a judged duration, calculate an accuracy result, compare a threshold, or
issue a gate verdict. Automation may display immutable evidence and verify
hashes or schemas only. Every block judgment, total, calculation, comparison,
and decision is performed manually from complete conversational context.

## Frozen evidence

| Item | Frozen value |
|---|---|
| FR50 code | `a6f0d33730326b19a3831019b1aba21fd900f126` |
| Run A | `/tmp/orator-spec013/release-a6f0d33-fr50-precompute/run-full-a.json` |
| Run A SHA-256 | `33482f741d2467f28a436a6ffd90bdd0dd708e0e93af7cf814ef29c62d4781da` |
| Run B | `/tmp/orator-spec013/release-a6f0d33-fr50-precompute/run-full-b.json` |
| Run B SHA-256 | `7ba17a7caacbe39bafd747fefd8fade4600f509e6c6372a03d1af7c7d14be65c` |
| Human reference | `test/data/reference/test.txt` |
| Human reference SHA-256 | `35e8695057be82f3028877f7dc159f10ecfa0ab7f06c444cefa9a079b0e24a86` |
| Stream PCM SHA-256 | `17f0edda49989f3ceada60170885091023eeb9d67faae0d6dd67bb585b8857fe` |
| Checked-in TOML SHA-256 | `d00150ae376d802af0fcf8c0a89aa3fae1e0abb2bf5d10601c55cefd570a40db` |
| Run A forward packet | `730506a70dffeeef673051fdc9455fedb7e1e20bddd1f47e23a4beed63e13d69` |
| Run A reverse packet | `2e716af703b0045fadb43cd3804389be65f117add6fc961d858f5b74fc95adf6` |
| Run B forward packet | `d9a580288aa745aa894e997d1e08911a29ab23b5cb893841b147c725044e17d7` |
| Run B reverse packet | `bb6fb4fff19e64d630a3bee5245d634bd9bb9c0e999072c0467b7b68a3032a4a` |
| Identity map | `spk_0=Zhu Jie`, `spk_1=Tang Yunfeng`, `spk_2=Xu Zijing`, `spk_3=Shi Yi` |

Run A begins with an empty registry. Run B is an independently restarted real-
WebSocket run with the exact frozen Run A registry. Both full artifacts have
already received complete chronological and reverse contextual-semantic
speaker review. FR57 reuses those judgments but reviews their time extent
separately.

## Time-block contract

1. `test.txt` is the time and speaker authority. Its source marks have one-
   second precision. Runtime decimal timestamps may explain where evidence
   appears but may not create sub-second reference truth.
2. Every positive source interval is read with its complete neighbouring
   conversation. A correct natural contribution is not automatically fully
   correct by time, and an incorrect contribution is not automatically wrong
   for its entire source interval.
3. Duplicate source timestamps remain in line order and in the natural-turn
   denominator. Their time extent is decided explicitly from context. A zero-
   length display interval is not silently expanded, and a neighbouring row is
   not silently given another speaker's words.
4. The `ref-0446`/`ref-0447` backward timestamp pair is reviewed as one named
   context. No sort, interpolation, absolute-value repair, or sub-second split
   is permitted.
5. A source interval crossing `600`, `1200`, `1800`, `2400`, `3000`, or
   `3600` seconds is manually split at that exact whole-second boundary. The
   final partial block is reported separately.
6. Overlap may contribute speaker time to more than one speaker only when the
   complete conversation explicitly establishes simultaneous speech at the
   recorded whole-second precision. It is never inferred from duplicate text
   alone.
7. Every attribution-affecting source offset is listed even when the natural-
   turn result is accepted. Unsupported median, percentile, or sub-second
   reference statistics are prohibited.

## Review sequence

1. Read all 556 `test.txt` contributions chronologically and record the manual
   reference-time interpretation, including every exceptional timestamp.
2. Read the complete Run A conversation chronologically and assign its usable
   speaker blocks. Repeat independently for Run B.
3. Read Run A from the final block to the first and assign blocks again without
   importing the chronological duration labels. Repeat independently for Run B.
4. Reconcile each disagreement in prose. Preserve missing, uncertain, mixed,
   and source-displaced intervals rather than forcing a binary label.
5. Manually total full-session, complete fixed-block, and per-speaker source
   time for each run. Repeat the arithmetic independently from the reverse
   record and reconcile any mismatch.
6. Compare the manually checked values with the Spec 013 gates: at least 90.0
   percent full-session speaker time, at least 90.0 percent in every complete
   600-second block, and at least 90.0 percent for every canonical speaker.

## Decision boundary

FR57 may complete T102 only after all four readings, both independent manual
totals, every exceptional timestamp note, and every attribution-affecting
source offset agree. A time-gate pass does not repair the 19 critical FR50
residuals, satisfy the confident-wrong-zero requirement, complete T084, or
establish speaker-business closure.
