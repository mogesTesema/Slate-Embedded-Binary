// slate/model/sheet.hh
//
// Sheets and the sheet table. A sheet owns its dimensions and a live/visible
// state. During incremental recalculation a sheet can be recycled (its backing
// region released and generation bumped); anything that cached a resolved
// pointer into a sheet must re-check liveness before dereferencing it.
#ifndef SLATE_MODEL_SHEET_HH
#define SLATE_MODEL_SHEET_HH

#include <cstdint>
#include <memory>
#include <vector>

#include "slate/ids.hh"

namespace slate {

// Per-sheet backing that name resolution may hold a handle to. Kept behind a
// unique_ptr so recycling actually frees it (and so a stale cached pointer is a
// real dangling pointer, which the liveness check exists to prevent).
struct SheetData {
  std::uint32_t rows = 0;
  std::uint32_t cols = 0;
  std::uint32_t generation = 0;
};

struct Sheet {
  StrId name = kNoStr;
  bool visible = true;
  bool live = true;
  std::unique_ptr<SheetData> data;
};

class SheetTable {
 public:
  SheetId add(StrId name, std::uint32_t rows, std::uint32_t cols, bool visible);

  std::size_t size() const { return sheets_.size(); }
  bool in_range(SheetId id) const { return id < sheets_.size(); }

  Sheet& at(SheetId id) { return sheets_[id]; }
  const Sheet& at(SheetId id) const { return sheets_[id]; }

  bool is_live(SheetId id) const { return in_range(id) && sheets_[id].live; }
  bool is_visible(SheetId id) const {
    return in_range(id) && sheets_[id].visible;
  }
  void set_visible(SheetId id, bool v) {
    if (in_range(id)) sheets_[id].visible = v;
  }

  // Borrow a sheet's backing, or nullptr if the sheet is not live.
  SheetData* data(SheetId id) {
    if (!is_live(id)) return nullptr;
    return sheets_[id].data.get();
  }

  // Recycle: free the backing and mark the sheet not-live. The slot remains so
  // ids stay stable, but the sheet is no longer usable until rebuilt.
  void recycle(SheetId id);

 private:
  std::vector<Sheet> sheets_;
};

}  // namespace slate

#endif  // SLATE_MODEL_SHEET_HH
