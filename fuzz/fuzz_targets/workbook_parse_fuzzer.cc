// fuzz/fuzz_targets/workbook_parse_fuzzer.cc
//
// Entry point 1: decode + link a workbook. Exercises the whole format layer and
// the model linker, but stops before evaluation. Reaches real library logic —
// section framing, CRC checks, SST expansion, and cell materialization.
#include <cstddef>
#include <cstdint>

#include "slate/slate.hh"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  slate::probe_workbook(data, size);
  return 0;
}
