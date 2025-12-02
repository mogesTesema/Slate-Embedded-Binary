// slate/ids.hh
//
// Dense index types shared across layers. These are deliberately plain 32-bit
// aliases rather than strong wrappers: cell and formula indices participate in
// arithmetic (row-major addressing, operand offsets) throughout the engine, and
// wrapping every one in a struct fought the code more than it helped. Sentinels
// mark "absent".
#ifndef SLATE_IDS_HH
#define SLATE_IDS_HH

#include <cstdint>

namespace slate {

using StrId      = std::uint32_t;  // index into the string directory
using CellId     = std::uint32_t;  // dense ordinal of a materialized cell
using FormulaId  = std::uint32_t;  // index into the formula table
using SheetId    = std::uint16_t;  // index into the sheet table
using NameId     = std::uint32_t;  // index into the defined-name table
using SpillId    = std::uint32_t;  // index into the spill table

inline constexpr StrId     kNoStr     = 0xFFFFFFFFu;
inline constexpr CellId    kNoCell    = 0xFFFFFFFFu;
inline constexpr FormulaId kNoFormula = 0xFFFFFFFFu;
inline constexpr SheetId   kNoSheet   = 0xFFFFu;
inline constexpr NameId    kNoName    = 0xFFFFFFFFu;
inline constexpr SpillId   kNoSpill   = 0xFFFFFFFFu;

}  // namespace slate

#endif  // SLATE_IDS_HH
