// tools/gen_seeds/main.cc
//
// Generates the fuzzing seed corpora. It authors a spread of valid, deep-
// reaching workbooks (covering every section and a wide slice of the function
// library) for the parse and recalc harnesses, and a set of standalone formula
// modules for the formula harness. Seeds that already parse/evaluate cleanly let
// the fuzzer mutate into deep state instead of being rejected at byte zero.
//
// Usage: gen_seeds <corpus_root>
//   writes  <root>/workbook_parse_fuzzer/*.slw
//           <root>/recalc_fuzzer/*.slw
//           <root>/formula_fuzzer/*.sfm
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "builder.hh"
#include "slate/compile/parser.hh"
#include "slate/format/wire.hh"

namespace {

using namespace slate;

void write_file(const std::string& path, const std::vector<std::uint8_t>& bytes) {
  std::FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) { std::fprintf(stderr, "cannot write %s\n", path.c_str()); return; }
  std::fwrite(bytes.data(), 1, bytes.size(), f);
  std::fclose(f);
}

// Fill a sheet with a small numeric table the seed formulas reference.
void seed_table(tools::WorkbookBuilder& b, SheetId s) {
  for (std::uint32_t r = 0; r < 6; ++r) {
    b.number_cell(s, r, 0, static_cast<double>(r + 1));       // A: 1..6
    b.number_cell(s, r, 1, static_cast<double>((r + 1) * 10)); // B: 10..60
  }
}

// One workbook per formula theme. Each returns fully-encoded bytes.
std::vector<std::vector<std::uint8_t>> workbook_seeds() {
  const char* themes[] = {
      "=SUM(A1:A6)+AVERAGE(B1:B6)",
      "=IF(A1>A2, MAX(B1:B6), MIN(B1:B6))",
      "=VLOOKUP(3, A1:B6, 2) + INDEX(B1:B6, MATCH(4, A1:A6, 0))",
      "=SUMIFS(B1:B6, A1:A6, \">2\") - COUNTIFS(A1:A6, \"<5\")",
      "=ROUND(POWER(A3, 2) + SQRT(B4), 3) * LOG(B6, 2)",
      "=DAY(EOMONTH(DATE(2020, A2, 15), A1)) + WEEKDAY(DATE(2021,1,1))",
      "=PMT(0.01, 12, B6) + NPV(0.1, A1, A2, A3)",
      "=LEN(PROPER(\"annual report\")) + FIND(\"e\", \"header\")",
      "=STDEV(A1:A6) + MEDIAN(B1:B6) - LARGE(A1:A6, 2)",
      "=SUMPRODUCT(A1:A6, B1:B6) + MMULT(A1:A2, A3:A4)",
  };

  std::vector<std::vector<std::uint8_t>> out;
  for (const char* theme : themes) {
    tools::WorkbookBuilder b;
    SheetId s = b.add_sheet("Data", 32, 8);
    seed_table(b, s);
    b.add_name_range("Inputs", s, 0, 0, 5, 0);
    b.add_merge(s, 10, 0, 11, 1);
    CellId f = b.formula_text_cell(s, 8, 0, theme);
    if (f == kNoCell) { std::fprintf(stderr, "seed parse failed: %s\n", theme); continue; }
    // A dependency edge + a dirty seed exercise the recalc directives.
    b.add_edge(f, /*to*/ 0);
    b.add_directive(wire::Directive::set_dirty, 0, 0);
    b.add_directive(wire::Directive::recalc_pass, 1, 0);
    out.push_back(b.build());
  }
  return out;
}

// Assemble a formula module: "SFM1" + inputs + parsed bytecode. The formula
// references cells A1..A{n} which the module exposes as its inputs.
std::vector<std::uint8_t> make_module(const std::vector<double>& inputs,
                                      const std::string& formula) {
  // Parse against a scratch sheet so A1.. resolve to (sheet 0, row, col 0).
  ParseContext ctx;
  ctx.default_sheet = 0;
  ctx.intern_string = [](std::string_view) -> StrId { return 0; };  // no literals
  std::vector<std::uint8_t> code;
  std::string err;
  if (!ok(parse_formula(formula, ctx, code, err))) {
    std::fprintf(stderr, "module parse failed: %s (%s)\n", formula.c_str(), err.c_str());
    return {};
  }
  std::vector<std::uint8_t> b;
  auto u16 = [&](std::uint16_t v) { b.push_back(v & 0xFF); b.push_back(v >> 8); };
  auto u32 = [&](std::uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back(v >> (8 * i)); };
  auto f64 = [&](double d) { std::uint64_t x; std::memcpy(&x, &d, 8); for (int i = 0; i < 8; ++i) b.push_back(x >> (8 * i)); };
  const char* m = "SFM1"; b.insert(b.end(), m, m + 4);
  u16(static_cast<std::uint16_t>(inputs.size()));
  for (double d : inputs) f64(d);
  u32(static_cast<std::uint32_t>(code.size()));
  b.insert(b.end(), code.begin(), code.end());
  return b;
}

std::vector<std::vector<std::uint8_t>> module_seeds() {
  struct M { std::vector<double> in; const char* f; };
  M specs[] = {
      {{3, 4}, "=A1*A2 + A1"},
      {{5, 2, 8}, "=MAX(A1, A2) - MIN(A2, A3)"},
      {{10, 3}, "=MOD(A1, A2) + QUOTIENT(A1, A2)"},
      {{2, 10}, "=POWER(A1, A2) + LOG(A2, A1)"},
      {{7}, "=FACT(A1) + SQRT(A1)"},
      {{1, 2, 3, 4}, "=SUM(A1, A2, A3, A4) * AVERAGE(A1, A4)"},
      {{45}, "=SIN(RADIANS(A1)) + COS(RADIANS(A1))"},
      {{100, 5}, "=ROUND(A1 / A2, 2) + INT(A1 / A2)"},
  };
  std::vector<std::vector<std::uint8_t>> out;
  for (const M& m : specs) {
    auto bytes = make_module(m.in, m.f);
    if (!bytes.empty()) out.push_back(std::move(bytes));
  }
  return out;
}

}  // namespace

int main(int argc, char** argv) {
  std::string root = argc > 1 ? argv[1] : "fuzz/corpus";

  auto wbs = workbook_seeds();
  int i = 0;
  for (const auto& w : wbs) {
    char name[64];
    std::snprintf(name, sizeof(name), "/seed_wb_%02d.slw", i++);
    write_file(root + "/workbook_parse_fuzzer" + name, w);
    write_file(root + "/recalc_fuzzer" + name, w);
  }

  auto mods = module_seeds();
  int j = 0;
  for (const auto& m : mods) {
    char name[64];
    std::snprintf(name, sizeof(name), "/seed_mod_%02d.sfm", j++);
    write_file(root + "/formula_fuzzer" + name, m);
  }

  std::fprintf(stderr, "gen_seeds: %zu workbook seeds, %zu module seeds\n",
               wbs.size(), mods.size());
  return 0;
}
