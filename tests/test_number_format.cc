// tests/test_number_format.cc
#include "framework.hh"
#include "test_support.hh"

#include "slate/model/number_format.hh"

using namespace slate;
using namespace slate::test;

TEST("number_format: fixed decimals") {
  CHECK(format_number(1234.5, "0.00") == "1234.50");
  CHECK(format_number(-5.5, "0.0") == "-5.5");
}

TEST("number_format: thousands grouping") {
  CHECK(format_number(1234.567, "#,##0.00") == "1,234.57");
  CHECK(format_number(1000000, "#,##0") == "1,000,000");
}

TEST("number_format: percent") {
  CHECK(format_number(0.25, "0%") == "25%");
}

TEST("number_format: currency prefix") {
  CHECK(format_number(1234, "$#,##0") == "$1,234");
}

TEST("number_format: empty code uses general") {
  CHECK(format_number(3.0, "") == "3");
  CHECK(format_number(2.5, "") == "2.5");
}

TEST("fn: TEXT then LEN gives the formatted length") {
  tools::WorkbookBuilder b; SheetId s = b.add_sheet("S", 8, 4);
  StrId fmt = b.add_string("0.00");
  // =LEN(TEXT(1234.5, "0.00")) -> "1234.50" -> length 7
  Code f; f.num(1234.5); f.str(fmt); f.call(wire::Func::text_fn, 2);
  f.call(wire::Func::len_, 1); f.end();
  FormulaId fid = b.add_formula(f.c);
  b.formula_cell(s, 20, 20, fid);
  Value v = recalc_value(b.build(), s, 20, 20);
  CHECK(v.is_number()); CHECK_EQ(v.as_number(), 7.0);
}
