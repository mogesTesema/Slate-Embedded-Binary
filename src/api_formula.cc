// slate/api_formula.cc
//
// The standalone formula-module entry point. A module is a compact blob:
//   u8[4] "SFM1"
//   u16   input_count
//   f64   inputs[input_count]        // exposed as cells m!A1..m!A{n} (col 0)
//   u32   code_len
//   u8    code[code_len]             // formula bytecode
// It is compiled, bound against a one-sheet scratch workbook holding the inputs,
// and evaluated. This drives the verifier, binder, evaluator, functions, array
// store, and dynamic string interning without a full workbook container.
#include "slate/slate.hh"

#include <cstring>

#include "slate/compile/binder.hh"
#include "slate/compile/verifier.hh"
#include "slate/eval/array_store.hh"
#include "slate/eval/evaluator.hh"
#include "slate/eval/string_interner.hh"
#include "slate/eval/value_ops.hh"
#include "slate/model/workbook.hh"
#include "slate/util/byte_reader.hh"

namespace slate {

namespace {
constexpr std::uint8_t kModuleMagic[4] = {'S', 'F', 'M', '1'};
constexpr std::uint16_t kMaxInputs = 4096;
}  // namespace

Status eval_formula_module(const std::uint8_t* data, std::size_t size,
                           DiagnosticSink& diag) {
  ByteReader r(data, size);
  const std::uint8_t* magic = nullptr;
  if (Status s = r.take(4, magic); !ok(s)) return s;
  if (std::memcmp(magic, kModuleMagic, 4) != 0) {
    diag.error(Status::bad_magic, "not a formula module");
    return Status::bad_magic;
  }

  std::uint16_t input_count = 0;
  if (Status s = r.u16(input_count); !ok(s)) return s;
  if (input_count > kMaxInputs) {
    diag.error(Status::bad_section_table, "too many module inputs");
    return Status::bad_section_table;
  }

  // A scratch workbook: one sheet, `input_count` number cells in column 0.
  Workbook wb;
  wb.sheets.add(kNoStr, input_count, 1, /*visible=*/true);
  for (std::uint16_t i = 0; i < input_count; ++i) {
    double v = 0.0;
    if (Status s = r.f64(v); !ok(s)) return s;
    CellId id = wb.grid.add(CellRef{0, i, 0}, wire::CellTag::number);
    wb.grid.at(id).value = Value::number(v);
  }

  std::uint32_t code_len = 0;
  if (Status s = r.u32(code_len); !ok(s)) return s;
  const std::uint8_t* code = nullptr;
  if (Status s = r.take(code_len, code); !ok(s)) return s;

  Program prog;
  if (Status s = verify_formula(code, code_len, prog, diag); !ok(s)) return s;
  if (Status s = bind_program(prog, wb, diag); !ok(s)) return s;

  StringInterner strings(wb);
  ArrayStore arrays;
  Evaluator eval(wb, strings, arrays);
  Value result = eval.run(prog, diag);

  // Balance the one reference the result carries.
  EvalContext c = eval.context();
  release_handles(c, result);
  return Status::ok;
}

bool probe_formula_module(const std::uint8_t* data, std::size_t size) {
  DiagnosticSink diag;
  return ok(eval_formula_module(data, size, diag));
}

}  // namespace slate
