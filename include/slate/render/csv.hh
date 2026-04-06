// slate/render/csv.hh
//
// Renders a sheet's current grid values to CSV text. This is a genuine consumer
// of the model: it walks the occupied extent of a sheet, formats each cell value
// (numbers via the number-format layer, text via the string directory, booleans
// and errors by their canonical spelling), and quotes fields that need it. It
// reflects whatever values are currently in the grid — static values after a
// plain load, or computed values after a recalculation.
#ifndef SLATE_RENDER_CSV_HH
#define SLATE_RENDER_CSV_HH

#include <string>

#include "slate/ids.hh"
#include "slate/model/workbook.hh"

namespace slate {

// Render one sheet to CSV. An out-of-range sheet yields an empty string.
std::string render_sheet_csv(const Workbook& wb, SheetId sheet);

}  // namespace slate

#endif  // SLATE_RENDER_CSV_HH
