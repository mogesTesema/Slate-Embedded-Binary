// tools/mkworkbook/builder.hh
//
// Authoring side of the workbook format: assemble sheets, strings, cells,
// formulas, names, spills, dependency edges, and directives into a valid .slw
// byte stream (correct section table, offsets, and CRCs). This is the writer
// that mirrors src/format's reader; it is used by the mkworkbook CLI, the test
// fixtures, and the seed/PoC generators.
#ifndef SLATE_TOOLS_MKWORKBOOK_BUILDER_HH
#define SLATE_TOOLS_MKWORKBOOK_BUILDER_HH

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "slate/format/wire.hh"
#include "slate/ids.hh"
#include "slate/util/intern_table.hh"

namespace slate::tools {

class WorkbookBuilder {
 public:
  // Strings are deduplicated and land in a single SST page. `recyclable` marks
  // the page reclaimable by recalc (used when authoring recalc scenarios).
  StrId add_string(std::string_view s);
  void set_sst_recyclable(bool on) { sst_recyclable_ = on; }

  SheetId add_sheet(std::string_view name, std::uint32_t rows,
                    std::uint32_t cols, bool visible = true);

  FormulaId add_formula(const std::vector<std::uint8_t>& code);
  SpillId add_spill(SheetId sheet, std::uint32_t row, std::uint32_t col,
                    std::uint32_t rows, std::uint32_t cols, FormulaId source);

  // Cell adders return the cell's dense ordinal (its CellId), so callers can
  // wire dependency edges and directives against it.
  CellId number_cell(SheetId s, std::uint32_t row, std::uint32_t col, double v);
  CellId text_cell(SheetId s, std::uint32_t row, std::uint32_t col,
                   std::string_view v);
  CellId bool_cell(SheetId s, std::uint32_t row, std::uint32_t col, bool v);
  CellId error_cell(SheetId s, std::uint32_t row, std::uint32_t col,
                    wire::ErrCode e);
  CellId formula_cell(SheetId s, std::uint32_t row, std::uint32_t col,
                      FormulaId f);

  // Parse `text` (e.g. "=SUM(A1:A3)+B1") into bytecode and place it as a formula
  // cell on sheet `s`. Returns kNoCell if the formula does not parse. Sheet
  // names and defined names are resolved against what has been added so far.
  CellId formula_text_cell(SheetId s, std::uint32_t row, std::uint32_t col,
                           const std::string& text);
  CellId array_cell(SheetId s, std::uint32_t row, std::uint32_t col,
                    SpillId sp);

  NameId add_name_cell(std::string_view name, SheetId s, std::uint32_t row,
                       std::uint32_t col);
  NameId add_name_range(std::string_view name, SheetId s, std::uint32_t r0,
                        std::uint32_t c0, std::uint32_t r1, std::uint32_t c1);

  void add_edge(CellId from, CellId to);
  void add_directive(wire::Directive op, std::uint32_t arg0,
                     std::uint32_t arg1);
  void add_merge(SheetId sheet, std::uint32_t r0, std::uint32_t c0,
                 std::uint32_t r1, std::uint32_t c1);

  // Assemble the final byte stream.
  std::vector<std::uint8_t> build() const;

 private:
  struct BSheet { StrId name; std::uint32_t rows, cols; std::uint16_t flags; };
  struct BCell {
    SheetId sheet; std::uint32_t row, col; wire::CellTag tag;
    double num = 0; StrId str = kNoStr; std::uint8_t b = 0, err = 0;
    FormulaId formula = kNoFormula; SpillId spill = kNoSpill;
  };
  struct BName {
    StrId name; wire::NameKind kind; SheetId sheet;
    std::uint32_t r0, c0, r1, c1;
  };
  struct BSpill {
    SheetId sheet; std::uint32_t row, col, rows, cols; FormulaId source;
  };
  struct BEdge { CellId from, to; };
  struct BDirective { wire::Directive op; std::uint32_t arg0, arg1; };
  struct BMerge { SheetId sheet; std::uint32_t r0, c0, r1, c1; };

  CellId push_cell(const BCell& c);

  std::vector<std::uint8_t> encode_sst() const;
  std::vector<std::uint8_t> encode_sheets() const;
  std::vector<std::uint8_t> encode_formulas() const;
  std::vector<std::uint8_t> encode_spill() const;
  std::vector<std::uint8_t> encode_cells() const;
  std::vector<std::uint8_t> encode_names() const;
  std::vector<std::uint8_t> encode_xref() const;
  std::vector<std::uint8_t> encode_directives() const;
  std::vector<std::uint8_t> encode_merges() const;

  InternTable strings_;
  bool sst_recyclable_ = false;
  std::vector<BSheet> sheets_;
  std::vector<std::vector<std::uint8_t>> formulas_;
  std::vector<BSpill> spills_;
  std::vector<BCell> cells_;
  std::vector<BName> names_;
  std::vector<BEdge> edges_;
  std::vector<BDirective> directives_;
  std::vector<BMerge> merges_;
};

}  // namespace slate::tools

#endif  // SLATE_TOOLS_MKWORKBOOK_BUILDER_HH
