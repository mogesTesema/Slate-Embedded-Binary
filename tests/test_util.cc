// tests/test_util.cc
#include "framework.hh"

#include "slate/util/arena.hh"
#include "slate/util/byte_reader.hh"
#include "slate/util/byte_writer.hh"
#include "slate/util/crc32.hh"
#include "slate/util/intern_table.hh"
#include "slate/util/small_vector.hh"

using namespace slate;

TEST("arena: allocate and rewind reuses space") {
  Arena a(128);
  ArenaMarker m = a.mark();
  int* p = a.alloc_array<int>(50);
  for (int i = 0; i < 50; ++i) p[i] = i;
  CHECK(a.bytes_used() >= 50 * sizeof(int));
  a.rewind(m);
  CHECK_EQ(a.bytes_used(), static_cast<std::size_t>(0));
  // A fresh allocation should reuse the rewound space.
  int* q = a.alloc_array<int>(10);
  q[0] = 7;
  CHECK_EQ(q[0], 7);
}

TEST("arena: nested markers unwind in order") {
  Arena a(64);
  auto m0 = a.mark();
  a.alloc_array<char>(40);
  auto m1 = a.mark();
  a.alloc_array<char>(200);  // forces a new block
  a.rewind(m1);
  a.rewind(m0);
  CHECK_EQ(a.bytes_used(), static_cast<std::size_t>(0));
}

TEST("crc32: known vector") {
  const char* s = "123456789";
  std::uint32_t c = Crc32::of(reinterpret_cast<const std::uint8_t*>(s), 9);
  CHECK_EQ(c, 0xCBF43926u);  // standard CRC-32/ISO-HDLC check value
}

TEST("small_vector: grows past inline capacity") {
  SmallVector<int, 4> v;
  for (int i = 0; i < 100; ++i) v.push_back(i * 2);
  CHECK_EQ(v.size(), static_cast<std::size_t>(100));
  CHECK_EQ(v[0], 0);
  CHECK_EQ(v[99], 198);
  v.pop_back();
  CHECK_EQ(v.size(), static_cast<std::size_t>(99));
}

TEST("small_vector: copy is independent") {
  SmallVector<int, 2> a;
  a.push_back(1); a.push_back(2); a.push_back(3);
  SmallVector<int, 2> b = a;
  b.push_back(4);
  CHECK_EQ(a.size(), static_cast<std::size_t>(3));
  CHECK_EQ(b.size(), static_cast<std::size_t>(4));
}

TEST("intern_table: dedup returns same id") {
  InternTable t;
  auto a = t.intern("hello");
  auto b = t.intern("world");
  auto c = t.intern("hello");
  CHECK_EQ(a, c);
  CHECK(a != b);
  CHECK(t.view(a) == "hello");
  CHECK(t.view(b) == "world");
}

TEST("byte reader/writer round-trip") {
  ByteWriter w;
  w.u8(0xAB);
  w.u16(0x1234);
  w.u32(0xDEADBEEF);
  w.f64(3.14159);
  ByteReader r(w.data().data(), w.size());
  std::uint8_t b8; std::uint16_t b16; std::uint32_t b32; double d;
  CHECK(ok(r.u8(b8)));
  CHECK(ok(r.u16(b16)));
  CHECK(ok(r.u32(b32)));
  CHECK(ok(r.f64(d)));
  CHECK_EQ(b8, static_cast<std::uint8_t>(0xAB));
  CHECK_EQ(b16, static_cast<std::uint16_t>(0x1234));
  CHECK_EQ(b32, 0xDEADBEEFu);
  CHECK(d > 3.14 && d < 3.15);
}

TEST("byte reader: short read is reported, not fatal") {
  std::uint8_t buf[2] = {1, 2};
  ByteReader r(buf, 2);
  std::uint32_t v;
  CHECK(r.u32(v) == Status::short_read);
}
