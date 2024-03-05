// vim:ts=2:et
//===========================================================================//
//                            "Basis/QtyConvs.hpp":                          //
//                   Convertions Between Different Qty Typs                  //
//===========================================================================//
#pragma once
#include "Basis/PxsQtys.h"
#include "Basis/SecDefs.h"

namespace MAQUETTE
{
  //=========================================================================//
  // Qty<QT,long> conversion into Qty<QT|UNDEFINED,double>:                  //
  //=========================================================================//
  template<QtyTypeT  QT>
  template<QtyTypeT  TargQT>
  inline Qty<QT,long>::operator Qty<TargQT,double>() const
  {
    // The following pre-condition must be met:
    static_assert(TargQT == QT || TargQT == QtyTypeT::UNDEFINED,
                  "Incompatible QTs");
    return Qty<TargQT,double>(m_v);
  }

  //=========================================================================//
  // Qty<QT,double> conversion into Qty<QT|UNDEFINED,long>:                  //
  //=========================================================================//
  template<QtyTypeT  QT>
  template<QtyTypeT  TargQT>
  inline Qty<QT,double>::operator Qty<TargQT,long>() const
  {
    // The following pre-condition must be met:
    static_assert(TargQT == QT || TargQT == QtyTypeT::UNDEFINED,
                  "Incompatible QTs");
    return Qty<TargQT,long>(m_v);
  }

  //=========================================================================//
  // "QtyConverter": Re-Scaling for Different QTs Based on SecDefS:          //
  //=========================================================================//
  // Here we really use the Scaling Factors provided by a SecDefS (or SecDefD).
  // NB:
  // (*) The following combination of a templated struct and templated methods
  //     provides the effect of partial specialisation of templated methods;
  // (*) The struct template params (QtyTypeT) determine  the general logic of
  //     conversion;
  // (*) The method template params (QR types) determine the representation de-
  //     tails. The conversion methods are called, unsurprisingly, "Convert":
  //
  template<QtyTypeT FromQT, QtyTypeT ToQT>
  struct   QtyConverter;

  //=========================================================================//
  // (0) Contracts -> Lots:                                                  //
  //=========================================================================//
  // No matter what the actual QRs are, Contracts and Lots  are integral, hence
  // LotSize must be integral as well, and we can use "long" as the intermediate
  // type:
  template<>
  struct QtyConverter<QtyTypeT::Contracts, QtyTypeT::Lots>
  {
    template<typename FromQR,  typename ToQR>
    static Qty<QtyTypeT::Lots, ToQR>    Convert
    (
      Qty<QtyTypeT::Contracts, FromQR>  a_from,
      SecDefS const&                    a_instr,
      PriceT                            UNUSED_PARAM(a_instr_px)
    )
    {
      //---------------------------------------------------------------------//
      // XXX: Special Vals first:                                            //
      //---------------------------------------------------------------------//
      if (a_from == 0)
        return Qty<QtyTypeT::Lots, ToQR>::Zero   ();
      else
      if (utxx::unlikely(IsPosInf (a_from)))
        return Qty<QtyTypeT::Lots, ToQR>::PosInf ();
      else
      if (utxx::unlikely(IsNegInf (a_from)))
        return Qty<QtyTypeT::Lots, ToQR>::NegInf ();
      else
      if (utxx::unlikely(IsInvalid(a_from)))
        return Qty<QtyTypeT::Lots, ToQR>::Invalid();

      //---------------------------------------------------------------------//
      // Generic Case:                                                       //
      //---------------------------------------------------------------------//
      assert(a_instr.m_LotSize >  0.0  &&
             a_instr.m_LotSize == Round(a_instr.m_LotSize));

      long   contrs = long(a_from); // Exception if fractional (should not be!)
      long   lotSz  = long(Round(a_instr.m_LotSize));
      assert(lotSz  > 0);           // XXX: contrs < 0 is allowed!

      // XXX: What to do if "contrs" is not a multiple of "lotSz"? We currently
      // throw an exception, because any rounding may be dangerous:
      CHECK_ONLY
      (
        if (utxx::unlikely(contrs % lotSz != 0))
          throw utxx::badarg_error
                ("Convert(Contracts->Lots): {}: NContracts={} is not a multiple"
                 " of LotSz={}", a_instr.m_Symbol.data(), contrs, lotSz);
      )
      // If OK:
      return Qty<QtyTypeT::Lots, ToQR>(contrs / lotSz);
    }
  };
  //=========================================================================//
  // (1) Contracts -> QtyA:                                                  //
  //=========================================================================//
  template<>
  struct QtyConverter<QtyTypeT::Contracts, QtyTypeT::QtyA>
  {
    template<typename FromQR,  typename ToQR>
    static Qty<QtyTypeT::QtyA, ToQR>    Convert
    (
      Qty<QtyTypeT::Contracts, FromQR>  a_from,
      SecDefS const&                    a_instr,
      PriceT                            UNUSED_PARAM(a_instr_px)
    )
    {
      //---------------------------------------------------------------------//
      // Special Cases:                                                      //
      //---------------------------------------------------------------------//
      // First of all, check that QtyA is enabled in SecDef:
      // "aQty" is per 1 Contract, and must be positive:
      double aQty = a_instr.m_ABQtys[0];
      CHECK_ONLY
      (
        if (utxx::unlikely(!(aQty > 0.0)))
          throw utxx::badarg_error
                ("Convert(Contracts->QtyA): {}: QtyA is not in use",
                 a_instr.m_Symbol.data());
      )
      // Then check for Special Vals:
      if (a_from == 0)
        return Qty<QtyTypeT::QtyA, ToQR>::Zero   ();
      else
      if (utxx::unlikely(IsPosInf (a_from)))
        return Qty<QtyTypeT::QtyA, ToQR>::PosInf ();
      else
      if (utxx::unlikely(IsNegInf (a_from)))
        return Qty<QtyTypeT::QtyA, ToQR>::NegInf ();
      else
      if (utxx::unlikely(IsInvalid(a_from)))
        return Qty<QtyTypeT::QtyA, ToQR>::Invalid();

      //---------------------------------------------------------------------//
      // Generic Case:                                                       //
      //---------------------------------------------------------------------//
      long   contrs = long(a_from); // Exception if fractional (should not be!)

      // If ToQR is fractional, the following conversion will always succeed.
      // If ToQR is integral, we may get an exception here if the val is fract:
      //
      return Qty<QtyTypeT::QtyA, ToQR>(double(contrs) * aQty);
    }
  };
  //=========================================================================//
  // (2) Contracts -> QtyB:                                                  //
  //=========================================================================//
  // Very similar to Contracts -> QtyA in (1) above:
  //
  template<>
  struct QtyConverter<QtyTypeT::Contracts, QtyTypeT::QtyB>
  {
    template<typename FromQR,  typename ToQR>
    static Qty<QtyTypeT::QtyB, ToQR>    Convert
    (
      Qty<QtyTypeT::Contracts, FromQR>  a_from,
      SecDefS const&                    a_instr,
      PriceT                            UNUSED_PARAM(a_instr_px)
    )
    {
      //---------------------------------------------------------------------//
      // Special Cases:                                                      //
      //---------------------------------------------------------------------//
      // First of all, check that QtyB is enabled in SecDef:
      // "bQty" is per 1 Contract, and must be positive:
      double bQty = a_instr.m_ABQtys[1];
      CHECK_ONLY
      (
        if (utxx::unlikely(!(bQty > 0.0)))
          throw utxx::badarg_error
                ("Convert(Contracts->QtyB): {}: QtyB is not in use",
                 a_instr.m_Symbol.data());
      )
      // Then check for Special Vals:
      if (a_from == 0)
        return Qty<QtyTypeT::QtyB, ToQR>::Zero   ();
      else
      if (utxx::unlikely(IsPosInf (a_from)))
        return Qty<QtyTypeT::QtyB, ToQR>::PosInf ();
      else
      if (utxx::unlikely(IsNegInf (a_from)))
        return Qty<QtyTypeT::QtyB, ToQR>::NegInf ();
      else
      if (utxx::unlikely(IsInvalid(a_from)))
        return Qty<QtyTypeT::QtyB, ToQR>::Invalid();

      //---------------------------------------------------------------------//
      // Generic Case:                                                       //
      //---------------------------------------------------------------------//
      long   contrs = long(a_from); // Exception if fractional (should not be!)

      // If ToQR is fractional, the following conversion will always succeed.
      // If ToQR is integral, we may get an exception here if the val is fract:
      //
      return Qty<QtyTypeT::QtyB, ToQR>(double(contrs) * bQty);
    }
  };
  //=========================================================================//
  // (3) Lots -> Contracts:                                                  //
  //=========================================================================//
  // Similar to Contracts -> Lots in (0), it is a purely integral conversion.
  // As in (1), LotSize must be integral as well in this case:
  //
  template<>
  struct QtyConverter<QtyTypeT::Lots, QtyTypeT::Contracts>
  {
    template<typename FromQR,    typename ToQR>
    static Qty<QtyTypeT::Contracts, ToQR> Convert
    (
      Qty<QtyTypeT::Lots, FromQR>   a_from,
      SecDefS const&                a_instr,
      PriceT                        UNUSED_PARAM(a_instr_px)
    )
    {
      // XXX: Special Vals first:
      if (a_from == 0)
        return Qty<QtyTypeT::Contracts, ToQR>::Zero   ();
      else
      if (utxx::unlikely(IsPosInf (a_from)))
        return Qty<QtyTypeT::Contracts, ToQR>::PosInf ();
      else
      if (utxx::unlikely(IsNegInf (a_from)))
        return Qty<QtyTypeT::Contracts, ToQR>::NegInf ();
      else
      if (utxx::unlikely(IsInvalid(a_from)))
        return Qty<QtyTypeT::Contracts, ToQR>::Invalid();

      // Generic Case is simple:
      assert(a_instr.m_LotSize >  0.0 &&
             a_instr.m_LotSize == Round(a_instr.m_LotSize));

      long   lots  = long(a_from); // Exception if fractional (should not be!)
      long   lotSz = long(Round(a_instr.m_LotSize));
      assert(lotSz > 0);
      return Qty<QtyTypeT::Contracts, ToQR>(lots * lotSz);
    }
  };
  //=========================================================================//
  // (4) Lots -> QtyA:                                                       //
  //=========================================================================//
  // Implemented as Lots -> Contracts -> QtyA:
  //
  template<>
  struct QtyConverter<QtyTypeT::Lots, QtyTypeT::QtyA>
  {
    template<typename FromQR,  typename ToQR>
    static Qty<QtyTypeT::QtyA, ToQR>    Convert
    (
      Qty<QtyTypeT::Lots, FromQR>       a_from,
      SecDefS const&                    a_instr,
      PriceT                            UNUSED_PARAM(a_instr_px)
    )
    {
      //---------------------------------------------------------------------//
      // Special Cases:                                                      //
      //---------------------------------------------------------------------//
      // First of all, check that QtyA is enabled in SecDef:
      // "aQty" is per 1 Contract, and must be positive:
      double aQty = a_instr.m_ABQtys[0];
      CHECK_ONLY
      (
        if (utxx::unlikely(!(aQty > 0.0)))
          throw utxx::badarg_error
                ("Convert(Contracts->QtyA): {}: QtyA is not in use",
                 a_instr.m_Symbol.data());
      )
      // Then check for Special Vals:
      if (a_from == 0)
        return Qty<QtyTypeT::QtyA, ToQR>::Zero   ();
      else
      if (utxx::unlikely(IsPosInf (a_from)))
        return Qty<QtyTypeT::QtyA, ToQR>::PosInf ();
      else
      if (utxx::unlikely(IsNegInf (a_from)))
        return Qty<QtyTypeT::QtyA, ToQR>::NegInf ();
      else
      if (utxx::unlikely(IsInvalid(a_from)))
        return Qty<QtyTypeT::QtyA, ToQR>::Invalid();

      //---------------------------------------------------------------------//
      // Generic Case:                                                       //
      //---------------------------------------------------------------------//
      long   lots  = long(a_from); // Exception if fractional (should not be!)
      double lotSz = a_instr.m_LotSize;
      assert(lotSz > 0.0);

      // If ToQR is fractional, the following conversion will always succeed.
      // If ToQR is integral, we may get an exception here if the val is fract:
      //
      return Qty<QtyTypeT::QtyA, ToQR>(double(lots) * lotSz * aQty);
    }
  };
  //=========================================================================//
  // (5) Lots -> QtyB:                                                       //
  //=========================================================================//
  // Similar to Lots -> QtyA in (4) above.
  // Implemented as Lots -> Contracts -> QtyB:
  //
  template<>
  struct QtyConverter<QtyTypeT::Lots, QtyTypeT::QtyB>
  {
    template<typename  FromQR, typename ToQR>
    static Qty<QtyTypeT::QtyB, ToQR> Convert
    (
      Qty<QtyTypeT::Lots, FromQR>    a_from,
      SecDefS const&                 a_instr,
      PriceT                         UNUSED_PARAM(a_instr_px)
    )
    {
      //---------------------------------------------------------------------//
      // Special Cases:                                                      //
      //---------------------------------------------------------------------//
      // First of all, check that QtyB is enabled in SecDef:
      // "bQty" is per 1 Contract, and must be positive:
      double bQty = a_instr.m_ABQtys[1];
      CHECK_ONLY
      (
        if (utxx::unlikely(!(bQty > 0.0)))
          throw utxx::badarg_error
                ("Convert(Contracts->QtyB): {}: QtyB is not in use",
                 a_instr.m_Symbol.data());
      )
      // Then check for Special Vals:
      if (a_from == 0)
        return Qty<QtyTypeT::QtyB, ToQR>::Zero   ();
      else
      if (utxx::unlikely(IsPosInf (a_from)))
        return Qty<QtyTypeT::QtyB, ToQR>::PosInf ();
      else
      if (utxx::unlikely(IsNegInf (a_from)))
        return Qty<QtyTypeT::QtyB, ToQR>::NegInf ();
      else
      if (utxx::unlikely(IsInvalid(a_from)))
        return Qty<QtyTypeT::QtyB, ToQR>::Invalid();

      //---------------------------------------------------------------------//
      // Generic Case:                                                       //
      //---------------------------------------------------------------------//
      long   lots  = long(a_from); // Exception if fractional (should not be!)
      double lotSz = a_instr.m_LotSize;
      assert(lotSz > 0.0);

      // If ToQR is fractional, the following conversion will always succeed.
      // If ToQR is integral, we may get an exception here if the val is fract:
      //
      return Qty<QtyTypeT::QtyB, ToQR>(double(lots) * lotSz * bQty);
    }
  };
  //=========================================================================//
  // (6) QtyA -> Contracts:                                                  //
  //=========================================================================//
  template<>
  struct QtyConverter<QtyTypeT::QtyA, QtyTypeT::Contracts>
  {
    template<typename FromQR,    typename ToQR>
    static Qty<QtyTypeT::Contracts, ToQR> Convert
    (
      Qty<QtyTypeT::QtyA, FromQR>   a_from,
      SecDefS const&                a_instr,
      PriceT                        UNUSED_PARAM(a_instr_px)
    )
    {
      // If we got here, then "QtyA" MUST be enabled:
      double aQty = a_instr.m_ABQtys[0];  // QtyA per 1 Contract
      assert(aQty > 0.0);

      // Check for Special Vals:
      if (a_from == 0)
        return Qty<QtyTypeT::Contracts, ToQR>::Zero   ();
      else
      if (utxx::unlikely(IsPosInf (a_from)))
        return Qty<QtyTypeT::Contracts, ToQR>::PosInf ();
      else
      if (utxx::unlikely(IsNegInf (a_from)))
        return Qty<QtyTypeT::Contracts, ToQR>::NegInf ();
      else
      if (utxx::unlikely(IsInvalid(a_from)))
        return Qty<QtyTypeT::Contracts, ToQR>::Invalid();

      // Generic Case:
      // No matter what the ToQR type is, the result MUST be integral.
      // To enforce that, we first create a Qty over Long:
      //
      Qty<QtyTypeT::Contracts, long>  tmp  (double(a_from) / aQty);
      return Qty<QtyTypeT::Contracts, ToQR>(tmp);
    }
  };
  //=========================================================================//
  // (7) QtyA -> Lots:                                                       //
  //=========================================================================//
  // Conversion Path: QtyA -> Contracts -> Lots:
  //
  template<>
  struct QtyConverter<QtyTypeT::QtyA, QtyTypeT::Lots>
  {
    template<typename FromQR,  typename ToQR>
    struct Qty<QtyTypeT::Lots, ToQR>    Convert
    (
      Qty<QtyTypeT::QtyA, FromQR>       a_from,
      SecDefS const&                    a_instr,
      PriceT                            UNUSED_PARAM(a_instr_px)
    )
    {
      // If we got here, then "QtyA" MUST be enabled:
      double aQty = a_instr.m_ABQtys[0];  // QtyA per 1 Contract
      assert(aQty > 0.0);

      // Check for Special Vals:
      if (a_from == 0)
        return Qty<QtyTypeT::Lots, ToQR>::Zero   ();
      else
      if (utxx::unlikely(IsPosInf (a_from)))
        return Qty<QtyTypeT::Lots, ToQR>::PosInf ();
      else
      if (utxx::unlikely(IsNegInf(a_from)))
        return Qty<QtyTypeT::Lots, ToQR>::NegInf ();
      else
      if (utxx::unlikely(IsInvalid(a_from)))
        return Qty<QtyTypeT::Lots, ToQR>::Invalid();

      // Generic Case:
      // No matter what the ToQR type is, the result MUST be integral.
      // To enforce that, we first create a Qty over Long:
      double lotSz = a_instr.m_LotSize;
      assert(lotSz > 0.0);

      Qty<QtyTypeT::Lots, long>  tmp  (double(a_from) / aQty / lotSz);
      return Qty<QtyTypeT::Lots, ToQR>(tmp);
    }
  };
  //=========================================================================//
  // (8) QtyB -> Contracts:                                                  //
  //=========================================================================//
  // Similar to QtyA -> Contracts in (6):
  //
  template<>
  struct QtyConverter<QtyTypeT::QtyB, QtyTypeT::Contracts>
  {
    template<typename FromQR, typename ToQR>
    static Qty<QtyTypeT::Contracts,    ToQR> Convert
    (
      Qty<QtyTypeT::QtyB, FromQR> a_from,
      SecDefS const&              a_instr,
      PriceT                      UNUSED_PARAM(a_instr_px)
    )
    {
      // If we got here, then "QtyB" MUST be enabled:
      double bQty = a_instr.m_ABQtys[1];  // QtyB per 1 Contract
      assert(bQty > 0.0);

      // Check for Special Vals:
      if (a_from == 0)
        return Qty<QtyTypeT::Contracts, ToQR>::Zero   ();
      else
      if (utxx::unlikely(IsPosInf (a_from)))
        return Qty<QtyTypeT::Contracts, ToQR>::PosInf ();
      else
      if (utxx::unlikely(IsNegInf (a_from)))
        return Qty<QtyTypeT::Contracts, ToQR>::NegInf ();
      else
      if (utxx::unlikely(IsInvalid(a_from)))
        return Qty<QtyTypeT::Contracts, ToQR>::Invalid();

      // Generic Case:
      // No matter what the ToQR type is, the result MUST be integral.
      // To enforce that, we first create a Qty over Long:
      //
      Qty<QtyTypeT::Contracts, long>  tmp  (double(a_from) / bQty);
      return Qty<QtyTypeT::Contracts, ToQR>(tmp);
    }
  };
  //=========================================================================//
  // (9) QtyB -> Lots:                                                       //
  //=========================================================================//
  // Similar to QtyA -> Lots in (7):
  // Conversion Path:   QtyB -> Contracts -> Lots:
  //
  template<>
  struct QtyConverter<QtyTypeT::QtyB, QtyTypeT::Lots>
  {
    template<typename FromQR,  typename ToQR>
    static Qty<QtyTypeT::Lots, ToQR>  Convert
    (
      Qty<QtyTypeT::QtyB, FromQR>     a_from,
      SecDefS const&                  a_instr,
      PriceT                          UNUSED_PARAM(a_instr_px)
    )
    {
      // If we got here, then "QtyB" MUST be enabled:
      double bQty = a_instr.m_ABQtys[1];  // QtyB per 1 Contract
      assert(bQty > 0.0);

      // Check for Special Vals:
      if (a_from == 0)
        return Qty<QtyTypeT::Lots, ToQR>::Zero   ();
      else
      if (utxx::unlikely(IsPosInf (a_from)))
        return Qty<QtyTypeT::Lots, ToQR>::PosInf ();
      else
      if (utxx::unlikely(IsNegInf (a_from)))
        return Qty<QtyTypeT::Lots, ToQR>::NegInf ();
      else
      if (utxx::unlikely(IsInvalid(a_from)))
        return Qty<QtyTypeT::Lots, ToQR>::Invalid();

      // Generic Case:
      // No matter what the ToQR type is, the result MUST be integral.
      // To enforce that, we first create a Qty over Long:
      double lotSz = a_instr.m_LotSize;
      assert(lotSz > 0.0);

      Qty<QtyTypeT::Lots, long>  tmp  (double(a_from) / bQty / lotSz);
      return Qty<QtyTypeT::Lots, ToQR>(tmp);
    }
  };

  //=========================================================================//
  // (10) QtyA -> QtyB: Requires Px(A/B):                                    //
  //=========================================================================//
  template<>
  struct QtyConverter<QtyTypeT::QtyA, QtyTypeT::QtyB>
  {
    template<typename FromQR,  typename ToQR>
    static Qty<QtyTypeT::QtyB, ToQR>  Convert
    (
      Qty<QtyTypeT::QtyA, FromQR>     a_from,
      SecDefS const&                  a_instr,
      PriceT                          a_instr_px
    )
    {
      // Verify the InstrPx: To avoid obscure errors, we reqire it to be strict-
      // ly Positive and Finite:
      CHECK_ONLY
      (
        if (utxx::unlikely(!(IsFinite(a_instr_px) && IsPos(a_instr_px))))
          throw utxx::badarg_error
                ("Convert(QtyA->QtyB): Invalid InstrPx=", double(a_instr_px));
      )
      // NB: InstrPx may not be the Px(A/B), so compute the latter:
      PriceT pxAB = a_instr.GetPxAB(a_instr_px);
      return Qty<QtyTypeT::QtyB, ToQR>(double(a_from) * double(pxAB));
    }
  };

  //=========================================================================//
  // (11) QtyB -> QtyA: Requires Px(A/B):                                    //
  //=========================================================================//
  template<>
  struct QtyConverter<QtyTypeT::QtyB, QtyTypeT::QtyA>
  {
    template<typename FromQR,  typename ToQR>
    static Qty<QtyTypeT::QtyA, ToQR>  Convert
    (
      Qty<QtyTypeT::QtyB, FromQR>     a_from,
      SecDefS const&                  a_instr,
      PriceT                          a_instr_px
    )
    {
      // Again, we require the InstrPx to be strictly Positive and Finite:
      CHECK_ONLY
      (
        if (utxx::unlikely(!(IsFinite(a_instr_px) && IsPos(a_instr_px))))
          throw utxx::badarg_error
                ("Convert(QtyA->QtyB): Invalid InstrPx=", double(a_instr_px));
      )
      // Again, get the Px(A/B) first, but in this case, it must be strictly
      // positive:
      PriceT pxAB = a_instr.GetPxAB(a_instr_px);
      return Qty<QtyTypeT::QtyA, ToQR>(double(a_from) / double(pxAB));
    }
  };

  //=========================================================================//
  // (12) Conversions to UNDEFINED (but not other way round!)                //
  //=========================================================================//
  // First, we provide the specific case UNDEFINED->UNDEFINED to avoid the ambi-
  // guity:
  template<>
  struct   QtyConverter<QtyTypeT::UNDEFINED, QtyTypeT::UNDEFINED>
  {
    template<typename FromQR, typename ToQR>
    static Qty<QtyTypeT::UNDEFINED,    ToQR> Convert
    (
      Qty<QtyTypeT::UNDEFINED, FromQR> a_from,
      SecDefS const&                   UNUSED_PARAM(a_instr),
      PriceT                           UNUSED_PARAM(a_instr_px)
    )
    { return Qty<QtyTypeT::UNDEFINED, ToQR>(a_from); }
  };

  template<QtyTypeT FromQT>
  struct   QtyConverter<FromQT, QtyTypeT::UNDEFINED>
  {
    template<typename FromQR, typename ToQR>
    static Qty<QtyTypeT::UNDEFINED,    ToQR> Convert
    (
      Qty<FromQT,FromQR>  a_from,
      SecDefS const&      UNUSED_PARAM(a_instr),
      PriceT              UNUSED_PARAM(a_instr_px)
    )
    { return Qty<QtyTypeT::UNDEFINED, ToQR>(a_from); }
  };

  //=========================================================================//
  // (13) Same-QT Conversions (required for Convert(QtyAny,...) below):      //
  //=========================================================================//
  template<QtyTypeT QT>
  struct   QtyConverter<QT,QT>
  {
    template<typename FromQR, typename ToQR>
    static Qty<QT,ToQR>   Convert
    (
      Qty<QT,FromQR>      a_from,
      SecDefS const&      UNUSED_PARAM(a_instr),
      PriceT              UNUSED_PARAM(a_instr_px)
    )
    { return Qty<QT,ToQR>(a_from); }
  };

  //=========================================================================//
  // "MkQty": Typed Qty<QT,QR> from a tagged Src value:                      //
  //=========================================================================//
  // Performs all necessary dybamic conversions:
  //
  template<QtyTypeT QT, typename QR, typename T>
  inline Qty<QT,QR> MkQty
  (
    T               a_src_val,
    QtyTypeT        a_src_qt,
    SecDefS const&  a_instr,
    PriceT          a_instr_px
  )
  {
    constexpr bool SrcIsFrac = std::is_floating_point_v<T>;

    switch (a_src_qt)
    {
    case QtyTypeT::Contracts:
      // "Contracts" can be stored in both Long and Double fmts, though Long is
      // a more natural rep in this case:  so "IsFrac is unlikely:
      return
        SrcIsFrac
        ? QtyConverter
                <QtyTypeT::Contracts, QT>::template Convert<double,QR>
            (Qty<QtyTypeT::Contracts, double>(a_src_val), a_instr, a_instr_px)
        : QtyConverter
                <QtyTypeT::Contracts, QT>::template Convert<long,  QR>
            (Qty<QtyTypeT::Contracts, long>  (a_src_val), a_instr, a_instr_px);

    case QtyTypeT::Lots:
      // Similar for "Lots":
      return
        SrcIsFrac
        ? QtyConverter
                 <QtyTypeT::Lots, QT>::template Convert<double,QR>
             (Qty<QtyTypeT::Lots,double>(a_src_val), a_instr, a_instr_px)
        : QtyConverter
                 <QtyTypeT::Lots, QT>::template Convert<long,  QR>
             (Qty<QtyTypeT::Lots,long>  (a_src_val), a_instr, a_instr_px);

    case QtyTypeT::QtyA:
      // Here Double QR is probably more likely, but we don't really know:
      return
        SrcIsFrac
        ? QtyConverter
                 <QtyTypeT::QtyA, QT>::template Convert<double,QR>
             (Qty<QtyTypeT::QtyA,double>(a_src_val), a_instr, a_instr_px)
        : QtyConverter
                 <QtyTypeT::QtyA, QT>::template Convert<long,  QR>
             (Qty<QtyTypeT::QtyA,long>  (a_src_val), a_instr, a_instr_px);

    case QtyTypeT::QtyB:
      // Similar for "QtyB" (though Long QR might be more likely here, eg
      // on BitMEX):
      return
        SrcIsFrac
        ? QtyConverter
                 <QtyTypeT::QtyB, QT>::template Convert<double,QR>
             (Qty<QtyTypeT::QtyB,double>(a_src_val), a_instr, a_instr_px)
        : QtyConverter
                 <QtyTypeT::QtyB, QT>::template Convert<long,  QR>
             (Qty<QtyTypeT::QtyB,long>  (a_src_val), a_instr, a_instr_px);

    case QtyTypeT::UNDEFINED:
      // FromQT=UNDEFINED is only allowed in Special Cases -- otherwise, we do
      // not know which scaling factor to apply:
      return
        (a_src_val  == T(0))
        ? Qty<QT,QR>::Zero   ()
        :
        utxx::unlikely(a_src_val == PosInfQ<T>)
        ? Qty<QT,QR>::PosInf ()
        :
        utxx::unlikely(a_src_val == NegInfQ<T>)
        ? Qty<QT,QR>::NegInf ()
        :
        utxx::unlikely(IsInvalidQ<T>(a_src_val))
        ? Qty<QT,QR>::Invalid()
        :
          throw utxx::badarg_error
                ("Convert(QtyAny): Invalid Arg: QT=",      int (a_src_qt),
                 ", IsFrac=",    SrcIsFrac,  "; LongVal=", long(a_src_val),
                 ", DoubleVal=", double(a_src_val));
    default:
      throw utxx::badarg_error
            ("Convert(QtyAny): UnSupported FromQT=", int(a_src_qt));
    }
    __builtin_unreachable();
  }

  //=========================================================================//
  // "ConvQty": Conversion of "QtyU" into a Typed Qty<QT,QR> using Tags:     //
  //=========================================================================//
  template<QtyTypeT QT, typename QR>
  inline Qty<QT,QR> QtyU::ConvQty
  (
    QtyTypeT        a_u_qt,
    bool            a_u_is_frac,
    SecDefS const&  a_instr,
    PriceT          a_instr_px
  )
  {
    return
      a_u_is_frac
      ? MkQty<QT,QR,double>(double(m_UD), a_u_qt, a_instr, a_instr_px)
      : MkQty<QT,QR,long>  (long  (m_UL), a_u_qt, a_instr, a_instr_px);
  }

  //=========================================================================//
  // Now: Top-Level "ConvQty": QtyAny -> Qty<QT,QR>:                         //
  //=========================================================================//
  template<QtyTypeT QT, typename QR>
  inline Qty<QT,QR> QtyAny::ConvQty
  (
    SecDefS const& a_instr,
    PriceT         a_instr_px
  )
  { return m_val.ConvQty<QT,QR>(m_qt, m_isFrac, a_instr, a_instr_px); }

} // End namespace MAQUETTE
