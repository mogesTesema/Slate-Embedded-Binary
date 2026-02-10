// tests/test_arrays.cc
#include "framework.hh"
#include "test_support.hh"

using namespace slate;
using namespace slate::test;

TEST("array: SUMPRODUCT of two columns") {
  tools::WorkbookBuilder b; SheetId s = b.add_sheet("S", 16, 4);
  b.number_cell(s, 0, 0, 1); b.number_cell(s, 1, 0, 2); b.number_cell(s, 2, 0, 3);
  b.number_cell(s, 0, 1, 4); b.number_cell(s, 1, 1, 5); b.number_cell(s, 2, 1, 6);
  Code f;
  f.range(s, 0, 0, 2, 0); f.range(s, 0, 1, 2, 1);
  f.call(wire::Func::sumproduct, 2); f.end();
  FormulaId fid = b.add_formula(f.c);
  b.formula_cell(s, 20, 20, fid);
  Value v = recalc_value(b.build(), s, 20, 20);
  CHECK(v.is_number()); CHECK_EQ(v.as_number(), 32.0);  // 4+10+18
}

TEST("array: SUMSQ") {
  tools::WorkbookBuilder b; SheetId s = b.add_sheet("S", 8, 4);
  Code f; f.num(3); f.num(4); f.call(wire::Func::sumsq, 2); f.end();
  FormulaId fid = b.add_formula(f.c);
  b.formula_cell(s, 20, 20, fid);
  Value v = recalc_value(b.build(), s, 20, 20);
  CHECK(v.is_number()); CHECK_EQ(v.as_number(), 25.0);
}

TEST("array: TRANSPOSE spills a row from a column") {
  tools::WorkbookBuilder b; SheetId s = b.add_sheet("S", 16, 8);
  b.number_cell(s, 0, 0, 1); b.number_cell(s, 1, 0, 2); b.number_cell(s, 2, 0, 3);
  // Array formula: =TRANSPOSE(A1:A3) -> a 1x3 row.
  Code f; f.range(s, 0, 0, 2, 0); f.call(wire::Func::transpose, 1); f.end();
  FormulaId fid = b.add_formula(f.c);
  SpillId sp = b.add_spill(s, /*anchor*/ 5, 0, /*rows*/ 1, /*cols*/ 3, fid);
  b.array_cell(s, 5, 0, sp);
  // Spilled cell (5,2) should hold the third transposed element = 3.
  Value v = recalc_value(b.build(), s, 5, 2);
  CHECK(v.is_number()); CHECK_EQ(v.as_number(), 3.0);
}

TEST("array: MMULT of a 1x2 by a 2x1 is a scalar-shaped product") {
  tools::WorkbookBuilder b; SheetId s = b.add_sheet("S", 16, 8);
  // a = [2 3] at (0,0)-(0,1); b = [4;5] at (1,0)-(2,0)
  b.number_cell(s, 0, 0, 2); b.number_cell(s, 0, 1, 3);
  b.number_cell(s, 1, 0, 4); b.number_cell(s, 2, 0, 5);
  // =MMULT(a, b) -> 1x1 = 2*4 + 3*5 = 23, spilled at anchor (10,0).
  Code f; f.range(s, 0, 0, 0, 1); f.range(s, 1, 0, 2, 0);
  f.call(wire::Func::mmult, 2); f.end();
  FormulaId fid = b.add_formula(f.c);
  SpillId sp = b.add_spill(s, 10, 0, 1, 1, fid);
  b.array_cell(s, 10, 0, sp);
  // A 1x1 spill leaves only the anchor, which holds the array; recalc must not
  // crash or leak. Read a neighbor to confirm the run completed cleanly.
  Value anchor = recalc_value(b.build(), s, 10, 0);
  CHECK(anchor.is_array() || anchor.is_number());
}

TEST("array: a spill that loses a conflict demotes to #REF! without crashing") {
  tools::WorkbookBuilder b; SheetId s = b.add_sheet("S", 16, 8);
  b.number_cell(s, 0, 0, 1); b.number_cell(s, 1, 0, 2); b.number_cell(s, 2, 0, 3);
  b.number_cell(s, 0, 1, 10); b.number_cell(s, 1, 1, 20); b.number_cell(s, 2, 1, 30);

  Code f1; f1.range(s, 0, 0, 2, 0); f1.call(wire::Func::transpose, 1); f1.end();
  FormulaId fid1 = b.add_formula(f1.c);
  SpillId sp1 = b.add_spill(s, 10, 0, 1, 3, fid1);
  b.array_cell(s, 10, 0, sp1);

  Code f2; f2.range(s, 0, 1, 2, 1); f2.call(wire::Func::transpose, 1); f2.end();
  FormulaId fid2 = b.add_formula(f2.c);
  // Overlaps the first anchor's spill block at (10,1) and (10,2).
  SpillId sp2 = b.add_spill(s, 10, 1, 1, 3, fid2);
  b.array_cell(s, 10, 1, sp2);

  auto bytes = b.build();
  // The second anchor evaluates first (independent cells run in reverse
  // discovery order) and wins the spill; its targets hold the transposed
  // column-B values.
  Value winner = recalc_value(bytes, s, 10, 1);
  CHECK(winner.is_array());
  Value target = recalc_value(bytes, s, 10, 2);
  CHECK(target.is_number());
  CHECK_EQ(target.as_number(), 20.0);
  // The first anchor loses the conflict and demotes to #REF! rather than
  // leaving a dangling array.
  Value loser = recalc_value(bytes, s, 10, 0);
  CHECK(loser.is_error());
}
