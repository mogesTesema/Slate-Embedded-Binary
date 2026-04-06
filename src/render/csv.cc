// slate/render/csv.cc
#include "slate/render/csv.hh"

#include <string>

#include "slate/model/number_format.hh"

namespace slate {
namespace {

std::string error_text(wire::ErrCode e) {
  switch (e) {
    case wire::ErrCode::div0: return "#DIV/0!";
    case wire::ErrCode::value: return "#VALUE!";
    case wire::ErrCode::ref: return "#REF!";
    case wire::ErrCode::name: return "#NAME?";
    case wire::ErrCode::num: return "#NUM!";
    case wire::ErrCode::na: return "#N/A";
    case wire::ErrCode::cycle: return "#CYCLE!";
    case wire::ErrCode::null: return "#NULL!";
  }
  return "#ERR!";
}

std::string render_value(const Workbook& wb, const Value& v) {
  switch (v.tag()) {
    case ValueTag::empty: return "";
    case ValueTag::number: return format_number(v.as_number(), "");
    case ValueTag::boolean: return v.as_bool() ? "TRUE" : "FALSE";
    case ValueTag::error: return error_text(v.as_error());
    case ValueTag::text: return std::string(wb.resolve_string(v.as_text()));
    case ValueTag::array: return "";  // arrays are shown at their spill cells
  }
  return "";
}

// Quote a field if it contains a comma, quote, or newline (RFC 4180 style).
std::string escape(const std::string& field) {
  bool needs = field.find_first_of(",\"\n") != std::string::npos;
  if (!needs) return field;
  std::string out = "\"";
  for (char c : field) {
    if (c == '"') out += "\"\"";
    else out += c;
  }
  out += "\"";
  return out;
}

}  // namespace

std::string render_sheet_csv(const Workbook& wb, SheetId sheet) {
  if (!wb.sheets.in_range(sheet)) return "";

  // Find the occupied extent of this sheet.
  bool any = false;
  std::uint32_t max_row = 0, max_col = 0;
  for (CellId id = 0; id < wb.grid.size(); ++id) {
    const CellSlot& s = wb.grid.at(id);
    if (s.ref.sheet != sheet) continue;
    any = true;
    if (s.ref.row > max_row) max_row = s.ref.row;
    if (s.ref.col > max_col) max_col = s.ref.col;
  }
  if (!any) return "";

  std::string out;
  for (std::uint32_t r = 0; r <= max_row; ++r) {
    for (std::uint32_t c = 0; c <= max_col; ++c) {
      if (c) out += ',';
      CellId id = wb.grid.find(CellRef{sheet, r, c});
      if (id != kNoCell) out += escape(render_value(wb, wb.grid.at(id).value));
    }
    out += '\n';
  }
  return out;
}

}  // namespace slate
