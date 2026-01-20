// slate/eval/evaluator.hh
//
// Executes a bound Program against the workbook, producing the formula's result
// Value. String and array temporaries created while evaluating one formula are
// tracked and released when the formula finishes; the single result is returned
// carrying exactly one owned reference, which the caller (recalc) commits into
// the target cell and then releases.
//
// Arrays produced here hold numeric/boolean/error values only — text lives in
// scalar values — so an array's backing never owns string handles and its
// lifetime is purely the ArrayStore slot's reference count.
#ifndef SLATE_EVAL_EVALUATOR_HH
#define SLATE_EVAL_EVALUATOR_HH

#include <vector>

#include "slate/compile/program.hh"
#include "slate/eval/array_store.hh"
#include "slate/eval/operand.hh"
#include "slate/eval/string_interner.hh"
#include "slate/eval/value_ops.hh"
#include "slate/model/workbook.hh"
#include "slate/util/diagnostic.hh"

namespace slate {

class NameResolver;

class Evaluator {
 public:
  Evaluator(Workbook& wb, StringInterner& strings, ArrayStore& arrays)
      : wb_(wb), ctx_{wb, strings, arrays} {}

  // Evaluate one bound program. The returned Value owns one reference to any
  // handle it carries; the caller must commit-and-release it.
  Value run(const Program& p, DiagnosticSink& diag);

  EvalContext& context() { return ctx_; }

  // Give the evaluator the resolved-name table so named ranges can be clamped
  // to their backing sheet's current extent.
  void bind_names(NameResolver* names) { names_ = names; }

 private:
  Value materialize_range(const RangeRef& r);
  Value finalize(const Operand& top);

  Workbook& wb_;
  EvalContext ctx_;
  NameResolver* names_ = nullptr;
  std::vector<Operand> stack_;
  std::vector<std::uint32_t> text_temps_;
  std::vector<std::uint32_t> array_temps_;
};

}  // namespace slate

#endif  // SLATE_EVAL_EVALUATOR_HH
