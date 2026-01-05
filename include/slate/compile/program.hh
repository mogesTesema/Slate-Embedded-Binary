// slate/compile/program.hh
//
// The compiled form of a formula: a flat instruction list plus the stack depth
// the evaluator must reserve. Compiling once (verify + bind) means the evaluator
// never re-decodes raw bytes, and the abstract type attached to each result is
// available to catch tag/storage mismatches before they reach runtime.
#ifndef SLATE_COMPILE_PROGRAM_HH
#define SLATE_COMPILE_PROGRAM_HH

#include <cstdint>
#include <vector>

#include "slate/format/wire.hh"
#include "slate/ids.hh"
#include "slate/model/sheet.hh"

namespace slate {

// Abstract value type tracked by the verifier's operand stack.
enum class VType : std::uint8_t {
  any = 0,
  number,
  text,
  boolean,
  error,
  scalar,   // one of number/text/boolean/error, exact kind not known statically
  ref,      // a single-cell reference
  range,    // a rectangular reference
  array,    // a materialized array value
};

struct Instr {
  wire::Op op;
  double num = 0.0;   // push_num
  std::uint32_t a = 0, b = 0, c = 0, d = 0, e = 0;  // generic operands
  std::uint16_t func = 0;
  std::uint8_t argc = 0;
  VType result = VType::any;  // static result type, filled by the verifier
};

struct Program {
  std::vector<Instr> code;
  std::uint32_t max_stack = 0;
  bool bound = false;   // set once the binder has validated references

  // Resolved once at bind time (indexed by NameId): the backing sheet each
  // push_name instruction in this program targets. Populated so the evaluator
  // doesn't have to re-walk the defined-names table on every push_name it
  // executes, across however many recalc passes reuse this bound program.
  std::vector<const SheetData*> name_sheet_cache;

  bool empty() const { return code.empty(); }
};

}  // namespace slate

#endif  // SLATE_COMPILE_PROGRAM_HH
