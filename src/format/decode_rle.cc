// slate/format/decode_rle.cc
//
// Expansion of a shared-string page's compressed byte stream. The codec is a
// compact LZ-style scheme (literal / run / prefix / copy / fill). Expansion runs
// against a destination sized to the page's declared decoded length, and every
// op validates its write against that length and every back-reference against
// the bytes already produced, so a page can never write past the buffer it was
// allotted or read output that does not yet exist.
#include <cstring>
#include <vector>

#include "decode_internal.hh"
#include "slate/format/wire.hh"

namespace slate::detail {
namespace {

// A back-reference op may only cite output already produced.
bool backref_ok(std::size_t off, std::size_t span, std::size_t pos) {
  return off + span <= pos;
}

}  // namespace

Status expand_sst_page(ByteReader& enc, std::uint32_t declared_size,
                       std::vector<std::uint8_t>& out, DiagnosticSink& diag) {
  out.assign(declared_size, 0);
  std::size_t pos = 0;

  auto remaining = [&]() { return declared_size - pos; };

  while (!enc.empty()) {
    std::uint8_t raw = 0;
    if (Status s = enc.u8(raw); !ok(s)) return s;

    switch (static_cast<wire::SstOp>(raw)) {
      case wire::SstOp::literal: {
        std::uint8_t len = 0;
        if (Status s = enc.u8(len); !ok(s)) return s;
        const std::uint8_t* src = nullptr;
        if (Status s = enc.take(len, src); !ok(s)) return s;
        if (len > remaining()) {
          diag.error(Status::bad_encoding, "literal exceeds page", enc.pos());
          return Status::bad_encoding;
        }
        std::memcpy(out.data() + pos, src, len);
        pos += len;
        break;
      }
      case wire::SstOp::run: {
        std::uint8_t len = 0, byte = 0;
        if (Status s = enc.u8(len); !ok(s)) return s;
        if (Status s = enc.u8(byte); !ok(s)) return s;
        if (len > remaining()) {
          diag.error(Status::bad_encoding, "run exceeds page", enc.pos());
          return Status::bad_encoding;
        }
        std::memset(out.data() + pos, byte, len);
        pos += len;
        break;
      }
      case wire::SstOp::prefix: {
        std::uint16_t back = 0;
        std::uint8_t share = 0, suffix_len = 0;
        if (Status s = enc.u16(back); !ok(s)) return s;
        if (Status s = enc.u8(share); !ok(s)) return s;
        if (Status s = enc.u8(suffix_len); !ok(s)) return s;
        const std::uint8_t* suffix = nullptr;
        if (Status s = enc.take(suffix_len, suffix); !ok(s)) return s;

        if (!backref_ok(back, share, pos)) {
          diag.error(Status::bad_encoding, "prefix backref past output", enc.pos());
          return Status::bad_encoding;
        }
        if (static_cast<std::size_t>(share) + suffix_len > remaining()) {
          diag.error(Status::bad_encoding, "prefix exceeds page", enc.pos());
          return Status::bad_encoding;
        }
        std::memmove(out.data() + pos, out.data() + back, share);
        pos += share;
        std::memcpy(out.data() + pos, suffix, suffix_len);
        pos += suffix_len;
        break;
      }
      case wire::SstOp::copy: {
        std::uint16_t distance = 0;
        std::uint8_t len = 0;
        if (Status s = enc.u16(distance); !ok(s)) return s;
        if (Status s = enc.u8(len); !ok(s)) return s;
        if (distance == 0 || distance > pos) {
          diag.error(Status::bad_encoding, "copy distance out of range", enc.pos());
          return Status::bad_encoding;
        }
        if (len > remaining()) {
          diag.error(Status::bad_encoding, "copy exceeds page", enc.pos());
          return Status::bad_encoding;
        }
        // Byte-by-byte so overlapping copies (distance < len) repeat correctly.
        std::size_t src = pos - distance;
        for (std::uint8_t i = 0; i < len; ++i) out[pos + i] = out[src + i];
        pos += len;
        break;
      }
      case wire::SstOp::fill: {
        std::uint8_t len = 0, b0 = 0, b1 = 0;
        if (Status s = enc.u8(len); !ok(s)) return s;
        if (Status s = enc.u8(b0); !ok(s)) return s;
        if (Status s = enc.u8(b1); !ok(s)) return s;
        if (len > remaining()) {
          diag.error(Status::bad_encoding, "fill exceeds page", enc.pos());
          return Status::bad_encoding;
        }
        for (std::uint8_t i = 0; i < len; ++i)
          out[pos + i] = (i & 1) ? b1 : b0;
        pos += len;
        break;
      }
      default:
        diag.error(Status::bad_encoding, "unknown sst op", enc.pos());
        return Status::bad_encoding;
    }
  }

  if (pos != declared_size) {
    diag.error(Status::size_mismatch, "page underfilled");
    return Status::size_mismatch;
  }
  return Status::ok;
}

}  // namespace slate::detail
