# Orator Performance Documentation

## Overview

This document describes the performance characteristics, benchmarks, and optimization strategies for the Orator real-time auditory pipeline.

## Performance Targets

### Real-Time Processing
| Metric | Target | Notes |
|--------|--------|-------|
| Real-Time Factor (RTF) | < 1.0 | Must process audio faster than real-time |
| End-to-End Latency | < 500ms | From audio input to timeline output |
| ASR Latency | < 300ms | Speech recognition processing time |
| VAD Latency | < 100ms | Voice activity detection time |
| Diarization Latency | < 200ms | Speaker segmentation time |

### Resource Usage
| Metric | Target | Notes |
|--------|--------|-------|
| Memory (CPU) | < 2GB | Typical operation |
| Memory (GPU) | < 8GB | With all models loaded |
| GPU Utilization | > 70% | During active processing |
| CPU Utilization | < 50% | Single core for orchestration |

## Benchmark Results

### Hardware Configuration
- **Platform**: NVIDIA Jetson Orin (32GB/64GB)
- **CUDA**: 12.x
- **Audio**: 16kHz mono PCM

### Streaming Performance
| Test Duration | Audio Size | RTF | Memory Peak | Notes |
|---------------|------------|-----|-------------|-------|
| 60 seconds | 1.9MB | 0.85x | 3.2GB | Baseline |
| 120 seconds | 3.8MB | 0.92x | 4.1GB | Extended |
| 600 seconds | 19MB | 0.95x | 5.8GB | Long session |

### Component Breakdown
| Component | Time per 1s Audio | GPU Memory | CPU Memory |
|-----------|-------------------|------------|------------|
| VAD | ~80ms | 128MB | 32MB |
| ASR Encoder | ~120ms | 1.2GB | 64MB |
| ASR Decoder | ~150ms | 512MB | 32MB |
| Diarization | ~100ms | 800MB | 48MB |
| Protocol | ~20ms | 64MB | 16MB |

## Memory Management

### Audio Buffer
- **Pre-allocation**: Buffer reserves space to avoid reallocation
- **Shrink-to-fit**: `RemovePassedPrefix()` with capacity check
- **Threshold**: Shrink when capacity > 10M samples

### GPU Memory
- **UVM Regions**: Monitored and cleaned regularly
- **RAII Wrappers**: All allocations use smart wrappers
- **Cleanup**: Periodic cleanup of old allocations

### Timeline Data
- **Retention Policy**: Configurable retention window
- **Cleanup**: `CleanupOldData()` removes old entries
- **Default**: 120 seconds retention

## Optimization Strategies

### 1. Buffer Pre-allocation
```cpp
// In SharedAudioBuffer::Append()
if (samples_.size() + n > samples_.capacity()) {
  samples_.reserve(samples_.size() + n + 1024);
}
```

### 2. Fine-Grained Locking
```cpp
// Separate locks for read and write operations
std::mutex read_mutex_;
std::mutex write_mutex_;
```

### 3. VAD-Gated Processing
- Only process ASR when VAD detects speech
- Reduces unnecessary computation
- Typical savings: 40-60% of processing time

### 4. Streaming Window Management
- Fixed-size windows for encoder processing
- Prefix rollback for context preservation
- KV-cache management for decoder

### 5. CUDA Kernel Optimization
- Shared memory usage for tile operations
- Warp-level primitives for reduction
- Async copy for overlap computation

## Monitoring and Profiling

### GPU Telemetry
```bash
# Enable GPU telemetry
export ORATOR_GPU_TELEMETRY_SEC=1.0

# View metrics in logs
tail -f orator_ws.log | grep telemetry
```

### Memory Monitoring
```bash
# Monitor process memory
watch -n 1 'pmap -x <pid> | tail -5'

# Monitor GPU memory
watch -n 1 nvidia-smi
```

### Performance Profiling
```bash
# Profile with Nsight Systems
nsys profile --trace=cuda,nvtx ./build/orator_ws 8765

# Profile with Nsight Compute
ncu --set full ./build/orator_ws 8765
```

## Configuration Tuning

### VAD Parameters
| Parameter | Default | Range | Impact |
|-----------|---------|-------|--------|
| threshold | 0.5 | 0.3-0.7 | Speech detection sensitivity |
| min_speech_ms | 250 | 100-500 | Minimum speech duration |
| min_silence_ms | 300 | 100-1000 | Minimum silence between segments |

### ASR Parameters
| Parameter | Default | Range | Impact |
|-----------|---------|-------|--------|
| max_new_tokens | 32 | 16-128 | Max tokens per segment |
| segment_sec | 24.0 | 10-60 | Segment duration |
| decode_batch | 4 | 1-16 | Parallel decode candidates |

### Diarization Parameters
| Parameter | Default | Range | Impact |
|-----------|---------|-------|--------|
| threshold | 0.4 | 0.3-0.6 | Speaker activity threshold |
| merge_gap_sec | 0.8 | 0.5-2.0 | Gap for segment merging |
| spkcache_len | 5000 | 1000-10000 | Speaker cache length |

## Troubleshooting

### High Memory Usage
1. Check for UVM memory accumulation
2. Verify cleanup mechanisms are enabled
3. Monitor timeline data retention
4. Review buffer pre-allocation settings

### High Latency
1. Check GPU utilization
2. Verify VAD gating is enabled
3. Review segment duration settings
4. Monitor network latency for WebSocket

### Low RTF
1. Check CPU/GPU resource contention
2. Verify CUDA kernel optimization
3. Review buffer sizes
4. Monitor I/O bottlenecks

## Future Optimizations

### Planned Improvements
- [ ] CUDA graph capture for reduced kernel launch overhead
- [ ] FP16 inference for ASR encoder
- [ ] Batch processing for multiple streams
- [ ] Adaptive window sizing based on content
- [ ] Model quantization for reduced memory

### Research Areas
- [ ] Neural VAD for improved accuracy
- [ ] Streaming transformer optimizations
- [ ] Cross-pipeline coordination
- [ ] Dynamic resource allocation

## References

- [CUDA Best Practices Guide](https://docs.nvidia.com/cuda/cuda-c-best-practices-guide/)
- [Nsight Systems Documentation](https://docs.nvidia.com/nsight-systems/)
- [Nsight Compute Documentation](https://docs.nvidia.com/nsight-compute/)