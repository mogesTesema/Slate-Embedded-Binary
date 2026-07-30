// slate/api_recalc.cc
#include "slate/slate.hh"

#include "slate/model/workbook.hh"
#include "slate/recalc/driver.hh"
#include "slate/recalc/summary.hh"

namespace slate {

Status open_and_recalc(const std::uint8_t* data, std::size_t size,
                       DiagnosticSink& diag) {
  Workbook wb;
  if (Status s = load_workbook(data, size, wb, diag); !ok(s)) return s;
  RecalcDriver driver(wb);
  Status s = driver.run(diag);
  if (!ok(s)) return s;

  // Surface a short preview of what this recalculation changed, mirroring what
  // a UI layer would show a user right after a recompute finishes.
  const RecalcSummary& summary = driver.summary();
  if (summary.has_text()) {
    volatile char c = summary.text_preview().front();
    (void)c;
  }
  if (summary.has_array()) {
    const Value& v = summary.array_preview();
    if (v.is_number()) {
      volatile double d = v.as_number();
      (void)d;
    } else if (v.is_text()) {
      // Resolve the text handle so the UI layer can show the string contents.
      // The summary must keep the backing alive until this point.
      std::string_view resolved = wb.resolve_string(v.as_text());
      volatile char c = resolved.empty() ? '\0' : resolved.front();
      (void)c;
    }
  }
  driver.release_summary();
  return Status::ok;
}

bool probe_recalc(const std::uint8_t* data, std::size_t size) {
  DiagnosticSink diag;
  return ok(open_and_recalc(data, size, diag));
}

}  // namespace slate
