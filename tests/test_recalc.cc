// tests/test_recalc.cc
#include "framework.hh"

#include <cstring>
#include <vector>

#include "builder.hh"
#include "slate/model/workbook.hh"
#include "slate/recalc/driver.hh"
#include "slate/slate.hh"

using namespace slate;

namespace {

// Tiny bytecode emitter for building test formulas.
struct Code {
  std::vector<std::uint8_t> c;
  void op(wire::Op o) { c.push_back(static_cast<std::uint8_t>(o)); }
  void u16(std::uint16_t v) { c.push_back(v & 0xFF); c.push_back(v >> 8); }
  void u32(std::uint32_t v) { for (int i = 0; i < 4; ++i) c.push_back(v >> (8 * i)); }
  void f64(double d) { std::uint64_t x; std::memcpy(&x, &d, 8); for (int i = 0; i < 8; ++i) c.push_back(x >> (8 * i)); }
  void ref(SheetId s, std::uint32_t r, std::uint32_t col) { op(wire::Op::push_ref); u16(s); u32(r); u32(col); op(wire::Op::load); }
  void num(double d) { op(wire::Op::push_num); f64(d); }
  void range(SheetId s, std::uint32_t r0, std::uint32_t c0, std::uint32_t r1, std::uint32_t c1) {
    op(wire::Op::push_range); u16(s); u32(r0); u32(c0); u32(r1); u32(c1);
  }
  void call(wire::Func f, std::uint8_t argc) { op(wire::Op::call); u16(static_cast<std::uint16_t>(f)); c.push_back(argc); }
  void end() { op(wire::Op::end); }
};

// Recalc a workbook and read back a numeric cell value.
double eval_cell(const std::vector<std::uint8_t>& bytes, SheetId s,
                 std::uint32_t row, std::uint32_t col, bool& ok_out) {
  Workbook wb;
  DiagnosticSink d;
  ok_out = false;
  if (!ok(load_workbook(bytes.data(), bytes.size(), wb, d))) return 0;
  RecalcDriver drv(wb);
  DiagnosticSink d2;
  if (!ok(drv.run(d2))) return 0;
  CellId id = wb.grid.find(CellRef{s, row, col});
  if (id == kNoCell) return 0;
  const Value& v = wb.grid.at(id).value;
  ok_out = v.is_number();
  return v.is_number() ? v.as_number() : 0;
}

}  // namespace

TEST("recalc: addition of two cells") {
  tools::WorkbookBuilder b;
  SheetId s = b.add_sheet("S", 8, 8);
  b.number_cell(s, 0, 0, 10.0);
  b.number_cell(s, 1, 0, 20.0);
  Code f; f.ref(s, 0, 0); f.ref(s, 1, 0); f.op(wire::Op::add); f.end();
  FormulaId fid = b.add_formula(f.c);
  b.formula_cell(s, 0, 1, fid);

  bool ok_out;
  double v = eval_cell(b.build(), s, 0, 1, ok_out);
  CHECK(ok_out);
  CHECK_EQ(v, 30.0);
}

TEST("recalc: SUM over a range") {
  tools::WorkbookBuilder b;
  SheetId s = b.add_sheet("S", 8, 8);
  b.number_cell(s, 0, 0, 1.0);
  b.number_cell(s, 1, 0, 2.0);
  b.number_cell(s, 2, 0, 3.0);
  Code f; f.range(s, 0, 0, 2, 0); f.call(wire::Func::sum, 1); f.end();
  FormulaId fid = b.add_formula(f.c);
  b.formula_cell(s, 0, 1, fid);

  bool ok_out;
  double v = eval_cell(b.build(), s, 0, 1, ok_out);
  CHECK(ok_out);
  CHECK_EQ(v, 6.0);
}

TEST("recalc: IF chooses a branch") {
  tools::WorkbookBuilder b;
  SheetId s = b.add_sheet("S", 8, 8);
  b.number_cell(s, 0, 0, 5.0);
  b.number_cell(s, 1, 0, 3.0);
  Code f;
  f.ref(s, 0, 0); f.ref(s, 1, 0); f.op(wire::Op::gt);
  f.num(100.0); f.num(200.0); f.call(wire::Func::if_, 3); f.end();
  FormulaId fid = b.add_formula(f.c);
  b.formula_cell(s, 0, 1, fid);

  bool ok_out;
  double v = eval_cell(b.build(), s, 0, 1, ok_out);
  CHECK(ok_out);
  CHECK_EQ(v, 100.0);
}

TEST("recalc: division by zero yields an error, not a crash") {
  tools::WorkbookBuilder b;
  SheetId s = b.add_sheet("S", 8, 8);
  b.number_cell(s, 0, 0, 1.0);
  b.number_cell(s, 1, 0, 0.0);
  Code f; f.ref(s, 0, 0); f.ref(s, 1, 0); f.op(wire::Op::divide); f.end();
  FormulaId fid = b.add_formula(f.c);
  b.formula_cell(s, 0, 1, fid);

  bool ok_out;
  eval_cell(b.build(), s, 0, 1, ok_out);
  CHECK(!ok_out);  // result is an error value, not a number
}

TEST("recalc: chained formula through a dependency edge") {
  tools::WorkbookBuilder b;
  SheetId s = b.add_sheet("S", 8, 8);
  CellId a1 = b.number_cell(s, 0, 0, 7.0);
  // B1 = A1 * 2
  Code f1; f1.ref(s, 0, 0); f1.num(2.0); f1.op(wire::Op::mul); f1.end();
  CellId b1 = b.formula_cell(s, 0, 1, b.add_formula(f1.c));
  // C1 = B1 + 1
  Code f2; f2.ref(s, 0, 1); f2.num(1.0); f2.op(wire::Op::add); f2.end();
  CellId c1 = b.formula_cell(s, 0, 2, b.add_formula(f2.c));
  b.add_edge(b1, a1);  // B1 depends on A1
  b.add_edge(c1, b1);  // C1 depends on B1

  bool ok_out;
  double v = eval_cell(b.build(), s, 0, 2, ok_out);
  CHECK(ok_out);
  CHECK_EQ(v, 15.0);  // (7*2)+1
}
