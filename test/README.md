# Orator 测试目录

当前 CMake 配置注册 51 项 C++ CTest。本目录尚无已注册的
Python/WebSocket 集成测试；真实流客户端位于
`tools/verify/py/ws_unified_test.py`，恢复自动 CTest 门禁由 Spec 013 跟踪。

## 目录结构

```text
test/
├── unit/
│   ├── core/            # 时基、配置、存储、数据结构
│   ├── model/           # ASR、diar、align、VAD、CUDA 数值测试
│   ├── pipeline/        # worker、AuditoryStream、音频缓存
│   ├── protocol/        # topic、router、protocol timeline
│   └── infrastructure/  # GPU 调度、WebSocket、HTTP
├── integration/cc/
│   ├── test_comprehensive_timeline.cc
│   ├── test_business_speaker_pipeline.cc
│   ├── test_typed_evidence_flow.cc
│   └── test_registration.cc
├── data/
│   ├── audio/test.mp3
│   └── reference/test.txt
├── utils/
│   ├── asr_vad_cpu.{h,cc}
│   └── test_business_speaker_pipeline_access.h
├── CMakeLists.txt
└── README.md
```

## 运行

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

定向运行示例：

```bash
ctest --test-dir build -R test_business_speaker_pipeline --output-on-failure
ctest --test-dir build -R test_typed_evidence_flow --output-on-failure
```

部分模型测试需要 CUDA、GPU 和本地模型/参考文件。准确率与产品收官评估还必须
按 `.specify/test-review-protocol.md` 和 Spec 013 走真实 WebSocket、全长音频与全量
上下文语义审核；CTest 或脚本结构指标不能代替该结论。

## 添加测试

1. C++ 测试放入对应分层，并在 `test/CMakeLists.txt` 用
   `orator_add_test` 注册。
2. Python 测试必须通过 `orator_add_py_test` 注册，并明确服务端启停、
   超时、`orator.toml` 和产物路径。
3. 流式产品测试必须走真实 WebSocket，不得直接调用内部 worker 代替。
