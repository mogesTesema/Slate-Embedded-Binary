// slate/graph/depgraph.cc
#include "slate/graph/depgraph.hh"

#include <vector>

namespace slate {

Status DepGraph::build(const Workbook& wb, DiagnosticSink& diag) {
  const std::size_t n = wb.grid.size();
  precedents_.assign(n, {});
  dependents_.assign(n, {});

  for (const RawEdge& e : wb.edges) {
    // Endpoints are cell-record ordinals; the decoder already validated them
    // against the record count, so trust that bound here instead of re-walking
    // the grid on every edge.
    if (e.from >= wb.cell_record_count || e.to >= wb.cell_record_count) {
      diag.error(Status::bad_reference, "dependency edge cites unknown cell");
      return Status::bad_reference;
    }
    precedents_[e.from].push_back(e.to);
    dependents_[e.to].push_back(e.from);
  }
  return Status::ok;
}

void DepGraph::propagate_dirty(CellId seed,
                               std::vector<std::uint8_t>& out) const {
  // `seed` is a cell ordinal the decoder already range-checked against the
  // record count; mark it dirty and walk its dependents.
  std::vector<CellId> stack;
  stack.push_back(seed);
  out[seed] = 1;
  while (!stack.empty()) {
    CellId cur = stack.back();
    stack.pop_back();
    for (CellId dep : dependents_[cur]) {
      if (!out[dep]) {
        out[dep] = 1;
        stack.push_back(dep);
      }
    }
  }
}

}  // namespace slate
