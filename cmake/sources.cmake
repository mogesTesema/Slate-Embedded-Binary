# cmake/sources.cmake
#
# Explicit first-party source list for the slate_core library. Listed by hand
# (not globbed) so adding or moving a file is a deliberate, reviewable change and
# the build never silently picks up stray files.
set(SLATE_CORE_SOURCES
  src/util/status.cc
  src/util/crc32.cc
  src/util/arena.cc
  src/util/intern_table.cc

  src/format/decode.cc
  src/format/sst.cc
  src/format/decode_rle.cc
  src/format/sections.cc

  src/model/reference.cc
  src/model/sheet.cc
  src/model/grid.cc
  src/model/number_format.cc
  src/model/workbook.cc

  src/graph/depgraph.cc
  src/graph/topo.cc
  src/graph/defined_names.cc

  src/compile/verifier.cc
  src/compile/binder.cc
  src/compile/parser.cc

  src/eval/string_interner.cc
  src/eval/array_store.cc
  src/eval/value_ops.cc
  src/eval/operand.cc
  src/eval/functions.cc
  src/eval/functions_ext.cc
  src/eval/array_ops.cc
  src/eval/date_functions.cc
  src/eval/math_functions.cc
  src/eval/financial_functions.cc
  src/eval/stats_text_functions.cc
  src/eval/lookup_functions.cc
  src/eval/logic_functions.cc
  src/eval/evaluator.cc

  src/recalc/epoch.cc
  src/recalc/teardown.cc
  src/recalc/compaction.cc
  src/recalc/preview.cc
  src/recalc/driver.cc

  src/render/csv.cc

  src/api_load.cc
  src/api_recalc.cc
  src/api_formula.cc
)
