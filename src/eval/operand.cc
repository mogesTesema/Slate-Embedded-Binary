// slate/eval/operand.cc
#include "slate/eval/operand.hh"

namespace slate {

Value cell_value(EvalContext& ctx, const CellRef& ref) {
  // A read of a merged region resolves through the region's anchor cell.
  CellRef eff = ctx.wb.resolve_merge(ref);
  CellId id = ctx.wb.grid.find(eff);
  if (id == kNoCell) return Value();
  return ctx.wb.grid.at(id).value;
}

Value load_scalar(EvalContext& ctx, const Operand& op) {
  switch (op.kind) {
    case Operand::Kind::value:
      return op.val;
    case Operand::Kind::cell:
      return cell_value(ctx, op.ref);
    case Operand::Kind::range:
      return cell_value(ctx, CellRef{op.range.sheet, op.range.r0, op.range.c0});
  }
  return Value();
}

}  // namespace slate
