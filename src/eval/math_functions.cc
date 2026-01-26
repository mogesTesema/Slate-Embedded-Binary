// slate/eval/math_functions.cc
//
// Scalar math beyond the core set: exponential/logarithmic, trigonometric,
// angle conversion, directional rounding, and combinatorics. Every function
// coerces its arguments through the shared numeric rules and returns an error
// value (never throws) on a domain problem such as log of a non-positive number.
#include "slate/eval/functions.hh"

#include <cmath>

namespace slate {
namespace {

Value err(wire::ErrCode e) { return Value::error(e); }

bool one(EvalContext& ctx, const Operand& op, double& out, wire::ErrCode& e) {
  return coerce_number(ctx, load_scalar(ctx, op), out, e);
}

Value unary(EvalContext& ctx, const Operand& op, double (*f)(double)) {
  double x; wire::ErrCode e;
  if (!one(ctx, op, x, e)) return err(e);
  return Value::number(f(x));
}

Value fn_log(EvalContext& ctx, const std::vector<Operand>& args) {
  double x; wire::ErrCode e;
  if (!one(ctx, args[0], x, e)) return err(e);
  if (x <= 0) return err(wire::ErrCode::num);
  double base = 10.0;
  if (args.size() >= 2) {
    if (!one(ctx, args[1], base, e)) return err(e);
    if (base <= 0 || base == 1) return err(wire::ErrCode::num);
  }
  return Value::number(std::log(x) / std::log(base));
}

Value fn_atan2(EvalContext& ctx, const std::vector<Operand>& args) {
  double x, y; wire::ErrCode e;
  if (!one(ctx, args[0], x, e)) return err(e);
  if (!one(ctx, args[1], y, e)) return err(e);
  return Value::number(std::atan2(y, x));  // spreadsheet order: ATAN2(x, y)
}

Value fn_directional_round(EvalContext& ctx, const std::vector<Operand>& args, bool up) {
  double x, digits; wire::ErrCode e;
  if (!one(ctx, args[0], x, e)) return err(e);
  if (!one(ctx, args[1], digits, e)) return err(e);
  double scale = std::pow(10.0, std::floor(digits));
  double scaled = x * scale;
  double r = up ? (x >= 0 ? std::ceil(scaled) : std::floor(scaled))
                : (x >= 0 ? std::floor(scaled) : std::ceil(scaled));
  return Value::number(r / scale);
}

Value fn_mround(EvalContext& ctx, const std::vector<Operand>& args) {
  double x, m; wire::ErrCode e;
  if (!one(ctx, args[0], x, e)) return err(e);
  if (!one(ctx, args[1], m, e)) return err(e);
  if (m == 0) return Value::number(0.0);
  return Value::number(std::round(x / m) * m);
}

Value fn_fact(EvalContext& ctx, const std::vector<Operand>& args) {
  double xd; wire::ErrCode e;
  if (!one(ctx, args[0], xd, e)) return err(e);
  if (xd < 0) return err(wire::ErrCode::num);
  long n = static_cast<long>(std::floor(xd));
  if (n > 170) return err(wire::ErrCode::num);  // beyond double range
  double acc = 1.0;
  for (long i = 2; i <= n; ++i) acc *= static_cast<double>(i);
  return Value::number(acc);
}

Value fn_combin(EvalContext& ctx, const std::vector<Operand>& args, bool permute) {
  double nd, kd; wire::ErrCode e;
  if (!one(ctx, args[0], nd, e)) return err(e);
  if (!one(ctx, args[1], kd, e)) return err(e);
  long n = static_cast<long>(std::floor(nd));
  long k = static_cast<long>(std::floor(kd));
  if (n < 0 || k < 0 || k > n) return err(wire::ErrCode::num);
  // Iterative product avoids overflow better than factorials.
  double acc = 1.0;
  for (long i = 0; i < k; ++i) {
    acc *= static_cast<double>(n - i);
    if (!permute) acc /= static_cast<double>(i + 1);
  }
  return Value::number(permute ? acc : std::round(acc));
}

Value fn_quotient(EvalContext& ctx, const std::vector<Operand>& args) {
  double a, b; wire::ErrCode e;
  if (!one(ctx, args[0], a, e)) return err(e);
  if (!one(ctx, args[1], b, e)) return err(e);
  if (b == 0) return err(wire::ErrCode::div0);
  return Value::number(std::trunc(a / b));
}

}  // namespace

Value call_math_function(EvalContext& ctx, wire::Func fn,
                         const std::vector<Operand>& args) {
  using F = wire::Func;
  switch (fn) {
    case F::exp_: return unary(ctx, args[0], [](double d) { return std::exp(d); });
    case F::ln_: return unary(ctx, args[0], [](double d) { return std::log(d); });
    case F::log10_: return unary(ctx, args[0], [](double d) { return std::log10(d); });
    case F::log_: return fn_log(ctx, args);
    case F::pi_: return Value::number(3.14159265358979323846);
    case F::sin_: return unary(ctx, args[0], [](double d) { return std::sin(d); });
    case F::cos_: return unary(ctx, args[0], [](double d) { return std::cos(d); });
    case F::tan_: return unary(ctx, args[0], [](double d) { return std::tan(d); });
    case F::asin_: return unary(ctx, args[0], [](double d) { return std::asin(d); });
    case F::acos_: return unary(ctx, args[0], [](double d) { return std::acos(d); });
    case F::atan_: return unary(ctx, args[0], [](double d) { return std::atan(d); });
    case F::atan2_: return fn_atan2(ctx, args);
    case F::degrees_: return unary(ctx, args[0], [](double d) { return d * 180.0 / 3.14159265358979323846; });
    case F::radians_: return unary(ctx, args[0], [](double d) { return d * 3.14159265358979323846 / 180.0; });
    case F::roundup: return fn_directional_round(ctx, args, true);
    case F::rounddown: return fn_directional_round(ctx, args, false);
    case F::mround: return fn_mround(ctx, args);
    case F::fact: return fn_fact(ctx, args);
    case F::combin: return fn_combin(ctx, args, false);
    case F::permut: return fn_combin(ctx, args, true);
    case F::quotient: return fn_quotient(ctx, args);
    default: return call_financial_function(ctx, fn, args);
  }
}

}  // namespace slate
