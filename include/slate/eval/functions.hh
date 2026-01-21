// slate/eval/functions.hh
//
// The formula function library. Each call receives its already-evaluated
// operands (scalars and unloaded cell/range references) and returns a Value;
// errors are returned as error Values, not thrown. Text-producing functions
// intern their result as a dynamic string, so the returned Value carries one
// owned reference that the evaluator's stack discipline accounts for.
#ifndef SLATE_EVAL_FUNCTIONS_HH
#define SLATE_EVAL_FUNCTIONS_HH

#include <vector>

#include "slate/eval/operand.hh"
#include "slate/format/wire.hh"
#include "slate/model/value.hh"

namespace slate {

Value call_function(EvalContext& ctx, wire::Func fn,
                    const std::vector<Operand>& args);

// Extended library (ids 60+). Split out to keep the core dispatch readable;
// call_function falls through to this for ids it does not handle directly.
Value call_function_ext(EvalContext& ctx, wire::Func fn,
                        const std::vector<Operand>& args);

// Array library (ids 100+): functions that consume or produce array values.
// Array results are allocated in ctx.arrays and returned as array Values; the
// evaluator records them as temporaries and balances their reference counts.
Value call_array_function(EvalContext& ctx, wire::Func fn,
                          const std::vector<Operand>& args);

// Date library (ids 110+): serial-date arithmetic. call_array_function falls
// through to this for ids it does not handle.
Value call_date_function(EvalContext& ctx, wire::Func fn,
                         const std::vector<Operand>& args);

// Math library (ids 130+): exp/log/trig/rounding/combinatorics.
Value call_math_function(EvalContext& ctx, wire::Func fn,
                         const std::vector<Operand>& args);

// Financial library (ids 160+): time-value-of-money and NPV.
Value call_financial_function(EvalContext& ctx, wire::Func fn,
                              const std::vector<Operand>& args);

// Statistical / info / text library (ids 170+).
Value call_stats_text_function(EvalContext& ctx, wire::Func fn,
                               const std::vector<Operand>& args);

// Lookup / reference library (ids 190+): ROW/COLUMN/ROWS/COLUMNS/ADDRESS,
// COUNTBLANK, and multi-criteria SUMIFS/COUNTIFS.
Value call_lookup_function(EvalContext& ctx, wire::Func fn,
                           const std::vector<Operand>& args);

// Logic / info library (ids 200+): TRUE/FALSE/IFS/SWITCH/IFNA/ISLOGICAL/ISNONTEXT.
Value call_logic_function(EvalContext& ctx, wire::Func fn,
                          const std::vector<Operand>& args);

}  // namespace slate

#endif  // SLATE_EVAL_FUNCTIONS_HH
