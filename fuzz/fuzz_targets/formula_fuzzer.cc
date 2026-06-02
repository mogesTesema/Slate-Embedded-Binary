// fuzz/fuzz_targets/formula_fuzzer.cc
//
// Entry point 3: a standalone formula module. Compiles and evaluates a compact
// bytecode blob with inline numeric inputs, exercising the verifier, binder,
// evaluator, function library, array store, and dynamic string interner via a
// smaller, different input shape than the workbook container.
#include <cstddef>
#include <cstdint>

#include "slate/slate.hh"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  slate::probe_formula_module(data, size);
  return 0;
}
