// slate/api_load.cc
//
// Implementation of the load/probe entry points. The recalc and formula-module
// entry points live in their own translation units once those layers are built.
#include "slate/slate.hh"

#include "slate/format/decode.hh"
#include "slate/model/workbook.hh"

namespace slate {

Status load_workbook(const std::uint8_t* data, std::size_t size, Workbook& out,
                     DiagnosticSink& diag) {
  RawWorkbook raw;
  if (Status s = decode_workbook(data, size, raw, diag); !ok(s)) return s;
  return Workbook::link(std::move(raw), out, diag);
}

bool probe_workbook(const std::uint8_t* data, std::size_t size) {
  Workbook wb;
  DiagnosticSink diag;
  return ok(load_workbook(data, size, wb, diag));
}

}  // namespace slate
