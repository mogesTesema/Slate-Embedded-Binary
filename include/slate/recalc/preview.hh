// slate/recalc/preview.hh
//
// Captures a lightweight preview of the most recently evaluated text and array
// results during a recalculation pass. The preview is read once at the end of
// RecalcDriver::run() to populate RecalcStats, then released.
//
// The tracker holds borrows on the backing stores it references so that the
// previewed data survives until release_borrows() is called. Text previews
// borrow the interned string handle; array previews track the handle and
// resolve the first cell on demand.
#ifndef SLATE_RECALC_PREVIEW_HH
#define SLATE_RECALC_PREVIEW_HH

#include <cstdint>
#include <string_view>
#include <vector>

#include "slate/eval/array_store.hh"
#include "slate/eval/string_interner.hh"
#include "slate/model/value.hh"

namespace slate {

class PreviewTracker {
 public:
  PreviewTracker(StringInterner& strings, ArrayStore& arrays)
      : strings_(strings), arrays_(arrays) {}

  // Record the text preview from the most recently evaluated cell.
  void note_text(const Value& v) {
    if (!v.is_text()) return;
    std::uint32_t h = v.as_text();
    if (strings_.is_static(h)) return;
    if (has_text_ && h == text_handle_) return;
    if (has_text_) strings_.release(text_handle_);
    strings_.retain(h);
    text_handle_ = h;
    has_text_ = true;
  }

  // Record the array preview. Keeps a raw pointer into the array's backing
  // cell vector so the caller can copy the first element once the pass is
  // complete. The handle is tracked for release.
  void note_array(std::uint32_t handle) {
    const ArrayValue* a = arrays_.get(handle);
    if (!a || a->cells.empty()) return;
    // Point directly at the first cell of the live array.
    array_cell_ = &a->cells[0];
    array_handle_ = handle;
  }

  std::string_view text() const {
    if (!has_text_) return {};
    return strings_.resolve(text_handle_);
  }

  const Value* array_cell() const { return array_cell_; }

  void release_borrows() {
    if (has_text_) {
      strings_.release(text_handle_);
      has_text_ = false;
    }
    // Note: no retain was taken on the array handle. The array's lifetime is
    // governed by the SpillRecord that owns it; the preview merely observes.
    array_cell_ = nullptr;
  }

 private:
  StringInterner& strings_;
  ArrayStore& arrays_;

  std::uint32_t text_handle_ = 0;
  bool has_text_ = false;

  const Value* array_cell_ = nullptr;    // raw interior pointer — no borrow
  std::uint32_t array_handle_ = 0;       // tracked for diagnostics only
};

}  // namespace slate

#endif  // SLATE_RECALC_PREVIEW_HH
