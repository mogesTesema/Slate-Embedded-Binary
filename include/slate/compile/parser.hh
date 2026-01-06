// slate/compile/parser.hh
//
// A formula text parser: turns source like "=SUM(A1:A3)+B1*2" into the same
// stack bytecode the binary format carries, so mkworkbook and the tests can
// author formulas from text instead of hand-assembled opcodes. It is a
// precedence-climbing (Pratt) parser over a small lexer; references, ranges,
// sheet-qualified references, defined names, string/number literals, the
// arithmetic/comparison/concat operators, and function calls are supported.
//
// Resolution of sheet names, defined names, and interned string literals is
// delegated to the caller via callbacks, so the parser has no dependency on how
// a particular workbook stores those tables.
#ifndef SLATE_COMPILE_PARSER_HH
#define SLATE_COMPILE_PARSER_HH

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "slate/format/wire.hh"
#include "slate/ids.hh"
#include "slate/util/status.hh"

namespace slate {

struct ParseContext {
  SheetId default_sheet = 0;
  // Intern a string literal and return its StrId. Required for string literals.
  std::function<StrId(std::string_view)> intern_string;
  // Resolve a sheet name to its id, or kNoSheet if unknown. Optional; when
  // absent, sheet-qualified references fail.
  std::function<SheetId(std::string_view)> resolve_sheet;
  // Resolve a defined name to its id, or kNoName. Optional.
  std::function<NameId(std::string_view)> resolve_name;
};

// Map a function name (case-insensitive) to its id. Returns true and sets `out`
// on success.
bool function_by_name(std::string_view name, wire::Func& out);

// Parse `src` (with or without a leading '=') into bytecode `out`. On failure
// returns a non-ok status and sets `error` to a human-readable message.
Status parse_formula(std::string_view src, const ParseContext& ctx,
                     std::vector<std::uint8_t>& out, std::string& error);

}  // namespace slate

#endif  // SLATE_COMPILE_PARSER_HH
