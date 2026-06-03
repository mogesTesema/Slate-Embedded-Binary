// fuzz/fuzz_targets/recalc_fuzzer.cc
//
// Entry point 2: the full pipeline. Decode + link, compile and bind formulas,
// build the dependency graph and evaluation order, evaluate (materializing
// array spills), apply recalc directives (dirty seeds, sheet visibility, extra
// passes), and tear down. Reaches every layer of the engine.
#include <cstddef>
#include <cstdint>

#include "slate/slate.hh"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  slate::probe_recalc(data, size);
  return 0;
}
