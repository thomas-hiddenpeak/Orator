# Orator 测试目录

本目录包含Orator项目的测试代码，按照工程化标准组织为以下几个主要部分：

## 目录结构

```
test/
├── unit/                     # 单元测试
│   ├── core/                 # 核心组件测试
│   │   ├── test_buffer.cc
│   │   ├── test_time_base.cc
│   │   ├── test_tensor.cc
│   │   └── test_memory.cc
│   ├── model/                # 模型组件测试
│   │   ├── test_asr_encoder.cc
│   │   ├── test_asr_decoder.cc
│   │   ├── test_asr_gemm.cc
│   │   ├── test_mel.cc
│   │   └── test_whisper_mel.cc
│   ├── pipeline/             # 管道组件测试
│   │   ├── test_auditory_stream.cc
│   │   ├── test_diar_stream.cc
│   │   └── test_vad.cc
│   ├── protocol/             # 协议组件测试
│   │   ├── test_protocol_timeline.cc
│   │   ├── test_protocol_types.cc
│   │   └── test_topic_router.cc
│   └── infrastructure/       # 基础设施测试
│       ├── test_gpu_lock.cc
│       ├── test_scheduler.cc
│       ├── test_websocket.cc
│       └── test_http_server.cc
├── integration/              # 集成测试
│   ├── cc/                   # C++集成测试
│   │   └── test_comprehensive_timeline.cc
│   └── py/                   # Python集成测试
│       ├── test_integration.py
│       ├── ws_test_harness.py
│       ├── run_py_test.py
│       └── real_business_test.py
├── data/                     # 测试数据
│   ├── audio/                # 音频测试数据
│   │   ├── test.mp3
│   │   └── test.pcm
│   └── reference/            # 参考数据
├── utils/                    # 测试辅助工具
│   ├── asr_vad_cpu.cc
│   ├── asr_vad_cpu.h
│   └── test_comprehensive_timeline_access.h
├── bench/                    # 性能测试
├── CMakeLists.txt            # 构建配置
└── README.md                 # 测试文档
```

## 测试类型说明

### 单元测试 (unit/)
- **core/**: 测试核心数据结构和基础组件
- **model/**: 测试模型组件如ASR编码器/解码器、Mel特征提取等
- **pipeline/**: 测试管道组件如听觉流、Diarization、VAD等
- **protocol/**: 测试协议相关组件如时间线、主题路由等
- **infrastructure/**: 测试基础设施如GPU锁、调度器、WebSocket服务器等

### 集成测试 (integration/)
- **cc/**: C++级别的集成测试
- **py/**: Python级别的集成测试，包括WebSocket流式测试和真实业务场景测试

### 测试数据 (data/)
- **audio/**: 音频测试文件
- **reference/**: 参考输出数据

### 辅助工具 (utils/)
- 测试过程中使用的辅助函数和类

## 运行测试

### 构建测试
```bash
cmake -S . -B build
cmake --build build -j
```

### 运行所有测试
```bash
cd build && ctest --output-on-failure
```

### 运行特定测试
```bash
cd build && ctest -R test_buffer --output-on-failure
cd build && ctest -R test_asr_encoder --output-on-failure
```

### 运行Python集成测试
```bash
cd build && ctest -R py-integration --output-on-failure
```

## 添加新测试

### 添加C++单元测试
1. 在`unit/`下相应的子目录中创建`test_*.cc`文件
2. 在`CMakeLists.txt`中添加相应的`orator_add_test`调用

### 添加Python集成测试
1. 在`integration/py/`中创建测试脚本
2. 在`CMakeLists.txt`中添加相应的`add_test`调用