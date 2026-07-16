# Production 600-Second Context Review - 2026-07-16

## Evaluation boundary

This is a manual contextual-semantic speaker review. No program, query,
formula, notebook, lexical metric, timestamp-overlap metric, or embedding score
assigned a correctness label, counted accepted turns, calculated the accuracy,
or issued the verdict. Tooling only ran the product, verified mechanical
contracts, and placed the timestamped reference beside the terminal business
view for reading.

The reviewer read every reference turn intersecting the 0-600 second run in
chronological order, then reread the same evidence in reverse 120-second blocks:
480-600, 360-480, 240-360, 120-240, and 0-120 seconds. `ref-0093` is judged only
on its audible portion before the 600-second cutoff.

## Frozen production evidence

| Item | Value |
|---|---|
| Audio | `test/data/audio/test.mp3`, first 600.000 seconds, 16 kHz mono |
| Runtime | real WebSocket, 1.0x input rate |
| Model | `models/sortformer_4spk_v2.1.safetensors` |
| Config | `orator.toml` |
| Artifact | `/tmp/orator-spec013/runtime-v21/cpp-fusion-600s-partial-edge-monotonic-ws.json` |
| Artifact SHA256 | `32cfaa46683a170553f49bedb2bf736a211b799ef40ce7bdaca8c0bcc160b110` |
| Config SHA256 | `e0a7fae454e5dfeeca1b88278ad08b40910706949f4046634938d0dfd2ef573d` |
| Binary SHA256 | `c988c9ecf82950e030e2e79c7a176c43bc71abcc09654174b6e94b6864d1f788` |
| Wall time / stream RTF | 606.43 seconds / 0.989x |
| Product contract | `TEST_SCRIPT_COMPLETED_SUCCESSFULLY` |
| Common time base | all seven pipeline extents at 9,600,000 samples; zero gaps |
| Terminal tracks | diar 98, primary 166, ASR 46, VAD 159, align 46, voiceprint 2476, business 202 |
| Runtime telemetry | 573 samples; required fields covered; cadence 95.5% |
| Tegrastats | 604 samples; required fields covered; cadence 100% |

Speaker identity mapping established from the full conversation context:

- `spk_0`: Zhu Jie
- `spk_1`: Tang Yunfeng
- `spk_2`: Xu Zijing
- `spk_3`: Shi Yi

## Full manual ledger

`Accepted` includes a contextually correct natural turn with a minor boundary
character assigned to an adjacent speaker. `Incorrect` is reserved for a
complete reference contribution whose business attribution is wrong in
context.

| Ref | Judgment | Note |
|---|---|---|
| 0001 | Accepted | Zhu main turn stable |
| 0002 | Accepted | Zhu continuation stable |
| 0003 | Accepted | Xu question |
| 0004 | Accepted | Zhu reply |
| 0005 | Accepted | Shi `就是杭州嘛` restored as `spk_3` |
| 0006 | Accepted | Zhu continuation |
| 0007 | Accepted | Xu question |
| 0008 | Accepted | Tang main clause; leading `对` remains a minor edge slip |
| 0009 | Accepted | Zhu `然后呢`; first character remains an edge slip |
| 0010 | Accepted | Tang main proposal |
| 0011 | Accepted | Zhu interruption; two-character leading edge slip |
| 0012 | Accepted | Tang `就这么定了` |
| 0013 | Accepted | Zhu `不是` |
| 0014 | Accepted | Tang `不能犹豫` |
| 0015 | Accepted | Zhu interruption |
| 0016 | Accepted | Tang long response stable |
| 0017 | Accepted | Zhu reply; leading character edge slip |
| 0018 | Accepted | Tang long turn stable; one leading handoff character |
| 0019 | Accepted | Zhu long turn stable |
| 0020 | Accepted | Shi response |
| 0021 | Accepted | Tang contribution visible in duplicate-time context |
| 0022 | Accepted | Shi numerical contribution visible in duplicate-time context |
| 0023 | Accepted | Tang main contribution stable |
| 0024 | Incorrect | Zhu `啊?` attributed to Xu |
| 0025 | Accepted | Xu `嗯?` |
| 0026 | Accepted | Zhu question |
| 0027 | Accepted | Tang response |
| 0028 | Accepted | Zhu long turn stable |
| 0029 | Accepted | Tang interruption; short internal boundary noise only |
| 0030 | Accepted | Tang long turn stable |
| 0031 | Accepted | Zhu long turn stable |
| 0032 | Accepted | Tang statement; trailing handoff follows source semantics |
| 0033 | Accepted | Zhu answer |
| 0034 | Accepted | Tang response |
| 0035 | Accepted | Zhu main clause; trailing handoff fragment |
| 0036 | Accepted | Tang long turn stable |
| 0037 | Accepted | Tang `不能再等了` |
| 0038 | Accepted | Xu long turn stable |
| 0039 | Accepted | Xu long continuation stable |
| 0040 | Accepted | Xu continuation |
| 0041 | Accepted | Tang main clause; final character edge slip |
| 0042 | Accepted | Xu short acknowledgment |
| 0043 | Accepted | Tang response |
| 0044 | Accepted | Xu response |
| 0045 | Incorrect | Shi `对` attributed to Xu |
| 0046 | Accepted | Xu acknowledgment |
| 0047 | Accepted | Xu main proposal; terminal handoff edge |
| 0048 | Accepted | Shi response |
| 0049 | Incorrect | Tang `对` attributed to Shi |
| 0050 | Accepted | Zhu response |
| 0051 | Accepted | Tang response |
| 0052 | Accepted | Shi response |
| 0053 | Accepted | Zhu acknowledgment |
| 0054 | Accepted | Shi continuation |
| 0055 | Accepted | Shi numerical statement |
| 0056 | Accepted | Tang `对` |
| 0057 | Accepted | Shi numerical contribution |
| 0058 | Incorrect | Tang `相差0.7` attributed mainly to Shi |
| 0059 | Accepted | Shi continuation |
| 0060 | Accepted | Xu question |
| 0061 | Incorrect | Shi `我说` attributed to Tang |
| 0062 | Accepted | Tang response |
| 0063 | Accepted | Shi `对` |
| 0064 | Accepted | Tang main clause |
| 0065 | Accepted | Shi contribution; terminal handoff fragment |
| 0066 | Accepted | Tang phrase restored except first character |
| 0067 | Accepted | Shi numerical turn stable |
| 0068 | Accepted | Tang question |
| 0069 | Accepted | Shi response |
| 0070 | Accepted | Tang contribution in shared ASR phrase |
| 0071 | Incorrect | Shi `才44 45` attributed to Tang |
| 0072 | Accepted | Tang `那你可以否决了` |
| 0073 | Accepted | Shi response; first two characters remain an edge slip |
| 0074 | Accepted | Tang question |
| 0075 | Accepted | Shi reply |
| 0076 | Accepted | Tang acknowledgment |
| 0077 | Accepted | Shi calculation stable |
| 0078 | Accepted | Tang `可以啊` restored |
| 0079 | Accepted | Shi `倒是可以` restored |
| 0080 | Accepted | Tang `合理的, 没问题` restored |
| 0081 | Accepted | Xu objection; following handoff belongs to Shi |
| 0082 | Accepted | Shi long proposal stable |
| 0083 | Accepted | Tang question stable |
| 0084 | Accepted | Shi reply |
| 0085 | Accepted | Tang question |
| 0086 | Accepted | Shi long turn restored and stable |
| 0087 | Accepted | Shi long numerical explanation stable |
| 0088 | Accepted | Xu `对` restored |
| 0089 | Accepted | Shi response |
| 0090 | Incorrect | Xu `嗯, 对, 对` attributed to Shi |
| 0091 | Accepted | Shi continuation |
| 0092 | Accepted | Xu contribution visible inside adjacent numerical exchange |
| 0093 | Accepted | Shi audible portion through 600 seconds stable |

## Manual conclusion

The chronological review and reverse-block reread agree on 86 accepted turns,
7 incorrect turns, and no unresolved turn among the 93 reviewed reference
contributions. The reviewer manually calculated `86 / 93 = 92.47%` natural-turn
speaker accuracy for this 600-second production run.

The run therefore passes the 90% industrial floor for this fixed 600-second
block. It is not the full-file closeout: the complete 3615.12-second production
run, all 556 reference turns, reverse-block review, browser convergence, full
CTest, and final evidence synchronization remain mandatory.
