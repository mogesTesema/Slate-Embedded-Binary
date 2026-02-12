// tests/test_dates.cc
#include "framework.hh"
#include "test_support.hh"

#include "slate/compile/parser.hh"

using namespace slate;
using namespace slate::test;

namespace {

Value eval_formula(const std::string& formula) {
  tools::WorkbookBuilder b;
  SheetId s = b.add_sheet("S", 8, 8);
  ParseContext ctx;
  ctx.default_sheet = s;
  ctx.intern_string = [&](std::string_view sv) { return b.add_string(sv); };
  std::vector<std::uint8_t> code; std::string err;
  if (!ok(parse_formula(formula, ctx, code, err)))
    return Value::error(wire::ErrCode::name);
  FormulaId fid = b.add_formula(code);
  b.formula_cell(s, 6, 6, fid);
  return recalc_value(b.build(), s, 6, 6);
}

double num(const std::string& f) {
  Value v = eval_formula(f);
  return v.is_number() ? v.as_number() : -1e18;
}

}  // namespace

TEST("date: YEAR/MONTH/DAY round-trip through DATE") {
  CHECK_EQ(num("=YEAR(DATE(2020,7,15))"), 2020.0);
  CHECK_EQ(num("=MONTH(DATE(2020,7,15))"), 7.0);
  CHECK_EQ(num("=DAY(DATE(2020,7,15))"), 15.0);
}

TEST("date: DAYS between two dates") {
  CHECK_EQ(num("=DAYS(DATE(2020,1,31), DATE(2020,1,1))"), 30.0);
  CHECK_EQ(num("=DAYS(DATE(2021,1,1), DATE(2020,1,1))"), 366.0);  // 2020 leap
}

TEST("date: DATE normalizes overflowing months") {
  // Month 13 rolls into the next year.
  CHECK_EQ(num("=YEAR(DATE(2020,13,1))"), 2021.0);
  CHECK_EQ(num("=MONTH(DATE(2020,13,1))"), 1.0);
}

TEST("date: EOMONTH lands on the last day") {
  CHECK_EQ(num("=DAY(EOMONTH(DATE(2020,2,10), 0))"), 29.0);   // Feb 2020 leap
  CHECK_EQ(num("=DAY(EOMONTH(DATE(2021,2,10), 0))"), 28.0);   // Feb 2021
  CHECK_EQ(num("=MONTH(EOMONTH(DATE(2020,1,15), 2))"), 3.0);  // +2 months -> Mar
}

TEST("date: EDATE clamps a short month") {
  // Jan 31 + 1 month -> Feb 29 in a leap year.
  CHECK_EQ(num("=DAY(EDATE(DATE(2020,1,31), 1))"), 29.0);
  CHECK_EQ(num("=MONTH(EDATE(DATE(2020,1,31), 1))"), 2.0);
}

TEST("date: WEEKDAY is in range 1..7") {
  double w = num("=WEEKDAY(DATE(2020,7,15))");
  CHECK(w >= 1.0 && w <= 7.0);
}
