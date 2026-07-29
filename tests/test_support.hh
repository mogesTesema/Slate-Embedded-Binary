// tests/test_support.hh
//
// Shared helpers for the recalc/function tests: a small formula-bytecode emitter
// and a routine that builds, loads, and recalculates a workbook and reads a
// resulting cell value. (Text results are released at teardown, so tests assert
// on numeric/boolean/error results — wrap text in LEN/FIND to check text paths.)
#ifndef SLATE_TESTS_TEST_SUPPORT_HH
#define SLATE_TESTS_TEST_SUPPORT_HH

#include <cstring>
#include <vector>

#include "builder.hh"
#include "slate/model/workbook.hh"
#include "slate/recalc/driver.hh"
#include "slate/slate.hh"

namespace slate::test {

struct Code {
  std::vector<std::uint8_t> c;
  void op(wire::Op o) { c.push_back(static_cast<std::uint8_t>(o)); }
  void u16(std::uint16_t v) { c.push_back(v & 0xFF); c.push_back(v >> 8); }
  void u32(std::uint32_t v) { for (int i = 0; i < 4; ++i) c.push_back(v >> (8 * i)); }
  void f64(double d) { std::uint64_t x; std::memcpy(&x, &d, 8); for (int i = 0; i < 8; ++i) c.push_back(x >> (8 * i)); }
  void num(double d) { op(wire::Op::push_num); f64(d); }
  void str(StrId id) { op(wire::Op::push_str); u32(id); }
  void ref(SheetId s, std::uint32_t r, std::uint32_t col) { op(wire::Op::push_ref); u16(s); u32(r); u32(col); op(wire::Op::load); }
  void range(SheetId s, std::uint32_t r0, std::uint32_t c0, std::uint32_t r1, std::uint32_t c1) {
    op(wire::Op::push_range); u16(s); u32(r0); u32(c0); u32(r1); u32(c1);
  }
  void call(wire::Func f, std::uint8_t argc) { op(wire::Op::call); u16(static_cast<std::uint16_t>(f)); c.push_back(argc); }
  void end() { op(wire::Op::end); }
};

// Recalc a built workbook and return the (post-recalc) value of a cell.
inline Value recalc_value(const std::vector<std::uint8_t>& bytes, SheetId s,
                          std::uint32_t row, std::uint32_t col) {
  Workbook wb;
  DiagnosticSink d;
  if (!ok(load_workbook(bytes.data(), bytes.size(), wb, d))) return Value();
  RecalcDriver drv(wb);
  DiagnosticSink d2;
  if (!ok(drv.run(d2))) return Value();
  CellId id = wb.grid.find(CellRef{s, row, col});
  return id == kNoCell ? Value() : wb.grid.at(id).value;
}

// Convenience for numeric assertions.
inline double recalc_number(const std::vector<std::uint8_t>& bytes, SheetId s,
                            std::uint32_t row, std::uint32_t col, bool& is_num) {
  Value v = recalc_value(bytes, s, row, col);
  is_num = v.is_number();
  return v.is_number() ? v.as_number() : 0.0;
}

}  // namespace slate::test

#endif  // SLATE_TESTS_TEST_SUPPORT_HH
