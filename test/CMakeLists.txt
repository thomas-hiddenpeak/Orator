# All tests link the orator_core library.

# Add test utils directory to include path for test helper headers
set(TEST_UTILS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/utils)

function(orator_add_test name source)
  add_executable(${name} ${CMAKE_CURRENT_SOURCE_DIR}/${source})
  set_property(TARGET ${name} PROPERTY CUDA_ARCHITECTURES native)
  target_link_libraries(${name} PRIVATE orator_core)
  target_include_directories(${name} PRIVATE ${TEST_UTILS_DIR})
  add_test(NAME ${name} COMMAND ${name})
endfunction()

# Core unit tests
orator_add_test(test_memory      unit/core/test_memory.cc)
orator_add_test(test_buffer      unit/core/test_buffer.cc)
orator_add_test(test_time_base   unit/core/test_time_base.cc)
orator_add_test(test_tensor      unit/core/test_tensor.cc)
orator_add_test(test_shared_buffer unit/core/test_shared_buffer.cc)
target_link_libraries(test_shared_buffer PRIVATE pthread)
orator_add_test(test_postprocess unit/core/test_postprocess.cc)
orator_add_test(test_speaker_db  unit/core/test_speaker_db.cc)
orator_add_test(test_audio       unit/core/test_audio.cc)
orator_add_test(test_reference   unit/core/test_reference.cc)
orator_add_test(test_safetensors unit/core/test_safetensors.cc)
orator_add_test(test_bpe         unit/core/test_bpe.cc)
orator_add_test(test_config      unit/core/test_config.cc)
orator_add_test(test_json        unit/core/test_json.cc)
orator_add_test(test_storage     unit/core/test_storage.cc)
orator_add_test(test_session_store unit/core/test_session_store.cc)
orator_add_test(test_pipeline_registry unit/core/test_pipeline_registry.cc)

# Model unit tests
orator_add_test(test_asr_ops     unit/model/test_asr_ops.cc)
orator_add_test(test_forced_align_decode unit/model/test_forced_align_decode.cc)
orator_add_test(test_aligner_tokenizer unit/model/test_aligner_tokenizer.cc)
orator_add_test(test_aligner_audio unit/model/test_aligner_audio.cc)
orator_add_test(test_aligner_lm unit/model/test_aligner_lm.cc)
orator_add_test(test_aligner_e2e unit/model/test_aligner_e2e.cc)
orator_add_test(test_aligner_mel unit/model/test_aligner_mel.cc)
orator_add_test(test_asr_weights unit/model/test_asr_weights.cc)
orator_add_test(test_whisper_mel unit/model/test_whisper_mel.cc)
orator_add_test(test_titanet     unit/model/test_titanet.cc)
orator_add_test(test_asr_encoder unit/model/test_asr_encoder.cc)
orator_add_test(test_asr_decoder unit/model/test_asr_decoder.cc)
orator_add_test(test_asr_gemm    unit/model/test_asr_gemm.cc)
orator_add_test(test_mel         unit/model/test_mel.cc)
orator_add_test(test_preprocessor unit/model/test_preprocessor.cc)
orator_add_test(test_qwen3       unit/model/test_qwen3.cc)
orator_add_test(test_kernels     unit/model/test_kernels.cc)

# Pipeline unit tests
add_executable(test_vad unit/pipeline/test_vad.cc utils/asr_vad_cpu.cc)
set_property(TARGET test_vad PROPERTY CUDA_ARCHITECTURES native)
target_link_libraries(test_vad PRIVATE orator_core)
target_include_directories(test_vad PRIVATE ${TEST_UTILS_DIR})
add_test(NAME test_vad COMMAND test_vad)

orator_add_test(test_diar_stream unit/pipeline/test_diar_stream.cc)
orator_add_test(test_auditory_stream unit/pipeline/test_auditory_stream.cc)
target_link_libraries(test_auditory_stream PRIVATE pthread)

orator_add_test(test_pipeline_audio_cache unit/pipeline/test_pipeline_audio_cache.cc)
target_link_libraries(test_pipeline_audio_cache PRIVATE pthread)

orator_add_test(test_asr_worker unit/pipeline/test_asr_worker.cc)
target_link_libraries(test_asr_worker PRIVATE pthread)

orator_add_test(test_retained_audio unit/pipeline/test_retained_audio.cc)
target_link_libraries(test_retained_audio PRIVATE pthread)

orator_add_test(test_speaker_id_stage unit/pipeline/test_speaker_id_stage.cc)
target_link_libraries(test_speaker_id_stage PRIVATE pthread)

# Protocol unit tests
orator_add_test(test_topic_router unit/protocol/test_topic_router.cc)
orator_add_test(test_protocol_types unit/protocol/test_protocol_types.cc)
orator_add_test(test_protocol_timeline unit/protocol/test_protocol_timeline.cc)

# Infrastructure unit tests
orator_add_test(test_gpu_lock    unit/infrastructure/test_gpu_lock.cc)
orator_add_test(test_scheduler   unit/infrastructure/test_scheduler.cc)
orator_add_test(test_websocket   unit/infrastructure/test_websocket.cc)
target_link_libraries(test_websocket PRIVATE pthread)
orator_add_test(test_http_server unit/infrastructure/test_http_server.cc)
target_link_libraries(test_http_server PRIVATE pthread)

# Integration tests
orator_add_test(test_comprehensive_timeline integration/cc/test_comprehensive_timeline.cc)
orator_add_test(test_business_speaker_pipeline integration/cc/test_business_speaker_pipeline.cc)
orator_add_test(test_typed_evidence_flow integration/cc/test_typed_evidence_flow.cc)
orator_add_test(test_registration integration/cc/test_registration.cc)

# test_asr_ops reads PyTorch-GPU reference activations under models/reference;
# run it from the repo root so those relative paths resolve.
set_tests_properties(test_asr_ops PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_asr_weights PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_whisper_mel PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_asr_encoder PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_asr_decoder PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_asr_gemm PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_bpe PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_vad PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_diar_stream PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# Manual evaluation tool (not a ctest): runs against a real reference file.
add_executable(orator_eval ${CMAKE_CURRENT_SOURCE_DIR}/eval_reference.cc)
set_property(TARGET orator_eval PROPERTY CUDA_ARCHITECTURES native)
target_link_libraries(orator_eval PRIVATE orator_core)
target_include_directories(orator_eval PRIVATE ${TEST_UTILS_DIR})

# ---------------------------------------------------------------------------
# Python integration tests registered as CTest tests.
# Each runs via cmake -P with working directory set to repo root so
# relative model/ paths resolve.
# ---------------------------------------------------------------------------
find_program(Python3_EXECUTABLE python3)
if(Python3_EXECUTABLE)
  function(orator_add_py_test name script)
    # Uses run_py_test.py wrapper which starts/shuts down orator_ws
    # automatically around the test.
    add_test(NAME ${name}
      COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/integration/py/run_py_test.py
        ${script}
    )
    set_tests_properties(${name} PROPERTIES
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      TIMEOUT 180
      LABELS "python;integration"
    )
  endfunction()
endif()
