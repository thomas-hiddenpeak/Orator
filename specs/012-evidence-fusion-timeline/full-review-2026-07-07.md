# Spec 012 Full-Length Evidence Fusion Review - 2026-07-07

> **Evaluation governance:** Under Constitution 1.7.0, no code or executable
> automation may assign correctness, calculate accuracy, rank/select a
> candidate, or issue a verdict. Automated values below are mechanical evidence
> only; product results require complete contextual semantic review and manual
> result verification.

## Test Summary

| Item | Content |
|---|---|
| Test type | Full-length real WebSocket, all-pipeline evidence capture + offline fusion |
| Input audio | `test/data/audio/test.mp3`, 3615.0 s, pushed at 1.0x |
| Reference text | `test/data/reference/test.txt` |
| Run result | Success; final timeline returned |
| Output package | `/tmp/orator_all_pipeline_full_20260707.json` |
| Fusion output | `/tmp/orator_all_pipeline_full_20260707_fusion.json` |
| Wall time | 3618.741 s |
| Stream RTF | 0.999x |
| ASR RTF | 1.812x, 288 final segments |
| Diar RTF | 96.977x, 773 segments |
| VAD | 972 speech segments, RTF 45.302x |
| Forced alignment | 288/288 ASR segments aligned, RTF 34.709x |
| Candidate fusion | 1553 entries at `pause_sec=0.25`, no mechanical audit issues |
| Subjective conclusion | Align evidence is complete and useful for text boundaries; the final business speaker view ("who said what in context") remains in the 70-79% band and is not restored to closing-grade accuracy. |

## Evaluation Method Correction

This review evaluates speaker accuracy at the business-view level, not as an
isolated diarization-track score. The target question is: did the final
comprehensive/fusion view preserve who made each business-relevant statement in
context?

ASR literal accuracy is not scored here. ASR text is still read as the system's
content claim and as context for speaker attribution; forced alignment and VAD
provide phrase/pause boundaries; diarization and speaker identity provide
ownership evidence. Historical code-derived attribution percentages below are
non-authoritative mechanical records and may not evaluate or explain the result.

## Mechanical Evidence

- `fusion_audit.py` found ASR=288, align=288, missing align=0, extra align=0.
- Candidate variants:
  - `pause_sec=0.15`: 2363 candidate entries, unknown speaker 23.310 s.
  - `pause_sec=0.25`: 1553 candidate entries, unknown speaker 22.120 s.
  - `pause_sec=0.40`: 1278 candidate entries, unknown speaker 19.920 s.
- Current comprehensive has 723 entries. The candidate has more entries because it uses forced-alignment unit runs and drops silent gaps from ASR spans.
- Historical `speaker_attrib_eval.py` output is retained only as a mechanical
  record and does not explain, compare, or decide business-view accuracy:
  - Current comprehensive: 73.4% duration-weighted attribution over 3064 s, 3/4 reference names covered by global mapping.
  - Fusion candidate: 73.3% over 2048 s, 3/4 reference names covered.
  - The candidate changes boundary granularity more than it changes speaker identity correctness.
- `device_series` has 3611 tegrastats samples. Parsed raw-line ranges: RAM 21116-22572 MB, GPU temp 48.937-61.312 C, VDD_GPU 5.508-27.922 W, VIN 24.552-82.424 W. This run's tegrastats lines did not include `GR3D_FREQ`, so GPU utilization percentage could not be derived from the capture.

## Segmented Business Speaker Review

| Time span | Reference business context | Final-view attribution summary | Speaker business eval | Issues |
|---|---|---|---|---|---|
| 00:00-05:00 | Four speakers discuss 15/51/28/6 equity framing; 朱杰 leads, 唐云峰 interrupts with direct objections. | Candidate splits utterances by real align timing. S0 starts as 朱杰 and S2 often carries 唐云峰, but mapping is not globally stable. | Minor to major confusion | Short interjections and early speaker identity mapping are noisy. |
| 05:00-10:00 | 徐子景 explains To C/To B product logic and voting/action rights; 石一 and 唐云峰 add ratio checks. | Speaker labels are usable for long 徐子景 and 石一 runs but not every short reply. | Mostly accurate | Short confirmations drift. |
| 10:00-15:00 | Voting-control math, "绝对通过", 3.14 floor, and 15 vs 10 discussion. | Candidate retains the exchange and separates pauses better than current comprehensive. | Minor confusion | Dense rapid turns still collapse into adjacent speakers. |
| 15:00-20:00 | Dilution, valuation, option pool, PE multiples and retirement-money discussion. | Candidate boundaries match speech bursts more closely, but speaker identities still need context to interpret. | Minor confusion | Some important speaker positions are only roughly attributable. |
| 20:00-25:00 | A/B financing, 80x PE, no-dilution condition, and agreement framing. | Candidate avoids stretching silent gaps into speaker turns. | Minor confusion | Speaker identities still depend on window context. |
| 25:00-30:00 | Investor names, prior delay, no need for一致行动人, and equity/option setup. | Candidate gives finer turn boundaries, but several clarifications remain assigned to adjacent speakers. | Minor confusion | Proper names are ASR issues; speaker attribution is still approximate. |
| 30:00-35:00 | 朱杰 narrative on separation from domestic business, Hangzhou/Chengdu staffing and technology reuse. | Long narrative is stable and mostly assigned to one speaker; short replies switch among other ids. | Mostly accurate | Long-turn ownership is usable. |
| 35:00-40:00 | 并表/全资子公司, LP possibility, PMP/cut-out strategy, and investor discussion. | Candidate preserves rapid Q/A shape better, but global speaker ids are inconsistent. | Minor to major confusion | This window shows real identity drift; not closing-grade. |
| 40:00-45:00 | Patents/open source, tax, ownership transfer, investor strategy. | Candidate cuts pauses well, but attribution alternates between the same ids without stable names. | Minor confusion | Short legal/finance clarifications are unreliable. |
| 45:00-50:00 | Investor negotiation terms, no buyback/no VAM, valuation ask, TS timing. | Candidate improves phrase boundaries and keeps long speaker positions readable. | Minor confusion | Short replies remain noisy. |
| 50:00-55:00 | Subsidiary accounting, capital injection, consolidation, dividends and tax. | Candidate boundary quality is better than current comprehensive. | Minor to major confusion | Dense back-and-forth makes exact attribution unreliable. |
| 55:00-60:15 | Tail discussion on overseas company, disclosure, agent/address, "small treasury". | Candidate remains active to 3615 s with align coverage, but tail rapid exchanges are fragmented. | Minor to major confusion | Some role attribution is only rough. |

## Conclusions

- **ASR semantic accuracy**: not scored in this speaker-business review. The 1920.4-1945.4 s repeat burst is recorded as context because it can obscure speaker attribution, but it is not counted as an ASR score here.
- **Speaker business accuracy**: approximately 70-79% for the final candidate view. Long business turns are often usable, but cross-window speaker identity and short rapid exchanges are not stable enough for closing-grade "who said what" use.
- **Fusion result**: conditional pass as an evidence-analysis tool, fail as a speaker-business accuracy recovery. Forced alignment is complete and valuable for boundary placement, but it cannot by itself fix wrong speaker identity.
- **Next step**: keep the frozen all-pipeline JSON and iterate offline on fusion rules. The likely useful direction is a two-layer view: align/VAD determine text and speech boundaries; diar segments provide local speaker evidence; speaker identity must be stabilized before the comprehensive view can be closing-grade again.
