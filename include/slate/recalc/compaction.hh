// slate/recalc/compaction.hh
//
// Between recalculation passes the string interner and array store accumulate
// dead slots (results of one pass overwritten by the next). Compaction reclaims
// those trailing dead slots so a long-running, multi-pass recalculation does not
// grow without bound. It only ever drops slots that are already dead and at the
// tail, so no live handle held by a cell value changes identity.
#ifndef SLATE_RECALC_COMPACTION_HH
#define SLATE_RECALC_COMPACTION_HH

#include <cstdint>

#include "slate/eval/array_store.hh"
#include "slate/eval/string_interner.hh"

namespace slate {

struct CompactionStats {
  std::uint32_t strings_reclaimed = 0;
  std::uint32_t arrays_reclaimed = 0;
};

// Compact both stores and report how many slots were reclaimed.
CompactionStats compact_stores(StringInterner& strings, ArrayStore& arrays);

}  // namespace slate

#endif  // SLATE_RECALC_COMPACTION_HH
