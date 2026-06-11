# All tests link the orator_core library.

function(orator_add_test name source)
  add_executable(${name} ${CMAKE_CURRENT_SOURCE_DIR}/${source})
  set_property(TARGET ${name} PROPERTY CUDA_ARCHITECTURES native)
  target_link_libraries(${name} PRIVATE orator_core)
  add_test(NAME ${name} COMMAND ${name})
endfunction()

orator_add_test(test_memory      test_memory.cc)
orator_add_test(test_buffer      test_buffer.cc)
orator_add_test(test_postprocess test_postprocess.cc)
orator_add_test(test_timeline    test_timeline.cc)
orator_add_test(test_speaker_db  test_speaker_db.cc)
orator_add_test(test_pipeline    test_pipeline.cc)
orator_add_test(test_mel         test_mel.cc)
orator_add_test(test_audio       test_audio.cc)
orator_add_test(test_reference   test_reference.cc)
orator_add_test(test_safetensors test_safetensors.cc)
orator_add_test(test_websocket   test_websocket.cc)
target_link_libraries(test_websocket PRIVATE pthread)

# Manual evaluation tool (not a ctest): runs against a real reference file.
add_executable(orator_eval ${CMAKE_CURRENT_SOURCE_DIR}/eval_reference.cc)
set_property(TARGET orator_eval PROPERTY CUDA_ARCHITECTURES native)
target_link_libraries(orator_eval PRIVATE orator_core)
