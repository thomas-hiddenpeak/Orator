# Exact Phrase/VAD Direct-Conflict Diagnosis (2026-07-19)

## Scope and authority

This diagnosis follows the retained FR33 frozen replay and uses only the
source-hashed T111 and T123 typed producer tracks captured by the completed
T142 real-WebSocket runs. It does not rerun audio or modify diarization,
primary speaker, ASR, VAD, forced alignment, voiceprint evidence, TOML, or the
common time base. `test/data/reference/test.txt` remains the authoritative
human-listened reference.

No code, script, query, formula, score, or interval operation assigns
correctness, aggregates accuracy, selects FR34, or issues a product verdict.
Shell tools only display immutable typed evidence on the common time base. The
complete T142 review already identifies `ref-0406` as Tang Yunfeng's ten-
working-day confirmation assigned to Zhu Jie; that contextual judgment
precedes the evidence diagnosis below.

## Frozen conversation and output

The reference conversation is one direct handoff:

- `ref-0405`: Zhu Jie asks whether the stated term is twelve working days;
- `ref-0406`: Tang Yunfeng answers that it is ten working days;
- `ref-0407`: Zhu Jie continues that the money then arrives in ten working
  days.

T123 `text_id=236` spans `2726.140-2750.164` on the common clock. The current
FR33 replay assigns `2741.900-2744.700` to Zhu Jie, assigns
`2744.940-2746.060` (`对，才十个工作日，`) to Zhu Jie with reason
`voiceprint_direct_regular`, and assigns `2746.060-2746.940` back to Zhu Jie.
The exact typed phrase `punctuation_phrase:236:10` covers source characters
`111-118` and `2745.180-2746.060`; it contains the substantive ten-working-day
answer but not the preceding aligned `对` at source character `109`.

## Typed evidence topology

The stable identities are `spk_0=朱杰`, `spk_1=唐云峰`,
`spk_2=徐子景`, and `spk_3=石一` for this frozen session.

| Evidence | Session top two | Robust top two | Existing-gate state |
|---|---|---|---|
| Exact phrase `2745.180-2746.060` | Tang `0.330041`, Shi `0.149947` | Tang `0.321762`, Shi `0.158689` | Both pass short score and margin |
| Containing business interval `2741.900-2746.940` | Zhu `0.581918`, Tang `0.526831` | Zhu `0.551104`, Tang `0.483549` | Both pass regular score and margin |
| Unique containing VAD `2740.964-2751.996` | Tang `0.649018`, Shi `0.567317` | Tang `0.582888`, Shi `0.526458` | Both pass regular score and margin |

The native views expose an overlap rather than one agreed top identity:

- activity Xu Zijing covers `2741.519939-2747.279939`;
- activity Tang Yunfeng covers `2744.959939-2747.199939` and therefore covers
  the exact phrase completely;
- the sole overlapping primary segment selects Xu Zijing over
  `2741.599939-2747.199939`, also covering the exact phrase completely.

The existing projector evaluates the regular business interval first and
writes Zhu Jie. The exact phrase later sees a conflicting direct anchor. Its
short score is below the regular-score override path and activity/primary do
not agree on Tang, so ordinary phrase handling abstains. VAD voiceprint
evidence is available in the comprehensive timeline but is not projected by
that ordinary conflict path. This is a scale/provenance precedence problem;
changing a global score or making primary authoritative would not represent the
typed evidence.

## FR34 boundary

FR34 may write only the exact short phrase when all of the following are true:

1. Every current phrase character has one uniform
   `voiceprint_direct_regular` identity A and no protected overlay.
2. Exactly one containing same-text regular business interval has complete
   session and robust evidence, and both existing regular gates select A.
3. The exact phrase is embedding-backed, robust-complete, contains the existing
   minimum number of aligned units, and both existing short gates select one
   different identity B.
4. Exactly one containing typed VAD is embedding-backed and robust-complete;
   both of its existing duration-class gates independently select B.
5. One activity slot carrying B covers the complete phrase. Exactly one
   overlapping primary segment carrying C also covers it, activity C covers it,
   and no activity identity other than B or C overlaps it.
6. A, B, and C are known and pairwise different. Existing specialized
   challenges execute first. Any missing, duplicate, mixed, abstaining,
   disagreeing, uncovered, ambiguous, or out-of-range input leaves ordinary
   behavior unchanged.

This rule adds no TOML value or numerical threshold and inspects no transcript
value, speaker name, timestamp constant, reference label, or review result. It
does not extend the write to the preceding `对`, the following phrase, or the
coarse interval. The archived FR16AAG experiment used a broader historical
candidate and never completed the required full forward/reverse review; it is
background evidence only and supplies no current acceptance verdict.

## Validation sequence

Focused tests first establish the positive topology and abstention for every
required provenance, containment, gallery, alignment, activity, primary, and
identity condition. Warning-clean compilation and CTest are engineering
evidence only.

The production C++ projector then replays both frozen T123 and T111 inputs at
least twice. Automation may verify hashes, source reconstruction, time order,
immutable tracks, and change scope. Every changed complete conversation is
read chronologically and in reverse against `test.txt`; only that contextual
semantic review may retain or reject FR34. No new audio run occurs before the
frozen review passes.
