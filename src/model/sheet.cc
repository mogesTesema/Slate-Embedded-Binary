// slate/model/sheet.cc
#include "slate/model/sheet.hh"

namespace slate {

SheetId SheetTable::add(StrId name, std::uint32_t rows, std::uint32_t cols,
                        bool visible) {
  Sheet sh;
  sh.name = name;
  sh.visible = visible;
  sh.live = true;
  sh.data = std::make_unique<SheetData>();
  sh.data->rows = rows;
  sh.data->cols = cols;
  sh.data->generation = 0;
  sheets_.push_back(std::move(sh));
  return static_cast<SheetId>(sheets_.size() - 1);
}

void SheetTable::recycle(SheetId id) {
  if (!in_range(id)) return;
  Sheet& sh = sheets_[id];
  // Release the backing; bump the generation on a fresh block only when the
  // sheet is later rebuilt. Marking not-live is what makes data() refuse it.
  sh.data.reset();
  sh.live = false;
}

}  // namespace slate
