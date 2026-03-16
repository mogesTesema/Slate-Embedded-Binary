// tests/test_names.cc
#include "framework.hh"
#include "test_support.hh"

using namespace slate;
using namespace slate::test;

TEST("names: a defined range name sums correctly through SUM") {
  tools::WorkbookBuilder b;
  SheetId data = b.add_sheet("Data", 8, 4);
  SheetId main = b.add_sheet("Main", 8, 4);
  b.number_cell(data, 0, 0, 10.0);
  b.number_cell(data, 1, 0, 20.0);
  b.number_cell(data, 2, 0, 30.0);
  b.add_name_range("rng", data, 0, 0, 2, 0);
  CellId f = b.formula_text_cell(main, 0, 0, "=SUM(rng)");
  CHECK(f != kNoCell);
  Value v = recalc_value(b.build(), main, 0, 0);
  CHECK(v.is_number());
  CHECK_EQ(v.as_number(), 60.0);
}

TEST("names: a name still resolves correctly across a second recalc pass") {
  tools::WorkbookBuilder b;
  SheetId data = b.add_sheet("Data", 8, 4);
  SheetId main = b.add_sheet("Main", 8, 4);
  b.number_cell(data, 0, 0, 1.0);
  b.number_cell(data, 1, 0, 2.0);
  b.add_name_range("rng", data, 0, 0, 1, 0);
  b.formula_text_cell(main, 0, 0, "=SUM(rng)");
  // A benign extra pass over the same live sheet must not disturb the result.
  b.add_directive(wire::Directive::recalc_pass, 1, 0);
  Value v = recalc_value(b.build(), main, 0, 0);
  CHECK(v.is_number());
  CHECK_EQ(v.as_number(), 3.0);
}
