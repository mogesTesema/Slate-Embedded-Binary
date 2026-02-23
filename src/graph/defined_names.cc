// slate/graph/defined_names.cc
#include "slate/graph/defined_names.hh"

namespace slate {

Status NameResolver::build(Workbook& wb) {
  entries_.clear();
  entries_.reserve(wb.names.size());
  for (const RawName& n : wb.names) {
    Entry e;
    e.name = n.name;
    e.range = RangeRef{n.sheet, n.r0, n.c0, n.r1, n.c1}.normalized();
    // Cache the current backing pointer; live_data re-checks before use.
    e.cached = wb.sheets.data(n.sheet);
    entries_.push_back(e);
  }
  return Status::ok;
}

const NameResolver::Entry* NameResolver::by_name(std::string_view name,
                                                 const Workbook& wb) const {
  for (const Entry& e : entries_) {
    if (wb.resolve_string(e.name) == name) return &e;
  }
  return nullptr;
}

SheetData* NameResolver::live_data(NameId id, SheetTable& sheets) {
  if (id >= entries_.size()) return nullptr;
  Entry& e = entries_[id];
  SheetId sid = e.range.sheet;
  // The cached pointer is only trustworthy while the sheet is live; refresh it
  // from the table so a recycled sheet yields nullptr instead of a dangling ref.
  if (!sheets.is_live(sid)) {
    e.cached = nullptr;
    return nullptr;
  }
  e.cached = sheets.data(sid);
  return e.cached;
}

}  // namespace slate
