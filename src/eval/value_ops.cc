// slate/eval/value_ops.cc
#include "slate/eval/value_ops.hh"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace slate {

void retain_handles(EvalContext& ctx, const Value& v) {
  if (v.is_text()) ctx.strings.retain(v.as_text());
  else if (v.is_array()) ctx.arrays.retain(v.as_array());
}

void release_handles(EvalContext& ctx, const Value& v) {
  if (v.is_text()) ctx.strings.release(v.as_text());
  else if (v.is_array()) ctx.arrays.release(v.as_array());
}

void assign_value(EvalContext& ctx, Value& slot, const Value& v) {
  // Retain the incoming handle before releasing the outgoing one so that
  // self-assignment (same handle) cannot transiently drop the last reference.
  retain_handles(ctx, v);
  release_handles(ctx, slot);
  slot = v;
}

bool coerce_number(EvalContext& ctx, const Value& v, double& out,
                   wire::ErrCode& err) {
  switch (v.tag()) {
    case ValueTag::empty:
      out = 0.0;
      return true;
    case ValueTag::number:
      out = v.as_number();
      return true;
    case ValueTag::boolean:
      out = v.as_bool() ? 1.0 : 0.0;
      return true;
    case ValueTag::text: {
      std::string_view sv = ctx.strings.resolve(v.as_text());
      std::string tmp(sv);
      char* endp = nullptr;
      double d = std::strtod(tmp.c_str(), &endp);
      if (endp == tmp.c_str() || *endp != '\0') {
        err = wire::ErrCode::value;
        return false;
      }
      out = d;
      return true;
    }
    case ValueTag::error:
      err = v.as_error();
      return false;
    case ValueTag::array:
      err = wire::ErrCode::value;
      return false;
  }
  err = wire::ErrCode::value;
  return false;
}

std::string coerce_text(EvalContext& ctx, const Value& v) {
  switch (v.tag()) {
    case ValueTag::empty:
      return "";
    case ValueTag::number: {
      double d = v.as_number();
      if (d == std::floor(d) && std::abs(d) < 1e15) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(d));
        return buf;
      }
      char buf[32];
      std::snprintf(buf, sizeof(buf), "%.10g", d);
      return buf;
    }
    case ValueTag::boolean:
      return v.as_bool() ? "TRUE" : "FALSE";
    case ValueTag::text:
      return std::string(ctx.strings.resolve(v.as_text()));
    case ValueTag::error:
      switch (v.as_error()) {
        case wire::ErrCode::div0: return "#DIV/0!";
        case wire::ErrCode::value: return "#VALUE!";
        case wire::ErrCode::ref: return "#REF!";
        case wire::ErrCode::name: return "#NAME?";
        case wire::ErrCode::num: return "#NUM!";
        case wire::ErrCode::na: return "#N/A";
        case wire::ErrCode::cycle: return "#CYCLE!";
        case wire::ErrCode::null: return "#NULL!";
      }
      return "#ERR!";
    case ValueTag::array:
      return "";
  }
  return "";
}

bool values_equal(EvalContext& ctx, const Value& a, const Value& b) {
  if (a.is_number() || b.is_number() || a.is_bool() || b.is_bool()) {
    double na, nb;
    wire::ErrCode e;
    if (coerce_number(ctx, a, na, e) && coerce_number(ctx, b, nb, e))
      return na == nb;
  }
  if (a.is_text() && b.is_text())
    return ctx.strings.resolve(a.as_text()) == ctx.strings.resolve(b.as_text());
  if (a.is_empty() && b.is_empty()) return true;
  return false;
}

int values_compare(EvalContext& ctx, const Value& a, const Value& b) {
  double na, nb;
  wire::ErrCode e;
  if (coerce_number(ctx, a, na, e) && coerce_number(ctx, b, nb, e))
    return na < nb ? -1 : (na > nb ? 1 : 0);
  std::string sa = coerce_text(ctx, a), sb = coerce_text(ctx, b);
  return sa < sb ? -1 : (sa > sb ? 1 : 0);
}

}  // namespace slate
