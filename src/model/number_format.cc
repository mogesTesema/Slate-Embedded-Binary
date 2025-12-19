// slate/model/number_format.cc
#include "slate/model/number_format.hh"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace slate {
namespace {

bool is_format_char(char c) {
  return c == '0' || c == '#' || c == ',' || c == '.' || c == '%';
}

// Parsed shape of a format code.
struct FormatSpec {
  std::string prefix;
  std::string suffix;
  int decimals = 0;
  bool grouping = false;
  bool percent = false;
};

FormatSpec parse_format(std::string_view fmt) {
  FormatSpec spec;
  std::size_t i = 0;
  while (i < fmt.size() && !is_format_char(fmt[i])) spec.prefix += fmt[i++];
  std::size_t j = fmt.size();
  while (j > i && !is_format_char(fmt[j - 1])) spec.suffix.insert(spec.suffix.begin(), fmt[j - 1]), --j;

  std::string_view core = fmt.substr(i, j - i);
  spec.percent = core.find('%') != std::string_view::npos;

  std::size_t dot = core.find('.');
  if (dot != std::string_view::npos) {
    // Integer part is core[0..dot); grouping is a comma there.
    spec.grouping = core.substr(0, dot).find(',') != std::string_view::npos;
    for (std::size_t k = dot + 1; k < core.size(); ++k)
      if (core[k] == '0' || core[k] == '#') ++spec.decimals;
  } else {
    spec.grouping = core.find(',') != std::string_view::npos;
  }
  return spec;
}

// Insert thousands separators into the integer-part digit string.
std::string group_thousands(const std::string& digits) {
  std::string out;
  int count = 0;
  for (std::size_t k = digits.size(); k > 0; --k) {
    out.push_back(digits[k - 1]);
    if (++count % 3 == 0 && k > 1) out.push_back(',');
  }
  return std::string(out.rbegin(), out.rend());
}

std::string general(double value) {
  if (value == std::floor(value) && std::fabs(value) < 1e15) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(value));
    return buf;
  }
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.10g", value);
  return buf;
}

}  // namespace

std::string format_number(double value, std::string_view fmt) {
  if (fmt.empty()) return general(value);

  FormatSpec spec = parse_format(fmt);
  double v = spec.percent ? value * 100.0 : value;

  bool negative = std::signbit(v) && v != 0.0;
  double mag = std::fabs(v);

  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.*f", spec.decimals, mag);
  std::string num = buf;

  // Split off the fractional part so grouping applies only to the integer part.
  std::string int_part = num, frac_part;
  if (std::size_t dot = num.find('.'); dot != std::string::npos) {
    int_part = num.substr(0, dot);
    frac_part = num.substr(dot);  // includes the '.'
  }
  if (spec.grouping) int_part = group_thousands(int_part);

  std::string out = spec.prefix;
  if (negative) out += '-';
  out += int_part;
  out += frac_part;
  if (spec.percent) out += '%';
  out += spec.suffix;
  return out;
}

}  // namespace slate
