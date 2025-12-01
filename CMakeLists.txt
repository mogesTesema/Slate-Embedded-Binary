cmake_minimum_required(VERSION 3.20)
project(slate CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SLATE_BUILD_FUZZERS "Build the libFuzzer targets" ON)
option(SLATE_BUILD_TOOLS "Build the mkworkbook CLI" ON)
option(SLATE_BUILD_TESTS "Build the unit tests" OFF)

include(cmake/sources.cmake)

add_library(slate_core STATIC ${SLATE_CORE_SOURCES})
target_include_directories(slate_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(NOT MSVC)
  target_compile_options(slate_core PRIVATE -Wall -Wextra)
endif()

# The fuzzing engine: ClusterFuzzLite/OSS-Fuzz exports $LIB_FUZZING_ENGINE and
# injects the sanitizer via $CXXFLAGS. For a local developer build we fall back
# to clang's bundled libFuzzer driver.
set(SLATE_FUZZING_ENGINE "$ENV{LIB_FUZZING_ENGINE}")

function(slate_add_fuzzer name src)
  add_executable(${name} ${src})
  target_link_libraries(${name} PRIVATE slate_core)
  if(SLATE_FUZZING_ENGINE)
    target_link_libraries(${name} PRIVATE ${SLATE_FUZZING_ENGINE})
  else()
    target_compile_options(${name} PRIVATE -fsanitize=fuzzer-no-link)
    target_link_options(${name} PRIVATE -fsanitize=fuzzer)
  endif()
endfunction()

if(SLATE_BUILD_FUZZERS)
  slate_add_fuzzer(workbook_parse_fuzzer fuzz/fuzz_targets/workbook_parse_fuzzer.cc)
  slate_add_fuzzer(recalc_fuzzer fuzz/fuzz_targets/recalc_fuzzer.cc)
  slate_add_fuzzer(formula_fuzzer fuzz/fuzz_targets/formula_fuzzer.cc)
endif()

if(SLATE_BUILD_TOOLS)
  add_executable(mkworkbook tools/mkworkbook/main.cc tools/mkworkbook/builder.cc)
  target_include_directories(mkworkbook PRIVATE tools/mkworkbook)
  target_link_libraries(mkworkbook PRIVATE slate_core)

  add_executable(gen_seeds tools/gen_seeds/main.cc tools/mkworkbook/builder.cc)
  target_include_directories(gen_seeds PRIVATE tools/mkworkbook)
  target_link_libraries(gen_seeds PRIVATE slate_core)
endif()

if(SLATE_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
