// vim:ts=2:et
//===========================================================================//
//                   "TradingStrats/MM-Single/Orders.hpp":                   //
//                  Making Passive and Aggressive Orders                     //
//===========================================================================//
#pragma once

#include "Basis/Maths.hpp"
#include "MM-Single.h"
#include <sstream>

namespace MAQUETTE {
  //=========================================================================//
  // "IPair::CancelQuotes":                                                  //
  //=========================================================================//
  template <class MDC, class OMC>
    inline void MM_Single<MDC, OMC>::IPair::CancelQuotes(
                                             utxx::time_val a_ts_exch,
                                             utxx::time_val a_ts_recv,
                                             utxx::time_val a_ts_strat) {
    for (int s = 0; s < 2; ++s)
      CancelOrderSafe<false>  // Buffered
          (m_AOSes[s], a_ts_exch, a_ts_recv, a_ts_strat);
    m_OMC->FlushOrders();
  }

  //=========================================================================//
  // "IPair::DoQuotes":                                                      //
  //=========================================================================//
  template <class MDC, class OMC>
    inline void MM_Single<MDC, OMC>::IPair::DoQuotes(utxx::time_val a_ts_exch,
                                         utxx::time_val a_ts_recv,
                                         utxx::time_val a_ts_strat) {
    //-----------------------------------------------------------------------//
    // Check the Temporal Limits for Quoting:                                //
    //-----------------------------------------------------------------------//
    if (a_ts_strat >= m_enabledUntil) {
      // Cancel existing Quotes (if any) -- and nothing else to do:
      CancelQuotes(a_ts_exch, a_ts_recv, a_ts_strat);
      return;
    }

    //-----------------------------------------------------------------------//
    // Get the Curr Best Bid and Ask:                                        //
    //-----------------------------------------------------------------------//
    PriceT bestAskPx(m_OB->GetBestAskPx());
    PriceT bestBidPx(m_OB->GetBestBidPx());

    double pxStep = (m_forcePxStep > 0.0) ? m_forcePxStep : m_Instr->m_PxStep;

    if (utxx::unlikely(!(IsFinite(bestBidPx) && IsFinite(bestAskPx)))) {
      // Dangerous evaporation of liquidity on PassInstr: Cancel active Quotes
      // (if any) for this IPair, but do NOT terminate the Strategy  or cancel
      // quotes belonging to other IPairs:
      //
      bool isBid = !IsFinite(bestBidPx);
      LOG_WARN(
          2,
          "MM-Single::IPair::DoQuotes: PassInstr={}: Insufficient Liquidity on "
          "{} side, Cancelling Both Quotes",
          isBid ? "Bid" : "Ask")

      CancelQuotes(a_ts_exch, a_ts_recv, a_ts_strat);
      return;
    }

    //-----------------------------------------------------------------------//
    // Compute Ask and Bid Quote Pxs (on the Passive side):                  //
    //-----------------------------------------------------------------------//
    // NB: All "PriceT"s are initially NaN:
    QR     qtys[2] = {0.0, 0.0};
    PriceT newQPxs[2];
    PriceT expPPxs[2];
    bool   unchPxs[2]{false, false};

    for (int s = 0; s < 2; ++s) {
      //---------------------------------------------------------------------//
      // First of all, compute the Pass Qty to be Quoted:                    //
      //---------------------------------------------------------------------//
      // XXX: Possible competitors standing in front of us, as given by "lots-
      // Ahead" computed below,  are NOT taken into account here, because that
      // would result in recursive re-calculation of VWAPs w/o any fixed-point
      // guarantees!
      bool isBid = s;

      // Determine the Qty we will Passively Quote: It is NOT always equal to
      // "m_passQty":
      qtys[s] = m_useFlipFlop
                    ?  // In the "FlipFlop" mode, we aim at altering the
                       // sign  of the curr Pass Pos:
                       // XXX: could be that m_Pos > m_Qty, and we
                       // get a perm displacement from 0 pos
                    ((m_Pos < 0 && !isBid) || (m_Pos > 0 && isBid))
                       ? (std::max<QR>(m_Qty - abs(m_Pos), 0.0))
                       : m_Qty
                    :  // In the Normal Mode, we use "m_passQty" but
                       // constrain it by the Pos Limit:
                    (isBid ? std::min<QR>(m_Qty, m_posSoftLimit - m_Pos)
                           : std::min<QR>(m_Qty, m_posSoftLimit + m_Pos));

      // If we got passQty <= MIN_SIZE, the curr side is not to be quoted,
      // and the corresp Px will remain NaN. If we compare to 0.0,
      // we might get an error from the exhange.
      // E.g. MIN_SIZE for BitFinex tBTCUSD is 0.0004:
      if (qtys[s] <= m_Instr->m_LotSize * double(m_Instr->m_MinSizeLots))
        continue;

      PriceT px = GetPxAtVol<true>(isBid, m_volTarget);
      PriceT currPx = m_quotePxs[s];

      // The Side-dependent Base Quoting Spread and the Pos-dependent adjust:
      // (*) for a Long  PassPos, we lower the Bid to make if more difficult
      //     to go yet longer, but do NOT lower Ask  --  that would result in
      //     sub-optimal Pair Spread!
      // (*) for a Short PassPos, it is opposite;
      // (*) also, align the PassPx to be a multiple of PxStep. We always move
      //     it AWAY from L1, for the following reasons:
      //     -- to avoid crossing of Bid and Ask quotes;
      //     -- to prevent P&L degradation when the PassPx is Hit/Lifted:
      //
      if (isBid) {
        px -= m_adjCoeffs[0];                    // Mark-Up!
        if (m_Pos > 0)                           // Skewing!
          px -= double(m_Pos) * m_adjCoeffs[1];  // Move Bid Down
        px.RoundDown(pxStep);
      } else {
        px += m_adjCoeffs[0];                    // As above
        if (m_Pos < 0)                           //
          px -= double(m_Pos) * m_adjCoeffs[1];  // Move Ask Up
        px.RoundUp(pxStep);
      }

      // OK: this is the calculated px which we need to get a Passive Fill at,
      // in order to be able to cover our position with the expected PnL:
      assert(IsFinite(px));
      expPPxs[s] = px;

      //---------------------------------------------------------------------//
      // Now possibly adjust the PassPx wrt the curr Target OrderBook:       //
      //---------------------------------------------------------------------//
      if (m_targAdjMode != TargAdjModeT::None) {
        // (1) If the PassPx falls inside the Bid-Ask spread of PassOrderBook,
        //     or actually becomes aggressive, we adjust it to the best L1 px,
        //     so to maximise the spread earned;
        // (2) also, in case (1) above, we could move our PassPx even beyond L1
        //     in some cases (when L1 is very thin), but this is not implemented
        //     yet (TODO: it will require  dynamic determination of what "thin"
        //     liquidity is, based on the curr trade volumes);
        // (3) alternatively, such a PassPx could be moved to the level BETTER
        //     by 1 than the curr L1, to increase the fill probability  -- the
        //     accurate decision should again be based on the L1 qty in front of
        //     us and the curr trade volumes; this is currently controlled  by
        //     the "ImproveFillRates" param:
        // NB: If "ImproveFillRates" is used and the Bid-Ask Sprd=1 PxStep, the
        //     "px" can actually become aggressive -- but that's OK; it can
        //     also cause a self-cross (handled below):
        //
        if (isBid && (bestBidPx < px))
          px = (m_targAdjMode == TargAdjModeT::ImprFillRates)
                   ? (bestBidPx + pxStep)
                   : bestBidPx;
        else if (!isBid && (px < bestAskPx))
          px = (m_targAdjMode == TargAdjModeT::ImprFillRates)
                   ? (bestAskPx - pxStep)
                   : bestAskPx;
      }

      //---------------------------------------------------------------------//
      // Apply the Resistances to avoid too frequent PassPx updates:         //
      //---------------------------------------------------------------------//
      // NB: The following only makes sense if "px" is valid. If Resistances
      // are not used, we could be charged for OverMessaging! However, this  lo-
      // gic ONLY applies to PassPxs which are BEYOND L1 (both old and new), be-
      // cause:
      // (*) if NewPassPx is L1 or better, the Fill becomes likely,  so  it is
      //     probably better to post such an Order;
      // (*) if OldPassPx was L1 is better (but not the New one), then it has
      //     to be removed, otherwise we would likely get a Fill at sub-opti-
      //     mal px;
      // (*) if both Old and New PassPxs are in the rear, don't move the Quote
      //     if the move distance is no more than the Resistance.   TODO: Make
      //     the Resistance dependent on the distance to L1 -- this will requ-
      //     ire analysis of the curr volatility etc!
      // (*) NB: the following cond is only satisfied if the existing QuotePx
      //     is Finite:
      if (IsFinite(px) && m_resistCoeff > 0.0) {
        double dist     = Abs(currPx - px);  // Dist of the proposed px move
        bool   bothRear = (isBid && currPx < bestBidPx && px < bestBidPx) ||
                        (!isBid && currPx > bestAskPx && px > bestAskPx);

        if (utxx::likely(px != currPx && bothRear)) {
          // Then the Resistance applies. The Resistance itself depends on our
          // curr distance to L1:
          double resist =
              (isBid ? (bestBidPx - currPx) : (currPx - bestAskPx)) *
              m_resistCoeff;

          if (dist < resist)
            px = currPx;  // Stay put!
        }
      }
      // So: is the price really unchanged (either because it was such by calcu-
      // lation, or forced do stay put)? NB: If "px" was not Finite at all,
      // the corresp "unchPxs" flag remains "false", because the equality fails:
      unchPxs[s] = (currPx == px);

      // In any case, if we get here, "px" must be Finite. Memoise it (NB:
      // it may be NaN!):
      newQPxs[s] = px;
    }
    // End of "s" loop

    //-----------------------------------------------------------------------//
    // Check for Self-Crossings (AskPx <= BidPx):                            //
    //-----------------------------------------------------------------------//
    // NB: The following condition can only hold if both pxs are Finite:
    //
    if (utxx::unlikely(newQPxs[Ask] <= newQPxs[Bid])) {
      // If only one px was unchanged, keep it unchanged and move the other
      // one away -- this is to avoid unnecessary moves:
      if (unchPxs[Bid] && !(unchPxs[Ask]))
        // Move the Ask up:
        newQPxs[Ask] = newQPxs[Bid] + pxStep;
      else if (unchPxs[Ask] && !(unchPxs[Bid]))
        // Move the Bid down:
        newQPxs[Bid] = newQPxs[Ask] - pxStep;
      else
        do {
          // Either both pxs are to be changed, or both unchanged. XXX: the lat-
          // ter case is extremely unlikely -- it means that there was ALREADY a
          // cross! But in any case, move the pxs symmetrically into the opposi-
          // te dirs:
          newQPxs[Ask] += pxStep;
          newQPxs[Bid] -= pxStep;
        } while (utxx::unlikely(newQPxs[Ask] <= newQPxs[Bid]));

      // So the pxs must now be valid (and both Finite):
      assert(newQPxs[Bid] < newQPxs[Ask]);
    }

    // Check target spread, both prices have to be set
    if (IsFinite(newQPxs[Ask]) && IsFinite(newQPxs[Bid])
        && m_bpsSpreadThresh > 0.0) {
      double bpsResSpread = 10000 * abs(newQPxs[Ask] - newQPxs[Bid])
        / double(newQPxs[Ask]);
      if (bpsResSpread < m_bpsSpreadThresh) {
        LOG_WARN(4,
                 "MM-Single::IPair::DoQuotes: Instr={}: Below spread thresh.",
                 m_Instr->m_Symbol.data())
        // If we don't make the spread - don't quote
        newQPxs[Ask] = PriceT();
        newQPxs[Bid] = PriceT();
      }
    }

    //-----------------------------------------------------------------------//
    // Submit the Quotes: //
    //-----------------------------------------------------------------------//
    bool done[2] = {false, false};
    bool doneCancel = false;

    for (int s = 0; s < 2; ++s) {
      bool isBid = s;

      if (IsFinite(newQPxs[s])) {
        //-------------------------------------------------------------------//
        // A New Quote (on an empty slot):                                   //
        //-------------------------------------------------------------------//
        if (utxx::unlikely(m_AOSes[s] == nullptr)) {
          // Do not place NewOrders if insufficient time has elapsed since the
          // last Passive Fill -- this can give rise to Passive Slippage:
          //
          if ((!m_lastFillTS.empty() &&
               (a_ts_strat - m_lastFillTS).milliseconds() < m_reQuoteDelayMSec))
            continue;

          // Otherwise, Go Ahead:
          assert(IsFinite(newQPxs[s]) && qtys[s] > 0);

          m_AOSes[s] = NewOrderSafe<false>  // Buffered
              (isBid, 0, newQPxs[s], expPPxs[s], NaN<double>, qtys[s],
               a_ts_exch, a_ts_recv, a_ts_strat);
          done[s] = (m_AOSes[s] != nullptr);
        } else
            //----------------------------------------------------------------//
            // Existing Quote Modification: But should we move it at all? //
            //----------------------------------------------------------------//
            if (newQPxs[s] != m_quotePxs[s]) {
          // Really try to move the order (the following call will do nothing
          // and return "false" if the order cannot be moved because it is eg
          // Part-Filled and/or being Calcelled):
          //
          done[s] = ModifyQuoteSafe<false>  // Buffered
              (isBid, newQPxs[s], expPPxs[s], qtys[s], a_ts_exch, a_ts_recv,
               a_ts_strat);
        }

        // Memorise the New Quote Px (if really quoted or re-quoted):
        if (done[s]) {
          m_quotePxs[s] = newQPxs[s];
          assert(IsFinite(m_quotePxs[s]));
        }
      } else if (m_AOSes[s] != nullptr) {
        //-------------------------------------------------------------------//
        // NewQuotePx is NaN -- Cancel an existing Quote:                    //
        //-------------------------------------------------------------------//
        // NB: "done[s]" remains unset -- there was no quote done:
        //
        CancelOrderSafe<false>  // Buffered
            (m_AOSes[s], a_ts_exch, a_ts_recv, a_ts_strat);
        doneCancel = true;
      }
    }
    // End of "s" loop

    // Flush the pending orders if any:
    bool doneSomething = done[Ask] || done[Bid] || doneCancel;
    if (doneSomething)
      m_OMC->FlushOrders();

    //-----------------------------------------------------------------------//
    // Log the Quote(s):                                                     //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely(m_debugLevel >= 2 && doneSomething)) {
      // Whether at least 1 of the Quoted Pxs is Tight (ie Fill is likely)
      bool isTight = (newQPxs[Ask] <= bestAskPx) || (newQPxs[Bid] >= bestBidPx);

      Req12 const* reqBid =
          (m_AOSes[Bid] != nullptr) ? m_AOSes[Bid]->m_lastReq : nullptr;
      Req12 const* reqAsk =
          (m_AOSes[Ask] != nullptr) ? m_AOSes[Ask]->m_lastReq : nullptr;

      PriceT actBidPx = (reqBid != nullptr) ? PriceT(reqBid->m_px) : PriceT();
      PriceT actAskPx = (reqAsk != nullptr) ? PriceT(reqAsk->m_px) : PriceT();

      OrderID actBidID = (reqBid != nullptr) ? reqBid->m_id : 0;
      OrderID actAskID = (reqAsk != nullptr) ? reqAsk->m_id : 0;

      m_logger->info("MM-Single::IPair::DoQuotes: Instr={}{}"
                     "\nQuoted: [{}]{}@{} .. {}@{}[{}]"
                     "\nBest:          {} .. {}",
                     m_Instr->m_FullName.data(), isTight ? " !!!" : "",
                     actBidID, qtys[Bid], double(actBidPx),
                     qtys[Ask], double(actAskPx), actAskID,
                     double(bestBidPx), double(bestAskPx));
      m_logger->flush();
    }
  }

  //=========================================================================//
  // "IPair::GetPxAtVol":                                                    //
  //=========================================================================//
  // Computes the price at which the specified Qty is accumulated:
  // XXX: This computation is relatively costly!
  //
  template <class MDC, class OMC>
  template <bool ReturnPrevious>
  inline PriceT MM_Single<MDC, OMC>::IPair::GetPxAtVol(
                                                bool   a_is_bid,
                                                QR     a_upto_qty) const {
    // Traverse the corresp side of the Passive OrderBook, up to the total size
    // equal to the q_upto_qty:
    QR resVols = 0;
    PriceT resPx;
    double pxStep = (m_forcePxStep > 0.0) ? m_forcePxStep : m_Instr->m_PxStep;
    auto scanner = [&resVols, &resPx, a_upto_qty]
      (int, PriceT, OrderBook::OBEntry const& a_obe) -> bool {
      // If the level is still within the range: Traverse all Orders at this
      // level (if available):
      for (OrderBook::OrderInfo const* order = a_obe.m_frstOrder;
           order != nullptr; order           = order->m_next) {
        resPx = order->m_px;
        if (utxx::likely(order->m_req12 == nullptr))
          resVols += QR(order->GetQty<QT,QR>());
        if (resVols > a_upto_qty)
          return false;
      }
      // Continue (will check exit conditions again at next PxLevel):
      return true;
    };

    // Go:
    assert(m_OB != nullptr);
    if (a_is_bid)
      m_OB->Traverse<Bid>(0, scanner);
    else
      m_OB->Traverse<Ask>(0, scanner);

    // The result:
    if (ReturnPrevious)
      resPx = a_is_bid ? resPx + pxStep : resPx - pxStep;
    return resPx;
  }

  //=========================================================================//
  // "IPair::GetOthersVolsUpTo":                                             //
  //=========================================================================//
  // Computes  the size of all Passive orders which may potentially represent
  // rivals to MM-Single. Returns the corresp size (in Base Units) of the Aggr
  // Instr:
  // XXX: This computation is relatively costly!
  //
  template <class MDC, class OMC>
  template <bool StopAtOurs>
  inline typename MM_Single<MDC, OMC>::QR
  MM_Single<MDC, OMC>::IPair::GetOthersVolsUpTo(
      bool   a_is_bid,
      PriceT a_upto_px) const {
    // Traverse the corresp side of the Passive OrderBook, up to the total size
    // equal to the avg trade size:
    QR resVols = 0;

    // XXX: The following calculation does NOT use our own quote px as a limit,
    // vecause that px is not known yet. Instead,  we use the avg  Trade  size
    // (which may lead to conservative estimates):
    auto scanner = [&resVols, a_is_bid,
                    a_upto_px](int, PriceT a_px,
                               OrderBook::OBEntry const& a_obe) -> bool {
      // All "low" px levels have been traversed?
      if (utxx::unlikely((a_is_bid && a_px < a_upto_px) ||
                         (!a_is_bid && a_px > a_upto_px)))
        return false;

      // If the level is still within the range: Traverse all Orders at this
      // level (if available):
      for (OrderBook::OrderInfo const* order = a_obe.m_frstOrder;
           order != nullptr; order           = order->m_next)
        // Our own orders are to be skipped (that's why we cannot simply use
        // the Aggregated Qtys). For all others:
        if (utxx::likely(order->m_req12 == nullptr))
          resVols += QR(order->GetQty<QT,QR>());
        else if (StopAtOurs)
          return false;

      // Continue (will check exit conditions again at next PxLevel):
      return true;
    };

    // Go:
    assert(m_OB != nullptr);
    if (a_is_bid)
      m_OB->Traverse<QtyTypeT::QtyA, QR, Bid>(0, scanner);
    else
      m_OB->Traverse<QtyTypeT::QtyA, QR, Ask>(0, scanner);

    // The result:
    return resVols;
  }

  //=========================================================================//
  // "IPair::NewOrderSafe":                                                  //
  //=========================================================================//
  // IsAggr is NOT set (Passive Order):
  // (*) "a_px1" is the new Passive   Quote Px
  // (*) "a_px2" is the Expected Pass Fill  Px (both Last and New, w/o  adjs)
  // Also:
  // (*) "a_slip" is Finite only if it is an Aggr but not Deep-Aggr Ord;
  // (*) "a_aggr_lots" is the volume (in Lots) on the Aggr side used to compute
  //     the Passive Px; it is then carried into the corresp Aggr order, so it
  //     is always set:
  //
  template <class MDC, class OMC>
  template <bool Buffered>
  inline AOS const* MM_Single<MDC, OMC>::IPair::NewOrderSafe
  (
    bool           a_is_bid,
    OrderID        a_ref_aosid,
    PriceT         a_px1,
    PriceT         a_px2,
    QR             UNUSED_PARAM(a_slip),
    QR             a_qty,
    utxx::time_val a_ts_exch,
    utxx::time_val a_ts_recv,
    utxx::time_val a_ts_strat
  )
  {
    //-----------------------------------------------------------------------//
    // Safe Mode?                                                            //
    //-----------------------------------------------------------------------//
    // Then do not even try to issue a New Order -- it will be rejected by the
    // RiskMgr anyway.  Initiate graceful shut-down instead:
    //
    if (utxx::unlikely(m_outer->m_riskMgr != nullptr &&
                       m_outer->m_riskMgr->GetMode() == RMModeT::Safe)) {
      MM_Single<MDC, OMC>::DelayedGracefulStop hmm(
          m_outer, a_ts_strat,
          "MM-Single::IPair::NewOrderSafe: RiskMgr has entered SafeMode");
      return nullptr;
    }

    //-----------------------------------------------------------------------//
    // Stopping?                                                             //
    //-----------------------------------------------------------------------//
    // If so, Passive Orders are not allowed (we should have already issued
    // Cancellation reqs for all Passive Quotes), but Aggressive Orders may
    // in theory happen:
    //
    if (utxx::unlikely(m_outer->IsStopping()))
      return nullptr;

    //-----------------------------------------------------------------------//
    // Order Too Small?                                                      //
    //-----------------------------------------------------------------------//
    // We better reject it now than wait for an exception from the Risk Mgr:
    //
    // The Instrument to be traded:
    SecDefD const* instr = m_Instr;
    assert(instr != nullptr);

    long lotSz = long(Round(instr->m_LotSize));

    if (utxx::unlikely(a_qty < double(lotSz) / 2.0))
      return nullptr;

    //-----------------------------------------------------------------------//
    // Calculate / Check the Px:                                             //
    //-----------------------------------------------------------------------//
    // NB: "px" is the actual LimitPx (Pass or Aggr) which will be used:
    PriceT px{a_px1};  // Initially NaN

    // The LimitPx must now be well-definied:
    assert(IsFinite(px));

    //-----------------------------------------------------------------------//
    // Actually issue a New Order:                                           //
    //-----------------------------------------------------------------------//
    // NB: TimeInForce is Day for BitFinex:
    FIX::TimeInForceT tif = FIX::TimeInForceT::Day;

    // Capture and log possible "NewOrder" exceptions, then terminate the Stra-
    // tegy. NB: BatchSend=Buffered:
    AOS const* res = nullptr;
    try {
      res = m_OMC->NewOrder(m_outer, *instr, FIX::OrderTypeT::Limit, a_is_bid,
                            px, false, QtyAny(a_qty, QtyTypeT::QtyA),
                            a_ts_exch, a_ts_recv, a_ts_strat, Buffered, tif);
    } catch (EPollReactor::ExitRun const&) {
      // This exception is always propagated:
      throw;
    } catch (std::exception const& exc) {
      MM_Single<MDC, OMC>::DelayedGracefulStop hmm(
          m_outer, a_ts_strat,
          "MM-Single::IPair::NewOrderSafe: Order FAILED: Instr=",
          instr->m_FullName.data(), (a_is_bid ? ", Bid" : ", Ask"), ", Px=", px,
          ", Qty=", a_qty, ": ", exc.what());
      return nullptr;
    }
    assert(res != nullptr);

    //-----------------------------------------------------------------------//
    // Install the OrderInfo:                                                //
    //-----------------------------------------------------------------------//
    // In particular, we memoise (in the AOS) the index of this "IPair" and the
    // expected Fill  Px (for both Pass and Aggr Orders):
    OrderInfo ori(a_ref_aosid,
                  short(this - m_outer->m_iPairs.data()),  // IPairID
                  a_px2);                                  // ExpPassPx
    res->m_userData.Set(ori);

    //-----------------------------------------------------------------------//
    // Log it (taking order data independently -- using Req12):              //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely(res != nullptr)) {
      Req12 const* req = res->m_lastReq;
      assert(req != nullptr && req->m_kind == Req12::KindT::New &&
             req->m_status == Req12::StatusT::New && res->m_omc != nullptr);

      LOG_INFO(3,
               "MM-Single::IPair::NewOrderSafe: OMC={}, Instr={}, NewAOSID={}, "
               "NewReq"
               "ID={}, {}, Qty={}, Px={}",
               res->m_omc->GetName(), instr->m_FullName.data(), res->m_id,
               req->m_id, res->m_isBuy ? "Bid" : "Ask",
               QR(req->GetQty<QT,QR>()), double(req->m_px))
    }
    // All Done:
    return res;
  }

  //=========================================================================//
  // "IPair::CancelOrderSafe":                                               //
  //=========================================================================//
  // For Passive and Non-Deep Aggressive Orders:
  //
  template <class MDC, class OMC>
  template <bool Buffered>
  inline void
  MM_Single<MDC, OMC>::IPair::CancelOrderSafe
  (
    AOS const*     a_aos,       // Non-NULL
    utxx::time_val a_ts_exch,
    utxx::time_val a_ts_recv,
    utxx::time_val a_ts_strat
  )
  {
    // Simply ignore non-existing Orders and other Degenerate Cases:
    // This is NOT an error:
    if (utxx::unlikely(a_aos == nullptr || a_aos->m_isInactive ||
                       a_aos->m_isCxlPending != 0))
      return;
    assert(a_aos->m_omc != nullptr);

    // Generic Case: Yes, really Cancel this Order:
    try {
      bool ok = a_aos->m_omc->CancelOrder(a_aos, a_ts_exch,
                                          a_ts_recv, a_ts_strat, Buffered);

      // "CancelOrder" should not return "false" in this case, because pre-conds
      // were satisfied:
      if (utxx::unlikely(!ok))
        throw utxx::runtime_error("CancelOrder returned False");
    } catch (EPollReactor::ExitRun const&) {
      // This exception is always propagated -- even for "Cancel":
      throw;
    } catch (std::exception const& exc) {
      // XXX: Cancellation failure is probably a critical error?
      SecDefD const*      instr = a_aos->m_instr;
      DelayedGracefulStop hmm(
          m_outer, a_ts_strat,
          "MM-Single::IPair::CancelOrderSafe: FAILED: Instr=",
          instr->m_FullName.data(), (a_aos->m_isBuy ? ", Bid" : ", Ask"),
          ", AOSID=", a_aos->m_id, ": ", exc.what());
    }

    // If OK: Log it:
    Req12 const* req = a_aos->m_lastReq;
    assert(req != nullptr && req->m_kind == Req12::KindT::Cancel);

    LOG_INFO(3,
             "MM-Single::IPair::CancelOrderSafe: OMC={}, Instr={}, {}: "
             "Cancelling AOSID={}, CancelReqID={}",
             a_aos->m_omc->GetName(), a_aos->m_instr->m_FullName.data(),
             a_aos->m_isBuy ? "Bid" : "Ask", a_aos->m_id, req->m_id)
  }

  //=========================================================================//
  // "IPair::ModifyQuoteSafe":                                               //
  //=========================================================================//
  // For Passive Quotes only -- NOT for eg Semi-Aggressive (Pegged) Covering
  // Orders:
  template <class MDC, class OMC>
  template <bool Buffered>
  inline bool MM_Single<MDC, OMC>::IPair::ModifyQuoteSafe(
      bool           a_is_bid,
      PriceT         a_px1,  // New Quote Px
      PriceT         a_px2,  // Expected  Pass Fill Px (w/o adjs)
      QR             a_qty,  // NOT in Lots!
      utxx::time_val a_ts_exch,
      utxx::time_val a_ts_recv,
      utxx::time_val a_ts_strat) {
    assert(IsFinite(a_px1) && IsFinite(a_px2));

    AOS const* aos = m_AOSes[a_is_bid];

    //-----------------------------------------------------------------------//
    // Check for Degenerate Cases:                                           //
    //-----------------------------------------------------------------------//
    // Check the AOS validity. Also, modifications are not allowed (silently
    // ignored) in the Stopping Mode (because we should have issued Cancella-
    // tion Reqs for all Quotes before entering that mode anyway):
    if (utxx::unlikely(aos == nullptr || aos->m_isInactive ||
                       aos->m_isCxlPending != 0 || m_outer->IsStopping())) {
      LOG_WARN(2, "MM-Single::ModifyQuoteSafe: Invalid AOSID={},"
               "Last ReqID={}, "
               "{}{}{}",
               aos->m_isCxlPending,
               (aos != nullptr) ? aos->m_id : 0,
               a_is_bid ? "Bid" : "Ask",
               (aos != nullptr && aos->m_isInactive) ? ": Inactive" : "",
               (aos != nullptr && aos->m_isCxlPending) ? ": CxlPending" : "")

      // If (by any chance) there is a stale non-NULL AOS ptr here, and the AOS
      // is now Inactive, reset it:
      if (aos != nullptr && aos->m_isInactive)
        m_AOSes[a_is_bid] = nullptr;
      return false;
    }

    // Also, if the new qty is too small, we better Cancel the order than Modify
    // it -- the Modify Req is likely to be rejected by the Risk Mgr anyway:
    //
    assert(aos != nullptr && aos->m_omc != nullptr && aos->m_instr != nullptr);
    SecDefD const& instr = *(aos->m_instr);
    long           lotSz = long(Round(instr.m_LotSize));

    if (utxx::unlikely(a_qty < double(lotSz) / 2.0)) {
      CancelOrderSafe<Buffered>(aos, a_ts_exch, a_ts_recv, a_ts_strat);
      return true;  // We still DID something!
    }

    //-----------------------------------------------------------------------//
    // Generic Case: Proceed with "ModifyOrder":                             //
    //-----------------------------------------------------------------------//
    try {
      // NB: IsAggr=false, BatchSend=Buffered:
      bool ok = aos->m_omc->ModifyOrder(
          aos, a_px1, false, QtyAny(a_qty, QtyTypeT::QtyA),
          a_ts_exch,  a_ts_recv, a_ts_strat, Buffered);

      // "ModifyOrder" should not return "false" in this case, because pre-conds
      // were supposed to be satisfied. But still check:
      if (utxx::unlikely(!ok))
        throw utxx::runtime_error("ModifyOrder returned False");
    } catch (EPollReactor::ExitRun const&) {
      // This exception is always propagated:
      throw;
    } catch (std::exception const& exc) {
      // If it has happened, try to cancel this Quote but still continue:
      LOG_WARN(
          2,
          "MM-Single::IPair::ModifyQuoteSafe: FAILED: Instr={}, {}, Px={}, "
          "AOSID={}: {}: Cancelling this Quote...",
          instr.m_FullName.data(), aos->m_isBuy ? "Bid" : "Ask", double(a_px1),
          aos->m_id, exc.what())

      CancelOrderSafe<Buffered>(aos, a_ts_exch, a_ts_recv, a_ts_strat);
      return false;
    }

    //-----------------------------------------------------------------------//
    // If successful: Post-Processing:                                       //
    //-----------------------------------------------------------------------//
    // Modify also the OrderInfo:
    OrderInfo& ori = aos->m_userData.Get<OrderInfo>();
    ori.m_expPx    = a_px2;

    // Log it:
    Req12 const* req = aos->m_lastReq;
    assert(req != nullptr &&
          (req->m_kind == Req12::KindT::Modify ||
           req->m_kind == Req12::KindT::ModLegN));

    LOG_INFO(
        3,
        "MM-Single::IPair::ModifyQuoteSafe: OMC={}, Instr={}, {}, AOSID={}, "
        "NewReqID={}, Qty={}, Px={}",
        aos->m_omc->GetName(), instr.m_FullName.data(),
        aos->m_isBuy ? "Bid" : "Ask", aos->m_id, req->m_id,
        QR(req->GetQty<QT,QR>()),    double(req->m_px))

    // All Done:
    return true;
  }

  //=========================================================================//
  // "GetOrderDetails":                                                      //
  //=========================================================================//
  template <class MDC, class OMC>
  inline std::pair<typename MM_Single<MDC, OMC>::OrderInfo*,
                   typename MM_Single<MDC, OMC>::IPair*>
  MM_Single<MDC, OMC>::GetOrderDetails(AOS const*     a_aos,
                             utxx::time_val a_ts_strat,
                             char const*    a_where) {
    assert(a_aos != nullptr && a_where != nullptr);
    OrderInfo& ori = a_aos->m_userData.Get<OrderInfo>();

    // Which IPair is it?
    if (utxx::unlikely(ori.m_iPairID < 0 ||
                       ori.m_iPairID >= int(m_iPairs.size()))) {
      // Invalid IPairID: This would be a serious error condition:
      DelayedGracefulStop hmm(this, a_ts_strat, "MM-Single::GetOrderDetails(",
                              a_where, "): AOSID=", a_aos->m_id,
                              ", Instr=", a_aos->m_instr->m_FullName.data(),
                              (a_aos->m_isBuy ? ", Bid" : ", Ask"),
                              ": Invalid IPairID=", ori.m_iPairID);

      return std::make_pair<MM_Single<MDC, OMC>::OrderInfo*,
                            MM_Single<MDC, OMC>::IPair*>(nullptr, nullptr);
    }
    // If OK:
    return std::make_pair(&ori, &(m_iPairs[size_t(ori.m_iPairID)]));
  }
}  // End namespace MAQUETTE
