// slate/format/raw.hh
//
// The decoder's output: a structurally-validated but not-yet-linked view of a
// workbook. Higher layers (model, graph, compile) consume this. Keeping decode
// separate from linking means the parser can be fuzzed on its own and the model
// layer never touches raw bytes.
#ifndef SLATE_FORMAT_RAW_HH
#define SLATE_FORMAT_RAW_HH

#include <cstdint>
#include <string>
#include <vector>

#include "slate/format/wire.hh"
#include "slate/ids.hh"

namespace slate {

struct RawSheet {
  StrId name = kNoStr;
  std::uint32_t rows = 0;
  std::uint32_t cols = 0;
  std::uint16_t flags = 0;
  bool visible() const { return (flags & wire::kSheetVisible) != 0; }
};

// One decoded shared-string page. `bytes` is the fully-expanded page buffer;
// `slots` indexes strings within it. `recyclable` mirrors the page flag and is
// what the recalc epoch consults when reclaiming string storage.
struct RawSstPage {
  bool recyclable = false;
  std::uint32_t declared_size = 0;         // decoded_size from the page header
  std::vector<std::uint8_t> bytes;         // expanded page contents
  struct Slot { std::uint32_t offset; std::uint32_t length; };
  std::vector<Slot> slots;
};

struct RawCell {
  SheetId sheet = 0;
  std::uint32_t row = 0;
  std::uint32_t col = 0;
  wire::CellTag tag = wire::CellTag::empty;
  // Overlapping payload interpreted by tag.
  double num = 0.0;
  StrId str = kNoStr;
  std::uint8_t boolean = 0;
  std::uint8_t err = 0;
  FormulaId formula = kNoFormula;
  SpillId spill = kNoSpill;
};

struct RawFormula {
  std::vector<std::uint8_t> code;  // raw bytecode, verified later by compile/
};

struct RawName {
  StrId name = kNoStr;
  wire::NameKind kind = wire::NameKind::cell;
  SheetId sheet = 0;
  std::uint32_t r0 = 0, c0 = 0, r1 = 0, c1 = 0;
};

struct RawSpill {
  SheetId sheet = 0;
  std::uint32_t row = 0, col = 0;   // anchor
  std::uint32_t rows = 0, cols = 0;
  FormulaId source = kNoFormula;
};

struct RawEdge {
  CellId from = kNoCell;  // dependent cell ordinal
  CellId to = kNoCell;    // precedent cell ordinal
};

struct RawDirective {
  wire::Directive op = wire::Directive::recalc_pass;
  std::uint32_t arg0 = 0;
  std::uint32_t arg1 = 0;
};

// A merged cell region. The top-left cell (r0,c0) is the anchor; the rest of the
// region borrows the anchor's value when read.
struct RawMerge {
  SheetId sheet = 0;
  std::uint32_t r0 = 0, c0 = 0, r1 = 0, c1 = 0;
};

// The complete decoded workbook. `string_dir` is a flat directory built from all
// SST pages: StrId -> (page, slot). Resolving a StrId returns bytes from the
// owning page, which matters once pages become recyclable during recalc.
struct RawWorkbook {
  std::uint16_t version = 0;
  std::uint16_t flags = 0;

  std::vector<RawSheet> sheets;
  std::vector<RawSstPage> sst_pages;
  struct StrRef { std::uint32_t page; std::uint32_t slot; };
  std::vector<StrRef> string_dir;   // StrId -> location

  std::vector<RawCell> cells;
  std::vector<RawFormula> formulas;
  std::vector<RawName> names;
  std::vector<RawSpill> spills;
  std::vector<RawEdge> edges;
  std::vector<RawDirective> directives;
  std::vector<RawMerge> merges;

  std::uint32_t string_count() const {
    return static_cast<std::uint32_t>(string_dir.size());
  }
};

}  // namespace slate

#endif  // SLATE_FORMAT_RAW_HH
