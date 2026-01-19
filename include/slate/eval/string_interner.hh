// slate/eval/string_interner.hh
//
// Runtime string identity for evaluation. Handles below the workbook's static
// string count name shared-string-table entries and resolve, zero-copy, into
// the owning SST page. Handles at or above it name dynamic strings produced by
// evaluation (e.g. CONCAT), which are owned, deduplicated, and reference
// counted here.
//
// Borrow tracking spans both spaces: a static handle carries a borrow count so
// recalc can gauge whether an SST page still backs active references.
#ifndef SLATE_EVAL_STRING_INTERNER_HH
#define SLATE_EVAL_STRING_INTERNER_HH

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "slate/ids.hh"
#include "slate/model/workbook.hh"

namespace slate {

class StringInterner {
 public:
  explicit StringInterner(const Workbook& wb);

  bool is_static(std::uint32_t handle) const { return handle < static_count_; }

  // Resolve a handle to bytes. Static handles read their SST page; dynamic
  // handles read their owned buffer. An unknown/dead handle yields "".
  std::string_view resolve(std::uint32_t handle) const;

  // Intern a computed string, returning a dynamic handle with one borrow.
  std::uint32_t intern_dynamic(std::string_view s);

  void retain(std::uint32_t handle);
  void release(std::uint32_t handle);

  std::uint32_t borrows(std::uint32_t handle) const;

  // Recalc support: does any live static handle backed by `page` still carry a
  // borrow? A page may only be recycled when this is false.
  bool page_has_live_borrows(std::uint32_t page) const;

  std::uint32_t static_count() const { return static_count_; }

  // Drop trailing dead dynamic slots, shrinking the table. Only trailing dead
  // slots are removed, so no live handle changes id. Returns slots reclaimed.
  std::uint32_t compact();

 private:
  struct Dynamic {
    std::string bytes;
    std::uint32_t refs = 0;
    bool live = false;
  };

  const Workbook& wb_;
  std::uint32_t static_count_;
  std::vector<std::uint32_t> static_borrows_;  // per static handle
  std::vector<Dynamic> dynamic_;
  std::unordered_map<std::string, std::uint32_t> dyn_index_;  // bytes -> handle
};

}  // namespace slate

#endif  // SLATE_EVAL_STRING_INTERNER_HH
