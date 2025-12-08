// slate/util/byte_reader.hh
//
// A bounds-checked cursor over a borrowed byte range. Every integer read is
// little-endian and validates remaining length up front, so higher layers can
// treat a successful read as trustworthy. The reader never owns its bytes.
#ifndef SLATE_UTIL_BYTE_READER_HH
#define SLATE_UTIL_BYTE_READER_HH

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "slate/util/status.hh"

namespace slate {

class ByteReader {
 public:
  ByteReader(const std::uint8_t* data, std::size_t size) noexcept
      : base_(data), size_(size), pos_(0) {}

  std::size_t pos() const noexcept { return pos_; }
  std::size_t size() const noexcept { return size_; }
  std::size_t remaining() const noexcept { return size_ - pos_; }
  bool empty() const noexcept { return pos_ >= size_; }
  const std::uint8_t* base() const noexcept { return base_; }

  // Reposition the cursor. Returns false if out of range (cursor unchanged).
  bool seek(std::size_t abs) noexcept {
    if (abs > size_) return false;
    pos_ = abs;
    return true;
  }

  bool skip(std::size_t n) noexcept {
    if (n > remaining()) return false;
    pos_ += n;
    return true;
  }

  Status u8(std::uint8_t& out) noexcept {
    if (remaining() < 1) return Status::short_read;
    out = base_[pos_++];
    return Status::ok;
  }

  Status u16(std::uint16_t& out) noexcept {
    if (remaining() < 2) return Status::short_read;
    out = static_cast<std::uint16_t>(base_[pos_]) |
          static_cast<std::uint16_t>(base_[pos_ + 1]) << 8;
    pos_ += 2;
    return Status::ok;
  }

  Status u32(std::uint32_t& out) noexcept {
    if (remaining() < 4) return Status::short_read;
    out = static_cast<std::uint32_t>(base_[pos_]) |
          static_cast<std::uint32_t>(base_[pos_ + 1]) << 8 |
          static_cast<std::uint32_t>(base_[pos_ + 2]) << 16 |
          static_cast<std::uint32_t>(base_[pos_ + 3]) << 24;
    pos_ += 4;
    return Status::ok;
  }

  Status u64(std::uint64_t& out) noexcept {
    std::uint32_t lo, hi;
    if (Status s = u32(lo); !slate::ok(s)) return s;
    if (Status s = u32(hi); !slate::ok(s)) return s;
    out = static_cast<std::uint64_t>(lo) | static_cast<std::uint64_t>(hi) << 32;
    return Status::ok;
  }

  Status f64(double& out) noexcept {
    std::uint64_t bits;
    if (Status s = u64(bits); !slate::ok(s)) return s;
    std::memcpy(&out, &bits, sizeof(out));
    return Status::ok;
  }

  // Borrow `n` raw bytes and advance. The returned pointer aliases the input.
  Status take(std::size_t n, const std::uint8_t*& out) noexcept {
    if (n > remaining()) return Status::short_read;
    out = base_ + pos_;
    pos_ += n;
    return Status::ok;
  }

  // A sub-reader over an absolute [offset, offset+len) window of the same base.
  Status window(std::size_t offset, std::size_t len, ByteReader& out) const noexcept {
    if (offset > size_ || len > size_ - offset) return Status::section_overrun;
    out = ByteReader(base_ + offset, len);
    return Status::ok;
  }

 private:
  const std::uint8_t* base_;
  std::size_t size_;
  std::size_t pos_;
};

}  // namespace slate

#endif  // SLATE_UTIL_BYTE_READER_HH
