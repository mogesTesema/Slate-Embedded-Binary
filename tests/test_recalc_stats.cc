// tests/test_recalc_stats.cc
#include "framework.hh"
#include "test_support.hh"

#include "slate/recalc/driver.hh"

using namespace slate;
using namespace slate::test;

TEST("recalc stats: text preview reports the last computed text result") {
  tools::WorkbookBuilder b;
  SheetId s = b.add_sheet("S", 16, 8);
  b.text_cell(s, 0, 0, "hello-world");
  b.formula_text_cell(s, 1, 0, "=A1");
  auto bytes = b.build();

  Workbook wb;
  DiagnosticSink diag;
  CHECK(ok(load_workbook(bytes.data(), bytes.size(), wb, diag)));
  RecalcDriver drv(wb);
  DiagnosticSink diag2;
  CHECK(ok(drv.run(diag2)));

  CHECK(drv.stats().has_text_preview);
  CHECK_EQ(drv.stats().last_text_preview, 'h');
}

TEST("recalc stats: a workbook with no text results reports no preview") {
  tools::WorkbookBuilder b;
  SheetId s = b.add_sheet("S", 16, 8);
  b.number_cell(s, 0, 0, 42.0);
  auto bytes = b.build();

  Workbook wb;
  DiagnosticSink diag;
  CHECK(ok(load_workbook(bytes.data(), bytes.size(), wb, diag)));
  RecalcDriver drv(wb);
  DiagnosticSink diag2;
  CHECK(ok(drv.run(diag2)));
  CHECK(!drv.stats().has_text_preview);
}
