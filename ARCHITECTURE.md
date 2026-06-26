# Orator Architecture Documentation

## Overview

Orator is a real-time edge-deployed auditory pipeline designed for NVIDIA Jetson Orin/Thor platforms. It ingests mono PCM audio over WebSocket, runs diarization (Sortformer), ASR (Qwen3-ASR), and VAD as three independent pipelines on a shared time base, and outputs a comprehensive timeline JSON.

## System Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        WebSocket Server                         │
│                     (libwebsockets-based)                       │
└──────────────┬──────────────────────────────────┬───────────────┘
               │                                  │
    ┌──────────▼──────────┐          ┌───────────▼────────────┐
    │  Audio Ingestion    │          │   Timeline Output      │
    │  (PCM 16kHz mono)   │          │   (JSON stream)        │
    └──────────┬──────────┘          └───────────┬────────────┘
               │                                  │
               ▼                                  │
    ┌─────────────────────────────────────────────┤
    │           AuditoryStream Controller          │
    │         (Pipeline Orchestration)             │
    └──┬─────────────┬──────────────┬─────────────┘
       │             │              │
  ┌────▼────┐  ┌────▼────┐    ┌────▼────┐
  │   VAD   │  │  ASR    │    │ Diarizer│
  │ Pipeline│  │ Pipeline│    │ Pipeline│
  └─────────┘  └─────────┘    └─────────┘
       │             │              │
       └─────────────┴──────────────┘
                     │
                     ▼
            ┌────────────────┐
            │ Time Base Sync │
            │ (Shared Clock) │
            └────────────────┘
```

## Core Components

### 1. Core Layer (`include/core/`, `src/core/`)

The foundation layer containing interfaces, data types, and contracts.

**Key Components:**
- `TimeBase`: Shared time reference for all pipelines
- `AudioBuffer`: Shared audio buffer with sample management
- `SpeakerSegment`: Diarization result container
- `TranscriptSegment`: ASR result container
- `VadSegment`: VAD result container

**Design Principles:**
- Pure contracts and data types only
- No implementation details
- Thread-safe data structures

### 2. GPU Layer (`include/gpu/`, `src/gpu/`)

GPU memory management and synchronization utilities.

**Key Components:**
- `DeviceAllocator`: CUDA device memory allocator
- `UnifiedAllocator`: CUDA unified memory allocator (UVM)
- `PinnedAllocator`: CUDA pinned memory allocator
- `HostAllocator`: CPU memory allocator
- `GpuLock`: GPU synchronization primitive

**Design Principles:**
- RAII-based memory management
- Error-checked CUDA calls
- Zero raw `cudaMalloc`/`cudaFree` on performance paths

### 3. Feature Layer (`include/feature/`, `src/feature/`)

Audio feature extraction (mel spectrograms, etc.).

**Key Components:**
- `MelSpectrogram`: Mel frequency spectrogram computation
- `WhisperMel`: Whisper-compatible mel extraction
- `AudioPreprocessor`: Audio preprocessing pipeline

**Design Principles:**
- CPU-based feature extraction
- Optimized for streaming operation
- Reference-validated against PyTorch implementations

### 4. I/O Layer (`include/io/`, `src/io/`)

File I/O, tokenizers, and configuration management.

**Key Components:**
- `ConfigReader`: TOML configuration file reader
- `Tokenizer`: BPE tokenizer for ASR
- `SafetensorsReader`: Model weight file reader
- `AudioReader`: Audio file reader (PCM, WAV, etc.)

**Design Principles:**
- Platform-agnostic I/O abstractions
- Efficient file parsing
- Configuration hierarchy support

### 5. Model Layer (`include/model/`, `src/model/`)

Model interfaces and implementations.

**Key Components:**
- `IAsr`: ASR model interface
- `IDiarizer`: Diarization model interface
- `IVad`: VAD model interface
- `Qwen3Asr`: Qwen3 ASR implementation
- `SortformerDiarizer`: Sortformer diarization implementation
- `SileroVad`: Silero VAD implementation

**Design Principles:**
- Interface-based design
- Registerable model implementations
- No tight coupling between models and consumers
- CUDA-accelerated inference

### 6. Pipeline Layer (`include/pipeline/`, `src/pipeline/`)

Pipeline orchestration and coordination.

**Key Components:**
- `AuditoryStream`: Main pipeline controller
- `VadPipeline`: VAD processing pipeline
- `AsrPipeline`: ASR processing pipeline
- `DiarPipeline`: Diarization processing pipeline
- `ComprehensiveTimeline`: Unified timeline container

**Design Principles:**
- Independent pipeline execution
- Shared time base synchronization
- No direct pipeline-to-pipeline communication
- Event-driven architecture

### 7. Network Layer (`include/net/`, `src/net/`)

WebSocket transport and HTTP server.

**Key Components:**
- `WebSocketServer`: libwebsockets-based WebSocket server
- `AuditoryWsHandler`: WebSocket connection handler
- `HttpStaticServer`: Static file HTTP server for web UI

**Design Principles:**
- Industrial-grade WebSocket implementation
- Multi-client support
- Proper backpressure handling
- TLS/WSS support (optional)

### 8. Protocol Layer (`include/protocol/`, `src/protocol/`)

Topic-based message protocol and storage.

**Key Components:**
- `ProtocolTimeline`: Protocol message timeline
- `PipelineRegistry`: Pipeline registration and discovery
- `TopicRouter`: Message routing based on topics
- `StorageManager`: Message storage (memory/disk)
- `TimeIndex`: Time-based message indexing

**Design Principles:**
- Pub/sub messaging pattern
- Persistent message storage
- Time-ordered message replay
- Schema validation

## Data Flow

### Audio Ingestion Flow
```
Client → WebSocket → AudioBuffer → VAD/ASR/Diar Pipelines
```

### Timeline Output Flow
```
Pipelines → ComprehensiveTimeline → ProtocolTimeline → WebSocket → Client
```

### Time Synchronization
```
All pipelines inherit from buffer_.time_base()
TimeBase::SecondsAt() / SampleAt() / Duration() for time calculations
```

## Threading Model

```
┌─────────────────────────────────────────────────────────┐
│                    Main Thread                           │
│  - WebSocket server event loop                          │
│  - HTTP server event loop                               │
│  - Client connection management                         │
└─────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│                  Pipeline Threads                        │
│  - VAD thread: Voice activity detection                 │
│  - ASR thread: Speech recognition                       │
│  - Diar thread: Speaker diarization                     │
└─────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│                  GPU Threads                             │
│  - CUDA kernel execution                                │
│  - Memory management                                    │
│  - Synchronization                                      │
└─────────────────────────────────────────────────────────┘
```

## Memory Management

### Audio Buffer
- `SharedAudioBuffer` with pre-allocation and shrink-to-fit
- `RemovePassedPrefix()` cleanup mechanism
- Thread-safe access with fine-grained locking

### GPU Memory
- RAII wrappers for all GPU allocations
- `cudaMallocManaged` for unified memory (UVM)
- Regular cleanup to prevent memory accumulation
- Memory telemetry (optional)

### Timeline Data
- `ComprehensiveTimeline::CleanupOldData()` for old data cleanup
- `ProtocolTimeline::CleanupOldData()` for protocol message cleanup
- Configurable retention policies

## Configuration Hierarchy

Configuration loading order (later overrides earlier):
1. Compile-time defaults (C++ struct initializers)
2. `orator.toml` configuration file
3. Environment variables (`ORATOR_*`)
4. CLI arguments

## Performance Characteristics

### Target Metrics
- Real-time factor (RTF) < 1.0 for 1x streaming
- Latency < 500ms for ASR results
- Memory usage < 2GB for typical operation
- GPU utilization > 70% during processing

### Optimization Strategies
- Pre-allocated buffers to avoid reallocation
- Fine-grained locking for concurrent access
- CUDA kernel optimization
- Streaming window management
- VAD-gated ASR processing

## Security Considerations

- Input validation for all client data
- Resource limits to prevent DoS
- Secure WebSocket connections (WSS support)
- Model file integrity verification
- Memory safety (RAII, no raw pointers)

## Testing Strategy

### Unit Tests
- Component-level testing
- Mock dependencies where appropriate
- Fast execution (< 1s per test)

### Integration Tests
- Pipeline integration testing
- Real audio processing validation
- WebSocket client simulation

### Performance Tests
- Streaming performance validation
- Memory usage monitoring
- GPU utilization tracking

## Deployment

### Target Platforms
- NVIDIA Jetson Orin (32GB/64GB)
- NVIDIA Jetson Thor
- x86_64 with NVIDIA GPU (development/testing)

### Container Support
- Dockerfile for containerized deployment
- NVIDIA Container Toolkit integration
- Volume mounts for model files

### Monitoring
- GPU telemetry (optional)
- Log level configuration
- Health check endpoints
- Performance metrics collection