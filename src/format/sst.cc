// slate/format/sst.cc
//
// Shared-string pool decoding: read each page header, expand its compressed
// stream (see decode_rle.cc), then read the slot directory that indexes
// individual strings within the expanded buffer and register them in the flat
// string directory. Page byte expansion itself lives in decode_rle.cc.
#include <vector>

#include "decode_internal.hh"
#include "slate/format/wire.hh"

namespace slate::detail {

Status decode_sst(ByteReader& r, RawWorkbook& wb, DiagnosticSink& diag) {
  std::uint16_t page_count = 0;
  if (Status s = r.u16(page_count); !ok(s)) return s;

  wb.sst_pages.reserve(page_count);
  for (std::uint16_t p = 0; p < page_count; ++p) {
    std::uint16_t flags = 0, string_count = 0;
    std::uint32_t declared = 0, enc_len = 0;
    if (Status s = r.u16(flags); !ok(s)) return s;
    if (Status s = r.u16(string_count); !ok(s)) return s;
    if (Status s = r.u32(declared); !ok(s)) return s;
    if (Status s = r.u32(enc_len); !ok(s)) return s;

    const std::uint8_t* enc_bytes = nullptr;
    if (Status s = r.take(enc_len, enc_bytes); !ok(s)) return s;

    RawSstPage page;
    page.recyclable = (flags & wire::kPageRecyclable) != 0;
    page.declared_size = declared;

    ByteReader enc(enc_bytes, enc_len);
    if (Status s = expand_sst_page(enc, declared, page.bytes, diag); !ok(s))
      return s;

    page.slots.reserve(string_count);
    for (std::uint16_t i = 0; i < string_count; ++i) {
      std::uint32_t off = 0, len = 0;
      if (Status s = r.u32(off); !ok(s)) return s;
      if (Status s = r.u32(len); !ok(s)) return s;
      if (off > page.bytes.size() || len > page.bytes.size() - off) {
        diag.error(Status::section_overrun, "sst slot out of page");
        return Status::section_overrun;
      }
      page.slots.push_back(RawSstPage::Slot{off, len});
    }

    std::uint32_t page_index = static_cast<std::uint32_t>(wb.sst_pages.size());
    for (std::uint32_t i = 0; i < page.slots.size(); ++i)
      wb.string_dir.push_back(RawWorkbook::StrRef{page_index, i});
    wb.sst_pages.push_back(std::move(page));
  }
  return Status::ok;
}

}  // namespace slate::detail
