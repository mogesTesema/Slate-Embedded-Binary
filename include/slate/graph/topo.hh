// slate/graph/topo.hh
//
// Topological evaluation order over the dependency graph, with cycle detection.
// The order places precedents before dependents so a single forward pass can
// evaluate every formula with its inputs already resolved. A graph containing a
// cycle has no such order; compute() reports it and the affected cells stay in
// the residual set for the evaluator to flag as circular.
#ifndef SLATE_GRAPH_TOPO_HH
#define SLATE_GRAPH_TOPO_HH

#include <vector>

#include "slate/graph/depgraph.hh"
#include "slate/ids.hh"
#include "slate/util/diagnostic.hh"
#include "slate/util/status.hh"

namespace slate {

class TopoOrder {
 public:
  // Compute an evaluation order. Returns cyclic_dependency if the graph is not a
  // DAG; `order()` then holds only the acyclic prefix and `cyclic()` lists the
  // cells that could not be ordered.
  Status compute(const DepGraph& g, DiagnosticSink& diag);

  const std::vector<CellId>& order() const { return order_; }
  const std::vector<CellId>& cyclic() const { return cyclic_; }
  bool has_cycle() const { return !cyclic_.empty(); }

 private:
  std::vector<CellId> order_;
  std::vector<CellId> cyclic_;
};

}  // namespace slate

#endif  // SLATE_GRAPH_TOPO_HH
