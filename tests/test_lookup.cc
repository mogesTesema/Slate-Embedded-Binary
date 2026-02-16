// tests/test_lookup.cc
#include "framework.hh"
#include "test_support.hh"

#include "slate/compile/parser.hh"

using namespace slate;
using namespace slate::test;

namespace {

Value ev(const std::string& formula,
         const std::function<void(tools::WorkbookBuilder&, SheetId)>& setup = nullptr) {
  tools::WorkbookBuilder b;
  SheetId s = b.add_sheet("S", 16, 8);
  if (setup) setup(b, s);
  ParseContext ctx;
  ctx.default_sheet = s;
  ctx.intern_string = [&](std::string_view sv) { return b.add_string(sv); };
  std::vector<std::uint8_t> code; std::string err;
  if (!ok(parse_formula(formula, ctx, code, err))) return Value::error(wire::ErrCode::name);
  FormulaId fid = b.add_formula(code);
  b.formula_cell(s, 14, 6, fid);
  return recalc_value(b.build(), s, 14, 6);
}

double n(const std::string& f, const std::function<void(tools::WorkbookBuilder&, SheetId)>& setup = nullptr) {
  Value v = ev(f, setup);
  return v.is_number() ? v.as_number() : -1e18;
}

void abc_col(tools::WorkbookBuilder& b, SheetId s) {
  b.number_cell(s, 0, 0, 1); b.number_cell(s, 1, 0, 2); b.number_cell(s, 2, 0, 3);
  b.number_cell(s, 0, 1, 10); b.number_cell(s, 1, 1, 20); b.number_cell(s, 2, 1, 30);
}

}  // namespace

TEST("lookup: ROW / COLUMN") {
  CHECK_EQ(n("=ROW(A5)"), 5.0);
  CHECK_EQ(n("=COLUMN(C1)"), 3.0);
}

TEST("lookup: ROWS / COLUMNS") {
  CHECK_EQ(n("=ROWS(A1:A4)"), 4.0);
  CHECK_EQ(n("=COLUMNS(A1:D1)"), 4.0);
}

TEST("lookup: ADDRESS builds an A1 string") {
  // "$A$3" has length 4
  CHECK_EQ(n("=LEN(ADDRESS(3,1))"), 4.0);
}

TEST("lookup: COUNTBLANK counts empty cells in a range") {
  CHECK_EQ(n("=COUNTBLANK(A1:A3)", [](tools::WorkbookBuilder& b, SheetId s) {
             b.number_cell(s, 0, 0, 1); b.number_cell(s, 2, 0, 3);  // A2 left blank
           }),
           1.0);
}

TEST("lookup: SUMIFS sums where the criterion holds") {
  CHECK_EQ(n("=SUMIFS(B1:B3, A1:A3, \">1\")", abc_col), 50.0);  // 20 + 30
}

TEST("lookup: COUNTIFS single and multiple criteria") {
  CHECK_EQ(n("=COUNTIFS(A1:A3, \">1\")", abc_col), 2.0);
  CHECK_EQ(n("=COUNTIFS(A1:A3, \">1\", B1:B3, \"<30\")", abc_col), 1.0);
}
