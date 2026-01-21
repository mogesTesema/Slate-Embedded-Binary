// slate/eval/functions.cc
#include "slate/eval/functions.hh"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

namespace slate {
namespace {

Value err(wire::ErrCode e) { return Value::error(e); }

// Fold every scalar an argument list covers, short-circuiting on the first
// error. `fn` receives each numeric value; non-numeric text/empty is skipped.
template <typename Fn>
bool fold_numbers(EvalContext& ctx, const std::vector<Operand>& args,
                  wire::ErrCode& out_err, Fn&& fn) {
  bool errored = false;
  for (const Operand& op : args) {
    for_each_scalar(ctx, op, [&](const Value& v) {
      if (errored) return;
      if (v.is_error()) { out_err = v.as_error(); errored = true; return; }
      if (v.is_number() || v.is_bool()) fn(v.coerce_number());
      // text and empty are ignored by numeric aggregates
    });
    if (errored) return false;
  }
  return true;
}

Value fn_sum(EvalContext& ctx, const std::vector<Operand>& args) {
  double acc = 0;
  wire::ErrCode e;
  if (!fold_numbers(ctx, args, e, [&](double d) { acc += d; })) return err(e);
  return Value::number(acc);
}

Value fn_avg(EvalContext& ctx, const std::vector<Operand>& args) {
  double acc = 0; long n = 0;
  wire::ErrCode e;
  if (!fold_numbers(ctx, args, e, [&](double d) { acc += d; ++n; })) return err(e);
  if (n == 0) return err(wire::ErrCode::div0);
  return Value::number(acc / static_cast<double>(n));
}

Value fn_count(EvalContext& ctx, const std::vector<Operand>& args) {
  long n = 0;
  wire::ErrCode e;
  fold_numbers(ctx, args, e, [&](double) { ++n; });
  return Value::number(static_cast<double>(n));
}

Value fn_minmax(EvalContext& ctx, const std::vector<Operand>& args, bool want_max) {
  bool seen = false;
  double best = 0;
  wire::ErrCode e;
  if (!fold_numbers(ctx, args, e, [&](double d) {
        if (!seen) { best = d; seen = true; }
        else best = want_max ? std::max(best, d) : std::min(best, d);
      }))
    return err(e);
  return Value::number(seen ? best : 0.0);
}

// One-argument numeric helper.
bool arg_number(EvalContext& ctx, const Operand& op, double& out, wire::ErrCode& e) {
  Value v = load_scalar(ctx, op);
  return coerce_number(ctx, v, out, e);
}

Value fn_unary(EvalContext& ctx, const Operand& op, double (*f)(double)) {
  double x; wire::ErrCode e;
  if (!arg_number(ctx, op, x, e)) return err(e);
  return Value::number(f(x));
}

Value fn_round(EvalContext& ctx, const std::vector<Operand>& args) {
  double x, digits; wire::ErrCode e;
  if (!arg_number(ctx, args[0], x, e)) return err(e);
  if (!arg_number(ctx, args[1], digits, e)) return err(e);
  double scale = std::pow(10.0, std::floor(digits));
  return Value::number(std::round(x * scale) / scale);
}

Value fn_mod(EvalContext& ctx, const std::vector<Operand>& args) {
  double a, b; wire::ErrCode e;
  if (!arg_number(ctx, args[0], a, e)) return err(e);
  if (!arg_number(ctx, args[1], b, e)) return err(e);
  if (b == 0) return err(wire::ErrCode::div0);
  return Value::number(a - b * std::floor(a / b));
}

Value fn_power(EvalContext& ctx, const std::vector<Operand>& args) {
  double a, b; wire::ErrCode e;
  if (!arg_number(ctx, args[0], a, e)) return err(e);
  if (!arg_number(ctx, args[1], b, e)) return err(e);
  return Value::number(std::pow(a, b));
}

bool truthy(EvalContext& ctx, const Operand& op, wire::ErrCode& e, bool& out) {
  double d;
  Value v = load_scalar(ctx, op);
  if (v.is_error()) { e = v.as_error(); return false; }
  if (!coerce_number(ctx, v, d, e)) return false;
  out = d != 0.0;
  return true;
}

Value fn_if(EvalContext& ctx, const std::vector<Operand>& args) {
  bool cond; wire::ErrCode e;
  if (!truthy(ctx, args[0], e, cond)) return err(e);
  return load_scalar(ctx, cond ? args[1] : args[2]);
}

Value fn_and_or(EvalContext& ctx, const std::vector<Operand>& args, bool is_and) {
  bool acc = is_and;
  for (const Operand& op : args) {
    bool b; wire::ErrCode e;
    if (!truthy(ctx, op, e, b)) return err(e);
    acc = is_and ? (acc && b) : (acc || b);
  }
  return Value::boolean(acc);
}

Value fn_not(EvalContext& ctx, const std::vector<Operand>& args) {
  bool b; wire::ErrCode e;
  if (!truthy(ctx, args[0], e, b)) return err(e);
  return Value::boolean(!b);
}

std::string text_arg(EvalContext& ctx, const Operand& op) {
  return coerce_text(ctx, load_scalar(ctx, op));
}

Value make_text(EvalContext& ctx, const std::string& s) {
  return Value::text(ctx.strings.intern_dynamic(s));
}

Value fn_len(EvalContext& ctx, const std::vector<Operand>& args) {
  return Value::number(static_cast<double>(text_arg(ctx, args[0]).size()));
}

Value fn_left(EvalContext& ctx, const std::vector<Operand>& args) {
  std::string s = text_arg(ctx, args[0]);
  double n; wire::ErrCode e;
  if (!arg_number(ctx, args[1], n, e)) return err(e);
  std::size_t k = n < 0 ? 0 : std::min<std::size_t>(s.size(), static_cast<std::size_t>(n));
  return make_text(ctx, s.substr(0, k));
}

Value fn_right(EvalContext& ctx, const std::vector<Operand>& args) {
  std::string s = text_arg(ctx, args[0]);
  double n; wire::ErrCode e;
  if (!arg_number(ctx, args[1], n, e)) return err(e);
  std::size_t k = n < 0 ? 0 : std::min<std::size_t>(s.size(), static_cast<std::size_t>(n));
  return make_text(ctx, s.substr(s.size() - k));
}

Value fn_mid(EvalContext& ctx, const std::vector<Operand>& args) {
  std::string s = text_arg(ctx, args[0]);
  double start, len; wire::ErrCode e;
  if (!arg_number(ctx, args[1], start, e)) return err(e);
  if (!arg_number(ctx, args[2], len, e)) return err(e);
  if (start < 1) return err(wire::ErrCode::value);
  std::size_t begin = static_cast<std::size_t>(start) - 1;
  if (begin >= s.size()) return make_text(ctx, "");
  std::size_t k = len < 0 ? 0 : std::min<std::size_t>(s.size() - begin, static_cast<std::size_t>(len));
  return make_text(ctx, s.substr(begin, k));
}

Value fn_case(EvalContext& ctx, const std::vector<Operand>& args, bool upper) {
  std::string s = text_arg(ctx, args[0]);
  for (char& ch : s)
    ch = upper ? static_cast<char>(std::toupper(static_cast<unsigned char>(ch)))
               : static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  return make_text(ctx, s);
}

Value fn_concat(EvalContext& ctx, const std::vector<Operand>& args) {
  std::string out;
  for (const Operand& op : args) {
    bool errored = false;
    wire::ErrCode ec = wire::ErrCode::value;
    for_each_scalar(ctx, op, [&](const Value& v) {
      if (errored) return;
      if (v.is_error()) { errored = true; ec = v.as_error(); return; }
      out += coerce_text(ctx, v);
    });
    if (errored) return err(ec);
  }
  return make_text(ctx, out);
}

Value fn_vlookup(EvalContext& ctx, const std::vector<Operand>& args) {
  // VLOOKUP(key, range, col_index): scan the first column of the range for key,
  // return the value in column col_index (1-based) of the matching row.
  if (!args[1].is_range()) return err(wire::ErrCode::ref);
  Value key = load_scalar(ctx, args[0]);
  double coli; wire::ErrCode e;
  if (!arg_number(ctx, args[2], coli, e)) return err(e);
  const RangeRef& r = args[1].range;
  std::uint32_t width = r.c1 - r.c0 + 1;
  if (coli < 1 || static_cast<std::uint32_t>(coli) > width)
    return err(wire::ErrCode::ref);
  std::uint32_t target_col = r.c0 + static_cast<std::uint32_t>(coli) - 1;
  for (std::uint32_t row = r.r0; row <= r.r1; ++row) {
    Value probe = cell_value(ctx, CellRef{r.sheet, row, r.c0});
    if (values_equal(ctx, probe, key))
      return cell_value(ctx, CellRef{r.sheet, row, target_col});
  }
  return err(wire::ErrCode::na);
}

Value fn_index(EvalContext& ctx, const std::vector<Operand>& args) {
  if (!args[0].is_range()) return err(wire::ErrCode::ref);
  double rr, cc; wire::ErrCode e;
  if (!arg_number(ctx, args[1], rr, e)) return err(e);
  if (!arg_number(ctx, args[2], cc, e)) return err(e);
  const RangeRef& r = args[0].range;
  if (rr < 1 || cc < 1) return err(wire::ErrCode::ref);
  std::uint32_t row = r.r0 + static_cast<std::uint32_t>(rr) - 1;
  std::uint32_t col = r.c0 + static_cast<std::uint32_t>(cc) - 1;
  if (row > r.r1 || col > r.c1) return err(wire::ErrCode::ref);
  return cell_value(ctx, CellRef{r.sheet, row, col});
}

Value fn_match(EvalContext& ctx, const std::vector<Operand>& args) {
  Value key = load_scalar(ctx, args[0]);
  if (!args[1].is_range()) return err(wire::ErrCode::ref);
  const RangeRef& r = args[1].range;
  std::uint32_t pos = 1;
  for (std::uint32_t row = r.r0; row <= r.r1; ++row)
    for (std::uint32_t col = r.c0; col <= r.c1; ++col, ++pos) {
      if (values_equal(ctx, cell_value(ctx, CellRef{r.sheet, row, col}), key))
        return Value::number(static_cast<double>(pos));
    }
  return err(wire::ErrCode::na);
}

}  // namespace

Value call_function(EvalContext& ctx, wire::Func fn,
                    const std::vector<Operand>& args) {
  using F = wire::Func;
  switch (fn) {
    case F::sum:   return fn_sum(ctx, args);
    case F::avg:   return fn_avg(ctx, args);
    case F::count: return fn_count(ctx, args);
    case F::min:   return fn_minmax(ctx, args, false);
    case F::max:   return fn_minmax(ctx, args, true);
    case F::abs_:  return fn_unary(ctx, args[0], [](double d) { return std::fabs(d); });
    case F::sqrt_: return fn_unary(ctx, args[0], [](double d) { return std::sqrt(d); });
    case F::round_: return fn_round(ctx, args);
    case F::mod_:  return fn_mod(ctx, args);
    case F::power: return fn_power(ctx, args);
    case F::if_:   return fn_if(ctx, args);
    case F::and_:  return fn_and_or(ctx, args, true);
    case F::or_:   return fn_and_or(ctx, args, false);
    case F::not_:  return fn_not(ctx, args);
    case F::len_:  return fn_len(ctx, args);
    case F::left_: return fn_left(ctx, args);
    case F::right_: return fn_right(ctx, args);
    case F::mid_:  return fn_mid(ctx, args);
    case F::upper_: return fn_case(ctx, args, true);
    case F::lower_: return fn_case(ctx, args, false);
    case F::concat_fn: return fn_concat(ctx, args);
    case F::vlookup: return fn_vlookup(ctx, args);
    case F::index_: return fn_index(ctx, args);
    case F::match_: return fn_match(ctx, args);
    default: return call_function_ext(ctx, fn, args);
  }
}

}  // namespace slate
