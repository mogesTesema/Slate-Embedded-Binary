// slate/eval/array_store.cc
#include "slate/eval/array_store.hh"

namespace slate {

std::uint32_t ArrayStore::allocate(std::uint32_t rows, std::uint32_t cols) {
  std::uint32_t handle;
  if (!free_list_.empty()) {
    handle = free_list_.back();
    free_list_.pop_back();
  } else {
    handle = static_cast<std::uint32_t>(arrays_.size());
    arrays_.emplace_back();
  }
  ArrayValue& a = arrays_[handle];
  a.rows = rows;
  a.cols = cols;
  a.cells.assign(static_cast<std::size_t>(rows) * cols, Value());
  a.refs = 1;
  a.live = true;
  return handle;
}

Value* ArrayStore::at(std::uint32_t handle, std::uint32_t r, std::uint32_t c) {
  ArrayValue* a = get(handle);
  if (!a || r >= a->rows || c >= a->cols) return nullptr;
  return &a->cells[static_cast<std::size_t>(r) * a->cols + c];
}

void ArrayStore::retain(std::uint32_t handle) {
  if (alive(handle)) ++arrays_[handle].refs;
}

void ArrayStore::release(std::uint32_t handle) {
  if (!alive(handle)) return;
  ArrayValue& a = arrays_[handle];
  if (--a.refs == 0) {
    a.cells.clear();
    a.cells.shrink_to_fit();
    a.live = false;
    free_list_.push_back(handle);
  }
}

std::size_t ArrayStore::live_count() const {
  std::size_t n = 0;
  for (const ArrayValue& a : arrays_)
    if (a.live) ++n;
  return n;
}

std::uint32_t ArrayStore::compact() {
  std::uint32_t reclaimed = 0;
  while (!arrays_.empty() && !arrays_.back().live) {
    std::uint32_t idx = static_cast<std::uint32_t>(arrays_.size() - 1);
    // A dead slot is on the free list; drop that entry before shrinking so the
    // free list never dangles past the end of the vector.
    for (std::size_t i = 0; i < free_list_.size(); ++i) {
      if (free_list_[i] == idx) {
        free_list_[i] = free_list_.back();
        free_list_.pop_back();
        break;
      }
    }
    arrays_.pop_back();
    ++reclaimed;
  }
  return reclaimed;
}

}  // namespace slate
