// slate/eval/functions_ext.cc
//
// The extended function library (ids 60+): conditional aggregates, statistics,
// integer math, richer text functions, HLOOKUP/CHOOSE, and type predicates.
// Split from functions.cc purely to keep each translation unit readable; the
// contract is identical — evaluated operands in, a Value (possibly an error
// value) out, dynamic text interned.
#include "slate/eval/functions.hh"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include "criteria.hh"
#include "slate/model/number_format.hh"

namespace slate {
namespace {

Value err(wire::ErrCode e) { return Value::error(e); }

Value make_text(EvalContext& ctx, const std::string& s) {
  return Value::text(ctx.strings.intern_dynamic(s));
}

// Gather every numeric scalar an argument list covers. Returns false and sets
// `ec` on the first error value encountered.
bool collect_numbers(EvalContext& ctx, const std::vector<Operand>& args,
                     std::size_t first, std::vector<double>& out,
                     wire::ErrCode& ec) {
  bool errored = false;
  for (std::size_t i = first; i < args.size(); ++i) {
    for_each_scalar(ctx, args[i], [&](const Value& v) {
      if (errored) return;
      if (v.is_error()) { ec = v.as_error(); errored = true; return; }
      if (v.is_number() || v.is_bool()) out.push_back(v.coerce_number());
    });
    if (errored) return false;
  }
  return true;
}

double scalar_number(EvalContext& ctx, const Operand& op, bool& ok_out,
                     wire::ErrCode& ec) {
  double d;
  Value v = load_scalar(ctx, op);
  ok_out = coerce_number(ctx, v, d, ec);
  return ok_out ? d : 0.0;
}

// Criteria matching for the conditional aggregates lives in criteria.hh so the
// multi-criteria SUMIFS/COUNTIFS share exactly the same rules.
using detail::Criteria;
using detail::criteria_match;
using detail::parse_criteria;
using detail::range_cells;

// --- implementations --------------------------------------------------------

Value fn_product(EvalContext& ctx, const std::vector<Operand>& args) {
  std::vector<double> nums; wire::ErrCode e;
  if (!collect_numbers(ctx, args, 0, nums, e)) return err(e);
  double acc = 1.0;
  for (double d : nums) acc *= d;
  return Value::number(nums.empty() ? 0.0 : acc);
}

Value fn_cond_agg(EvalContext& ctx, const std::vector<Operand>& args, int mode) {
  // mode 0 = SUMIF, 1 = COUNTIF, 2 = AVERAGEIF
  std::vector<CellRef> range;
  range_cells(args[0], range);
  Criteria crit = parse_criteria(ctx, args[1]);
  const Operand* value_src = &args[0];
  if (mode != 1 && args.size() >= 3) value_src = &args[2];
  std::vector<CellRef> values;
  range_cells(*value_src, values);

  double acc = 0.0;
  long n = 0;
  for (std::size_t i = 0; i < range.size(); ++i) {
    Value probe = cell_value(ctx, range[i]);
    if (!criteria_match(ctx, crit, probe)) continue;
    if (mode == 1) { ++n; continue; }
    CellRef vr = i < values.size() ? values[i] : range[i];
    Value v = cell_value(ctx, vr);
    double d; wire::ErrCode e;
    if (coerce_number(ctx, v, d, e)) { acc += d; ++n; }
  }
  if (mode == 1) return Value::number(static_cast<double>(n));
  if (mode == 2) return n ? Value::number(acc / n) : err(wire::ErrCode::div0);
  return Value::number(acc);
}

Value fn_unary_math(EvalContext& ctx, const Operand& op, int which) {
  bool okk; wire::ErrCode e;
  double x = scalar_number(ctx, op, okk, e);
  if (!okk) return err(e);
  switch (which) {
    case 0: return Value::number(std::floor(x));                 // INT
    case 1: return Value::number(std::trunc(x));                 // TRUNC
    case 2: return Value::number((x > 0) - (x < 0));             // SIGN
  }
  return err(wire::ErrCode::value);
}

Value fn_step(EvalContext& ctx, const std::vector<Operand>& args, bool up) {
  bool a, b; wire::ErrCode e1, e2;
  double x = scalar_number(ctx, args[0], a, e1);
  double sig = scalar_number(ctx, args[1], b, e2);
  if (!a) return err(e1);
  if (!b) return err(e2);
  if (sig == 0.0) return Value::number(0.0);
  double q = x / sig;
  return Value::number(sig * (up ? std::ceil(q) : std::floor(q)));
}

long igcd(long a, long b) {
  a = std::labs(a); b = std::labs(b);
  while (b) { long t = a % b; a = b; b = t; }
  return a;
}

Value fn_gcd_lcm(EvalContext& ctx, const std::vector<Operand>& args, bool is_gcd) {
  std::vector<double> nums; wire::ErrCode e;
  if (!collect_numbers(ctx, args, 0, nums, e)) return err(e);
  if (nums.empty()) return Value::number(0.0);
  long acc = static_cast<long>(std::floor(std::fabs(nums[0])));
  for (std::size_t i = 1; i < nums.size(); ++i) {
    long v = static_cast<long>(std::floor(std::fabs(nums[i])));
    if (is_gcd) acc = igcd(acc, v);
    else {
      long g = igcd(acc, v);
      acc = g ? (acc / g) * v : 0;
    }
  }
  return Value::number(static_cast<double>(acc));
}

Value fn_median(EvalContext& ctx, const std::vector<Operand>& args) {
  std::vector<double> nums; wire::ErrCode e;
  if (!collect_numbers(ctx, args, 0, nums, e)) return err(e);
  if (nums.empty()) return err(wire::ErrCode::num);
  std::sort(nums.begin(), nums.end());
  std::size_t m = nums.size() / 2;
  double med = (nums.size() % 2) ? nums[m] : (nums[m - 1] + nums[m]) / 2.0;
  return Value::number(med);
}

Value fn_var_stdev(EvalContext& ctx, const std::vector<Operand>& args, bool stdev) {
  std::vector<double> nums; wire::ErrCode e;
  if (!collect_numbers(ctx, args, 0, nums, e)) return err(e);
  if (nums.size() < 2) return err(wire::ErrCode::div0);
  double mean = 0;
  for (double d : nums) mean += d;
  mean /= nums.size();
  double ss = 0;
  for (double d : nums) ss += (d - mean) * (d - mean);
  double var = ss / (nums.size() - 1);  // sample
  return Value::number(stdev ? std::sqrt(var) : var);
}

Value fn_rank(EvalContext& ctx, const std::vector<Operand>& args) {
  bool okk; wire::ErrCode e;
  double key = scalar_number(ctx, args[0], okk, e);
  if (!okk) return err(e);
  std::vector<double> nums;
  if (!collect_numbers(ctx, {args[1]}, 0, nums, e)) return err(e);
  bool ascending = false;
  if (args.size() >= 3) {
    bool o; wire::ErrCode oe;
    ascending = scalar_number(ctx, args[2], o, oe) != 0.0;
  }
  long rank = 1;
  for (double d : nums)
    if (ascending ? (d < key) : (d > key)) ++rank;
  return Value::number(static_cast<double>(rank));
}

std::string text_of(EvalContext& ctx, const Operand& op) {
  return coerce_text(ctx, load_scalar(ctx, op));
}

Value fn_find(EvalContext& ctx, const std::vector<Operand>& args) {
  std::string needle = text_of(ctx, args[0]);
  std::string hay = text_of(ctx, args[1]);
  std::size_t start = 0;
  if (args.size() >= 3) {
    bool o; wire::ErrCode e;
    double s = scalar_number(ctx, args[2], o, e);
    if (!o) return err(e);
    if (s < 1) return err(wire::ErrCode::value);
    start = static_cast<std::size_t>(s) - 1;
  }
  if (start > hay.size()) return err(wire::ErrCode::value);
  std::size_t pos = hay.find(needle, start);
  if (pos == std::string::npos) return err(wire::ErrCode::value);
  return Value::number(static_cast<double>(pos + 1));
}

Value fn_substitute(EvalContext& ctx, const std::vector<Operand>& args) {
  std::string s = text_of(ctx, args[0]);
  std::string from = text_of(ctx, args[1]);
  std::string to = text_of(ctx, args[2]);
  long which = 0;  // 0 = all
  if (args.size() >= 4) {
    bool o; wire::ErrCode e;
    which = static_cast<long>(scalar_number(ctx, args[3], o, e));
  }
  if (from.empty()) return make_text(ctx, s);
  std::string out;
  std::size_t pos = 0;
  long count = 0;
  while (true) {
    std::size_t hit = s.find(from, pos);
    if (hit == std::string::npos) { out += s.substr(pos); break; }
    ++count;
    if (which == 0 || which == count) { out += s.substr(pos, hit - pos) + to; }
    else out += s.substr(pos, hit - pos + from.size());
    pos = hit + from.size();
  }
  return make_text(ctx, out);
}

Value fn_trim(EvalContext& ctx, const std::vector<Operand>& args) {
  std::string s = text_of(ctx, args[0]);
  std::string out;
  bool prev_space = true;  // collapse leading + internal runs
  for (char ch : s) {
    if (std::isspace(static_cast<unsigned char>(ch))) {
      if (!prev_space) out.push_back(' ');
      prev_space = true;
    } else {
      out.push_back(ch);
      prev_space = false;
    }
  }
  while (!out.empty() && out.back() == ' ') out.pop_back();
  return make_text(ctx, out);
}

Value fn_rept(EvalContext& ctx, const std::vector<Operand>& args) {
  std::string s = text_of(ctx, args[0]);
  bool o; wire::ErrCode e;
  double n = scalar_number(ctx, args[1], o, e);
  if (!o) return err(e);
  if (n < 0) return err(wire::ErrCode::value);
  std::size_t k = static_cast<std::size_t>(n);
  if (s.size() * k > (1u << 20)) return err(wire::ErrCode::value);  // 1MB cap
  std::string out;
  out.reserve(s.size() * k);
  for (std::size_t i = 0; i < k; ++i) out += s;
  return make_text(ctx, out);
}

Value fn_exact(EvalContext& ctx, const std::vector<Operand>& args) {
  return Value::boolean(text_of(ctx, args[0]) == text_of(ctx, args[1]));
}

Value fn_text(EvalContext& ctx, const std::vector<Operand>& args) {
  bool o; wire::ErrCode e;
  double v = scalar_number(ctx, args[0], o, e);
  if (!o) return err(e);
  std::string fmt = text_of(ctx, args[1]);
  return make_text(ctx, format_number(v, fmt));
}

Value fn_textjoin(EvalContext& ctx, const std::vector<Operand>& args) {
  std::string delim = text_of(ctx, args[0]);
  std::string out;
  bool first = true;
  for (std::size_t i = 1; i < args.size(); ++i) {
    bool errored = false; wire::ErrCode ec = wire::ErrCode::value;
    for_each_scalar(ctx, args[i], [&](const Value& v) {
      if (errored) return;
      if (v.is_error()) { errored = true; ec = v.as_error(); return; }
      if (v.is_empty()) return;
      if (!first) out += delim;
      out += coerce_text(ctx, v);
      first = false;
    });
    if (errored) return err(ec);
  }
  return make_text(ctx, out);
}

Value fn_hlookup(EvalContext& ctx, const std::vector<Operand>& args) {
  if (!args[1].is_range()) return err(wire::ErrCode::ref);
  Value key = load_scalar(ctx, args[0]);
  bool o; wire::ErrCode e;
  double rowi = scalar_number(ctx, args[2], o, e);
  if (!o) return err(e);
  const RangeRef& r = args[1].range;
  std::uint32_t height = r.r1 - r.r0 + 1;
  if (rowi < 1 || static_cast<std::uint32_t>(rowi) > height)
    return err(wire::ErrCode::ref);
  std::uint32_t target_row = r.r0 + static_cast<std::uint32_t>(rowi) - 1;
  for (std::uint32_t col = r.c0; col <= r.c1; ++col) {
    Value probe = cell_value(ctx, CellRef{r.sheet, r.r0, col});
    if (values_equal(ctx, probe, key))
      return cell_value(ctx, CellRef{r.sheet, target_row, col});
  }
  return err(wire::ErrCode::na);
}

Value fn_choose(EvalContext& ctx, const std::vector<Operand>& args) {
  bool o; wire::ErrCode e;
  double idx = scalar_number(ctx, args[0], o, e);
  if (!o) return err(e);
  std::size_t k = static_cast<std::size_t>(idx);
  if (idx < 1 || k >= args.size()) return err(wire::ErrCode::value);
  return load_scalar(ctx, args[k]);
}

Value fn_xor(EvalContext& ctx, const std::vector<Operand>& args) {
  int parity = 0;
  for (const Operand& op : args) {
    bool errored = false; wire::ErrCode ec = wire::ErrCode::value;
    for_each_scalar(ctx, op, [&](const Value& v) {
      if (errored) return;
      if (v.is_error()) { errored = true; ec = v.as_error(); return; }
      double d; wire::ErrCode e2;
      if (coerce_number(ctx, v, d, e2) && d != 0.0) parity ^= 1;
    });
    if (errored) return err(ec);
  }
  return Value::boolean(parity != 0);
}

Value fn_iferror(EvalContext& ctx, const std::vector<Operand>& args) {
  Value v = load_scalar(ctx, args[0]);
  return v.is_error() ? load_scalar(ctx, args[1]) : v;
}

Value fn_is(EvalContext& ctx, const Operand& op, wire::Func which) {
  Value v = load_scalar(ctx, op);
  switch (which) {
    case wire::Func::isnumber: return Value::boolean(v.is_number());
    case wire::Func::istext: return Value::boolean(v.is_text());
    case wire::Func::iserror: return Value::boolean(v.is_error());
    case wire::Func::isblank: return Value::boolean(v.is_empty());
    default: return Value::boolean(false);
  }
}

}  // namespace

Value call_function_ext(EvalContext& ctx, wire::Func fn,
                        const std::vector<Operand>& args) {
  using F = wire::Func;
  switch (fn) {
    case F::product: return fn_product(ctx, args);
    case F::sumif: return fn_cond_agg(ctx, args, 0);
    case F::countif: return fn_cond_agg(ctx, args, 1);
    case F::averageif: return fn_cond_agg(ctx, args, 2);
    case F::int_: return fn_unary_math(ctx, args[0], 0);
    case F::trunc_: return fn_unary_math(ctx, args[0], 1);
    case F::sign_: return fn_unary_math(ctx, args[0], 2);
    case F::ceiling_: return fn_step(ctx, args, true);
    case F::floor_: return fn_step(ctx, args, false);
    case F::gcd: return fn_gcd_lcm(ctx, args, true);
    case F::lcm: return fn_gcd_lcm(ctx, args, false);
    case F::median: return fn_median(ctx, args);
    case F::stdev: return fn_var_stdev(ctx, args, true);
    case F::var_: return fn_var_stdev(ctx, args, false);
    case F::rank: return fn_rank(ctx, args);
    case F::find_: return fn_find(ctx, args);
    case F::substitute: return fn_substitute(ctx, args);
    case F::trim: return fn_trim(ctx, args);
    case F::rept: return fn_rept(ctx, args);
    case F::exact: return fn_exact(ctx, args);
    case F::text_fn: return fn_text(ctx, args);
    case F::textjoin: return fn_textjoin(ctx, args);
    case F::hlookup: return fn_hlookup(ctx, args);
    case F::choose: return fn_choose(ctx, args);
    case F::xor_: return fn_xor(ctx, args);
    case F::iferror: return fn_iferror(ctx, args);
    case F::isnumber:
    case F::istext:
    case F::iserror:
    case F::isblank: return fn_is(ctx, args[0], fn);
    default: return call_array_function(ctx, fn, args);
  }
}

}  // namespace slate
