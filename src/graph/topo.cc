// slate/graph/topo.cc
#include "slate/graph/topo.hh"

#include <cstdint>

namespace slate {

Status TopoOrder::compute(const DepGraph& g, DiagnosticSink& diag) {
  const std::size_t n = g.size();
  order_.clear();
  cyclic_.clear();
  order_.reserve(n);

  // Kahn's algorithm: a cell is ready once all its precedents are emitted.
  std::vector<std::uint32_t> indegree(n, 0);
  for (CellId i = 0; i < n; ++i)
    indegree[i] = static_cast<std::uint32_t>(g.precedents(i).size());

  std::vector<CellId> ready;
  for (CellId i = 0; i < n; ++i)
    if (indegree[i] == 0) ready.push_back(i);

  while (!ready.empty()) {
    CellId u = ready.back();
    ready.pop_back();
    order_.push_back(u);
    for (CellId d : g.dependents(u)) {
      if (--indegree[d] == 0) ready.push_back(d);
    }
  }

  if (order_.size() != n) {
    for (CellId i = 0; i < n; ++i)
      if (indegree[i] != 0) cyclic_.push_back(i);
    diag.error(Status::cyclic_dependency, "dependency cycle detected");
    return Status::cyclic_dependency;
  }
  return Status::ok;
}

}  // namespace slate
