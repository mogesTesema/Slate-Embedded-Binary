// tests/test_merges.cc
#include "framework.hh"
#include "test_support.hh"

using namespace slate;
using namespace slate::test;

TEST("merge: reading a covered cell resolves to the anchor value") {
  tools::WorkbookBuilder b; SheetId s = b.add_sheet("S", 16, 8);
  b.number_cell(s, 0, 0, 42.0);        // anchor A1
  b.add_merge(s, 0, 0, 1, 1);          // merge A1:B2
  // Formula reads B2 (covered, not the anchor) -> should see 42.
  Code f; f.ref(s, 1, 1); f.end();
  FormulaId fid = b.add_formula(f.c);
  b.formula_cell(s, 20, 20, fid);
  Value v = recalc_value(b.build(), s, 20, 20);
  CHECK(v.is_number()); CHECK_EQ(v.as_number(), 42.0);
}

TEST("merge: the anchor cell keeps its own value") {
  tools::WorkbookBuilder b; SheetId s = b.add_sheet("S", 16, 8);
  b.number_cell(s, 2, 2, 7.0);         // anchor C3
  b.add_merge(s, 2, 2, 3, 4);          // merge C3:E4
  Code f; f.ref(s, 2, 2); f.end();     // read the anchor itself
  FormulaId fid = b.add_formula(f.c);
  b.formula_cell(s, 20, 20, fid);
  Value v = recalc_value(b.build(), s, 20, 20);
  CHECK(v.is_number()); CHECK_EQ(v.as_number(), 7.0);
}

TEST("merge: a cell outside any region is unaffected") {
  tools::WorkbookBuilder b; SheetId s = b.add_sheet("S", 16, 8);
  b.number_cell(s, 0, 0, 1.0);
  b.number_cell(s, 5, 5, 99.0);
  b.add_merge(s, 0, 0, 1, 1);
  Code f; f.ref(s, 5, 5); f.end();
  FormulaId fid = b.add_formula(f.c);
  b.formula_cell(s, 20, 20, fid);
  Value v = recalc_value(b.build(), s, 20, 20);
  CHECK(v.is_number()); CHECK_EQ(v.as_number(), 99.0);
}
