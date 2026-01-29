// slate/eval/financial_functions.cc
//
// Time-value-of-money functions (PMT, FV, PV, NPER) and discounted cash flow
// (NPV), following the standard annuity formulas with an end/begin `type` flag.
// The zero-rate cases are handled separately to avoid division by the rate.
#include "slate/eval/functions.hh"

#include <cmath>

namespace slate {
namespace {

Value err(wire::ErrCode e) { return Value::error(e); }

double arg(EvalContext& ctx, const std::vector<Operand>& a, std::size_t i,
           double dflt, bool& ok_out) {
  if (i >= a.size()) return dflt;
  double d; wire::ErrCode e;
  ok_out = coerce_number(ctx, load_scalar(ctx, a[i]), d, e) && ok_out;
  return d;
}

Value fn_pmt(EvalContext& ctx, const std::vector<Operand>& a) {
  bool ok = true;
  double rate = arg(ctx, a, 0, 0, ok), nper = arg(ctx, a, 1, 0, ok),
         pv = arg(ctx, a, 2, 0, ok), fv = arg(ctx, a, 3, 0, ok),
         type = arg(ctx, a, 4, 0, ok);
  if (!ok) return err(wire::ErrCode::value);
  if (nper == 0) return err(wire::ErrCode::num);
  if (rate == 0) return Value::number(-(pv + fv) / nper);
  double factor = std::pow(1 + rate, nper);
  double pmt = -(pv * factor + fv) * rate /
               ((1 + rate * type) * (factor - 1));
  return Value::number(pmt);
}

Value fn_fv(EvalContext& ctx, const std::vector<Operand>& a) {
  bool ok = true;
  double rate = arg(ctx, a, 0, 0, ok), nper = arg(ctx, a, 1, 0, ok),
         pmt = arg(ctx, a, 2, 0, ok), pv = arg(ctx, a, 3, 0, ok),
         type = arg(ctx, a, 4, 0, ok);
  if (!ok) return err(wire::ErrCode::value);
  if (rate == 0) return Value::number(-(pv + pmt * nper));
  double factor = std::pow(1 + rate, nper);
  double fv = -(pv * factor + pmt * (1 + rate * type) * (factor - 1) / rate);
  return Value::number(fv);
}

Value fn_pv(EvalContext& ctx, const std::vector<Operand>& a) {
  bool ok = true;
  double rate = arg(ctx, a, 0, 0, ok), nper = arg(ctx, a, 1, 0, ok),
         pmt = arg(ctx, a, 2, 0, ok), fv = arg(ctx, a, 3, 0, ok),
         type = arg(ctx, a, 4, 0, ok);
  if (!ok) return err(wire::ErrCode::value);
  if (rate == 0) return Value::number(-(fv + pmt * nper));
  double factor = std::pow(1 + rate, nper);
  double pv = -(fv + pmt * (1 + rate * type) * (factor - 1) / rate) / factor;
  return Value::number(pv);
}

Value fn_nper(EvalContext& ctx, const std::vector<Operand>& a) {
  bool ok = true;
  double rate = arg(ctx, a, 0, 0, ok), pmt = arg(ctx, a, 1, 0, ok),
         pv = arg(ctx, a, 2, 0, ok), fv = arg(ctx, a, 3, 0, ok),
         type = arg(ctx, a, 4, 0, ok);
  if (!ok) return err(wire::ErrCode::value);
  if (rate == 0) {
    if (pmt == 0) return err(wire::ErrCode::num);
    return Value::number(-(pv + fv) / pmt);
  }
  double adj = pmt * (1 + rate * type);
  double num = adj - fv * rate;
  double den = adj + pv * rate;
  if (num <= 0 || den <= 0) return err(wire::ErrCode::num);
  return Value::number(std::log(num / den) / std::log(1 + rate));
}

Value fn_npv(EvalContext& ctx, const std::vector<Operand>& a) {
  bool ok = true;
  double rate = arg(ctx, a, 0, 0, ok);
  if (!ok) return err(wire::ErrCode::value);
  double acc = 0.0;
  double disc = 1.0;
  for (std::size_t i = 1; i < a.size(); ++i) {
    disc *= (1 + rate);
    double d; wire::ErrCode e;
    if (!coerce_number(ctx, load_scalar(ctx, a[i]), d, e)) return err(e);
    acc += d / disc;
  }
  return Value::number(acc);
}

}  // namespace

Value call_financial_function(EvalContext& ctx, wire::Func fn,
                              const std::vector<Operand>& args) {
  switch (fn) {
    case wire::Func::pmt: return fn_pmt(ctx, args);
    case wire::Func::fv: return fn_fv(ctx, args);
    case wire::Func::pv: return fn_pv(ctx, args);
    case wire::Func::nper: return fn_nper(ctx, args);
    case wire::Func::npv: return fn_npv(ctx, args);
    default: return call_stats_text_function(ctx, fn, args);
  }
}

}  // namespace slate
