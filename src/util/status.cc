// slate/util/status.cc
#include "slate/util/status.hh"

namespace slate {

std::string_view status_name(Status s) noexcept {
  switch (s) {
    case Status::ok:                return "ok";
    case Status::short_read:        return "short_read";
    case Status::bad_magic:         return "bad_magic";
    case Status::bad_version:       return "bad_version";
    case Status::bad_checksum:      return "bad_checksum";
    case Status::bad_section_table: return "bad_section_table";
    case Status::duplicate_section: return "duplicate_section";
    case Status::missing_section:   return "missing_section";
    case Status::section_overrun:   return "section_overrun";
    case Status::bad_encoding:      return "bad_encoding";
    case Status::size_mismatch:     return "size_mismatch";
    case Status::bad_reference:     return "bad_reference";
    case Status::unknown_sheet:     return "unknown_sheet";
    case Status::unknown_name:      return "unknown_name";
    case Status::cyclic_dependency: return "cyclic_dependency";
    case Status::too_deep:          return "too_deep";
    case Status::bad_opcode:        return "bad_opcode";
    case Status::stack_underflow:   return "stack_underflow";
    case Status::stack_overflow:    return "stack_overflow";
    case Status::arity_mismatch:    return "arity_mismatch";
    case Status::type_error:        return "type_error";
    case Status::div_by_zero:       return "div_by_zero";
    case Status::name_error:        return "name_error";
    case Status::internal:          return "internal";
  }
  return "?";
}

}  // namespace slate
