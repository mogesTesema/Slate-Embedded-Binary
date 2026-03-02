// slate/recalc/epoch.hh
//
// Per-recalc resource management: the scratch arena used for array
// materialization and the recycling of shared-string pages between passes. A
// recyclable SST page may be reclaimed only when no live static handle still
// borrows a string on it — the interner is the authority on that. The arena is
// wound back to a marker at the end of each pass so transient array scratch does
// not accumulate.
#ifndef SLATE_RECALC_EPOCH_HH
#define SLATE_RECALC_EPOCH_HH

#include <cstdint>

#include "slate/eval/string_interner.hh"
#include "slate/model/workbook.hh"
#include "slate/util/arena.hh"

namespace slate {

class RecalcEpoch {
 public:
  RecalcEpoch(Workbook& wb, StringInterner& strings)
      : wb_(wb), strings_(strings) {}

  std::uint32_t epoch() const { return epoch_; }
  void advance() { ++epoch_; }

  Arena& arena() { return arena_; }
  ArenaMarker mark() const { return arena_.mark(); }
  void rewind(const ArenaMarker& m) { arena_.rewind(m); }

  // Reclaim a recyclable page's backing bytes when it holds no live borrows.
  // Returns true if the page was freed.
  bool try_recycle_page(std::uint32_t page);

  // Sweep every recyclable page after a pass; returns the number freed.
  std::uint32_t recycle_unused_pages();

 private:
  Workbook& wb_;
  StringInterner& strings_;
  Arena arena_;
  std::uint32_t epoch_ = 0;
};

}  // namespace slate

#endif  // SLATE_RECALC_EPOCH_HH
