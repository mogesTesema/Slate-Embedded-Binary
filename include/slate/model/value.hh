// slate/model/value.hh
//
// The runtime cell value: a tag plus a small union. Text and array values carry
// *handles* (ids into the evaluator's string interner and array store), not
// owning pointers — the model layer never allocates that storage, it only names
// it. The tag is authoritative: readers must consult it before touching the
// union, and any overwrite must go through set_*/reset so a handle is never left
// dangling under a scalar tag.
#ifndef SLATE_MODEL_VALUE_HH
#define SLATE_MODEL_VALUE_HH

#include <cstdint>

#include "slate/format/wire.hh"
#include "slate/ids.hh"

namespace slate {

enum class ValueTag : std::uint8_t {
  empty = 0,
  number,
  text,     // union.str  -> interner handle
  boolean,
  error,    // union.err  -> wire::ErrCode
  array,    // union.array -> array-store handle
};

class Value {
 public:
  Value() : tag_(ValueTag::empty) { u_.bits = 0; }

  static Value make_empty() { return Value(); }
  static Value number(double d) { Value v; v.tag_ = ValueTag::number; v.u_.num = d; return v; }
  static Value text(std::uint32_t handle) { Value v; v.tag_ = ValueTag::text; v.u_.str = handle; return v; }
  static Value boolean(bool b) { Value v; v.tag_ = ValueTag::boolean; v.u_.b = b ? 1 : 0; return v; }
  static Value error(wire::ErrCode e) { Value v; v.tag_ = ValueTag::error; v.u_.err = static_cast<std::uint8_t>(e); return v; }
  static Value array(std::uint32_t handle) { Value v; v.tag_ = ValueTag::array; v.u_.array = handle; return v; }

  ValueTag tag() const { return tag_; }
  bool is_empty() const { return tag_ == ValueTag::empty; }
  bool is_number() const { return tag_ == ValueTag::number; }
  bool is_text() const { return tag_ == ValueTag::text; }
  bool is_bool() const { return tag_ == ValueTag::boolean; }
  bool is_error() const { return tag_ == ValueTag::error; }
  bool is_array() const { return tag_ == ValueTag::array; }

  // Accessors assume the caller has checked the tag; they never reinterpret.
  double as_number() const { return u_.num; }
  std::uint32_t as_text() const { return u_.str; }
  bool as_bool() const { return u_.b != 0; }
  wire::ErrCode as_error() const { return static_cast<wire::ErrCode>(u_.err); }
  std::uint32_t as_array() const { return u_.array; }

  // True when this value owns a handle into an external store (string/array).
  bool holds_handle() const {
    return tag_ == ValueTag::text || tag_ == ValueTag::array;
  }
  std::uint32_t handle() const { return u_.array; }  // str/array share the slot

  // Reset to empty, clearing the union so a stale handle cannot survive.
  void clear() {
    tag_ = ValueTag::empty;
    u_.bits = 0;
  }

  bool numeric_like() const {
    return tag_ == ValueTag::number || tag_ == ValueTag::boolean;
  }
  double coerce_number() const {
    if (tag_ == ValueTag::number) return u_.num;
    if (tag_ == ValueTag::boolean) return u_.b ? 1.0 : 0.0;
    return 0.0;
  }

 private:
  ValueTag tag_;
  union Storage {
    double num;
    std::uint32_t str;
    std::uint32_t array;
    std::uint8_t b;
    std::uint8_t err;
    std::uint64_t bits;
  } u_;
};

}  // namespace slate

#endif  // SLATE_MODEL_VALUE_HH
