// tests/test_stats_text.cc
#include "framework.hh"
#include "test_support.hh"

#include <cmath>

#include "slate/compile/parser.hh"

using namespace slate;
using namespace slate::test;

namespace {

Value ev(const std::string& formula,
         const std::function<void(tools::WorkbookBuilder&, SheetId)>& setup = nullptr) {
  tools::WorkbookBuilder b;
  SheetId s = b.add_sheet("S", 16, 8);
  if (setup) setup(b, s);
  ParseContext ctx;
  ctx.default_sheet = s;
  ctx.intern_string = [&](std::string_view sv) { return b.add_string(sv); };
  std::vector<std::uint8_t> code; std::string err;
  if (!ok(parse_formula(formula, ctx, code, err))) return Value::error(wire::ErrCode::name);
  FormulaId fid = b.add_formula(code);
  b.formula_cell(s, 12, 6, fid);
  return recalc_value(b.build(), s, 12, 6);
}

double n(const std::string& f) { Value v = ev(f); return v.is_number() ? v.as_number() : -1e18; }
bool near(double a, double b) { return std::fabs(a - b) < 1e-9; }

}  // namespace

TEST("stats: LARGE and SMALL") {
  auto setup = [](tools::WorkbookBuilder& b, SheetId s) {
    b.number_cell(s, 0, 0, 3); b.number_cell(s, 1, 0, 1);
    b.number_cell(s, 2, 0, 4); b.number_cell(s, 3, 0, 1); b.number_cell(s, 4, 0, 5);
  };
  CHECK(near(ev("=LARGE(A1:A5, 1)", setup).as_number(), 5));
  CHECK(near(ev("=LARGE(A1:A5, 2)", setup).as_number(), 4));
  CHECK(near(ev("=SMALL(A1:A5, 1)", setup).as_number(), 1));
}

TEST("stats: population variance and stdev") {
  auto setup = [](tools::WorkbookBuilder& b, SheetId s) {
    b.number_cell(s, 0, 0, 2); b.number_cell(s, 1, 0, 4); b.number_cell(s, 2, 0, 6);
  };
  // mean 4; population var = ((4)+(0)+(4))/3 = 8/3
  CHECK(near(ev("=VARP(A1:A3)", setup).as_number(), 8.0 / 3.0));
  CHECK(near(ev("=STDEVP(A1:A3)", setup).as_number(), std::sqrt(8.0 / 3.0)));
}

TEST("stats: MODE and GEOMEAN") {
  auto setup = [](tools::WorkbookBuilder& b, SheetId s) {
    b.number_cell(s, 0, 0, 2); b.number_cell(s, 1, 0, 2); b.number_cell(s, 2, 0, 8);
  };
  CHECK(near(ev("=MODE(A1:A3)", setup).as_number(), 2));
  CHECK(near(ev("=GEOMEAN(A1:A3)", setup).as_number(), std::cbrt(2.0 * 2.0 * 8.0)));
}

TEST("info: ISEVEN / ISODD") {
  CHECK(ev("=ISEVEN(4)").as_bool());
  CHECK(ev("=ISODD(7)").as_bool());
  CHECK(!ev("=ISEVEN(3)").as_bool());
}

TEST("info: N of a comparison boolean") {
  CHECK(near(n("=N(1<2)"), 1));
  CHECK(near(n("=N(2<1)"), 0));
}

TEST("text: PROPER / CHAR / CODE via numeric checks") {
  // LEN(PROPER("hello world")) == 11
  CHECK(near(n("=LEN(PROPER(\"hello world\"))"), 11));
  // CODE(CHAR(65)) == 65
  CHECK(near(n("=CODE(CHAR(65))"), 65));
}

TEST("text: VALUE parses a numeric string") {
  CHECK(near(n("=VALUE(\"42.5\")"), 42.5));
  CHECK(n("=VALUE(\"abc\")") < -1e17);  // error
}

TEST("text: SEARCH is case-insensitive, REPLACE edits a span") {
  CHECK(near(n("=SEARCH(\"WOR\", \"hello world\")"), 7));
  // REPLACE("abcdef", 2, 3, "XY") -> "aXYef" length 5
  CHECK(near(n("=LEN(REPLACE(\"abcdef\", 2, 3, \"XY\"))"), 5));
}
