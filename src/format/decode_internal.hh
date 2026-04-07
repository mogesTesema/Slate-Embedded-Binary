// slate/format/decode_internal.hh
//
// Per-section decoders, shared between decode.cc (which owns framing) and the
// section-specific translation units. Each takes a ByteReader already windowed
// to the section body and appends into the RawWorkbook.
#ifndef SLATE_FORMAT_DECODE_INTERNAL_HH
#define SLATE_FORMAT_DECODE_INTERNAL_HH

#include "slate/format/raw.hh"
#include "slate/util/byte_reader.hh"
#include "slate/util/diagnostic.hh"
#include "slate/util/status.hh"

namespace slate::detail {

Status decode_sheets(ByteReader& r, RawWorkbook& wb, DiagnosticSink& diag);
Status decode_sst(ByteReader& r, RawWorkbook& wb, DiagnosticSink& diag);
Status decode_cells(ByteReader& r, RawWorkbook& wb, DiagnosticSink& diag);
Status decode_formulas(ByteReader& r, RawWorkbook& wb, DiagnosticSink& diag);
Status decode_names(ByteReader& r, RawWorkbook& wb, DiagnosticSink& diag);
Status decode_spill(ByteReader& r, RawWorkbook& wb, DiagnosticSink& diag);
Status decode_xref(ByteReader& r, RawWorkbook& wb, DiagnosticSink& diag);
Status decode_directives(ByteReader& r, RawWorkbook& wb, DiagnosticSink& diag);
Status decode_merges(ByteReader& r, RawWorkbook& wb, DiagnosticSink& diag);

// Expand one SST page's encoded stream into `out`, bounded by the page's
// declared decoded size. Defined in sst.cc.
Status expand_sst_page(ByteReader& enc, std::uint32_t declared_size,
                       std::vector<std::uint8_t>& out, DiagnosticSink& diag);

}  // namespace slate::detail

#endif  // SLATE_FORMAT_DECODE_INTERNAL_HH
