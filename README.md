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
- C++17 / CUDA17

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

可选环境变量：

| 变量 | 默认值 | 说明 |
|------|--------|------|
| `ORATOR_UI_PORT` | `port+1` | 覆盖 UI 端口 |
| `ORATOR_UI_ROOT` | `web` | 覆盖静态页面目录 |
| `ORATOR_ASR_MODEL_DIR` | `models/asr/Qwen/Qwen3-ASR-1.7B` | 覆盖 ASR 模型目录 |
| `ORATOR_ASR_DISABLE` | `0` | `=1` 显式禁用 ASR |
| `ORATOR_PREWARM_ON_BOOT` | `1` | `=0` 关闭启动预热 |
| `ORATOR_GPU_TELEMETRY_SEC` | `0` | GPU 遥测推送间隔（秒）；`0`=禁用 |
| `ORATOR_VAD_STREAM` | `1` | `=0` 禁用 VAD 流 |
| `ORATOR_LOG_LEVEL` | `0` | 日志级别：`0`=DEBUG, `1`=INFO, `2`=WARN, `3`=ERROR |
| `ORATOR_ASR_PROFILE` | — | 设置即启用 ASR 每帧耗时日志 |
| `ORATOR_STREAM_PROGRESS` | — | 设置即显示 diarizer 流式进度条 |
| `ORATOR_TIMEBASE_CHECK` | — | `=1` 启用时间基一致性校验 |

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

包含 25+ CTest 测试和可选 Python 集成测试：

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

## 贡献

提交代码即表示你同意你的贡献在 AGPL-3.0-or-later 下发布。
