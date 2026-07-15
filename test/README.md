# Orator 测试目录

当前 CMake 配置注册 64 项 CTest：55 项 C++ 测试、8 项 Python
工具/集成测试、1 项 Node Web UI 状态模型测试。其中真实 WebSocket
合同测试覆盖语音、静音、单音频生产者和并发观察连接。
真实流客户端仍只有 `tools/verify/py/ws_unified_test.py`；
`integration/py/run_ws_integration.py` 只负责服务端生命周期和临时 TOML
隔离，不读写 WebSocket。

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
├── integration/py/
│   └── run_ws_integration.py # 进程启停与测试隔离，不是 WS 客户端
├── web/
│   └── model_contract.test.mjs
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
ctest --test-dir build -R test_ws_contract --output-on-failure
ctest --test-dir build -R test_web_model --output-on-failure
```

部分模型测试需要 CUDA、GPU 和本地模型/参考文件。准确率与产品收官评估还必须
按 `.specify/test-review-protocol.md` 和 Spec 013 走真实 WebSocket、全长音频与全量
上下文语义审核；CTest 或脚本结构指标不能代替该结论。

当前收官基线是流式 Sortformer v2.1 `340/1/188/188`。v2 权重及其旧
`test_diar_stream` 已删除；当前模型门禁由 `test_diar_async_stream` 及
v2.1 高/低延迟配置测试承担。

## 添加测试

1. C++ 测试放入对应分层，并在 `test/CMakeLists.txt` 用
   `orator_add_test` 注册。
2. Python 流式测试必须调用唯一的 `ws_unified_test.py` 客户端；辅助脚本
   只可负责服务端启停、超时、临时 TOML 和产物路径，并通过 CTest 注册。
3. 流式产品测试必须走真实 WebSocket，不得直接调用内部 worker 代替。
4. Web UI 联动测试使用观察连接；观察连接不得重置音频生产者或产生第二套
   时间线。
