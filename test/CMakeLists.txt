# All tests link the orator_core library.

function(orator_add_test name source)
  add_executable(${name} ${CMAKE_CURRENT_SOURCE_DIR}/${source})
  set_property(TARGET ${name} PROPERTY CUDA_ARCHITECTURES native)
  target_link_libraries(${name} PRIVATE orator_core)
  add_test(NAME ${name} COMMAND ${name})
endfunction()

orator_add_test(test_memory      test_memory.cc)
orator_add_test(test_buffer      test_buffer.cc)
orator_add_test(test_shared_buffer test_shared_buffer.cc)
target_link_libraries(test_shared_buffer PRIVATE pthread)
orator_add_test(test_postprocess test_postprocess.cc)
orator_add_test(test_comprehensive_timeline test_comprehensive_timeline.cc)
orator_add_test(test_time_base   test_time_base.cc)
add_executable(test_vad ${CMAKE_CURRENT_SOURCE_DIR}/test_vad.cc ${CMAKE_CURRENT_SOURCE_DIR}/asr_vad_cpu.cc)
  set_property(TARGET test_vad PROPERTY CUDA_ARCHITECTURES native)
  target_link_libraries(test_vad PRIVATE orator_core)
  add_test(NAME test_vad COMMAND test_vad)
orator_add_test(test_speaker_db  test_speaker_db.cc)
orator_add_test(test_diar_stream test_diar_stream.cc)
orator_add_test(test_asr_ops     test_asr_ops.cc)
orator_add_test(test_asr_weights test_asr_weights.cc)
orator_add_test(test_whisper_mel test_whisper_mel.cc)
orator_add_test(test_asr_encoder test_asr_encoder.cc)
orator_add_test(test_asr_decoder test_asr_decoder.cc)
orator_add_test(test_bpe         test_bpe.cc)
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
set_tests_properties(test_bpe PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_vad PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_diar_stream PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
orator_add_test(test_mel         test_mel.cc)
orator_add_test(test_audio       test_audio.cc)
orator_add_test(test_reference   test_reference.cc)
orator_add_test(test_safetensors test_safetensors.cc)
orator_add_test(test_kernels test_kernels.cc)
orator_add_test(test_websocket   test_websocket.cc)
target_link_libraries(test_websocket PRIVATE pthread)
orator_add_test(test_topic_router test_topic_router.cc)
orator_add_test(test_protocol_types test_protocol_types.cc)
orator_add_test(test_pipeline_registry test_pipeline_registry.cc)
orator_add_test(test_storage test_storage.cc)
orator_add_test(test_protocol_timeline test_protocol_timeline.cc)

# Manual evaluation tool (not a ctest): runs against a real reference file.
add_executable(orator_eval ${CMAKE_CURRENT_SOURCE_DIR}/eval_reference.cc)
set_property(TARGET orator_eval PROPERTY CUDA_ARCHITECTURES native)
target_link_libraries(orator_eval PRIVATE orator_core)

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
        ${CMAKE_CURRENT_SOURCE_DIR}/run_py_test.py
        ${script}
    )
    set_tests_properties(${name} PROPERTIES
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      TIMEOUT 180
      LABELS "python;integration"
    )
  endfunction()

  orator_add_py_test(py-ws-comprehensive ws_comprehensive_test.py)
  orator_add_py_test(py-ws-real-audio test_ws_real_audio.py)
endif()
