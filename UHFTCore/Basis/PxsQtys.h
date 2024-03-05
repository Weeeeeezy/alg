// vim:ts=2:et
//===========================================================================//
//                               "Basis/PxsQtys.h":                          //
//            Safe Types for Representing Asset Prices and Qtys              //
//===========================================================================//
#pragma once
#include "Basis/Maths.hpp"
#include "Basis/Macros.h"
#include <type_traits>
#include <utxx/error.hpp>
#include <utxx/compiler_hints.hpp>
#include <sstream>

namespace MAQUETTE
{
  //=========================================================================//
  // "PriceT":                                                               //
  //=========================================================================//
  // A wrapper around "double" with Tolerance-based comparisons:
  //
  class PriceT
  {
  private:
    //-----------------------------------------------------------------------//
    // Data Flds: Both Val and Tol are stored, but not PxStep:               //
    //-----------------------------------------------------------------------//
    // NB: PxStep is not stored for compactness reasons. It is used much less
    // frequently than Tolerance, and if necessary, can easily and accurately
    // be re-constructed from the latter:
    //
    double  m_val;

  public:
    //-----------------------------------------------------------------------//
    // Tolerance:                                                            //
    //-----------------------------------------------------------------------//
    // Use the following value which is at least 10 times smaller than any known
    // PxStep, yet large enough not to be blurred by "double" rounding errors.
    // It is made visible for public use:
    //
    constexpr static double s_Tol = 1e-13;

    //-----------------------------------------------------------------------//
    // Ctors, Dtor, ...
    //-----------------------------------------------------------------------//
    constexpr PriceT()                     : m_val(NaN<double>) {}
    constexpr explicit PriceT(double a_val): m_val(a_val)       {}

    // Copy Ctor is auto-generated. Dtor is trivial.
    // Assignemnt is auto-generated as well.

    // Conversion back to "double":
    constexpr explicit operator double() const { return m_val; }

    //-----------------------------------------------------------------------//
    // Comparison Operators:                                                 //
    //-----------------------------------------------------------------------//
    // XXX: Comparison and arithmetic operations could also be made "constexpr",
    // but there is currently no real need for that:
    //
    bool operator<  (PriceT a_right) const
      { return (m_val - a_right.m_val) <   - s_Tol; }

    bool operator>= (PriceT a_right) const
      { return (m_val - a_right.m_val) >=  - s_Tol; }

    bool operator<= (PriceT a_right) const
      { return (m_val - a_right.m_val) <=    s_Tol; }

    bool operator>  (PriceT a_right) const
      { return (m_val - a_right.m_val) >     s_Tol; }

    bool operator== (PriceT a_right) const
      { return Abs(m_val - a_right.m_val) <  s_Tol; }

    bool operator!= (PriceT a_right) const
      { return Abs(m_val - a_right.m_val) >= s_Tol; }

    friend bool IsFinite  (PriceT a_px)
      { return ::MAQUETTE::IsFinite(a_px.m_val); }

    // The following is consistent with == 0.0:
    friend    bool IsZero (PriceT a_px) { return Abs(a_px.m_val) <  s_Tol; }

    // The following is consistent with >  0.0:
    friend    bool IsPos  (PriceT a_px) { return a_px.m_val      >= s_Tol; }

    //-----------------------------------------------------------------------//
    // Arithmetic Operations: Deltas are still "double"s, not "PriceT"s:     //
    //-----------------------------------------------------------------------//
    // XXX: No automatic rounding to a PxStep multiple is done here:
    //
    PriceT  operator+  (double a_delta) const
      { return PriceT(m_val  + a_delta); }

    PriceT& operator+= (double a_delta)
      { m_val += a_delta; return *this;  }

    PriceT operator-   (double a_delta) const
      { return PriceT(m_val  - a_delta); }

    PriceT& operator-= (double a_delta)
      { m_val -= a_delta; return *this;  }

    double operator-  (PriceT  a_right) const
      { return m_val - a_right.m_val;    }

    // Multiplication (eg by a factor representing PointPx, but NOT of two
    // "PriceT"s):
    PriceT  operator*  (double a_fact)  const
      { return PriceT(m_val  * a_fact);  }

    PriceT& operator*= (double a_fact)
      { m_val *= a_fact;  return *this;  }

    friend PriceT operator*(double a_fact, PriceT a_px)
      { return PriceT  (a_px.m_val * a_fact); }

    // Division is for convenience only:
    //
    PriceT operator/   (double a_div)   const
    {
      assert(a_div != 0.0);
      return PriceT(m_val / a_div);
    }

    PriceT& operator/= (double a_div)
    {
      assert(a_div != 0.0);
      m_val /= a_div;
      return *this;
    }

    //-----------------------------------------------------------------------//
    // Rounding to a PxStep multiple:                                        //
    //-----------------------------------------------------------------------//
    // Rounding Down:
    // NB: If "m_val" is an exact multiple of "a_px_step", it may still be imp-
    // roperly rounded down by Floor due to finite accuracy of division  -- so
    // move the Floor arg up a bit:
    //
    void RoundDown(double a_px_step)
    {
      assert(a_px_step > s_Tol);
      m_val = ::MAQUETTE::Floor(m_val / a_px_step + s_Tol) * a_px_step;
    }

    friend PriceT RoundDown(PriceT a_px, double a_px_step)
    {
      assert(a_px_step > PriceT::s_Tol);
      return PriceT(::MAQUETTE::Floor(a_px.m_val / a_px_step + s_Tol) *
             a_px_step);
    }

    // Rounding Up:
    // Similarly to the above, if "m_val" is an exact multiple of "a_px_step",
    // it may still be improperly rounded up by Ceil due to finite accuracy of
    // division  -- so move the Ceil arg down a bit:
    void RoundUp  (double a_px_step)
    {
      assert(a_px_step > s_Tol);
      m_val = ::MAQUETTE::Ceil (m_val / a_px_step - s_Tol) * a_px_step;
    }

    friend PriceT RoundUp  (PriceT a_px, double a_px_step)
    {
      assert(a_px_step > PriceT::s_Tol);
      return PriceT(::MAQUETTE::Ceil (a_px.m_val / a_px_step - s_Tol) *
            a_px_step);
    }

    // Rounding to the nearest multiple of "a_px_step":
    //
    void Round    (double a_px_step)
    {
      assert(a_px_step > s_Tol);
      m_val = ::MAQUETTE::Round(m_val / a_px_step) * a_px_step;
    }

    friend PriceT Round(PriceT a_px, double a_px_step)
    {
      assert(a_px_step > s_Tol);
      return PriceT(::MAQUETTE::Round(a_px.m_val / a_px_step) * a_px_step);
    }
  };

  //-------------------------------------------------------------------------//
  // Output:                                                                 //
  //-------------------------------------------------------------------------//
  inline std::ostream& operator<< (std::ostream& a_os, PriceT a_price)
    { return (a_os << double(a_price)); }

  //-------------------------------------------------------------------------//
  // Averaging:                                                              //
  //-------------------------------------------------------------------------//
  inline PriceT ArithmMidPx(PriceT a_left, PriceT a_right)
    { return PriceT(0.5 * (double(a_left) + double(a_right))); }

  inline PriceT GeomMidPx  (PriceT a_left, PriceT a_right)
    { return PriceT(::MAQUETTE::SqRt(double(a_left) * double(a_right))); }

  //=========================================================================//
  // "QtyTypeT": How Order and MktData Qtys can be Specified (ie Units):     //
  //=========================================================================//
  enum class QtyTypeT: int
  {
    // For the Special Qty vals (0, +-oo, Invalid: see below), it does not mat-
    // ter in which units that val is expressed -- the semantics  remains  the
    // same.
    // Also, any Qty<QT,QR> can be converted into Qty<QtyTypeT::UNDEFINED,QR>:
    UNDEFINED  = 0,

    // Normally, the Qty is given as a number of Contracts (Instruments). A Con-
    // tract may correspond to a fixed number of A units (eg 1) or, rarely, to
    // a fixed number of B units:
    Contracts  = 1,

    // Contracts can come in Lots (Batches), so the #Lots can be specified in-
    // stead of #Contracts. NB: This is a rarely-used option. The difference
    // between Contracts and Lots is that Px is always quoted for 1 Contract.
    // Currently the only known example of Lots is MICEX FX MDC. Cf:
    //
    // MICEX FX OMC: Contract=1 USD/RUB (so Contract=QtyA), for both Px and Qty
    // MICEX FX MDC: Px is still  for 1 USD/RUB, but Qty in Lots (of 1000 USD)
    // FORTS       : Contract=Fut for 1000 USD/RUB,  Px for Contract, Qty also
    //               in Contracts:
    Lots       = 2,

    // Each contract is a pair A/B of Assets (possibly with some multipliers),
    // so QtyA or QtyB can be specified instead of the above. Because QtyA and
    // QtyB are linked by the Trade Px, only one of them can be specified (the
    // one which is proportional to Contracts / Lots as supported by the corr-
    // esp Exchange, normally QtyA):
    QtyA       = 3,
    QtyB       = 4
  };

  //=========================================================================//
  // "Qty":                                                                  //
  //=========================================================================//
  // Qty is parameterised by:
  // (*) its "QtyTypeT"   (so we cannot add up Contracts and Lots!);
  // (*) its "QR" Representation Type, currently "long" or "double";
  // (*) because this type is templated, we name is just as "Qty", not "QtyT":
  //
  // In the past, Qtys used to be Integral; but nowadays, esp in Crypto-Assets,
  // they can often be Fractional as well. Thus, the following type is a  union
  // of Long and Double (to guarantee a uniform underlying representation), and
  // is also parameterised by the actual underlying type  (that Long or Double)
  // for type safety:
  //
  // "Qty" is used in MDC and OMC internals where the QT and QR params are ult-
  // imately determined by underlying Protocols:
  //
  template<QtyTypeT QT, typename QR>
  class Qty;

  template<QtyTypeT QT> class Qty<QT, long>;
  template<QtyTypeT QT> class Qty<QT, double>;

  union  QtyU;
  struct QtyAny;  // This is a "tagged" version of "QtyU"

  //=========================================================================//
  // "IsValidQtyRep":                                                        //
  //=========================================================================//
  // Verifies that the Static and Dynamic QT and QR match.
  // XXX:
  // (*) By convenstion, if the template "QT" param is UNDEFINED, then we accept
  //     any "a_qt" (this is similar to convering any typed ptr to void ptr);
  // (*) We also accept UNDEFINED target for any src, but not other way round
  //     (similar to void*):
  //
  template<QtyTypeT QT, typename QR>
  inline bool IsValidQtyRep(QtyTypeT a_qt, bool a_with_frac_qtys)
  {
    constexpr bool IsLong   = std::is_integral_v      <QR>;
    constexpr bool IsDouble = std::is_floating_point_v<QR>;
    static_assert (IsLong   ^ IsDouble);
    return (QT       == QtyTypeT::UNDEFINED || QT == a_qt) &&
           (IsDouble == a_with_frac_qtys);
  }

  //=========================================================================//
  // Representation of Special (Non-Finite) Vals:                            //
  //=========================================================================//
  // XXX: It is VERY IMPORTANT that Invalid Long is actually NEGATIVE, because
  // we (sometimes, somewhere) also use other negative vals (eg -1) to represent
  // invalid Qtys, and then test the vals for < 0, so both tests must be consis-
  // tent:
  template<typename T> T             PosInfQ;
  template<typename T> T             NegInfQ;
  template<typename T> T             InvalidQ;
  template<typename T> bool          IsInvalidQ(T);

  template<> constexpr inline long   NegInfQ   <long>   = -LONG_MAX;
  template<> constexpr inline long   PosInfQ   <long>   =   LONG_MAX;
  template<> constexpr inline long   InvalidQ  <long>   =  LONG_MIN;
  template<> constexpr inline bool   IsInvalidQ<long>(long a_v)
    { return a_v == InvalidQ<long>; }

  static_assert(NegInfQ   <long>  < 0 && PosInfQ<long> > 0 &&
                NegInfQ   <long>  ==   - PosInfQ<long>     &&
                InvalidQ  <long>  < 0 &&
                IsInvalidQ<long>(InvalidQ<long>));

  template<> constexpr inline double NegInfQ   <double> = - Inf<double>;
  template<> constexpr inline double PosInfQ   <double> =   Inf<double>;
  template<> constexpr inline double InvalidQ  <double> =   NaN<double>;
  template<> constexpr inline bool   IsInvalidQ<double>(double a_v)
    { return IsNaN(a_v); }

  static_assert(NegInfQ   <double> < 0 && PosInfQ<double> > 0 &&
                NegInfQ   <double> ==   - PosInfQ<double>     &&
                IsInvalidQ<double>(InvalidQ<double>));

  //=========================================================================//
  // Specialization: Qty<QT, long>:                                          //
  //=========================================================================//
  template <QtyTypeT QT>
  class Qty<QT,    long>
  {
  private:
    //-----------------------------------------------------------------------//
    // Data Flds:                                                            //
    //-----------------------------------------------------------------------//
    friend union  QtyU;
    friend struct QtyAny;

    long   m_v;

    // Tolerance for conversions from double: Using same value as for QR=double:
    constexpr static double s_Tol = 1e-9;

  public:
    //-----------------------------------------------------------------------//
    // Ctors:                                                                //
    //-----------------------------------------------------------------------//
    // XXX: Negative vals are allowed (as they may sometimes be required):
    constexpr          Qty():           m_v(0)     {}
    constexpr explicit Qty(long a_val): m_v(a_val) {}

    // NB: However, for safety, conversion from "double" to "long"  is only
    // allowed if the arg is actually integral (or special): No rounding is
    // performed:
    //
    constexpr explicit Qty(double a_val)
    {
      // Here we must check for special args first:
      double whole = Round(a_val);
      m_v =
        utxx::unlikely(a_val == NegInfQ <double>)
        ? NegInfQ <long> :
        utxx::unlikely(a_val == PosInfQ <double>)
        ? PosInfQ <long> :
        utxx::unlikely(a_val == InvalidQ<double>)
        ? InvalidQ<long> :
        utxx::likely(Abs(whole - a_val) <  s_Tol)
        ? long(whole)    // Generic case, incl Zero:
        : throw utxx::badarg_error
                ("Qty<long>::Ctor: Not an Integral arg: ", a_val);
    }
    //-----------------------------------------------------------------------//
    // Special Vals and Properties:                                          //
    //-----------------------------------------------------------------------//
    // NB: Special vals (incl 0) are assumed  to share  same semantics across
    // different "QT" params. Eg, Zero Qty is 0 irrespective to whether it is
    // expressed in Contracts, Lots etc:
    //
    constexpr static Qty Zero   ()       { return Qty();                    }
    constexpr static Qty NegInf ()       { return Qty(NegInfQ <long>);      }
    constexpr static Qty PosInf ()       { return Qty(PosInfQ <long>);      }
    constexpr static Qty Invalid()       { return Qty(InvalidQ<long>);      }

    friend    bool   IsZero    (Qty a_v) { return a_v.m_v == 0;             }
    friend    bool   IsNeg     (Qty a_v) { return a_v.m_v  < 0;             }
    friend    bool   IsPos     (Qty a_v) { return a_v.m_v  > 0;             }
    friend    bool   IsNegInf  (Qty a_v) { return a_v.m_v == NegInfQ<long>; }
    friend    bool   IsPosInf  (Qty a_v) { return a_v.m_v == PosInfQ<long>; }
    friend    bool   IsInvalid (Qty a_v) { return IsInvalidQ(a_v.m_v);      }

    // Check for Special and/or or 0 value:
    friend    bool   IsSpecial0(Qty a_v)
      { return IsZero   (a_v)  || IsPosInf(a_v) || IsNegInf(a_v) ||
               IsInvalid(a_v); }

    friend    bool   IsFinite  (Qty a_v)
      { return !(IsPosInf(a_v) || IsNegInf(a_v) || IsInvalid(a_v)); }

    //-----------------------------------------------------------------------//
    // Accessors:                                                            //
    //-----------------------------------------------------------------------//
    constexpr explicit operator long()     const { return        m_v;  }
    constexpr explicit operator double()   const { return double(m_v); }

    //-----------------------------------------------------------------------//
    // Arithmetic:                                                           //
    //-----------------------------------------------------------------------//
    // XXX: We do NOT check for special vals in arithmetic ops. The Caller is
    // responsible for providing valid args, for the sake of efficiency. Also,
    // arithmetic operations could be made "constexpr", but there is currently
    // no need for that:
    //
    Qty  operator+  ()               const { return     *this; }
    Qty  operator-  ()               const { return Qty(-m_v); }

    Qty  operator+  (Qty    a_right) const { return Qty(m_v + a_right.m_v); }
    Qty  operator-  (Qty    a_right) const { return Qty(m_v - a_right.m_v); }

    // Multiplication by "long":
    Qty  operator*  (long   a_right) const { return Qty(m_v * a_right); }

    // Integral Division (XXX: no zero-denom check is performed -- a hardware
    // error would occur in that case):
    Qty  operator/  (long   a_right) const { return Qty(m_v / a_right); }
    Qty  operator%  (long   a_right) const { return Qty(m_v % a_right); }

    // It is also possible to obtain a ratio of 2 Qtys, as a "double":
    friend double operator/ (Qty a_left,   Qty a_right)
      { return double(a_left.m_v) / double(a_right.m_v); }

    // XXX: Multiplication and division by a "double" is also possible, with
    // rounding of results. Again, no zero-denom checks are performed:
    Qty  operator*  (double a_right) const
      { return Qty(long(Round(double(m_v) * a_right))); }

    Qty  operator/  (double a_right) const
      { return Qty(long(Round(double(m_v) / a_right))); }

    // Abs/Min/Max:
    friend Qty Abs(Qty  a_right) { return Qty(std::abs(a_right.m_v)); }

    friend Qty Min(Qty  a_left, Qty a_right)
      { return Qty(std::min<long>(a_left.m_v, a_right.m_v)); }

    friend Qty Max(Qty  a_left, Qty a_right)
      { return Qty(std::max<long>(a_left.m_v, a_right.m_v)); }

    // Updating Arithmetic:
    Qty& operator+= (Qty    a_right) { m_v += a_right.m_v; return *this; }
    Qty& operator-= (Qty    a_right) { m_v -= a_right.m_v; return *this; }
    Qty& operator*= (long   a_right) { m_v *= a_right;     return *this; }
    Qty& operator/= (long   a_right) { m_v /= a_right;     return *this; }

    Qty& MinWith    (Qty    a_right)
      { m_v =  std::min<long>(m_v, a_right.m_v);  return *this; }
    Qty& MaxWith    (Qty    a_right)
      { m_v =  std::max<long>(m_v, a_right.m_v);  return *this; }

    // XXX: Similar to above, updating multiplication and division by "double".
    // Again, no zero-denom checks:
    Qty& operator*= (double a_right)
      { m_v = long(Round(double(m_v) * a_right)); return *this; }

    Qty& operator/= (double a_right)
      { m_v = long(Round(double(m_v) / a_right)); return *this; }

    //-----------------------------------------------------------------------//
    // Comparisons:                                                          //
    //-----------------------------------------------------------------------//
    // Between Qtys:
    bool operator== (Qty  a_right) const { return m_v == a_right.m_v; }
    bool operator!= (Qty  a_right) const { return m_v != a_right.m_v; }
    bool operator<  (Qty  a_right) const { return m_v <  a_right.m_v; }
    bool operator<= (Qty  a_right) const { return m_v <= a_right.m_v; }
    bool operator>  (Qty  a_right) const { return m_v >  a_right.m_v; }
    bool operator>= (Qty  a_right) const { return m_v >= a_right.m_v; }

    // Between a Qty and a Long (XXX: This is primarily for easy cmp with 0):
    bool operator== (long a_right) const { return m_v == a_right;     }
    bool operator!= (long a_right) const { return m_v != a_right;     }
    bool operator<  (long a_right) const { return m_v <  a_right;     }
    bool operator<= (long a_right) const { return m_v <= a_right;     }
    bool operator>  (long a_right) const { return m_v >  a_right;     }
    bool operator>= (long a_right) const { return m_v >= a_right;     }

    //-----------------------------------------------------------------------//
    // Casts: Same QR (long) but different QTs:                              //
    //-----------------------------------------------------------------------//
    // There is only 1 valid case currently:
    // Qty<QT, long> can be cast into Qty<UNDEFINED, long>, but not other way
    // round (this is similar to conversion of void and typed ptrs).
    // Any other conversions require scaling factors from "SecDef"s...
    //
    constexpr explicit operator Qty<QtyTypeT::UNDEFINED, long>() const
      { return Qty<QtyTypeT::UNDEFINED, long>(m_v); }

    //-----------------------------------------------------------------------//
    // Conversions to QR=double (TargQT=QT|UNDEFINED):                       //
    //-----------------------------------------------------------------------//
    template<QtyTypeT TargQT>
    explicit operator Qty<TargQT, double>() const;
  };

  //=========================================================================//
  // Specialization: Qty<QT, double>:                                        //
  //=========================================================================//
  template <QtyTypeT QT>
  class Qty<QT,  double>
  {
  private:
    //-----------------------------------------------------------------------//
    // Data Flds:                                                            //
    //-----------------------------------------------------------------------//
    friend union  QtyU;
    friend struct QtyAny;

    double m_v;

    // Tolerance for 0-comparisons:
    // XXX: Kraken uses 8 digits for quantities, so this needs to be less than
    // 1e-8:
    constexpr static double s_Tol = 1e-9;

  public:
    //-----------------------------------------------------------------------//
    // Ctors:                                                                //
    //-----------------------------------------------------------------------//
    // XXX: Negative vals are allowed (as they may sometimes be required):
    constexpr Qty():   m_v(0.0) {}

    constexpr explicit Qty(double a_val)
      // NB: Because Sign-Tests are not "s_Tol"-enabled (see below), we must do
      // that it Ctor and in all arithmetic operations:
      // XXX: GCC does not like "constexpr"s with NaN args,   so do an explicit
      // test first:
    : m_v(IsNaN(a_val) ? NaN<double> : (Abs(a_val) < s_Tol) ? 0.0 : a_val) {}

    constexpr explicit Qty(long   a_val)
    {
      // Here we must check for special args first:
      m_v =
        utxx::unlikely(a_val == NegInfQ <long>)
        ? NegInfQ <double> :
        utxx::unlikely(a_val == PosInfQ <long>)
        ? PosInfQ <double> :
        utxx::unlikely(a_val == InvalidQ<long>)
        ? InvalidQ<double>
        : double(a_val);  // Generic case, incl Zero: Always succeeds!
    }

    //-----------------------------------------------------------------------//
    // Special Vals:                                                         //
    //-----------------------------------------------------------------------//
    // NB: Special vals (incl 0) are assumed  to share  same semantics across
    // different "QT" params. Eg, Zero Qty is 0 irrespective to whether it is
    // expressed in Contracts, Lots etc:
    //
    constexpr static Qty Zero   ()       { return Qty();                  }
    constexpr static Qty PosInf ()       { return Qty(PosInfQ <double>);  }
    constexpr static Qty NegInf ()       { return Qty(NegInfQ <double>);  }
    constexpr static Qty Invalid()       { return Qty(InvalidQ<double>);  }

    // XXX: The following Sign Tests do not use "s_Tol", as otherwise they would
    // be inconsistent with similar tests for QR=long, and it would not be poss-
    // ible to use uniform Rep-independent sign tests for "QtyU" union (see bel-
    // ow):
    friend    bool   IsZero    (Qty a_v) { return a_v.m_v == 0; }
    friend    bool   IsNeg     (Qty a_v) { return a_v.m_v <  0; }
    friend    bool   IsPos     (Qty a_v) { return a_v.m_v >  0; }

    // Non-Finiteness Tests:
    friend    bool   IsNegInf  (Qty a_v) { return a_v.m_v == NegInfQ<double>; }
    friend    bool   IsPosInf  (Qty a_v) { return a_v.m_v == PosInfQ<double>; }
    friend    bool   IsInvalid (Qty a_v) { return IsInvalidQ(a_v.m_v);        }

    // Check for Special and/or or 0 value:
    friend    bool   IsSpecial0(Qty a_v)
      { return IsZero   (a_v) || IsPosInf(a_v) || IsNegInf(a_v) ||
               IsInvalid(a_v); }

    friend    bool   IsFinite  (Qty a_v) { return IsFinite(a_v.m_v); }

    //-----------------------------------------------------------------------//
    // Accessors:                                                            //
    //-----------------------------------------------------------------------//
    // XXX: For safety, conversion to "long" only works when the val is within
    // "s_Tol" from integral: No "big" rounding is performed:
    explicit operator long() const
    {
      double  whole = Round(m_v);
      if (utxx::likely(Abs(whole - m_v) < s_Tol))
        return long(whole);
      else
        throw utxx::badarg_error
              ("Qty<double>::operator long: Not an Integral val: ", m_v);
    }
    constexpr explicit operator double() const { return m_v; }

    //-----------------------------------------------------------------------//
    // Arithmetic:                                                           //
    //-----------------------------------------------------------------------//
    // NB: Arithmetic operations could in theory also be made "constexpr",  but
    // there is currenly no need for that. XXX: Once again, because  sign tests
    // are not "s_Tol"-enabled, we must adjust the results of arithmetic ops --
    // this is typically done by the Ctor:
    Qty  operator+  ()               const { return     *this; }
    Qty  operator-  ()               const { return Qty(-m_v); }

    Qty  operator+  (Qty    a_right) const { return Qty(m_v + a_right.m_v); }
    Qty  operator-  (Qty    a_right) const { return Qty(m_v - a_right.m_v); }

    // Multiplication is by Double and Long:
    Qty  operator*  (double a_right) const { return Qty(m_v * a_right); }
    Qty  operator*  (long   a_right) const
      { return Qty(m_v * double(a_right)); }

    // Division (XXX: no zero-denom check is performed -- Infinity res is OK):
    Qty  operator/  (double a_right) const { return Qty(m_v / a_right); }
    Qty  operator/  (long   a_right) const
      { return Qty(m_v / double(a_right)); }

    // It is also possible to obtain a ratio of 2 Qtys, as a "double". XXX: In
    // this case, the result is a "double", not Qty, so we do NOT perform adj-
    // ustment to 0:
    friend double operator/ (Qty a_left, Qty a_right)
      { return a_left.m_v / a_right.m_v;  }

    // NB: We support froating-point remainder:
    Qty  operator%  (double a_right) const
      { return Qty(remainder(m_v, a_right)); }

    // Abs/Min/Max:
    friend Qty Abs(Qty a_right) { return Qty(fabs(a_right.m_v)); }

    friend Qty Min(Qty a_left,  Qty a_right)
      { return Qty(std::min<double>(a_left.m_v, a_right.m_v)); }

    friend Qty Max(Qty a_left,  Qty a_right)
      { return Qty(std::max<double>(a_left.m_v, a_right.m_v)); }

    // Updating Arithmetic ops; here adjustments to 0 must be explicitly perf-
    // ormed in most cases:
    Qty& operator+= (Qty    a_right)
    {
      m_v += a_right.m_v;
      if (utxx::unlikely(Abs(m_v) < s_Tol))
        m_v = 0.0;
      return *this;
    }

    Qty& operator-= (Qty    a_right)
    {
      m_v -= a_right.m_v;
      if (utxx::unlikely(Abs(m_v) < s_Tol))
        m_v = 0.0;
      return *this;
    }

    Qty& operator*= (double a_right)
    {
      m_v *= a_right;
      if (utxx::unlikely(Abs(m_v) < s_Tol))
        m_v = 0.0;
      return *this;
    }

    Qty& operator*= (long   a_right) { m_v *= double(a_right); }
    // In this particular case, no adjustment is necessary...

    Qty& operator/= (double a_right)
    {
      m_v /= a_right;
      if (utxx::unlikely(Abs(m_v) < s_Tol))
        m_v = 0.0;
      return *this;
    }

    Qty& operator/= (long   a_right)
    {
      m_v /= double(a_right);
      if (utxx::unlikely(Abs(m_v) < s_Tol))
        m_v = 0.0;
      return *this;
    }

    // Here no need for adjustment, either:
    Qty& MinWith    (Qty    a_right)
      { m_v = std::min<double>(m_v, a_right.m_v); return *this; }

    Qty& MaxWith    (Qty    a_right)
      { m_v = std::max<double>(m_v, a_right.m_v); return *this; }

    //-----------------------------------------------------------------------//
    // Comparisons (with Tolerance!!!):                                      //
    //-----------------------------------------------------------------------//
    // Between Qtys:
    bool operator== (Qty a_right) const
      { return Abs(m_v - a_right.m_v) <  s_Tol; }

    bool operator!= (Qty a_right) const
      { return Abs(m_v - a_right.m_v) >= s_Tol; }

    bool operator<  (Qty a_right) const
      { return (m_v - a_right.m_v) <   - s_Tol; }

    bool operator>= (Qty a_right) const
      { return (m_v - a_right.m_v) >=  - s_Tol; }

    bool operator<= (Qty a_right) const
      { return (m_v - a_right.m_v) <=    s_Tol; }

    bool operator>  (Qty a_right) const
      { return (m_v - a_right.m_v) >     s_Tol; }

    // Between a Qty and a Double (XXX: This is primarily for easy cmp with 0):
    bool operator== (double a_right) const
      { return Abs(m_v  - a_right) <     s_Tol; }

    bool operator!= (double a_right) const
      { return Abs(m_v  - a_right) >=    s_Tol; }

    bool operator<  (double a_right) const
      { return (m_v - a_right) <       - s_Tol; }

    bool operator>= (double a_right) const
      { return (m_v - a_right) >=      - s_Tol; }

    bool operator<= (double a_right) const
      { return (m_v - a_right) <=        s_Tol; }

    bool operator>  (double a_right) const
      { return (m_v - a_right) >         s_Tol; }

    //-----------------------------------------------------------------------//
    // Casts: Same QR (double) but different QTs:                            //
    //-----------------------------------------------------------------------//
    // There is only 1 valid case currently: Any Qty<QT, double> can be cast
    // into Qty<UNDEFINED, double>, but not other way round (this is similar
    // to conversion of void and typed ptrs).
    // Any other conversions require scaling factors from "SecDef"s...
    //
    constexpr explicit operator Qty<QtyTypeT::UNDEFINED, double>() const
      { return Qty<QtyTypeT::UNDEFINED, double>(m_v); }

    //-----------------------------------------------------------------------//
    // Conversions to QR=long (TargQT=QT|UNDEFINED):                         //
    //-----------------------------------------------------------------------//
    template<QtyTypeT TargQT>
    explicit operator Qty<TargQT, long>() const;
  };

  //=========================================================================//
  // Type Abbreviations:                                                     //
  //=========================================================================//
  // XXX: "Contracts" and "Lots" are inherently integral (whereas "QtyA" and
  // "QtyB" may or may not be),  so  using Double QR for them may be unneces-
  // sary and sub-optimal -- but we still allow that:
  //
  using QtyCL = Qty<QtyTypeT::Contracts,   long>;
  using QtyLL = Qty<QtyTypeT::Lots,        long>;
  using QtyAL = Qty<QtyTypeT::QtyA,        long>;
  using QtyBL = Qty<QtyTypeT::QtyB,        long>;
  using QtyCD = Qty<QtyTypeT::Contracts, double>;  // Unnecessary?
  using QtyLD = Qty<QtyTypeT::Lots,      double>;  // Unnecessary?
  using QtyAD = Qty<QtyTypeT::QtyA,      double>;
  using QtyBD = Qty<QtyTypeT::QtyB,      double>;

  // XXX: In addition, QtyType::UNDEFINED can be used for holding Special vals,
  // or for building the QtyU and QtyAny types:
  //
  using QtyUL = Qty<QtyTypeT::UNDEFINED, long>;
  using QtyUD = Qty<QtyTypeT::UNDEFINED, double>;

  //=========================================================================//
  // "QtyU": UnDiscriminated Union of Qty<*,long|double> Types:              //
  //=========================================================================//
  // Primarily intended for "monomorphic" (non-templated) data types  such as
  // AOS, Req12 and Trade, so some type erasure / casts / dynamic conversions
  // become necessary.  XXX: Making those types polymorphic / templated would
  // significantly complicate  the Strategies API  because they are currently
  // passed to Strategies!
  //
  struct SecDefS; // Required for conversions, defined elsewhere

  union QtyU
  {
  private:
    //-----------------------------------------------------------------------//
    // Data Flds:                                                            //
    //-----------------------------------------------------------------------//
    QtyUL   m_UL;
    QtyUD   m_UD;

    friend struct QtyAny;

  public:
    //-----------------------------------------------------------------------//
    // Ctors:                                                                //
    //-----------------------------------------------------------------------//
    // Default Ctor, same as "Zero":
    constexpr QtyU():      m_UL()   {}       // 0 for both Long & Double!
    constexpr static QtyU  Zero()   { return QtyU();   }

    // Comparison with 0 is simple, because both L and D zeros are represented
    // identically (as all-0 bits):
    friend bool  IsZero(QtyU a_qty) { return IsZero(a_qty.m_UL); }

    // Also, for both L and D vals, negative vals correspond to bit 7 of the
    // lowest-address byte set (XXX: only in Low-Endian notation), so it  is
    // enough to test the "UL":
    friend bool  IsPos (QtyU a_qty) { return IsPos(a_qty.m_UL); }
    friend bool  IsNeg (QtyU a_qty) { return IsNeg(a_qty.m_UL); }

    // From Long and Double:
    constexpr explicit QtyU(long   a_val): m_UL(a_val) {}
    constexpr explicit QtyU(double a_val): m_UD(a_val) {}

    // From Qty<*,*>:
    template<QtyTypeT  QT>
    constexpr explicit QtyU(Qty<QT,long>   a_qty):  m_UL(a_qty) {}

    template<QtyTypeT  QT>
    constexpr explicit QtyU(Qty<QT,double> a_qty):  m_UD(a_qty) {}

    //-----------------------------------------------------------------------//
    // Access to Flds:                                                       //
    //-----------------------------------------------------------------------//
    // (Mostly for use in "QtyAny" below). The access in UnChecked -- the Caller
    // must known which fld actually contains a valid value:
    constexpr QtyUL GetUL() const  { return m_UL; }
    constexpr QtyUD GetUD() const  { return m_UD; }

    //-----------------------------------------------------------------------//
    // Dynamically-Checked Cast into Qty<*,*>:                               //
    //-----------------------------------------------------------------------//
    template<QtyTypeT QT, typename QR, bool ConvReps = false>
    Qty<QT,QR> GetQty
    (
      QtyTypeT DEBUG_ONLY(a_qt),
      bool     a_is_frac
    )
    const
    {
      // Target properties:
      constexpr bool IsLong   = std::is_integral_v      <QR>;
      constexpr bool IsDouble = std::is_floating_point_v<QR>;
      static_assert( IsLong   ^ IsDouble );

      // NB: The src and target semantics must always match, unless the target
      // is UNDEFINED. However, we check this only in the DEBUG modeL
      assert(QT == QtyTypeT::UNDEFINED || QT == a_qt);

      // However, conversion of Reps is possible, if specifically requested (it
      // is off by default, to avoid hidden inefficient conversions):
      if constexpr(ConvReps)
      {
        return
          utxx::likely(IsDouble == a_is_frac)
          ? // Reps match, so perform a cast:
            this->operator Qty<QT,QR>()
          : // Perform a conversion:
          IsDouble
          ? // Src is Long,   Targ is Double:
            Qty<QT,QR>(m_UL.m_v)
          : // Src is Double, Targ is Long:
            Qty<QT,QR>(m_UD.m_v);
      }
      else
      {
        // The Reps must match exactly:
        assert(IsDouble == a_is_frac);

        // Perform the cast (using a private cast operator):
        return this->operator Qty<QT,QR>();
      }
      __builtin_unreachable();
    }

    //-----------------------------------------------------------------------//
    // Dynamic Conversion (not a Cast) into Typed Qty<QT,QR>:                //
    //-----------------------------------------------------------------------//
    template<QtyTypeT QT, typename QR>
    Qty<QT,QR> ConvQty
    (
      QtyTypeT        a_qt,
      bool            a_is_frac,
      SecDefS const&  a_instr,
      PriceT          a_instr_px
    );

  private:
    //-----------------------------------------------------------------------//
    // UnChecked Static Casts into "Qty<*,*>":                               //
    //-----------------------------------------------------------------------//
    // NB: Only used internally:
    //
    template<QtyTypeT QT>
    constexpr explicit operator Qty<QT,long>   () const
      { return Qty<QT, long>  (long  (m_UL)); }

    template<QtyTypeT QT>
    constexpr explicit operator Qty<QT,double> () const
      { return Qty<QT, double>(double(m_UD)); }

    // NB: There is no "operator==" for "QtyU", the reason is that we could on-
    // ly do bitwise comparison in this case, which can lead to false positives
    // (if the actual reps are different, but appear to be same bitwise),   and
    // also to false negatives (eg if the actual reps "double" -- but we do not
    // use any Tolerance then)...
  };

  //=========================================================================//
  // "QtyAny": Discriminated Union:                                          //
  //=========================================================================//
  // For top-level interfaces only (XXX: using "QtyAny" internally would be qu-
  // ite inefficient). Contains tags for both QtyTypeT and QR.   Normally used
  // in OMC API methods where the Caller can provide Qtys  of a type different
  // from the internal representation of that OMC:
  //
  struct QtyAny
  {
    //-----------------------------------------------------------------------//
    // Data Flds:                                                            //
    //-----------------------------------------------------------------------//
    QtyU      m_val;     // Unified    Rep
    QtyTypeT  m_qt;      // Semantical Tag
    bool      m_isFrac;  // Determines the "long" or "double" rep

  public:
    //-----------------------------------------------------------------------//
    // Default Ctor:                                                         //
    //-----------------------------------------------------------------------//
    constexpr QtyAny()
    : m_val   (),                      // Zero  val in both Long and Double
      m_qt    (QtyTypeT::UNDEFINED),   // UNDEFINED is compatible with Zero
      m_isFrac(false)                  // Assume non-fractional (long)
    {}

    //-----------------------------------------------------------------------//
    // Ctor from Long / Double:                                              //
    //-----------------------------------------------------------------------//
    // NB: For the QtyTypeT, most commonly-used defaults vals are provided:
    //
    constexpr explicit  QtyAny(long   a_val, QtyTypeT a_qt = QtyTypeT::QtyA)
    : m_val   (a_val),
      m_qt    (a_qt),
      m_isFrac(false)
    {
      // Here QT==UNDEFINED is only allowed for Special vals:
      CHECK_ONLY
      (
        if (m_qt == QtyTypeT::UNDEFINED && !IsSpecial0(m_val.GetUL()))
          throw utxx::badarg_error("QrtAny(long): Invalid QT=UNDEFINED");
      )
    }

    constexpr explicit  QtyAny(double a_val, QtyTypeT a_qt = QtyTypeT::QtyA)
    : m_val   (a_val),
      m_qt    (a_qt),
      m_isFrac(true)
    {
      // Here we should check "a_qt" for being valid for the "double" rep.
      // And again, QT==UNDEFINED is only allowed for Special vals:
      CHECK_ONLY
      (
        if ((m_qt == QtyTypeT::UNDEFINED && !IsSpecial0(m_val.GetUD())) ||
            (m_qt != QtyTypeT::QtyA      &&  m_qt != QtyTypeT::QtyB))
          throw utxx::badarg_error("QtyAny(double): Invalid QT=", int(a_qt));
      )
    }

    //-----------------------------------------------------------------------//
    // Ctor from Qty Instances:                                              //
    //-----------------------------------------------------------------------//
    template <QtyTypeT QT, typename QR>
    constexpr explicit QtyAny(Qty<QT,QR> a_qty)
    : m_val   (QR(a_qty)),
      m_qt    (QT),
      m_isFrac(std::is_floating_point_v<QR>)
    {}

    //------------------------------------------------------------------------//
    // Special Vals:                                                          //
    //------------------------------------------------------------------------//
    // XXX: For the moment, we select Double reps of special Vals, although this
    // does not really matter:
    constexpr static QtyAny Zero    ()        { return QtyAny ();  }

    constexpr static QtyAny PosInf  ()
      { return QtyAny(Qty<QtyTypeT::UNDEFINED,double>::PosInf ()); }

    constexpr static QtyAny NegInf ()
      { return QtyAny(Qty<QtyTypeT::UNDEFINED,double>::NegInf ()); }

    constexpr static QtyAny Invalid()
      { return QtyAny(Qty<QtyTypeT::UNDEFINED,double>::Invalid()); }

    //-----------------------------------------------------------------------//
    // Properties:                                                           //
    //-----------------------------------------------------------------------//
    friend QtyTypeT GetQT    (QtyAny a_v) { return a_v.m_qt;      }
    friend bool     IsFrac   (QtyAny a_v) { return a_v.m_isFrac;  }

    // The following tests are delegated directly to "QtyU":
    friend bool     IsZero   (QtyAny a_v) { return IsZero   (a_v.m_val); }
    friend bool     IsPos    (QtyAny a_v) { return IsPos    (a_v.m_val); }
    friend bool     IsNeg    (QtyAny a_v) { return IsNeg    (a_v.m_val); }

    // But for the following ones, we need to use the repr tag:
    friend bool     IsPosInf (QtyAny a_v)
    { return a_v.m_isFrac
             ? IsPosInf (a_v.m_val.GetUD())
             : IsPosInf (a_v.m_val.GetUL()); }

    friend bool     IsNegInf (QtyAny a_v)
    { return a_v.m_isFrac
             ? IsNegInf (a_v.m_val.GetUD())
             : IsNegInf (a_v.m_val.GetUL()); }

    friend bool     IsInvalid(QtyAny a_v)
    { return a_v.m_isFrac
             ? IsInvalid(a_v.m_val.GetUD())
             : IsInvalid(a_v.m_val.GetUL()); }

    //-----------------------------------------------------------------------//
    // Semi-Structural Equality:                                             //
    //-----------------------------------------------------------------------//
    // XXX: It requires that the QtyTypes and Reps are same, and then compares
    // the vals according to the Long or Double semantics,   the latter with a
    // Tolerance.
    bool operator==(QtyAny const& a_right) const
    {
      return
        (m_qt != a_right.m_qt || m_isFrac != a_right.m_isFrac)
        ? false                                // Incompatible -> not equal
        :
        m_isFrac
        ? (m_val.m_UD == a_right.m_val.m_UD)   // Tolerance applied here
        : (m_val.m_UL == a_right.m_val.m_UL);  // Exact
    }

    //-----------------------------------------------------------------------//
    // (Checked) Conversions to Long and Double:                             //
    //-----------------------------------------------------------------------//
    // NB: Conversion to "long" throws an exception if the value stored is act-
    // ually fractional, and cannot be converted into an integer within the to-
    // lerance limits:
    explicit operator long  () const
      { return m_isFrac ? long  (m_val.GetUD()) : long  (m_val.GetUL()); }

    explicit operator double() const
      { return m_isFrac ? double(m_val.GetUD()) : double(m_val.GetUL()); }

    //-----------------------------------------------------------------------//
    // Conversion into Qty<QT,QR>:                                           //
    //-----------------------------------------------------------------------//
    // In general, requires a "SecDefS" and "InstrPx".
    // Implemented in "Basis/QtyConvs.hpp", not here:
    //
    template<QtyTypeT QT, typename QR>
    Qty<QT,QR> ConvQty
    (
      SecDefS const& a_instr,
      PriceT         a_instr_px
    );
  };
}
// End namespace MAQUETTE
