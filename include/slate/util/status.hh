// slate/util/status.hh
//
// Slate uses a small, explicit result type for the decode and evaluation paths.
// We deliberately do *not* funnel everything through exceptions: the hot decode
// loop returns Status codes so callers can branch cheaply, while a handful of
// truly exceptional situations (programmer error, exhausted arena) still throw.
#ifndef SLATE_UTIL_STATUS_HH
#define SLATE_UTIL_STATUS_HH

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace slate {

enum class Status : std::uint16_t {
  ok = 0,

  // --- framing / decode ---
  short_read,          // ran off the end of the input buffer
  bad_magic,
  bad_version,
  bad_checksum,
  bad_section_table,
  duplicate_section,
  missing_section,
  section_overrun,     // a section's declared bounds escape the file
  bad_encoding,        // malformed SST run / prefix stream
  size_mismatch,

  // --- model / graph ---
  bad_reference,
  unknown_sheet,
  unknown_name,
  cyclic_dependency,
  too_deep,

  // --- compile / eval ---
  bad_opcode,
  stack_underflow,
  stack_overflow,
  arity_mismatch,
  type_error,
  div_by_zero,
  name_error,

  internal,            // an invariant we expected to hold did not
};

// Human-readable, stable spelling for diagnostics and tests.
std::string_view status_name(Status s) noexcept;

inline bool ok(Status s) noexcept { return s == Status::ok; }

// Lightweight "value or status" without pulling in std::expected everywhere.
// Only used where a decode step naturally produces a value; the graph layer
// prefers std::optional and the tools layer throws.
template <typename T>
struct Result {
  Status status = Status::ok;
  T value{};

  Result() = default;
  Result(T v) : status(Status::ok), value(std::move(v)) {}
  Result(Status s) : status(s), value{} {}

  bool ok() const noexcept { return status == Status::ok; }
  explicit operator bool() const noexcept { return ok(); }
};

}  // namespace slate

#endif  // SLATE_UTIL_STATUS_HH
