// slate/eval/criteria.hh (internal to the eval library)
//
// Shared criteria matching for the conditional aggregates (SUMIF/COUNTIF/
// AVERAGEIF and the multi-criteria SUMIFS/COUNTIFS). A criterion is either a
// bare value (equality) or a text comparison like ">10" / "<>x". Kept in one
// place so every conditional function agrees on the rules.
#ifndef SLATE_EVAL_CRITERIA_HH
#define SLATE_EVAL_CRITERIA_HH

#include <string>

#include "slate/eval/operand.hh"
#include "slate/eval/value_ops.hh"

namespace slate::detail {

struct Criteria {
  enum Op { eq, ne, lt, le, gt, ge } op = eq;
  bool numeric = false;
  double num = 0.0;
  std::string text;
};

inline Criteria parse_criteria(EvalContext& ctx, const Operand& op) {
  Criteria c;
  Value v = load_scalar(ctx, op);
  if (v.is_number() || v.is_bool()) {
    c.numeric = true;
    c.num = v.coerce_number();
    return c;
  }
  std::string s = coerce_text(ctx, v);
  std::size_t i = 0;
  if (s.size() >= 2 && s[0] == '<' && s[1] == '>') { c.op = Criteria::ne; i = 2; }
  else if (s.size() >= 2 && s[0] == '>' && s[1] == '=') { c.op = Criteria::ge; i = 2; }
  else if (s.size() >= 2 && s[0] == '<' && s[1] == '=') { c.op = Criteria::le; i = 2; }
  else if (!s.empty() && s[0] == '>') { c.op = Criteria::gt; i = 1; }
  else if (!s.empty() && s[0] == '<') { c.op = Criteria::lt; i = 1; }
  else if (!s.empty() && s[0] == '=') { c.op = Criteria::eq; i = 1; }
  std::string rest = s.substr(i);
  char* endp = nullptr;
  double d = std::strtod(rest.c_str(), &endp);
  if (!rest.empty() && endp && *endp == '\0') { c.numeric = true; c.num = d; }
  else c.text = rest;
  return c;
}

inline bool criteria_match(EvalContext& ctx, const Criteria& c, const Value& cell) {
  if (c.numeric) {
    double d;
    wire::ErrCode e;
    if (!coerce_number(ctx, cell, d, e)) return false;
    switch (c.op) {
      case Criteria::eq: return d == c.num;
      case Criteria::ne: return d != c.num;
      case Criteria::lt: return d < c.num;
      case Criteria::le: return d <= c.num;
      case Criteria::gt: return d > c.num;
      case Criteria::ge: return d >= c.num;
    }
  }
  std::string s = coerce_text(ctx, cell);
  return c.op == Criteria::ne ? (s != c.text) : (s == c.text);
}

// Collect the cells a range/cell operand covers, row-major.
inline void range_cells(const Operand& op, std::vector<CellRef>& out) {
  if (op.is_range()) {
    const RangeRef& r = op.range;
    for (std::uint32_t row = r.r0; row <= r.r1; ++row)
      for (std::uint32_t col = r.c0; col <= r.c1; ++col)
        out.push_back(CellRef{r.sheet, row, col});
  } else if (op.is_cell()) {
    out.push_back(op.ref);
  }
}

}  // namespace slate::detail

#endif  // SLATE_EVAL_CRITERIA_HH
