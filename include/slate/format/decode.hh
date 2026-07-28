// slate/format/decode.hh
//
// Public entry point for turning workbook bytes into a RawWorkbook. Decoding is
// total: any malformed input yields a Status and a populated diagnostic sink
// rather than a crash. Structural invariants (section bounds, checksums, string
// directory) are enforced here; semantic linking happens in later layers.
#ifndef SLATE_FORMAT_DECODE_HH
#define SLATE_FORMAT_DECODE_HH

#include <cstddef>
#include <cstdint>

#include "slate/format/raw.hh"
#include "slate/util/diagnostic.hh"
#include "slate/util/status.hh"

namespace slate {

// Decode a whole workbook. On success returns Status::ok and fills `out`. On
// failure returns the first failing status and records detail in `diag`.
Status decode_workbook(const std::uint8_t* data, std::size_t size,
                       RawWorkbook& out, DiagnosticSink& diag);

}  // namespace slate

#endif  // SLATE_FORMAT_DECODE_HH
