// slate/util/byte_writer.hh
//
// Little-endian append-only byte buffer, the mirror of ByteReader. Used by the
// mkworkbook tool and the test fixtures to assemble workbook bytes. Header-only;
// it is a thin wrapper over a std::vector<uint8_t>.
#ifndef SLATE_UTIL_BYTE_WRITER_HH
#define SLATE_UTIL_BYTE_WRITER_HH

#include <cstdint>
#include <cstring>
#include <string_view>
#include <vector>

namespace slate {

class ByteWriter {
 public:
  void u8(std::uint8_t v) { buf_.push_back(v); }

  void u16(std::uint16_t v) {
    buf_.push_back(static_cast<std::uint8_t>(v));
    buf_.push_back(static_cast<std::uint8_t>(v >> 8));
  }

  void u32(std::uint32_t v) {
    for (int i = 0; i < 4; ++i)
      buf_.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
  }

  void u64(std::uint64_t v) {
    for (int i = 0; i < 8; ++i)
      buf_.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
  }

  void f64(double d) {
    std::uint64_t bits;
    std::memcpy(&bits, &d, sizeof(bits));
    u64(bits);
  }

  void bytes(const void* p, std::size_t n) {
    const auto* b = static_cast<const std::uint8_t*>(p);
    buf_.insert(buf_.end(), b, b + n);
  }

  void bytes(std::string_view s) { bytes(s.data(), s.size()); }

  // Overwrite 4 bytes at an earlier position (for back-patching offsets/crcs).
  void patch_u32(std::size_t at, std::uint32_t v) {
    for (int i = 0; i < 4; ++i)
      buf_[at + i] = static_cast<std::uint8_t>(v >> (8 * i));
  }

  std::size_t size() const { return buf_.size(); }
  const std::vector<std::uint8_t>& data() const { return buf_; }
  std::vector<std::uint8_t>& data() { return buf_; }

 private:
  std::vector<std::uint8_t> buf_;
};

}  // namespace slate

#endif  // SLATE_UTIL_BYTE_WRITER_HH
