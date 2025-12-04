// slate/util/arena.cc
#include "slate/util/arena.hh"

#include <cassert>
#include <cstdlib>
#include <new>

namespace slate {

namespace {
std::size_t align_up(std::size_t v, std::size_t a) noexcept {
  return (v + (a - 1)) & ~(a - 1);
}
}  // namespace

Arena::Arena(std::size_t block_size) : block_size_(block_size) {
  add_block(block_size_);
}

Arena::~Arena() {
  for (Block& b : blocks_)
    std::free(b.data);
}

void Arena::add_block(std::size_t min_size) {
  std::size_t cap = block_size_ > min_size ? block_size_ : min_size;
  auto* mem = static_cast<std::uint8_t*>(std::malloc(cap));
  if (!mem) throw std::bad_alloc();
  blocks_.push_back(Block{mem, cap, 0});
  current_ = static_cast<std::uint32_t>(blocks_.size() - 1);
}

void* Arena::allocate(std::size_t n, std::size_t align) {
  // Try to satisfy from the current block, then any already-allocated block
  // ahead of it (they may have been retained by a rewind), else grow.
  for (;;) {
    Block& blk = blocks_[current_];
    std::size_t start = align_up(blk.used, align);
    if (start + n <= blk.capacity) {
      blk.used = start + n;
      return blk.data + start;
    }
    if (current_ + 1 < blocks_.size()) {
      ++current_;
      blocks_[current_].used = 0;
      continue;
    }
    add_block(n + align);
  }
}

ArenaMarker Arena::mark() const noexcept {
  return ArenaMarker{current_, static_cast<std::uint32_t>(blocks_[current_].used)};
}

void Arena::rewind(const ArenaMarker& m) noexcept {
  // Reset every block past the marked one so its space is reusable, then
  // restore the marked block's high-water mark.
  for (std::size_t i = m.block + 1; i < blocks_.size(); ++i)
    blocks_[i].used = 0;
  blocks_[m.block].used = m.offset;
  current_ = m.block;
}

void Arena::reset() noexcept {
  for (Block& b : blocks_)
    b.used = 0;
  current_ = 0;
}

std::size_t Arena::bytes_used() const noexcept {
  std::size_t total = 0;
  for (const Block& b : blocks_)
    total += b.used;
  return total;
}

}  // namespace slate
