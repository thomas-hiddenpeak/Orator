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
target_compile_definitions(test_safetensors PRIVATE
  ORATOR_TEST_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
orator_add_test(test_bpe         unit/core/test_bpe.cc)
orator_add_test(test_config      unit/core/test_config.cc)
target_compile_definitions(test_config PRIVATE
  ORATOR_TEST_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
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
orator_add_test(test_gemm_batched unit/model/test_gemm_batched.cu)

# Pipeline unit tests
add_executable(test_vad unit/pipeline/test_vad.cc utils/asr_vad_cpu.cc)
set_property(TARGET test_vad PROPERTY CUDA_ARCHITECTURES native)
target_link_libraries(test_vad PRIVATE orator_core)
target_include_directories(test_vad PRIVATE ${TEST_UTILS_DIR})
add_test(NAME test_vad COMMAND test_vad)

orator_add_test(test_diar_sync_v21 unit/pipeline/test_diar_sync_v21.cc)
orator_add_test(test_diar_async_stream unit/pipeline/test_diar_async_stream.cc)
add_test(NAME test_diar_async_stream_v21_high
  COMMAND test_diar_async_stream
    ${CMAKE_SOURCE_DIR}/specs/013-industrial-closing-validation/sortformer-v21-high-latency.toml
    ${CMAKE_SOURCE_DIR}/models/reference/ref_stream_async_v21_high_official_long.f32
    3)
add_test(NAME test_diar_async_stream_v21_low
  COMMAND test_diar_async_stream
    ${CMAKE_SOURCE_DIR}/specs/013-industrial-closing-validation/sortformer-v21-low-latency.toml
    ${CMAKE_SOURCE_DIR}/models/reference/ref_stream_async_v21_low_official_long.f32
    3)
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
orator_add_test(test_speaker_evidence_stage
  unit/pipeline/test_speaker_evidence_stage.cc)

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
set_tests_properties(test_diar_sync_v21 PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_diar_async_stream PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(
    test_diar_async_stream_v21_high
    test_diar_async_stream_v21_low
  PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    TIMEOUT 120
    LABELS "cuda;unit;model-parity")

# Real-WebSocket integration uses the one authoritative Python socket client.
# The runner only owns process lifecycle and test isolation; it never opens a
# WebSocket itself.
find_program(Python3_EXECUTABLE python3)
if(Python3_EXECUTABLE)
  add_test(NAME test_repro_manifest
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/integration/py/test_repro_manifest.py)
  set_tests_properties(test_repro_manifest PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    TIMEOUT 30
    LABELS "python;unit;reproducibility")

  add_test(NAME test_ws_unified_manifest
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/integration/py/test_ws_unified_manifest.py)
  set_tests_properties(test_ws_unified_manifest PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    TIMEOUT 30
    LABELS "python;unit;reproducibility")

  add_test(NAME test_closing_ledger
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/integration/py/test_closing_ledger.py)
  set_tests_properties(test_closing_ledger PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    TIMEOUT 30
    LABELS "python;unit;manual-review")

  add_test(NAME test_speaker_frozen_evidence
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/integration/py/test_speaker_frozen_evidence.py)
  set_tests_properties(test_speaker_frozen_evidence PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    TIMEOUT 30
    LABELS "python;unit;model-evidence")

  add_test(NAME test_speaker_decision_evidence
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/integration/py/test_speaker_decision_evidence.py)
  set_tests_properties(test_speaker_decision_evidence PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    TIMEOUT 30
    LABELS "python;unit;model-evidence")

  add_test(NAME test_speaker_business_replay_inputs
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/integration/py/test_speaker_business_replay_inputs.py)
  set_tests_properties(test_speaker_business_replay_inputs PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    TIMEOUT 30
    LABELS "python;unit;model-evidence")

  add_test(NAME test_speaker_primary_top1_replay_inputs
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/integration/py/test_speaker_primary_top1_replay_inputs.py)
  set_tests_properties(test_speaker_primary_top1_replay_inputs PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    TIMEOUT 30
    LABELS "python;unit;model-evidence")

  add_test(NAME test_speaker_multiprototype_evidence
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/integration/py/test_speaker_multiprototype_evidence.py)
  set_tests_properties(test_speaker_multiprototype_evidence PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    TIMEOUT 30
    LABELS "python;unit;model-evidence")

  add_test(NAME test_speaker_robust_gallery_evidence
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/integration/py/test_speaker_robust_gallery_evidence.py)
  set_tests_properties(test_speaker_robust_gallery_evidence PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    TIMEOUT 30
    LABELS "python;unit;model-evidence")

  add_test(NAME test_speaker_business_review_packet
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/integration/py/test_speaker_business_review_packet.py)
  set_tests_properties(test_speaker_business_review_packet PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    TIMEOUT 30
    LABELS "python;unit;manual-review")

  add_test(NAME test_ws_contract
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/integration/py/run_ws_integration.py
      --repo ${CMAKE_SOURCE_DIR}
      --server $<TARGET_FILE:orator_ws>
      --client ${CMAKE_SOURCE_DIR}/tools/verify/py/ws_unified_test.py
      --config ${CMAKE_SOURCE_DIR}/orator.toml
      --audio ${CMAKE_SOURCE_DIR}/test/data/audio/test.mp3
      --artifacts ${CMAKE_BINARY_DIR}/test-artifacts/ws_contract)
  set_tests_properties(test_ws_contract PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    TIMEOUT 240
    RUN_SERIAL TRUE
    SKIP_RETURN_CODE 77
    LABELS "python;integration;websocket")
endif()

# Browser-state contract tests use only Node's standard test runner. The Web UI
# remains dependency-free and is still served as raw ES modules.
find_program(Node_EXECUTABLE node)
if(Node_EXECUTABLE)
  add_test(NAME test_web_model
    COMMAND ${Node_EXECUTABLE}
      --test
      ${CMAKE_CURRENT_SOURCE_DIR}/web/model_contract.test.mjs)
  set_tests_properties(test_web_model PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    TIMEOUT 30
    LABELS "javascript;unit")
endif()
