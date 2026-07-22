# FR50 Full-Residual Earliest-Loss Review (2026-07-23)

## Authority and current status

This is a human contextual-semantic review record for T226 and T227. The
authoritative reference is the complete human-listened conversation in
`test/data/reference/test.txt`. The reviewer reads that context together with
the unmodified FR49 business view, decision audit, ASR, forced alignment, VAD,
Sortformer activity and primary tracks, four-channel posterior, TitaNet
evidence, and local identity epochs on the common time base.

No code, script, query, formula, notebook, metric, hash, or equality check has
assigned a cause, grouped or counted a result, ranked a repair, selected a
candidate, or issued a product verdict. Automation only captured and displayed
raw evidence. The observations below are the independently completed **full A
chronological**, **full B chronological**, **full A reverse**, and **full B
reverse** readings. Every named control was reread in each pass. T226 and T227
are complete and the four readings have been manually reconciled. This record
still authorizes no production-code, TOML, model, replay, ledger, or closure
change; T228 must separately decide whether the evidence establishes a bounded
reference-free topology.

The contextual identity mapping used by the human reading is
`spk_0 = Zhu Jie`, `spk_1 = Tang Yunfeng`, `spk_2 = Xu Zijing`, and
`spk_3 = Shi Yi`. Local-slot identity is interpreted through the displayed
epoch evidence rather than assumed globally.

## Full A chronological reading

Each row was read as a complete conversational context, including every
accepted control named below. "Earliest observed layer" means the first place
where the human reviewer could no longer find usable evidence for the
speaker-business contribution, or where later processing displaced,
contradicted, or overwrote evidence that was still usable. It is a manual
observation, not an executable classification rule.

| Focus | Accepted controls reread | Contextual evidence reading | Earliest observed layer, provisional |
|---|---|---|---|
| `ref-0049` | `0048`, `0050`, `0051` | Tang's short `dui` lies between Shi and Zhu. ASR retains the token, but forced alignment puts the preceding `yi` and `dui` at the same `432.732 s` boundary while primary activity remains Shi. | Forced alignment: the usable token is displaced to a zero-duration boundary before final reconstruction. |
| `ref-0058` | `0056`, `0057`, `0060`, `0062` | The reference contribution is Tang's `xiang cha 0.7`. The displayed ASR source contains a different numerical phrase and does not retain that contribution as a separable semantic unit; activity is mixed. | Source/VAD/ASR: the referenced semantic contribution is not preserved for downstream attribution. |
| `ref-0063` | `0061`, `0062`, `0064`, `0065` | Shi's short `dui` should occur between neighboring sources, but no corresponding ASR/alignment unit is present. | Source/VAD/ASR: no usable text unit reaches speaker fusion. |
| `ref-0066` | `0062`, `0064`, `0065`, `0067`, `0068` | Tang repeats `ni men lia ke yi`; ASR retains only one occurrence and attaches it to Shi's source. Tang activity is visible during overlap, but the second semantic contribution is absent. | Source/VAD/ASR: the repeated contribution is lost before a separately attributable unit exists. |
| `ref-0099` | `0097`, `0098`, `0100`, `0101`, `0103` | Shi's full clause is retained by ASR and supported by alignment, activity, primary, and source-scale voiceprint evidence. A short direct voiceprint decision rewrites the middle `661.276-662.076 s` portion to Zhu. | Final fusion/source reconstruction: usable Shi evidence is present and later overwritten. |
| `ref-0102` | `0098`, `0100`, `0101`, `0103` | Tang's repeated `jiu bu yong kai hui le` is retained and aligned at `669.116-669.676 s`. Tang has secondary activity across the phrase, while primary favors Shi; short-span voiceprint puts Zhu slightly ahead of Tang and Shi lower. | Raw posterior/activity/primary versus final fusion remains contradictory; this boundary requires the independent reverse readings before reconciliation. |
| `ref-0118` | `0116`, `0117`, `0119`, `0120` | Tang says the first `dui a`, followed by Shi's `dui a, shuo dao si...`. ASR preserves two reactions. Tang activity covers the first aligned phrase at `806.732-806.972 s`, but reconstruction splits it into Shi's `dui` and Tang's `a`. | Final fusion/source reconstruction: a usable short-turn activity island is split by the larger reconstructed source. |
| `ref-0135` | `0133`, `0134`, `0136`, `0137`, `0138` | Tang's `wo jiu` brackets Shi's `dui ba`. ASR merges them as one phrase. Shi has an exact secondary island at `896.160-896.400 s`, and phrase voiceprint also slightly favors Shi, but the micro-island is filtered and the whole phrase is assigned to Tang. | Final fusion/source reconstruction: separable Shi evidence is present but filtered inside a merged phrase. |
| `ref-0171` | `0168`, `0169`, `0170`, `0172` | The reference gives Shi `zuo de hao dian, gen yu qi`. The substantive interval's primary and TitaNet evidence favor Tang; only a small trailing Shi activity run remains. | Raw posterior/activity/primary and identity/gallery: the upstream speaker evidence is contradictory before final fusion. |
| `ref-0221` | `0217`, `0218`, `0219`, `0220`, `0222`, `0223` | Xu's clause is textually imperfect but retained. Raw diarization and primary switch to Xu at `1522.080 s` and cover the phrase. The opening reaction remains Xu, while a near voiceprint decision rewrites the rest to Tang. | Final fusion/source reconstruction: sustained usable Xu primary evidence is overwritten by the competing voiceprint branch. |
| `ref-0239` | `0238`, `0240`, `0242`, `0243` | Xu's two reactions appear as two ASR `a` units. The short raw diarization view favors Tang, while VAD-, primary-run-, and complete-source-scale TitaNet evidence favor Xu. Final fusion follows only the diarization identity. | Final fusion/source reconstruction: useful source-scale identity evidence is available but not used in the final choice. |
| `ref-0241` | `0238`, `0240`, `0242`, `0243` | Xu's `sha mao zi` has a separate VAD unit, but raw primary favors Tang and TitaNet also narrowly favors Tang; Xu is only secondary. | Raw posterior/activity/primary and identity/gallery: the displayed upstream evidence does not provide a stable Xu basis. |
| `ref-0252` | `0251`, `0253`, `0254` | Zhu's `dui a, liang wei lao ban jue ding a` is fragmented by ASR. The displayed raw model and TitaNet evidence support Shi or Tang rather than Zhu. | Source/VAD/ASR, followed by contradictory raw and identity evidence: no stable Zhu unit reaches final fusion. |
| `ref-0298` | `0294`, `0295`, `0296`, `0297`, `0299`, `0300`, `0301` | Tang's `wo gen ni men shuo` loses `shuo` in ASR. Raw primary favors Shi and source-scale TitaNet favors Xu, with Tang only nearby. | Source/VAD/ASR and upstream speaker evidence: the contribution and its identity evidence are already incomplete or contradictory. |
| `ref-0313` | `0311`, `0314` | Shi's `dui` after Tang's `dui ba` is absent from the text source. A second raw local-slot island exists at `2247.920-2248.320 s`, but its current epoch maps to Tang. | Source/VAD/ASR: no target text unit exists; the remaining identity evidence is also not usable for Shi. |
| `ref-0327` | `0326`, `0328`, `0329` | Shi's `dui` after Tang's `shou quan ba` has an exact Shi primary island at `2344.400-2344.800 s`. Alignment places `dui` at a zero-duration `2344.780 s` boundary, and the business span is extended backward and assigned to Tang. | Forced alignment, then final reconstruction: usable Shi activity survives, but the text boundary prevents its use. |
| `ref-0331` | `0329`, `0330`, `0332` | Shi's `dui` falls between Tang sources. A Shi primary island is present at `2362.800-2363.200 s`, but the preceding Tang ASR question ends without a separately retained target token. | Source/VAD/ASR: the speaker island exists, but there is no corresponding text unit to attribute. |
| `ref-0333` | `0332`, `0334`, `0335` | Shi's `dui` is appended to a long Tang ASR source at `2375.676 s`. A raw local-slot island exists, but the active epochs map both relevant local slots to Tang and voiceprint is unavailable. | Raw posterior/activity/primary and identity/gallery: the source is merged and available speaker evidence does not distinguish Shi. |
| `ref-0341` | `0338`, `0339`, `0340`, `0342`, `0343` | Tang's `feng le` is absent or mistranscribed inside a long Shi source; raw activity and voiceprint favor Shi. | Source/VAD/ASR: the contribution is not retained as usable text, with no compensating Tang evidence downstream. |
| `ref-0354` | `0352`, `0353`, `0355`, `0356` | Zhu's `dui, du li gong si` is present in ASR, but the active local slots both map to Tang; primary favors Tang and phrase voiceprint does not support Zhu. | Raw posterior/activity/primary and identity/gallery: the upstream identity evidence is contradictory for Zhu. |
| `ref-0390` | `0389`, `0391` | Tang's `dui` is appended to Shi's preceding phrase and aligned at zero duration at `2690.380 s`; the clear Tang primary run starts later at `2690.800 s`. | Forced alignment: token placement precedes the usable Tang onset. |
| `ref-0409` | `0405`, `0406`, `0407`, `0408`, `0410` | Zhu's `ma shang zan men ye neng chui yi chui` is entirely absent between retained ASR sources. Raw activity is weak and maps toward Tang or Shi, with no stable Zhu evidence. | Source/VAD/ASR: no attributable semantic unit reaches downstream processing. |
| `ref-0417` | `0413`, `0414`, `0415`, `0416`, `0418`, `0419`, `0420`, `0421`, `0422` | Tang's `en` precedes Shi's long sentence. ASR and alignment retain it at `2804.396-2804.556 s`, and Tang primary at `2804.320-2804.720 s` covers it exactly. Whole-phrase reconstruction nevertheless assigns the combined source to Shi. | Final fusion/source reconstruction: a correct short-turn primary island is swallowed by the longer source-scale decision. |
| `ref-0426` | `0425`, `0427` | Shi's long `B he C yi qi...` clause is garbled by ASR. Raw top activity alternates between Xu and Tang with Shi only secondary; voiceprint likewise favors Tang or Xu except for a later phrase. | Source/VAD/ASR and raw speaker evidence: the semantic unit and stable Shi ownership are both lost upstream. |
| `ref-0442` | `0441`, `0443` | Zhu's `shen me jia ge` is absent; ASR retains only neighboring material, while raw activity favors Tang or Shi and provides no Zhu support. | Source/VAD/ASR: no target semantic unit or usable Zhu evidence reaches fusion. |
| `ref-0444` | `0443`, `0445` | Zhu's `shen me jia ge` is reduced to `jia ge`; raw activity favors Xu and phrase voiceprint favors Tang, not Zhu. | Source/VAD/ASR and contradictory speaker evidence: the contribution is incomplete before final fusion. |
| `ref-0457` | `0455`, `0456`, `0458`, `0459`, `0460` | Shi's `sha a` is recognized as `shi ba`. Xu dominates raw activity; only a small Shi primary onset appears at the tail. | Source/VAD/ASR and raw posterior/activity/primary: neither text nor speaker evidence provides a robust Shi unit. |
| `ref-0461` | `0460`, `0462` | Tang's financial-responsibility clause is split by ASR. Primary and TitaNet favor Shi; Tang activity appears only in the early overlap. | Raw posterior/activity/primary and identity/gallery: the substantive source has contradictory Tang ownership before fusion. |
| `ref-0499` | `0498`, `0500`, `0501` | Shi's `o, mei qu bie` is reconstructed first as Zhu, then Tang, then no speaker. Raw local identities transition from Zhu to Tang and TitaNet supplies no stable Shi support. | Raw posterior/activity/primary and identity/gallery: the displayed evidence itself changes identity across the short contribution. |
| `ref-0503` | `0501`, `0502`, `0504` | Zhu's long `3268-3299 s` turn begins with Zhu-supported source, primary, and voiceprint evidence. Later ASR chunks are mostly supported as Shi, with only limited later evidence again near Zhu. Conversational continuity suggests a possible cue, but the evidence is not uniformly Zhu and the gaps are material. | Raw posterior/activity/primary and identity/gallery for most of the turn; a broad discourse-continuity rewrite would not yet be low blast. |
| `ref-0505` | `0504` | Tang's clause has a coarse reference timestamp and a semantic match earlier near `3301.4 s`. The displayed raw activity and voiceprint evidence favor Shi throughout the candidate source, not Tang. | Alignment plus raw/identity evidence: the reference contribution is temporally coarse and no stable Tang evidence supports a final-only repair. |
| `ref-0506` | `0504`, `0507`, `0508`, `0509`, `0510` | Tang's `ai ya` is retained by ASR/VAD/alignment near `3312.970 s`, but there is no qualifying diarization or primary run; all posterior channels remain below the producer threshold and weak TitaNet evidence favors another speaker. | Raw posterior/activity/primary: usable speaker evidence is absent even though the text unit exists. |
| `ref-0537` | `0535`, `0536`, `0538`, `0539` | Tang's `shuai shou zhang gui` is merged or mistranscribed within a long Shi source. Raw activity and all displayed voiceprint views strongly support Shi. | Source/VAD/ASR and upstream speaker evidence: no separable Tang contribution survives for final fusion. |

## Full B chronological reading

The B artifact tree was read independently from its own raw files rather than
accepted through A/B hash equality. The same definition of "earliest observed
layer" applies. Agreement with an A observation means that the second reading
reached the same contextual interpretation; it is not a mechanical equality
verdict.

| Focus | Accepted controls reread | Independent B contextual evidence reading | Earliest observed layer, provisional |
|---|---|---|---|
| `ref-0049` | `0048`, `0050`, `0051` | Tang's short `dui` remains in ASR between Shi and Zhu, while alignment collapses it with the preceding unit at `432.732 s`; primary activity at that boundary remains Shi. | Forced alignment: the token has no usable duration before final reconstruction. |
| `ref-0058` | `0056`, `0057`, `0060`, `0062` | The displayed text substitutes a different numerical phrase for Tang's `xiang cha 0.7`, and mixed activity cannot restore the missing semantic contribution. | Source/VAD/ASR: no separable target contribution reaches attribution. |
| `ref-0063` | `0061`, `0062`, `0064`, `0065` | A short Shi-shaped activity island is visible, but the corresponding `dui` does not exist in ASR or alignment. | Source/VAD/ASR: speaker timing survives without a text unit. |
| `ref-0066` | `0062`, `0064`, `0065`, `0067`, `0068` | ASR retains only one occurrence of Tang's repeated `ni men lia ke yi` and embeds it in Shi's source; the other occurrence is not available as text. | Source/VAD/ASR: the repeated semantic unit is lost before final fusion. |
| `ref-0099` | `0097`, `0098`, `0100`, `0101`, `0103` | Shi activity and primary evidence continue through the clause, but `ta men jiu sha dou bie shuo` at `661.276-662.076 s` is rewritten to Zhu by a short direct voiceprint branch. | Final fusion/source reconstruction: sustained usable Shi evidence is overwritten. |
| `ref-0102` | `0098`, `0100`, `0101`, `0103` | Tang has secondary activity across `jiu bu yong kai hui le`, Shi is primary, and short-span voiceprint favors Zhu with Tang next. The evidence remains internally contradictory. | Raw posterior/activity/primary versus identity/gallery and final fusion remains unresolved pending reverse review. |
| `ref-0118` | `0116`, `0117`, `0119`, `0120` | The first aligned `dui a` overlaps Tang activity, but final reconstruction divides the two characters between Shi and Tang before Shi's following sentence. | Final fusion/source reconstruction: the short turn is split inside a retained phrase. |
| `ref-0135` | `0133`, `0134`, `0136`, `0137`, `0138` | Shi has a precisely timed secondary island at `896.160-896.400 s`; both phrase galleries slightly favor Shi, yet the merged `wo jiu dui ba` phrase remains Tang after micro-island filtering. | Final fusion/source reconstruction: usable short-turn evidence is filtered inside a merged phrase. |
| `ref-0171` | `0168`, `0169`, `0170`, `0172` | The substantive part of `zuo de hao dian, gen yu qi` is supported as Tang by primary activity and source-scale voiceprint, with Shi appearing only at the tail. | Raw posterior/activity/primary and identity/gallery: the upstream evidence conflicts with the reference ownership. |
| `ref-0221` | `0217`, `0218`, `0219`, `0220`, `0222`, `0223` | Xu raw diarization and primary cover the retained clause from `1522.080 s`; final output keeps the opening reaction as Xu but rewrites the rest to Tang through phrase voiceprint. | Final fusion/source reconstruction: sustained usable Xu evidence is displaced. |
| `ref-0239` | `0238`, `0240`, `0242`, `0243` | The two retained reactions are assigned to Tang from the short diarization interval, although broader VAD, primary-run, and complete-source voiceprint evidence supports Xu. | Final fusion/source reconstruction: useful cross-scale identity evidence is present but the final choice follows only the short diarization view. |
| `ref-0241` | `0238`, `0240`, `0242`, `0243` | `sha mao zi` is retained in a separate VAD span, but its primary run and both voiceprint galleries favor Tang; Xu remains weak rather than decisive. | Raw posterior/activity/primary and identity/gallery: no stable Xu evidence reaches fusion. |
| `ref-0252` | `0251`, `0253`, `0254` | The Zhu response is textually fragmented across neighboring phrases. Primary activity remains Tang, the short overlapping slot also resolves to Tang, and no displayed identity evidence establishes Zhu. | Source/VAD/ASR followed by raw identity evidence: no stable Zhu contribution reaches final fusion. |
| `ref-0298` | `0294`, `0295`, `0296`, `0297`, `0299`, `0300`, `0301` | ASR retains `wo gen ni men` but drops `shuo`; raw primary identifies Shi, while phrase/source voiceprint favors Xu with Tang only nearby. | Source/VAD/ASR and upstream speaker evidence: both contribution and ownership are incomplete or contradictory. |
| `ref-0313` | `0311`, `0314` | Shi's response after Tang's `dui ba` is absent from ASR. The remaining local-slot island at `2247.920-2248.320 s` resolves to Tang in the displayed epoch. | Source/VAD/ASR: no target text exists, and remaining speaker evidence is not usable for Shi. |
| `ref-0327` | `0326`, `0328`, `0329` | Shi primary activity exactly covers `2344.400-2344.800 s`, but alignment places `dui` at the zero-duration `2344.780 s` boundary and reconstruction backfills its span into Tang's preceding region. | Forced alignment, then final reconstruction: correct Shi timing exists but is detached from the token. |
| `ref-0331` | `0329`, `0330`, `0332` | Shi primary activity appears at `2362.800-2363.200 s`, but neither adjacent ASR source contains a separately attributable response. | Source/VAD/ASR: the activity island has no corresponding text unit. |
| `ref-0333` | `0332`, `0334`, `0335` | The final `dui` is retained at the end of Tang's long source. Its short local slot also maps to Tang in the current identity epoch, and no voiceprint evidence separates Shi. | Raw posterior/activity/primary and identity/gallery: retained text lacks usable Shi identity evidence. |
| `ref-0341` | `0338`, `0339`, `0340`, `0342`, `0343` | Tang's `feng le` is not retained as that contribution; the surrounding source is recognized and identified as Shi throughout. | Source/VAD/ASR: no separable Tang text or speaker evidence survives. |
| `ref-0354` | `0352`, `0353`, `0355`, `0356` | `dui, du li gong si` is retained, but both overlapping local slots resolve to Tang, primary remains Tang, and the displayed galleries do not identify Zhu. | Raw posterior/activity/primary and identity/gallery: Zhu ownership is already unavailable upstream. |
| `ref-0390` | `0389`, `0391` | Tang's `dui` is appended to Shi's question at a zero-duration `2690.380 s` boundary; a usable Tang primary run starts only around `2690.800 s`. | Forced alignment: the token precedes the usable Tang evidence. |
| `ref-0409` | `0405`, `0406`, `0407`, `0408`, `0410` | Zhu's `ma shang zan men ye neng chui yi chui` is missing between two retained ASR sources; the raw interval maps toward Tang and then Shi rather than Zhu. | Source/VAD/ASR: the target semantic contribution never reaches speaker fusion. |
| `ref-0417` | `0413`, `0414`, `0415`, `0416`, `0418`, `0419`, `0420`, `0421`, `0422` | Tang primary activity at `2804.320-2804.720 s` covers the aligned opening `en` at `2804.396-2804.556 s`; the following stable Shi run causes the whole combined phrase to be emitted as Shi. | Final fusion/source reconstruction: a correct short prefix island is swallowed by the longer source decision. |
| `ref-0426` | `0425`, `0427` | The long reference clause is semantically garbled in ASR. Raw primary alternates among Tang, Xu, and short Shi spans, while phrase decisions also alternate instead of establishing one Shi turn. | Source/VAD/ASR and raw speaker evidence: neither a stable contribution nor stable ownership reaches final fusion. |
| `ref-0442` | `0441`, `0443` | Zhu's `shen me jia ge` is absent; neighboring text is retained, but raw activity only supplies Tang and Shi identities. | Source/VAD/ASR: there is no target text or usable Zhu speaker evidence. |
| `ref-0444` | `0443`, `0445` | Only `jia ge` survives from Zhu's contribution. Its raw interval is Xu and short-span identity evidence favors Tang, with no Zhu basis. | Source/VAD/ASR and contradictory speaker evidence: the contribution is incomplete before fusion. |
| `ref-0457` | `0455`, `0456`, `0458`, `0459`, `0460` | Shi's `sha a` is rendered as `shi ba`; Xu owns almost all matching activity, with only a tiny Shi primary tail. | Source/VAD/ASR and raw posterior/activity/primary: no robust Shi unit reaches final fusion. |
| `ref-0461` | `0460`, `0462` | Tang's clause is split and altered by ASR. Shi owns the substantive primary interval and phrase evidence, while Tang appears only in early overlap. | Raw posterior/activity/primary and identity/gallery: substantive ownership already contradicts Tang. |
| `ref-0499` | `0498`, `0500`, `0501` | The retained `o, mei qu bie` passes through Zhu, Tang, and an unassigned tail. Raw diarization and primary make the same Zhu-to-Tang transition and never establish Shi. | Raw posterior/activity/primary and identity/gallery: no stable Shi path reaches final fusion. |
| `ref-0503` | `0501`, `0502`, `0504` | The long Zhu turn opens with a Zhu-supported chunk, but subsequent independently segmented sources are predominantly identified as Shi, with gaps and an intervening Xu island. | Raw posterior/activity/primary and identity/gallery for most of the turn; continuity alone is insufficient for a low-blast rewrite. |
| `ref-0505` | `0504` | The semantic match occurs earlier than the coarse reference interval, near `3301.404 s`, where raw diarization, primary, and final output all support Shi rather than Tang. | Alignment plus raw/identity evidence: no stable Tang basis exists for a final-only change. |
| `ref-0506` | `0504`, `0507`, `0508`, `0509`, `0510` | `ai ya` is retained and aligned, but no diarization or primary interval qualifies at its time. The final view explicitly abstains rather than borrowing a neighboring identity. | Raw posterior/activity/primary: speaker evidence is absent; final abstention preserves that uncertainty. |
| `ref-0537` | `0535`, `0536`, `0538`, `0539` | Tang's `shuai shou zhang gui` is rendered inside the long Shi phrase as `shu zhang gui, shu ren zhang gui`; raw activity, primary, and source-scale identity remain Shi. | Source/VAD/ASR and upstream speaker evidence: no separable Tang unit reaches final fusion. |

## Full A reverse reading

This pass started at the end of the conversation and independently reread each
complete reference section and every accepted control before consulting A's
raw tracks. The dependency column records whether changing ASR, VAD, forced
alignment, or raw speaker evidence would remove or materially alter the
evidence just read. It does not propose such a change or predict its effect.

| Focus, reverse order | Independent A reverse observation | Upstream dependency exposed by this reading |
|---|---|---|
| `ref-0537` | The target contribution remains embedded and mistranscribed in Shi's long source; activity, primary, and all displayed identity views support Shi. | A final-only speaker rule has no separable Tang unit. ASR or raw-speaker changes would create different evidence and require a new full review. |
| `ref-0506` | `ai ya` is retained near `3312.970 s`, but no diarization or primary run qualifies and weak voiceprint evidence points elsewhere. The unassigned final span preserves the missing evidence. | Any VAD/diarization change that creates a qualifying run would invalidate this abstention record; current evidence does not authorize borrowing a neighbor. |
| `ref-0505` | The semantic match lies earlier than the coarse reference time, near `3301.4 s`; raw activity and identity evidence there support Shi, not Tang. | Better source timing alone would not establish Tang. Alignment and raw-speaker evidence would both need fresh contextual review. |
| `ref-0503` | Zhu is supported at the opening of the long turn, while later source chunks are mostly supported as Shi, with gaps and an intervening Xu island. | A broad continuity rewrite would depend on reference-only discourse interpretation. Re-segmentation or new raw evidence would invalidate this reading. |
| `ref-0499` | `o, mei qu bie` crosses Zhu, Tang, and unassigned final spans; the raw tracks never establish a stable Shi interval. | The loss precedes final fusion. A VAD or diarization change could alter the available identities and therefore requires complete rereading. |
| `ref-0461` | Tang's clause is split and altered, while the substantive primary and TitaNet evidence support Shi; Tang appears only in early overlap. | ASR segmentation and raw-speaker ownership are both material. The current evidence cannot support a final-only Tang override. |
| `ref-0457` | Shi's `sha a` is recognized as `shi ba`; Xu dominates the matching activity and only a small Shi tail remains. | Both the semantic unit and speaker ownership are upstream-dependent; a tail island is insufficient for a broad rewrite. |
| `ref-0444` | Only `jia ge` survives, with Xu raw activity and Tang voiceprint evidence rather than Zhu. | ASR incompleteness and contradictory raw identity prevent a final-only repair. |
| `ref-0442` | Zhu's question is absent and the nearby raw tracks contain Tang and Shi, not Zhu. | There is no text unit to attribute. Any ASR recovery would produce a new evidence case. |
| `ref-0426` | The long reference contribution is garbled, while raw primary and identity evidence alternate among Tang, Xu, and short Shi regions. | ASR and raw-speaker evidence are both unstable; no final-fusion topology is established. |
| `ref-0417` | Tang's aligned `en` at `2804.396-2804.556 s` is fully covered by the distinct Tang primary run at `2804.320-2804.720 s`, but the combined source is emitted as Shi. | This is final reconstruction loss under the current alignment and primary run. Moving or deleting either upstream boundary would invalidate the evidence. |
| `ref-0409` | Zhu's contribution is absent between retained sources and the raw interval supplies no stable Zhu identity. | A final-only policy cannot reconstruct missing text or identity; ASR/VAD changes would create a different case. |
| `ref-0390` | Tang's `dui` is zero-duration at `2690.380 s`, before the clear Tang primary run begins near `2690.800 s`. | Alignment is the first unusable boundary and there is no exact correct island for final fusion to consume. |
| `ref-0354` | `dui, du li gong si` is retained, but the overlapping slots, primary run, and phrase identity all resolve away from Zhu. | Speaker evidence is already contradictory upstream; preserving the text does not authorize a Zhu override. |
| `ref-0341` | Tang's `feng le` is absent or mistranscribed inside a long Shi source whose raw and identity evidence remain Shi. | ASR and raw-speaker evidence both precede the loss; final fusion has no separable Tang input. |
| `ref-0333` | Shi's `dui` is appended to Tang's long source; the short local slot maps to Tang in the active epoch and no voiceprint evidence distinguishes Shi. | The text survives, but identity does not. Changing the epoch or raw slot would invalidate this reading. |
| `ref-0331` | A Shi primary island remains at `2362.800-2363.200 s`, but no adjacent ASR source contains the corresponding response. | ASR is the missing contract. The activity island alone cannot create business text. |
| `ref-0327` | Shi primary activity at `2344.400-2344.800 s` survives exactly around the response, while forced alignment collapses `dui` to `2344.780 s` and reconstruction backfills its span into Tang's preceding region. | Alignment and final reconstruction jointly lose usable evidence. This record is valid only while both the token point and primary island remain unchanged. |
| `ref-0313` | Shi's response is absent from text; the remaining local-slot island at `2247.920-2248.320 s` maps to Tang in the active epoch. | ASR and identity evidence are both unusable for Shi, so this is not a final-only candidate. |
| `ref-0298` | `wo gen ni men` survives without `shuo`; primary supports Shi and phrase/source voiceprint favors Xu, not Tang. | Both semantic completeness and ownership are upstream-dependent; no stable Tang evidence reaches fusion. |
| `ref-0252` | Zhu's response is fragmented across a merged source while primary and identity evidence support Shi or Tang. | ASR segmentation and raw identity both fail to provide a Zhu unit. |
| `ref-0241` | `sha mao zi` survives as a separate VAD region, but primary and both identity galleries support Tang; Xu is only weak secondary evidence. | The loss is in raw identity, not in final selection. A final override would lack a stable Xu basis. |
| `ref-0239` | The two reactions survive. Short diarization resolves to Tang, while the primary-run, VAD-scale, and complete-source identity views support Xu; final fusion uses only the short diarization identity. | Current ASR and alignment preserve the unit. This is a final evidence-usage conflict, but changing VAD or source extent could remove the broader Xu evidence. |
| `ref-0221` | The retained clause has sustained Xu diarization and primary support from `1522.080 s`; final output keeps the opening reaction as Xu but rewrites the rest to Tang through phrase identity. | The current alignment and primary transition are material. This is final overwrite only while the retained semantic unit and Xu run remain intact. |
| `ref-0171` | The substantive interval is primary- and voiceprint-supported as Tang; only a small trailing Shi region remains. | Raw-speaker evidence conflicts before final fusion. No low-impact final-only correction follows from the current tracks. |
| `ref-0135` | ASR merges Tang's `wo jiu`, Shi's `dui ba`, and Tang's continuation. A short Shi posterior/phrase-identity cue exists, but primary becomes Tang and the whole phrase remains Tang. | Source segmentation, alignment, and the competing speaker views are all material; the current evidence is too contradictory for selection. |
| `ref-0118` | Two reactions are retained. Tang activity overlaps the first `dui a`, but the primary transition occurs inside the aligned phrase and final reconstruction assigns `dui` to Shi and `a` to Tang. | The loss is reconstruction at a sub-phrase transition; different alignment or primary boundaries would change the case. |
| `ref-0102` | The repeated `jiu bu yong kai hui le` is retained. Shi is primary, Tang is a secondary diarization channel, and short-span voiceprint is weak and split between Zhu and Tang. | The evidence remains genuinely contradictory. No branch is authorized; any upstream boundary or identity change requires full rereading. |
| `ref-0099` | Shi's complete clause retains sustained activity, primary, and source-scale identity support, but a short direct voiceprint branch rewrites the middle phrase to Zhu. | This is final overwrite under the current text and timing. A changed phrase boundary or gallery result would invalidate the observation. |
| `ref-0066` | ASR contains only one `ni men lia ke yi` occurrence inside Shi's long source; Tang activity overlaps part of it, while the repeated contribution itself is unavailable. | ASR/source segmentation loses the business unit before final fusion. |
| `ref-0063` | A clear Shi activity and primary island exists at `471.440-472.160 s`, but no target token appears in either adjacent ASR source. | ASR is the missing evidence; the speaker island cannot be projected without text. |
| `ref-0058` | The displayed numerical text does not retain Tang's `xiang cha 0.7`, and the overlapping speaker activity is mixed. | A semantic unit is missing upstream; final attribution cannot restore it. |
| `ref-0049` | Tang's `dui` is present, but both it and the preceding `yi` are collapsed to `432.732 s`; primary remains Shi through that boundary. | Forced alignment removes a usable token interval before final reconstruction. |

The A reverse reading does not disagree with the A chronological reading. That
statement is a human reconciliation of complete contexts, not an automated
comparison. It also does not yet establish a shared repair: `ref-0327` and
`ref-0417` both expose a short contribution absorbed by a longer source, but
one depends on a zero-duration aligned token and the other retains a positive
aligned interval. `ref-0102` remains contradictory, and `ref-0503` remains
ineligible for a broad continuity rewrite.

## Full B reverse reading

This pass started independently at `ref-0537`, used B's own worksheet tree,
and reread each complete reference section and every named control before the
raw B tracks. Agreement below is a manual contextual reconciliation; artifact
hashes or equality checks did not supply any observation or verdict.

| Focus, reverse order | Independent B reverse observation | Upstream dependency exposed by this reading |
|---|---|---|
| `ref-0537` | Tang's contribution remains merged and mistranscribed inside a long Shi source; raw activity, primary, and every displayed identity view support Shi. | No separable Tang unit reaches final fusion. ASR or raw-speaker changes would create a different case. |
| `ref-0506` | `ai ya` remains in ASR and alignment near `3312.970 s`, but there is no qualifying diarization or primary run and only weak, conflicting identity evidence. The final span remains unassigned. | The abstention is evidence-preserving. A VAD or diarization change that creates a run would require a new contextual review. |
| `ref-0505` | The semantic match is earlier than the coarse reference interval, near `3301.4 s`; raw diarization, primary, and identity there support Shi rather than Tang. | Alignment and raw identity are both material; current evidence cannot authorize a Tang override. |
| `ref-0503` | The long contribution opens with Zhu evidence, then crosses predominantly Shi sources, a Xu island, and material gaps. | Broad continuity is not reference-free. Re-segmentation or changed raw evidence would invalidate this reading. |
| `ref-0499` | The retained reaction traverses Zhu, Tang, and an unassigned tail; no raw or identity view establishes a stable Shi interval. | The loss precedes final fusion and depends on the current VAD and diarization boundaries. |
| `ref-0461` | Tang's clause is split and altered, while the substantive raw and phrase-scale identity evidence support Shi; Tang appears only in early overlap. | ASR segmentation and raw ownership are both upstream dependencies. |
| `ref-0457` | Shi's `sha a` remains recognized as `shi ba`; Xu dominates the matching activity and only a tiny Shi tail remains. | Neither the semantic unit nor a stable Shi interval reaches final fusion. |
| `ref-0444` | Only `jia ge` survives; raw activity supports Xu and phrase identity supports Tang rather than Zhu. | ASR is incomplete and speaker evidence is contradictory before final selection. |
| `ref-0442` | Zhu's question is absent, and nearby raw activity supports Tang or Shi rather than Zhu. | No text or Zhu identity unit is available to final fusion. |
| `ref-0426` | The long clause remains garbled and the matching raw and identity evidence alternates among Tang, Xu, and short Shi regions. | Source semantics and raw ownership are both unstable; no final-only repair follows. |
| `ref-0417` | Tang's aligned `en` at `2804.396-2804.556 s` is fully contained by the distinct Tang primary run at `2804.320-2804.720 s`; the merged source is nevertheless emitted as Shi. | This is final reconstruction loss only while the positive alignment and exact primary island remain unchanged. |
| `ref-0409` | Zhu's contribution is absent between adjacent ASR sources, and the weak raw interval does not establish Zhu. | Final fusion cannot construct missing semantic or identity evidence. |
| `ref-0390` | Tang's `dui` is still a zero-duration token at `2690.380 s`; the clear Tang primary run begins later near `2690.800 s`. | Forced alignment loses the usable boundary before final fusion, with no exact Tang island covering the token. |
| `ref-0354` | `dui, du li gong si` remains in ASR, but both active local slots map to Tang, primary is Tang, and phrase identity does not establish Zhu. | Upstream speaker identity is contradictory for Zhu. |
| `ref-0341` | Tang's `feng le` remains absent inside a long Shi source whose activity, primary, and identity evidence support Shi. | ASR loses the separable contribution and downstream evidence cannot restore it. |
| `ref-0333` | Shi's `dui` is appended to Tang's source; the short local slot also maps to Tang in the active epoch and voiceprint is unavailable. | Text survives, but no usable Shi identity reaches final fusion. |
| `ref-0331` | A Shi primary island remains at `2362.800-2363.200 s`, but neither adjacent source retains the corresponding response token. | ASR is the missing contract; activity alone cannot create business text. |
| `ref-0327` | Shi primary activity at `2344.400-2344.800 s` contains the `dui` point at `2344.780 s`; reconstruction instead backfills the token span into Tang's preceding region. | Alignment and reconstruction jointly lose the usable evidence; either boundary changing invalidates the case. |
| `ref-0313` | Shi's response remains absent from ASR; the remaining local-slot island at `2247.920-2248.320 s` maps to Tang in the active epoch. | Both text and identity are unusable for a final-only Shi repair. |
| `ref-0298` | ASR retains `wo gen ni men` but drops `shuo`; primary supports Shi and phrase/source identity favors Xu, with Tang only nearby. | Semantic completeness and speaker ownership are already contradictory upstream. |
| `ref-0252` | Zhu's response is fragmented across one source while primary and identity evidence support Shi or Tang rather than Zhu. | Neither source segmentation nor raw identity supplies a stable Zhu unit. |
| `ref-0241` | `sha mao zi` survives in its own VAD region, but primary and both identity galleries support Tang; Xu remains weak. | The loss is in upstream identity rather than final selection. |
| `ref-0239` | The two reactions survive. Short diarization maps to Tang, while primary-run-, VAD-, and complete-source-scale identity support Xu; final fusion uses only the short diarization identity. | This is a final evidence-usage conflict under the exact source and VAD extents. |
| `ref-0221` | The retained, imperfect clause has sustained Xu diarization and primary support from `1522.080 s`; final output keeps its opening as Xu but rewrites the rest to Tang through phrase identity. | This remains final overwrite only while the current alignment and Xu run stay intact. |
| `ref-0171` | The substantive interval alternates locally but its main primary and phrase identity evidence support Tang; only a short trailing region supports Shi. | Raw speaker evidence conflicts before final fusion, so no low-impact Shi override is established. |
| `ref-0135` | ASR merges Tang's `wo jiu`, Shi's `dui ba`, and Tang's continuation. A short Shi posterior cue and slight phrase-identity cue survive, while primary and the emitted phrase remain Tang. | Source segmentation, alignment, posterior, primary, and identity are mutually material and too contradictory for selection. |
| `ref-0118` | The first `dui a` is retained, but the primary transition falls inside it: reconstruction assigns `dui` to Shi and `a` to Tang before Shi's next reaction. | This is a sub-phrase reconstruction loss whose result depends on the exact alignment and primary transition. |
| `ref-0102` | The repeated phrase is retained; Shi is primary, Tang is secondary activity, and short-span identity is weak and split among Zhu, Tang, and Shi. | The evidence is genuinely contradictory. No branch is authorized, and any upstream change requires rereading. |
| `ref-0099` | Shi's full clause retains sustained activity, primary, and source-scale identity support, but a short direct voiceprint branch rewrites the middle phrase to Zhu. | This is final overwrite under the exact text, phrase boundary, and gallery evidence. |
| `ref-0066` | Only one `ni men lia ke yi` occurrence survives inside Shi's long source; Tang activity overlaps part of it, but the repeated contribution itself is unavailable. | ASR/source segmentation loses the business unit before final attribution. |
| `ref-0063` | A Shi activity and primary island survives at `471.440-472.160 s`, but neither adjacent ASR source contains the target response. | ASR is the missing evidence; final fusion must not invent text. |
| `ref-0058` | The displayed numerical text does not retain Tang's `xiang cha 0.7`, and the overlapping activity remains mixed. | The semantic unit is already missing upstream. |
| `ref-0049` | Tang's `dui` is present, but it and the preceding `yi` collapse to the same `432.732 s` point while primary remains Shi. | Forced alignment removes a usable token interval before reconstruction. |

The B reverse reading does not disagree with B chronological or either A
reading. This is a manual reconciliation of all complete contexts and controls.
In particular, `ref-0102` remains contradictory, `ref-0503` remains unsuitable
for a continuity rewrite, and source-absent cases remain ineligible for any
final-fusion rule.

## Four-pass reconciliation

The four readings expose two possible reference-free shapes, but T227 does not
authorize either one:

- a short, separately timed activity island survives while forced alignment or
  source reconstruction absorbs its token into a longer neighboring source;
- sustained activity or source-scale identity evidence survives, but a later
  evidence branch overwrites or ignores it.

Any ASR, VAD, forced-alignment, Sortformer, identity-epoch, or TitaNet producer
change invalidates the affected FR49 speaker evidence and its signed product
judgments. The FR49 source-leading-prefix repairs at `ref-0061` and
`ref-0121` specifically depend on the current aligned prefix, primary boundary,
and identity evidence. Such an upstream change therefore requires a new exact
real-path capture and complete chronological plus reverse contextual-semantic
review against `test.txt`; the existing FR49 ledger cannot be carried forward.

## T228 manual topology decision

After the four complete readings were reconciled, the reviewer reread the
actual neighboring primary, thresholded activity, forced-alignment, final
label, and decision-audit evidence for the only two material contexts sharing
the same exact final-loss shape: `ref-0327` and `ref-0417`. This selection was
made manually from the complete conversational evidence. No script, query,
formula, metric, count, or ranking selected it.

In both contexts, one known identity B owns a unique `0.4 s` primary run that
wholly contains exactly one aligned lexical codepoint. B has no thresholded
activity interval overlapping that run and no usable local TitaNet embedding.
Exactly at the run's right boundary, a longer primary run for identity A begins
and is fully covered by activity A. The existing source-scale decision labels
the candidate codepoint as A with `voiceprint_direct_regular`. Thus the useful
primary boundary survives, the activity view filters it out, and the later
source-scale voiceprint branch extends A left across the retained codepoint.
At `ref-0327`, the immediately following A codepoint starts at the target's
zero-duration point and crosses the primary right boundary; this exact
zero-point successor is continuation evidence, not a second candidate. No
other partial aligned-unit overlap is authorized.

The first frozen candidate replay exposed a previously unnamed control at
`2241.356-2241.436 s`. Complete `test.txt` context identifies its `对` as part
of Tang Yunfeng's uninterrupted explanation, so changing it to Shi Yi is a
manual contextual regression. Its positive aligned duration is exactly the
configured boundary-split tolerance, unlike the longer positive unit at
`ref-0417`; it is now a mandatory abstention control. This finding came from
reading the complete conversation, not from an executable comparison.

The shared shape is narrow enough to authorize one frozen experiment, but not
to accept a repair. The rejected broad primary-aligned experiment changed many
unrelated contexts because alignment lag and primary share Sortformer ancestry.
The rejected broad A-B-A return experiment leaked across phrase boundaries.
This proposal differs by requiring B to be absent from thresholded activity,
requiring one wholly contained codepoint, requiring the incumbent to be the
same direct regular voiceprint reason, and requiring a contiguous fully
activity-covered A continuation. It rewrites neither a phrase nor an interval.

The manual abstention boundary is explicit. `ref-0118` crosses a primary
transition inside a two-codepoint aligned phrase; `ref-0135` has only secondary
posterior/phrase evidence; `ref-0102` is a primary-versus-secondary/voiceprint
contradiction; `ref-0049` and `ref-0390` place their zero-duration points
outside the useful identity run; and source-absent contexts have no writable
semantic unit. Missing, duplicate, partial, multi-unit, candidate-activity,
noncontiguous continuation, mixed incumbent, non-voiceprint incumbent,
third-identity, and already-retained cases likewise abstain by contract.

T228 is complete. It authorizes only a false-by-default,
`orator.toml`-enabled implementation and frozen FR49 A/B replay. It does not
authorize a real audio run, product score, ledger change, or speaker-business
closure. Complete candidate A/B chronological and reverse contextual-semantic
review remains mandatory before any such decision. That subsequent T229-T231
implementation, replay, and four-pass review is recorded separately in
`fr50-right-bounded-short-primary-unit-review-2026-07-23.md`; this document
remains the immutable pre-implementation topology decision.
