// slate/eval/array_store.hh
//
// Storage for materialized array values (the result of an array formula or a
// loaded range). Each array is reference counted: the anchor cell holds one
// reference and every spill-target cell that borrows the result holds another.
// Releasing the last reference frees the backing and returns the slot to a free
// list for reuse. A handle is only valid while its slot is live.
#ifndef SLATE_EVAL_ARRAY_STORE_HH
#define SLATE_EVAL_ARRAY_STORE_HH

#include <cstdint>
#include <vector>

#include "slate/model/value.hh"

namespace slate {

struct ArrayValue {
  std::uint32_t rows = 0;
  std::uint32_t cols = 0;
  std::vector<Value> cells;   // row-major, size rows*cols
  std::uint32_t refs = 0;
  bool live = false;
};

class ArrayStore {
 public:
  // Allocate an array with one reference. Reuses a freed slot when available.
  std::uint32_t allocate(std::uint32_t rows, std::uint32_t cols);

  bool alive(std::uint32_t handle) const {
    return handle < arrays_.size() && arrays_[handle].live;
  }

  ArrayValue* get(std::uint32_t handle) {
    return alive(handle) ? &arrays_[handle] : nullptr;
  }
  const ArrayValue* get(std::uint32_t handle) const {
    return alive(handle) ? &arrays_[handle] : nullptr;
  }

  Value* at(std::uint32_t handle, std::uint32_t r, std::uint32_t c);

  void retain(std::uint32_t handle);
  void release(std::uint32_t handle);

  std::size_t live_count() const;

  // Drop trailing dead slots (and their free-list entries), shrinking the store.
  // Only trailing dead slots are removed, so no live handle is ever invalidated.
  // Returns the number of slots reclaimed.
  std::uint32_t compact();

 private:
  std::vector<ArrayValue> arrays_;
  std::vector<std::uint32_t> free_list_;
};

}  // namespace slate

#endif  // SLATE_EVAL_ARRAY_STORE_HH
