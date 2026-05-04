// tools/mkworkbook/builder.cc
#include "builder.hh"

#include "slate/compile/parser.hh"
#include "slate/util/byte_writer.hh"
#include "slate/util/crc32.hh"

namespace slate::tools {

StrId WorkbookBuilder::add_string(std::string_view s) {
  return strings_.intern(s);
}

SheetId WorkbookBuilder::add_sheet(std::string_view name, std::uint32_t rows,
                                   std::uint32_t cols, bool visible) {
  BSheet sh;
  sh.name = add_string(name);
  sh.rows = rows;
  sh.cols = cols;
  sh.flags = visible ? wire::kSheetVisible : 0;
  sheets_.push_back(sh);
  return static_cast<SheetId>(sheets_.size() - 1);
}

FormulaId WorkbookBuilder::add_formula(const std::vector<std::uint8_t>& code) {
  formulas_.push_back(code);
  return static_cast<FormulaId>(formulas_.size() - 1);
}

SpillId WorkbookBuilder::add_spill(SheetId sheet, std::uint32_t row,
                                   std::uint32_t col, std::uint32_t rows,
                                   std::uint32_t cols, FormulaId source) {
  spills_.push_back(BSpill{sheet, row, col, rows, cols, source});
  return static_cast<SpillId>(spills_.size() - 1);
}

CellId WorkbookBuilder::push_cell(const BCell& c) {
  cells_.push_back(c);
  return static_cast<CellId>(cells_.size() - 1);
}

CellId WorkbookBuilder::number_cell(SheetId s, std::uint32_t row,
                                    std::uint32_t col, double v) {
  BCell c; c.sheet = s; c.row = row; c.col = col;
  c.tag = wire::CellTag::number; c.num = v;
  return push_cell(c);
}

CellId WorkbookBuilder::text_cell(SheetId s, std::uint32_t row,
                                  std::uint32_t col, std::string_view v) {
  BCell c; c.sheet = s; c.row = row; c.col = col;
  c.tag = wire::CellTag::text; c.str = add_string(v);
  return push_cell(c);
}

CellId WorkbookBuilder::bool_cell(SheetId s, std::uint32_t row,
                                  std::uint32_t col, bool v) {
  BCell c; c.sheet = s; c.row = row; c.col = col;
  c.tag = wire::CellTag::boolean; c.b = v ? 1 : 0;
  return push_cell(c);
}

CellId WorkbookBuilder::error_cell(SheetId s, std::uint32_t row,
                                   std::uint32_t col, wire::ErrCode e) {
  BCell c; c.sheet = s; c.row = row; c.col = col;
  c.tag = wire::CellTag::error; c.err = static_cast<std::uint8_t>(e);
  return push_cell(c);
}

CellId WorkbookBuilder::formula_cell(SheetId s, std::uint32_t row,
                                     std::uint32_t col, FormulaId f) {
  BCell c; c.sheet = s; c.row = row; c.col = col;
  c.tag = wire::CellTag::formula; c.formula = f;
  return push_cell(c);
}

CellId WorkbookBuilder::array_cell(SheetId s, std::uint32_t row,
                                   std::uint32_t col, SpillId sp) {
  BCell c; c.sheet = s; c.row = row; c.col = col;
  c.tag = wire::CellTag::array; c.spill = sp;
  return push_cell(c);
}

CellId WorkbookBuilder::formula_text_cell(SheetId sheet, std::uint32_t row,
                                          std::uint32_t col,
                                          const std::string& text) {
  ParseContext ctx;
  ctx.default_sheet = sheet;
  ctx.intern_string = [this](std::string_view sv) { return add_string(sv); };
  ctx.resolve_sheet = [this](std::string_view name) -> SheetId {
    for (std::size_t i = 0; i < sheets_.size(); ++i)
      if (strings_.view(sheets_[i].name) == name)
        return static_cast<SheetId>(i);
    return kNoSheet;
  };
  ctx.resolve_name = [this](std::string_view name) -> NameId {
    for (std::size_t i = 0; i < names_.size(); ++i)
      if (strings_.view(names_[i].name) == name)
        return static_cast<NameId>(i);
    return kNoName;
  };

  std::vector<std::uint8_t> code;
  std::string error;
  if (!ok(parse_formula(text, ctx, code, error))) return kNoCell;
  FormulaId fid = add_formula(code);
  return formula_cell(sheet, row, col, fid);
}

NameId WorkbookBuilder::add_name_cell(std::string_view name, SheetId s,
                                      std::uint32_t row, std::uint32_t col) {
  BName n; n.name = add_string(name); n.kind = wire::NameKind::cell;
  n.sheet = s; n.r0 = row; n.c0 = col; n.r1 = row; n.c1 = col;
  names_.push_back(n);
  return static_cast<NameId>(names_.size() - 1);
}

NameId WorkbookBuilder::add_name_range(std::string_view name, SheetId s,
                                       std::uint32_t r0, std::uint32_t c0,
                                       std::uint32_t r1, std::uint32_t c1) {
  BName n; n.name = add_string(name); n.kind = wire::NameKind::range;
  n.sheet = s; n.r0 = r0; n.c0 = c0; n.r1 = r1; n.c1 = c1;
  names_.push_back(n);
  return static_cast<NameId>(names_.size() - 1);
}

void WorkbookBuilder::add_edge(CellId from, CellId to) {
  edges_.push_back(BEdge{from, to});
}

void WorkbookBuilder::add_directive(wire::Directive op, std::uint32_t arg0,
                                    std::uint32_t arg1) {
  directives_.push_back(BDirective{op, arg0, arg1});
}

void WorkbookBuilder::add_merge(SheetId sheet, std::uint32_t r0,
                                std::uint32_t c0, std::uint32_t r1,
                                std::uint32_t c1) {
  merges_.push_back(BMerge{sheet, r0, c0, r1, c1});
}

// --- section encoders -------------------------------------------------------

std::vector<std::uint8_t> WorkbookBuilder::encode_sst() const {
  // One page holding every interned string, encoded as literal runs (chunks of
  // at most 255 bytes). Slot directory follows the encoded stream.
  std::string concat;
  std::vector<std::pair<std::uint32_t, std::uint32_t>> slots;
  for (std::uint32_t i = 0; i < strings_.size(); ++i) {
    std::string_view sv = strings_.view(i);
    slots.emplace_back(static_cast<std::uint32_t>(concat.size()),
                       static_cast<std::uint32_t>(sv.size()));
    concat.append(sv);
  }

  ByteWriter enc;
  std::size_t pos = 0;
  while (pos < concat.size()) {
    std::size_t chunk = std::min<std::size_t>(255, concat.size() - pos);
    enc.u8(static_cast<std::uint8_t>(wire::SstOp::literal));
    enc.u8(static_cast<std::uint8_t>(chunk));
    enc.bytes(concat.data() + pos, chunk);
    pos += chunk;
  }

  ByteWriter w;
  w.u16(1);  // page_count
  std::uint16_t flags = sst_recyclable_ ? wire::kPageRecyclable : 0;
  w.u16(flags);
  w.u16(static_cast<std::uint16_t>(slots.size()));
  w.u32(static_cast<std::uint32_t>(concat.size()));  // declared_size
  w.u32(static_cast<std::uint32_t>(enc.size()));      // enc_len
  w.bytes(enc.data().data(), enc.size());
  for (auto& s : slots) { w.u32(s.first); w.u32(s.second); }
  return w.data();
}

std::vector<std::uint8_t> WorkbookBuilder::encode_sheets() const {
  ByteWriter w;
  w.u16(static_cast<std::uint16_t>(sheets_.size()));
  for (const BSheet& s : sheets_) {
    w.u32(s.name); w.u32(s.rows); w.u32(s.cols); w.u16(s.flags);
  }
  return w.data();
}

std::vector<std::uint8_t> WorkbookBuilder::encode_formulas() const {
  ByteWriter w;
  w.u32(static_cast<std::uint32_t>(formulas_.size()));
  for (const auto& code : formulas_) {
    w.u32(static_cast<std::uint32_t>(code.size()));
    w.bytes(code.data(), code.size());
  }
  return w.data();
}

std::vector<std::uint8_t> WorkbookBuilder::encode_spill() const {
  ByteWriter w;
  w.u32(static_cast<std::uint32_t>(spills_.size()));
  for (const BSpill& s : spills_) {
    w.u16(s.sheet); w.u32(s.row); w.u32(s.col);
    w.u32(s.rows); w.u32(s.cols); w.u32(s.source);
  }
  return w.data();
}

std::vector<std::uint8_t> WorkbookBuilder::encode_cells() const {
  ByteWriter w;
  w.u32(static_cast<std::uint32_t>(cells_.size()));
  for (const BCell& c : cells_) {
    w.u16(c.sheet); w.u32(c.row); w.u32(c.col);
    w.u8(static_cast<std::uint8_t>(c.tag));
    switch (c.tag) {
      case wire::CellTag::empty: break;
      case wire::CellTag::number: w.f64(c.num); break;
      case wire::CellTag::text: w.u32(c.str); break;
      case wire::CellTag::boolean: w.u8(c.b); break;
      case wire::CellTag::error: w.u8(c.err); break;
      case wire::CellTag::formula: w.u32(c.formula); break;
      case wire::CellTag::array: w.u32(c.spill); break;
    }
  }
  return w.data();
}

std::vector<std::uint8_t> WorkbookBuilder::encode_names() const {
  ByteWriter w;
  w.u16(static_cast<std::uint16_t>(names_.size()));
  for (const BName& n : names_) {
    w.u32(n.name);
    w.u8(static_cast<std::uint8_t>(n.kind));
    w.u16(n.sheet);
    w.u32(n.r0); w.u32(n.c0);
    if (n.kind == wire::NameKind::range) { w.u32(n.r1); w.u32(n.c1); }
  }
  return w.data();
}

std::vector<std::uint8_t> WorkbookBuilder::encode_xref() const {
  ByteWriter w;
  w.u32(static_cast<std::uint32_t>(edges_.size()));
  for (const BEdge& e : edges_) { w.u32(e.from); w.u32(e.to); }
  return w.data();
}

std::vector<std::uint8_t> WorkbookBuilder::encode_merges() const {
  ByteWriter w;
  w.u16(static_cast<std::uint16_t>(merges_.size()));
  for (const BMerge& m : merges_) {
    w.u16(m.sheet); w.u32(m.r0); w.u32(m.c0); w.u32(m.r1); w.u32(m.c1);
  }
  return w.data();
}

std::vector<std::uint8_t> WorkbookBuilder::encode_directives() const {
  ByteWriter w;
  w.u16(static_cast<std::uint16_t>(directives_.size()));
  for (const BDirective& d : directives_) {
    w.u16(static_cast<std::uint16_t>(d.op)); w.u32(d.arg0); w.u32(d.arg1);
  }
  return w.data();
}

// --- assembly ---------------------------------------------------------------

std::vector<std::uint8_t> WorkbookBuilder::build() const {
  struct Sec { wire::Section kind; std::vector<std::uint8_t> body; };
  std::vector<Sec> secs;
  auto add = [&](wire::Section k, std::vector<std::uint8_t> b, bool always) {
    if (always || b.size() > /*count field only*/ 0) secs.push_back({k, std::move(b)});
  };
  // SST and sheets are always present; the rest only when populated.
  add(wire::Section::sst, encode_sst(), true);
  add(wire::Section::sheets, encode_sheets(), true);
  if (!formulas_.empty()) add(wire::Section::formulas, encode_formulas(), false);
  if (!spills_.empty()) add(wire::Section::spill, encode_spill(), false);
  add(wire::Section::cells, encode_cells(), true);
  if (!names_.empty()) add(wire::Section::names, encode_names(), false);
  if (!merges_.empty()) add(wire::Section::merges, encode_merges(), false);
  if (!edges_.empty()) add(wire::Section::xref, encode_xref(), false);
  if (!directives_.empty())
    add(wire::Section::directives, encode_directives(), false);

  const std::uint16_t count = static_cast<std::uint16_t>(secs.size());
  const std::size_t bodies_start = wire::kHeaderSize +
                                   static_cast<std::size_t>(count) *
                                       wire::kSectionEntrySize;

  // Header (crc back-patched).
  ByteWriter w;
  w.bytes(wire::kMagic, 4);
  w.u16(wire::kVersion);
  w.u16(0);           // flags
  w.u16(count);
  w.u16(0);           // reserved
  const std::size_t crc_at = w.size();
  w.u32(0);           // header crc placeholder

  // Section table with running offsets.
  std::size_t offset = bodies_start;
  for (const Sec& s : secs) {
    std::uint32_t body_crc = Crc32::of(s.body.data(), s.body.size());
    w.u16(static_cast<std::uint16_t>(s.kind));
    w.u16(0);  // flags
    w.u32(static_cast<std::uint32_t>(offset));
    w.u32(static_cast<std::uint32_t>(s.body.size()));
    w.u32(body_crc);
    offset += s.body.size();
  }

  // Bodies.
  for (const Sec& s : secs) w.bytes(s.body.data(), s.body.size());

  // Back-patch the header crc over the first 12 bytes.
  std::uint32_t header_crc = Crc32::of(w.data().data(), 12);
  w.patch_u32(crc_at, header_crc);
  return w.data();
}

}  // namespace slate::tools
