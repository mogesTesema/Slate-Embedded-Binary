// slate/util/diagnostic.hh
//
// A tiny append-only diagnostic sink. Decode and evaluation record where they
// gave up so callers (and tests) can inspect the first failure without a heavy
// logging dependency. Severity is coarse on purpose.
#ifndef SLATE_UTIL_DIAGNOSTIC_HH
#define SLATE_UTIL_DIAGNOSTIC_HH

#include <cstddef>
#include <string>
#include <vector>

#include "slate/util/status.hh"

namespace slate {

enum class Severity { info, warning, error };

struct Diagnostic {
  Severity severity;
  Status status;
  std::size_t offset;   // byte offset in the workbook, or ~0 if not applicable
  std::string detail;
};

class DiagnosticSink {
 public:
  static constexpr std::size_t no_offset = static_cast<std::size_t>(-1);

  void note(Severity sev, Status st, std::string detail,
            std::size_t offset = no_offset) {
    items_.push_back(Diagnostic{sev, st, offset, std::move(detail)});
    if (sev == Severity::error) ++error_count_;
  }

  void error(Status st, std::string detail, std::size_t offset = no_offset) {
    note(Severity::error, st, std::move(detail), offset);
  }

  void warn(Status st, std::string detail, std::size_t offset = no_offset) {
    note(Severity::warning, st, std::move(detail), offset);
  }

  bool has_errors() const noexcept { return error_count_ > 0; }
  std::size_t error_count() const noexcept { return error_count_; }
  const std::vector<Diagnostic>& items() const noexcept { return items_; }

  // Status of the first recorded error, or ok if none.
  Status first_error() const noexcept {
    for (const auto& d : items_)
      if (d.severity == Severity::error) return d.status;
    return Status::ok;
  }

  void clear() noexcept {
    items_.clear();
    error_count_ = 0;
  }

 private:
  std::vector<Diagnostic> items_;
  std::size_t error_count_ = 0;
};

}  // namespace slate

#endif  // SLATE_UTIL_DIAGNOSTIC_HH
