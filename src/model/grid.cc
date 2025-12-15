// slate/model/grid.cc
#include "slate/model/grid.hh"

namespace slate {

CellId Grid::add(const CellRef& ref, wire::CellTag kind) {
  Key key{ref.sheet, ref.row, ref.col};
  if (auto it = index_.find(key); it != index_.end()) {
    // Update kind in-place; cell id remains stable.
    CellId id = it->second;
    cells_[id].kind = kind;
    return id;
  }
  CellId id = static_cast<CellId>(cells_.size());
  CellSlot slot;
  slot.ref = ref;
  slot.kind = kind;
  cells_.push_back(slot);
  index_.emplace(key, id);
  return id;
}

CellId Grid::find(const CellRef& ref) const {
  Key key{ref.sheet, ref.row, ref.col};
  auto it = index_.find(key);
  return it == index_.end() ? kNoCell : it->second;
}

}  // namespace slate
