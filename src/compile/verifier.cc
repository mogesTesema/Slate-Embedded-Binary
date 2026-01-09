// slate/compile/verifier.cc
#include "slate/compile/verifier.hh"

#include <vector>

#include "slate/util/byte_reader.hh"

namespace slate {
namespace {

bool is_scalarish(VType t) {
  // Anything the arithmetic/logic ops can consume directly (references must be
  // loaded first, arrays must be reduced first).
  return t != VType::ref && t != VType::range && t != VType::array;
}

struct Verifier {
  ByteReader r;
  DiagnosticSink& diag;
  std::vector<VType> stack;
  std::uint32_t max_stack = 0;

  Verifier(const std::uint8_t* code, std::size_t len, DiagnosticSink& d)
      : r(code, len), diag(d) {}

  bool push(VType t) {
    if (stack.size() >= kMaxFormulaStack) {
      diag.error(Status::stack_overflow, "formula stack overflow");
      return false;
    }
    stack.push_back(t);
    if (stack.size() > max_stack) max_stack = static_cast<std::uint32_t>(stack.size());
    return true;
  }

  bool pop(VType& out) {
    if (stack.empty()) {
      diag.error(Status::stack_underflow, "formula stack underflow");
      return false;
    }
    out = stack.back();
    stack.pop_back();
    return true;
  }

  bool need_scalar_pair() {
    VType a, b;
    if (!pop(b) || !pop(a)) return false;
    if (!is_scalarish(a) || !is_scalarish(b)) {
      diag.error(Status::type_error, "operator needs scalar operands");
      return false;
    }
    return true;
  }
};

}  // namespace

Status verify_formula(const std::uint8_t* code, std::size_t len, Program& out,
                      DiagnosticSink& diag) {
  Verifier v(code, len, diag);
  out.code.clear();

  bool terminated = false;
  while (!v.r.empty()) {
    std::uint8_t raw = 0;
    if (Status s = v.r.u8(raw); !ok(s)) return s;
    Instr in;
    in.op = static_cast<wire::Op>(raw);

    switch (in.op) {
      case wire::Op::end:
        terminated = true;
        break;

      case wire::Op::push_num:
        if (Status s = v.r.f64(in.num); !ok(s)) return s;
        in.result = VType::number;
        if (!v.push(VType::number)) return Status::stack_overflow;
        break;

      case wire::Op::push_str: {
        if (Status s = v.r.u32(in.a); !ok(s)) return s;
        in.result = VType::text;
        if (!v.push(VType::text)) return Status::stack_overflow;
        break;
      }
      case wire::Op::push_bool: {
        std::uint8_t b = 0;
        if (Status s = v.r.u8(b); !ok(s)) return s;
        in.a = b;
        in.result = VType::boolean;
        if (!v.push(VType::boolean)) return Status::stack_overflow;
        break;
      }
      case wire::Op::push_err: {
        std::uint8_t e = 0;
        if (Status s = v.r.u8(e); !ok(s)) return s;
        in.a = e;
        in.result = VType::error;
        if (!v.push(VType::error)) return Status::stack_overflow;
        break;
      }
      case wire::Op::push_ref: {
        std::uint16_t sheet = 0;
        if (Status s = v.r.u16(sheet); !ok(s)) return s;
        if (Status s = v.r.u32(in.b); !ok(s)) return s;
        if (Status s = v.r.u32(in.c); !ok(s)) return s;
        in.a = sheet;
        in.result = VType::ref;
        if (!v.push(VType::ref)) return Status::stack_overflow;
        break;
      }
      case wire::Op::push_range: {
        std::uint16_t sheet = 0;
        if (Status s = v.r.u16(sheet); !ok(s)) return s;
        if (Status s = v.r.u32(in.b); !ok(s)) return s;
        if (Status s = v.r.u32(in.c); !ok(s)) return s;
        if (Status s = v.r.u32(in.d); !ok(s)) return s;
        if (Status s = v.r.u32(in.e); !ok(s)) return s;
        in.a = sheet;
        in.result = VType::range;
        if (!v.push(VType::range)) return Status::stack_overflow;
        break;
      }
      case wire::Op::push_name: {
        if (Status s = v.r.u32(in.a); !ok(s)) return s;
        // A name may resolve to a cell or a range; leave its static type open so
        // it can be used both as a scalar operand and as a function argument.
        in.result = VType::any;
        if (!v.push(VType::any)) return Status::stack_overflow;
        break;
      }
      case wire::Op::load: {
        VType t;
        if (!v.pop(t)) return Status::stack_underflow;
        if (t == VType::ref) {
          in.result = VType::scalar;
          if (!v.push(VType::scalar)) return Status::stack_overflow;
        } else if (t == VType::range) {
          in.result = VType::array;
          if (!v.push(VType::array)) return Status::stack_overflow;
        } else {
          diag.error(Status::type_error, "load expects a reference");
          return Status::type_error;
        }
        break;
      }
      case wire::Op::add:
      case wire::Op::sub:
      case wire::Op::mul:
      case wire::Op::divide:
        if (!v.need_scalar_pair()) return Status::type_error;
        in.result = VType::number;
        if (!v.push(VType::number)) return Status::stack_overflow;
        break;

      case wire::Op::neg: {
        VType t;
        if (!v.pop(t)) return Status::stack_underflow;
        if (!is_scalarish(t)) { diag.error(Status::type_error, "neg needs scalar"); return Status::type_error; }
        in.result = VType::number;
        if (!v.push(VType::number)) return Status::stack_overflow;
        break;
      }
      case wire::Op::concat:
        if (!v.need_scalar_pair()) return Status::type_error;
        in.result = VType::text;
        if (!v.push(VType::text)) return Status::stack_overflow;
        break;

      case wire::Op::eq:
      case wire::Op::lt:
      case wire::Op::gt:
      case wire::Op::le:
      case wire::Op::ge:
      case wire::Op::ne:
        if (!v.need_scalar_pair()) return Status::type_error;
        in.result = VType::boolean;
        if (!v.push(VType::boolean)) return Status::stack_overflow;
        break;

      case wire::Op::call: {
        std::uint16_t func = 0;
        std::uint8_t argc = 0;
        if (Status s = v.r.u16(func); !ok(s)) return s;
        if (Status s = v.r.u8(argc); !ok(s)) return s;
        in.func = func;
        in.argc = argc;
        if (v.stack.size() < argc) {
          diag.error(Status::arity_mismatch, "call underflows stack");
          return Status::arity_mismatch;
        }
        for (std::uint8_t i = 0; i < argc; ++i) v.stack.pop_back();
        in.result = VType::scalar;
        if (!v.push(VType::scalar)) return Status::stack_overflow;
        break;
      }
      case wire::Op::make_array: {
        if (Status s = v.r.u32(in.b); !ok(s)) return s;  // rows
        if (Status s = v.r.u32(in.c); !ok(s)) return s;  // cols
        std::uint64_t count = static_cast<std::uint64_t>(in.b) * in.c;
        if (count > kMaxFormulaStack) {
          diag.error(Status::stack_overflow, "array literal too large");
          return Status::stack_overflow;
        }
        if (v.stack.size() < count) {
          diag.error(Status::stack_underflow, "make_array underflow");
          return Status::stack_underflow;
        }
        for (std::uint64_t i = 0; i < count; ++i) v.stack.pop_back();
        in.result = VType::array;
        if (!v.push(VType::array)) return Status::stack_overflow;
        break;
      }
      default:
        diag.error(Status::bad_opcode, "unknown formula opcode");
        return Status::bad_opcode;
    }

    out.code.push_back(in);
    if (terminated) break;
  }

  if (!terminated) {
    diag.error(Status::bad_opcode, "formula missing end");
    return Status::bad_opcode;
  }
  if (v.stack.size() != 1) {
    diag.error(Status::type_error, "formula must leave exactly one result");
    return Status::type_error;
  }
  out.max_stack = v.max_stack;
  return Status::ok;
}

}  // namespace slate
