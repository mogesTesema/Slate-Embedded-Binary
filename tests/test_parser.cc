// tests/test_parser.cc
#include "framework.hh"
#include "test_support.hh"

#include "slate/compile/parser.hh"

using namespace slate;
using namespace slate::test;

namespace {

// Parse `formula` against a builder, attach it as a cell, recalc, and return the
// result cell's value. `setup` populates data cells before parsing.
Value parse_eval(const std::string& formula,
                 const std::function<void(tools::WorkbookBuilder&, SheetId)>& setup) {
  tools::WorkbookBuilder b;
  SheetId s = b.add_sheet("Sheet1", 32, 16);
  if (setup) setup(b, s);

  ParseContext ctx;
  ctx.default_sheet = s;
  ctx.intern_string = [&](std::string_view sv) { return b.add_string(sv); };

  std::vector<std::uint8_t> code;
  std::string err;
  Status st = parse_formula(formula, ctx, code, err);
  if (!ok(st)) return Value::error(wire::ErrCode::name);

  FormulaId fid = b.add_formula(code);
  b.formula_cell(s, 25, 10, fid);
  return recalc_value(b.build(), s, 25, 10);
}

}  // namespace

TEST("parser: arithmetic precedence") {
  Value v = parse_eval("=1+2*3", nullptr);
  CHECK(v.is_number()); CHECK_EQ(v.as_number(), 7.0);
}

TEST("parser: parentheses override precedence") {
  Value v = parse_eval("=(1+2)*3", nullptr);
  CHECK(v.is_number()); CHECK_EQ(v.as_number(), 9.0);
}

TEST("parser: unary minus") {
  Value v = parse_eval("=-5+3", nullptr);
  CHECK(v.is_number()); CHECK_EQ(v.as_number(), -2.0);
}

TEST("parser: SUM over a parsed range") {
  Value v = parse_eval("=SUM(A1:A3)", [](tools::WorkbookBuilder& b, SheetId s) {
    b.number_cell(s, 0, 0, 1); b.number_cell(s, 1, 0, 2); b.number_cell(s, 2, 0, 3);
  });
  CHECK(v.is_number()); CHECK_EQ(v.as_number(), 6.0);
}

TEST("parser: IF with a comparison") {
  Value v = parse_eval("=IF(A1>A2, 10, 20)", [](tools::WorkbookBuilder& b, SheetId s) {
    b.number_cell(s, 0, 0, 5); b.number_cell(s, 1, 0, 3);
  });
  CHECK(v.is_number()); CHECK_EQ(v.as_number(), 10.0);
}

TEST("parser: <= comparison yields a boolean") {
  Value v = parse_eval("=A1<=A2", [](tools::WorkbookBuilder& b, SheetId s) {
    b.number_cell(s, 0, 0, 2); b.number_cell(s, 1, 0, 2);
  });
  CHECK(v.is_bool()); CHECK(v.as_bool());
}

TEST("parser: nested calls") {
  Value v = parse_eval("=ROUND(POWER(2,10)/3, 2)", nullptr);
  CHECK(v.is_number()); CHECK_EQ(v.as_number(), 341.33);
}

TEST("parser: LEN of a string literal") {
  Value v = parse_eval("=LEN(\"hello\")", nullptr);
  CHECK(v.is_number()); CHECK_EQ(v.as_number(), 5.0);
}

TEST("parser: cell reference arithmetic") {
  Value v = parse_eval("=A1*2+A2", [](tools::WorkbookBuilder& b, SheetId s) {
    b.number_cell(s, 0, 0, 10); b.number_cell(s, 1, 0, 5);
  });
  CHECK(v.is_number()); CHECK_EQ(v.as_number(), 25.0);
}

TEST("parser: unknown function is rejected") {
  ParseContext ctx;
  std::vector<std::uint8_t> code; std::string err;
  Status st = parse_formula("=BOGUS(1)", ctx, code, err);
  CHECK(!ok(st));
}
