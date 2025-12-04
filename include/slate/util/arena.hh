// slate/util/arena.hh
//
// A bump-pointer arena with explicit rollback markers. Evaluation of nested
// array formulas allocates scratch here and unwinds it with markers as each
// nesting level completes, which keeps transient allocations off the general
// heap. Blocks are page-sized and chained; freeing is all-or-nothing per marker.
#ifndef SLATE_UTIL_ARENA_HH
#define SLATE_UTIL_ARENA_HH

#include <cstddef>
#include <cstdint>
#include <vector>

namespace slate {

// An opaque position inside the arena. Rewinding to a marker frees everything
// allocated after it. Markers are ordered; rewinding to an older marker also
// invalidates any newer marker.
struct ArenaMarker {
  std::uint32_t block = 0;
  std::uint32_t offset = 0;
};

class Arena {
 public:
  explicit Arena(std::size_t block_size = 64 * 1024);
  ~Arena();

  Arena(const Arena&) = delete;
  Arena& operator=(const Arena&) = delete;

  // Allocate `n` bytes aligned to `align` (power of two). Never returns null;
  // grows by chaining a fresh block when the current one is exhausted.
  void* allocate(std::size_t n, std::size_t align = alignof(std::max_align_t));

  template <typename T>
  T* alloc_array(std::size_t count) {
    return static_cast<T*>(allocate(count * sizeof(T), alignof(T)));
  }

  // Capture the current high-water position.
  ArenaMarker mark() const noexcept;

  // Release everything allocated since `m`. Blocks past the marked block are
  // retained for reuse (not returned to the OS) to keep recompute cheap.
  void rewind(const ArenaMarker& m) noexcept;

  // Drop every allocation and keep a single block for reuse.
  void reset() noexcept;

  std::size_t bytes_used() const noexcept;

 private:
  struct Block {
    std::uint8_t* data = nullptr;
    std::size_t capacity = 0;
    std::size_t used = 0;
  };

  void add_block(std::size_t min_size);

  std::size_t block_size_;
  std::vector<Block> blocks_;
  std::uint32_t current_ = 0;  // index of the block we are bumping into
};

}  // namespace slate

#endif  // SLATE_UTIL_ARENA_HH
