// slate/compile/verifier.hh
//
// Decodes and type-checks a formula's bytecode into a Program. The verifier
// simulates the operand stack: it confirms every opcode has its inline operands,
// that operations have enough arguments, that the net stack effect leaves a
// single result, and that result types flow consistently (a value's static type
// must match the storage the evaluator will later place under it). It does not
// resolve references — that is the binder's job.
#ifndef SLATE_COMPILE_VERIFIER_HH
#define SLATE_COMPILE_VERIFIER_HH

#include <cstddef>
#include <cstdint>

#include "slate/compile/program.hh"
#include "slate/util/diagnostic.hh"
#include "slate/util/status.hh"

namespace slate {

// Upper bound on formula stack depth; keeps a malicious program from forcing an
// unbounded verifier stack and bounds the evaluator's reservation.
inline constexpr std::uint32_t kMaxFormulaStack = 256;

Status verify_formula(const std::uint8_t* code, std::size_t len, Program& out,
                      DiagnosticSink& diag);

}  // namespace slate

#endif  // SLATE_COMPILE_VERIFIER_HH
