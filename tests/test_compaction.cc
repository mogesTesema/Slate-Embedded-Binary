// tests/test_compaction.cc
#include "framework.hh"

#include "slate/eval/array_store.hh"
#include "slate/eval/string_interner.hh"
#include "slate/model/workbook.hh"

using namespace slate;

TEST("compaction: interner reclaims trailing dead dynamic slots") {
  Workbook wb;  // no static strings
  StringInterner si(wb);
  std::uint32_t a = si.intern_dynamic("alpha");
  std::uint32_t b = si.intern_dynamic("beta");
  std::uint32_t c = si.intern_dynamic("gamma");
  (void)b;
  si.release(c);  // dead, trailing
  si.release(b);  // dead, trailing
  std::uint32_t reclaimed = si.compact();
  CHECK_EQ(reclaimed, 2u);
  // The surviving handle still resolves.
  CHECK(si.resolve(a) == "alpha");
}

TEST("compaction: interner keeps a live slot below a dead one") {
  Workbook wb;
  StringInterner si(wb);
  std::uint32_t a = si.intern_dynamic("keep");
  std::uint32_t b = si.intern_dynamic("drop");
  si.release(b);
  CHECK_EQ(si.compact(), 1u);
  CHECK(si.resolve(a) == "keep");
}

TEST("compaction: array store reclaims trailing dead slots") {
  ArrayStore store;
  std::uint32_t h0 = store.allocate(1, 1);
  std::uint32_t h1 = store.allocate(2, 2);
  std::uint32_t h2 = store.allocate(3, 3);
  (void)h1;
  store.release(h2);
  store.release(h1);
  CHECK_EQ(store.compact(), 2u);
  // The remaining handle is still live and addressable.
  CHECK(store.alive(h0));
  CHECK(store.at(h0, 0, 0) != nullptr);
}
