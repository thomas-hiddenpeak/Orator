# Future-Epoch Full Promotion Review (2026-07-18)

> **Comparison superseded 2026-07-19.** T135 corrects the T111 ledger to
> `514/556`. The T116 `518/556` ledger has not received the same reconciliation,
> so its numerical rank against T111 is withdrawn. FR16ABO remains rejected for
> its independently established A/B inconsistency and critical/confident-wrong
> failures. See `speaker-baseline-reconciliation-2026-07-19.md`.

## Scope and evaluation authority

This record completes T116 for FR16ABO. Transitional experimental commit
`f49a8278e0d8a4567bc0cdd43c8f42fa5d06913f` passed the engineering and
real-WebSocket promotion ladder, after which empty-registry Run A and restarted
frozen-registry Run B each streamed the complete 3615.120-second `test.mp3`.

`test/data/reference/test.txt` is the existing human-listened reference. It was
not replaced by a script, a second transcript, or a request for the user to
listen again. The reviewer read all 556 contributions in Run A and all 556 in
Run B with complete surrounding conversation in chronological order, then
performed a second complete pass from the tail toward the beginning. The two
passes were reconciled independently for each run.

No compiled code, script, test, notebook, formula, query, metric, or algorithm
assigned speaker correctness, counted a product result, calculated an accuracy
result, ranked the runs, selected the candidate, or issued the verdict. Tools
only captured the real stream, checked mechanical contracts, froze hashes, and
arranged unjudged reference and candidate evidence. Every speaker label, total,
comparison, percentage, and gate decision below was derived and checked
manually from complete conversational context.

## Frozen evidence

| Item | Value |
|---|---|
| Source commit | `f49a8278e0d8a4567bc0cdd43c8f42fa5d06913f`, clean before and after every accepted run |
| Server binary | `209b16f402f89c101aa76521f98bbae9a28f874487b2081197dbc90ab92cb858` |
| Sortformer v2.1 | `d036020b6b93977098929d417b1b106a952ec02cc38cafc9d3315ae0ec4d90b8` |
| `test.mp3` | `b7c25d1c349b02d654b6a406bc29039749e4240a4109dda4fcc905285b14b18b` |
| `test.txt` | `35e8695057be82f3028877f7dc159f10ecfa0ab7f06c444cefa9a079b0e24a86` |
| Run A config / resolved config | `8aac83350babb159b24c4d86924bf624cae788a7af5efdaa81214f9030fa491c` / `c4507a7a22ba5e0109459b28547003920f1a6554be7107c65f52ef32a27eeb8c` |
| Run B config / resolved config | `1bb76d67db856f2d8c944e6c39bd66ded8390bc28947626f99adbe16f370bbe2` / `9c47206adf7f12c3c84eb580aae88aa205b91f6f337bdf64c92a7e827483712b` |
| Run A artifact / manifest | `b444d55a72a0bf224325530dd8cc409baa96069d26846bc2212969ef91ea25eb` / `7d00dedc5e9be5a2477d9fce4946e55978cff3dc4cdd8f3da1e857e7e196a4b8` |
| Run B artifact / manifest | `67e3791f08f00c13f4d768881b9fe55624245d30407809b9f7ac9d0ce7113f65` / `53aca90996e30b5d5f0a41792d9da23599c5d89ad7ee60a29e571b6d9f727793` |
| Run A forward / corrected reverse packet | `472e1b3c30dbb148b7960df9f78d53a5b5b80b389391bef9c9cb78ad805f3696` / `fb5c303c796160da63cde1be87fc74684147c9588ba72c5845399416f2f34b8f` |
| Run B forward / corrected reverse packet | `4c858a68ceab4a071ce71fbc893faa710b390db169fbecefef36783d6e3dc18d` / `c9579e1522dea49221084e547aca2f40ee36e6cbb2432c73c7398e0862a0777f` |

The Run A and Run B TOML files differ only where the acceptance topology
requires different isolated storage and speaker-registry fixtures. Both freeze
`speaker_fusion.future_epoch_lookahead_sec = 120.0`; no command-line or
environment override changes runtime model behavior.

## Real-WebSocket ladder

| Run | Audio | Stream factor | Direct `end` wait | Mechanical result |
|---|---:|---:|---:|---|
| Promotion prefix | 120.000 s | 0.993x | 0.802 s | Pass |
| Promotion prefix | 600.000 s | 0.992x | 4.712 s | Pass |
| Full Run A, empty registry | 3615.120 s | 0.993x | 26.235 s | Pass |
| Full Run B, restarted frozen registry | 3615.120 s | 0.993x | 27.018 s | Pass |

All four runs sent `end` directly after the final audio frame. Their contract
issue arrays are empty, observer terminal documents converge, source/config/
binary provenance remains stable, and required runtime plus `tegrastats`
fields and cadence have complete coverage. Both full runs reach exactly
57,841,920 samples with zero gap on all seven common-clock extents.

Run A records 755 diarization, 1348 primary-speaker, 275 ASR, 972 VAD, 275
alignment, 16094 voiceprint, and 1763 business-speaker entries. Run B records
755, 1348, 275, 972, 275, 16083, and 1751 entries respectively. These are
mechanical capture facts and are not speaker-accuracy measures.

In both full runs FR16ABO activates on exactly two phrases. The
`2991.16-2991.88` phrase remains `spk_0` and changes only its audit reason. The
`3299.90-3300.62` phrase changes from `spk_0` to `spk_1`. The runtime identity
map used in semantic review is `spk_0=朱杰`, `spk_1=唐云峰`, `spk_2=徐子景`,
and `spk_3=石一`.

## Review-packet ordering correction

The first `--by-reference` reverse packets were not valid reverse-order
evidence: the renderer accepted reverse windows but then globally restored
reference-number order. They were not used as the reverse pass. The complete
second review instead used reverse 600-second blocks, which began at the tail
and covered the full session.

The renderer is now corrected so explicit window order controls by-reference
section order while every reference row is emitted exactly once and uncovered
rows remain present. Focused tests verify the reverse order and evidence
completeness. Corrected A and B packets each contain all 556 rows, begin with
the final 3600-second block, and have hashes distinct from their forward
packets. This correction changes evidence presentation only; it does not alter
the already completed semantic judgments.

## Complete contextual findings

The historical pre-T135 T111 ledger used by this review had 519 accepted and 37
incorrect contributions in each run. T135 supersedes it with 514 accepted and
42 incorrect. The T116 review did not carry a
changed-row shortcut forward as a verdict: every reference contribution was
reread in both directions before these differences were signed.

### Run A

Relative to the accepted Run A ledger, complete context repairs `ref-0102`,
`ref-0135`, and `ref-0504`. It introduces `CW` errors at `ref-0116`,
`ref-0127`, `ref-0134`, and `ref-0235`:

- `ref-0102`: Tang Yunfeng's repeated governance conclusion is now assigned to
  `spk_1`; the preceding Shi Yi joke remains separate.
- `ref-0134` / `ref-0135`: the candidate combines Tang's incomplete `我就`
  and Shi's `对吧` under `spk_3`. This repairs Shi's tag question but moves the
  duplicate-time Tang fragment into the wrong known identity.
- `ref-0235`: Shi's valuation statement remains under `spk_3`, but the
  human-listened closing `可以可以` is assigned to `spk_0`.
- `ref-0504`: Tang's reply begins under `spk_1`, repairing the baseline's
  extension of Zhu Jie's preceding turn.

The final Run A error references are:

`0009, 0024, 0045, 0049, 0058, 0061, 0116, 0118, 0127, 0134, 0221, 0235,
0241, 0249, 0252, 0253, 0296, 0298, 0313, 0327, 0331, 0333, 0338, 0341,
0354, 0375, 0390, 0417, 0442, 0444, 0457, 0461, 0499, 0505, 0506, 0507,
0509, 0537`.

### Run B

Relative to the accepted Run B ledger, complete context repairs `ref-0135`,
`ref-0249`, and `ref-0504`. It introduces `CW` errors at `ref-0116`,
`ref-0127`, `ref-0134`, and `ref-0250`:

- `ref-0102` remains a baseline error in Run B, unlike Run A.
- `ref-0249`: the complete company-split question is now assigned to Zhu Jie;
  the short lead-in from the preceding overlap does not obscure the speaker of
  the business question.
- `ref-0250`: Tang Yunfeng's direct `不一定啊` response moves into the next
  source edge and is assigned to `spk_3`.
- `ref-0504` receives the same contextual repair as Run A.

The apparently mixed `ref-0155` and `ref-0248` rows remain accepted after full
context review. In each case the source row spans a rapid actual turn handoff;
the candidate preserves the identifiable principal contributions rather than
materially asserting one wrong known speaker for the whole exchange.

The final Run B error references are:

`0009, 0024, 0045, 0049, 0058, 0061, 0102, 0116, 0118, 0127, 0134, 0221,
0241, 0250, 0252, 0253, 0296, 0298, 0313, 0327, 0331, 0333, 0338, 0341,
0354, 0375, 0390, 0417, 0442, 0444, 0457, 0461, 0499, 0505, 0506, 0507,
0509, 0537`.

## Manual totals and decision

The historical T116 ledger contains 518 accepted and 38 incorrect
contributions, manually classified as 32 `CW`, five `M`, and one `U`. The
manually checked natural-turn
result is therefore `518/556`, approximately `93.17%`, for both Run A and Run
B. T135 did not reaudit these artifacts under the corrected material-fragment
rule, so this total is not ranked against T111. The confident-wrong share in
the historical ledger also rises
to approximately `5.76%`, above the at-most-two-percent gate, and known wrong
critical attributions remain, so the zero-critical-error gate still fails.

FR16ABO itself consistently repairs `ref-0504`; the failed promotion does not
erase that evidence. However, the complete real runs change additional source
contexts, and the A/B error sets differ. Run A repairs `ref-0102` but regresses
`ref-0235`; Run B keeps the `ref-0102` error, repairs `ref-0249`, and regresses
the adjacent `ref-0250`. The evidence therefore shows that one local future-
epoch rule is insufficient to produce a repeatable higher full-session
business view. It does not prove that FR16ABO caused every other run-to-run
change.

T116 passes its engineering, transport, latency, telemetry, time-base, and
evidence-completeness contracts but fails product promotion. Neither full run
advances a speaker gate. The best uniformly reconciled frozen comparison is
the T111 FR16ABN result at corrected `514/556`; the checked-in TOML returns
`speaker_fusion.future_epoch_lookahead_sec` to `0.0`. The implementation remains
dormant for evidence tracing and cannot activate in the default runtime.

## Next investigation

The next phase starts from evidence stability rather than another attribution
heuristic. It will mechanically locate the first A/B divergence separately in
diarization, primary speaker, identity epochs, ASR, forced alignment, and final
business projection, then replay the frozen typed inputs to distinguish
producer variance from projection variance. These tools may report only raw
structural differences. Any proposed correction must pass complete contextual
semantic review against `test.txt` before another real full-length capture is
justified.
