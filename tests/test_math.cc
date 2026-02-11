// tests/test_math.cc
#include "framework.hh"
#include "test_support.hh"

#include <cmath>

#include "slate/compile/parser.hh"

using namespace slate;
using namespace slate::test;

namespace {

double eval_num(const std::string& formula) {
  tools::WorkbookBuilder b;
  SheetId s = b.add_sheet("S", 8, 8);
  ParseContext ctx;
  ctx.default_sheet = s;
  ctx.intern_string = [&](std::string_view sv) { return b.add_string(sv); };
  std::vector<std::uint8_t> code; std::string err;
  if (!ok(parse_formula(formula, ctx, code, err))) return -1e18;
  FormulaId fid = b.add_formula(code);
  b.formula_cell(s, 6, 6, fid);
  Value v = recalc_value(b.build(), s, 6, 6);
  return v.is_number() ? v.as_number() : -1e18;
}

bool near(double a, double b) { return std::fabs(a - b) < 1e-9; }

}  // namespace

TEST("math: combinatorics") {
  CHECK(near(eval_num("=FACT(5)"), 120));
  CHECK(near(eval_num("=COMBIN(5,2)"), 10));
  CHECK(near(eval_num("=PERMUT(5,2)"), 20));
}

TEST("math: QUOTIENT and MROUND") {
  CHECK(near(eval_num("=QUOTIENT(17,5)"), 3));
  CHECK(near(eval_num("=MROUND(17,5)"), 15));
}

TEST("math: directional rounding") {
  CHECK(near(eval_num("=ROUNDUP(3.141,2)"), 3.15));
  CHECK(near(eval_num("=ROUNDDOWN(3.19,1)"), 3.1));
  CHECK(near(eval_num("=ROUNDDOWN(-3.19,1)"), -3.1));
}

TEST("math: logarithms") {
  CHECK(near(eval_num("=LOG(1000)"), 3));
  CHECK(near(eval_num("=LOG(8,2)"), 3));
  CHECK(near(eval_num("=EXP(0)"), 1));
  CHECK(near(eval_num("=LN(1)"), 0));
}

TEST("math: LOG of non-positive is an error") {
  CHECK(eval_num("=LOG(-5)") < -1e17);  // error value, not a number
}

TEST("math: trig round-trip") {
  CHECK(near(eval_num("=DEGREES(RADIANS(90))"), 90));
  CHECK(near(eval_num("=SIN(0)"), 0));
}

TEST("financial: PMT/FV/PV at zero rate") {
  CHECK(near(eval_num("=PMT(0,10,1000)"), -100));
  CHECK(near(eval_num("=FV(0,10,-100)"), 1000));
  CHECK(near(eval_num("=PV(0,5,-100)"), 500));
}

TEST("financial: NPV discounts future cash flows") {
  // 100/1.1 + 100/1.21
  double expected = 100.0 / 1.1 + 100.0 / 1.21;
  CHECK(near(eval_num("=NPV(0.1,100,100)"), expected));
}

TEST("financial: PMT amortizes a loan") {
  // A 1000 loan at 1%/period over 12 periods has a known payment.
  double pmt = eval_num("=PMT(0.01,12,1000)");
  CHECK(pmt < 0);                       // payment is an outflow
  CHECK(std::fabs(pmt + 88.8487876) < 1e-3);
}
