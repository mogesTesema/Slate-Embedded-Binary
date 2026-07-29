// slate/recalc/driver.hh
//
// Drives a full recalculation: sets up the string/array stores, compiles and
// binds every formula, builds the dependency graph and evaluation order,
// evaluates in topological order (materializing array-formula spills), then
// applies the workbook's recalc directives (dirty seeds, sheet visibility
// changes, extra passes) before tearing everything down and releasing handles.
#ifndef SLATE_RECALC_DRIVER_HH
#define SLATE_RECALC_DRIVER_HH

#include <cstdint>
#include <deque>
#include <string_view>
#include <vector>

#include "slate/compile/program.hh"
#include "slate/eval/array_store.hh"
#include "slate/eval/evaluator.hh"
#include "slate/eval/string_interner.hh"
#include "slate/graph/defined_names.hh"
#include "slate/graph/depgraph.hh"
#include "slate/graph/topo.hh"
#include "slate/model/workbook.hh"
#include "slate/recalc/epoch.hh"
#include "slate/recalc/preview.hh"
#include "slate/recalc/summary.hh"
#include "slate/recalc/teardown.hh"
#include "slate/util/diagnostic.hh"
#include "slate/util/status.hh"

namespace slate {

struct RecalcStats {
  std::uint32_t evaluated = 0;
  std::uint32_t errors = 0;
  std::uint32_t spills = 0;
  std::uint32_t pages_recycled = 0;
  // First byte of the most recently computed text result, for a lightweight
  // "what changed" preview a caller can surface without resolving a handle.
  char last_text_preview = 0;
  bool has_text_preview = false;
  // First cell of the most recently spilled array, for the same kind of
  // lightweight preview on the array side.
  Value last_array_preview;
  bool has_array_preview = false;
};

class RecalcDriver {
 public:
  explicit RecalcDriver(Workbook& wb)
      : wb_(wb), strings_(wb), epoch_(wb, strings_), eval_(wb, strings_, arrays_),
        preview_(strings_, arrays_) {}

  Status run(DiagnosticSink& diag);
  const RecalcStats& stats() const { return stats_; }
  const RecalcSummary& summary() const { return summary_; }
  // Release the borrows this driver holds on the summary's previewed handles.
  // Call once the caller is done reading summary() — the summary's raw views
  // remain valid until this is called, even though run() has already returned.
  void release_summary();

 private:
  // SpillRecord and the ordered drain live in recalc/teardown.hh.
  void setup_borrows();
  Status compile_all(DiagnosticSink& diag);
  Status build_graph(DiagnosticSink& diag);
  void evaluate_all(DiagnosticSink& diag);
  void evaluate_formula(CellId id, DiagnosticSink& diag);
  void evaluate_anchor(CellId id, DiagnosticSink& diag);
  void resolve_pending_conflicts();
  void materialize_spill(CellId anchor, std::uint32_t array_handle,
                         const RawSpill& sp);
  void apply_directives(DiagnosticSink& diag);
  void recompute_dirty(const std::vector<std::uint8_t>& dirty,
                       DiagnosticSink& diag);
  void teardown();
  void note_summary_text(std::uint32_t handle);
  void note_summary_array(std::uint32_t handle);

  EvalContext ctx() { return eval_.context(); }

  Workbook& wb_;
  StringInterner strings_;
  ArrayStore arrays_;
  RecalcEpoch epoch_;
  Evaluator eval_;
  NameResolver names_;
  DepGraph graph_;
  TopoOrder topo_;

  std::vector<Program> programs_;         // indexed by FormulaId
  std::vector<SpillRecord> spill_records_; // one per active anchor
  std::vector<SpillRecord*> pending_conflicts_; // pointers needing deferred resolution
  std::vector<CellId> spill_cells_;        // materialized target cells
  RecalcStats stats_;
  RecalcSummary summary_;
  PreviewTracker preview_;
};

}  // namespace slate

#endif  // SLATE_RECALC_DRIVER_HH
