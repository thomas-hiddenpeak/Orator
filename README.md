# Orator

Orator 是一个面向 Jetson Orin / Thor 边缘设备的实时语音处理系统。它通过
WebSocket 接收单声道 PCM 音频流，在一个公共时间基上运行多个独立流水线，
并输出统一的 timeline JSON。

当前核心能力：

- ASR：Qwen3-ASR，流式 KV-cache 解码。
- 说话人分离：Sortformer 流式 diarization。
- VAD：Silero VAD，GPU 批处理。
- 说话人身份：TitaNet-Large voiceprint，跨会话全局 `spk_N` 身份。
- 强制对齐：Qwen3-ForcedAligner，可为最终 ASR 文本生成字/词级时间戳。
- Web UI：由 `orator_ws` 内置 HTTP 静态服务器提供，无前端构建步骤。

项目约束以 [.specify/memory/constitution.md](.specify/memory/constitution.md)
为准。状态说明见 [specs/PROJECT_STATE.md](specs/PROJECT_STATE.md)，但代码是
最高事实来源；发现文档和代码不一致时，应以代码为准并修正文档。

## 当前收官基线

Spec 013 的唯一收官模型基线是流式 Sortformer v2.1，使用 `orator.toml`
中的 `340/1/188/188` 配置（chunk/right-context/FIFO/cache-update）。编译期
默认值与 TOML 均指向 `models/sortformer_4spk_v2.1.safetensors`。v2 权重已
从本机模型目录删除；历史结果仅保留在报告中，不能用于新的候选选择或收官验收。

v2.1 是后续工作的固定起点，不表示当前结果已经通过验收。现有全长
上下文诊断为 `413/556`；在达到 Spec 013 门槛前，项目状态仍为未收官。
所有正式准确率测试必须使用 `test.mp3`、人工参考 `test.txt`、已记录的
`orator.toml` 和真实增量 WebSocket 路径。

## 许可证

本项目采用 **GNU Affero General Public License v3.0 or later
AGPL-3.0-or-later**。

如果你修改并分发本项目，或将修改版作为网络服务提供，必须按 AGPL-3.0-or-later
向用户提供对应源码。详见 [LICENSE](LICENSE) 与 [COPYRIGHT](COPYRIGHT)。

## 环境要求

- Linux，主要目标为 Jetson / ARM64。
- CMake 3.20 或更高版本。
- C++20 编译器。
- CUDA 工具链，项目当前按 CUDA C++ 构建并链接 `cudart`。
- 构建期通过 CMake `FetchContent` 获取：
  - `tomlplusplus`，header-only 配置解析。
  - `libwebsockets`，WebSocket 传输层。

运行时产品约束是纯 C++20/CUDA，并且只允许宪法中列出的边界基础设施例外。
Python、PyTorch、NeMo 等只允许作为 `tools/` 下的离线 oracle 或验证工具。

## 大文件

模型权重和参考数据通过 Git LFS 管理：

- `*.safetensors`
- `*.npz`
- `*.npy`
- `*.f32`
- `*.i32`
- `*.pcm`

首次克隆后通常需要：

```bash
git lfs install
git lfs pull
```

主要默认模型路径由 `orator.toml` 配置：

- `models/asr/Qwen/Qwen3-ASR-1.7B`
- `models/sortformer_4spk_v2.1.safetensors`
- `models/vad/silero_vad.safetensors`
- `models/speaker/titanet_large.safetensors`
- `models/ForcedAligner`

## 构建

```bash
cmake -S . -B build
cmake --build build -j
```

主目标：

- `orator_core`：核心静态库。
- `orator_ws`：实时 WebSocket 服务和 Web UI 静态服务器。
- `asr_testmp3`、`asr_stream_test`：ASR / 流式验证工具。
- `asr_stream_window_probe`、`asr_encoder_chunk_probe`、
  `asr_stream_incremental_probe`：ASR 流式策略探针。
- `orator_eval`：历史机械投影诊断工具，不作为 CTest 运行，也不得用于结果
  准确率、候选比较、参数选择或验收结论。

## 运行

推荐使用 `orator.toml` 固定运行参数：

```bash
ORATOR_CONFIG=orator.toml ./build/orator_ws
```

服务启动后：

- WebSocket 默认监听 `ws://0.0.0.0:8765`
- Web UI 默认监听 `http://0.0.0.0:8766`
- UI 端口为 `ui_port`，若为 `0` 则使用 `port + 1`
- 静态页面目录默认为 `web`

客户端向 WebSocket 发送二进制音频帧：

- 默认格式：`int16le`、mono、16 kHz。
- 发送包含 `"f32"` 的文本控制消息后，后续二进制帧按 float32 PCM 解释。

常用控制消息：

```json
{"flush": true}
{"end": true}
{"reset": true}
{"describe": true}
{"sessions": true}
{"load_session": {"session_id": "..."}}
{"speakers": true}
{"rename_speaker": {"id": "spk_0", "name": "张三"}}
```

`flush` 输出当前 timeline 并继续流式处理；`end` 结束当前流、输出最终
timeline，然后重置会话。

一个会话只允许一个连接发送音频。浏览器和诊断连接可以同时观察该会话，
并接收相同的实时事件与终端 timeline；观察连接的建立和断开不会重置流。
第二个连接尝试发送音频时会收到 `audio producer already active` 错误，
对应音频帧不会进入管线。

## 配置

配置文件入口是 `orator.toml`，解析代码在
[include/io/config_reader.h](include/io/config_reader.h) 和
[src/io/config_reader.cc](src/io/config_reader.cc)。

当前代码路径中的有效加载顺序为：

1. `AuditoryStream::Config` 编译期默认值。
2. `ORATOR_CONFIG` 指定的 TOML 文件，默认 `orator.toml`。
3. `ORATOR_*` 环境变量覆盖 TOML 和默认值。
4. CLI 参数最终覆盖前述配置。

主要配置段：

- `[server]`：WebSocket/UI 端口和 UI 静态目录。
- `[asr]`：Qwen3-ASR 模型目录、VAD gate、segment cap、decode 参数。
- `[align]`：强制对齐开关、模型目录、保留音频窗口。
- `[speaker]`：说话人身份开关、TitaNet 模型目录、registry 持久化路径。
- `[vad]`：VAD 模型路径、阈值、最短语音/静音、padding。
- `[diarizer]`：Sortformer 权重、speaker cache、FIFO、后处理阈值。
- `[storage]`：ProtocolTimeline 和 session persistence 路径。
- `[buffer]`：音频缓存容量和 shrink 阈值。
- `[telemetry]`、`[telemetry.cursor]`：GPU 实时利用率/来源、CUDA
  统一显存、频率、`VIN` 系统功耗和 cursor progress 遥测。
- `[debug]`：日志级别、时间基检查、GPU 调度模式。
- `[debug_model]`：ASR 模型开发环境变量说明，不由 `ConfigReader` 直接读取。

常用环境变量：

| 变量 | 作用 |
|---|---|
| `ORATOR_CONFIG` | 指定 TOML 配置文件路径 |
| `ORATOR_PORT` | 覆盖 WebSocket 端口 |
| `ORATOR_UI_PORT` | 覆盖 UI 端口 |
| `ORATOR_UI_ROOT` | 覆盖 UI 静态目录 |
| `ORATOR_ASR_MODEL_DIR` | 覆盖 ASR 模型目录 |
| `ORATOR_ASR_DISABLE=1` | 禁用 ASR |
| `ORATOR_ASR_MAX_NEW_TOKENS` | 覆盖 ASR 最大 decode token 数 |
| `ORATOR_ASR_SEGMENT_SEC` | 覆盖 ASR segment cap |
| `ORATOR_ASR_LANGUAGE` | 覆盖 ASR 语言提示 |
| `ORATOR_ASR_SYSTEM_PROMPT` | 覆盖 ASR system prompt |
| `ORATOR_VAD_STREAM` | 开关 VAD 流水线 |
| `ORATOR_VAD_MODEL` | 覆盖 VAD 模型路径 |
| `ORATOR_VAD_THRESHOLD` | 覆盖 VAD 阈值 |
| `ORATOR_GPU_TELEMETRY_SEC` | GPU telemetry 推送间隔，`0` 为禁用 |
| `ORATOR_CURSOR_TELEMETRY_SEC` | cursor progress 推送间隔 |
| `ORATOR_STORAGE_DISK_PATH` | 协议 timeline 磁盘后端路径 |
| `ORATOR_SESSION_DIR` | session 持久化目录 |
| `ORATOR_LOG_LEVEL` | 日志级别：`0` DEBUG，`1` INFO，`2` WARN，`3` ERROR |
| `ORATOR_TIMEBASE_CHECK` | 启用时间基一致性检查 |
| `ORATOR_GPU_SERIAL` | 强制 GPU serial 调度模式 |
| `ORATOR_GPU_CONCURRENT` | 强制 GPU concurrent 调度模式 |

## 架构

目录分层：

```text
include/        public headers
src/            implementations
  core/         base data types and model interfaces
  gpu/          CUDA memory, kernels, scheduling
  io/           safetensors, tokenizer, config, audio/reference I/O
  feature/      mel and Whisper-style feature extraction
  model/        Qwen3-ASR, Sortformer, VAD, aligner, speaker models
  protocol/     topic registry, router, storage, replay, sessions
  pipeline/     AuditoryStream controller and workers
  net/          libwebsockets server and HTTP static server
web/            browser UI, plain ES modules
test/           CTest unit and integration tests
tools/          probes, offline validation, observability
specs/          SDD artifacts
```

核心运行路径：

```text
WebSocket PCM
  -> AuditoryWsHandler
  -> AuditoryStream::PushAudio
  -> per-pipeline audio caches
  -> DiarizationWorker / AsrWorker / VAD / SpeakerIdentityStage
  -> ComprehensiveTimeline raw typed tracks
       -> AlignWorker (reads finalized ASR, writes align)
       -> BusinessSpeakerPipeline (reads raw evidence, writes business_speaker)
  -> ProtocolTimeline mirrors committed track records
  -> WebSocket live events and terminal timeline
```

关键设计约束：

- 所有流水线使用同一个 `core::TimeBase`。
- 时间码必须通过 `TimeBase::SecondsAt()`、`SampleAt()`、`Duration()` 等接口派生。
- 流水线之间不直接交换结果，不建立 pipeline-to-pipeline 回调、共享指针或
  原子标志；跨流水线读取统一经过会话拥有的类型化时间线。
- 跨流水线业务证据只通过类型化 `ComprehensiveTimeline` 汇合；
  `ProtocolTimeline` 仅镜像已提交记录，用于持久化、传输和外部观察。
- ASR 输出文本和自身时间码；diarization 输出 speaker 和自身时间码；
  raw tracks 采用 append-once/不可变语义。注册的 `business_speaker` 管线独立
  负责 speaker 选择、align-aware 文本切分、gap policy 和支持度诊断，并写入
  自己的可修订轨道；容器不改写任何原始管线内容。
- ASR 的 VAD gate 读取 `ComprehensiveTimeline` 的不可变 VAD 快照，避免热路径
  反复反序列化或读取 VAD worker 私有状态。

## WebSocket 输出

服务端会发送两类 JSON：

1. 连接建立时的 legacy `ready` 消息。
2. 大多数运行时事件会被包装为 Spec 004 topic envelope：

```json
{
  "topic": "asr/event",
  "pipeline": "asr",
  "pipeline_version": "1.0.0",
  "msg_id": 1,
  "ts": 0,
  "qos": 0,
  "schema_version": 1,
  "data": "{\"type\":\"asr\",...}"
}
```

`data` 当前是内层 JSON 字符串。客户端需要先解析 envelope，再解析 `data`。
`ts` 当前固定为 `0`，业务时间应读取内层消息里的 `start`、`end`、`time_sec` 等字段。

常见内层消息类型：

- `ready`
- `asr_partial`
- `asr`
- `diar`
- `vad`
- `align`
- `revision`
- `timeline`
- `gpu_telemetry`
- `cursor_progress`
- `sessions`
- `speakers`
- `reset_ok`
- `error`

## Web UI

`orator_ws` 同时启动 HTTP 静态服务器，直接托管 `web/`。当前 UI 是无框架、
无构建步骤的 ES module SPA：

- [web/index.html](web/index.html)
- [web/style.css](web/style.css)
- [web/js/app.js](web/js/app.js)
- [web/js/ws.js](web/js/ws.js)
- [web/js/model.js](web/js/model.js)
- [web/js/render/](web/js/render/)

UI 支持：

- 麦克风输入和音频文件上传。
- 实时 transcript 和 partial draft。
- diar / ASR / VAD / align 轨道数据与终态综合 JSON 检查视图；图形化时间线仍是开放工作。
- GPU 利用率、统一显存、系统功耗、各管线 RTF/backlog 和 cursor progress
  observability 面板。
- speaker registry 显示和重命名。
- session 列表和加载。

更多前端说明见 [web/README.md](web/README.md)。

## 测试

完整 CTest：

```bash
cd build
ctest --output-on-failure
```

常用子集：

```bash
cd build
ctest -R test_config --output-on-failure
ctest -R test_protocol_timeline --output-on-failure
ctest -R test_auditory_stream --output-on-failure
ctest -R test_vad --output-on-failure
```

统一 WebSocket 测试客户端：

```bash
python3 tools/verify/py/ws_unified_test.py --duration 120 --port 8765 --out test_120s.json
python3 tools/verify/py/ws_unified_test.py --duration 600 --port 8765 --out test_600s.json
python3 tools/verify/py/ws_unified_test.py --duration 3615 --port 8765 --rate 1.0 --out test_full.json
python3 tools/verify/py/ws_unified_test.py --duration 120 --port 8765 --test-observer --out test_observer.json
```

项目测试治理要求：

- 除单元测试外，实际流水线测试使用 `test/data/audio/test.mp3` 和
  `test/data/reference/test.txt`。
- 流式行为必须通过真实 WebSocket 增量输入验证。
- 性能结果只能来自真实流式路径。
- Jetson 设备指标通过 `tegrastats` 观察。
- ASR、说话人归属、端点、幻觉和综合时间线的结果评估，只允许逐项读取完整
  对话上下文并进行语义比对。
- 任何形式的代码、测试、脚本、Notebook、公式、查询、自动指标或算法，都
  不得判定正确/错误、汇总准确率、排名或选择候选、输出通过/失败结论。
- 自动化仅可运行和观测系统、验证机械/数值合同、采集并展示未判定证据；
  CER、DER、字符匹配、时间重叠和时长映射都不是准确率结论。
- 说话人分离人工语义评估还需要记录时间块、偏移和指标含义。
- 全局只维护一个 WebSocket 测试客户端：
  [tools/verify/py/ws_unified_test.py](tools/verify/py/ws_unified_test.py)。

## 开发规则

非平凡改动遵循 SDD：

1. 读取宪法和 `specs/PROJECT_STATE.md`。
2. 检查 `.agents/skills/` 是否有相关 skill。
3. 更新或创建 `spec.md`、`plan.md`、`tasks.md`。
4. 实现代码。
5. 验证 build、CTest、必要的真实 WebSocket 流式路径。
6. 同步 `specs/PROJECT_STATE.md` 和相关 spec/task 状态。

代码约束摘要：

- Google C++ Style：2 空格缩进，类型/方法 `PascalCase`，局部变量
  `lower_snake_case`，成员字段尾随 `_`。
- 公共头文件用 `#pragma once`。
- 性能路径禁止裸 `new`/`delete`/`cudaMalloc`，必须使用 RAII。
- 每个 CUDA 调用必须检查错误。
- 不添加未批准的运行时依赖。
- 不做无关重构。
- 文档使用标准工程术语，避免隐喻和未定义缩写。

## 当前注意事项

- 工作流和配置相关文档必须以代码核验为准。当前配置加载顺序的实现与
  `orator.toml` 注释、宪法 Article IX 的文字不完全一致。
- `specs/PROJECT_STATE.md` 的部分状态文字可能落后于代码；开始新任务前要定位
  对应符号和测试确认。
- `build/`、`build_debug/`、`.omo/`、`.pytest_cache/`、`models/` 内大文件不应作为
  普通源码改动提交。
