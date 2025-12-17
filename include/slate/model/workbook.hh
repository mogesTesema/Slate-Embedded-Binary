// slate/model/workbook.hh
//
// The live workbook: the linked, evaluatable form of a decoded RawWorkbook. It
// owns the shared-string pages and directory, the sheet table, the materialized
// cell grid, and the still-raw formula/name/spill/edge/directive tables that the
// graph, compile, and eval layers consume.
//
// String identity: StrId in [0, string_count) names a static shared string,
// resolved through the directory into its owning SST page's bytes. The evaluator
// seeds its interner so those same ids are valid runtime handles, and allocates
// dynamic handles above string_count for computed text.
#ifndef SLATE_MODEL_WORKBOOK_HH
#define SLATE_MODEL_WORKBOOK_HH

#include <string_view>
#include <vector>

#include "slate/format/raw.hh"
#include "slate/model/grid.hh"
#include "slate/model/sheet.hh"
#include "slate/util/diagnostic.hh"
#include "slate/util/status.hh"

namespace slate {

class Workbook {
 public:
  SheetTable sheets;
  Grid grid;

  // Owned string storage, moved out of the decoder.
  std::vector<RawSstPage> sst_pages;
  std::vector<RawWorkbook::StrRef> string_dir;  // StrId -> {page, slot}

  // Still-raw tables consumed by later layers.
  std::vector<RawFormula> formulas;
  std::vector<RawName> names;
  std::vector<RawSpill> spills;
  std::vector<RawEdge> edges;
  std::vector<RawDirective> directives;
  std::vector<RawMerge> merges;

  // Number of cell records that were decoded from the cells section. Dependency
  // edges and dirty-directive targets are ordinals into that record list and the
  // decoder validated them against this count, so downstream passes can bound
  // against it without re-walking the grid.
  std::uint32_t cell_record_count = 0;

  std::uint32_t string_count() const {
    return static_cast<std::uint32_t>(string_dir.size());
  }

  // If `ref` lies inside a merged region but is not its anchor, return the
  // anchor (top-left) cell; otherwise return `ref` unchanged. Reads of a merged
  // region resolve through the anchor.
  CellRef resolve_merge(const CellRef& ref) const;

  // Resolve a static string to its bytes in the owning page. Returns an empty
  // view for an unknown id or a page that has been recycled.
  std::string_view resolve_string(StrId id) const;

  // Link a decoded workbook into evaluatable form. Consumes `raw`.
  static Status link(RawWorkbook&& raw, Workbook& out, DiagnosticSink& diag);

 private:
  void materialize_cells(const std::vector<RawCell>& cells);
};

}  // namespace slate

#endif  // SLATE_MODEL_WORKBOOK_HH
