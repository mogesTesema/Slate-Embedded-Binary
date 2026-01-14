// slate/eval/operand.hh
//
// A runtime operand on the evaluator's stack. Unlike a plain Value it can also
// stand for an unloaded cell or range reference, which the aggregate functions
// (SUM, COUNT, VLOOKUP, ...) need so they can walk cells without the evaluator
// flattening every range up front. Helpers here load an operand to a single
// value or iterate the scalar values it covers.
#ifndef SLATE_EVAL_OPERAND_HH
#define SLATE_EVAL_OPERAND_HH

#include "slate/eval/value_ops.hh"
#include "slate/model/reference.hh"
#include "slate/model/value.hh"

namespace slate {

struct Operand {
  enum class Kind : std::uint8_t { value, cell, range };
  Kind kind = Kind::value;
  Value val;
  CellRef ref;
  RangeRef range;

  static Operand of_value(const Value& v) { Operand o; o.kind = Kind::value; o.val = v; return o; }
  static Operand of_cell(const CellRef& r) { Operand o; o.kind = Kind::cell; o.ref = r; return o; }
  static Operand of_range(const RangeRef& r) { Operand o; o.kind = Kind::range; o.range = r.normalized(); return o; }

  bool is_value() const { return kind == Kind::value; }
  bool is_cell() const { return kind == Kind::cell; }
  bool is_range() const { return kind == Kind::range; }
};

// Read the current value of a cell (empty if the grid has no such cell).
Value cell_value(EvalContext& ctx, const CellRef& ref);

// Reduce an operand to one value: a cell operand loads its cell, a range operand
// loads its top-left cell (the scalar-context rule).
Value load_scalar(EvalContext& ctx, const Operand& op);

// Invoke `fn(const Value&)` for every scalar the operand covers: the value
// itself, the referenced cell, or each cell of the range (row-major).
template <typename Fn>
void for_each_scalar(EvalContext& ctx, const Operand& op, Fn&& fn) {
  switch (op.kind) {
    case Operand::Kind::value:
      fn(op.val);
      break;
    case Operand::Kind::cell:
      fn(cell_value(ctx, op.ref));
      break;
    case Operand::Kind::range: {
      const RangeRef& r = op.range;
      for (std::uint32_t row = r.r0; row <= r.r1; ++row)
        for (std::uint32_t col = r.c0; col <= r.c1; ++col)
          fn(cell_value(ctx, CellRef{r.sheet, row, col}));
      break;
    }
  }
}

}  // namespace slate

#endif  // SLATE_EVAL_OPERAND_HH
