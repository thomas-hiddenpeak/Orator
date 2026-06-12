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

## 主要可执行目标

- `orator_ws`：实时 WebSocket 服务（主入口）
- `asr_testmp3`：ASR 单段端到端测试
- `asr_stream_test`：流式链路测试
- `asr_stream_window_probe`：窗口策略探针工具

## 测试

```bash
cd build
ctest --output-on-failure
```

## 目录概览

- `include/`：头文件
- `src/`：核心实现
- `tools/`：工具与验证程序
- `test/`：测试
- `models/`：模型与参考数据（含 LFS 大文件）

## 贡献

提交代码即表示你同意你的贡献在 AGPL-3.0-or-later 下发布。
