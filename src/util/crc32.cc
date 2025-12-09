// slate/util/crc32.cc
#include "slate/util/crc32.hh"

#include <array>

namespace slate {
namespace {

// Reflected CRC-32 table, generated once at first use. Kept in an anonymous
// namespace so the polynomial choice stays an implementation detail.
struct Crc32Table {
  std::array<std::uint32_t, 256> t;
  Crc32Table() {
    constexpr std::uint32_t poly = 0xEDB88320u;
    for (std::uint32_t i = 0; i < 256; ++i) {
      std::uint32_t c = i;
      for (int k = 0; k < 8; ++k)
        c = (c & 1u) ? (poly ^ (c >> 1)) : (c >> 1);
      t[i] = c;
    }
  }
};

const Crc32Table& table() {
  static const Crc32Table tbl;
  return tbl;
}

}  // namespace

void Crc32::update(const std::uint8_t* data, std::size_t len) noexcept {
  const auto& t = table().t;
  std::uint32_t c = acc_;
  for (std::size_t i = 0; i < len; ++i)
    c = t[(c ^ data[i]) & 0xFFu] ^ (c >> 8);
  acc_ = c;
}

}  // namespace slate
