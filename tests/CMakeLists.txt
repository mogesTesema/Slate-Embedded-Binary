# tests/CMakeLists.txt
add_executable(slate_tests
  tests_main.cc
  test_util.cc
  test_format.cc
  test_recalc.cc
  test_functions.cc
  test_arrays.cc
  test_codec.cc
  test_compaction.cc
  test_merges.cc
  test_number_format.cc
  test_parser.cc
  test_dates.cc
  test_math.cc
  test_stats_text.cc
  test_lookup.cc
  test_logic.cc
  test_names.cc
  test_recalc_stats.cc
  ${CMAKE_SOURCE_DIR}/tools/mkworkbook/builder.cc
)
target_include_directories(slate_tests PRIVATE
  ${CMAKE_SOURCE_DIR}/tools/mkworkbook
  ${CMAKE_SOURCE_DIR}/src/format
  ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(slate_tests PRIVATE slate_core)
add_test(NAME slate_tests COMMAND slate_tests)
