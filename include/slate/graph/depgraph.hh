// slate/graph/depgraph.hh
//
// The cell dependency graph. An edge {from -> to} means cell `from` reads cell
// `to`, so `to` must be evaluated first. The graph is built from the workbook's
// xref section (the authored dependency index) and validated against the live
// cell grid: every endpoint must name a real cell. Dependents/precedents are
// stored both ways so recalc can walk downstream (dirty propagation) and
// upstream (evaluation order).
#ifndef SLATE_GRAPH_DEPGRAPH_HH
#define SLATE_GRAPH_DEPGRAPH_HH

#include <cstdint>
#include <vector>

#include "slate/ids.hh"
#include "slate/model/workbook.hh"
#include "slate/util/diagnostic.hh"
#include "slate/util/small_vector.hh"
#include "slate/util/status.hh"

namespace slate {

class DepGraph {
 public:
  // Build from the workbook's edges. Returns bad_reference if an edge cites a
  // cell ordinal the grid does not contain.
  Status build(const Workbook& wb, DiagnosticSink& diag);

  std::size_t size() const { return precedents_.size(); }

  const SmallVector<CellId>& precedents(CellId id) const {
    return precedents_[id];
  }
  const SmallVector<CellId>& dependents(CellId id) const {
    return dependents_[id];
  }

  // Mark `seed` and everything transitively downstream of it dirty in `out`.
  // `out` must be sized to size(); entries are set to true, never cleared.
  void propagate_dirty(CellId seed, std::vector<std::uint8_t>& out) const;

 private:
  std::vector<SmallVector<CellId>> precedents_;
  std::vector<SmallVector<CellId>> dependents_;
};

}  // namespace slate

#endif  // SLATE_GRAPH_DEPGRAPH_HH
