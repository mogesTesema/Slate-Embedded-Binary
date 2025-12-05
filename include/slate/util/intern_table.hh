// slate/util/intern_table.hh
//
// A deduplicating string table: identical byte spans map to one stable id, and
// the bytes are copied into a contiguous backing buffer. The format layer uses
// it to normalize the decoded shared-string pool into a flat directory. This is
// a *building* structure — it is not the reference-counted interner the
// evaluator uses during recalculation (see eval/string_interner.hh).
#ifndef SLATE_UTIL_INTERN_TABLE_HH
#define SLATE_UTIL_INTERN_TABLE_HH

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace slate {

class InternTable {
 public:
  static constexpr std::uint32_t invalid = 0xFFFFFFFFu;

  // Intern `s`, returning its id. Repeated identical spans return the same id.
  std::uint32_t intern(std::string_view s);

  // Look up bytes by id. Out-of-range ids return an empty view.
  std::string_view view(std::uint32_t id) const noexcept {
    if (id >= entries_.size()) return {};
    const Entry& e = entries_[id];
    return std::string_view(backing_.data() + e.offset, e.length);
  }

  std::uint32_t size() const noexcept {
    return static_cast<std::uint32_t>(entries_.size());
  }

  bool contains(std::string_view s) const {
    return index_.find(std::string(s)) != index_.end();
  }

  void clear() {
    entries_.clear();
    backing_.clear();
    index_.clear();
  }

 private:
  struct Entry {
    std::uint32_t offset;
    std::uint32_t length;
  };

  std::vector<Entry> entries_;
  std::string backing_;
  std::unordered_map<std::string, std::uint32_t> index_;
};

}  // namespace slate

#endif  // SLATE_UTIL_INTERN_TABLE_HH
