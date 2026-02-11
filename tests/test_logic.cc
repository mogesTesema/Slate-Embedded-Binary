// tests/test_logic.cc
#include "framework.hh"
#include "test_support.hh"

#include "slate/compile/parser.hh"

using namespace slate;
using namespace slate::test;

namespace {

double n(const std::string& formula,
         const std::function<void(tools::WorkbookBuilder&, SheetId)>& setup = nullptr) {
  tools::WorkbookBuilder b;
  SheetId s = b.add_sheet("S", 16, 8);
  if (setup) setup(b, s);
  ParseContext ctx;
  ctx.default_sheet = s;
  ctx.intern_string = [&](std::string_view sv) { return b.add_string(sv); };
  std::vector<std::uint8_t> code; std::string err;
  if (!ok(parse_formula(formula, ctx, code, err))) return -1e18;
  FormulaId fid = b.add_formula(code);
  b.formula_cell(s, 14, 6, fid);
  Value v = recalc_value(b.build(), s, 14, 6);
  return v.is_number() ? v.as_number() : (v.is_bool() ? (v.as_bool() ? 1 : 0) : -1e18);
}

}  // namespace

TEST("logic: TRUE/FALSE constants") {
  CHECK_EQ(n("=IF(TRUE(), 1, 2)"), 1.0);
  CHECK_EQ(n("=IF(FALSE(), 1, 2)"), 2.0);
}

TEST("logic: IFS returns the first matching branch") {
  CHECK_EQ(n("=IFS(1>2, 10, 3>2, 20)"), 20.0);
  CHECK(n("=IFS(1>2, 10, 2>3, 20)") < -1e17);  // no branch -> #N/A
}

TEST("logic: SWITCH matches or falls to default") {
  CHECK_EQ(n("=SWITCH(2, 1, 100, 2, 200, 999)"), 200.0);
  CHECK_EQ(n("=SWITCH(9, 1, 100, 2, 200, 999)"), 999.0);  // default
}

TEST("logic: IFNA substitutes on #N/A") {
  auto setup = [](tools::WorkbookBuilder& b, SheetId s) {
    b.number_cell(s, 0, 0, 1); b.number_cell(s, 1, 0, 2);
  };
  // MATCH(99,...) is #N/A -> IFNA yields the fallback.
  CHECK_EQ(n("=IFNA(MATCH(99, A1:A2, 0), -1)", setup), -1.0);
}

TEST("logic: ISLOGICAL / ISNONTEXT") {
  CHECK_EQ(n("=IF(ISLOGICAL(1<2), 1, 0)"), 1.0);
  CHECK_EQ(n("=IF(ISNONTEXT(5), 1, 0)"), 1.0);
}
