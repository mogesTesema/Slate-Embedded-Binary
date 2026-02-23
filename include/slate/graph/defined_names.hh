// slate/graph/defined_names.hh
//
// Resolves defined names to concrete ranges and caches the resolution so repeat
// lookups during evaluation are cheap. A cached entry also keeps the owning
// sheet's backing pointer. The pointer may become stale if the owning sheet is
// recycled; use the liveness check before dereferencing.
#ifndef SLATE_GRAPH_DEFINED_NAMES_HH
#define SLATE_GRAPH_DEFINED_NAMES_HH

#include <cstdint>
#include <string_view>
#include <vector>

#include "slate/ids.hh"
#include "slate/model/reference.hh"
#include "slate/model/sheet.hh"
#include "slate/model/workbook.hh"
#include "slate/util/status.hh"

namespace slate {

class NameResolver {
 public:
  struct Entry {
    StrId name = kNoStr;
    RangeRef range;
    SheetData* cached = nullptr;  // resolved at build time; may go stale
  };

  Status build(Workbook& wb);

  std::size_t size() const { return entries_.size(); }
  const Entry* by_id(NameId id) const {
    return id < entries_.size() ? &entries_[id] : nullptr;
  }
  const Entry* by_name(std::string_view name, const Workbook& wb) const;

  // Hot-path accessor for the backing a name resolved to. Returns the pointer
  // cached at build time; used where the resolution was just established and a
  // table re-lookup would be redundant.
  const SheetData* backing(NameId id) const {
    return id < entries_.size() ? entries_[id].cached : nullptr;
  }

  // Safe accessor: returns the sheet backing only if the sheet is still live,
  // refreshing the cached pointer from the table. Returns nullptr otherwise.
  SheetData* live_data(NameId id, SheetTable& sheets);

 private:
  std::vector<Entry> entries_;
};

}  // namespace slate

#endif  // SLATE_GRAPH_DEFINED_NAMES_HH
