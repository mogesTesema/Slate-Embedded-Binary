// slate/eval/value_ops.hh
//
// Operations on Values that need the evaluator's stores: numeric/text coercion
// (following spreadsheet rules), and the assignment discipline that keeps string
// and array handles reference-balanced. assign_value is the single sanctioned
// way to overwrite a cell's value — it retains the incoming handle and releases
// the outgoing one, so a slot never ends up tagged one way while its union still
// holds a handle of another.
#ifndef SLATE_EVAL_VALUE_OPS_HH
#define SLATE_EVAL_VALUE_OPS_HH

#include <string>

#include "slate/eval/array_store.hh"
#include "slate/eval/string_interner.hh"
#include "slate/model/value.hh"
#include "slate/model/workbook.hh"

namespace slate {

struct EvalContext {
  const Workbook& wb;
  StringInterner& strings;
  ArrayStore& arrays;
};

// Retain/release the external storage a value references, if any.
void retain_handles(EvalContext& ctx, const Value& v);
void release_handles(EvalContext& ctx, const Value& v);

// Overwrite `slot` with `v`, keeping handle reference counts balanced.
void assign_value(EvalContext& ctx, Value& slot, const Value& v);

// Coerce to a number. Returns true and sets `out` on success; returns false and
// sets `err` when the value is (or coerces to) an error.
bool coerce_number(EvalContext& ctx, const Value& v, double& out,
                   wire::ErrCode& err);

// Render a value as text (numbers formatted, booleans as TRUE/FALSE, errors as
// their code spelling). Does not intern; caller decides ownership.
std::string coerce_text(EvalContext& ctx, const Value& v);

// Structural/ value equality following spreadsheet comparison rules.
bool values_equal(EvalContext& ctx, const Value& a, const Value& b);
int values_compare(EvalContext& ctx, const Value& a, const Value& b);

}  // namespace slate

#endif  // SLATE_EVAL_VALUE_OPS_HH
