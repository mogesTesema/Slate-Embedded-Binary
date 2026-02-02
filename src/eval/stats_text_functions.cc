// slate/eval/stats_text_functions.cc
//
// Population statistics (VARP/STDEVP), order statistics (LARGE/SMALL/MODE),
// GEOMEAN, a few type predicates (ISEVEN/ISODD/N), and additional text
// functions (PROPER/CHAR/CODE/VALUE/SEARCH/REPLACE/CLEAN). These round out the
// library; each returns an error value on a domain problem rather than throwing.
#include "slate/eval/functions.hh"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

namespace slate {
namespace {

Value err(wire::ErrCode e) { return Value::error(e); }

Value make_text(EvalContext& ctx, const std::string& s) {
  return Value::text(ctx.strings.intern_dynamic(s));
}

std::string text_of(EvalContext& ctx, const Operand& op) {
  return coerce_text(ctx, load_scalar(ctx, op));
}

bool collect(EvalContext& ctx, const std::vector<Operand>& args, std::size_t first,
             std::vector<double>& out, wire::ErrCode& ec) {
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

Value fn_popvar(EvalContext& ctx, const std::vector<Operand>& args, bool stdev) {
  std::vector<double> n; wire::ErrCode e;
  if (!collect(ctx, args, 0, n, e)) return err(e);
  if (n.empty()) return err(wire::ErrCode::div0);
  double mean = 0; for (double d : n) mean += d; mean /= n.size();
  double ss = 0; for (double d : n) ss += (d - mean) * (d - mean);
  double var = ss / n.size();
  return Value::number(stdev ? std::sqrt(var) : var);
}

Value fn_counta(EvalContext& ctx, const std::vector<Operand>& args) {
  long n = 0;
  for (const Operand& op : args)
    for_each_scalar(ctx, op, [&](const Value& v) { if (!v.is_empty()) ++n; });
  return Value::number(static_cast<double>(n));
}

Value fn_order(EvalContext& ctx, const std::vector<Operand>& args, bool large) {
  std::vector<double> n; wire::ErrCode e;
  if (!collect(ctx, {args[0]}, 0, n, e)) return err(e);
  double kd; if (!coerce_number(ctx, load_scalar(ctx, args[1]), kd, e)) return err(e);
  long k = static_cast<long>(kd);
  if (k < 1 || static_cast<std::size_t>(k) > n.size()) return err(wire::ErrCode::num);
  std::sort(n.begin(), n.end());
  return Value::number(large ? n[n.size() - k] : n[k - 1]);
}

Value fn_mode(EvalContext& ctx, const std::vector<Operand>& args) {
  std::vector<double> n; wire::ErrCode e;
  if (!collect(ctx, args, 0, n, e)) return err(e);
  std::unordered_map<double, long> freq;
  double best = 0; long best_count = 0;
  for (double d : n) {
    long c = ++freq[d];
    if (c > best_count) { best_count = c; best = d; }
  }
  if (best_count < 2) return err(wire::ErrCode::na);
  return Value::number(best);
}

Value fn_geomean(EvalContext& ctx, const std::vector<Operand>& args) {
  std::vector<double> n; wire::ErrCode e;
  if (!collect(ctx, args, 0, n, e)) return err(e);
  if (n.empty()) return err(wire::ErrCode::num);
  double logsum = 0;
  for (double d : n) {
    if (d <= 0) return err(wire::ErrCode::num);
    logsum += std::log(d);
  }
  return Value::number(std::exp(logsum / n.size()));
}

Value fn_parity(EvalContext& ctx, const Operand& op, bool even) {
  double d; wire::ErrCode e;
  if (!coerce_number(ctx, load_scalar(ctx, op), d, e)) return err(e);
  long n = static_cast<long>(std::trunc(d));
  bool is_even = (n % 2) == 0;
  return Value::boolean(even ? is_even : !is_even);
}

Value fn_n(EvalContext& ctx, const Operand& op) {
  Value v = load_scalar(ctx, op);
  if (v.is_number()) return v;
  if (v.is_bool()) return Value::number(v.as_bool() ? 1 : 0);
  if (v.is_error()) return v;
  return Value::number(0);
}

Value fn_proper(EvalContext& ctx, const Operand& op) {
  std::string s = text_of(ctx, op);
  bool start = true;
  for (char& c : s) {
    unsigned char uc = static_cast<unsigned char>(c);
    if (std::isalpha(uc)) {
      c = start ? static_cast<char>(std::toupper(uc)) : static_cast<char>(std::tolower(uc));
      start = false;
    } else {
      start = true;
    }
  }
  return make_text(ctx, s);
}

Value fn_char(EvalContext& ctx, const Operand& op) {
  double d; wire::ErrCode e;
  if (!coerce_number(ctx, load_scalar(ctx, op), d, e)) return err(e);
  long n = static_cast<long>(d);
  if (n < 1 || n > 255) return err(wire::ErrCode::value);
  return make_text(ctx, std::string(1, static_cast<char>(n)));
}

Value fn_code(EvalContext& ctx, const Operand& op) {
  std::string s = text_of(ctx, op);
  if (s.empty()) return err(wire::ErrCode::value);
  return Value::number(static_cast<double>(static_cast<unsigned char>(s[0])));
}

Value fn_value(EvalContext& ctx, const Operand& op) {
  std::string s = text_of(ctx, op);
  char* endp = nullptr;
  double d = std::strtod(s.c_str(), &endp);
  if (endp == s.c_str()) return err(wire::ErrCode::value);
  while (*endp && std::isspace(static_cast<unsigned char>(*endp))) ++endp;
  if (*endp != '\0') return err(wire::ErrCode::value);
  return Value::number(d);
}

Value fn_search(EvalContext& ctx, const std::vector<Operand>& args) {
  std::string needle = text_of(ctx, args[0]);
  std::string hay = text_of(ctx, args[1]);
  std::size_t start = 0;
  if (args.size() >= 3) {
    double d; wire::ErrCode e;
    if (!coerce_number(ctx, load_scalar(ctx, args[2]), d, e)) return err(e);
    if (d < 1) return err(wire::ErrCode::value);
    start = static_cast<std::size_t>(d) - 1;
  }
  auto lower = [](std::string x) {
    for (char& c : x) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return x;
  };
  std::string ln = lower(needle), lh = lower(hay);
  if (start > lh.size()) return err(wire::ErrCode::value);
  std::size_t pos = lh.find(ln, start);
  if (pos == std::string::npos) return err(wire::ErrCode::value);
  return Value::number(static_cast<double>(pos + 1));
}

Value fn_replace(EvalContext& ctx, const std::vector<Operand>& args) {
  std::string s = text_of(ctx, args[0]);
  double startd, lend; wire::ErrCode e;
  if (!coerce_number(ctx, load_scalar(ctx, args[1]), startd, e)) return err(e);
  if (!coerce_number(ctx, load_scalar(ctx, args[2]), lend, e)) return err(e);
  std::string repl = text_of(ctx, args[3]);
  if (startd < 1 || lend < 0) return err(wire::ErrCode::value);
  std::size_t start = static_cast<std::size_t>(startd) - 1;
  std::size_t len = static_cast<std::size_t>(lend);
  if (start > s.size()) start = s.size();
  if (len > s.size() - start) len = s.size() - start;
  return make_text(ctx, s.substr(0, start) + repl + s.substr(start + len));
}

Value fn_clean(EvalContext& ctx, const Operand& op) {
  std::string s = text_of(ctx, op), out;
  for (char c : s)
    if (static_cast<unsigned char>(c) >= 32) out.push_back(c);
  return make_text(ctx, out);
}

}  // namespace

Value call_stats_text_function(EvalContext& ctx, wire::Func fn,
                               const std::vector<Operand>& args) {
  using F = wire::Func;
  switch (fn) {
    case F::varp: return fn_popvar(ctx, args, false);
    case F::stdevp: return fn_popvar(ctx, args, true);
    case F::counta: return fn_counta(ctx, args);
    case F::large: return fn_order(ctx, args, true);
    case F::small: return fn_order(ctx, args, false);
    case F::mode: return fn_mode(ctx, args);
    case F::geomean: return fn_geomean(ctx, args);
    case F::iseven: return fn_parity(ctx, args[0], true);
    case F::isodd: return fn_parity(ctx, args[0], false);
    case F::n_fn: return fn_n(ctx, args[0]);
    case F::proper: return fn_proper(ctx, args[0]);
    case F::char_: return fn_char(ctx, args[0]);
    case F::code_: return fn_code(ctx, args[0]);
    case F::value_fn: return fn_value(ctx, args[0]);
    case F::search: return fn_search(ctx, args);
    case F::replace_fn: return fn_replace(ctx, args);
    case F::clean: return fn_clean(ctx, args[0]);
    default: return call_lookup_function(ctx, fn, args);
  }
}

}  // namespace slate
