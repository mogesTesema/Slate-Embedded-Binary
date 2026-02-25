// slate/recalc/teardown.hh
//
// End-of-recalc drain. Teardown releases resources in a fixed order so every
// handle is given up exactly once: array spill blocks first (their sole owner is
// the spill record), then the string/array handle each cell value still holds,
// then defined-name label borrows, and finally any shared-string pages that no
// longer have live borrows. The ordering matters — releasing cell handles before
// the arrays they might alias, or recycling a page before its borrows are
// dropped, would either double-free or strand a reference.
#ifndef SLATE_RECALC_TEARDOWN_HH
#define SLATE_RECALC_TEARDOWN_HH

#include <cstdint>
#include <deque>
#include <vector>

#include "slate/eval/array_store.hh"
#include "slate/eval/string_interner.hh"
#include "slate/format/raw.hh"
#include "slate/ids.hh"
#include "slate/model/workbook.hh"
#include "slate/recalc/epoch.hh"

namespace slate {

// Ownership record for one array-formula spill: the anchor cell, the array
// backing handle it produced, and whether that handle has already been released
// (e.g. by demotion on a spill conflict).
struct SpillRecord {
  CellId anchor = kNoCell;
  std::uint32_t array = 0;
  bool released = false;
};

class Teardown {
 public:
  Teardown(Workbook& wb, StringInterner& strings, ArrayStore& arrays,
           RecalcEpoch& epoch)
      : wb_(wb), strings_(strings), arrays_(arrays), epoch_(epoch) {}

  // Run the ordered drain. Returns the number of SST pages recycled.
  std::uint32_t run(std::deque<SpillRecord>& spills);

 private:
  void release_spill_blocks(std::deque<SpillRecord>& spills);
  void release_cell_handles();
  void release_name_borrows();

  Workbook& wb_;
  StringInterner& strings_;
  ArrayStore& arrays_;
  RecalcEpoch& epoch_;
};

}  // namespace slate

#endif  // SLATE_RECALC_TEARDOWN_HH
