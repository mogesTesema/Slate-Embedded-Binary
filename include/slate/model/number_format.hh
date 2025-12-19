// slate/model/number_format.hh
//
// A small number-formatting mini-language, a practical subset of the spreadsheet
// format-code syntax. A format code is parsed into: an optional literal prefix
// and suffix, a thousands-grouping flag, a fixed number of decimal places, and a
// percent flag. Examples: "0.00", "#,##0", "$#,##0.00", "0%". Unknown/empty
// codes fall back to a compact "general" rendering.
#ifndef SLATE_MODEL_NUMBER_FORMAT_HH
#define SLATE_MODEL_NUMBER_FORMAT_HH

#include <string>
#include <string_view>

namespace slate {

std::string format_number(double value, std::string_view fmt);

}  // namespace slate

#endif  // SLATE_MODEL_NUMBER_FORMAT_HH
