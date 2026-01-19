// slate/eval/string_interner.cc
#include "slate/eval/string_interner.hh"

namespace slate {

StringInterner::StringInterner(const Workbook& wb)
    : wb_(wb), static_count_(wb.string_count()) {
  static_borrows_.assign(static_count_, 0);
}

std::string_view StringInterner::resolve(std::uint32_t handle) const {
  if (handle < static_count_) return wb_.resolve_string(handle);
  std::uint32_t idx = handle - static_count_;
  if (idx >= dynamic_.size() || !dynamic_[idx].live) return {};
  return dynamic_[idx].bytes;
}

std::uint32_t StringInterner::intern_dynamic(std::string_view s) {
  std::string key(s);
  if (auto it = dyn_index_.find(key); it != dyn_index_.end()) {
    std::uint32_t handle = it->second;
    std::uint32_t idx = handle - static_count_;
    ++dynamic_[idx].refs;
    return handle;
  }
  std::uint32_t idx = static_cast<std::uint32_t>(dynamic_.size());
  Dynamic d;
  d.bytes = std::move(key);
  d.refs = 1;
  d.live = true;
  dynamic_.push_back(std::move(d));
  std::uint32_t handle = static_count_ + idx;
  dyn_index_.emplace(dynamic_[idx].bytes, handle);
  return handle;
}

void StringInterner::retain(std::uint32_t handle) {
  if (handle < static_count_) {
    ++static_borrows_[handle];
    return;
  }
  std::uint32_t idx = handle - static_count_;
  if (idx < dynamic_.size() && dynamic_[idx].live) ++dynamic_[idx].refs;
}

void StringInterner::release(std::uint32_t handle) {
  if (handle < static_count_) {
    if (static_borrows_[handle] > 0) --static_borrows_[handle];
    return;
  }
  std::uint32_t idx = handle - static_count_;
  if (idx >= dynamic_.size() || !dynamic_[idx].live) return;
  if (--dynamic_[idx].refs == 0) {
    dyn_index_.erase(dynamic_[idx].bytes);
    dynamic_[idx].bytes.clear();
    dynamic_[idx].bytes.shrink_to_fit();
    dynamic_[idx].live = false;
  }
}

std::uint32_t StringInterner::borrows(std::uint32_t handle) const {
  if (handle < static_count_) return static_borrows_[handle];
  std::uint32_t idx = handle - static_count_;
  return idx < dynamic_.size() ? dynamic_[idx].refs : 0;
}

std::uint32_t StringInterner::compact() {
  // Reclaim trailing dead dynamic slots. Dead slots were already removed from
  // the dedup index at release, so shrinking the vector cannot orphan a live id.
  std::uint32_t reclaimed = 0;
  while (!dynamic_.empty() && !dynamic_.back().live) {
    dynamic_.pop_back();
    ++reclaimed;
  }
  return reclaimed;
}

bool StringInterner::page_has_live_borrows(std::uint32_t page) const {
  for (std::uint32_t h = 0; h < static_count_; ++h) {
    if (static_borrows_[h] == 0) continue;
    if (h < wb_.string_dir.size() && wb_.string_dir[h].page == page) return true;
  }
  return false;
}

}  // namespace slate
