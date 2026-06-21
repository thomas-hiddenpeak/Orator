cmake_minimum_required(VERSION 3.20)
project(Orator CUDA CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_STANDARD 20)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

# CUDA setup
find_package(CUDA REQUIRED)
find_package(Threads REQUIRED)
enable_language(CUDA)

# ---------------------------------------------------------------------------
# tomlplusplus (MIT license) - header-only TOML config parser
include(FetchContent)
FetchContent_Declare(
  tomlplusplus
  GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
  GIT_TAG v3.4.0
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(tomlplusplus)

# libwebsockets (MIT license) - robust WebSocket server implementation
# Replaces hand-rolled websocket_server.cc with industrial-grade WS support:
# - Multi-client concurrent connections
# - Proper EINTR handling, timeouts, backpressure
# - Permessage-deflate compression (RFC 7692)
# - TLS/WSS support (optional)
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
  libwebsockets
  GIT_REPOSITORY https://github.com/warmcat/libwebsockets.git
  GIT_TAG v4.3.3
  GIT_SHALLOW TRUE
)
# Disable unnecessary features to keep build minimal
set(LWS_WITH_SSL OFF CACHE BOOL "" FORCE)
set(LWS_WITHOUT_TESTAPPS ON CACHE BOOL "" FORCE)
set(LWS_WITH_LEJP ON CACHE BOOL "" FORCE)
set(LWS_WITH_LEJP_PULL ON CACHE BOOL "" FORCE)
set(LWS_WITH_SYS_Signal ON CACHE BOOL "" FORCE)
set(LWS_WITH_IPV6 ON CACHE BOOL "" FORCE)
set(LWS_UNIXSOCK OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(libwebsockets)

# Resolve the CUDA runtime library explicitly (Tegra/ARM64 friendly).
link_directories(${CUDA_TOOLKIT_ROOT_DIR}/lib64)

# ---------------------------------------------------------------------------
# orator_core: all library code (everything except the application entrypoint).
# Layered: infrastructure (gpu/io) -> core data model + interfaces -> model
# implementations -> pipeline orchestration. The executable and all tests link
# this single target.
# ---------------------------------------------------------------------------
set(ORATOR_CORE_SOURCES
    # infrastructure
    src/gpu/memory.cc
    src/gpu/buffer.cc
    src/gpu/kernels.cu
    src/gpu/gpu_lock.cc
    src/gpu/scheduler.cc
    src/io/safetensor.cc
    src/io/sharded_safetensor.cc
    src/io/bpe_tokenizer.cc
    src/io/json_sink.cc
    src/io/audio_file.cc
    src/io/reference_transcript.cc
    src/io/config_reader.cc
    # feature extraction (real Sortformer front-end)
    src/feature/mel_spectrogram.cu
    src/feature/whisper_mel.cu
    # core data model
    src/core/tensor.cc
    # model implementations (decoupled behind interfaces)
    src/model/streaming_sortformer.cc
    src/model/conformer_preencode.cu
    src/model/conformer_layer.cu
    src/model/sortformer_decoder.cu
    src/model/asr_ops.cu
    src/model/asr_gemm.cu
    src/model/asr_audio_tower.cu
    src/model/asr_text_decoder.cu
    src/model/qwen3_asr.cc
    src/model/speaker_database.cc
    src/model/builtin_registration.cc
    # pipeline orchestration
    src/pipeline/diar_postprocess.cc
    src/pipeline/json_util.cc
    src/pipeline/auditory_stream.cc
    src/pipeline/asr_preprocessor.cc
    src/pipeline/comprehensive_timeline.cc
    src/pipeline/gpu_vad.cu
    src/pipeline/shared_audio_buffer.cc
    src/pipeline/diarization_worker.cc
    src/pipeline/asr_worker.cc
    # networking (libwebsockets-based WebSocket server)
    src/net/http_static_server.cc
    src/net/websocket_server.cc
    src/net/auditory_ws_handler.cc
    # protocol layer (Spec 004)
    src/protocol/pipeline_registry.cc
    src/protocol/topic_router.cc
    src/protocol/storage.cc
    src/protocol/memory_backend.cc
    src/protocol/disk_backend.cc
    src/protocol/time_index.cc
    src/protocol/protocol_timeline.cc
    src/protocol/ws_envelope.cc
    src/protocol/session_store.cc
)

add_library(orator_core STATIC ${ORATOR_CORE_SOURCES})
set_property(TARGET orator_core PROPERTY CUDA_ARCHITECTURES native)
target_include_directories(orator_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CUDA_INCLUDE_DIRS}
)
target_include_directories(orator_core PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/minimp3
)
target_link_libraries(orator_core PUBLIC cudart cublas Threads::Threads websockets tomlplusplus::tomlplusplus)
if(NOT MSVC)
    target_compile_options(orator_core PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra>)
endif()

# ---------------------------------------------------------------------------
# orator_ws: real-time diarization + ASR WebSocket server (the realtime entry).
# ---------------------------------------------------------------------------
add_executable(orator_ws src/ws_main.cc)
set_property(TARGET orator_ws PROPERTY CUDA_ARCHITECTURES native)
target_link_libraries(orator_ws PRIVATE orator_core)

# ---------------------------------------------------------------------------
# asr_testmp3: native Qwen3-ASR end-to-end on an audio clip.
# ---------------------------------------------------------------------------
add_executable(asr_testmp3 tools/asr_testmp3.cc)
set_property(TARGET asr_testmp3 PROPERTY CUDA_ARCHITECTURES native)
target_link_libraries(asr_testmp3 PRIVATE orator_core)

# ---------------------------------------------------------------------------
# asr_stream_test: drive the streaming AuditoryStream (diar + asr) frame by
# frame, exactly like the WebSocket server -- the real streaming path.
# ---------------------------------------------------------------------------
add_executable(asr_stream_test tools/asr_stream_test.cc)
set_property(TARGET asr_stream_test PROPERTY CUDA_ARCHITECTURES native)
target_link_libraries(asr_stream_test PRIVATE orator_core)

# ---------------------------------------------------------------------------
# asr_stream_window_probe: evaluate the official growing-window streaming
# method (audio_accum + prefix rollback) and observe memory / RTF growth.
# ---------------------------------------------------------------------------
add_executable(asr_stream_window_probe tools/asr_stream_window_probe.cc)
set_property(TARGET asr_stream_window_probe PROPERTY CUDA_ARCHITECTURES native)
target_link_libraries(asr_stream_window_probe PRIVATE orator_core)

# ---------------------------------------------------------------------------
# asr_encoder_chunk_probe (Spec 003 T011): verify windowed-encoder chunk-local
# equivalence (standalone window encode == slice of full encode).
# ---------------------------------------------------------------------------
add_executable(asr_encoder_chunk_probe tools/asr_encoder_chunk_probe.cc)
set_property(TARGET asr_encoder_chunk_probe PROPERTY CUDA_ARCHITECTURES native)
target_link_libraries(asr_encoder_chunk_probe PRIVATE orator_core)

# ---------------------------------------------------------------------------
# asr_stream_incremental_probe (Spec 003 T031): drive the incremental KV-cache
# streaming session; measure per-step cost, RTF, memory; write transcript JSON.
# ---------------------------------------------------------------------------
add_executable(asr_stream_incremental_probe tools/asr_stream_incremental_probe.cc)
set_property(TARGET asr_stream_incremental_probe PROPERTY CUDA_ARCHITECTURES native)
target_link_libraries(asr_stream_incremental_probe PRIVATE orator_core)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()
add_subdirectory(test)
