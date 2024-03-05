// vim:ts=2:et
//===========================================================================//
//                        "InfraStruct/RiskMgrTypes.cpp":                    //
//                    Types Used by "RiskMgr" and its Clients                //
//===========================================================================//
#include "InfraStruct/RiskMgr.h"
#include "Basis/Maths.hpp"
#include "Basis/Macros.h"
#include "Basis/TimeValUtils.hpp"
#include <utxx/compiler_hints.hpp>
#include <utxx/error.hpp>
#include <cassert>

namespace
{
  // For Junk Coins or similar:
  constexpr double TrivialAssetEvalRate = 1e-9;
}

namespace MAQUETTE
{
  using namespace std;

  //=========================================================================//
  // "InstrRisks" Default Ctor:                                              //
  //=========================================================================//
  InstrRisks::InstrRisks()
  : m_outer             (nullptr),
    m_userID            (0),
    m_instr             (nullptr),
    m_ob                (nullptr),
    m_risksA            (nullptr),
    m_risksB            (nullptr),
    m_lastRateB         (NaN<double>),
    m_ts                (),
    m_posA              (0.0),
    m_avgPosPxAB        (0.0),
    m_realisedPnLB      (0.0),
    m_unrPnLB           (0.0),
    m_realisedPnLRFC    (0.0),
    m_apprRealPnLRFC    (0.0),
    m_unrPnLRFC         (0.0),
    m_activeOrdsSzA     (0.0),
    m_activeOrdsSzRFC   (0.0),
    m_ordsCount         (0),
    m_initAttempts      (0)
  {
    assert(IsEmpty());
  }

  //=========================================================================//
  // "InstrRisks::IsEmpty":                                                  //
  //=========================================================================//
  bool InstrRisks::IsEmpty() const
  {
    // The main Emptiness Flag:
    bool res = (m_outer == nullptr);

    CHECK_ONLY
    (
      // The following conds should be equivalent to Emptiness:
      if (utxx::unlikely
         (res != (m_instr  == nullptr) || res != (m_risksA == nullptr)    ||
          res != (m_risksB == nullptr)))
        throw utxx::logic_error
          ("InstrRisks::IsEmpty: Inconsistent Outer/Instr/Risks{A,B} settings");

      // The following conds are icompatible with Emptiness:
      if (utxx::unlikely
         (res                                                &&
         (m_userID != 0          || m_ob != nullptr          ||
         AssetRisks::IsValidRate(m_lastRateB)                ||
         !m_ts.empty()           || m_posA            != 0.0 ||
         IsFinite(m_avgPosPxAB)  || m_realisedPnLB    != 0.0 ||
         m_unrPnLB        != 0.0 || m_realisedPnLRFC  != 0.0 ||
         m_apprRealPnLRFC != 0.0 || m_unrPnLRFC       != 0.0 ||
         m_activeOrdsSzA  != 0.0 || m_activeOrdsSzRFC != 0.0 ||
         m_ordsCount      != 0 )))
        throw utxx::logic_error
          ("InstrRisks::IsEmpty: Empty settings but non-empty data");
    )
    return res;
  }

  //=========================================================================//
  // "InstrRisks" Non-Default Ctor:                                          //
  //=========================================================================//
  InstrRisks::InstrRisks
  (
    RiskMgr       const* a_outer,     // Must be non-NULL
    UserID               a_user_id,
    SecDefD       const& a_instr,
    OrderBookBase const* a_ob,
    AssetRisks*          a_ar_a,      // Must be non-NULL
    AssetRisks*          a_ar_b       // ditto
  )
  : m_outer             (a_outer),
    m_userID            (a_user_id),
    m_instr             (&a_instr),
    m_ob                (a_ob),
    m_risksA            (a_ar_a),
    m_risksB            (a_ar_b),
    m_lastRateB         (NaN<double>),
    m_ts                (),
    m_posA              (0.0),
    m_avgPosPxAB        (0.0),
    m_realisedPnLB      (0.0),
    m_unrPnLB           (0.0),
    m_realisedPnLRFC    (0.0),
    m_apprRealPnLRFC    (0.0),
    m_unrPnLRFC         (0.0),
    m_activeOrdsSzA     (0.0),
    m_activeOrdsSzRFC   (0.0),
    m_ordsCount         (0),
    m_initAttempts      (0)
  {
    // Check what we got. NB: In fld inits above, we guard against segfault if
    // a_outer=NULL, but we actually disallow that:
    // Ptrs to the RiskMgr and AssetRisks must be non-NULL:
    if (utxx::unlikely
       (m_outer  == nullptr || m_risksA == nullptr || m_risksB == nullptr))
      throw utxx::badarg_error
            ("InstrRisks::Ctor: RiskMgr and AssetRisks Ptrs must be non-NULL");

    // XXX: Normally, the OrderBook ptr MUST be present. However, in STP mode,
    // "InstrRisks" can work (with reduced functionality) even without it: The
    // UnRealised PnL will not be available:
    if (utxx::unlikely(m_ob == nullptr))
    {
      LOGA_WARN(m_outer->RiskMgr, 1,
        "InstrRisks::Ctor: {}: No OrderBook: UnRealised PnL will NOT be "
        "available", m_instr->m_FullName.data())
    }
    else
    {
      // Generic Case: With OrderBook:
      // IMPORTANT: As mentioned above, it is NOT required that the OrderBook
      // specified here must be for the specified Instr: if the former is not
      // "liquid" enough, an alternative src of MktData may be provided. How-
      // ever, the OrderBook and Instr must be consistent  in terms of Assets
      // and SettlDates:
      //
      SecDefD const& obInstr = m_ob->GetInstr();
      if (utxx::unlikely
         (m_instr->m_AssetA    != obInstr.m_AssetA   ||
          m_instr->m_QuoteCcy  != obInstr.m_QuoteCcy ||
          m_instr->m_SettlDate != obInstr.m_SettlDate))
        throw utxx::badarg_error
              ("InstrRisks::Ctor: Inconsistency: ExplInstr=",
               m_instr->m_FullName.data(), ", FromOrderBook=",
               obInstr. m_FullName.data(),
               ": Asset, QuoteCcy and/or SettlDate do not match");
    }
    // The obj is of course non-empty:
    assert(!IsEmpty());
  }

  //=========================================================================//
  // "InstrRisks::ResetTransient":                                           //
  //=========================================================================//
  // We currently reset the following TRANSIENT flds:
  // (*) The OrderBook ptr which may become invalid after a process restart
  //     (it points into conventional memory, whereas the RiskMgr  objs are
  //     located in ShM), so it can be replaced with "a_new_ob"  (or NULL);
  // (*) ArctiveOrdSzs: most likely, those Orders have been cancelled during
  //     the process restart;
  // (*) The previous Logger ptr is not valid either:
  //
  void InstrRisks::ResetTransient(OrderBookBase const* a_new_ob)
  {
    m_ob              =  a_new_ob;     // May be NULL
    m_activeOrdsSzA   =  RMQtyA();
    m_activeOrdsSzRFC =  0.0;
    m_initAttempts    =  0;
  }

  //=========================================================================//
  // "InstrRisks::OnMktDataUpdate":                                          //
  //=========================================================================//
  // Using the MID-POINT px, updates UnRPnLB, UnRPnLRFC and RealisedPnLRFC. It
  // does NOT update ActiveOrdsSzRFC, because the latter requires (A/RFC) rate.
  // NB: This method is NOT called in the Realxed mode!
  //
  void InstrRisks::OnMktDataUpdate
  (
    OrderBookBase const& a_ob,
    PriceT               a_bid_px_ab,
    PriceT               a_ask_px_ab,
    utxx::time_val       a_ts
  )
  const
  {
    //-----------------------------------------------------------------------//
    // Checks:                                                               //
    //-----------------------------------------------------------------------//
    CHECK_ONLY
    (
      // This "InstrRisks" must NOT be empty:
      if (utxx::unlikely(IsEmpty()))
        throw utxx::logic_error("InstrRisks::OnMktDataUpdate: Empty");
    )
    // NB: Invocation Frequency Control is performed by the Caller (RiskMgr::
    // OnMktDataUpdate).   The Bid and Ask Pxs have already been computed and
    // verified by the Caller:
    assert(a_bid_px_ab < a_ask_px_ab);
    assert(m_outer != nullptr && m_risksA != nullptr && m_risksB != nullptr);

    // The following invariant must hold: m_posA==0 => m_unrPnLB==0:
    assert(m_posA != 0.0 || m_unrPnLB == 0.0);

    //-----------------------------------------------------------------------//
    // VarRate(B) and RefPx(A/B):                                            //
    //-----------------------------------------------------------------------//
    // The B/RFC Valuation Rate: Get it w/o re-evaluation (if updated, its new
    // val would already be memoised in "m_risksB"): FullReCalc=false:
    //
    double rateB  = m_risksB->GetValuationRate<false>(a_ts);
    bool   rateOK = AssetRisks::IsValidRate(rateB);

    // RefPx used in UnRPnLcomputation: Only computed below when we know that
    // this OrderBook is applicable:
    bool   refPxIsBid  = false;
    PriceT refPx;               // NaN
    bool doneSomething = false; // Not yet...

    //-----------------------------------------------------------------------//
    // UnRealisedPnL:                                                        //
    //-----------------------------------------------------------------------//
    // IMPORTANT: First, check whether the given OrderBook provides UnRealised
    // PnL for this Instrument (it may or may not do that!):
    //
    if (m_ob == &a_ob)
    {
      // Yes, this OrderBook is applicable to compute the RefPx: The L1 Aggres-
      // sive Liquidation Px (XXX: currently NOT a VWAP), so it may be somewhat
      // optimistic:
      // Long  Pos: use L1 Bid Px;
      // Short Pos: use L1 Ask Px:
      refPxIsBid    = (m_posA > 0.0);
      refPx         = refPxIsBid ? a_bid_px_ab : a_ask_px_ab;
      doneSomething = true;     // Because "refPx" itself is a result!

      if (m_posA != 0.0)
      {
        // Yes, update the UnRPnLB:
        m_unrPnLB     = RMQtyB  (double(m_posA) * (refPx -  m_avgPosPxAB));
        m_ts          = a_ts;

        // Since VBM has changed, we need to re-evaluate it in RFC, provided
        // that the rate is available (XXX: otherwise, the prev stale RFC value
        // will be preserved until the rate appears):
        if (rateOK)
          m_unrPnLRFC = double(m_unrPnLB) * rateB;
      }
    }
    //-----------------------------------------------------------------------//
    // B-Denominated Vals -> RFC:                                            //
    //-----------------------------------------------------------------------//
    // It is possible that this OrderBook was (also) registered as a Valuator
    // for AssetB. In that case, perform RFC re-evaluation of UnRPnL and Real-
    // ised PnL. It's OK if UnRPnLRFC was already evaled above, as it is not
    // additive (unlike appreciations!):
    //
    if (rateOK && AssetRisks::IsValidRate(m_lastRateB) && m_lastRateB != rateB)
    {
      m_realisedPnLRFC = rateB * double(m_realisedPnLB);
      m_ts             = a_ts;
      doneSomething    = true;
      // Also update the Appreciation:
      m_apprRealPnLRFC += double(m_realisedPnLB) * (rateB - m_lastRateB);
    }
    //-----------------------------------------------------------------------//
    // Also: A-Denominated Vals -> RFC Valuation:                            //
    //-----------------------------------------------------------------------//
    // Currently, this only applies to "ActiveOrdsSzA" only, and thus only to
    // non-STP mode:
    if (utxx::unlikely
       (m_outer->m_mode != RMModeT::STP && m_activeOrdsSzA != 0.0 &&
        m_risksA->IsApplicableOrderBook(a_ob)))
    {
      // NB: "m_activeOrdsSzRFC": unchanged if Rate(A/RFC) is not available:
      // Again, FullReCalc=false:
      (void) m_risksA->ToRFC<false>
        (double(m_activeOrdsSzA), a_ts, &m_activeOrdsSzRFC);
      m_ts          = a_ts;
      doneSomething = true;
    }
    //-----------------------------------------------------------------------//
    // Memoise the Last (B/RFC) Rate:                                        //
    //-----------------------------------------------------------------------//
    if (rateOK)
    {
      m_lastRateB = rateB;
      m_ts        = a_ts;  // Once again, just in case...
    }
    //-----------------------------------------------------------------------//
    // Call-Back Invocation:                                                 //
    //-----------------------------------------------------------------------//
    // BEWARE: It's OK to invoke it even if "rateB" was unavailable, but since
    // this is an MtM Event, "refPx" MUST be available:
    //
    if (doneSomething && IsFinite(refPx))
    {
      InstrRisksUpdateCB const* irUpdtCB = m_outer->m_irUpdtCB;

      if (utxx::likely(irUpdtCB != nullptr && (*irUpdtCB)))
        // NB:
        // PrevPos     is the same as "m_posA"
        // TradeID     is empty
        // IsBuy       is the side of the RefPx
        // PxAB        is RefPx
        // QtyA        is 0, and this implies MtM
        // RealGainB   is 0
        // RealGainRFC is 0 as well:
        //
        (*irUpdtCB)(*this, m_posA, "", refPxIsBid, refPx, RMQtyA(), RMQtyB(),
                    rateB, 0.0);
    }
    // All Done!
  }

  //=========================================================================//
  // "InstrRisks::OnTrade":                                                  //
  //=========================================================================//
  double InstrRisks::OnTrade
  (
    ExchOrdID const&  a_trade_id, // For propagation into Call-Back only
    bool              a_is_buy,
    PriceT            a_px_ab,   // Px(A/B) itself
    RMQtyA            a_qty_a,   // Qty in units of A
    RMQtyB            a_fee_b,   // Trading Fee / Commission
    utxx::time_val    a_ts       // ExchTS or (if unavail) RecvTS
  )
  {
    //-----------------------------------------------------------------------//
    // Checks:                                                               //
    //-----------------------------------------------------------------------//
    CHECK_ONLY
    (
      if (utxx::unlikely(IsEmpty()))
        throw utxx::logic_error("InstrRisks::OnTrade: Empty");
    )
    assert(IsFinite(a_px_ab)   && IsPos(a_qty_a) && m_outer != nullptr &&
           m_risksA != nullptr && m_risksB != nullptr);

    RMModeT mode = m_outer->m_mode;

    //-----------------------------------------------------------------------//
    // New position:                                                         //
    //-----------------------------------------------------------------------//
    // NB: "basePosA" is the "old" pos (in A) which is used in re-calculation
    // of AvgPosPx. It there was 0 crossing, it will be reset to 0 (see below):
    RMQtyA oldPosA   = m_posA;
    RMQtyA basePosA  = oldPosA;
    RMQtyA deltaPosA = a_is_buy ? a_qty_a : (- a_qty_a);
    m_posA += deltaPosA;

    // We will return "deltaPosB", which is the change in Pos(B)  due  to this
    // Trade:  NB: The main term has the opposite sign to "deltaPosA";  "FeeB"
    // is normally >= 0, may may also be negative (a Rebate):
    //
    RMQtyB deltaPosB(- double(deltaPosA) * double(a_px_ab));

    // Compute the Trade Size in RFC (before Fees). It's better to use "delta-
    // PosB" for that, as we will need "rateB" is the following as well.   NB:
    // It may be NaN is some rare cases when "rateB" is not available:
    //
    double rateB      = m_risksB->GetValuationRate<false>(a_ts);
    bool   rateOK     = AssetRisks::IsValidRate(rateB);
    double tradeSzRFC =
      utxx::likely(rateOK)
      ? Abs(double(deltaPosB) * rateB)   // NB: Abs!
      : NaN<double>;

    // Only now, apply the Fees:
    deltaPosB -= a_fee_b;

    //-----------------------------------------------------------------------//
    // Update the "AssetRisks" for A and B:                                  //
    //-----------------------------------------------------------------------//
    m_risksA->OnTrade(a_trade_id, double(deltaPosA), a_ts);
    m_risksB->OnTrade(a_trade_id, double(deltaPosB), a_ts);

    // The rest is NOT required if we are in the Relaxed mode:
    if (mode == RMModeT::Relaxed)
      return tradeSzRFC;

    //-----------------------------------------------------------------------//
    // Effects of moving "pos" towards 0 and/or away from 0:                 //
    //-----------------------------------------------------------------------//
    // "redQty" is part of "qty" which contributed to position reduction twrds
    // 0. Ie we consider 2 cases:
    // (1) "m_posA" is of the same sign  (or became  0) as "oldPosA";
    // (2) "m_posA" has changed sign (passed through 0) compared to "oldPosA":
    // NB: simple condition |newPosA| < |oldPosA| can NOT be  used for detec-
    // ting pos reductions, because a position can cross 0.
    //
    // Similarly, "awayQty" is part of "qty" which resulted in position moving
    // away from 0. There are 3 possible cases:
    // (1) qty == redQty :          pos just  moved towards 0;
    // (2) qty == awayQty:          pos just  moved towards 0;
    // (3) qty == redQty + awayQty: pos first moved towards 0, crossed it,
    //     and then moved away from 0; note that it CANNOT happen in the oppos-
    //     ite order.
    // IMPORTANT: When "pos" shrinks towards 0, RealizedPnL is updated, BUT the
    // AvgPosPx of the remaining pos (if non-0) remains the same. Thus, once we
    // got a position on one side, all further updates  of AvgPosPx  will occur
    // due to trades on THAT side only; trades on the opposite side will affect
    // the PnL but NOT the AvgPosPx, and that will continue  until  pos returns
    // to 0:
    RMQtyA redQtyA =
      (oldPosA > 0.0  && m_posA  < oldPosA)
      ? (oldPosA   -  Max(RMQtyA(), m_posA))   // NB: "max" for 0-cross
      :
      (oldPosA < 0.0  && m_posA  > oldPosA)
      ? (Min(m_posA,  RMQtyA())  - oldPosA)    // NB: "min" for 0-cross
      : RMQtyA();
     assert
        (!IsNeg(redQtyA)  && redQtyA  <= a_qty_a && redQtyA  <= Abs(oldPosA));

     RMQtyA awayQtyA = a_qty_a - redQtyA;
     assert
        (!IsNeg(awayQtyA) && awayQtyA <= a_qty_a && awayQtyA <= Abs(m_posA));

    //-----------------------------------------------------------------------//
    // Moving towards 0: Affects RealisedPnL:                                //
    //-----------------------------------------------------------------------//
    // Trade Gain (in B and RFC). Always includes the Fee/Commission (typically
    // a negative term). Its main part is non-0 only for such "Closing" trades:
    //
    RMQtyB deltaPnLB = - a_fee_b;
    if (IsPos(redQtyA))
    {
      // Update RealizedPnLB but NOT AvgPosAPx:
      assert(oldPosA != 0.0);

      // The change in posA made by "redQtyA":
      RMQtyA deltaPosARed = a_is_buy ? redQtyA : (- redQtyA);

      // It is valid to say that the "deltaPosRed" now dissipated, was previou-
      // sly acquired at "m_avgPosAPx"; so the Realized PnL impact will be the
      // following, in the units of B:
      deltaPnLB +=
        RMQtyB(double(deltaPosARed) * double(m_avgPosPxAB - a_px_ab));

      // XXX: If "m_posA" has arrived at 0 or crosses 0, AvgPosPxs is NOT reset
      // to 0, but becomes irrelevant; UnRPnLB is reset because there is no Un-
      // Realised PnL anymore:
      if (redQtyA == Abs(oldPosA))
      {
        m_unrPnLB = RMQtyB();  // 0
        basePosA  = RMQtyA();  // 0; If we then re-calculate AvgPosPx,
      }                        //    newUnRPnLB remains 0

      // Update the (Cumulative) Realised PnL, with the Fee/Commission included
      // into the increment:
      m_realisedPnLB += deltaPnLB;
    }
    //-----------------------------------------------------------------------//
    // Convert the RealisedPnLB and DeltaPnLB into RFC:                      //
    //-----------------------------------------------------------------------//
    // XXX: We do so even if RealisedPnLB did not change, as we may need to re-
    // port the Rate(B/RFC) to the Call-Back. NB: RealisedPnL is converted into
    // RFC at the current B/RFC rate, so it is NOT an integral sum over the hi-
    // storical rates (as is the case of per-Asset integral RFC vals):
    // FullReCalc=false as this is NOT an MDU:
    //
    if (utxx::likely(rateOK))
      m_realisedPnLRFC = double(m_realisedPnLB) * rateB;

    // Similarly, convert the PnL Gain of this single trade, using the rate just
    // computed. It is for information only (passed to the Call-Back), not for
    // State update, so we deliberatly set it to NaN if "rateB" is unavailable:
    //
    double deltaPnLRFC =
      (utxx::likely(rateOK)) ? (double(deltaPnLB) * rateB) : NaN<double>;

    //-----------------------------------------------------------------------//
    // Moving away from 0: Affects AvgPosPx:                                 //
    //-----------------------------------------------------------------------//
    // NB: This is not mutually-exclusive with "moving towards 0": Pos can move
    // away AFTER crossing 0!
    if (IsPos(awayQtyA))
    {
      // Update AvgPosPx. Obviously, in this case new "m_posA" cannot be 0:
      assert(m_posA != 0.0);

      // NB: "basePos" is used instead of  "oldPos", since the former takes pos-
      // sible 0-crossing into account; "basePos" must be 0, or of the same sign
      // as new "m_posA":
      assert(((basePosA >= 0.0 && m_posA >= 0.0)           ||
              (basePosA <= 0.0 && m_posA <= 0.0))          &&
             (( a_is_buy && m_posA == basePosA + awayQtyA) ||
              (!a_is_buy && m_posA == basePosA - awayQtyA)));

      // Update the AvgPosPxAB, based only on the "away" Delta:
      RMQtyA deltaPosAwayA = a_is_buy ? awayQtyA : (- awayQtyA);
      m_avgPosPxAB =
        PriceT((double(basePosA)      * double(m_avgPosPxAB) +
                double(deltaPosAwayA) * double(a_px_ab))     / double(m_posA));
    }
    //-----------------------------------------------------------------------//
    // Reduce the size of Currently-Active Orders:                           //
    //-----------------------------------------------------------------------//
    // XXX: If, by any chance, we get a negative size as a result, reset it to
    // 0. Obviously, this is only applicable to non-STP modes:
    if (mode != RMModeT::STP)
    {
      m_activeOrdsSzA  -= a_qty_a;
      if (utxx::unlikely(m_activeOrdsSzA < 0.0))
        m_activeOrdsSzA = RMQtyA();

      // And re-calculate its RFC value (it is expressend in units of Asset):
      assert(m_risksA  != nullptr);
      (void) m_risksA->ToRFC<false>
        (double(m_activeOrdsSzA), a_ts, &m_activeOrdsSzRFC);
    }
    //-----------------------------------------------------------------------//
    // Call-Back Invocation:                                                 //
    //-----------------------------------------------------------------------//
    // Update the TimeStamp:
    m_ts = a_ts;

    InstrRisksUpdateCB const* irUpdtCB = m_outer->m_irUpdtCB;

    if (utxx::likely(irUpdtCB != nullptr && (*irUpdtCB)))
      (*irUpdtCB)(*this,   oldPosA, a_trade_id.ToString(), a_is_buy,
                  a_px_ab, a_qty_a, deltaPnLB,      rateB, deltaPnLRFC);
    // All Done!
    return tradeSzRFC;
  }

  //=========================================================================//
  // "AsetRisks" Default Ctor:                                               //
  //=========================================================================//
  AssetRisks::AssetRisks()
  : m_outer          (nullptr),
    m_userID         (0),
    m_asset          (),
    m_isRFC          (false),
    m_settlDate      (0),
    m_fixedEvalRate  ( NaN<double> ),
    m_isDirectRate   (),
    m_evalOB1        (nullptr),
    m_adj1           { NaN<double>, NaN<double> },
    m_rollOverTime   (),
    m_evalOB2        (nullptr),
    m_adj2           { NaN<double>, NaN<double> },
    m_lastEvalRate   ( NaN<double> ),
    m_ts             (),
    m_epoch          (),
    m_initPos        (0.0),
    m_trdDelta       (0.0),
    m_cumTranss      (0.0),
    m_cumDeposs      (0.0),
    m_cumDebt        (0.0),
    m_extTotalPos    (0.0),
    m_initRFC        (0.0),
    m_trdDeltaRFC    (0.0),
    m_cumTranssRFC   (0.0),
    m_cumDepossRFC   (0.0),
    m_cumDebtRFC     (0.0),
    m_apprInitRFC    (0.0),
    m_apprTrdDeltaRFC(0.0),
    m_apprTranssRFC  (0.0),
    m_apprDepossRFC  (0.0),
    m_initAttempts   (0)
  {
    assert(IsEmpty());
  }

  //=========================================================================//
  // "AssetRisks::IsEmpty":                                                  //
  //=========================================================================//
  bool AssetRisks::IsEmpty() const
  {
    // The main Emptiness Flag:
    bool res = (m_outer == nullptr);

    CHECK_ONLY
    (
      // The following conds should be equivalent to Emptiness:
      if (utxx::unlikely(res != MAQUETTE::IsEmpty(m_asset)))
        throw utxx::logic_error
              ("AssetRisks::IsEmpty: Inconsistent Outer/Asset settings");

      // The following conds are incompatible with Emptiness:
      if (utxx::unlikely(res &&
         (m_userID        != 0         || m_settlDate       != 0   ||
          IsValidRate(m_fixedEvalRate) || m_isDirectRate           ||
          m_evalOB1       != nullptr   || IsFinite(m_adj1[0])      ||
          IsFinite(m_adj1[1])          || !m_rollOverTime.empty()  ||
          m_evalOB1       != nullptr   || IsFinite(m_adj2[0])      ||
          IsFinite(m_adj2[1])          ||
          IsValidRate(m_lastEvalRate)  || !m_ts.empty()            ||
          !m_epoch.empty()             || m_initPos         != 0.0 ||
          m_trdDelta      != 0.0       || m_cumTranss       != 0.0 ||
          m_cumDeposs     != 0.0       || m_cumDebt         != 0.0 ||
          m_extTotalPos   != 0.0       || m_initRFC         != 0.0 ||
          m_trdDeltaRFC   != 0.0       || m_cumTranssRFC    != 0.0 ||
          m_cumDepossRFC  != 0.0       || m_cumDebtRFC      != 0.0 ||
          m_apprInitRFC   != 0.0       || m_apprTrdDeltaRFC != 0.0 ||
          m_apprTranssRFC != 0.0       || m_apprDepossRFC   != 0.0)))
        throw utxx::logic_error
              ("AssetRisks::IsEmpty: Empty settings but non-empty data");
    )
    return res;
  }

  //=========================================================================//
  // "AssetRisks" Non-Default Ctor:                                          //
  //=========================================================================//
  // NB: Similar to "InstrRisks" Non-Default Ctor, we guard against Outer=NULL
  // here, but this cond is then checked anyway below:
  //
  AssetRisks::AssetRisks
  (
    RiskMgr const*   a_outer,
    UserID           a_user_id,
    SymKey  const&   a_asset,
    int              a_settl_date
  )
  : m_outer          (a_outer),
    m_userID         (a_user_id),
    m_asset          (a_asset),
    m_isRFC          ((m_outer != nullptr)
                      ? strcmp(a_asset.data(), m_outer->m_RFC.data()) == 0
                      : false),
    m_settlDate      (a_settl_date),
    m_fixedEvalRate  (m_isRFC ? 1.0 : TrivialAssetEvalRate),
    m_isDirectRate   (true),
    m_evalOB1        (nullptr),
    m_adj1           { NaN<double>, NaN<double> },
    m_rollOverTime   (),
    m_evalOB2        (nullptr),
    m_adj2           { NaN<double>, NaN<double> },
    m_lastEvalRate   ( NaN<double> ),
    m_ts             (),
    m_epoch          (),
    m_initPos        (0.0),
    m_trdDelta       (0.0),
    m_cumTranss      (0.0),
    m_cumDeposs      (0.0),
    m_cumDebt        (0.0),
    m_extTotalPos    (0.0),
    m_initRFC        (0.0),
    m_trdDeltaRFC    (0.0),
    m_cumTranssRFC   (0.0),
    m_cumDepossRFC   (0.0),
    m_cumDebtRFC     (0.0),
    m_apprInitRFC    (0.0),
    m_apprTrdDeltaRFC(0.0),
    m_apprTranssRFC  (0.0),
    m_apprDepossRFC  (0.0),
    m_initAttempts   (0)
  {
    // Verify the settings. NB: SettlDate==0 is allowed:
    if (utxx::unlikely
       (m_outer == nullptr || MAQUETTE::IsEmpty(m_asset) ||
       (m_settlDate != 0   && m_settlDate < EarliestDate)))
      throw utxx::badarg_error("AssetRisks::Ctor: Invalid arg(s)");

    // The constructed "AssetRisks" obj is not empty, but by default, it is
    // endowed with a Trivial Valuator only:
    assert(!IsEmpty() && (m_isRFC || HasTrivialValuator()));
  }

  //=========================================================================//
  // "AssetRisks::ResetValuator":                                            //
  //=========================================================================//
  // Removes all TRANSIENT ptrs (incl the Logger)  which may   otherwise become
  // dangling, sets to NaN or 0 all transient data which would otherwise become
  // stale, but preserves the object validity, Position, and ptrs to "TotalAss-
  // etRisks":
  // NB: Do NOT reset "m_lastEvalRate", or Appreciations could become disconti-
  // nuous!
  void AssetRisks::ResetValuator()
  {
    // NB: Do NOT reset "m_lastEvalRate", or Appreciations could become discon-
    // tinuous! IsRFC flag is to be honoured:
    m_fixedEvalRate  = m_isRFC ? 1.0 : TrivialAssetEvalRate;
    m_isDirectRate   = true;
    m_evalOB1        = nullptr;
    m_adj1[0]        = NaN<double>;
    m_adj1[1]        = NaN<double>;
    m_rollOverTime   = utxx::time_val();
    m_evalOB2        = nullptr;
    m_adj2[0]        = NaN<double>;
    m_adj2[1]        = NaN<double>;
    m_initAttempts   = 0;
  }

  //=========================================================================//
  // "AssetRisks::HasTrivialValuator":                                       //
  //=========================================================================//
  bool AssetRisks::HasTrivialValuator() const
    { return m_fixedEvalRate == TrivialAssetEvalRate; }

  //=========================================================================//
  // "AssetRisks::InstallValuator" (OrderBooks):                             //
  //=========================================================================//
  void AssetRisks::InstallValuator
  (
    OrderBookBase const* a_ob1,
    double               a_bid_adj1,
    double               a_ask_adj1,
    utxx::time_val       a_roll_over_time,
    OrderBookBase const* a_ob2,
    double               a_bid_adj2,
    double               a_ask_adj2
  )
  {
    //-----------------------------------------------------------------------//
    // Verify the Args:                                                      //
    //-----------------------------------------------------------------------//
    // If this Asset is RFC itself, do nothing:
    if (utxx::unlikely(m_isRFC))
      return;

    // The LHS must not be Empty:
    CHECK_ONLY
    (
      if (utxx::unlikely(IsEmpty()))
        throw utxx::badarg_error
              ("AssetRisks::Installvaluator(OB): Empty AssetRisks");
    )
    // If it does have a Valuator already installed, produce a warning but go
    // ahead:
    if (utxx::unlikely(!HasTrivialValuator()))
      LOGA_WARN(m_outer->RiskMgr, 2,
        "AssetRisks::InstallValuator(OB): Asset={}, UserID={}: OverWriting a "
        "previously-installed Valuator", m_asset.data(), m_userID)

    // XXX: The following checks may be performed repeatedly if this method is
    // invoked multiple times with the same args but  on different "AssetRisks"
    // objs -- but this is OK since that is only done at the system init time:
    //
    // The 1st OrderBook must always be present;
    // The 2nd OrderBook is present (or otherwise) together with the RollOver
    // Time:
    if (utxx::unlikely
       (a_ob1 == nullptr || (a_ob2 == nullptr) != a_roll_over_time.empty()))
      throw utxx::badarg_error
            ("RiskMgr::InstallValuator(OB) for ", m_asset.data(),
             ": Invalid OrderBook(s)#1,2");

    // If both OrderBooks are specified, their Instruments must have same
    // Asset(A) and QuoteCcy(B) flds:
    SecDefD const&  instr1 = a_ob1->GetInstr();
    SymKey          a1     = instr1.m_AssetA;
    Ccy             b1     = instr1.m_QuoteCcy;
    assert(!MAQUETTE::IsEmpty(a1) && !MAQUETTE::IsEmpty(b1) &&
           strcmp(a1.data(), b1.data()) != 0);

    // If "adjustments" are specified, they MUST NOT narrow down the Bid-Ask
    // spread; NaNs are also not allowed:
    if (utxx::unlikely(!(a_bid_adj1 <= a_ask_adj1)))
      throw utxx::badarg_error
            ("RiskMgr::InstallValuator(OB) for ",  m_asset.data(),
             ": Invalid Bid/Ask adjustments for ", instr1.m_FullName.data(),
             ": Bid=", a_bid_adj1, ", Ask=", a_ask_adj1);

    if (a_ob2 != nullptr)
    {
      // Check that the Roll-Over Time is indeed an intra-day time:
      time_t rotSec = a_roll_over_time.sec();
      if (utxx::unlikely(rotSec <= 0 || rotSec >= 86400))
        throw utxx::badarg_error
              ("RiskMgr::InstallValuator(OB) for ", m_asset.data(),
               ": RollOverTime is not Intra-Day: ", rotSec, " sec");

      SecDefD const&  instr2 = a_ob2->GetInstr();
      SymKey          a2     = instr2.m_AssetA;
      Ccy             b2     = instr2.m_QuoteCcy;
      assert(!MAQUETTE::IsEmpty(a2) && !MAQUETTE::IsEmpty(b2) &&
             strcmp(a2.data(), b2.data()) != 0);

      if (utxx::unlikely(a1 != a2 || b1 != b2))
        throw utxx::badarg_error
              ("RiskMgr::InstallValuator(OB) for ",   m_asset.data(),
               ": Instr1=", instr1.m_FullName.data(), " and Instr2=",
               instr2.m_FullName.data(),
               " have inconsistent Assets and/or QuoteCcys");

      // Similar to Instr1, check the Bid-Ask adjustments:
      if (utxx::unlikely(!(a_bid_adj2 <= a_ask_adj2)))
        throw utxx::badarg_error
              ("RiskMgr::InstallValuator(OB) for ",   m_asset.data(),
               " : Invalid Bid/Ask adjustments for ", instr2.m_FullName.data(),
               ": Bid=", a_bid_adj2, ", Ask=", a_ask_adj2);
    }
    else
    // No OrderBook2 -- then there must be no RollOverTime and Adjs2:
    if (utxx::unlikely
       (!a_roll_over_time.empty() || a_bid_adj2 != 0.0 || a_ask_adj2 != 0.0))
      throw utxx::badarg_error
            ("RiskMgr::InstallValuator(OB) for ", m_asset.data(),
             ": Extraneous args");

    //-----------------------------------------------------------------------//
    // So can we make a Valuator for this Asset with the Args specified?     //
    //-----------------------------------------------------------------------//
    bool isDirect  =
      (a1 == m_asset) && (strcmp(b1.data(), m_outer->m_RFC.data()) == 0);

    bool isInverse =
      (strcmp(a1.data(), m_outer->m_RFC.data()) == 0) &&
      (strcmp(b1.data(), m_asset.data()) == 0);

    // Obviously, it is not possible that both "isDirect" and "isInverse" flags
    // are set, unless both Asset and QuoteCcy are RFC -- but that has already
    // been ruled out above:
    assert(!(isDirect && isInverse));

    // However, both those flags could theoretically be unset -- in this case,
    // it is an error -- no valuation is possible:
    if (utxx::unlikely(!(isDirect || isInverse)))
      throw utxx::badarg_error
            ("RiskMgr::InstallValuator(OB) for ",      m_asset.data(),
             ": OrderBook(s) not applicable: Instr1=", instr1.m_FullName.data(),
             ": Asset=", a1.data(), ", QuoteCcy=",     b1.data());

    // So:
    assert(isInverse == !isDirect);
    m_isDirectRate   =   isDirect;

    //-----------------------------------------------------------------------//
    // Install the OrderBooks and Adjustments:                               //
    //-----------------------------------------------------------------------//
    assert(a_ob1 != nullptr);

    m_evalOB1 = a_ob1;
    m_adj1[0] = a_ask_adj1;          // 0 is Ask
    m_adj1[1] = a_bid_adj1;          // 1 is Bid

    if (a_ob2 != nullptr)
    {
      m_rollOverTime = a_roll_over_time;
      m_evalOB2      = a_ob2;
      m_adj2[0]      = a_ask_adj2;   // 0 is Ask
      m_adj2[1]      = a_bid_adj2;   // 1 is Bid
    }

    // Reset the FixedRate  -- it is not applicable anymore. But again, do NOT
    // reset the LastEvalRate, or Appreciations could become discontinuous!
    m_fixedEvalRate  = NaN<double>;

    //-----------------------------------------------------------------------//
    // If we got here, the Valuator has been installed:                      //
    //-----------------------------------------------------------------------//
    // Log the details:
    char const* how = m_isDirectRate ? "DirectRate" : "InverseRate";

    if (a_ob2 == nullptr)
    {
      LOGA_INFO(m_outer->RiskMgr, 1,
        "AssetRisks::InstallValuator(OB) for {}->{} (SettlDate={}), UserID={}"
        ": Using {}: {} with adjs: ({}), ({})",
        m_asset.data(), m_outer->m_RFC.data(),    m_settlDate,
        m_userID, how,  instr1.m_FullName.data(), a_bid_adj1,  a_ask_adj1)
    }
    else
    {
      SecDefD const& instr2 = a_ob2->GetInstr();
      LOGA_INFO(m_outer->RiskMgr, 1,
        "AssetRisks::InstallValuator(OB) for {}->{} (SettlDate={}), UserID={}"
        ": Using {}: {} with adjs: ({}), ({}) and {} with adjs: ({}), ({}); "
        "RollOverTime={} sec UTC",
        m_asset.data(), m_outer->m_RFC.data(), m_settlDate, m_userID, how,
        instr1.m_FullName.data(),
        a_bid_adj1,     a_ask_adj1,   instr2.m_FullName.data(),
        a_bid_adj2,     a_ask_adj2,   a_roll_over_time.sec() )
    }
  }

  //=========================================================================//
  // "AssetRisks::InstallValuator" (Fixed Asset/RFC Rate):                   //
  //=========================================================================//
  void AssetRisks::InstallValuator(double a_fixed_rate)
  {
    // If this Asset is RFC itself, do nothing:
    if (utxx::unlikely(m_isRFC))
      return;

    // The LHS must not be Empty:
    CHECK_ONLY
    (
      if (utxx::unlikely(IsEmpty()))
        throw utxx::badarg_error
              ("AssetRisks::Installvaluator(FixedRate): Empty");
    )
    // If it does have a Valuator already installed, produce a warning but go
    // ahead:
    if (utxx::unlikely(HasTrivialValuator()))
      LOGA_WARN(m_outer->RiskMgr, 2,
        "AssetRisks::InstallValuator(FixedRate): Asset={}, UserID={}: Over"
        "Writing a previously-installed Valuator", m_asset.data(), m_userID)

    // Otherwise: Verify the Fixed Rate:
    if (utxx::unlikely(!IsFinite(a_fixed_rate) || a_fixed_rate <= 0.0))
      throw utxx::badarg_error
            ("AssetRisks::InstallValuator(FixedRate) for ", m_asset.data(),
             ": Invalid Rate: ", a_fixed_rate);

    // This rate is supposed to be Direct:
    m_isDirectRate  = true;
    m_fixedEvalRate = a_fixed_rate;

    // NB: Do NOT change LastEvalRate to avoid discontinuities...
    LOGA_INFO(m_outer->RiskMgr, 1,
      "AssetRisks::InstallValuator(FixedRate) for {}->{} (SettlDate={}), "
      "UserID={}: Using a FixedRate={}",
      m_asset.data(), m_outer->m_RFC.data(), m_settlDate, m_userID,
      m_fixedEvalRate)
  }

  //=========================================================================//
  // "AssetRisks::InstallValuator" (copy from another "AssetRisks"):         //
  //=========================================================================//
  void AssetRisks::InstallValuator(AssetRisks const& a_ar)
  {
    // If this Asset is RFC itself, do nothing:
    if (utxx::unlikely(m_isRFC))
      return;

    // The RHS must be Ready, and the LHS must NOT be Empty. They must also be
    // compatible in Asset and SettlDate. This is to be ensured by the CallER:
    if (utxx::unlikely
       (IsEmpty () || a_ar.IsEmpty() || a_ar.m_settlDate != m_settlDate ||
        strcmp(a_ar.m_asset.data(), m_asset.data()) != 0))
      throw utxx::badarg_error
            ("AssetRisks::InstallValuator(AR): Invalid/Incompatible LHS/RHS");

    // OK, copy the data over (XXX: do NOT change LastEvalRate to avoid discon-
    // tinuities):
    m_fixedEvalRate = a_ar.m_fixedEvalRate;
    m_isDirectRate  = a_ar.m_isDirectRate;
    m_evalOB1       = a_ar.m_evalOB1;
    m_adj1[0]       = a_ar.m_adj1[0];
    m_adj1[1]       = a_ar.m_adj1[1];
    m_rollOverTime  = a_ar.m_rollOverTime;
    m_evalOB2       = a_ar.m_evalOB2;
    m_adj2[0]       = a_ar.m_adj2[0];
    m_adj2[1]       = a_ar.m_adj2[1];

    LOGA_INFO(m_outer->RiskMgr, 1,
      "AssetRisks::InstallValuator(FixedRate) for {}->{} (SettlDate={}): "
      "UserID={} copied from UserID={}",
      m_asset.data(), m_outer->m_RFC.data(), m_settlDate, m_userID,
      a_ar.m_userID)
  }

  //=========================================================================//
  // "AssetRisks::OnValuatorTick" (similar to "OnMktDataUpdate"):            //
  //=========================================================================//
  void AssetRisks::OnValuatorTick
  (
    OrderBookBase const& a_ob,
    utxx::time_val       a_ts
  )
  const
  {
    CHECK_ONLY
    (
      // This "AssetRisks" must NOT be empty:
      if (utxx::unlikely(IsEmpty()))
        throw utxx::logic_error("AssetRisks::OnValuatorTick: Empty");

      // The OrderBook which has ticked must be a previously-configured one:
      if (utxx::unlikely(&a_ob != m_evalOB1 && &a_ob != m_evalOB2))
        throw utxx::logic_error
              ("AssetRisks::OnValuatorTick: Asset=",        m_asset.data(),
               ": Inconsistent OrderBook=", a_ob.GetInstr().m_FullName.data());

      // Furthermore, that OrderBook must be for Asset/RFC or RFC/Asset:
      SecDefD const& obInstr = a_ob.GetInstr();
      bool ok =
         (strcmp(obInstr.m_AssetA.data(),   m_asset.data())        == 0  &&
          strcmp(obInstr.m_QuoteCcy.data(), m_outer->m_RFC.data()) == 0) ||
         (strcmp(obInstr.m_AssetA.data(),   m_outer->m_RFC.data()) == 0  &&
          strcmp(obInstr.m_QuoteCcy.data(), m_asset.data())        == 0);
      if (utxx::unlikely(!ok))
        throw utxx::logic_error
              ("AssetRisks::OnValuatorTick: Inconsistency: Asset=",
               m_asset.data(),   ", RFC=",  m_outer->m_RFC.data(),
               ", OBA=", obInstr.m_AssetA.data(),
               ", OBB=", obInstr.m_QuoteCcy.data());
    )
    // First of all, this Valuator OrderBook  may not be  applicable  to this
    // Asset at all, although it would be strange why this method got invoked
    // then:
    if (!IsApplicableOrderBook(a_ob))
    {
      LOGA_WARN(m_outer->RiskMgr, 2,
        "AssetRisks::OnValuatorTick: Asset={} (SettlDate={}), UserID={}: "
        "OrderBook={} is not applicable?",
        m_asset.data(), m_settlDate, m_userID,
        a_ob.GetInstr().m_FullName.data())
      return;
    }
    // If applicable: Calculate the new Asset/RFC rate. It may or may not use
    // "a_ob", depending on "a_ts". NB: FullReCalc=true:   The Asset/RFC rate
    // will actually be updated HERE:
    //
    double rate = GetValuationRate<true>(a_ts);

    if (utxx::unlikely(!IsValidRate(rate)))
      return;  // Nothing to do...

    // Generic Case:
    // If the "rate" is valid, and is different from the prev one, integrate
    // the RFC vals over its change.
    //
    if (utxx::likely(IsValidRate(m_lastEvalRate) && rate != m_lastEvalRate))
    {
      // Update the RFCs:
      m_initRFC          = m_initPos    * rate;
      m_trdDeltaRFC      = m_trdDelta   * rate;
      m_cumTranssRFC     = m_cumTranss  * rate;
      m_cumDepossRFC     = m_cumDeposs  * rate;
      m_cumDebtRFC       = m_cumDebt    * rate;

      // Update the Appreciations incrementally (except DebtRFC which is NOT
      // appreciated):
      double dRate       = rate - m_lastEvalRate;

      m_apprInitRFC     += m_initPos   * dRate;
      m_apprTrdDeltaRFC += m_trdDelta  * dRate;
      m_apprTranssRFC   += m_cumTranss * dRate;
      m_apprDepossRFC   += m_cumDeposs * dRate;

      // Invoke the Call-Back (yes, ONLY if the Valuation was successful -- no
      // point in doing it otherwise):
      // NB:
      // TransID   is empty
      // TransType is MtM
      // Qty       is 0
      AssetRisksUpdateCB const* arUpdtCB = m_outer->m_arUpdtCB;

      if (utxx::likely(arUpdtCB != nullptr && (*arUpdtCB)))
        (*arUpdtCB)(*this, "", AssetTransT::MtM, 0.0, rate);
    }
    // HERE we memoise the new rate and the update time:
    m_lastEvalRate = rate;
    m_ts = a_ts;
    assert(IsValidRate(m_lastEvalRate));

    // All Done!
  }

  //=========================================================================//
  // "AssetRisks::OnBalanceUpdate":                                          //
  //=========================================================================//
  // Returns "true" on success, "false" on failutre (due to possible arg incon-
  // sistencies);
  //
  void AssetRisks::OnBalanceUpdate
  (
    char const*    a_trans_id,
    AssetTransT    a_trans_t,
    double         a_new_total,
    utxx::time_val a_ts_exch
  )
  {
    assert(a_trans_id != nullptr);
    CHECK_ONLY
    (
      if (utxx::unlikely(IsEmpty()))
        throw utxx::logic_error("AssetRisks::OnBalanceUpdate: Empty");
    )
    //-----------------------------------------------------------------------//
    // InitPos:                                                              //
    //-----------------------------------------------------------------------//
    // First of all, if Epoch is not yet set, try to initialize the AssetRisk
    // now:
    bool doneSomething = false;

    double rate        = GetValuationRate<false>(a_ts_exch);
    bool   isValidRate = IsValidRate(rate);

    if (utxx::unlikely
       (m_epoch.empty() && IsFinite(a_new_total)) && !a_ts_exch.empty())
    {
      m_ts              = a_ts_exch;
      m_epoch           = a_ts_exch;
      m_initPos         = a_new_total;
      m_trdDelta        = 0.0;
      m_cumTranss       = 0.0;
      m_cumDeposs       = 0.0;
      m_cumDebt         = 0.0;
      m_extTotalPos     = a_new_total;
      m_trdDeltaRFC     = 0.0;
      m_cumTranssRFC    = 0.0;
      m_cumDepossRFC    = 0.0;
      m_cumDebtRFC      = 0.0;
      m_apprInitRFC     = 0.0;
      m_apprTrdDeltaRFC = 0.0;
      m_apprTranssRFC   = 0.0;
      m_apprDepossRFC   = 0.0;

      doneSomething     = true;

      if (utxx::likely(isValidRate))
        m_initRFC       = m_initPos * rate;

      LOGA_INFO(m_outer->RiskMgr, 2,
        "AssetRisks::OnBalanceUpdate: Asset={} (SettlDate={}), UserID={}: "
        " Set InitPos={}, InitPosRFC={}, Epoch={}.{}",
        m_asset.data(), m_settlDate,  m_userID, m_initPos, m_initRFC,
        m_epoch.sec(),  m_epoch.usec())

      // Adjust TransType if not set to any meaningful value:
      if (utxx::unlikely(a_trans_t == AssetTransT::UNDEFINED))
        a_trans_t = AssetTransT::Initial;
    }
    //-----------------------------------------------------------------------//
    // Transfers:                                                            //
    //-----------------------------------------------------------------------//
    // IMPORTANT: Update AltTotalPos HERE:
    //
    double deltaBal = a_new_total - m_extTotalPos;
    m_extTotalPos   = a_new_total;

    // XXX: Tolerance to avoid rounding errors:
    if (utxx::unlikely(Abs(deltaBal) > 1e-9))
    {
      // OK, got a sizeable update. Adjust TransType based on its sign:
      if (a_trans_t == AssetTransT::TransferIn ||
          a_trans_t == AssetTransT::TransferOut)
      {
        //-------------------------------------------------------------------//
        // This is a Transfer between our Accounts:                          //
        //-------------------------------------------------------------------//
        a_trans_t =
          (deltaBal > 0.0)
          ? AssetTransT::TransferIn
          : AssetTransT::TransferOut;

        m_cumTranss  += deltaBal;
        m_ts          = a_ts_exch;
        doneSomething = true;

        if (utxx::likely(isValidRate))
          m_cumTranssRFC = m_cumTranss * rate;
      }
      else
      if (a_trans_t == AssetTransT::Deposit  ||
          a_trans_t == AssetTransT::Withdrawal)
      {
        //-------------------------------------------------------------------//
        // This is an External Deposit or Withdrawal:                        //
        //-------------------------------------------------------------------//
        a_trans_t =
          (deltaBal > 0.0)
          ? AssetTransT::Deposit
          : AssetTransT::Withdrawal;

        m_cumDeposs  += deltaBal;
        m_ts          = a_ts_exch;
        doneSomething = true;

        if (utxx::likely(isValidRate))
          m_cumDepossRFC = m_cumDeposs * rate;
      }
      else
      if (a_trans_t == AssetTransT::Borrowing ||
          a_trans_t == AssetTransT::Lending)
      {
        //-------------------------------------------------------------------//
        // This is a Debt (details unspecified):                             //
        //-------------------------------------------------------------------//
        a_trans_t =
          (deltaBal > 0.0)
          ? AssetTransT::Borrowing
          : AssetTransT::Lending;

        m_cumDebt     += deltaBal;
        m_ts          = a_ts_exch;
        doneSomething = true;

        if (utxx::likely(isValidRate))
          m_cumDebtRFC = m_cumDebt * rate;
      }
      // Any other TransTypes are NOT Transfers, so ignore them. This is NOT an
      // error!
    }
    //-----------------------------------------------------------------------//
    // If we got here and HaveDoneSomething: Invoke the Call-Back:           //
    //-----------------------------------------------------------------------//
    // NB:
    //   TransID   is empty
    //   TransType is MtM
    //   Qty       is Abs(deltaBal):
    //
    AssetRisksUpdateCB const* arUpdtCB = m_outer->m_arUpdtCB;

    if (doneSomething && arUpdtCB != nullptr && (*arUpdtCB))
      (*arUpdtCB)(*this, a_trans_id, a_trans_t, Abs(deltaBal), rate);
    // All Done!
  }

  //=========================================================================//
  // "AssetRisks::OnTrade":                                                  //
  //=========================================================================//
  void AssetRisks::OnTrade
  (
    ExchOrdID const&  a_trade_id,   // For propagation into Call-Back only
    double            a_pos_delta,  // Normally non-0
    utxx::time_val    a_ts          // ExchTS or (if unavail) RecvTS
  )
  const
  {
    // Checks:
    assert(IsFinite   (a_pos_delta));
    CHECK_ONLY
    (
      if (utxx::unlikely(IsEmpty()))
        throw utxx::logic_error("AssetRisks::OnTrade: Empty");
    )
    if (utxx::unlikely(a_pos_delta == 0.0))
    {
      LOGA_WARN(m_outer->RiskMgr, 2,
        "AssetRisks::OnTrade:  {}: TradeID={}, UserID={}: PosDelta=0, Ignored",
        m_asset.data(), a_trade_id.ToString(), m_userID)
      return;
    }
    // If OK: Apply this change to our TradingPos first:
    m_trdDelta      += a_pos_delta;
    m_ts             = a_ts;

    // Apply the corresp change to the RFC. Do NOT re-calculate the rate  if it
    // is already available (FullReCalc=false) --  it is re-calculated later in
    // "OnValuatorTick":
    double rate = ToRFC<false>(m_trdDelta, m_ts, &m_trdDeltaRFC);

    // Invoke the Call-Back in any case:
    AssetRisksUpdateCB const* arUpdtCB = m_outer->m_arUpdtCB;

    if (utxx::likely(arUpdtCB != nullptr && (*arUpdtCB)))
      (*arUpdtCB)
      (
        *this,
        a_trade_id.ToString(),
        (a_pos_delta > 0.0) ? AssetTransT::Buy : AssetTransT::Sell,
        Abs(a_pos_delta),     // Qty must be > 0
        rate
      );
    // All Done!
  }

  //=========================================================================//
  // "AssetRisks::ToRFC":                                                    //
  //=========================================================================//
  template<bool FullReCalc>
  double AssetRisks::ToRFC
    (double a_asset_qty, utxx::time_val a_now, double* a_res) const
  {
    assert(a_res != nullptr);
    CHECK_ONLY
    (
      if (utxx::unlikely(IsEmpty()))
        throw utxx::logic_error("AssetRisks::ToRFC: Empty");
    )
    // Further optimisation: If the AssetQty is 0, the res is also 0 (even if
    // there is no Valuator);   but we may STILL need to compute the Rate, so
    // proceed with the Generic Case:
    //
    double rate = GetValuationRate<FullReCalc>(a_now);

    if (utxx::likely(IsValidRate(rate)))
    {
      // OK: The "rate" is a valid multiplicative valuation factor:
      *a_res = a_asset_qty * rate;
      return rate;
    }
    else
    {
      LOGA_WARN(m_outer->RiskMgr, 2,
        "AssetRisks::ToRFC: {}->{} (SettlDate={}): ValuationRate "
        "UnAvailable: {}",
        m_asset.data(), m_outer->m_RFC.data(), m_settlDate, rate)

      // XXX: Still, if the AssetQty was 0, we set "res" to 0, even though the
      // rate was undefined:
      if (utxx::unlikely(a_asset_qty == 0.0))
        *a_res = 0.0;
      return NaN<double>;
    }
    // All Done:
    __builtin_unreachable();
  }

  //=========================================================================//
  // "AssetRisks::GetValuationRate":                                         //
  //=========================================================================//
  // May return NaN if the rate could not be computed:
  //
  template<bool FullReCalc>
  double AssetRisks::GetValuationRate(utxx::time_val a_now) const
  {
    CHECK_ONLY
    (
      if (utxx::unlikely(IsEmpty()))
        throw utxx::logic_error("AssetRisks::GetValuationRate: Empty");
    )
    // (1) First of all, if "FullReCalc" is NOT set and the previously-saved
    //     value is valid, use that one:
    if (!FullReCalc && IsValidRate(m_lastEvalRate))
      return m_lastEvalRate;

    // (2) Otherwise: If this Asset is RFC itself, no Valuator is required, and
    //     the rate is 1.0 if course. Still memoise it:
    if (utxx::unlikely(m_isRFC))
    {
      m_lastEvalRate = 1.0;
      return m_lastEvalRate;
    }
    // (3) Then, if a FixedRate is available, it takes priority over OrderBook-
    //     based methods:
    if (utxx::unlikely(IsFinite(m_fixedEvalRate)))
    {
      assert(m_isDirectRate);
      return m_fixedEvalRate;
    }
    // (4) Otherwise: Use the OrderBook(s). NB: We use GEOMETRIC Bid-Ask avera-
    //     ges, because they are stable under inversion.   Any exceptions here
    //     are caught:
    double rate = NaN<double>;

    if (m_evalOB1 != nullptr)
    try
    {
      // NB: The "rate" needs to be for Asset/RFC itself, although the OrderBook
      // may be for an instrument with non-trivial PointPxAB;
      // so adjust the rate:
      rate =
        double
          ((m_evalOB2 == nullptr || a_now - GetCurrDate() < m_rollOverTime)
           ? // Use OrderBook1:
             m_evalOB1->GetInstr().GetPxAB
               (GeomMidPx(m_evalOB1->GetBestBidPx() + m_adj1[1],   // 1 = Bid
                          m_evalOB1->GetBestAskPx() + m_adj1[0]))  // 0 = Ask
           : // Use OrderBook2:
             m_evalOB2->GetInstr().GetPxAB
               (GeomMidPx(m_evalOB2->GetBestBidPx() + m_adj2[1],   // 1 = Bid
                          m_evalOB2->GetBestAskPx() + m_adj2[0]))  // 0 = Ask
          );
      // NB: Here, unlike the FixedRate case above, the rate is not always
      // direct:
      if (!m_isDirectRate)
        rate = 1.0 / rate;
    }
    catch (...) {}

    // (5) If the rate is still invalid, try the last saved one (even if "Full-
    //     ReCalc" was requested):
    if (!IsValidRate(rate))
      rate = m_lastEvalRate;

    // (6) Return what we got, but do NOT save "rate" in "m_lastEvalRate",  as
    //     the CallER may still need the latter. The returned "rate" may STILL
    //     be invalid!
    return rate;
  }

  //=========================================================================//
  // Explicit Instances of Template Methods:                                 //
  //=========================================================================//
  template double AssetRisks::ToRFC<false>
    (double a_asset_qty, utxx::time_val a_now, double* a_res) const;

  template double AssetRisks::ToRFC<true>
    (double a_asset_qty, utxx::time_val a_now, double* a_res) const;

  template double AssetRisks::GetValuationRate<false>
    (utxx::time_val a_now) const;

  template double AssetRisks::GetValuationRate<true>
    (utxx::time_val a_now) const;
}
// End namespace MAQUETTE
