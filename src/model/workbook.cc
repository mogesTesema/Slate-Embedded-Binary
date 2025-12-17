// slate/model/workbook.cc
#include "slate/model/workbook.hh"

namespace slate {

std::string_view Workbook::resolve_string(StrId id) const {
  if (id >= string_dir.size()) return {};
  const RawWorkbook::StrRef& ref = string_dir[id];
  if (ref.page >= sst_pages.size()) return {};
  const RawSstPage& page = sst_pages[ref.page];
  if (ref.slot >= page.slots.size()) return {};
  const RawSstPage::Slot& slot = page.slots[ref.slot];
  if (slot.offset > page.bytes.size() ||
      slot.length > page.bytes.size() - slot.offset)
    return {};
  return std::string_view(
      reinterpret_cast<const char*>(page.bytes.data()) + slot.offset,
      slot.length);
}

CellRef Workbook::resolve_merge(const CellRef& ref) const {
  // Merge regions are few; a linear scan is cheaper than maintaining a per-cell
  // map, and a cell belongs to at most one region.
  for (const RawMerge& m : merges) {
    if (m.sheet != ref.sheet) continue;
    if (ref.row < m.r0 || ref.row > m.r1 || ref.col < m.c0 || ref.col > m.c1)
      continue;
    if (ref.row == m.r0 && ref.col == m.c0) return ref;  // the anchor itself
    return CellRef{m.sheet, m.r0, m.c0};
  }
  return ref;
}

void Workbook::materialize_cells(const std::vector<RawCell>& cells) {
  for (const RawCell& rc : cells) {
    CellRef ref{rc.sheet, rc.row, rc.col};
    CellId id = grid.add(ref, rc.tag);
    CellSlot& slot = grid.at(id);

    switch (rc.tag) {
      case wire::CellTag::empty:
        slot.value.clear();
        break;
      case wire::CellTag::number:
        slot.value = Value::number(rc.num);
        break;
      case wire::CellTag::text:
        slot.value = Value::text(rc.str);
        break;
      case wire::CellTag::boolean:
        slot.value = Value::boolean(rc.boolean != 0);
        break;
      case wire::CellTag::error:
        slot.value = Value::error(static_cast<wire::ErrCode>(rc.err));
        break;
      case wire::CellTag::formula:
        slot.formula = rc.formula;
        slot.value.clear();  // computed during recalc
        break;
      case wire::CellTag::array:
        slot.spill = rc.spill;  // anchor; results filled during recalc
        slot.value.clear();
        break;
    }
  }
}

Status Workbook::link(RawWorkbook&& raw, Workbook& out, DiagnosticSink& diag) {
  // Move owned storage across.
  out.sst_pages = std::move(raw.sst_pages);
  out.string_dir = std::move(raw.string_dir);
  out.formulas = std::move(raw.formulas);
  out.names = std::move(raw.names);
  out.spills = std::move(raw.spills);
  out.edges = std::move(raw.edges);
  out.directives = std::move(raw.directives);
  out.merges = std::move(raw.merges);

  // Build the sheet table.
  for (const RawSheet& rs : raw.sheets)
    out.sheets.add(rs.name, rs.rows, rs.cols, rs.visible());

  // Cross-check that spill anchors name a formula source that exists; this is a
  // model-level invariant the decoder already enforced, re-asserted cheaply.
  for (const RawSpill& sp : out.spills) {
    if (sp.source >= out.formulas.size()) {
      diag.error(Status::bad_reference, "spill source vanished during link");
      return Status::bad_reference;
    }
  }

  out.cell_record_count = static_cast<std::uint32_t>(raw.cells.size());
  out.materialize_cells(raw.cells);
  return Status::ok;
}

}  // namespace slate
