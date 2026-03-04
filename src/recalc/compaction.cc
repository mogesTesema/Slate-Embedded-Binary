// slate/recalc/compaction.cc
#include "slate/recalc/compaction.hh"

namespace slate {

CompactionStats compact_stores(StringInterner& strings, ArrayStore& arrays) {
  CompactionStats stats;
  stats.strings_reclaimed = strings.compact();
  stats.arrays_reclaimed = arrays.compact();
  return stats;
}

}  // namespace slate
