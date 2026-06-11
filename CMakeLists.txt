cmake_minimum_required(VERSION 3.20)
project(Orator CUDA CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

# CUDA setup
find_package(CUDA REQUIRED)
enable_language(CUDA)

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
    src/io/safetensor.cc
    src/io/json_sink.cc
    src/io/audio_file.cc
    src/io/reference_transcript.cc
    # feature extraction (real Sortformer front-end)
    src/feature/mel_spectrogram.cu
    # core data model
    src/core/tensor.cc
    # model implementations (decoupled behind interfaces)
    src/model/streaming_sortformer.cc
    src/model/conformer_preencode.cu
    src/model/conformer_layer.cu
    src/model/sortformer_decoder.cu
    src/model/stub_diarizer.cc
    src/model/stub_embedder.cc
    src/model/speaker_database.cc
    src/model/builtin_registration.cc
    # pipeline orchestration
    src/pipeline/diar_postprocess.cc
    src/pipeline/timeline_merger.cc
    src/pipeline/auditory_pipeline.cc
    # networking (dependency-free WebSocket over POSIX sockets)
    src/net/websocket_server.cc
    src/net/diarization_ws_handler.cc
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
target_link_libraries(orator_core PUBLIC cudart)
if(NOT MSVC)
    target_compile_options(orator_core PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra>)
endif()

# ---------------------------------------------------------------------------
# orator: application entrypoint.
# ---------------------------------------------------------------------------
add_executable(orator src/main.cc)
set_property(TARGET orator PROPERTY CUDA_ARCHITECTURES native)
target_link_libraries(orator PRIVATE orator_core)

# ---------------------------------------------------------------------------
# orator_ws: real-time diarization WebSocket server.
# ---------------------------------------------------------------------------
add_executable(orator_ws src/ws_main.cc)
set_property(TARGET orator_ws PROPERTY CUDA_ARCHITECTURES native)
target_link_libraries(orator_ws PRIVATE orator_core)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()
add_subdirectory(test)
