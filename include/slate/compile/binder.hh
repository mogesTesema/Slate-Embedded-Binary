// slate/compile/binder.hh
//
// Second compile stage: resolve and validate the references a verified Program
// carries. Sheet ids on push_ref/push_range must exist; push_name must cite a
// defined name; every call must name a known function with a legal argument
// count. On success the Program is marked bound and is safe for the evaluator.
#ifndef SLATE_COMPILE_BINDER_HH
#define SLATE_COMPILE_BINDER_HH

#include "slate/compile/program.hh"
#include "slate/model/workbook.hh"
#include "slate/util/diagnostic.hh"
#include "slate/util/status.hh"

namespace slate {

Status bind_program(Program& p, const Workbook& wb, DiagnosticSink& diag);

// Argument-count rules for a function id. Variadic functions report min>=1 and
// max == 0xFF as "unbounded". Unknown ids report {0,0}.
struct Arity { std::uint8_t min; std::uint8_t max; };
Arity function_arity(wire::Func f);

}  // namespace slate

#endif  // SLATE_COMPILE_BINDER_HH
