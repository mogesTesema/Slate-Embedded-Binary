// slate/eval/evaluator.cc
#include "slate/eval/evaluator.hh"

#include "slate/eval/functions.hh"
#include "slate/graph/defined_names.hh"

namespace slate {
namespace {

Value binop(EvalContext& ctx, wire::Op op, const Value& a, const Value& b) {
  double x, y;
  wire::ErrCode e;
  if (!coerce_number(ctx, a, x, e)) return Value::error(e);
  if (!coerce_number(ctx, b, y, e)) return Value::error(e);
  switch (op) {
    case wire::Op::add: return Value::number(x + y);
    case wire::Op::sub: return Value::number(x - y);
    case wire::Op::mul: return Value::number(x * y);
    case wire::Op::divide:
      if (y == 0.0) return Value::error(wire::ErrCode::div0);
      return Value::number(x / y);
    default: return Value::error(wire::ErrCode::value);
  }
}

}  // namespace

Value Evaluator::materialize_range(const RangeRef& r) {
  std::uint32_t rows = r.r1 - r.r0 + 1;
  std::uint32_t cols = r.c1 - r.c0 + 1;
  std::uint32_t handle = ctx_.arrays.allocate(rows, cols);
  array_temps_.push_back(handle);
  ArrayValue* a = ctx_.arrays.get(handle);
  for (std::uint32_t rr = 0; rr < rows; ++rr) {
    for (std::uint32_t cc = 0; cc < cols; ++cc) {
      Value v = cell_value(ctx_, CellRef{r.sheet, r.r0 + rr, r.c0 + cc});
      // Arrays are numeric-only: coerce, preserving errors, dropping text to 0.
      double d;
      wire::ErrCode e;
      Value stored = v.is_error() ? v
                     : coerce_number(ctx_, v, d, e) ? Value::number(d)
                                                    : Value::error(e);
      a->cells[static_cast<std::size_t>(rr) * cols + cc] = stored;
    }
  }
  return Value::array(handle);
}

Value Evaluator::finalize(const Operand& top) {
  switch (top.kind) {
    case Operand::Kind::value:
      return top.val;
    case Operand::Kind::cell:
      return cell_value(ctx_, top.ref);
    case Operand::Kind::range:
      return materialize_range(top.range);
  }
  return Value();
}

Value Evaluator::run(const Program& p, DiagnosticSink& diag) {
  stack_.clear();
  text_temps_.clear();
  array_temps_.clear();

  Value result;
  bool faulted = false;

  auto record_temp = [&](const Value& v) {
    if (v.is_text() && !ctx_.strings.is_static(v.as_text()))
      text_temps_.push_back(v.as_text());
    else if (v.is_array())
      array_temps_.push_back(v.as_array());
  };

  for (const Instr& in : p.code) {
    switch (in.op) {
      case wire::Op::end:
        break;
      case wire::Op::push_num:
        stack_.push_back(Operand::of_value(Value::number(in.num)));
        break;
      case wire::Op::push_str:
        stack_.push_back(Operand::of_value(Value::text(in.a)));
        break;
      case wire::Op::push_bool:
        stack_.push_back(Operand::of_value(Value::boolean(in.a != 0)));
        break;
      case wire::Op::push_err:
        stack_.push_back(Operand::of_value(
            Value::error(static_cast<wire::ErrCode>(in.a))));
        break;
      case wire::Op::push_ref:
        stack_.push_back(Operand::of_cell(
            CellRef{static_cast<SheetId>(in.a), in.b, in.c}));
        break;
      case wire::Op::push_range:
        stack_.push_back(Operand::of_range(
            RangeRef{static_cast<SheetId>(in.a), in.b, in.c, in.d, in.e}));
        break;
      case wire::Op::push_name: {
        if (in.a >= wb_.names.size()) { faulted = true; break; }
        const RawName& n = wb_.names[in.a];
        if (n.kind == wire::NameKind::cell) {
          std::uint32_t rr = n.r0, cc = n.c0;
          // Clamp a single-cell name to its backing sheet's bound-time extent,
          // resolved once when the program was bound rather than re-walked on
          // every evaluation of this cell.
          if (in.a < p.name_sheet_cache.size()) {
            if (const SheetData* sd = p.name_sheet_cache[in.a]) {
              if (rr >= sd->rows) rr = sd->rows ? sd->rows - 1 : 0;
              if (cc >= sd->cols) cc = sd->cols ? sd->cols - 1 : 0;
            }
          }
          stack_.push_back(Operand::of_cell(CellRef{n.sheet, rr, cc}));
          break;
        }
        std::uint32_t r1 = n.r1, c1 = n.c1;
        // Clamp the named range to its backing sheet's current extent so a name
        // authored against a wider sheet cannot address cells past the grid.
        if (names_) {
          const SheetData* sd = names_->backing(in.a);
          if (sd) {
            if (r1 >= sd->rows) r1 = sd->rows ? sd->rows - 1 : 0;
            if (c1 >= sd->cols) c1 = sd->cols ? sd->cols - 1 : 0;
          }
        }
        stack_.push_back(Operand::of_range(
            RangeRef{n.sheet, n.r0, n.c0, r1, c1}));
        break;
      }
      case wire::Op::load: {
        if (stack_.empty()) { faulted = true; break; }
        Operand op = stack_.back();
        stack_.pop_back();
        if (op.is_cell()) {
          stack_.push_back(Operand::of_value(cell_value(ctx_, op.ref)));
        } else if (op.is_range()) {
          Value arr = materialize_range(op.range);
          stack_.push_back(Operand::of_value(arr));
        } else {
          stack_.push_back(op);  // already a value
        }
        break;
      }
      case wire::Op::add:
      case wire::Op::sub:
      case wire::Op::mul:
      case wire::Op::divide: {
        if (stack_.size() < 2) { faulted = true; break; }
        Value b = load_scalar(ctx_, stack_.back()); stack_.pop_back();
        Value a = load_scalar(ctx_, stack_.back()); stack_.pop_back();
        stack_.push_back(Operand::of_value(binop(ctx_, in.op, a, b)));
        break;
      }
      case wire::Op::neg: {
        if (stack_.empty()) { faulted = true; break; }
        Value a = load_scalar(ctx_, stack_.back()); stack_.pop_back();
        double x; wire::ErrCode e;
        Value r = coerce_number(ctx_, a, x, e) ? Value::number(-x)
                                               : Value::error(e);
        stack_.push_back(Operand::of_value(r));
        break;
      }
      case wire::Op::concat: {
        if (stack_.size() < 2) { faulted = true; break; }
        Value b = load_scalar(ctx_, stack_.back()); stack_.pop_back();
        Value a = load_scalar(ctx_, stack_.back()); stack_.pop_back();
        std::string s = coerce_text(ctx_, a) + coerce_text(ctx_, b);
        Value r = Value::text(ctx_.strings.intern_dynamic(s));
        record_temp(r);
        stack_.push_back(Operand::of_value(r));
        break;
      }
      case wire::Op::eq:
      case wire::Op::lt:
      case wire::Op::gt:
      case wire::Op::le:
      case wire::Op::ge:
      case wire::Op::ne: {
        if (stack_.size() < 2) { faulted = true; break; }
        Value b = load_scalar(ctx_, stack_.back()); stack_.pop_back();
        Value a = load_scalar(ctx_, stack_.back()); stack_.pop_back();
        int cmp = values_compare(ctx_, a, b);
        bool res = false;
        switch (in.op) {
          case wire::Op::eq: res = values_equal(ctx_, a, b); break;
          case wire::Op::ne: res = !values_equal(ctx_, a, b); break;
          case wire::Op::lt: res = cmp < 0; break;
          case wire::Op::gt: res = cmp > 0; break;
          case wire::Op::le: res = cmp <= 0; break;
          case wire::Op::ge: res = cmp >= 0; break;
          default: break;
        }
        stack_.push_back(Operand::of_value(Value::boolean(res)));
        break;
      }
      case wire::Op::call: {
        if (stack_.size() < in.argc) { faulted = true; break; }
        std::vector<Operand> args(stack_.end() - in.argc, stack_.end());
        stack_.erase(stack_.end() - in.argc, stack_.end());
        Value r = call_function(ctx_, static_cast<wire::Func>(in.func), args);
        record_temp(r);
        stack_.push_back(Operand::of_value(r));
        break;
      }
      case wire::Op::make_array: {
        std::uint64_t count = static_cast<std::uint64_t>(in.b) * in.c;
        if (stack_.size() < count) { faulted = true; break; }
        std::uint32_t handle = ctx_.arrays.allocate(in.b, in.c);
        array_temps_.push_back(handle);
        ArrayValue* a = ctx_.arrays.get(handle);
        for (std::uint64_t i = 0; i < count; ++i) {
          Value v = load_scalar(ctx_, stack_[stack_.size() - count + i]);
          double d; wire::ErrCode e;
          a->cells[i] = v.is_error() ? v
                        : coerce_number(ctx_, v, d, e) ? Value::number(d)
                                                       : Value::error(e);
        }
        stack_.erase(stack_.end() - count, stack_.end());
        stack_.push_back(Operand::of_value(Value::array(handle)));
        break;
      }
    }
    if (faulted) break;
  }

  if (faulted || stack_.size() != 1) {
    result = Value::error(wire::ErrCode::value);
  } else {
    result = finalize(stack_.back());
  }

  // Claim one reference for the returned value, then release every temp so only
  // the result's storage survives this evaluation.
  retain_handles(ctx_, result);
  for (std::uint32_t h : text_temps_) ctx_.strings.release(h);
  for (std::uint32_t h : array_temps_) ctx_.arrays.release(h);
  (void)diag;
  return result;
}

}  // namespace slate
