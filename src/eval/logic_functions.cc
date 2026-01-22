// slate/eval/logic_functions.cc
//
// Boolean constants and the branching/lookup logic functions (IFS, SWITCH,
// IFNA) plus two type predicates. These complement IF/AND/OR/NOT in the core
// library. Each returns an error value on a domain problem rather than throwing.
#include "slate/eval/functions.hh"

#include <cmath>

namespace slate {
namespace {

Value err(wire::ErrCode e) { return Value::error(e); }

bool truthy(EvalContext& ctx, const Operand& op, wire::ErrCode& e, bool& out) {
  Value v = load_scalar(ctx, op);
  if (v.is_error()) { e = v.as_error(); return false; }
  double d;
  if (!coerce_number(ctx, v, d, e)) return false;
  out = d != 0.0;
  return true;
}

Value fn_ifs(EvalContext& ctx, const std::vector<Operand>& args) {
  for (std::size_t i = 0; i + 1 < args.size(); i += 2) {
    bool cond; wire::ErrCode e;
    if (!truthy(ctx, args[i], e, cond)) return err(e);
    if (cond) return load_scalar(ctx, args[i + 1]);
  }
  return err(wire::ErrCode::na);
}

Value fn_switch(EvalContext& ctx, const std::vector<Operand>& args) {
  Value subject = load_scalar(ctx, args[0]);
  std::size_t i = 1;
  for (; i + 1 < args.size(); i += 2) {
    Value candidate = load_scalar(ctx, args[i]);
    if (values_equal(ctx, subject, candidate))
      return load_scalar(ctx, args[i + 1]);
  }
  // A trailing lone argument is the default.
  if (i < args.size()) return load_scalar(ctx, args[i]);
  return err(wire::ErrCode::na);
}

Value fn_ifna(EvalContext& ctx, const std::vector<Operand>& args) {
  Value v = load_scalar(ctx, args[0]);
  if (v.is_error() && v.as_error() == wire::ErrCode::na)
    return load_scalar(ctx, args[1]);
  return v;
}

}  // namespace

Value call_logic_function(EvalContext& ctx, wire::Func fn,
                          const std::vector<Operand>& args) {
  using F = wire::Func;
  switch (fn) {
    case F::true_fn: return Value::boolean(true);
    case F::false_fn: return Value::boolean(false);
    case F::ifs: return fn_ifs(ctx, args);
    case F::switch_fn: return fn_switch(ctx, args);
    case F::ifna: return fn_ifna(ctx, args);
    case F::islogical: return Value::boolean(load_scalar(ctx, args[0]).is_bool());
    case F::isnontext: return Value::boolean(!load_scalar(ctx, args[0]).is_text());
    default: return err(wire::ErrCode::name);
  }
}

}  // namespace slate
