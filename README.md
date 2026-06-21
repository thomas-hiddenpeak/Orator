# Orator

Orator 是一个面向实时业务的本地化语音处理系统，提供：

- 实时 ASR（Qwen3-ASR）
- 可选说话人分离（Sortformer）
- WebSocket 服务输出统一时间线（timeline）

项目强调工程约束：纯 C++/CUDA、实时链路优先、可测量性能与精度。

## 开源协议（强制开源传染）

本项目采用 **GNU Affero General Public License v3.0 or later（AGPL-3.0-or-later）**。

这意味着：

- 你可以使用、修改、再发布。
- 若你分发修改版，必须在同一许可证下开源源码。
- 若你将修改版作为网络服务提供（SaaS/API），也必须向服务使用者提供对应源码。

详见 [LICENSE](LICENSE) 与 [COPYRIGHT](COPYRIGHT)。

## 环境要求

- Linux（已在 Jetson/ARM64 场景使用）
- CMake >= 3.20
- CUDA 工具链（需可用 `cudart`、`cublas`）
- C++20 / CUDA20

## 大文件管理（Git LFS）

仓库已配置 Git LFS 跟踪以下格式：

- `*.safetensors`
- `*.npz`
- `*.npy`
- `*.f32`
- `*.i32`
- `*.pcm`

首次克隆建议执行：

```bash
git lfs install
git lfs pull
```

## 构建

```bash
cmake -S . -B build
cmake --build build -j
```

## 运行实时 WebSocket 服务

命令格式：

```bash
./build/orator_ws <port> <diarizer_weights_or_empty> <asr_model_dir>
```

示例（关闭 diarizer，仅启用 ASR）：

```bash
./build/orator_ws 8765 "" /path/to/models/asr/Qwen/Qwen3-ASR-1.7B
```

服务启动后，可向 `ws://<host>:<port>` 发送 `int16le mono 16k` PCM 二进制流，最后发送 `{"end"}` 或 `{"flush"}` 获取时间线结果。

当前默认行为（便于联调）：

- WebSocket：`<port>`（默认 `8765`）
- Web UI：`<port+1>`（默认 `8766`）
- 进程启动时默认预热全管线（diar + asr + vad）进入待命；若未显式传 `asr_model_dir`，默认使用 `models/asr/Qwen/Qwen3-ASR-1.7B`

运行时配置通过 `orator.toml`（TOML 格式）管理，环境变量可覆盖配置文件中的值：

```bash
# 使用默认配置
./build/orator_ws 8765 ""

# 使用自定义配置文件
ORATOR_CONFIG=/path/to/orator.toml ./build/orator_ws 8765 ""
```

可选环境变量（覆盖 `orator.toml` 中的对应字段）：

| 变量 | 默认值 | 说明 |
|------|--------|------|
| `ORATOR_CONFIG` | `orator.toml` | 配置文件路径 |
| `ORATOR_PORT` | `8765` | WebSocket 端口 |
| `ORATOR_UI_PORT` | `port+1` | 覆盖 UI 端口 |
| `ORATOR_UI_ROOT` | `web` | 覆盖静态页面目录 |
| `ORATOR_ASR_MODEL_DIR` | `models/asr/Qwen/Qwen3-ASR-1.7B` | 覆盖 ASR 模型目录 |
| `ORATOR_ASR_DISABLE` | `0` | `=1` 显式禁用 ASR |
| `ORATOR_ASR_MAX_NEW_TOKENS` | `32` | 覆盖 ASR 最大解码 token 数 |
| `ORATOR_ASR_SEGMENT_SEC` | `24.0` | 覆盖 ASR 硬分段阈值（秒） |
| `ORATOR_ASR_LANGUAGE` | `Chinese` | 覆盖 ASR 语言提示 |
| `ORATOR_ASR_BAN_STEPS` | `3` | 覆盖 ASR 去重步数 |
| `ORATOR_ASR_DECODE_BATCH` | `4` | 覆盖 ASR 解码批量大小 |
| `ORATOR_ASR_SYSTEM_PROMPT` | `` | 覆盖 ASR 系统提示词 |
| `ORATOR_GPU_TELEMETRY_SEC` | `0` | GPU 遥测推送间隔（秒）；`0`=禁用 |
| `ORATOR_GPU_SERIAL` | `0` | `=1` 强制 GPU 序列化模式（禁用并发流） |
| `ORATOR_GPU_CONCURRENT` | `0` | `=1` 强制 GPU 全并发模式（实验性） |
| `ORATOR_VAD_STREAM` | `1` | `=0` 禁用 VAD 流 |
| `ORATOR_VAD_THRESHOLD` | `0.5` | VAD 敏感度 |
| `ORATOR_VAD_MIN_SPEECH_MS` | `250` | VAD 最短语音 (ms) |
| `ORATOR_VAD_MIN_SILENCE_MS` | `300` | VAD 最短静音 (ms) |
| `ORATOR_VAD_MODEL` | `models/vad/silero_vad.safetensors` | 覆盖 VAD 模型路径 |
| `ORATOR_LOG_LEVEL` | `2` | 日志级别：`0`=DEBUG, `1`=INFO, `2`=WARN, `3`=ERROR |
| `ORATOR_ASR_PROFILE` | — | 设置即启用 ASR 每帧耗时日志 |
| `ORATOR_STREAM_PROGRESS` | — | 设置即显示 diarizer 流式进度条 |
| `ORATOR_TIMEBASE_CHECK` | — | `=1` 启用时间基一致性校验 |
| `ORATOR_STORAGE_DISK_PATH` | `/tmp/orator/storage/` | 协议时间线持久化路径 |
| `ORATOR_SESSION_DIR` | — | 会话持久化目录 |

完整参数列表见 `orator.toml` 文件。加载顺序：编译期默认值 → CLI 参数 → `orator.toml` → 环境变量。

## 主要可执行目标

- `orator_ws`：实时 WebSocket 服务（主入口）
- `asr_testmp3`：ASR 单段端到端测试
- `asr_stream_test`：流式链路测试
- `asr_stream_window_probe`：窗口策略探针工具
- `asr_encoder_chunk_probe`：编码器分块等价性验证
- `asr_stream_incremental_probe`：增量 KV-cache 流式探针

## 测试

```bash
cd build
ctest --output-on-failure
```

包含 40 测试（38 C++ + 2 Python）和可选 Python 集成测试：

- C++ 单元测试覆盖：时间基、ComprehensiveTimeline、ASR 算子、GPU 核函数（Add/Multiply/Normalize/CosineSimilarity）、WebSocket、协议层等
- CUDA kernel 测试（`test_kernels`）：13 个测试，验证 GPU 核函数与 CPU 参考实现的数值等价性
- Python 集成测试（需 pytest）：测试真实 WebSocket 流式路径

```bash
# 单独运行 CUDA kernel 测试
./build/test/test_kernels

# Python 集成测试
cd build && ctest -R py- --output-on-failure
```

## 目录概览

- `include/`：头文件
- `src/`：核心实现
- `tools/`：工具与验证程序
- `test/`：测试
- `models/`：模型与参考数据（含 LFS 大文件）
- `web/`：Web UI（SPA）
- `specs/`：SDD 工件（spec、plan、tasks）

## 系统架构

### 分层结构

依赖方向由外向内指向 `core/`：

```
protocol/  (主题路由、存储、WS 信封)
    ↑
  net/      (WebSocket 服务器、HTTP 静态服务器)
    ↑
pipeline/  (管道编排、工作者线程、时间线)
    ↑
 model/    (Qwen3-ASR, Sortformer, Silero VAD 推理)
    ↑
 gpu/ io/ feature/ → core/ (基础类型、模型接口、CUDA 调度器)
```

### 启动流程

`orator_ws` 是唯一主入口（`src/ws_main.cc`）：

1. **配置加载** — 解析 CLI 参数和 26 环境变量到 `AuditoryStream::Config`
2. **单例管线创建** — `AuditoryStream` 只创建一次（GPU 模型仅加载一次），通过 `shared_ptr` 在所有 WebSocket 连接间共享。每个新连接调用 `Reset()`（清状态但不卸载模型），避免 Jetson OOM
3. **服务器双端口** — 主端口提供 WebSocket 服务（libwebsockets），`port+1` 提供 HTTP 静态服务（零依赖原始 socket），托管 `web/` SPA
4. **服务循环** — 新 WS 连接创建 `AuditoryWsHandler`，共享已有的 `AuditoryStream`

### 三管道体系

三个完全独立的管道在各自线程上运行，共享 **唯一**的东西是音频数据：

```
WS → PushAudio() → SharedAudioBuffer (单生产者)
                          │
              ┌───────────┼───────────┐
              ▼           ▼           ▼
        Diarization      ASR         VAD
         (管线 0)       (管线 1)    (管线 2)
```

| 管道 | 模型 | 产出 | 优先级 |
|------|------|------|--------|
| 说话人分离 | Sortformer (17层 Conformer + 解码器) | `diar/speaker_segment` | 0 (最高) |
| 语音识别 | Qwen3-ASR (KV-cache 流式解码) | `asr/transcript` | 1 |
| 语音检测 | Silero VAD (GPU 批处理) | `vad/speech_segment` | 2 (最低) |

**关键不变量**：
- 管道间**绝不直接通信** — 无共享指针、无原子标志、无回调引用
- 全部通过 `ProtocolTimeline`（主题消息总线）汇集到 `ComprehensiveTimeline`
- ASR 通过 `ProtocolTimeline::Replay()` 读取 VAD 输出（VAD 门控机制），而非直接调用
- 所有时间码从公共 `TimeBase` 派生

### 数据流

```
[1] PCM 二进制入站 → OnBinary → PushAudio() → SharedAudioBuffer
                       │
[2] 三个工作者线程各自读取 → 推理 → ProtocolTimeline::Publish()
                   │                     │
                   │              ComprehensiveTimeline
                   │              (说话人归属: SplitTextByDiar)
                   │                     │
[3] On Flush/End → EmitTimeline → Serialize() → WS 输出
                       │
[4] On Reset → SessionStore::Save() (持久化到磁盘)
```

`ComprehensiveTimeline` 的说话人归属逻辑（`SplitTextByDiar`）：
由于 ASR 不输出字级时间戳，文本按时间比例分配给重叠的说话人段。在说话人边界切割文本区间，每个子区间归属到最大重叠的说话人。

### 协议层 (Spec 004)

主题消息总线，MQTT 风格 pub/sub：

```
ProtocolTimeline
├── PipelineRegistry    (管线生命周期)
├── TopicRouter         (主题路由, 支持 +/# 通配符)
├── StorageManager      (MEMORY 128MB / DISK 文件双后端)
├── TimeIndex           (排序时间索引, 支持 Replay)
└── SchemaRegistry      (版本化模式)
```

- 管线注册返回 RAII `PipelineHandle`，析构自动注销
- 发布流程：序列化 → 存储 → 索引 → 路由 → 锁外分发回调
- WS 消息自动包裹 JSON 信封（topic, pipeline, msg_id, ts, qos, schema_version, data）
- 旧版 `{"type":"vad",...}` 自动兼容

### GPU 调度

通过 `GpuScheduler` 为每个管道分配专用 CUDA 流，三层优先级：

| 优先级 | 管道 | CUDA 优先级 |
|--------|------|-------------|
| 0 | Diarization | 最高 (foreground) |
| 1 | ASR | 中 (foreground) |
| 2 | VAD | 最低 (background) |

在 Jetson (Tegra) 上，CUDA 优先级范围可能只有单值，此时降级为普通流并发。

### Web UI

SPA 架构（`web/app.js`），通过 WS 实时接收事件推送：

| 消息类型 | 处理 |
|----------|------|
| `asr_partial` | 实时文本草稿 |
| `asr` | 确认文本，渲染进 Transcript |
| `timeline` | 三轨 canvas 时间线（说话人/ASR/VAD） |
| `sessions` | 会话历史面板 |
| `gpu_telemetry` | 实时 RTF 指标 |

Canvas 时间线支持缩放、拖拽平移、缩放复位、键盘导航。

### 调试环境变量（仅用于模型开发）

以下环境变量用于 ASR 模型调试和性能分析，不纳入常规配置系统：

| 变量 | 说明 |
|---|---|
| `ORATOR_ASR_PROFILE2` | 逐 token GPU 耗时分析 |
| `ORATOR_ASR_BATCH` | 解码批量大小覆盖 |
| `ORATOR_ASR_DECPROF` | 解码器性能分析 |
| `ORATOR_ASR_NOGRAPH` | 禁用 CUDA Graph 捕获 |
| `ORATOR_ASR_WINDOWED` | 分窗编码器模式 |
| `ORATOR_ASR_CUBLAS_GEMV` | 强制使用 cuBLAS GEMV |

## 贡献

提交代码即表示你同意你的贡献在 AGPL-3.0-or-later 下发布。
