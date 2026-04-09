// slate/format/sections.cc
//
// Decoders for the non-SST sections. Each validates its own references against
// the tables decoded earlier (see decode_priority in decode.cc): StrIds against
// the string directory, formula/spill ids against their tables, xref endpoints
// and directive targets against the cell and sheet tables.
#include "decode_internal.hh"
#include "slate/format/wire.hh"

namespace slate::detail {
namespace {

bool valid_str(const RawWorkbook& wb, StrId id) {
  return id == kNoStr || id < wb.string_count();
}

}  // namespace

Status decode_sheets(ByteReader& r, RawWorkbook& wb, DiagnosticSink& diag) {
  std::uint16_t count = 0;
  if (Status s = r.u16(count); !ok(s)) return s;

  wb.sheets.reserve(count);
  for (std::uint16_t i = 0; i < count; ++i) {
    RawSheet sh;
    std::uint32_t name = 0;
    if (Status s = r.u32(name); !ok(s)) return s;
    if (Status s = r.u32(sh.rows); !ok(s)) return s;
    if (Status s = r.u32(sh.cols); !ok(s)) return s;
    if (Status s = r.u16(sh.flags); !ok(s)) return s;
    sh.name = name;
    if (!valid_str(wb, sh.name)) {
      diag.error(Status::bad_reference, "sheet name strid out of range");
      return Status::bad_reference;
    }
    wb.sheets.push_back(sh);
  }
  return Status::ok;
}

Status decode_formulas(ByteReader& r, RawWorkbook& wb, DiagnosticSink& diag) {
  std::uint32_t count = 0;
  if (Status s = r.u32(count); !ok(s)) return s;

  wb.formulas.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    std::uint32_t code_len = 0;
    if (Status s = r.u32(code_len); !ok(s)) return s;
    const std::uint8_t* code = nullptr;
    if (Status s = r.take(code_len, code); !ok(s)) {
      diag.error(Status::short_read, "formula code truncated");
      return s;
    }
    RawFormula f;
    f.code.assign(code, code + code_len);
    wb.formulas.push_back(std::move(f));
  }
  return Status::ok;
}

Status decode_spill(ByteReader& r, RawWorkbook& wb, DiagnosticSink& diag) {
  std::uint32_t count = 0;
  if (Status s = r.u32(count); !ok(s)) return s;

  wb.spills.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    RawSpill sp;
    std::uint16_t sheet = 0;
    std::uint32_t source = 0;
    if (Status s = r.u16(sheet); !ok(s)) return s;
    if (Status s = r.u32(sp.row); !ok(s)) return s;
    if (Status s = r.u32(sp.col); !ok(s)) return s;
    if (Status s = r.u32(sp.rows); !ok(s)) return s;
    if (Status s = r.u32(sp.cols); !ok(s)) return s;
    if (Status s = r.u32(source); !ok(s)) return s;
    sp.sheet = sheet;
    sp.source = source;
    if (sheet >= wb.sheets.size()) {
      diag.error(Status::unknown_sheet, "spill sheet out of range");
      return Status::unknown_sheet;
    }
    if (source >= wb.formulas.size()) {
      diag.error(Status::bad_reference, "spill source formula out of range");
      return Status::bad_reference;
    }
    wb.spills.push_back(sp);
  }
  return Status::ok;
}

Status decode_cells(ByteReader& r, RawWorkbook& wb, DiagnosticSink& diag) {
  std::uint32_t count = 0;
  if (Status s = r.u32(count); !ok(s)) return s;

  wb.cells.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    RawCell c;
    std::uint16_t sheet = 0;
    std::uint8_t tag = 0;
    if (Status s = r.u16(sheet); !ok(s)) return s;
    if (Status s = r.u32(c.row); !ok(s)) return s;
    if (Status s = r.u32(c.col); !ok(s)) return s;
    if (Status s = r.u8(tag); !ok(s)) return s;
    c.sheet = sheet;
    c.tag = static_cast<wire::CellTag>(tag);

    if (sheet >= wb.sheets.size()) {
      diag.error(Status::unknown_sheet, "cell sheet out of range");
      return Status::unknown_sheet;
    }

    switch (c.tag) {
      case wire::CellTag::empty:
        break;
      case wire::CellTag::number:
        if (Status s = r.f64(c.num); !ok(s)) return s;
        break;
      case wire::CellTag::text: {
        std::uint32_t id = 0;
        if (Status s = r.u32(id); !ok(s)) return s;
        c.str = id;
        if (!valid_str(wb, id)) {
          diag.error(Status::bad_reference, "text cell strid out of range");
          return Status::bad_reference;
        }
        break;
      }
      case wire::CellTag::boolean:
        if (Status s = r.u8(c.boolean); !ok(s)) return s;
        break;
      case wire::CellTag::error:
        if (Status s = r.u8(c.err); !ok(s)) return s;
        break;
      case wire::CellTag::formula: {
        std::uint32_t id = 0;
        if (Status s = r.u32(id); !ok(s)) return s;
        c.formula = id;
        if (id >= wb.formulas.size()) {
          diag.error(Status::bad_reference, "cell formula id out of range");
          return Status::bad_reference;
        }
        break;
      }
      case wire::CellTag::array: {
        std::uint32_t id = 0;
        if (Status s = r.u32(id); !ok(s)) return s;
        c.spill = id;
        if (id >= wb.spills.size()) {
          diag.error(Status::bad_reference, "cell spill id out of range");
          return Status::bad_reference;
        }
        break;
      }
      default:
        diag.error(Status::bad_reference, "unknown cell tag");
        return Status::bad_reference;
    }
    wb.cells.push_back(c);
  }
  return Status::ok;
}

Status decode_names(ByteReader& r, RawWorkbook& wb, DiagnosticSink& diag) {
  std::uint16_t count = 0;
  if (Status s = r.u16(count); !ok(s)) return s;

  wb.names.reserve(count);
  for (std::uint16_t i = 0; i < count; ++i) {
    RawName n;
    std::uint32_t name = 0;
    std::uint16_t sheet = 0;
    std::uint8_t kind = 0;
    if (Status s = r.u32(name); !ok(s)) return s;
    if (Status s = r.u8(kind); !ok(s)) return s;
    if (Status s = r.u16(sheet); !ok(s)) return s;
    n.name = name;
    n.sheet = sheet;
    n.kind = static_cast<wire::NameKind>(kind);

    if (!valid_str(wb, name)) {
      diag.error(Status::bad_reference, "defined name strid out of range");
      return Status::bad_reference;
    }
    if (sheet >= wb.sheets.size()) {
      diag.error(Status::unknown_sheet, "defined name sheet out of range");
      return Status::unknown_sheet;
    }

    if (Status s = r.u32(n.r0); !ok(s)) return s;
    if (Status s = r.u32(n.c0); !ok(s)) return s;
    if (n.kind == wire::NameKind::range) {
      if (Status s = r.u32(n.r1); !ok(s)) return s;
      if (Status s = r.u32(n.c1); !ok(s)) return s;
    } else {
      n.r1 = n.r0;
      n.c1 = n.c0;
    }
    wb.names.push_back(n);
  }
  return Status::ok;
}

Status decode_xref(ByteReader& r, RawWorkbook& wb, DiagnosticSink& diag) {
  std::uint32_t count = 0;
  if (Status s = r.u32(count); !ok(s)) return s;

  const std::uint32_t cell_count = static_cast<std::uint32_t>(wb.cells.size());
  wb.edges.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    RawEdge e;
    if (Status s = r.u32(e.from); !ok(s)) return s;
    if (Status s = r.u32(e.to); !ok(s)) return s;
    if (e.from >= cell_count || e.to >= cell_count) {
      diag.error(Status::bad_reference, "xref edge cites unknown cell");
      return Status::bad_reference;
    }
    wb.edges.push_back(e);
  }
  return Status::ok;
}

Status decode_merges(ByteReader& r, RawWorkbook& wb, DiagnosticSink& diag) {
  std::uint16_t count = 0;
  if (Status s = r.u16(count); !ok(s)) return s;

  wb.merges.reserve(count);
  for (std::uint16_t i = 0; i < count; ++i) {
    RawMerge m;
    std::uint16_t sheet = 0;
    if (Status s = r.u16(sheet); !ok(s)) return s;
    if (Status s = r.u32(m.r0); !ok(s)) return s;
    if (Status s = r.u32(m.c0); !ok(s)) return s;
    if (Status s = r.u32(m.r1); !ok(s)) return s;
    if (Status s = r.u32(m.c1); !ok(s)) return s;
    m.sheet = sheet;
    if (sheet >= wb.sheets.size()) {
      diag.error(Status::unknown_sheet, "merge sheet out of range");
      return Status::unknown_sheet;
    }
    if (m.r1 < m.r0 || m.c1 < m.c0) {
      diag.error(Status::bad_reference, "merge region inverted");
      return Status::bad_reference;
    }
    wb.merges.push_back(m);
  }
  return Status::ok;
}

Status decode_directives(ByteReader& r, RawWorkbook& wb, DiagnosticSink& diag) {
  std::uint16_t count = 0;
  if (Status s = r.u16(count); !ok(s)) return s;

  wb.directives.reserve(count);
  for (std::uint16_t i = 0; i < count; ++i) {
    std::uint16_t op = 0;
    RawDirective d;
    if (Status s = r.u16(op); !ok(s)) return s;
    if (Status s = r.u32(d.arg0); !ok(s)) return s;
    if (Status s = r.u32(d.arg1); !ok(s)) return s;
    d.op = static_cast<wire::Directive>(op);

    switch (d.op) {
      case wire::Directive::set_dirty:
        if (d.arg0 >= wb.cells.size()) {
          diag.error(Status::bad_reference, "dirty directive cites unknown cell");
          return Status::bad_reference;
        }
        break;
      case wire::Directive::sheet_visible:
        if (d.arg0 >= wb.sheets.size()) {
          diag.error(Status::unknown_sheet, "visibility directive unknown sheet");
          return Status::unknown_sheet;
        }
        break;
      case wire::Directive::recalc_pass:
        break;
      default:
        diag.warn(Status::bad_section_table, "unknown directive, ignored");
        break;
    }
    wb.directives.push_back(d);
  }
  return Status::ok;
}

}  // namespace slate::detail
