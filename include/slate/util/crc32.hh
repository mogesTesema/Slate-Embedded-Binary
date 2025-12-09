// slate/util/crc32.hh
//
// Plain CRC-32 (IEEE 802.3 polynomial, reflected) used for the workbook header
// and per-section integrity fields. Table-driven; no hardware intrinsics so the
// build stays portable and deterministic across the fuzzing image.
#ifndef SLATE_UTIL_CRC32_HH
#define SLATE_UTIL_CRC32_HH

#include <cstddef>
#include <cstdint>

namespace slate {

// Incremental interface so we can checksum a header prefix and a section body
// without copying them together.
class Crc32 {
 public:
  Crc32() = default;

  void update(const std::uint8_t* data, std::size_t len) noexcept;
  std::uint32_t finish() const noexcept { return acc_ ^ 0xFFFFFFFFu; }
  void reset() noexcept { acc_ = 0xFFFFFFFFu; }

  static std::uint32_t of(const std::uint8_t* data, std::size_t len) noexcept {
    Crc32 c;
    c.update(data, len);
    return c.finish();
  }

 private:
  std::uint32_t acc_ = 0xFFFFFFFFu;
};

}  // namespace slate

#endif  // SLATE_UTIL_CRC32_HH
