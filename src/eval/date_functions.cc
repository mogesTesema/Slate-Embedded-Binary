// slate/eval/date_functions.cc
//
// Date arithmetic over a serial-day representation (days since 1970-01-01, using
// the standard civil<->days conversion). Only deterministic functions are
// provided — no TODAY/NOW — so a workbook always evaluates the same way, which
// the fuzzing pipeline requires. DATE builds a serial; YEAR/MONTH/DAY/WEEKDAY
// decompose one; EDATE/EOMONTH do calendar-aware month math; DAYS subtracts.
#include "slate/eval/functions.hh"

#include <cmath>

namespace slate {
namespace {

Value err(wire::ErrCode e) { return Value::error(e); }

// Days since 1970-01-01 for a proleptic Gregorian date (Howard Hinnant's
// algorithm). Valid for any y/m/d; callers pass normalized months.
long long days_from_civil(long long y, unsigned m, unsigned d) {
  y -= m <= 2;
  long long era = (y >= 0 ? y : y - 399) / 400;
  unsigned yoe = static_cast<unsigned>(y - era * 400);
  unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<long long>(doe) - 719468;
}

struct Civil { long long y; unsigned m; unsigned d; };

Civil civil_from_days(long long z) {
  z += 719468;
  long long era = (z >= 0 ? z : z - 146096) / 146097;
  unsigned doe = static_cast<unsigned>(z - era * 146097);
  unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  long long y = static_cast<long long>(yoe) + era * 400;
  unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  unsigned mp = (5 * doy + 2) / 153;
  unsigned d = doy - (153 * mp + 2) / 5 + 1;
  unsigned m = mp + (mp < 10 ? 3 : -9);
  return Civil{y + (m <= 2), m, d};
}

bool arg_int(EvalContext& ctx, const Operand& op, long long& out, wire::ErrCode& e) {
  double d;
  Value v = load_scalar(ctx, op);
  if (!coerce_number(ctx, v, d, e)) return false;
  out = static_cast<long long>(std::floor(d));
  return true;
}

unsigned last_day_of_month(long long y, unsigned m) {
  // Day 0 of next month = last day of this month.
  unsigned nm = m == 12 ? 1 : m + 1;
  long long ny = m == 12 ? y + 1 : y;
  return static_cast<unsigned>(days_from_civil(ny, nm, 1) - days_from_civil(y, m, 1));
}

Value fn_date(EvalContext& ctx, const std::vector<Operand>& args) {
  long long y, m, d; wire::ErrCode e;
  if (!arg_int(ctx, args[0], y, e)) return err(e);
  if (!arg_int(ctx, args[1], m, e)) return err(e);
  if (!arg_int(ctx, args[2], d, e)) return err(e);
  // Normalize out-of-range months (Excel rolls them into adjacent years).
  y += (m - 1) / 12;
  m = (m - 1) % 12 + 1;
  if (m <= 0) { m += 12; --y; }
  long long serial = days_from_civil(y, static_cast<unsigned>(m), 1) + (d - 1);
  return Value::number(static_cast<double>(serial));
}

Value fn_component(EvalContext& ctx, const Operand& op, int which) {
  long long serial; wire::ErrCode e;
  if (!arg_int(ctx, op, serial, e)) return err(e);
  Civil c = civil_from_days(serial);
  switch (which) {
    case 0: return Value::number(static_cast<double>(c.y));
    case 1: return Value::number(static_cast<double>(c.m));
    case 2: return Value::number(static_cast<double>(c.d));
  }
  return err(wire::ErrCode::value);
}

Value fn_weekday(EvalContext& ctx, const std::vector<Operand>& args) {
  long long serial; wire::ErrCode e;
  if (!arg_int(ctx, args[0], serial, e)) return err(e);
  // 1970-01-01 was a Thursday; shift so Sunday = 0.
  long long dow = ((serial % 7) + 4) % 7;
  if (dow < 0) dow += 7;
  return Value::number(static_cast<double>(dow + 1));  // type 1: Sunday = 1
}

Value fn_edate(EvalContext& ctx, const std::vector<Operand>& args, bool end_of_month) {
  long long serial, months; wire::ErrCode e;
  if (!arg_int(ctx, args[0], serial, e)) return err(e);
  if (!arg_int(ctx, args[1], months, e)) return err(e);
  Civil c = civil_from_days(serial);
  long long total = static_cast<long long>(c.m - 1) + months;
  long long ny = c.y + total / 12;
  long long nm = total % 12;
  if (nm < 0) { nm += 12; --ny; }
  unsigned month = static_cast<unsigned>(nm) + 1;
  if (end_of_month) {
    unsigned last = last_day_of_month(ny, month);
    return Value::number(static_cast<double>(days_from_civil(ny, month, last)));
  }
  unsigned last = last_day_of_month(ny, month);
  unsigned d = c.d < last ? c.d : last;  // clamp (e.g. Jan 31 + 1 month)
  return Value::number(static_cast<double>(days_from_civil(ny, month, d)));
}

Value fn_days(EvalContext& ctx, const std::vector<Operand>& args) {
  long long a, b; wire::ErrCode e;
  if (!arg_int(ctx, args[0], a, e)) return err(e);
  if (!arg_int(ctx, args[1], b, e)) return err(e);
  return Value::number(static_cast<double>(a - b));
}

}  // namespace

Value call_date_function(EvalContext& ctx, wire::Func fn,
                         const std::vector<Operand>& args) {
  switch (fn) {
    case wire::Func::date: return fn_date(ctx, args);
    case wire::Func::year: return fn_component(ctx, args[0], 0);
    case wire::Func::month: return fn_component(ctx, args[0], 1);
    case wire::Func::day: return fn_component(ctx, args[0], 2);
    case wire::Func::weekday: return fn_weekday(ctx, args);
    case wire::Func::edate: return fn_edate(ctx, args, false);
    case wire::Func::eomonth: return fn_edate(ctx, args, true);
    case wire::Func::days: return fn_days(ctx, args);
    default: return call_math_function(ctx, fn, args);
  }
}

}  // namespace slate
