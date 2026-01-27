// slate/eval/lookup_functions.cc
//
// Reference-shape functions (ROW/COLUMN/ROWS/COLUMNS/ADDRESS), COUNTBLANK, and
// the multi-criteria aggregates SUMIFS/COUNTIFS. These operate on reference
// operands (kept unloaded by the parser) so they can inspect coordinates rather
// than just values. Criteria matching is shared with SUMIF/COUNTIF via
// criteria.hh.
#include "slate/eval/functions.hh"

#include <string>
#include <vector>

#include "criteria.hh"
#include "slate/model/reference.hh"

namespace slate {
namespace {

Value err(wire::ErrCode e) { return Value::error(e); }

Value make_text(EvalContext& ctx, const std::string& s) {
  return Value::text(ctx.strings.intern_dynamic(s));
}

Value fn_row(const Operand& op) {
  if (op.is_cell()) return Value::number(static_cast<double>(op.ref.row + 1));
  if (op.is_range()) return Value::number(static_cast<double>(op.range.r0 + 1));
  return Value::error(wire::ErrCode::ref);
}

Value fn_column(const Operand& op) {
  if (op.is_cell()) return Value::number(static_cast<double>(op.ref.col + 1));
  if (op.is_range()) return Value::number(static_cast<double>(op.range.c0 + 1));
  return Value::error(wire::ErrCode::ref);
}

Value fn_rows(const Operand& op) {
  if (op.is_cell()) return Value::number(1);
  if (op.is_range()) return Value::number(static_cast<double>(op.range.r1 - op.range.r0 + 1));
  return Value::error(wire::ErrCode::ref);
}

Value fn_columns(const Operand& op) {
  if (op.is_cell()) return Value::number(1);
  if (op.is_range()) return Value::number(static_cast<double>(op.range.c1 - op.range.c0 + 1));
  return Value::error(wire::ErrCode::ref);
}

Value fn_address(EvalContext& ctx, const std::vector<Operand>& args) {
  double rd, cd; wire::ErrCode e;
  if (!coerce_number(ctx, load_scalar(ctx, args[0]), rd, e)) return err(e);
  if (!coerce_number(ctx, load_scalar(ctx, args[1]), cd, e)) return err(e);
  if (rd < 1 || cd < 1) return err(wire::ErrCode::value);
  std::string s = "$" + column_label(static_cast<std::uint32_t>(cd) - 1) + "$" +
                  std::to_string(static_cast<long long>(rd));
  return make_text(ctx, s);
}

Value fn_countblank(EvalContext& ctx, const Operand& op) {
  long n = 0;
  for_each_scalar(ctx, op, [&](const Value& v) { if (v.is_empty()) ++n; });
  return Value::number(static_cast<double>(n));
}

// Shared driver for SUMIFS/COUNTIFS. `sum_first` points at the sum range (or is
// npos for COUNTIFS); criteria pairs follow starting at `crit_first`.
Value conditional_multi(EvalContext& ctx, const std::vector<Operand>& args,
                        std::size_t sum_index, std::size_t crit_first) {
  std::vector<CellRef> sum_cells;
  if (sum_index != static_cast<std::size_t>(-1))
    detail::range_cells(args[sum_index], sum_cells);

  // Gather (range, criteria) pairs.
  struct Pair { std::vector<CellRef> cells; detail::Criteria crit; };
  std::vector<Pair> pairs;
  for (std::size_t i = crit_first; i + 1 < args.size(); i += 2) {
    Pair pr;
    detail::range_cells(args[i], pr.cells);
    pr.crit = detail::parse_criteria(ctx, args[i + 1]);
    pairs.push_back(std::move(pr));
  }
  if (pairs.empty()) return Value::number(0);

  std::size_t n = pairs[0].cells.size();
  double acc = 0.0;
  long count = 0;
  for (std::size_t i = 0; i < n; ++i) {
    bool all = true;
    for (const Pair& pr : pairs) {
      if (i >= pr.cells.size()) { all = false; break; }
      if (!detail::criteria_match(ctx, pr.crit, cell_value(ctx, pr.cells[i]))) {
        all = false;
        break;
      }
    }
    if (!all) continue;
    ++count;
    if (sum_index != static_cast<std::size_t>(-1) && i < sum_cells.size()) {
      double d; wire::ErrCode e;
      if (coerce_number(ctx, cell_value(ctx, sum_cells[i]), d, e)) acc += d;
    }
  }
  return sum_index != static_cast<std::size_t>(-1)
             ? Value::number(acc)
             : Value::number(static_cast<double>(count));
}

}  // namespace

Value call_lookup_function(EvalContext& ctx, wire::Func fn,
                           const std::vector<Operand>& args) {
  using F = wire::Func;
  switch (fn) {
    case F::row_fn: return fn_row(args[0]);
    case F::column_fn: return fn_column(args[0]);
    case F::rows_fn: return fn_rows(args[0]);
    case F::columns_fn: return fn_columns(args[0]);
    case F::address: return fn_address(ctx, args);
    case F::countblank: return fn_countblank(ctx, args[0]);
    case F::sumifs: return conditional_multi(ctx, args, 0, 1);
    case F::countifs: return conditional_multi(ctx, args, static_cast<std::size_t>(-1), 0);
    default: return call_logic_function(ctx, fn, args);
  }
}

}  // namespace slate
