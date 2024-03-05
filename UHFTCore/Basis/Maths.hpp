// vim:ts=2:et
//===========================================================================//
//                            "Common/Maths.hpp":                            //
//  Portable Mathematical Functions (Including Templated and CUDA Versions)  //
//===========================================================================//
#pragma  once

#include <cstdlib>
#include <cfloat>
#include <climits>
#include <cassert>

// In CUDA compilation mode, include CUDA-specific Math header:
#ifdef   __CUDACC__
#include <math_functions.h>
#include <math_constants.h>
#define  DEVICE __device__
#define  GLOBAL __global__
#else    // !__CUDACC__
#include <cmath>
#include <limits>
#include <string>
// NB: Unlike other components, here we use "std::" exceptions rather than
// "utxx::" ones:
#include <stdexcept>
#define  DEVICE
#define  GLOBAL
#endif   // __CUDACC__

namespace MAQUETTE
{
  //=========================================================================//
  // Constants:                                                              //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "Eps":                                                                  //
  //-------------------------------------------------------------------------//
  template<typename F> F Eps;
  template<> constexpr inline float  Eps<float>  = FLT_EPSILON;
  template<> constexpr inline double Eps<double> = DBL_EPSILON;

  //-------------------------------------------------------------------------//
  // "NaN":                                                                  //
  //-------------------------------------------------------------------------//
# ifdef __CUDACC__
  template<typename F> F NaN;
  template<> constexpr inline float  NaN<float>  = CUDART_NAN_F;
  template<> constexpr inline double NaN<double> = CUDART_NAN;
# else
  template<typename F>
  constexpr inline  F    NaN = std::numeric_limits<F>::quiet_NaN();
# endif

  //-------------------------------------------------------------------------//
  // "Inf":                                                                  //
  //-------------------------------------------------------------------------//
# ifdef __CUDACC__
  template<typename F> F Inf;
  template<> constexpr inline float  Inf<float>  = CUDART_INF_F;
  template<> constexpr inline double Inf<double> = CUDART_INF;
# else
  template<typename F>
  constexpr inline  F    Inf = std::numeric_limits<F>::infinity();
# endif

  //-------------------------------------------------------------------------//
  // Algebraic Functions:                                                    //
  //-------------------------------------------------------------------------//
  // Generic "Min", "Max":
  template<typename F>
  DEVICE constexpr inline F Min(F x, F y)  { return (x < y) ? x : y; }

  template<typename F>
  DEVICE constexpr inline F Max(F x, F y)  { return (x > y) ? x : y; }

  // Generic Square:
  template<typename F>
  DEVICE constexpr inline F Sqr(F x)       { return x * x; }

  // Real Absolute Value:
# ifdef __CUDACC__
  DEVICE    inline float  Abs  (float  x)  { return      fabsf(x); }
  DEVICE    inline double Abs  (double x)  { return      fabs (x); }
# else
  constexpr inline float  Abs  (float  x)  { return std::abs  (x); }
  constexpr inline double Abs  (double x)  { return std::abs  (x); }
# endif

  // Square Root:
# ifdef __CUDACC__
  DEVICE    inline float  SqRt (float  x)  { return      sqrtf(x); }
  DEVICE    inline double SqRt (double x)  { return      sqrt (x); }
# else
  constexpr inline float  SqRt (float  x)  { return std::sqrt (x); }
  constexpr inline double SqRt (double x)  { return std::sqrt (x); }
# endif

  // Power:
# ifdef __CUDACC__
  DEVICE    inline float  Pow  (float  x, float  y) { return      powf(x, y); }
  DEVICE    inline double Pow  (double x, double y) { return      pow (x, y); }
# else
  constexpr inline float  Pow  (float  x, float  y) { return std::pow (x, y); }
  constexpr inline double Pow  (double x, double y) { return std::pow (x, y); }
# endif

  // Cubic Root:
# ifdef __CUDACC__
  DEVICE    inline float  CbRt (float  x)  { return      cbrtf(x);  }
  DEVICE    inline double CbRt (double x)  { return      cbrt (x);  }
# else
  constexpr inline float  CbRt (float  x)  { return std::cbrt (x);  }
  constexpr inline double CbRt (double x)  { return std::cbrt (x);  }
# endif

  // Exp:
# ifdef __CUDACC__
  DEVICE    inline float  Exp  (float  x)  { return      expf (x);  }
  DEVICE    inline double Exp  (double x)  { return      exp  (x);  }
# else
  constexpr inline float  Exp  (float  x)  { return std::exp  (x);  }
  constexpr inline double Exp  (double x)  { return std::exp  (x);  }
# endif

  // Log:
# ifdef __CUDACC__
  DEVICE    inline float  Log  (float  x)  { return      logf (x);  }
  DEVICE    inline double Log  (double x)  { return      log  (x);  }
# else
  constexpr inline float  Log  (float  x)  { return std::log  (x);  }
  constexpr inline double Log  (double x)  { return std::log  (x);  }
# endif

  // Log10:
# ifdef __CUDACC__
  DEVICE    inline float  Log10(float  x)  { return      log10f(x); }
  DEVICE    inline double Log10(double x)  { return      log10 (x); }
# else
  constexpr inline float  Log10(float  x)  { return std::log10 (x); }
  constexpr inline double Log10(double x)  { return std::log10 (x); }
# endif

  // Log(1+x):
# ifdef __CUDACC__
  DEVICE    inline float  Log1P(float  x)  { return      log1pf(x); }
  DEVICE    inline double Log1P(double x)  { return      log1p (x); }
# else
  constexpr inline float  Log1P(float  x)  { return std::log1p (x); }
  constexpr inline double Log1P(double x)  { return std::log1p (x); }
# endif

  //-------------------------------------------------------------------------//
  // "Pi", 1/SqRt(2*Pi):                                                     //
  //-------------------------------------------------------------------------//
  template<typename F>
  DEVICE  constexpr F Pi;

# ifdef __CUDACC__
  template<> constexpr inline float  Pi<float>  = CUDART_PI_F;
  template<> constexpr inline double Pi<double> = CUDART_PI;
# else
  template<> constexpr inline float  Pi<float>  = float(M_PI);
  template<> constexpr inline double Pi<double> = M_PI;
# endif

  template<typename F>
  constexpr inline  F InvSqRt2Pi = F(1.0) / SqRt(F(2.0) * Pi<F>);

  //-------------------------------------------------------------------------//
  // Trigonomtric Functions and Their Inverses:                              //
  //-------------------------------------------------------------------------//
# ifdef __CUDACC__
  DEVICE    inline float  Cos  (float  x)  { return      cosf (x);  }
  DEVICE    inline double Cos  (double x)  { return      cos  (x);  }
# else
  constexpr inline float  Cos  (float  x)  { return std::cos  (x);  }
  constexpr inline double Cos  (double x)  { return std::cos  (x);  }
# endif

# ifdef __CUDACC__
  DEVICE    inline float  Sin  (float  x)  { return      sinf (x);  }
  DEVICE    inline double Sin  (double x)  { return      sin  (x);  }
# else
  constexpr inline float  Sin  (float  x)  { return std::sin  (x);  }
  constexpr inline double Sin  (double x)  { return std::sin  (x);  }
# endif

# ifdef __CUDACC__
  DEVICE    inline float  Tan  (float  x)  { return      tanf(x);   }
  DEVICE    inline double Tan  (double x)  { return      tan (x);   }
# else
  constexpr inline float  Tan  (float  x)  { return std::tan (x);   }
  constexpr inline double Tan  (double x)  { return std::tan (x);   }
# endif

# ifdef __CUDACC__
  DEVICE    inline float  ASin (float  x)  { return      asinf(x);  }
  DEVICE    inline double ASin (double x)  { return      asin (x);  }
# else
  constexpr inline float  ASin (float  x)  { return std::asin (x);  }
  constexpr inline double ASin (double x)  { return std::asin (x);  }
# endif

# ifdef __CUDACC__
  DEVICE    inline float  ACos (float  x)  { return      acosf(x);  }
  DEVICE    inline double ACos (double x)  { return      acos (x);  }
# else
  constexpr inline float  ACos (float  x)  { return std::acos (x);  }
  constexpr inline double ACos (double x)  { return std::acos (x);  }
# endif

# ifdef __CUDACC__
  DEVICE    inline float  ATan (float  x)  { return      atanf(x);  }
  DEVICE    inline double ATan (double x)  { return      atan (x);  }
# else
  constexpr inline float  ATan (float  x)  { return std::atan (x);  }
  constexpr inline double ATan (double x)  { return std::atan (x);  }
# endif

  //-------------------------------------------------------------------------//
  // Hyperbolic Functions and Their Inverses:                                //
  //-------------------------------------------------------------------------//
# ifdef __CUDACC__
  DEVICE    inline float  CosH   (float  x)  { return      coshf(x);  }
  DEVICE    inline double CosH   (double x)  { return      cosh (x);  }
# else
  constexpr inline float  CosH   (float  x)  { return std::cosh (x);  }
  constexpr inline double CosH   (double x)  { return std::cosh (x);  }
# endif

# ifdef __CUDACC__
  DEVICE    inline float  SinH   (float  x)  { return      sinhf(x);  }
  DEVICE    inline double SinH   (double x)  { return      sinh (x);  }
# else
  constexpr inline float  SinH   (float  x)  { return std::sinh (x);  }
  constexpr inline double SinH   (double x)  { return std::sinh (x);  }
# endif

# ifdef __CUDACC__
  DEVICE    inline float  TanH   (float  x)  { return      tanhf(x);  }
  DEVICE    inline double TanH   (double x)  { return      tanh (x);  }
# else
  constexpr inline float  TanH   (float  x)  { return std::tanh (x);  }
  constexpr inline double TanH   (double x)  { return std::tanh (x);  }
# endif

# ifdef __CUDACC__
  DEVICE    inline float  ACosH  (float  x)  { return      acoshf(x); }
  DEVICE    inline double ACosH  (double x)  { return      acosh (x); }
# else
  constexpr inline float  ACosH  (float  x)  { return std::acosh (x); }
  constexpr inline double ACosH  (double x)  { return std::acosh (x); }
# endif

# ifdef __CUDACC__
  DEVICE    inline float  ASinH  (float  x)  { return      asinhf(x); }
  DEVICE    inline double ASinH  (double x)  { return      asinh (x); }
# else
  constexpr inline float  ASinH  (float  x)  { return std::asinh (x); }
  constexpr inline double ASinH  (double x)  { return std::asinh (x); }
# endif

# ifdef __CUDACC__
  DEVICE    inline float  ATanH  (float  x)  { return      atanhf(x); }
  DEVICE    inline double ATanH  (double x)  { return      atanh (x); }
# else
  constexpr inline float  ATanH  (float  x)  { return std::atanh (x); }
  constexpr inline double ATanH  (double x)  { return std::atanh (x); }
# endif

  //-------------------------------------------------------------------------//
  // PDF and CDF of the Standard Normal Distribution:                        //
  //-------------------------------------------------------------------------//
# ifdef __CUDACC__
  DEVICE    inline float  Erf    (float  x)  { return      erff(x); }
  DEVICE    inline double Erf    (double x)  { return      erf (x); }
# else
  constexpr inline float  Erf    (float  x)  { return std::erf (x); }
  constexpr inline double Erf    (double x)  { return std::erf (x); }
# endif

  template<typename F>
  DEVICE constexpr inline F NPDF (F x)
    { return Exp(- F(0.5) * x * x) * InvSqRt2Pi<F>; }

  template<typename F>
  DEVICE constexpr inline F NCDF (F x)
    { return F(0.5)  * (F(1.0)  + Erf(x / F(M_SQRT2))); }

  //-------------------------------------------------------------------------//
  // Rounding, Finiteness:                                                   //
  //-------------------------------------------------------------------------//
  // Floor:
# ifdef __CUDACC__
  DEVICE    inline float   Floor   (float  x)  { return      floorf(x); }
  DEVICE    inline double  Floor   (double x)  { return      floor (x); }
# else
  constexpr inline float   Floor   (float  x)  { return std::floor (x); }
  constexpr inline double  Floor   (double x)  { return std::floor (x); }
# endif

  // Ceil:
# ifdef __CUDACC__
  DEVICE    inline float   Ceil    (float  x)  { return      ceilf(x);  }
  DEVICE    inline double  Ceil    (double x)  { return      ceil (x);  }
# else
  constexpr inline float   Ceil    (float  x)  { return std::ceil (x);  }
  constexpr inline double  Ceil    (double x)  { return std::ceil (x);  }
# endif

  // Round:
# ifdef __CUDACC__
  DEVICE    inline float   Round   (float  x)  { return      roundf(x); }
  DEVICE    inline double  Round   (double x)  { return      round (x); }
# else
  constexpr inline float   Round   (float  x)  { return std::round (x); }
  constexpr inline double  Round   (double x)  { return std::round (x); }
# endif

  // IsFinite:
# ifdef __CUDACC__
  DEVICE    inline bool    IsFinite(float  x)  { return bool(isfinite(x)); }
  DEVICE    inline bool    IsFinite(double x)  { return bool(isfinite(x)); }
# else
  constexpr inline bool    IsFinite(float  x)  { return std::isfinite(x);  }
  constexpr inline bool    IsFinite(double x)  { return std::isfinite(x);  }
# endif

  // IsNaN:
# ifdef __CUDACC__
  DEVICE    inline bool    IsNaN   (float  x)  { return bool(isnan(x)); }
  DEVICE    inline bool    IsNaN   (double x)  { return bool(isnan(x)); }
# else
  constexpr inline bool    IsNaN   (float  x)  { return std::isnan(x);  }
  constexpr inline bool    IsNaN   (double x)  { return std::isnan(x);  }
# endif

  // IsInf:
# ifdef __CUDACC__
  DEVICE    inline bool    IsInf   (float  x)  { return bool(isinf(x)); }
  DEVICE    inline bool    IsInf   (double x)  { return bool(isinf(x)); }
# else
  constexpr inline bool    IsInf   (float  x)  { return std::isinf(x);  }
  constexpr inline bool    IsInf   (double x)  { return std::isinf(x);  }
# endif

  //=========================================================================//
  // Floating-Point Conversions and Types Identification:                    //
  //=========================================================================//
  // "IsFloat":
  template<typename F>
  DEVICE constexpr  inline bool IsFloat()                   { return false; }
  template<> DEVICE inline constexpr bool IsFloat<float>()  { return true;  }

  // "IsDouble":
  template<typename F>
  DEVICE constexpr  inline bool IsDouble()                  { return false; }
  template<> DEVICE inline constexpr bool IsDouble<double>(){ return true;  }

  //=========================================================================//
  // "ContFrac": Continuous Fraction Expansion:                              //
  //=========================================================================//
  // Approximates x =~= m / n, where min(m,n) <= Limit:
  //
  template<typename F>
  DEVICE inline void ContFrac(F x, long* m, long* n, long Limit)
  {
    assert(m != nullptr && n != nullptr);
    *m = 0;
    *n = 0;

    if (x <= 0.0)
#     ifndef __CUDACC__
      throw std::invalid_argument("ContFrac: Currently requires x > 0");
#     else
      return;
#     endif

    bool inv = false;
    if (x > 1.0)
    {
      x   = 1.0 / x;
      inv = true;
    }
    assert(0.0 < x && x < 1.0);

    // Now proceed:
    long m0 = 0, n0 = 0;

    long m2 = 0;
    long m1 = 1;
    long n2 = 1;
    long n1 = 0;
    long a  = 0;

    while (m0 <= Limit || n0 <= Limit)    // min(m0,n0) <= Limit
    {
      long mA = a * m1 + m2;
      long nA = a * n1 + n2;
      assert(mA >= 0 && nA >= 1 && mA < nA);

      if (mA > Limit && nA > Limit)       // min(mA,nA) > Limit
        // Keep the current (m0, n0):
        break;

      m0 = mA;
      n0 = nA;

      // For the next iteartion:
      x  = 1.0 / x;
      a  = long(Floor(x));
      x -= F(a);

      m2 = m1;  m1 = m0;
      n2 = n1;  n1 = n0;
    }
    // Store the result:
    if (!inv)
      { *m = m0;  *n = n0; }
    else
      { *m = n0;  *n = m0; }
  }

  //=========================================================================//
  // Compile-Time GCD and Normalisation Functions:                           //
  //=========================================================================//
  DEVICE constexpr int GCDrec(int p, int q)
  {
    // 0 <= p <= q
    return (p == 0) ? q : GCDrec(q % p, p);
  }

  DEVICE constexpr int GCD(int m, int n)
  {
    int am = std::abs(m);
    int an = std::abs(n);
    return am <= an
           ? GCDrec(am, an)
           : GCDrec(an, am);
  }

  DEVICE constexpr int NormaliseNumer(int m, int n)
  {
    // NB: GCD is always >= 0. Compile-time error occurs if GCD == 0 (which can
    // happen only if m == n == 0):
    return (n > 0)
            ?   (m / GCD(m, n))
            : - (m / GCD(m, n));
  }

  DEVICE constexpr int NormaliseDenom(int m, int n)
    { return std::abs(n) / GCD(m, n); }

  //=========================================================================//
  // Power Templated Functions:                                              //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // Power Expr with a Natural Degree:                                       //
  //-------------------------------------------------------------------------//
  // The Even Positive case:
  template<typename T, int M, bool IsEven = (M % 2 == 0)>
  struct NatPower
  {
    static_assert((M >= 2) && IsEven, "Invalid degree in NatPower");
    DEVICE constexpr static T res(T base)
      { return Sqr(NatPower<T, M/2>::res(base)); }
  };

  // The Odd case:
  template<typename T, int M>
  struct NatPower<T, M, false>
  {
    static_assert(M >= 1, "Invalid degree in NatPower");
    DEVICE constexpr static T res(T base)
      { return base * Sqr(NatPower<T, M/2>::res(base)); }
  };

  // M==1: This case is not really necessary, but is provided for efficiency:
  template<typename T>
  struct NatPower<T, 1, false>
  {
    DEVICE constexpr static T res(T base)
      { return base; }
  };

  // M==0: Trivial case:
  template<typename T>
  struct NatPower<T, 0, true>
  {
    DEVICE constexpr static T res(T)
      { return T(1.0); }  // XXX: We do not check if base==0
  };

  //-------------------------------------------------------------------------//
  // Power Expr with a General Integral Degree:                              //
  //-------------------------------------------------------------------------//
  // Natural case:
  template<typename T, int M, bool IsNatural = (M >= 0)>
  struct IntPower
  {
    static_assert(IsNatural, "IntPower: Case match error");
    DEVICE constexpr static T res(T base)
      { return NatPower<T, M>::res(base); }
  };

  // The Negative Integral case:
  template<typename T, int M>
  struct IntPower<T, M, false>
  {
    DEVICE constexpr static T res(T base)
      { return T(1.0) / NatPower<T, -M>::res(base); }
  };

  //-------------------------------------------------------------------------//
  // "SqRtPower":                                                            //
  //-------------------------------------------------------------------------//
  // The degree is a (normalised) Rational; testing the denom "N" for being a
  // multiple of 2 -- if so, rewriting the power expr via "SqRt".
  //
  // Generic case: N != 1 and N is NOT a multiple of 2: use "pow":
  template<typename T, int M, int N, bool IsSqrt = (N % 2 == 0)>
  struct SqRtPower
  {
    static_assert(N >= 3 && !IsSqrt, "SqRtPower: Degree not normalised");
    DEVICE constexpr static T res(T base)
      { return pow(base, T(M) / T(N)); }
  };

  // The Integral degree case (N==1):
  template<typename T, int M>
  struct SqRtPower<T, M, 1, false>
  {
    DEVICE constexpr static T res(T base)
      { return IntPower<T, M>::res(base); }
  };

  // "SqRt" case: NB: "N" is indeed a multiple of 2:
  template<typename T, int M, int N>
  struct SqRtPower <T, M,  N, true>
  {
    static_assert(N >= 2 && N % 2 == 0, "SqRtPower: Case match error");
    DEVICE constexpr static T res(T base)
      { return SqRtPower<T, M, N/2>::res(SqRt(base)); }
  };

  //-------------------------------------------------------------------------//
  // "CbRtPower":                                                            //
  //-------------------------------------------------------------------------//
  // Similar to "SqRtPower"; testing "N" for being a multiple of 3,  and if so,
  // rewriting the power expr via "CbRt". However, "CbRt" is only available for
  // real types.
  //
  // Generic case: N is NOT a multiple of 3, or inappropritate type "T":   Fall
  // back to "SqRtPower":
  template<typename T, int M, int N, bool IsCbrt = (N % 3 == 0)>
  struct CbRtPower
  {
    DEVICE constexpr static T res(T base)
      { return SqRtPower<T, M, N>::res(base); }
  };

  // The following types allow us to use "CbRt":
  template<typename T, int M, int N>
  struct CbRtPower <T, M,  N, true>
  {
    static_assert(N >= 3 && N % 3 == 0, "CbRtPower: Case match error");
    DEVICE constexpr static T res(T base)
      { return CbRtPower<T, M, N/3>::res(CbRt(base)); }
  };

  //-------------------------------------------------------------------------//
  // Power Expr with a General Fractional Degree:                            //
  //-------------------------------------------------------------------------//
  // NB: This is a template function, not a struct -- it has no partial specs.
  // First attempt "CbRtPower", then it gets reduced further:
  template<int M, int N,  typename T>
  DEVICE inline constexpr T FracPower(T base)
  {
    static_assert(N != 0, "Zero denom in fractional degree");
    return CbRtPower<T, NormaliseNumer(M,N), NormaliseDenom(M,N)>::res(base);
  }

  //=========================================================================//
  // "StrToF":                                                               //
  //=========================================================================//
  // XXX: currently implemented via "strtod", so could lead to loss of accuracy
  // if "F" is larger than "double" (but this does not happen on modern archs):
  // NB: "to" is the pos of the last char to be scanned; if to < 0, the pos is
  // counted from th eend of the string (Python-like):
  //
# ifndef __CUDACC__
  template<typename F>
  F StrToF(std::string const& str, int from, int to)
  {
    int l = int(str.length());
    if (to < 0)
      to += l + 1;

    if (!(0 <= from && from < to && to <= l))
      throw std::invalid_argument("StrToF: Invalid substr");

    char const* start = str.data() + from;
    char const* end   = str.data() + to;
    char*       got   = nullptr;

    double val = strtod(start, &got);
    if (got != end)
      throw std::invalid_argument("StrToF: Invalid string format");

    return F(val);
  }
# endif

  //=========================================================================//
  // "BSMImplVol":                                                           //
  //=========================================================================//
  // XXX: Currently, this is for European options only (Americal options would
  // be different in case of Put):
  //
  template<typename F>
  DEVICE inline F BSMImplVol
  (
    bool  is_call,
    F     C0,             // Option price at pricing time
    F     tau,            // Time to expiration, as a Year Fraction
    F     S0,             // Spot price at pricing time
    F     K,              // Strike
    F     r,              // Risk-free interest rate
    F     D     = F(0.0), // Dividend rate
    F*    delta = nullptr // If non-NULL, "delta" is returned here
  )
  {
    if (C0 <= F(0.0) || tau <= F(0.0) || S0 <= F(0.0) || K <= F(0.0))
#     ifndef __CUDACC__
      throw std::invalid_argument("BSMImplVol: Invalid arg(s)");
#     else
      return NaN<F>();
#     endif

    // Tolerance:
    F Tol   = F(500.0) * Eps<F>();

    // Initial guess: Should be fine in most normal situations:
    F sigma = F(0.3);

    // Moneyness:
    F sqrT   = SqRt(tau);
    F x      = Log(S0 / K) / sqrT + (r - D) * sqrT;
    F z      = F(0.5) * sqrT;

    // Exponential Coeffs:
    F D0     = Exp(- D * tau);
    F E0     = S0 * D0;
    F K0     = K  * Exp(- r * tau);
    F Vega0  = E0 * sqrT * InvSqRt2Pi<F>();

    // Check the limits. XXX: Lower limits need to be made more precise, eg for
    // at-the-money options:
    //
    F LoC    = F(0.0);
    F UpC    = F(0.0);
    if (is_call)
    {
      LoC = Max(E0 - K0, F(0.0));
      UpC = E0;
    }
    else
    {
      LoC = Max(K0 - E0, F(0.0));
      UpC = K0;
    }
    if (!(LoC < C0 && C0 < UpC))
#     ifndef __CUDACC__
      throw std::invalid_argument("BSMImplVol: Unattaianble Option Price");
#     else
      return NaN<F>();
#     endif

    int const MAX_ITERS = 1000000;
    int  i = 0;
    for (; i < MAX_ITERS; ++i)
    {
      // "C" is the option price at "sigma":
      F xs  = x / sigma;
      F zs  = z * sigma;

      F d1  = xs + zs;
      F d2  = xs - zs;
      if (!is_call)
      {
        d1 = - d1;
        d2 = - d2;
      }

      F N1 = NCDF(d1);
      F N2 = NCDF(d2);
      F C = E0 * N1 - K0 * N2;
      if (!is_call)
        C = - C;

      // Discrepancy:
      F dC = C - C0;

      // Vega (same for call and Put):
      F Vega  = Vega0 * Exp(- F(0.5) * d1 * d1);
      assert(Vega > F(0.0));

      // Next approximation or exit?
      F dSigma = dC / Vega;
      if (Abs(dSigma) < Tol || Abs(dC) < Tol)
      {
        // Exit condition is satisfied:
        // Save "delta" if requested:
        if (delta != nullptr)
          *delta = D0 * (is_call ? N1 : (N1 - 1.0));
        break;
      }

      // If next approximation: apply some limits on "sigma":
      sigma -= dSigma;
      if (sigma <= F(0.0))
        sigma = F(1e-4);
      else
      if (sigma >= F(2.0))
        sigma = F(2.0);
    }
    if (i == MAX_ITERS)
    {
      if (delta != nullptr)
        *delta = NaN<F>();

#     ifndef __CUDACC__
      throw std::runtime_error("BSMImplVol: Divergence");
#     else
      return NaN<F>();
#     endif
    }
    return sigma;
  }

  //=========================================================================//
  // Misc:                                                                   //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // Power-of-2 Test:                                                        //
  //-------------------------------------------------------------------------//
  template<typename T>
  constexpr bool IsPow2(T a_n)
  {
    return
      (a_n <= 0)
      ? false
      : ((a_n & (~a_n + 1)) == a_n);
  }
} // End namespace MAQUETTE
