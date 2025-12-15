// slate/model/grid.hh
//
// The cell grid: a dense array of cell slots addressed by CellId, with a lookup
// from (sheet,row,col) back to CellId for reference resolution. Array-formula
// results occupy an anchor slot plus a set of borrowed spill-target slots; the
// grid tracks that relationship so teardown can release a spill exactly once.
#ifndef SLATE_MODEL_GRID_HH
#define SLATE_MODEL_GRID_HH

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "slate/format/wire.hh"
#include "slate/ids.hh"
#include "slate/model/reference.hh"
#include "slate/model/value.hh"

namespace slate {

struct CellSlot {
  CellRef ref;
  wire::CellTag kind = wire::CellTag::empty;
  Value value;
  FormulaId formula = kNoFormula;

  // Array-spill bookkeeping. An anchor has spill != kNoSpill; a borrowed target
  // has spill_anchor != kNoCell and points back to its anchor cell.
  SpillId spill = kNoSpill;
  CellId spill_anchor = kNoCell;
  bool is_spill_target = false;

  bool dirty = false;

  bool is_anchor() const { return spill != kNoSpill; }
};

class Grid {
 public:
  CellId add(const CellRef& ref, wire::CellTag kind);

  std::size_t size() const { return cells_.size(); }
  bool in_range(CellId id) const { return id < cells_.size(); }
  CellSlot& at(CellId id) { return cells_[id]; }
  const CellSlot& at(CellId id) const { return cells_[id]; }

  // Resolve a reference to a CellId, or kNoCell if no cell exists there.
  CellId find(const CellRef& ref) const;

  void set_value(CellId id, const Value& v) {
    if (in_range(id)) cells_[id].value = v;
  }
  void mark_dirty(CellId id) {
    if (in_range(id)) cells_[id].dirty = true;
  }

 private:
  struct Key {
    SheetId sheet;
    std::uint32_t row;
    std::uint32_t col;
    bool operator==(const Key& o) const {
      return sheet == o.sheet && row == o.row && col == o.col;
    }
  };
  struct KeyHash {
    std::size_t operator()(const Key& k) const {
      std::uint64_t h = (static_cast<std::uint64_t>(k.sheet) << 1) ^
                        (static_cast<std::uint64_t>(k.row) * 0x9E3779B97F4A7C15ull) ^
                        (static_cast<std::uint64_t>(k.col) * 0xC2B2AE3D27D4EB4Full);
      return static_cast<std::size_t>(h ^ (h >> 29));
    }
  };

  std::vector<CellSlot> cells_;
  std::unordered_map<Key, CellId, KeyHash> index_;
};

}  // namespace slate

#endif  // SLATE_MODEL_GRID_HH
