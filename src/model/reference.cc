// slate/model/reference.cc
#include "slate/model/reference.hh"

#include <cctype>

namespace slate {

std::string column_label(std::uint32_t col) {
  // Bijective base-26: 0->A, 25->Z, 26->AA. Build reversed then flip.
  std::string out;
  std::uint64_t n = static_cast<std::uint64_t>(col) + 1;
  while (n > 0) {
    std::uint64_t rem = (n - 1) % 26;
    out.push_back(static_cast<char>('A' + rem));
    n = (n - 1) / 26;
  }
  std::string flipped(out.rbegin(), out.rend());
  return flipped;
}

std::string a1(const CellRef& ref) {
  return column_label(ref.col) + std::to_string(ref.row + 1);
}

bool parse_a1(const std::string& text, std::uint32_t& row, std::uint32_t& col) {
  std::size_t i = 0;
  std::uint64_t c = 0;
  while (i < text.size() && std::isalpha(static_cast<unsigned char>(text[i]))) {
    char up = static_cast<char>(std::toupper(static_cast<unsigned char>(text[i])));
    c = c * 26 + static_cast<std::uint64_t>(up - 'A' + 1);
    ++i;
  }
  if (i == 0 || i >= text.size()) return false;

  std::uint64_t r = 0;
  bool any = false;
  while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) {
    r = r * 10 + static_cast<std::uint64_t>(text[i] - '0');
    ++i;
    any = true;
  }
  if (!any || i != text.size() || r == 0 || c == 0) return false;

  col = static_cast<std::uint32_t>(c - 1);
  row = static_cast<std::uint32_t>(r - 1);
  return true;
}

}  // namespace slate
