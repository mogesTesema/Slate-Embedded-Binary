// slate/model/reference.hh
//
// Cell and range references in (sheet, row, col) space, plus A1 <-> numeric
// helpers used by the tools layer and diagnostics. Rows and columns are
// zero-based internally; A1 text is one-based with letter columns.
#ifndef SLATE_MODEL_REFERENCE_HH
#define SLATE_MODEL_REFERENCE_HH

#include <cstdint>
#include <string>

#include "slate/ids.hh"

namespace slate {

struct CellRef {
  SheetId sheet = 0;
  std::uint32_t row = 0;
  std::uint32_t col = 0;

  bool operator==(const CellRef& o) const {
    return sheet == o.sheet && row == o.row && col == o.col;
  }
};

struct RangeRef {
  SheetId sheet = 0;
  std::uint32_t r0 = 0, c0 = 0, r1 = 0, c1 = 0;

  std::uint64_t area() const {
    if (r1 < r0 || c1 < c0) return 0;
    return static_cast<std::uint64_t>(r1 - r0 + 1) *
           static_cast<std::uint64_t>(c1 - c0 + 1);
  }
  bool contains(const CellRef& c) const {
    return c.sheet == sheet && c.row >= r0 && c.row <= r1 && c.col >= c0 &&
           c.col <= c1;
  }
  bool overlaps(const RangeRef& o) const {
    if (sheet != o.sheet) return false;
    return !(o.c0 > c1 || o.c1 < c0 || o.r0 > r1 || o.r1 < r0);
  }
  CellRef anchor() const { return CellRef{sheet, r0, c0}; }
  // Normalize so r0<=r1 and c0<=c1.
  RangeRef normalized() const {
    RangeRef n = *this;
    if (n.r1 < n.r0) std::swap(n.r0, n.r1);
    if (n.c1 < n.c0) std::swap(n.c0, n.c1);
    return n;
  }
};

// Column index (0-based) -> spreadsheet letters ("A", "Z", "AA", ...).
std::string column_label(std::uint32_t col);

// A1 text for a cell, e.g. {0, 3, 2} -> "C4" (sheet is not encoded here).
std::string a1(const CellRef& ref);

// Parse "C4" style text into row/col (0-based). Returns false on malformed
// input. Sheet-qualified references are the caller's responsibility.
bool parse_a1(const std::string& text, std::uint32_t& row, std::uint32_t& col);

}  // namespace slate

#endif  // SLATE_MODEL_REFERENCE_HH
