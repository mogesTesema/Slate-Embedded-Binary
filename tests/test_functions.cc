// tests/test_functions.cc
#include "framework.hh"
#include "test_support.hh"

using namespace slate;
using namespace slate::test;

namespace {

// Build a one-sheet workbook with a single formula cell parked well clear of any
// data region and return its value.
Value one_formula(Code& f, tools::WorkbookBuilder& b, SheetId s) {
  constexpr std::uint32_t kRow = 20, kCol = 20;
  FormulaId fid = b.add_formula(f.c);
  b.formula_cell(s, kRow, kCol, fid);
  return recalc_value(b.build(), s, kRow, kCol);
}

}  // namespace

TEST("fn: PRODUCT") {
  tools::WorkbookBuilder b; SheetId s = b.add_sheet("S", 4, 4);
  Code f; f.num(2); f.num(3); f.num(4); f.call(wire::Func::product, 3); f.end();
  Value v = one_formula(f, b, s);
  CHECK(v.is_number()); CHECK_EQ(v.as_number(), 24.0);
}

TEST("fn: MEDIAN odd and even") {
  { tools::WorkbookBuilder b; SheetId s = b.add_sheet("S", 4, 4);
    Code f; f.num(3); f.num(1); f.num(2); f.call(wire::Func::median, 3); f.end();
    Value v = one_formula(f, b, s); CHECK(v.is_number()); CHECK_EQ(v.as_number(), 2.0); }
  { tools::WorkbookBuilder b; SheetId s = b.add_sheet("S", 4, 4);
    Code f; f.num(1); f.num(2); f.num(3); f.num(4); f.call(wire::Func::median, 4); f.end();
    Value v = one_formula(f, b, s); CHECK(v.is_number()); CHECK_EQ(v.as_number(), 2.5); }
}

TEST("fn: GCD and LCM") {
  { tools::WorkbookBuilder b; SheetId s = b.add_sheet("S", 4, 4);
    Code f; f.num(12); f.num(18); f.call(wire::Func::gcd, 2); f.end();
    Value v = one_formula(f, b, s); CHECK(v.is_number()); CHECK_EQ(v.as_number(), 6.0); }
  { tools::WorkbookBuilder b; SheetId s = b.add_sheet("S", 4, 4);
    Code f; f.num(4); f.num(6); f.call(wire::Func::lcm, 2); f.end();
    Value v = one_formula(f, b, s); CHECK(v.is_number()); CHECK_EQ(v.as_number(), 12.0); }
}

TEST("fn: INT / TRUNC / SIGN") {
  { tools::WorkbookBuilder b; SheetId s = b.add_sheet("S", 4, 4);
    Code f; f.num(3.7); f.call(wire::Func::int_, 1); f.end();
    Value v = one_formula(f, b, s); CHECK_EQ(v.as_number(), 3.0); }
  { tools::WorkbookBuilder b; SheetId s = b.add_sheet("S", 4, 4);
    Code f; f.num(-3.7); f.call(wire::Func::trunc_, 1); f.end();
    Value v = one_formula(f, b, s); CHECK_EQ(v.as_number(), -3.0); }
  { tools::WorkbookBuilder b; SheetId s = b.add_sheet("S", 4, 4);
    Code f; f.num(-5); f.call(wire::Func::sign_, 1); f.end();
    Value v = one_formula(f, b, s); CHECK_EQ(v.as_number(), -1.0); }
}

TEST("fn: SUMIF with comparison criteria") {
  tools::WorkbookBuilder b; SheetId s = b.add_sheet("S", 8, 4);
  b.number_cell(s, 0, 0, 5); b.number_cell(s, 1, 0, 15); b.number_cell(s, 2, 0, 25);
  StrId crit = b.add_string(">10");
  Code f; f.range(s, 0, 0, 2, 0); f.str(crit); f.call(wire::Func::sumif, 2); f.end();
  Value v = one_formula(f, b, s);
  CHECK(v.is_number()); CHECK_EQ(v.as_number(), 40.0);  // 15 + 25
}

TEST("fn: COUNTIF equality") {
  tools::WorkbookBuilder b; SheetId s = b.add_sheet("S", 8, 4);
  b.number_cell(s, 0, 0, 7); b.number_cell(s, 1, 0, 7); b.number_cell(s, 2, 0, 9);
  StrId crit = b.add_string("7");
  Code f; f.range(s, 0, 0, 2, 0); f.str(crit); f.call(wire::Func::countif, 2); f.end();
  Value v = one_formula(f, b, s);
  CHECK(v.is_number()); CHECK_EQ(v.as_number(), 2.0);
}

TEST("fn: FIND returns 1-based position") {
  tools::WorkbookBuilder b; SheetId s = b.add_sheet("S", 4, 4);
  StrId needle = b.add_string("b");
  StrId hay = b.add_string("abcb");
  Code f; f.str(needle); f.str(hay); f.call(wire::Func::find_, 2); f.end();
  Value v = one_formula(f, b, s);
  CHECK(v.is_number()); CHECK_EQ(v.as_number(), 2.0);
}

TEST("fn: LEN(TRIM(x)) collapses whitespace") {
  tools::WorkbookBuilder b; SheetId s = b.add_sheet("S", 4, 4);
  StrId msg = b.add_string("  a   b  ");
  Code f; f.str(msg); f.call(wire::Func::trim, 1); f.call(wire::Func::len_, 1); f.end();
  Value v = one_formula(f, b, s);
  CHECK(v.is_number()); CHECK_EQ(v.as_number(), 3.0);  // "a b"
}

TEST("fn: CHOOSE picks the nth argument") {
  tools::WorkbookBuilder b; SheetId s = b.add_sheet("S", 4, 4);
  Code f; f.num(2); f.num(10); f.num(20); f.num(30); f.call(wire::Func::choose, 4); f.end();
  Value v = one_formula(f, b, s);
  CHECK(v.is_number()); CHECK_EQ(v.as_number(), 20.0);
}

TEST("fn: IFERROR substitutes on error") {
  tools::WorkbookBuilder b; SheetId s = b.add_sheet("S", 4, 4);
  Code f; f.num(1); f.num(0); f.op(wire::Op::divide); f.num(99);
  f.call(wire::Func::iferror, 2); f.end();
  Value v = one_formula(f, b, s);
  CHECK(v.is_number()); CHECK_EQ(v.as_number(), 99.0);
}

TEST("fn: ISNUMBER / XOR return booleans") {
  { tools::WorkbookBuilder b; SheetId s = b.add_sheet("S", 4, 4);
    Code f; f.num(42); f.call(wire::Func::isnumber, 1); f.end();
    Value v = one_formula(f, b, s); CHECK(v.is_bool()); CHECK(v.as_bool()); }
  { tools::WorkbookBuilder b; SheetId s = b.add_sheet("S", 4, 4);
    Code f; f.num(1); f.num(0); f.num(1); f.call(wire::Func::xor_, 3); f.end();
    Value v = one_formula(f, b, s); CHECK(v.is_bool()); CHECK(!v.as_bool()); }
}

TEST("fn: HLOOKUP finds a column and reads a row") {
  tools::WorkbookBuilder b; SheetId s = b.add_sheet("S", 8, 8);
  // Row 0: keys 1,2,3 ; Row 1: values 10,20,30
  b.number_cell(s, 0, 0, 1); b.number_cell(s, 0, 1, 2); b.number_cell(s, 0, 2, 3);
  b.number_cell(s, 1, 0, 10); b.number_cell(s, 1, 1, 20); b.number_cell(s, 1, 2, 30);
  Code f; f.num(2); f.range(s, 0, 0, 1, 2); f.num(2); f.call(wire::Func::hlookup, 3); f.end();
  Value v = one_formula(f, b, s);
  CHECK(v.is_number()); CHECK_EQ(v.as_number(), 20.0);
}
