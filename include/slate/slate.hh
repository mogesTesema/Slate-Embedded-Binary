// slate/slate.hh
//
// Public entry points for the Slate engine — the three surfaces the fuzz
// harnesses drive:
//   * load_workbook       : decode + link a workbook (structure only)
//   * open_and_recalc     : the full pipeline, incl. evaluation and recalc
//   * eval_formula_module : compile + evaluate a standalone formula module
//
// All three are total: malformed input yields a status, never undefined
// behavior. They exist so callers never have to touch the layered headers.
#ifndef SLATE_SLATE_HH
#define SLATE_SLATE_HH

#include <cstddef>
#include <cstdint>

#include "slate/util/diagnostic.hh"
#include "slate/util/status.hh"

namespace slate {

class Workbook;  // forward decl; full type in model/workbook.hh

// Decode and link a workbook. On success `out` is a fully materialized model.
Status load_workbook(const std::uint8_t* data, std::size_t size, Workbook& out,
                     DiagnosticSink& diag);

// Convenience: decode/link and discard, reporting only whether it held together.
// This is what the parse fuzzer calls.
bool probe_workbook(const std::uint8_t* data, std::size_t size);

// Decode, link, and run a full recalculation (evaluation, spills, directives,
// teardown). This is what the recalc fuzzer drives.
Status open_and_recalc(const std::uint8_t* data, std::size_t size,
                       DiagnosticSink& diag);
bool probe_recalc(const std::uint8_t* data, std::size_t size);

// Compile and evaluate a standalone formula module (a compact, self-contained
// bytecode blob with inline numeric inputs). This is what the formula fuzzer
// drives — it reaches the verifier, binder, evaluator, and function library
// without a full workbook.
Status eval_formula_module(const std::uint8_t* data, std::size_t size,
                           DiagnosticSink& diag);
bool probe_formula_module(const std::uint8_t* data, std::size_t size);

}  // namespace slate

#endif  // SLATE_SLATE_HH
