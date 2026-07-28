// slate/format/wire.hh
//
// On-disk constants for the Slate workbook container (".slw"). The format is a
// fixed header, a section table, and then a set of length-delimited sections at
// their declared offsets. Everything is little-endian. These values are the
// contract between tools/mkworkbook (the writer) and src/format (the reader).
#ifndef SLATE_FORMAT_WIRE_HH
#define SLATE_FORMAT_WIRE_HH

#include <cstdint>

namespace slate::wire {

inline constexpr std::uint8_t  kMagic[4] = {'S', 'L', 'W', 'B'};
inline constexpr std::uint16_t kVersion  = 3;

// Header layout (little-endian):
//   u8[4] magic
//   u16   version
//   u16   flags
//   u16   section_count
//   u16   reserved (must be 0)
//   u32   header_crc  (crc32 over the 12 preceding bytes)
inline constexpr std::size_t kHeaderSize = 16;

// Section table entry (little-endian), one per section_count:
//   u16 kind
//   u16 flags
//   u32 offset  (from file start)
//   u32 length
//   u32 checksum (crc32 over the section body)
inline constexpr std::size_t kSectionEntrySize = 16;

enum class Section : std::uint16_t {
  sheets     = 1,
  sst        = 2,   // shared string pool (paged, RLE + prefix)
  cells      = 3,
  formulas   = 4,   // per-formula bytecode
  names      = 5,   // defined names
  spill      = 6,   // array-formula spill table
  xref       = 7,   // dependency edge index
  directives = 8,   // recalc directives (dirty seeds, sheet visibility, ...)
  merges     = 9,   // merged cell regions
};

// Cell value tags as they appear in the cell table.
enum class CellTag : std::uint8_t {
  empty   = 0,
  number  = 1,   // f64
  text    = 2,   // StrId
  boolean = 3,   // u8 (0/1)
  error   = 4,   // u8 error code
  formula = 5,   // FormulaId
  array   = 6,   // SpillId (anchor of an array result)
};

// Error codes carried by error cells / produced by evaluation.
enum class ErrCode : std::uint8_t {
  null    = 0,
  div0    = 1,
  value   = 2,
  ref     = 3,
  name    = 4,
  num     = 5,
  na      = 6,
  cycle   = 7,
};

// Formula bytecode opcodes. Stack machine, one byte each, followed by inline
// operands as noted.
enum class Op : std::uint8_t {
  end         = 0x00,  // -
  push_num    = 0x01,  // f64
  push_str    = 0x02,  // StrId (u32)
  push_bool   = 0x03,  // u8
  push_err    = 0x04,  // u8 error code
  push_ref    = 0x05,  // sheet u16, row u32, col u32
  push_range  = 0x06,  // sheet u16, r0 u32, c0 u32, r1 u32, c1 u32
  push_name   = 0x07,  // NameId (u32)
  load        = 0x10,  // pop ref -> push resolved value
  add         = 0x20,
  sub         = 0x21,
  mul         = 0x22,
  divide      = 0x23,
  neg         = 0x24,
  concat      = 0x25,  // text concatenation
  eq          = 0x30,
  lt          = 0x31,
  gt          = 0x32,
  le          = 0x33,
  ge          = 0x34,
  ne          = 0x35,
  call        = 0x40,  // func u16, argc u8
  make_array  = 0x50,  // rows u32, cols u32  (pops rows*cols values)
};

// Sheet flags.
inline constexpr std::uint16_t kSheetVisible = 0x0001;

// SST page flags.
inline constexpr std::uint16_t kPageRecyclable = 0x0001;

// SST encoding op bytes (within a page's encoded stream). The codec is a small
// LZ-style scheme: literals and runs for raw bytes, prefix/copy for back-
// references into already-produced output, and fill for short repeating
// patterns. Decoders must bound every op against the page's declared size.
enum class SstOp : std::uint8_t {
  literal = 0x00,  // u8 len, then len bytes
  run     = 0x01,  // u8 len, u8 byte  (RLE)
  prefix  = 0x02,  // u16 backref_off, u8 share_len, u8 suffix_len, suffix bytes
  copy    = 0x03,  // u16 distance, u8 len  (LZ copy from pos-distance, may overlap)
  fill    = 0x04,  // u8 len, u8 b0, u8 b1  (repeat the two-byte pattern)
};

// Defined-name kinds.
enum class NameKind : std::uint8_t {
  cell  = 0,   // sheet u16, row u32, col u32
  range = 1,   // sheet u16, r0 u32, c0 u32, r1 u32, c1 u32
};

// Directive ops.
enum class Directive : std::uint16_t {
  set_dirty     = 1,   // arg0 = cell ordinal
  sheet_visible = 2,   // arg0 = sheet, arg1 = 0/1
  recalc_pass   = 3,   // arg0 = pass count
};

// Function ids for the CALL opcode. The evaluator's function library maps these
// to implementations; kept stable so bytecode is portable. Ids are grouped by
// family and never reused, so old bytecode keeps meaning as the library grows.
enum class Func : std::uint16_t {
  // aggregates
  sum = 1, avg = 2, min = 3, max = 4, count = 5,
  // scalar math
  abs_ = 10, round_ = 11, mod_ = 12, power = 13, sqrt_ = 14,
  // logic
  if_ = 20, and_ = 21, or_ = 22, not_ = 23,
  // text
  len_ = 30, left_ = 31, right_ = 32, mid_ = 33, upper_ = 34, lower_ = 35,
  concat_fn = 36, text_fn = 46,
  // lookup
  vlookup = 40, index_ = 41, match_ = 42,

  // --- extended library (ids 60+) ---
  product = 60, sumif = 61, countif = 62, averageif = 63,
  int_ = 64, trunc_ = 65, sign_ = 66, ceiling_ = 67, floor_ = 68,
  gcd = 69, lcm = 70,
  median = 71, stdev = 72, var_ = 73, rank = 74,
  find_ = 75, substitute = 76, trim = 77, rept = 78, exact = 79, textjoin = 80,
  hlookup = 81, choose = 82,
  xor_ = 84, iferror = 85,
  isnumber = 87, istext = 88, iserror = 89, isblank = 90,

  // --- array functions (ids 100+): consume/produce array values ---
  sumproduct = 100, sumsq = 101, transpose = 102, mmult = 103,

  // --- date functions (ids 110+): serial-date arithmetic ---
  date = 110, year = 111, month = 112, day = 113, weekday = 114,
  edate = 115, eomonth = 116, days = 117,

  // --- math functions (ids 130+) ---
  exp_ = 130, ln_ = 131, log_ = 132, log10_ = 133, pi_ = 134,
  sin_ = 135, cos_ = 136, tan_ = 137, asin_ = 138, acos_ = 139,
  atan_ = 140, atan2_ = 141, degrees_ = 142, radians_ = 143,
  roundup = 144, rounddown = 145, mround = 146, fact = 147,
  combin = 148, permut = 149, quotient = 150,

  // --- financial functions (ids 160+) ---
  pmt = 160, fv = 161, pv = 162, nper = 163, npv = 164,

  // --- statistical / info / text (ids 170+) ---
  varp = 170, stdevp = 171, counta = 172, large = 173, small = 174,
  mode = 175, geomean = 176, iseven = 177, isodd = 178, n_fn = 179,
  proper = 180, char_ = 181, code_ = 182, value_fn = 183, search = 184,
  replace_fn = 185, clean = 186,

  // --- lookup / reference (ids 190+) ---
  row_fn = 190, column_fn = 191, rows_fn = 192, columns_fn = 193,
  address = 194, countblank = 195, sumifs = 196, countifs = 197,

  // --- logic / info (ids 200+) ---
  true_fn = 200, false_fn = 201, ifs = 202, switch_fn = 203, ifna = 204,
  islogical = 205, isnontext = 206,
};

}  // namespace slate::wire

#endif  // SLATE_FORMAT_WIRE_HH
