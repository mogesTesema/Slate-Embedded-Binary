// slate/format/decode.cc
//
// Framing: validate the fixed header (magic/version/CRC), walk the section
// table, bounds- and checksum-check every section, then hand each section body
// to its decoder. Section decoders live in sst.cc and sections.cc.
#include "slate/format/decode.hh"

#include <algorithm>
#include <array>
#include <cstring>

#include "decode_internal.hh"
#include "slate/format/wire.hh"
#include "slate/util/byte_reader.hh"
#include "slate/util/crc32.hh"

namespace slate {
namespace {

struct SectionEntry {
  wire::Section kind;
  std::uint16_t flags;
  std::uint32_t offset;
  std::uint32_t length;
  std::uint32_t checksum;
};

Status read_header(ByteReader& r, RawWorkbook& wb, std::uint16_t& section_count,
                   DiagnosticSink& diag) {
  if (r.size() < wire::kHeaderSize) {
    diag.error(Status::short_read, "file smaller than header");
    return Status::short_read;
  }

  const std::uint8_t* magic = nullptr;
  if (Status s = r.take(4, magic); !ok(s)) return s;
  if (std::memcmp(magic, wire::kMagic, 4) != 0) {
    diag.error(Status::bad_magic, "wrong magic");
    return Status::bad_magic;
  }

  std::uint16_t version = 0, flags = 0, reserved = 0;
  r.u16(version);
  r.u16(flags);
  r.u16(section_count);
  r.u16(reserved);

  std::uint32_t stored_crc = 0;
  r.u32(stored_crc);

  std::uint32_t actual_crc = Crc32::of(r.base(), 12);
  if (actual_crc != stored_crc) {
    diag.error(Status::bad_checksum, "header crc mismatch", 12);
    return Status::bad_checksum;
  }
  if (version != wire::kVersion) {
    diag.error(Status::bad_version, "unsupported version", 4);
    return Status::bad_version;
  }
  if (reserved != 0) {
    diag.warn(Status::bad_section_table, "reserved field nonzero", 10);
  }

  wb.version = version;
  wb.flags = flags;
  return Status::ok;
}

Status read_section_table(ByteReader& r, std::uint16_t section_count,
                          std::array<bool, 16>& seen,
                          std::vector<SectionEntry>& out, DiagnosticSink& diag) {
  const std::size_t table_bytes =
      static_cast<std::size_t>(section_count) * wire::kSectionEntrySize;
  const std::size_t table_end = wire::kHeaderSize + table_bytes;
  if (table_end > r.size()) {
    diag.error(Status::bad_section_table, "section table overruns file");
    return Status::bad_section_table;
  }

  for (std::uint16_t i = 0; i < section_count; ++i) {
    SectionEntry e{};
    std::uint16_t kind = 0;
    r.u16(kind);
    r.u16(e.flags);
    r.u32(e.offset);
    r.u32(e.length);
    r.u32(e.checksum);
    e.kind = static_cast<wire::Section>(kind);

    if (kind == 0 || kind >= seen.size()) {
      diag.warn(Status::bad_section_table, "unknown section kind, skipped");
      continue;
    }
    if (seen[kind]) {
      diag.error(Status::duplicate_section, "section kind repeated");
      return Status::duplicate_section;
    }
    seen[kind] = true;

    // The body must lie entirely after the table and within the file.
    if (e.offset < table_end || e.offset > r.size() ||
        e.length > r.size() - e.offset) {
      diag.error(Status::section_overrun, "section body out of range", e.offset);
      return Status::section_overrun;
    }

    std::uint32_t body_crc = Crc32::of(r.base() + e.offset, e.length);
    if (body_crc != e.checksum) {
      diag.error(Status::bad_checksum, "section crc mismatch", e.offset);
      return Status::bad_checksum;
    }
    out.push_back(e);
  }
  return Status::ok;
}

Status dispatch_section(const SectionEntry& e, ByteReader& file, RawWorkbook& wb,
                        DiagnosticSink& diag) {
  ByteReader body(nullptr, 0);
  if (Status s = file.window(e.offset, e.length, body); !ok(s)) return s;

  switch (e.kind) {
    case wire::Section::sheets:     return detail::decode_sheets(body, wb, diag);
    case wire::Section::sst:        return detail::decode_sst(body, wb, diag);
    case wire::Section::cells:      return detail::decode_cells(body, wb, diag);
    case wire::Section::formulas:   return detail::decode_formulas(body, wb, diag);
    case wire::Section::names:      return detail::decode_names(body, wb, diag);
    case wire::Section::spill:      return detail::decode_spill(body, wb, diag);
    case wire::Section::xref:       return detail::decode_xref(body, wb, diag);
    case wire::Section::directives: return detail::decode_directives(body, wb, diag);
    case wire::Section::merges:     return detail::decode_merges(body, wb, diag);
  }
  return Status::ok;  // unknown-but-tolerated
}

// Sections must be decoded in dependency order: SST feeds every StrId; formulas
// and spill must exist before cells (which cite formula/spill ids); cells must
// exist before xref and directives (which cite cell ordinals).
int decode_priority(wire::Section k) {
  switch (k) {
    case wire::Section::sst:        return 0;
    case wire::Section::sheets:     return 1;
    case wire::Section::formulas:   return 2;
    case wire::Section::spill:      return 3;
    case wire::Section::cells:      return 4;
    case wire::Section::names:      return 5;
    case wire::Section::merges:     return 6;
    case wire::Section::xref:       return 7;
    case wire::Section::directives: return 8;
  }
  return 9;
}

}  // namespace

Status decode_workbook(const std::uint8_t* data, std::size_t size,
                       RawWorkbook& out, DiagnosticSink& diag) {
  ByteReader r(data, size);

  std::uint16_t section_count = 0;
  if (Status s = read_header(r, out, section_count, diag); !ok(s)) return s;

  std::array<bool, 16> seen{};
  std::vector<SectionEntry> table;
  if (Status s = read_section_table(r, section_count, seen, table, diag); !ok(s))
    return s;

  std::sort(table.begin(), table.end(),
            [](const SectionEntry& a, const SectionEntry& b) {
              return decode_priority(a.kind) < decode_priority(b.kind);
            });

  ByteReader file(data, size);
  for (const SectionEntry& e : table) {
    if (Status s = dispatch_section(e, file, out, diag); !ok(s)) return s;
  }
  return Status::ok;
}

}  // namespace slate
