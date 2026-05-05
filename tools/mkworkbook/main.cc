// tools/mkworkbook/main.cc
//
// Small CLI around WorkbookBuilder. With no arguments it writes a representative
// sample workbook to stdout; `mkworkbook <out.slw>` writes it to a file. The
// sample is also the shape used for parse-harness seeds.
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "builder.hh"
#include "slate/format/wire.hh"

namespace {

// Build a small but structurally-complete workbook: two sheets, a shared-string
// page, a mix of cell kinds, one formula, a defined name, a spill, an xref edge,
// and a recalc directive. Enough to drive every section decoder.
std::vector<std::uint8_t> sample_workbook() {
  using namespace slate;
  using slate::wire::Op;
  tools::WorkbookBuilder b;

  SheetId s0 = b.add_sheet("Summary", 32, 8);
  SheetId s1 = b.add_sheet("Data", 64, 4);

  // Formula: =Data!A1 + 2   (push_ref, load, push_num 2, add, end)
  std::vector<std::uint8_t> code;
  auto emit_u32 = [&](std::uint32_t v) {
    for (int i = 0; i < 4; ++i) code.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
  };
  auto emit_f64 = [&](double d) {
    std::uint64_t bits; __builtin_memcpy(&bits, &d, 8);
    for (int i = 0; i < 8; ++i) code.push_back(static_cast<std::uint8_t>(bits >> (8 * i)));
  };
  code.push_back(static_cast<std::uint8_t>(Op::push_ref));
  code.push_back(static_cast<std::uint8_t>(s1)); code.push_back(0);  // sheet u16
  emit_u32(0); emit_u32(0);                                          // row,col
  code.push_back(static_cast<std::uint8_t>(Op::load));
  code.push_back(static_cast<std::uint8_t>(Op::push_num)); emit_f64(2.0);
  code.push_back(static_cast<std::uint8_t>(Op::add));
  code.push_back(static_cast<std::uint8_t>(Op::end));
  FormulaId f0 = b.add_formula(code);

  CellId data_a1 = b.number_cell(s1, 0, 0, 40.0);
  b.text_cell(s1, 1, 0, "widgets");
  b.bool_cell(s1, 2, 0, true);
  CellId sum = b.formula_cell(s0, 0, 0, f0);
  b.text_cell(s0, 1, 0, "Total");

  b.add_name_cell("Answer", s0, 0, 0);
  b.add_name_range("Inputs", s1, 0, 0, 2, 0);

  // A tiny array formula spilling 2x1 from Data!C1.
  FormulaId af = b.add_formula(code);
  SpillId sp = b.add_spill(s1, 0, 2, 2, 1, af);
  b.array_cell(s1, 0, 2, sp);

  b.add_edge(sum, data_a1);  // Summary!A1 depends on Data!A1
  b.add_directive(wire::Directive::set_dirty, data_a1, 0);
  b.add_directive(wire::Directive::recalc_pass, 1, 0);

  return b.build();
}

}  // namespace

int main(int argc, char** argv) {
  std::vector<std::uint8_t> bytes = sample_workbook();
  const char* path = (argc > 1) ? argv[1] : nullptr;
  std::FILE* out = path ? std::fopen(path, "wb") : stdout;
  if (!out) {
    std::fprintf(stderr, "mkworkbook: cannot open %s\n", path);
    return 1;
  }
  std::fwrite(bytes.data(), 1, bytes.size(), out);
  if (path) std::fclose(out);
  std::fprintf(stderr, "mkworkbook: wrote %zu bytes\n", bytes.size());
  return 0;
}
