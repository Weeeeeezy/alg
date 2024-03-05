// vim:ts=2:et
//===========================================================================//
//                          "Basis/TimeValUtils.hpp":                        //
//                       Extra Utils for "utxx::time_val"                    //
//===========================================================================//
#pragma once

#include "Basis/Macros.h"
#include <boost/core/noncopyable.hpp>
#include <utxx/compiler_hints.hpp>
#include <utxx/convert.hpp>
#include <utxx/time_val.hpp>

namespace MAQUETTE
{
  //=========================================================================//
  // Consts:                                                                 //
  //=========================================================================//
  // The earliest possible date we recognise:
  constexpr int EarliestDate = 20150101;

  // The range of years we recognise:
  constexpr int MinYear      = 2015;
  constexpr int MaxYear      = 2099;

  //=========================================================================//
  // "DaysInMonth":                                                          //
  //=========================================================================//
  // NB: If "year" or "month" are themselves invalid, no exception is thrown --
  // 0 is returned instead, so any comparison of an actual date for "less" with
  // this total, will fail:
  //
  inline int DaysInMonth(int a_year, int a_month) noexcept
  {
    return (a_month == 4 || a_month == 6 || a_month == 9 || a_month == 11)
            ? 30
            : (a_month == 2)
            ? ((a_year % 4 != 0 || (a_year % 100 == 0 && a_year % 400 != 0))
              ? 28 : 29)
            : (0 < a_month && a_month <= 12)
              ? 31 : 0;
  }

  //=========================================================================//
  // "IsValidDate":                                                          //
  //=========================================================================//
  inline bool IsValidDate(int a_year, int a_month, int a_day) noexcept
  {
    return MinYear <= a_year && a_year <= MaxYear && 1 <= a_month &&
           a_month <= 12 && 1 <= a_day && a_day <= DaysInMonth(a_year, a_month);
  }

  //=========================================================================//
  // "IsValidTime":                                                          //
  //=========================================================================//
  // XXX: Leap Seconds are allowed but we do not check the validity of the ins-
  // tants when they can occur:
  //
  inline bool IsValidTime(int a_hour, int a_min, int a_sec) noexcept
  {
    return 0 <= a_hour && a_hour <= 23 && 0 <= a_min && a_min <= 59 &&
           0 <= a_sec && a_sec <= 60;
  }

  //=========================================================================//
  // "IsValidDateTime":                                                      //
  //=========================================================================//
  inline bool IsValidDateTime
    (int a_year, int a_month, int a_day, int a_hour, int a_min, int a_sec)
  noexcept
  {
    return IsValidDate(a_year, a_month, a_day) &&
           IsValidTime(a_hour, a_min, a_sec);
  }

  //=========================================================================//
  // "IncrDate":                                                             //
  //=========================================================================//
  // XXX: Args are Non-Const Refs:
  //
  inline void IncrDate(int& a_year, int& a_month, int& a_day)
  {
    int M = DaysInMonth(a_year, a_month);

    CHECK_ONLY(if (utxx::unlikely(a_day <= 0 || a_day > M))
               // This includes the case M==0 when (Year, Month) were invalid:
               throw utxx::badarg_error("IncrDate: Invalid Date", a_year,
                                        a_month, a_day);)
    // OK, increment it:
    if (a_day < M)
      ++a_day;
    else
    if (a_month < 12)
    {
      ++a_month;
      a_day = 1;
    }
    else
    {
      ++a_year;  // XXX: We do not check the resulting year for MaxYear
      a_month = 1;
      a_day   = 1;
    }
  }

  //=========================================================================//
  // "DecrDate":                                                             //
  //=========================================================================//
  // XXX: Again, Args are Non-Const Refs:
  //
  inline void DecrDate(int& a_year, int& a_month, int& a_day)
  {
    CHECK_ONLY
    (
      int M = DaysInMonth(a_year, a_month);

      if (utxx::unlikely(a_day <= 0 || a_day > M))
        // This includes the case M==0 when (Year, Month) were invalid:
        throw utxx::badarg_error
              ("DecrDate: Invalid Date", a_year, a_month, a_day);)

    // OK, decrement it:
    if (a_day > 1)
      --a_day;
    else
    if (a_month > 1)
    {
      --a_month;
      a_day = DaysInMonth(a_year, a_month);
    }
    else
    {
      --a_year;  // XXX: We do not check the resulting Year for MinYear
      a_month = 12;
      a_day   = 31;
    }
  }

  //=========================================================================//
  // "DateToTimeValFAST":                                                    //
  //=========================================================================//
  // NB: "a_date" is encoded in decimal format: "YYYYMMDD" (assumed to be UTC);
  // Returns an empty time stamp if the arg is mal-formatted:
  //
  template <bool IsStrict = false>
  inline utxx::time_val DateToTimeValFAST(int a_date) noexcept
  {
    if (utxx::likely(a_date != 0))
    {
      int day = a_date % 100;
      a_date /= 100;
      int month = a_date % 100;
      int year  = a_date / 100;

      // Check for invalid date components:
      if (utxx::likely(!IsStrict || IsValidDate(year, month, day)))
        // UTC=true:
        return utxx::time_val(year, unsigned(month), unsigned(day), true);
    }
    // Any error:
    return utxx::time_val();
  }

  //=========================================================================//
  // "DateToTimeValFIX":                                                     //
  //=========================================================================//
  // "a_date" is a string "YYYYMMDD":
  //                       01234567
  template <bool IsStrict = false>
  inline utxx::time_val DateToTimeValFIX(char const* a_date) noexcept
  {
    assert(a_date != nullptr);
    // XXX: We also assume without checking that "a_date" indeed has sufficient
    // length...
    int year  = 0;
    int month = 0;
    int day   = 0;
    return
      (utxx::likely
      (utxx::atoi_left<int, 4, char>(a_date, year)      == (a_date + 4) &&
       utxx::atoi_left<int, 2, char>(a_date + 4, month) == (a_date + 6) &&
       utxx::atoi_left<int, 2, char>(a_date + 6, day)   == (a_date + 8) &&
      (!IsStrict || IsValidDate(year, month, day))))
       // UTC=true:
       ? utxx::time_val(year, unsigned(month), unsigned(day), true)
       // Any error:
       : utxx::time_val();
  }

  //=========================================================================//
  // "TimeToTimeVal":                                                        //
  //=========================================================================//
  // "a_time" is a string "hh:mm:ss[.mmm]" ( WithSeparators: eg FIX)
  //                       01234567[8901]
  // or                   "hhmmssmmm"      (!WithSeparators: eg ITCH)
  //                       012345678
  // XXX: It is a legacy implementation; it would be better to provide 2 diffe-
  // rent impls for FIX- and ITCH-type formats:
  //
  template <bool IsStrict, bool WithSeparators>
  inline utxx::time_val TimeToTimeVal(char const* a_time)
  {
    assert(a_time != nullptr);
    // XXX: We also assume without checking that "a_time" indeed has sufficient
    // length (and 1 char beyond it is available):
    int hour = 0;
    int min  = 0;
    int sec  = 0;
    int msec = 0;

    constexpr int OffHour = 0;
    constexpr int OffMin  = OffHour + (WithSeparators ? 3 : 2);
    constexpr int OffSec  = OffMin  + (WithSeparators ? 3 : 2);
    constexpr int OffMSec = OffSec  + (WithSeparators ? 3 : 2);

    // Hours:
    bool ok1 = (utxx::atoi_left<int, 2, char>(a_time + OffHour, hour) ==
                a_time + (OffHour  + 2) &&    hour >= 0);
    bool ok2 = (!WithSeparators || a_time[OffHour + 2] == ':');

    // Minutes:
    bool ok3 = (utxx::atoi_left<int, 2, char>(a_time + OffMin, min) ==
                a_time + (OffMin   + 2) &&    min >= 0);
    bool ok4 = (!WithSeparators || a_time[OffMin + 2] == ':');

    // Seconds:
    bool ok5 = (utxx::atoi_left<int, 2, char>(a_time + OffSec, sec) ==
                a_time + (OffSec   + 2) &&    sec >= 0);

    // MilliSeconds:
    bool ok6 =
      (!WithSeparators || a_time[OffSec + 2] == '.')
      ? // Then there is an "mmm" or ".mmm" part:
        (utxx::atoi_left<int, 3, char>(a_time + OffMSec, msec) ==
         a_time + (OffMSec  + 3) &&    msec >= 0)
      : true;  // No Milliseconds: "msec" will be 0

    // The Result:
    return
      (utxx::likely(ok1 && ok2 && ok3 && ok4 && ok5 && ok6 &&
                   (!IsStrict || IsValidTime(hour, min, sec))))
      // NB: No UTC flag here because it's a "time interval", not "abs time":
      ? utxx::time_val(3600 * hour + 60 * min + sec, 1000 * msec)
      : utxx::time_val();
  }

  //-------------------------------------------------------------------------//
  // "TimeToTimeValFIX":                                                     //
  //-------------------------------------------------------------------------//
  template <bool IsStrict = false>
  inline utxx::time_val TimeToTimeValFIX(char const* a_time)
    { return TimeToTimeVal<IsStrict, true>(a_time); }

  //-------------------------------------------------------------------------//
  // "TimeToTimeValITCH":                                                    //
  //-------------------------------------------------------------------------//
  template <bool IsStrict = false>
  inline utxx::time_val TimeToTimeValITCH(char const* a_time)
    { return TimeToTimeVal<IsStrict, false>(a_time); }

  //=========================================================================//
  // "DateTimeToTimeValFAST" (1 arg):                                        //
  //=========================================================================//
  // "a_ts" is encoded in 64-bit decimal format:
  //      "YYYYMMDDHHMMSSsss" (including milliseconds)
  // Returns an empty value if the arg is mal-formed:
  //
  template <bool IsStrict = false>
  inline utxx::time_val DateTimeToTimeValFAST(long a_ts) noexcept
  {
    if (utxx::likely(a_ts != 0))
    {
      int usec = int(a_ts % 1000) * 1000;
      a_ts /= 1000;
      int sec = int(a_ts % 100);
      a_ts /= 100;
      int min = int(a_ts % 100);
      a_ts /= 100;
      int hour = int(a_ts % 100);
      a_ts /= 100;
      int day = int(a_ts % 100);
      a_ts /= 100;
      int month = int(a_ts % 100);
      int year  = int(a_ts / 100);

      // Check for invalid date and time components:
      if (utxx::likely(!IsStrict ||
                       IsValidDateTime(year, month, day, hour, min, sec)))
        // UTC=true:
        return utxx::time_val
               (year, unsigned(month), unsigned(day), unsigned(hour),
                      unsigned(min),   unsigned(sec), unsigned(usec),  true);
    }
    // Any error:
    return utxx::time_val();
  }

  //=========================================================================//
  // "DateTimeToTimeValFAST" (2- or 3-arg):                                  //
  //=========================================================================//
  // "a_date" is encoded in decimal format: "YYYYMMDD"
  // "a_time" is encoded in decimal format: "HHMMSSsss" (NB: still w/msecs!)
  // "a_usec" is the number of microseconds to be added
  // Returns an empty time stamp if the arg(s) are mal-formed:
  //
  template <bool IsStrict = false>
  inline utxx::time_val DateTimeToTimeValFAST
    (int a_date, long a_time, int a_usec = 0) noexcept
  {
    // NB: Unlike a_date==0, a_time==0 is perfectly valid!
    if (utxx::likely(a_date != 0))
    {
      int day = a_date   % 100;
      a_date /= 100;
      int month = a_date % 100;
      int year  = int(a_date / 100);

      a_usec += int(a_time  % 1000) * 1000;
      a_time /= 1000;
      int sec = int(a_time  % 100);
      a_time /= 100;
      int min  = int(a_time % 100);
      int hour = int(a_time / 100);

      // Check for invalid date and time components. XXX: We allow for leap
      // seconds (sec==60) but do not check the instants when they can occur!
      if (utxx::likely
         (!IsStrict || IsValidDateTime(year, month, day, hour, min, sec)))
        // UTC=true:
        return utxx::time_val
               (year, unsigned(month), unsigned(day), unsigned(hour),
                      unsigned(min),   unsigned(sec), unsigned(a_usec), true);
    }
    // Any error:
    return utxx::time_val();
  }

  //=========================================================================//
  // "DateTimeToTimeValFAST" (2- or 3-arg, with pre-computed "a_date"):      //
  //=========================================================================//
  // FAST Protocol Encoding:
  // "a_date": must indeed represent date start (00:00:00.000) -- NOT CHECKED!
  // "a_time"  is encoded in decimal format: "HHMMSSsss" (including millisecs)
  // "a_usec"  (microseconds), if specified, as simply added to "a_time";
  //
  template <bool IsStrict = false>
  inline utxx::time_val DateTimeToTimeValFAST
    ( utxx::time_val a_date, long a_time, int a_usec = 0)
  noexcept
  {
    // NB: We do not check for a_time==0; normally, it would be non-0:
    //
    a_usec  += int(a_time % 1000) * 1000;
    a_time  /= 1000;

    int sec  = int(a_time % 100);
    a_time  /= 100;
    int min  = int(a_time % 100);
    int hour = int(a_time / 100);

    // Check for invalid date and time components.
    // NB: we can get a_usec >= 1000000 because they are added to msec (but
    //  utxx::time_val ctor still works correctly in that case):
    return
      (utxx::likely(!IsStrict || IsValidTime(hour, min, sec)))
      ? (a_date + utxx::time_val(3600 * hour + 60 * min + sec, a_usec))
      : utxx::time_val();  // Invalid val
  }

  //=========================================================================//
  // "DateTimeToTimeValFIX" (1-arg, string):                                 //
  //=========================================================================//
  // "a_ts" is a string "YYYYMMDD-hh:mm:ss[.sss]"
  //                     01234567890123456[7890]
  //
  template <bool IsStrict = false>
  inline utxx::time_val DateTimeToTimeValFIX(char const* a_ts) noexcept
  {
    assert(a_ts != nullptr);
    // XXX: We also assume without checking that "a_date" indeed has sufficient
    // length...
    int year  = 0;
    int month = 0;
    int day   = 0;
    int hour  = 0;
    int min   = 0;
    int sec   = 0;
    int msec  = 0;

    return
      (utxx::likely
      (utxx::atoi_left<int, 4, char>(a_ts,     year)  == (a_ts +  4) &&
       year  >= 0 &&
       utxx::atoi_left<int, 2, char>(a_ts + 4, month) == (a_ts +  6) &&
       month >= 0 &&
       utxx::atoi_left<int, 2, char>(a_ts + 6, day)   == (a_ts +  8) &&
       day   >= 0 && a_ts[8] == '-'  &&
       utxx::atoi_left<int, 2, char>(a_ts + 9, hour)  == (a_ts + 11) &&
       hour  >= 0 && a_ts[11] == ':' &&
       utxx::atoi_left<int, 2, char>(a_ts + 12, min)  == (a_ts + 14) &&
       min   >= 0 && a_ts[14] == ':' &&
       utxx::atoi_left<int, 2, char>(a_ts + 15, sec)  == (a_ts + 17) &&
       sec   >= 0 &&
       // NB: The msec part can be omitted:
       (a_ts[17] != '.' ||
       (a_ts[17] == '.' &&
        utxx::atoi_left<int, 3, char>(a_ts + 18, msec) == (a_ts + 21) &&
        msec >= 0))     &&
       // Again, leap seconds (sec==60) are allowed but not verified:
       (!IsStrict       ||
        IsValidDateTime(year, month, day, hour, min, sec))))
       // UTC=true:
       ? utxx::time_val(year, unsigned(month), unsigned(day),
                              unsigned(hour),  unsigned(min), unsigned(sec),
                              unsigned(1000 * msec),   true)
       // Any error:
       : utxx::time_val();
  }

  //=========================================================================//
  // "DateTimeToTimeValTZ":                                                  //
  //=========================================================================//
  // "a_ts" is a string "YYYY-MM-DDThh:mm:ss.mmm[uuu]Z"
  //
  inline utxx::time_val DateTimeToTimeValTZ(char const* a_ts) noexcept
  {
    assert(a_ts != nullptr);
    // Milli- or micro-seconds:
    if (utxx::likely(*(a_ts + 23) == 'Z' || *(a_ts + 26) == 'Z'))
    {
      // XXX: We also assume without checking that "a_date" indeed has
      // sufficient length...
      int year  = 0;
      int month = 0;
      int day   = 0;
      int hour  = 0;
      int min   = 0;
      int sec   = 0;
      int msec  = 0;

      if (utxx::likely
         (utxx::atoi_left<int, 4, char>(a_ts,      year)  == (a_ts +  4) &&
          year  >= 0 && a_ts[4]  == '-' &&
          utxx::atoi_left<int, 2, char>(a_ts +  5, month) == (a_ts +  7) &&
          month >= 0 && a_ts[7]  == '-' &&
          utxx::atoi_left<int, 2, char>(a_ts +  8, day)   == (a_ts + 10) &&
          day   >= 0 && a_ts[10] == 'T' &&
          utxx::atoi_left<int, 2, char>(a_ts + 11, hour)  == (a_ts + 13) &&
          hour  >= 0 && a_ts[13] == ':' &&
          utxx::atoi_left<int, 2, char>(a_ts + 14, min)   == (a_ts + 16) &&
          min   >= 0 && a_ts[16] == ':' &&
          utxx::atoi_left<int, 2, char>(a_ts + 17, sec)   == (a_ts + 19) &&
          sec   >= 0 && a_ts[19] == '.' &&
          utxx::atoi_left<int, 3, char>(a_ts + 20, msec)  == (a_ts + 23) &&
          msec   >= 0))
      {
        // The result up to milliseconds (UTC=true):
        utxx::time_val res
          (year, unsigned(month), unsigned(day), unsigned(hour),
                 unsigned(min),   unsigned(sec), unsigned(1000 * msec), true);

        // Check for the microsecond part (extra "usec" on top of "msec" already
        // applied):
        int usec = 0;
        if (*(a_ts + 26) == 'Z' &&
            utxx::atoi_left<int, 3, char>(a_ts + 23, usec) == (a_ts + 26) &&
            usec >= 0)
          res += utxx::usecs(usec);
        return res;
      }
    }
    // Any error:
    return utxx::time_val();
  }

  //=========================================================================//
  // "DateTimeToTimeValTZhhmm":                                              //
  //=========================================================================//
  // "a_ts" is a string "YYYY-MM-DDThh:mm:ss.mmmuuu±hh:mm"
  //
  inline utxx::time_val DateTimeToTimeValTZhhmm(char const* a_ts) noexcept
  {
    assert(a_ts != nullptr);
    // Milli- or micro-seconds:
    if (utxx::likely(*(a_ts + 23) == '+' || *(a_ts + 23) == '-' ||
                     *(a_ts + 26) == '+' || *(a_ts + 26) == '-'))
    {
      // XXX: We also assume without checking that "a_date" indeed has
      // sufficient length...
      int year  = 0;
      int month = 0;
      int day   = 0;
      int hour  = 0;
      int min   = 0;
      int sec   = 0;
      int usec  = 0;
//      int tzhh  = 0;
//      int tzmm  = 0;

      if (utxx::likely
          (utxx::atoi_left<int, 4, char>(a_ts,      year)  == (a_ts +  4) &&
           year  >= 0 && a_ts[4]  == '-' &&
           utxx::atoi_left<int, 2, char>(a_ts +  5, month) == (a_ts +  7) &&
           month >= 0 && a_ts[7]  == '-' &&
           utxx::atoi_left<int, 2, char>(a_ts +  8, day)   == (a_ts + 10) &&
           day   >= 0 && a_ts[10] == 'T' &&
           utxx::atoi_left<int, 2, char>(a_ts + 11, hour)  == (a_ts + 13) &&
           hour  >= 0 && a_ts[13] == ':' &&
           utxx::atoi_left<int, 2, char>(a_ts + 14, min)   == (a_ts + 16) &&
           min   >= 0 && a_ts[16] == ':' &&
           utxx::atoi_left<int, 2, char>(a_ts + 17, sec)   == (a_ts + 19) &&
           sec   >= 0 && a_ts[19] == '.' &&
           utxx::atoi_left<int, 6, char>(a_ts + 20, usec)  == (a_ts + 26) &&
           usec  >= 0 &&
           (a_ts[26] == '+' || a_ts[26] == '-') &&
           a_ts[27]  == '0' && a_ts[28]  == '0' &&
           a_ts[29]  == ':' &&
           a_ts[30]  == '0' && a_ts[31]  == '0'
//           utxx::atoi_left<int, 2, char>(a_ts + 27, tzhh)  == (a_ts + 29) &&
//           tzhh >= 0 && a_ts[29]  == ':' &&
//           utxx::atoi_left<int, 2, char>(a_ts + 30, tzmm)  == (a_ts + 32)) &&
//           tzmm  >= 0
           ))
      {
//        if (utxx::likely(a_ts[26]  == '+'))
//        {
//          tzhh *= -1;
//          tzmm *= -1;
//        }
//        else
//        if (utxx::unlikely(a_ts[26] != '-'))
//          return utxx::time_val();

//        hour += tzhh;
//        min  += tzmm;

        // The result up to milliseconds (UTC=true):
        utxx::time_val res
          (year, unsigned(month), unsigned(day), unsigned(hour),
           unsigned(min),   unsigned(sec), unsigned(usec), true);

        return res;
      }
    }
    // Any error:
    return utxx::time_val();
  }

  //=========================================================================//
  // "DateTimeToTimeValSQL":                                                 //
  //=========================================================================//
  // "a_ts" is a string "YYYY-MM-DD hh:mm:ss.mmm[uuu]"
  //                     01234567890123456789012[345]
  //
  template <bool IsStrict = false>
  inline utxx::time_val DateTimeToTimeValSQL(char const* a_ts) noexcept
  {
    assert(a_ts != nullptr);
    // XXX: We also assume without checking that "a_date" indeed has sufficient
    // length...
    int year  = 0;
    int month = 0;
    int day   = 0;
    int hour  = 0;
    int min   = 0;
    int sec   = 0;
    int msec  = 0;

    if (utxx::likely
       (utxx::atoi_left<int, 4, char>(a_ts,    year)  == (a_ts +  4) &&
       year  >= 0 && a_ts[4]  == '-' &&
       utxx::atoi_left<int, 2, char>(a_ts + 5, month) == (a_ts +  7) &&
       month >= 0 && a_ts[7]  == '-' &&
       utxx::atoi_left<int, 2, char>(a_ts + 8, day)   == (a_ts + 10) &&
       day   >= 0 && a_ts[10] == ' ' &&
       utxx::atoi_left<int, 2, char>(a_ts + 11, hour) == (a_ts + 13) &&
       hour  >= 0 && a_ts[13] == ':' &&
       utxx::atoi_left<int, 2, char>(a_ts + 14, min)  == (a_ts + 16) &&
       min   >= 0 && a_ts[16] == ':' &&
       utxx::atoi_left<int, 2, char>(a_ts + 17, sec)  == (a_ts + 19) &&
       sec   >= 0 && a_ts[19] == '.' &&
       utxx::atoi_left<int, 3, char>(a_ts + 20, msec) == (a_ts + 23) &&
       msec  >= 0 &&
       // Again, leap seconds (sec==60) are allowed but not verified:
       (!IsStrict || IsValidDateTime(year, month, day, hour, min, sec))))
    {
      // The result up to milliseconds (UTC=true):
      utxx::time_val res
        (year, unsigned(month), unsigned(day), unsigned(hour),
               unsigned(min),   unsigned(sec), unsigned(1000 * msec), true);

      // Check for the microsecond part (extra "usec" on top of "msec" already
      // applied):
      int usec = 0;
      if (utxx::atoi_left<int, 3, char>(a_ts + 23, usec) == (a_ts + 26) &&
          usec >= 0)
        res += utxx::usecs(usec);
      return res;
    }
    // Any error:
    return utxx::time_val();
  }

  //=========================================================================//
  // "OutputTimeFIX(hour, min, sec, usec)", UTC assumed:                     //
  //=========================================================================//
  //                            012345678901 234
  // The Time output format is "hh:mm:ss.sss[uuu]", so "a_buff" must be at
  // least 13 (or 16, if usecs are used) chars long (incl the terminator);
  // "a_musec" is interpreted either as milliseconds  or  as microseconds,
  // depending on the "WithUSec" flag.  Returns a ptr to  the  terminator:
  //
  template <bool WithUSec = false>
  inline char* OutputTimeFIX
    (int a_hour, int a_min, int a_sec, int a_musec, char* a_buff)
  {
    // XXX: The following pre-conds are only asserted, for efficiency:
    assert(IsValidTime(a_hour, a_min, a_sec) && 0 <= a_musec &&
           ((WithUSec && a_musec <= 999999) || (!WithUSec && a_musec <= 999)));

    (void) utxx::itoa_right<int, 2, char>(a_buff,     a_hour, '0');
    a_buff[2] = ':';
    (void) utxx::itoa_right<int, 2, char>(a_buff + 3, a_min,  '0');
    a_buff[5] = ':';
    (void) utxx::itoa_right<int, 2, char>(a_buff + 6, a_sec,  '0');
    a_buff[8] = '.';

    if (WithUSec)
    {
      (void) utxx::itoa_right<int, 6, char>(a_buff + 9, a_musec, '0');
      a_buff[15] = '\0';
      return (a_buff + 15);
    }
    else
    {
      (void) utxx::itoa_right<int, 3, char>(a_buff + 9, a_musec, '0');
      a_buff[12] = '\0';
      return (a_buff + 12);
    }
    // This point is unreachable:
    __builtin_unreachable();
  }

  //=========================================================================//
  // "OutputDateFIX", UTC assumed:                                           //
  //=========================================================================//
  // The Date output format is "YYYYMMDD" ("a_buff" must be >= 9 bytes long):
  //                            01234567
  // Returns a ptr to the terminator:
  //
  inline char* OutputDateFIX(int a_year, int a_month, int a_day, char* a_buff)
  {
    // XXX: the following pre-conds are only asserted, for efficiency:
    assert(a_buff != nullptr && IsValidDate(a_year, a_month, a_day));

    (void) utxx::itoa_right<int, 4, char>(a_buff,     a_year,  '0');
    (void) utxx::itoa_right<int, 2, char>(a_buff + 4, a_month, '0');
    (void) utxx::itoa_right<int, 2, char>(a_buff + 6, a_day,   '0');
    a_buff[8] = '\0';

    return (a_buff + 8);
  }

  //=========================================================================//
  // "OutputDateFIX(utxx::time_val)", UTC assumed:                           //
  //=========================================================================//
  // Same requirements regarding "a_buff" as above. Returns a ptr to the termi-
  // nator:
  inline char* OutputDateFIX(utxx::time_val a_ts, char* a_buff)
  {
    if (utxx::unlikely(a_ts.empty()))
    {
      *a_buff = '\0';
      return a_buff;
    }
    // Generic Case:
    auto ymd   = a_ts.to_ymd();
    int  year  = std::get<0>    (ymd);
    int  month = int(std::get<1>(ymd));
    int  day   = int(std::get<2>(ymd));

    return OutputDateFIX(year, month, day, a_buff);
  }

  //=========================================================================//
  // "OutputDateTimeFIX(utxx::time_val)", UTC assumed:                       //
  //=========================================================================//
  // The buffer must be >= 22 (or 25, if usecs are used) bytes long, incl the
  // terminator). Format: "YYYYMMDD-hh:mm:ss.sss[uuu]":
  //                       012345678901234567890 123
  // Returns a ptr to the terminator:
  //
  template <bool WithUSec = false>
  inline char* OutputDateTimeFIX(utxx::time_val a_ts, char* a_buff)
  {
    if (utxx::unlikely(a_ts.empty()))
    {
      *a_buff = '\0';
      return a_buff;
    }

    // Generfic Case:
    auto ymdhms = a_ts.to_ymdhms();
    int  year   = std::get<0>    (ymdhms);
    int  month  = int(std::get<1>(ymdhms));
    int  day    = int(std::get<2>(ymdhms));
    int  hour   = int(std::get<3>(ymdhms));
    int  min    = int(std::get<4>(ymdhms));
    int  sec    = int(std::get<5>(ymdhms));
    int  musec  = WithUSec ? int(a_ts.usec()) : int(a_ts.msec());

    a_buff  = OutputDateFIX(year, month, day, a_buff);
    *a_buff = '-';
    a_buff  = OutputTimeFIX<WithUSec>(hour, min, sec, musec, a_buff + 1);
    assert(*a_buff == '\0');
    return a_buff;
  }

  //=========================================================================//
  // "GetDate":                                                              //
  //=========================================================================//
  // In "a_ts", {hh:mm:ss.ssssss} are dropped, so the result corresponds to the
  // beginning of that date. The arg is assumed to be UTC:
  //
  inline utxx::time_val GetDate(utxx::time_val a_ts) noexcept
  {
    struct tm tm;
    // Seconds from the Epoch; fractional secs dropped, as they would never
    // cross the Date boundary:
    time_t s = a_ts.sec();
    (void) gmtime_r(&s, &tm);

    // Get back to "time_val": from "a_ts", subtract the number of seconds ela-
    // psed since the beginning of the curr date:
    s -= (3600 * tm.tm_hour + 60 * tm.tm_min + tm.tm_sec);

    // Seconds from the Epoch corresponding to the beginning of that Date:
    utxx::secs   secs(s);
    return utxx::time_val(secs);
  }

  //=========================================================================//
  // "GetCurrDate":                                                          //
  //=========================================================================//
  // Returns the current UTC date and its components.
  // XXX: For efficiency, uses a statically-initialised object holding the curr
  // date. Thus, the result of this function will be incorrect if the applicat-
  // ion runs continuously for more than 24 hours:
  //
  class CurrDateHolder : public boost::noncopyable
  {
  private:
    //-----------------------------------------------------------------------//
    // Curr Date and Its Components:                                         //
    //-----------------------------------------------------------------------//
    utxx::time_val m_date;
    int            m_year;
    int            m_month;
    int            m_day;
    char           m_dateStr[9];  // "YYYYMMDD"
    int            m_dateInt;     //  YYYYMMDD

  public:
    //-----------------------------------------------------------------------//
    // Default Ctor:                                                         //
    //-----------------------------------------------------------------------//
    CurrDateHolder()
    : m_date (GetDate(utxx::now_utc())),
      m_year (0),
      m_month(0),
      m_day  (0)
    {
      // Now get the Date components (UTC=true). FIXME: This is  because ALL of
      // our time stamps are curerntly in UTC. However, this can cause problems
      // if an application runs round-the-clock, esp in a non-UTC time-zone:
      auto ymd = m_date.to_ymd  (true);
      m_year   = std::get<0>    (ymd);
      m_month  = int(std::get<1>(ymd));
      m_day    = int(std::get<2>(ymd));

      // DateStr: This is not time-critical, but we still avoid "sprintf":
      (void) utxx::itoa_right<int, 4, char>(m_dateStr,     m_year,  '0');
      (void) utxx::itoa_right<int, 2, char>(m_dateStr + 4, m_month, '0');
      (void) utxx::itoa_right<int, 2, char>(m_dateStr + 6, m_day,   '0');
      m_dateStr[8] = '\0';

      // DateInt:
      m_dateInt = m_year * 10000 + m_month * 100 + m_day;
    }

    //-----------------------------------------------------------------------//
    // Accessors:                                                            //
    //-----------------------------------------------------------------------//
    utxx::time_val operator()() const { return m_date;    }
    int            GetYear()    const { return m_year;    }
    int            GetMonth()   const { return m_month;   }
    int            GetDay()     const { return m_day;     }
    char const*    GetDateStr() const { return m_dateStr; }
    int            GetDateInt() const { return m_dateInt; }
  };

  //-------------------------------------------------------------------------//
  // Singleton Holder Object:                                                //
  //-------------------------------------------------------------------------//
  extern CurrDateHolder const s_CurrDateH;

  //-------------------------------------------------------------------------//
  // Accessor Functions:                                                     //
  //-------------------------------------------------------------------------//
  inline utxx::time_val GetCurrDate()    { return s_CurrDateH();            }
  inline int            GetCurrYear()    { return s_CurrDateH.GetYear();    }
  inline int            GetCurrMonth()   { return s_CurrDateH.GetMonth();   }
  inline int            GetCurrDay()     { return s_CurrDateH.GetDay();     }
  inline char const*    GetCurrDateStr() { return s_CurrDateH.GetDateStr(); }
  inline int            GetCurrDateInt() { return s_CurrDateH.GetDateInt(); }

  //=========================================================================//
  // "GetCurrTime":                                                          //
  //=========================================================================//
  inline utxx::time_val GetCurrTime()
    { return utxx::now_utc() - GetCurrDate(); }
}  // End namespace MAQUETTE
