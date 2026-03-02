// slate/recalc/epoch.cc
#include "slate/recalc/epoch.hh"

namespace slate {

bool RecalcEpoch::try_recycle_page(std::uint32_t page) {
  if (page >= wb_.sst_pages.size()) return false;
  RawSstPage& p = wb_.sst_pages[page];
  if (!p.recyclable) return false;
  // Only reclaim when nothing still borrows a string on this page. The interner
  // tracks per-handle borrows; a nonzero count means a live cell or name still
  // resolves through this page.
  if (strings_.page_has_live_borrows(page)) return false;

  p.bytes.clear();
  p.bytes.shrink_to_fit();
  p.slots.clear();
  p.slots.shrink_to_fit();
  return true;
}

std::uint32_t RecalcEpoch::recycle_unused_pages() {
  std::uint32_t freed = 0;
  for (std::uint32_t i = 0; i < wb_.sst_pages.size(); ++i)
    if (try_recycle_page(i)) ++freed;
  return freed;
}

}  // namespace slate
