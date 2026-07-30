// slate/recalc/driver.cc
#include "slate/recalc/driver.hh"

#include <algorithm>

#include "slate/compile/binder.hh"
#include "slate/compile/verifier.hh"
#include "slate/recalc/compaction.hh"

namespace slate {

void RecalcDriver::setup_borrows() {
  // Each static text cell and each defined-name label borrows its shared string
  // so page recycling can tell whether a page is still in use.
  for (CellId id = 0; id < wb_.grid.size(); ++id) {
    const Value& v = wb_.grid.at(id).value;
    if (v.is_text()) strings_.retain(v.as_text());
  }
  for (const RawName& n : wb_.names)
    if (n.name != kNoStr) strings_.retain(n.name);

  names_.build(wb_);
  eval_.bind_names(&names_);
}

Status RecalcDriver::compile_all(DiagnosticSink& diag) {
  programs_.assign(wb_.formulas.size(), Program{});
  for (std::size_t i = 0; i < wb_.formulas.size(); ++i) {
    const RawFormula& f = wb_.formulas[i];
    Program prog;
    DiagnosticSink local;
    if (!ok(verify_formula(f.code.data(), f.code.size(), prog, local)))
      continue;  // leaves programs_[i] empty -> cell resolves to an error
    if (!ok(bind_program(prog, wb_, local))) continue;
    programs_[i] = std::move(prog);
  }
  (void)diag;
  return Status::ok;
}

Status RecalcDriver::build_graph(DiagnosticSink& diag) {
  if (Status s = graph_.build(wb_, diag); !ok(s)) return s;
  DiagnosticSink local;
  topo_.compute(graph_, local);  // a cycle is not fatal; cyclic cells error out
  return Status::ok;
}

void RecalcDriver::evaluate_formula(CellId id, DiagnosticSink& diag) {
  FormulaId fid = wb_.grid.at(id).formula;
  Value result;
  if (fid >= programs_.size() || programs_[fid].empty() ||
      !programs_[fid].bound) {
    result = Value::error(wire::ErrCode::value);
  } else {
    result = eval_.run(programs_[fid], diag);
  }
  EvalContext c = ctx();
  CellSlot& slot = wb_.grid.at(id);
  assign_value(c, slot.value, result);
  release_handles(c, result);
  preview_.note_text(slot.value);
  if (slot.value.is_text()) {
    note_summary_text(slot.value.as_text());
    summary_.note_text(strings_.resolve(slot.value.as_text()));
  }
  ++stats_.evaluated;
  if (slot.value.is_error()) ++stats_.errors;
}

void RecalcDriver::materialize_spill(CellId anchor, std::uint32_t array_handle,
                                     const RawSpill& sp) {
  EvalContext c = ctx();
  const ArrayValue* a = arrays_.get(array_handle);
  if (!a) return;
  if (!a->cells.empty()) {
    note_summary_array(array_handle);
    summary_.note_array(&a->cells[0]);
  }
  for (std::uint32_t rr = 0; rr < sp.rows; ++rr) {
    for (std::uint32_t cc = 0; cc < sp.cols; ++cc) {
      if (rr == 0 && cc == 0) continue;  // anchor already set
      CellRef ref{sp.sheet, sp.row + rr, sp.col + cc};
      CellId tid = wb_.grid.find(ref);
      if (tid == kNoCell) tid = wb_.grid.add(ref, wire::CellTag::empty);

      CellSlot& t = wb_.grid.at(tid);
      Value elem;
      std::size_t idx = static_cast<std::size_t>(rr) * a->cols + cc;
      if (idx < a->cells.size()) elem = a->cells[idx];
      assign_value(c, t.value, elem);
      t.is_spill_target = true;
      t.spill_anchor = anchor;
      spill_cells_.push_back(tid);
    }
  }
}

void RecalcDriver::evaluate_anchor(CellId id, DiagnosticSink& diag) {
  SpillId sid = wb_.grid.at(id).spill;
  if (sid >= wb_.spills.size()) return;
  const RawSpill sp = wb_.spills[sid];
  Value result;
  if (sp.source >= programs_.size() || programs_[sp.source].empty()) {
    result = Value::error(wire::ErrCode::value);
  } else {
    result = eval_.run(programs_[sp.source], diag);
  }

  EvalContext c = ctx();
  if (!result.is_array()) {
    CellSlot& slot = wb_.grid.at(id);
    assign_value(c, slot.value, result);
    release_handles(c, result);
    preview_.note_text(slot.value);
    ++stats_.evaluated;
    return;
  }

  // The array result carries one owned reference; transfer it to a spill record
  // (the sole owner) and expose a non-owning view in the anchor cell.
  std::uint32_t handle = result.as_array();
  spill_records_.push_back(SpillRecord{id, handle, false});
  SpillRecord* rec_ptr = &spill_records_.back();

  // First-come-wins: if any target is already occupied by another spill, this
  // anchor cannot spill — demote it and give the block back.
  bool conflict = false;
  for (std::uint32_t rr = 0; rr < sp.rows && !conflict; ++rr)
    for (std::uint32_t cc = 0; cc < sp.cols && !conflict; ++cc) {
      if (rr == 0 && cc == 0) continue;
      CellId tid = wb_.grid.find(CellRef{sp.sheet, sp.row + rr, sp.col + cc});
      if (tid != kNoCell && wb_.grid.at(tid).is_spill_target &&
          wb_.grid.at(tid).spill_anchor != id)
        conflict = true;
    }

  CellSlot& slot = wb_.grid.at(id);
  slot.value = Value::array(handle);  // non-owning view; record owns the ref
  preview_.note_array(handle);
  preview_.note_format_table(handle);
  ++stats_.evaluated;

  if (conflict) {
    pending_conflicts_.push_back(rec_ptr);
    preview_.note_spill_record(rec_ptr);
  } else {
    ++stats_.spills;
    materialize_spill(id, handle, sp);
  }
}

void RecalcDriver::resolve_pending_conflicts() {
  for (SpillRecord* rec_ptr : pending_conflicts_) {
    SpillRecord& rec = *rec_ptr;
    if (!rec.released) {
      arrays_.release(rec.array);
      rec.released = true;
    }
    // The anchor's slot never owned this array (the SpillRecord did, and its
    // ref was just released above), so overwrite it directly rather than
    // through assign_value, which would release the same handle a second time.
    CellSlot& slot = wb_.grid.at(rec.anchor);
    slot.value = Value::error(wire::ErrCode::ref);
  }
  pending_conflicts_.clear();
}

void RecalcDriver::evaluate_all(DiagnosticSink& diag) {
  const std::vector<CellId>& order = topo_.order();
  for (CellId id : order) {
    const CellSlot& slot = wb_.grid.at(id);
    if (slot.formula != kNoFormula) evaluate_formula(id, diag);
    else if (slot.spill != kNoSpill) evaluate_anchor(id, diag);
  }
  resolve_pending_conflicts();
  EvalContext c = ctx();
  for (CellId id : topo_.cyclic()) {
    CellSlot& slot = wb_.grid.at(id);
    if (slot.formula != kNoFormula)
      assign_value(c, slot.value, Value::error(wire::ErrCode::cycle));
  }
}

void RecalcDriver::recompute_dirty(const std::vector<std::uint8_t>& dirty,
                                   DiagnosticSink& diag) {
  for (CellId id : topo_.order()) {
    if (id >= dirty.size() || !dirty[id]) continue;
    const CellSlot& slot = wb_.grid.at(id);
    if (slot.formula != kNoFormula) evaluate_formula(id, diag);
    else if (slot.spill != kNoSpill) evaluate_anchor(id, diag);
  }
}

void RecalcDriver::apply_directives(DiagnosticSink& diag) {
  for (const RawDirective& d : wb_.directives) {
    switch (d.op) {
      case wire::Directive::set_dirty: {
        std::vector<std::uint8_t> dirty(wb_.grid.size(), 0);
        graph_.propagate_dirty(d.arg0, dirty);
        recompute_dirty(dirty, diag);
        break;
      }
      case wire::Directive::sheet_visible: {
        bool vis = d.arg1 != 0;
        wb_.sheets.set_visible(static_cast<SheetId>(d.arg0), vis);
        if (!vis) wb_.sheets.recycle(static_cast<SheetId>(d.arg0));
        break;
      }
      case wire::Directive::recalc_pass: {
        std::uint32_t passes = std::min<std::uint32_t>(d.arg0, 8);
        for (std::uint32_t i = 0; i < passes; ++i) {
          evaluate_all(diag);
          // Reclaim slots that this pass's overwrites left dead before the next.
          compact_stores(strings_, arrays_);
        }
        break;
      }
    }
  }
}

void RecalcDriver::teardown() {
  Teardown drain(wb_, strings_, arrays_, epoch_);
  stats_.pages_recycled = drain.run(spill_records_);
}

void RecalcDriver::note_summary_text(std::uint32_t handle) {
  summary_.note_text(strings_.resolve(handle));
}

void RecalcDriver::note_summary_array(std::uint32_t handle) {
  const ArrayValue* a = arrays_.get(handle);
  if (a && !a->cells.empty()) {
    summary_.note_array(&a->cells[0]);
  }
}

void RecalcDriver::release_summary() {
}

Status RecalcDriver::run(DiagnosticSink& diag) {
  setup_borrows();
  if (Status s = compile_all(diag); !ok(s)) return s;
  if (Status s = build_graph(diag); !ok(s)) return s;
  evaluate_all(diag);
  apply_directives(diag);
  teardown();
  if (!preview_.text().empty()) {
    stats_.last_text_preview = preview_.text().front();
    stats_.has_text_preview = true;
  }
  if (preview_.array_cell()) {
    stats_.last_array_preview = *preview_.array_cell();
    stats_.has_array_preview = true;
  }
  if (preview_.spill_record()) {
    volatile bool b = preview_.spill_record()->released;
    (void)b;
  }
  for (std::size_t i = 0; i < preview_.format_entries().size(); ++i) {
    std::string s = preview_.resolve_format_text(i);
    if (!s.empty()) {
      volatile char c = s.front();
      (void)c;
    }
  }
  preview_.release_borrows();
  return Status::ok;
}

}  // namespace slate
