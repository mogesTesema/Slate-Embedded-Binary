// slate/compile/parser.cc
#include "slate/compile/parser.hh"

#include <cctype>
#include <cstring>
#include <unordered_map>

#include "slate/model/reference.hh"
#include "slate/util/byte_writer.hh"

namespace slate {
namespace {

// --- lexer ------------------------------------------------------------------

enum class Tok {
  end, num, str, ident,
  plus, minus, star, slash, caret, amp,
  eq, lt, gt, le, ge, ne,
  lparen, rparen, comma, colon, bang,
};

struct Token {
  Tok kind = Tok::end;
  double num = 0.0;
  std::string text;
};

struct Lexer {
  std::string_view s;
  std::size_t i = 0;
  std::string error;

  bool lex(std::vector<Token>& out) {
    while (i < s.size()) {
      char c = s[i];
      if (std::isspace(static_cast<unsigned char>(c))) { ++i; continue; }
      if (std::isdigit(static_cast<unsigned char>(c)) ||
          (c == '.' && i + 1 < s.size() && std::isdigit(static_cast<unsigned char>(s[i + 1])))) {
        std::size_t j = i;
        while (j < s.size() && (std::isdigit(static_cast<unsigned char>(s[j])) || s[j] == '.')) ++j;
        Token t; t.kind = Tok::num;
        t.num = std::strtod(std::string(s.substr(i, j - i)).c_str(), nullptr);
        out.push_back(std::move(t));
        i = j;
        continue;
      }
      if (c == '"') {
        std::size_t j = i + 1;
        std::string val;
        while (j < s.size() && s[j] != '"') val.push_back(s[j++]);
        if (j >= s.size()) { error = "unterminated string"; return false; }
        Token t; t.kind = Tok::str; t.text = std::move(val);
        out.push_back(std::move(t));
        i = j + 1;
        continue;
      }
      if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
        std::size_t j = i;
        while (j < s.size() && (std::isalnum(static_cast<unsigned char>(s[j])) || s[j] == '_')) ++j;
        Token t; t.kind = Tok::ident; t.text = std::string(s.substr(i, j - i));
        out.push_back(std::move(t));
        i = j;
        continue;
      }
      Token t;
      switch (c) {
        case '+': t.kind = Tok::plus; break;
        case '-': t.kind = Tok::minus; break;
        case '*': t.kind = Tok::star; break;
        case '/': t.kind = Tok::slash; break;
        case '^': t.kind = Tok::caret; break;
        case '&': t.kind = Tok::amp; break;
        case '(': t.kind = Tok::lparen; break;
        case ')': t.kind = Tok::rparen; break;
        case ',': t.kind = Tok::comma; break;
        case ':': t.kind = Tok::colon; break;
        case '!': t.kind = Tok::bang; break;
        case '=': t.kind = Tok::eq; break;
        case '<':
          if (i + 1 < s.size() && s[i + 1] == '=') { t.kind = Tok::le; ++i; }
          else if (i + 1 < s.size() && s[i + 1] == '>') { t.kind = Tok::ne; ++i; }
          else t.kind = Tok::lt;
          break;
        case '>':
          if (i + 1 < s.size() && s[i + 1] == '=') { t.kind = Tok::ge; ++i; }
          else t.kind = Tok::gt;
          break;
        default:
          error = std::string("unexpected character '") + c + "'";
          return false;
      }
      out.push_back(std::move(t));
      ++i;
    }
    out.push_back(Token{Tok::end, 0, ""});
    return true;
  }
};

bool looks_like_cellref(const std::string& t) {
  std::size_t k = 0;
  while (k < t.size() && std::isalpha(static_cast<unsigned char>(t[k]))) ++k;
  if (k == 0 || k == t.size()) return false;
  for (std::size_t m = k; m < t.size(); ++m)
    if (!std::isdigit(static_cast<unsigned char>(t[m]))) return false;
  return true;
}

// --- parser -----------------------------------------------------------------

enum class Kind { value, cell, range, name };

struct Parser {
  std::vector<Token> toks;
  std::size_t p = 0;
  const ParseContext& ctx;
  ByteWriter& out;
  std::string error;
  bool failed = false;

  Parser(const ParseContext& c, ByteWriter& o) : ctx(c), out(o) {}

  const Token& cur() const { return toks[p]; }
  const Token& peek(std::size_t n = 1) const {
    return toks[p + n < toks.size() ? p + n : toks.size() - 1];
  }
  void advance() { if (p + 1 < toks.size()) ++p; }
  void fail(const std::string& m) { if (!failed) { error = m; failed = true; } }

  void emit(wire::Op op) { out.u8(static_cast<std::uint8_t>(op)); }

  // Ensure the top operand is a scalar value where an operator needs one.
  void coerce(Kind k) {
    if (failed) return;
    if (k == Kind::cell || k == Kind::range) emit(wire::Op::load);
    // value and name are already usable directly.
  }

  int lbp(Tok t) const {
    switch (t) {
      case Tok::eq: case Tok::lt: case Tok::gt:
      case Tok::le: case Tok::ge: case Tok::ne: return 1;
      case Tok::amp: return 2;
      case Tok::plus: case Tok::minus: return 3;
      case Tok::star: case Tok::slash: return 4;
      case Tok::caret: return 5;
      default: return 0;
    }
  }

  void emit_binop(Tok t) {
    switch (t) {
      case Tok::plus: emit(wire::Op::add); break;
      case Tok::minus: emit(wire::Op::sub); break;
      case Tok::star: emit(wire::Op::mul); break;
      case Tok::slash: emit(wire::Op::divide); break;
      case Tok::caret: fail("^ is not supported"); break;  // reserved
      case Tok::amp: emit(wire::Op::concat); break;
      case Tok::eq: emit(wire::Op::eq); break;
      case Tok::lt: emit(wire::Op::lt); break;
      case Tok::gt: emit(wire::Op::gt); break;
      case Tok::le: emit(wire::Op::le); break;
      case Tok::ge: emit(wire::Op::ge); break;
      case Tok::ne: emit(wire::Op::ne); break;
      default: fail("bad operator"); break;
    }
  }

  // Parse a cell reference identifier into (row, col). Returns false on failure.
  bool ref_coords(const std::string& t, std::uint32_t& row, std::uint32_t& col) {
    return parse_a1(t, row, col);
  }

  Kind parse_reference(SheetId sheet) {
    // cur() is an ident that is a cell ref. Optionally followed by ':' + ref.
    std::uint32_t r0, c0;
    if (!ref_coords(cur().text, r0, c0)) { fail("bad cell reference '" + cur().text + "'"); return Kind::value; }
    advance();
    if (cur().kind == Tok::colon) {
      advance();
      if (cur().kind != Tok::ident || !looks_like_cellref(cur().text)) { fail("expected cell after ':'"); return Kind::value; }
      std::uint32_t r1, c1;
      if (!ref_coords(cur().text, r1, c1)) { fail("bad cell reference"); return Kind::value; }
      advance();
      emit(wire::Op::push_range);
      out.u16(sheet); out.u32(r0); out.u32(c0); out.u32(r1); out.u32(c1);
      return Kind::range;
    }
    emit(wire::Op::push_ref);
    out.u16(sheet); out.u32(r0); out.u32(c0);
    return Kind::cell;
  }

  Kind parse_primary() {
    Token t = cur();
    switch (t.kind) {
      case Tok::num:
        advance();
        emit(wire::Op::push_num); out.f64(t.num);
        return Kind::value;
      case Tok::str: {
        advance();
        StrId id = ctx.intern_string ? ctx.intern_string(t.text) : kNoStr;
        emit(wire::Op::push_str); out.u32(id);
        return Kind::value;
      }
      case Tok::minus: {
        advance();
        Kind k = parse_expr(6);  // unary binds tighter than binary ops
        coerce(k);
        emit(wire::Op::neg);
        return Kind::value;
      }
      case Tok::lparen: {
        advance();
        Kind k = parse_expr(0);
        if (cur().kind != Tok::rparen) { fail("expected ')'"); return Kind::value; }
        advance();
        return k;
      }
      case Tok::ident: {
        // Function call?
        if (peek().kind == Tok::lparen) {
          std::string fname = t.text;
          wire::Func fn;
          if (!function_by_name(fname, fn)) { fail("unknown function '" + fname + "'"); return Kind::value; }
          advance();  // ident
          advance();  // '('
          std::uint8_t argc = 0;
          if (cur().kind != Tok::rparen) {
            for (;;) {
              parse_expr(0);       // argument; leaves its operand (ref/range stay)
              ++argc;
              if (cur().kind == Tok::comma) { advance(); continue; }
              break;
            }
          }
          if (cur().kind != Tok::rparen) { fail("expected ')' in call"); return Kind::value; }
          advance();
          emit(wire::Op::call); out.u16(static_cast<std::uint16_t>(fn)); out.u8(argc);
          return Kind::value;
        }
        // Sheet-qualified reference:  Sheet!A1
        if (peek().kind == Tok::bang) {
          SheetId sheet = ctx.resolve_sheet ? ctx.resolve_sheet(t.text) : kNoSheet;
          if (sheet == kNoSheet) { fail("unknown sheet '" + t.text + "'"); return Kind::value; }
          advance();  // ident
          advance();  // '!'
          if (cur().kind != Tok::ident || !looks_like_cellref(cur().text)) { fail("expected reference after '!'"); return Kind::value; }
          return parse_reference(sheet);
        }
        // Bare cell reference on the default sheet.
        if (looks_like_cellref(t.text)) return parse_reference(ctx.default_sheet);
        // Otherwise a defined name.
        NameId id = ctx.resolve_name ? ctx.resolve_name(t.text) : kNoName;
        if (id == kNoName) { fail("unknown name '" + t.text + "'"); return Kind::value; }
        advance();
        emit(wire::Op::push_name); out.u32(id);
        return Kind::name;
      }
      default:
        fail("unexpected token");
        return Kind::value;
    }
  }

  // Precedence-climbing expression. Returns the kind of the top result WITHOUT
  // coercing it, so a lone reference passed as a function argument stays a
  // reference operand.
  Kind parse_expr(int min_bp) {
    Kind left = parse_primary();
    while (!failed) {
      Tok op = cur().kind;
      int bp = lbp(op);
      if (bp == 0 || bp < min_bp) break;
      advance();
      coerce(left);
      Kind right = parse_expr(bp + 1);
      coerce(right);
      emit_binop(op);
      left = Kind::value;
    }
    return left;
  }
};

}  // namespace

bool function_by_name(std::string_view name, wire::Func& out) {
  static const std::unordered_map<std::string, wire::Func> table = {
      {"SUM", wire::Func::sum}, {"AVERAGE", wire::Func::avg}, {"AVG", wire::Func::avg},
      {"MIN", wire::Func::min}, {"MAX", wire::Func::max}, {"COUNT", wire::Func::count},
      {"ABS", wire::Func::abs_}, {"ROUND", wire::Func::round_}, {"MOD", wire::Func::mod_},
      {"POWER", wire::Func::power}, {"SQRT", wire::Func::sqrt_},
      {"IF", wire::Func::if_}, {"AND", wire::Func::and_}, {"OR", wire::Func::or_},
      {"NOT", wire::Func::not_}, {"LEN", wire::Func::len_}, {"LEFT", wire::Func::left_},
      {"RIGHT", wire::Func::right_}, {"MID", wire::Func::mid_}, {"UPPER", wire::Func::upper_},
      {"LOWER", wire::Func::lower_}, {"CONCAT", wire::Func::concat_fn},
      {"CONCATENATE", wire::Func::concat_fn}, {"TEXT", wire::Func::text_fn},
      {"VLOOKUP", wire::Func::vlookup}, {"INDEX", wire::Func::index_},
      {"MATCH", wire::Func::match_}, {"HLOOKUP", wire::Func::hlookup},
      {"CHOOSE", wire::Func::choose}, {"PRODUCT", wire::Func::product},
      {"SUMIF", wire::Func::sumif}, {"COUNTIF", wire::Func::countif},
      {"AVERAGEIF", wire::Func::averageif}, {"INT", wire::Func::int_},
      {"TRUNC", wire::Func::trunc_}, {"SIGN", wire::Func::sign_},
      {"CEILING", wire::Func::ceiling_}, {"FLOOR", wire::Func::floor_},
      {"GCD", wire::Func::gcd}, {"LCM", wire::Func::lcm}, {"MEDIAN", wire::Func::median},
      {"STDEV", wire::Func::stdev}, {"VAR", wire::Func::var_}, {"RANK", wire::Func::rank},
      {"FIND", wire::Func::find_}, {"SUBSTITUTE", wire::Func::substitute},
      {"TRIM", wire::Func::trim}, {"REPT", wire::Func::rept}, {"EXACT", wire::Func::exact},
      {"TEXTJOIN", wire::Func::textjoin}, {"XOR", wire::Func::xor_},
      {"IFERROR", wire::Func::iferror}, {"ISNUMBER", wire::Func::isnumber},
      {"ISTEXT", wire::Func::istext}, {"ISERROR", wire::Func::iserror},
      {"ISBLANK", wire::Func::isblank}, {"SUMPRODUCT", wire::Func::sumproduct},
      {"SUMSQ", wire::Func::sumsq}, {"TRANSPOSE", wire::Func::transpose},
      {"MMULT", wire::Func::mmult},
      {"DATE", wire::Func::date}, {"YEAR", wire::Func::year},
      {"MONTH", wire::Func::month}, {"DAY", wire::Func::day},
      {"WEEKDAY", wire::Func::weekday}, {"EDATE", wire::Func::edate},
      {"EOMONTH", wire::Func::eomonth}, {"DAYS", wire::Func::days},
      {"EXP", wire::Func::exp_}, {"LN", wire::Func::ln_}, {"LOG", wire::Func::log_},
      {"LOG10", wire::Func::log10_}, {"PI", wire::Func::pi_},
      {"SIN", wire::Func::sin_}, {"COS", wire::Func::cos_}, {"TAN", wire::Func::tan_},
      {"ASIN", wire::Func::asin_}, {"ACOS", wire::Func::acos_}, {"ATAN", wire::Func::atan_},
      {"ATAN2", wire::Func::atan2_}, {"DEGREES", wire::Func::degrees_},
      {"RADIANS", wire::Func::radians_}, {"ROUNDUP", wire::Func::roundup},
      {"ROUNDDOWN", wire::Func::rounddown}, {"MROUND", wire::Func::mround},
      {"FACT", wire::Func::fact}, {"COMBIN", wire::Func::combin},
      {"PERMUT", wire::Func::permut}, {"QUOTIENT", wire::Func::quotient},
      {"PMT", wire::Func::pmt}, {"FV", wire::Func::fv}, {"PV", wire::Func::pv},
      {"NPER", wire::Func::nper}, {"NPV", wire::Func::npv},
      {"VARP", wire::Func::varp}, {"STDEVP", wire::Func::stdevp},
      {"COUNTA", wire::Func::counta}, {"LARGE", wire::Func::large},
      {"SMALL", wire::Func::small}, {"MODE", wire::Func::mode},
      {"GEOMEAN", wire::Func::geomean}, {"ISEVEN", wire::Func::iseven},
      {"ISODD", wire::Func::isodd}, {"N", wire::Func::n_fn},
      {"PROPER", wire::Func::proper}, {"CHAR", wire::Func::char_},
      {"CODE", wire::Func::code_}, {"VALUE", wire::Func::value_fn},
      {"SEARCH", wire::Func::search}, {"REPLACE", wire::Func::replace_fn},
      {"CLEAN", wire::Func::clean},
      {"ROW", wire::Func::row_fn}, {"COLUMN", wire::Func::column_fn},
      {"ROWS", wire::Func::rows_fn}, {"COLUMNS", wire::Func::columns_fn},
      {"ADDRESS", wire::Func::address}, {"COUNTBLANK", wire::Func::countblank},
      {"SUMIFS", wire::Func::sumifs}, {"COUNTIFS", wire::Func::countifs},
      {"TRUE", wire::Func::true_fn}, {"FALSE", wire::Func::false_fn},
      {"IFS", wire::Func::ifs}, {"SWITCH", wire::Func::switch_fn},
      {"IFNA", wire::Func::ifna}, {"ISLOGICAL", wire::Func::islogical},
      {"ISNONTEXT", wire::Func::isnontext},
  };
  std::string upper;
  upper.reserve(name.size());
  for (char c : name) upper.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
  auto it = table.find(upper);
  if (it == table.end()) return false;
  out = it->second;
  return true;
}

Status parse_formula(std::string_view src, const ParseContext& ctx,
                     std::vector<std::uint8_t>& out, std::string& error) {
  if (!src.empty() && src.front() == '=') src.remove_prefix(1);

  Lexer lex{src};
  std::vector<Token> toks;
  if (!lex.lex(toks)) { error = lex.error; return Status::bad_opcode; }

  ByteWriter w;
  Parser parser(ctx, w);
  parser.toks = std::move(toks);

  Kind top = parser.parse_expr(0);
  if (!parser.failed) parser.coerce(top);  // top-level cell/range -> value/array
  if (!parser.failed && parser.cur().kind != Tok::end) parser.fail("trailing tokens");
  if (parser.failed) { error = parser.error; return Status::bad_opcode; }

  w.u8(static_cast<std::uint8_t>(wire::Op::end));
  out = w.data();
  return Status::ok;
}

}  // namespace slate
