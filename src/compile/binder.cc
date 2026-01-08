// slate/compile/binder.cc
#include "slate/compile/binder.hh"

namespace slate {

Arity function_arity(wire::Func f) {
  using F = wire::Func;
  constexpr std::uint8_t var = 0xFF;
  switch (f) {
    case F::sum: case F::avg: case F::min: case F::max: case F::count:
    case F::and_: case F::or_: case F::concat_fn:
    case F::product: case F::gcd: case F::lcm:
    case F::median: case F::stdev: case F::var_:
    case F::xor_: case F::sumsq:
    case F::varp: case F::stdevp: case F::counta: case F::mode:
    case F::geomean:
      return {1, var};
    case F::transpose:
      return {1, 1};
    case F::sumproduct:
      return {2, var};
    case F::mmult:
      return {2, 2};
    case F::abs_: case F::sqrt_: case F::not_: case F::len_:
    case F::upper_: case F::lower_:
    case F::int_: case F::trunc_: case F::sign_: case F::trim:
    case F::isnumber: case F::istext: case F::iserror: case F::isblank:
    case F::year: case F::month: case F::day:
    case F::iseven: case F::isodd: case F::n_fn:
    case F::proper: case F::char_: case F::code_: case F::value_fn:
    case F::clean:
    case F::row_fn: case F::column_fn: case F::rows_fn: case F::columns_fn:
    case F::countblank:
      return {1, 1};
    case F::address: case F::ifna:
      return {2, 2};
    case F::sumifs: case F::switch_fn:
      return {3, var};
    case F::countifs: case F::ifs:
      return {2, var};
    case F::true_fn: case F::false_fn:
      return {0, 0};
    case F::islogical: case F::isnontext:
      return {1, 1};
    case F::large: case F::small:
      return {2, 2};
    case F::search:
      return {2, 3};
    case F::replace_fn:
      return {4, 4};
    case F::weekday: case F::log_:
      return {1, 2};
    case F::date:
      return {3, 3};
    case F::edate: case F::eomonth: case F::days:
    case F::atan2_: case F::roundup: case F::rounddown: case F::mround:
    case F::combin: case F::permut: case F::quotient:
      return {2, 2};
    case F::exp_: case F::ln_: case F::log10_:
    case F::sin_: case F::cos_: case F::tan_:
    case F::asin_: case F::acos_: case F::atan_:
    case F::degrees_: case F::radians_: case F::fact:
      return {1, 1};
    case F::pi_:
      return {0, 0};
    case F::pmt: case F::fv: case F::pv: case F::nper:
      return {3, 5};
    case F::npv:
      return {2, var};
    case F::round_: case F::mod_: case F::power:
    case F::left_: case F::right_:
    case F::countif: case F::ceiling_: case F::floor_:
    case F::rept: case F::exact: case F::iferror:
    case F::text_fn:
      return {2, 2};
    case F::match_:
      return {2, 3};  // lookup_value, lookup_array, [match_type]
    case F::if_: case F::mid_: case F::vlookup: case F::index_:
    case F::hlookup:
      return {3, 3};
    case F::sumif: case F::averageif: case F::rank: case F::find_:
      return {2, 3};
    case F::substitute:
      return {3, 4};
    case F::textjoin: case F::choose:
      return {2, var};
  }
  return {var, 0};  // unknown: impossible min>max range, distinct from {0,0} (a
                    // real zero-arg arity used by PI/TRUE/FALSE)
}

Status bind_program(Program& p, const Workbook& wb, DiagnosticSink& diag) {
  const std::size_t sheet_count = wb.sheets.size();
  const std::size_t name_count = wb.names.size();

  for (const Instr& in : p.code) {
    switch (in.op) {
      case wire::Op::push_ref:
      case wire::Op::push_range:
        if (in.a >= sheet_count) {
          diag.error(Status::unknown_sheet, "formula references unknown sheet");
          return Status::unknown_sheet;
        }
        break;
      case wire::Op::push_name:
        if (in.a >= name_count) {
          diag.error(Status::name_error, "formula references unknown name");
          return Status::name_error;
        }
        if (p.name_sheet_cache.size() < name_count) p.name_sheet_cache.resize(name_count);
        p.name_sheet_cache[in.a] = wb.sheets.at(wb.names[in.a].sheet).data.get();
        break;
      case wire::Op::call: {
        Arity ar = function_arity(static_cast<wire::Func>(in.func));
        if (ar.min == 0xFF && ar.max == 0) {  // unknown-function sentinel
          diag.error(Status::bad_opcode, "call to unknown function");
          return Status::bad_opcode;
        }
        if (in.argc < ar.min || (ar.max != 0xFF && in.argc > ar.max)) {
          diag.error(Status::arity_mismatch, "wrong argument count");
          return Status::arity_mismatch;
        }
        break;
      }
      default:
        break;
    }
  }
  p.bound = true;
  return Status::ok;
}

}  // namespace slate
