// slate/eval/array_ops.cc
//
// Array-valued functions. SUMPRODUCT/SUMSQ reduce ranges to a scalar; TRANSPOSE
// and MMULT allocate a fresh array in the store and hand it back as an array
// Value (the evaluator records it as a temporary and balances its reference
// count). Reading a range into a dense numeric grid is shared by all of them.
#include "slate/eval/functions.hh"

#include <cmath>
#include <vector>

namespace slate {
namespace {

Value err(wire::ErrCode e) { return Value::error(e); }

// A rectangular block of numbers read out of a range/cell/scalar operand.
struct NumGrid {
  std::uint32_t rows = 0;
  std::uint32_t cols = 0;
  std::vector<double> v;
  double at(std::uint32_t r, std::uint32_t c) const { return v[static_cast<std::size_t>(r) * cols + c]; }
  std::size_t count() const { return v.size(); }
};

NumGrid read_grid(EvalContext& ctx, const Operand& op) {
  NumGrid g;
  if (op.is_range()) {
    const RangeRef& r = op.range;
    g.rows = r.r1 - r.r0 + 1;
    g.cols = r.c1 - r.c0 + 1;
    g.v.reserve(static_cast<std::size_t>(g.rows) * g.cols);
    for (std::uint32_t row = r.r0; row <= r.r1; ++row)
      for (std::uint32_t col = r.c0; col <= r.c1; ++col) {
        Value cv = cell_value(ctx, CellRef{r.sheet, row, col});
        double d; wire::ErrCode e;
        g.v.push_back(coerce_number(ctx, cv, d, e) ? d : 0.0);
      }
  } else {
    Value cv = load_scalar(ctx, op);
    double d; wire::ErrCode e;
    g.rows = g.cols = 1;
    g.v.push_back(coerce_number(ctx, cv, d, e) ? d : 0.0);
  }
  return g;
}

Value make_array(EvalContext& ctx, std::uint32_t rows, std::uint32_t cols,
                 const std::vector<double>& cells) {
  std::uint32_t handle = ctx.arrays.allocate(rows, cols);
  ArrayValue* a = ctx.arrays.get(handle);
  for (std::size_t i = 0; i < cells.size() && i < a->cells.size(); ++i)
    a->cells[i] = Value::number(cells[i]);
  return Value::array(handle);
}

Value fn_sumproduct(EvalContext& ctx, const std::vector<Operand>& args) {
  std::vector<NumGrid> grids;
  grids.reserve(args.size());
  for (const Operand& op : args) grids.push_back(read_grid(ctx, op));
  std::size_t n = grids[0].count();
  for (const NumGrid& g : grids)
    if (g.count() != n) return err(wire::ErrCode::value);
  double acc = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    double prod = 1.0;
    for (const NumGrid& g : grids) prod *= g.v[i];
    acc += prod;
  }
  return Value::number(acc);
}

Value fn_sumsq(EvalContext& ctx, const std::vector<Operand>& args) {
  double acc = 0.0;
  for (const Operand& op : args) {
    NumGrid g = read_grid(ctx, op);
    for (double d : g.v) acc += d * d;
  }
  return Value::number(acc);
}

Value fn_transpose(EvalContext& ctx, const std::vector<Operand>& args) {
  NumGrid g = read_grid(ctx, args[0]);
  std::vector<double> out(g.count(), 0.0);
  for (std::uint32_t r = 0; r < g.rows; ++r)
    for (std::uint32_t c = 0; c < g.cols; ++c)
      out[static_cast<std::size_t>(c) * g.rows + r] = g.at(r, c);
  return make_array(ctx, g.cols, g.rows, out);
}

Value fn_mmult(EvalContext& ctx, const std::vector<Operand>& args) {
  NumGrid a = read_grid(ctx, args[0]);
  NumGrid b = read_grid(ctx, args[1]);
  if (a.cols != b.rows || a.count() == 0 || b.count() == 0)
    return err(wire::ErrCode::value);
  std::uint32_t m = a.rows, n = a.cols, p = b.cols;
  // Guard the result size so a crafted pair of ranges cannot request a huge
  // allocation.
  if (static_cast<std::uint64_t>(m) * p > (1u << 20))
    return err(wire::ErrCode::num);
  std::vector<double> out(static_cast<std::size_t>(m) * p, 0.0);
  for (std::uint32_t i = 0; i < m; ++i)
    for (std::uint32_t j = 0; j < p; ++j) {
      double s = 0.0;
      for (std::uint32_t k = 0; k < n; ++k) s += a.at(i, k) * b.at(k, j);
      out[static_cast<std::size_t>(i) * p + j] = s;
    }
  return make_array(ctx, m, p, out);
}

}  // namespace

Value call_array_function(EvalContext& ctx, wire::Func fn,
                          const std::vector<Operand>& args) {
  switch (fn) {
    case wire::Func::sumproduct: return fn_sumproduct(ctx, args);
    case wire::Func::sumsq: return fn_sumsq(ctx, args);
    case wire::Func::transpose: return fn_transpose(ctx, args);
    case wire::Func::mmult: return fn_mmult(ctx, args);
    default: return call_date_function(ctx, fn, args);
  }
}

}  // namespace slate
