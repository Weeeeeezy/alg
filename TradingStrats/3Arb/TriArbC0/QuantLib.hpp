// vim:ts=2:et
//===========================================================================//
//                "TradingStrats/3Arb/TriArbC0/QuantLib.hpp":                //
//                     Quantitative Analysis Functions                       //
//===========================================================================//
#pragma  once
#include "TradingStrats/3Arb/TriArbC0/TriArbC0.h"
#include <ctime>

namespace MAQUETTE
{
  //=========================================================================//
  // "SolveLiquidityEqn":                                                    //
  //=========================================================================//
  // Solves the inverse problem to TotalPx computations. That is, for A/B and a
  // given Side and B, solves {CumPx(a, Side) = B} wrt a:
  //
  template<ExchC0T X>
  template<bool    IsBid>
  typename TriArbC0<X>::QtyN TriArbC0<X>::SolveLiquidityEqn
    (OrderBook const* a_ob,  QtyN a_b) const
  {
    assert(a_ob != nullptr && !IsNeg(a_b));

    // XXX: "QtyN" for A and B are actually expressed in different units (need-
    // less to say, CcyA and CcyB, resp), so be careful with using "QtyN" for B:
    //
    QtyN           cumA;   // Initially 0
    SecDefD const& instr = a_ob->GetInstr();

    auto scanner =
      [&cumA, &a_b, &instr]
      (int,  PriceT a_px_level, OrderBook::OBEntry const& a_obe) -> bool
      {
        // Get the OrderBook-typed Qty at this PxLevel:
        QtyO lq  = a_obe.GetAggrQty<QTO,QRO>();

        // Convert it into the A qty (so here using "QtyN" is fully safe):
        QtyN qA =
          QtyConverter<QTO,QT>::template Convert<QRO,QR>
            (lq, instr, a_px_level);

        // Compute the B qty at this level. NB: "a_px_level" is in general an
        // intrinsic InstrPx, not Px(A/B), so compute the latter first:
        PriceT pxAB = instr.GetPxAB(a_px_level);
        QtyN   qB   = qA   * double(pxAB);
        assert (IsPos(qA) && IsPos(qB) && !IsNeg(a_b));

        // Now check where we are (with tolerance):
        if (utxx::likely(qB <= a_b))
        {
          // This level is consumed entirely. It may or may not be the last one:
          cumA += qA;
          a_b  -= qB;         // So "a_b" is a decreasing remainder

          // In theory, a_b >= 0 (even for QR=double, this should be ensured by
          // the adjustment mechanism -- see Qty<*,double>):
          assert(!IsNeg(a_b));
          return  IsPos(a_b); // Continue if has not spent all B yet
        }
        else
        {
          assert(a_b < qB);   // Also includes the case when a_b==0 initially
          // Increment "cumA" proportionally:
          cumA += qA * (double(a_b) / double(qB));
          return false;
        }
        __builtin_unreachable();
      };

    // Run the "scanner" to an unlimited depth:
    a_ob->Traverse<IsBid>(0, scanner);

    // If there is an unspent portion of "a_b", no solution is available due to
    // lack of liquidity:
    return
      utxx::likely(IsZero(a_b)) ? cumA : QtyN::Invalid();
  }

  //=========================================================================//
  // "MkSynthPx*":                                                           //
  //=========================================================================//
  // NB:
  // (*) We use simple "GetVWAP1" here. In particular, we do not exclude poten-
  //     tially "manipulative" orders from the available liquidity, nor we acc-
  //     ount for our own aggressive in-flight orders. Arguably, ReserveFactors
  //     should be able to account for both effects.
  // (*) TODO: Use continuously-predicted, rather than actual, VWAPs:
  //
  //=========================================================================//
  // "MkSynthPxAB":                                                          //
  //=========================================================================//
  template<ExchC0T X>
  template<bool    IsBid>
  double TriArbC0<X>::MkSynthPxAB(TrInfo* a_tri) const
  {
    // Check the Passive and Aggr Legs  (I=I0=AB, I1=CB, I2=AC):
    // NB: For both I1=CB and I2=AC: same side as AB (IsBid):
    assert(a_tri != nullptr                         &&
           a_tri->m_passI         == AB             &&
           a_tri->m_passS         == int(IsBid)     &&
           a_tri->m_passInstr     == m_instrs[AB]   &&
           a_tri->m_passQty       == m_qtys  [AB]   &&
           a_tri->m_aggrIs    [0] == AB             &&
           a_tri->m_aggrIs    [1] == CB             &&
           a_tri->m_aggrIs    [2] == AC             &&
           a_tri->m_aggrSs    [0] == int(!IsBid)    &&
           a_tri->m_aggrSs    [1] == int( IsBid)    &&
           a_tri->m_aggrSs    [2] == int( IsBid)    &&
           a_tri->m_aggrInstrs[0] == m_instrs[AB]   &&
           a_tri->m_aggrInstrs[1] == m_instrs[CB]   &&
           a_tri->m_aggrInstrs[2] == m_instrs[AC]);

    // AC: Same qty as AB, but apply the reserve:
    // XXX "volAC" could be statically-precomputed, but the overhead of it is
    //     negligible:
    QtyN      qAB = m_qtys[AB];
    QtyN      qAC = qAB;
    QtyN    volAC = qAC * m_resrvFactors[AC];  // With the reserve

    // VWAP for "volAC": Potentially different Qty Types for the OrderBook and
    // this Strategy, result must be normalised to Px(A/C):
    PriceT vwapAC =
      m_orderBooks[AC]->GetVWAP1<QTO,QRO,IsBid,QT,QR,true>(volAC);

    // CB: Dynamic qtys. XXX: For the sizes of the covering orders,  the actal
    //     avg trade pxs can be used instead of theoretical VWAPs with reserve
    //     factors:
    QtyN      qCB = qAB * double(vwapAC);
    QtyN    volCB = qCB * m_resrvFactors[CB];
    PriceT vwapCB =
      m_orderBooks[CB]->GetVWAP1<QTO,QRO,IsBid,QT,QR,true>(volCB);

    // The synthetic px (w/o mark-up or other adjustments) is:
    double synthPxAB =  double(vwapAC) * double(vwapCB);
    assert(IsFinite(synthPxAB) && synthPxAB > 0.0);

    // Store the src data in "TrInfo" (I0=CB, I1=AC):
    a_tri->m_aggrQtys  [0] = qAB;
    a_tri->m_aggrQtys  [1] = qCB;
    a_tri->m_aggrQtys  [2] = qAC;
    a_tri->m_aggrVols  [0] = qAB;               // Does not matter much
    a_tri->m_aggrVols  [1] = volCB;
    a_tri->m_aggrVols  [2] = volAC;
    a_tri->m_aggrSrcPxs[0] = PriceT(synthPxAB); // Does not matter much
    a_tri->m_aggrSrcPxs[1] = vwapCB;
    a_tri->m_aggrSrcPxs[2] = vwapAC;

    return synthPxAB;
  }

  //=========================================================================//
  // "MkSynthPxCB":                                                          //
  //=========================================================================//
  template<ExchC0T X>
  template<bool    IsBid>
  double TriArbC0<X>::MkSynthPxCB(TrInfo* a_tri) const
  {
    // Check the Passive and Aggr Legs (I=CB, I1=AB, I2=AC):
    assert(a_tri != nullptr                         &&
           a_tri->m_passI         == CB             &&
           a_tri->m_passS         == int(IsBid)     &&
           a_tri->m_passInstr     == m_instrs[CB]   &&
           a_tri->m_passQty       == m_qtys  [CB]   &&
           a_tri->m_aggrIs    [0] == CB             &&
           a_tri->m_aggrIs    [1] == AB             &&
           a_tri->m_aggrIs    [2] == AC             &&
           a_tri->m_aggrSs    [0] == int(!IsBid)    &&
           a_tri->m_aggrSs    [1] == int( IsBid)    &&
           a_tri->m_aggrSs    [2] == int(!IsBid)    &&
           a_tri->m_aggrInstrs[0] == m_instrs[CB]   &&
           a_tri->m_aggrInstrs[1] == m_instrs[AB]   &&
           a_tri->m_aggrInstrs[2] == m_instrs[AC]);

    // First, obtain Vol(AC) and VWAP(AC) by solving the following LiquidityEqn:
    //    CumPx(AC, !IsBid, a) = Qty(CB) * ResFact(AC)
    // for a, and then Vol(AC) = a,        Qty(AC) = a / ResFact(AC);
    // NB: The side of AC is opposite to that of CB:
    //
    QtyN      qCB = m_qtys[CB];
    QtyN     volC = qCB * m_resrvFactors[AC];
    QtyN    volAC = SolveLiquidityEqn<!IsBid>  (m_orderBooks[AC], volC);
    QtyN      qAC = volAC       / m_resrvFactors[AC];
    PriceT vwapAC = PriceT(volC / volAC);

    // Now for AB (same side as CB):
    QtyN      qAB = qAC;
    QtyN    volAB = qAB         * m_resrvFactors[AB];
    PriceT vwapAB =
      m_orderBooks[AB]->GetVWAP1<QTO,QRO,IsBid,QT,QR,true>(volAB);

    // The synthetic px (w/o mark-up or other adjustments) is:
    double synthPxCB =  double(vwapAB) / double(vwapAC);
    assert(IsFinite(synthPxCB) && synthPxCB > 0.0);

    // Store the src data in "TrInfo" (I=I0=CB, I1=AB, I2=AC):
    a_tri->m_aggrQtys  [0] = qCB;
    a_tri->m_aggrQtys  [1] = qAB;
    a_tri->m_aggrQtys  [2] = qAC;
    a_tri->m_aggrVols  [0] = qCB;               // Does not matter much
    a_tri->m_aggrVols  [1] = volAB;
    a_tri->m_aggrVols  [2] = volAC;
    a_tri->m_aggrSrcPxs[0] = PriceT(synthPxCB); // Does not matter much
    a_tri->m_aggrSrcPxs[1] = vwapAB;
    a_tri->m_aggrSrcPxs[2] = vwapAC;

    return synthPxCB;
  }

  //=========================================================================//
  // "MkSynthPxAC":                                                          //
  //=========================================================================//
  template<ExchC0T X>
  template<bool    IsBid>
  double TriArbC0<X>::MkSynthPxAC(TrInfo* a_tri) const
  {
    // Check the Passive and Aggr Legs (I=I0=AC, I1=AB, I2=CB):
    assert(a_tri != nullptr                         &&
           a_tri->m_passI         == AC             &&
           a_tri->m_passS         == int(IsBid)     &&
           a_tri->m_passInstr     == m_instrs[AC]   &&
           a_tri->m_passQty       == m_qtys  [AC]   &&
           a_tri->m_aggrIs    [0] == AC             &&
           a_tri->m_aggrIs    [1] == AB             &&
           a_tri->m_aggrIs    [2] == CB             &&
           a_tri->m_aggrSs    [0] == int(!IsBid)    &&
           a_tri->m_aggrSs    [1] == int (IsBid)    &&
           a_tri->m_aggrSs    [2] == int(!IsBid)    &&
           a_tri->m_aggrInstrs[0] == m_instrs[AC]   &&
           a_tri->m_aggrInstrs[1] == m_instrs[AB]   &&
           a_tri->m_aggrInstrs[2] == m_instrs[CB]);

    // AB: Same side as AC:
    QtyN      qAC = m_qtys[AC];
    QtyN      qAB = qAC;
    QtyN    volAB = qAB * m_resrvFactors[AB];
    PriceT vwapAB =
      m_orderBooks[AB]->GetVWAP1<QTO,QRO,IsBid,QT,QR,true>(volAB);

    // CB: Opposite side to AB:
    QtyN     volB = qAB * (double(vwapAB) * m_resrvFactors[CB]);
    QtyN    volCB = SolveLiquidityEqn<!IsBid>(m_orderBooks[CB], volB);
    QtyN      qCB = volCB / m_resrvFactors[CB];
    PriceT vwapCB = PriceT(volB  / volCB);

    double synthPxAC = volCB / (qAB * m_resrvFactors[CB]);
    assert(IsFinite(synthPxAC) && synthPxAC > 0.0);

    // Store the src data in "TrInfo" (I=I0=AC, I1=AB, I2=CB):
    a_tri->m_aggrQtys  [0] = qAC;
    a_tri->m_aggrQtys  [1] = qAB;
    a_tri->m_aggrQtys  [2] = qCB;
    a_tri->m_aggrVols  [0] = qAC;               // Does not matter much
    a_tri->m_aggrVols  [1] = volAB;
    a_tri->m_aggrVols  [2] = volCB;
    a_tri->m_aggrSrcPxs[0] = PriceT(synthPxAC); // Does not matter much
    a_tri->m_aggrSrcPxs[1] = vwapAB;
    a_tri->m_aggrSrcPxs[2] = vwapCB;

    // All Done:
    return synthPxAC;
  }
} // End namespace MAQUETTE
