// slate/recalc/teardown.cc
#include "slate/recalc/teardown.hh"

namespace slate {

void Teardown::release_spill_blocks(std::vector<SpillRecord>& spills) {
  // Release spill-array blocks. Check the released flag to avoid redundant
  // releases.
  for (SpillRecord& rec : spills) {
    if (!rec.released) {
      arrays_.release(rec.array);
      rec.released = true;
    }
  }
}

void Teardown::release_cell_handles() {
  // Text handles and arrays produced by ordinary formulas are owned by the cell.
  // Array anchors (spill != kNoSpill) are owned by their spill record and were
  // released above, so they are skipped here to avoid a double release.
  for (CellId id = 0; id < wb_.grid.size(); ++id) {
    CellSlot& slot = wb_.grid.at(id);
    Value& v = slot.value;
    if (v.is_text()) {
      strings_.release(v.as_text());
      v.clear();
    } else if (v.is_array() && slot.spill == kNoSpill) {
      arrays_.release(v.as_array());
      v.clear();
    }
  }
}

void Teardown::release_name_borrows() {
  for (const RawName& n : wb_.names)
    if (n.name != kNoStr) strings_.release(n.name);
}

std::uint32_t Teardown::run(std::vector<SpillRecord>& spills) {
  release_spill_blocks(spills);
  release_cell_handles();
  release_name_borrows();
  // Only now, with every borrow dropped, is it safe to reclaim unused pages.
  return epoch_.recycle_unused_pages();
}

}  // namespace slate
