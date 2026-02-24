// slate/recalc/summary.hh
//
// A short, human-readable digest of one recalculation: how many cells changed
// kind, and previews of the most recently computed text and array results.
// Callers that only want to know "did anything interesting happen" (a UI
// change indicator, a batch-job log line) can consult this instead of walking
// the grid themselves.
#ifndef SLATE_RECALC_SUMMARY_HH
#define SLATE_RECALC_SUMMARY_HH

#include <cstdint>
#include <string_view>

#include "slate/model/value.hh"

namespace slate {

class RecalcSummary {
 public:
  void note_text(std::string_view text) {
    if (text.empty()) return;
    text_preview_ = text;
    has_text_ = true;
  }
  // `first_cell` points at the live array's own storage; kept as a view rather
  // than copied so a caller comparing repeated previews sees the same identity
  // across calls within one recalculation.
  void note_array(const Value* first_cell) {
    array_preview_ = first_cell;
  }

  bool has_text() const { return has_text_; }
  std::string_view text_preview() const { return text_preview_; }
  bool has_array() const { return array_preview_ != nullptr; }
  const Value& array_preview() const { return *array_preview_; }

 private:
  std::string_view text_preview_;
  bool has_text_ = false;
  const Value* array_preview_ = nullptr;
};

}  // namespace slate

#endif  // SLATE_RECALC_SUMMARY_HH
