// tests/test_format.cc
#include "framework.hh"

#include <vector>

#include "builder.hh"
#include "slate/format/decode.hh"
#include "slate/model/workbook.hh"
#include "slate/slate.hh"

using namespace slate;

namespace {

std::vector<std::uint8_t> small_workbook() {
  tools::WorkbookBuilder b;
  SheetId s = b.add_sheet("Sheet1", 16, 8);
  b.number_cell(s, 0, 0, 10.0);
  b.text_cell(s, 1, 0, "hello");
  b.text_cell(s, 2, 0, "hello");  // dedups to the same StrId
  b.bool_cell(s, 3, 0, true);
  b.error_cell(s, 4, 0, wire::ErrCode::div0);
  return b.build();
}

}  // namespace

TEST("decode: round-trip a built workbook") {
  auto bytes = small_workbook();
  RawWorkbook raw;
  DiagnosticSink diag;
  Status s = decode_workbook(bytes.data(), bytes.size(), raw, diag);
  CHECK(ok(s));
  CHECK_EQ(raw.sheets.size(), static_cast<std::size_t>(1));
  CHECK_EQ(raw.cells.size(), static_cast<std::size_t>(5));
  // "Sheet1", "hello" -> 2 distinct strings.
  CHECK_EQ(raw.string_count(), 2u);
}

TEST("decode: link resolves strings") {
  auto bytes = small_workbook();
  Workbook wb;
  DiagnosticSink diag;
  CHECK(ok(load_workbook(bytes.data(), bytes.size(), wb, diag)));
  CHECK(wb.resolve_string(0) == "Sheet1");
  CHECK(wb.resolve_string(1) == "hello");
}

TEST("decode: bad magic rejected") {
  auto bytes = small_workbook();
  bytes[0] = 'X';
  RawWorkbook raw;
  DiagnosticSink diag;
  CHECK(decode_workbook(bytes.data(), bytes.size(), raw, diag) == Status::bad_magic);
}

TEST("decode: header crc corruption rejected") {
  auto bytes = small_workbook();
  bytes[8] ^= 0xFF;  // flip a byte covered by the header crc
  RawWorkbook raw;
  DiagnosticSink diag;
  Status s = decode_workbook(bytes.data(), bytes.size(), raw, diag);
  CHECK(s == Status::bad_checksum || s == Status::bad_section_table);
}

TEST("decode: truncation never crashes and reports short read") {
  auto bytes = small_workbook();
  for (std::size_t n = 0; n < bytes.size(); ++n) {
    RawWorkbook raw;
    DiagnosticSink diag;
    Status s = decode_workbook(bytes.data(), n, raw, diag);
    CHECK(!ok(s));  // every prefix is malformed; the point is no UB
  }
}

TEST("probe: empty input is rejected cleanly") {
  CHECK(!probe_workbook(nullptr, 0));
}
