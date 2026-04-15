// tests/test_codec.cc
//
// Direct tests for the shared-string page codec (decode_rle.cc). Uses the
// internal decoder header so each op and its bounds checks can be exercised in
// isolation.
#include "framework.hh"

#include <vector>

#include "decode_internal.hh"  // src/format (added to the test include path)
#include "slate/format/wire.hh"

using namespace slate;

namespace {

struct Enc {
  std::vector<std::uint8_t> b;
  void op(wire::SstOp o) { b.push_back(static_cast<std::uint8_t>(o)); }
  void byte(std::uint8_t v) { b.push_back(v); }
  void u16(std::uint16_t v) { b.push_back(v & 0xFF); b.push_back(v >> 8); }
  void literal(const std::string& s) {
    op(wire::SstOp::literal); byte(static_cast<std::uint8_t>(s.size()));
    for (char c : s) byte(static_cast<std::uint8_t>(c));
  }
  void run(std::uint8_t len, std::uint8_t v) { op(wire::SstOp::run); byte(len); byte(v); }
  void copy(std::uint16_t dist, std::uint8_t len) { op(wire::SstOp::copy); u16(dist); byte(len); }
  void fill(std::uint8_t len, std::uint8_t a, std::uint8_t c) { op(wire::SstOp::fill); byte(len); byte(a); byte(c); }
};

std::string expand(const Enc& e, std::uint32_t declared, Status& st) {
  ByteReader r(e.b.data(), e.b.size());
  std::vector<std::uint8_t> out;
  DiagnosticSink diag;
  st = detail::expand_sst_page(r, declared, out, diag);
  return std::string(out.begin(), out.end());
}

}  // namespace

TEST("codec: literal") {
  Enc e; e.literal("abc");
  Status st; auto s = expand(e, 3, st);
  CHECK(ok(st)); CHECK(s == "abc");
}

TEST("codec: run") {
  Enc e; e.run(5, 'x');
  Status st; auto s = expand(e, 5, st);
  CHECK(ok(st)); CHECK(s == "xxxxx");
}

TEST("codec: overlapping copy repeats the pattern") {
  Enc e; e.literal("ab"); e.copy(2, 4);
  Status st; auto s = expand(e, 6, st);
  CHECK(ok(st)); CHECK(s == "ababab");
}

TEST("codec: fill alternates two bytes") {
  Enc e; e.fill(4, 'x', 'y');
  Status st; auto s = expand(e, 4, st);
  CHECK(ok(st)); CHECK(s == "xyxy");
}

TEST("codec: literal past declared size is rejected") {
  Enc e; e.literal("abcd");
  Status st; expand(e, 3, st);
  CHECK(st == Status::bad_encoding);
}

TEST("codec: copy distance past output is rejected") {
  Enc e; e.literal("ab"); e.copy(5, 2);  // distance 5 > produced 2
  Status st; expand(e, 4, st);
  CHECK(st == Status::bad_encoding);
}

TEST("codec: underfilled page is rejected") {
  Enc e; e.literal("ab");
  Status st; expand(e, 8, st);  // declared 8 but only 2 produced
  CHECK(st == Status::size_mismatch);
}
