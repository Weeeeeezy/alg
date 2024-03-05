// vim:ts=2:et
//===========================================================================//
//                   "TradingStrats/Pairs-MF/Orders.hpp":                    //
//                  Making Passive and Aggressive Orders                     //
//===========================================================================//
#pragma once

#include "Pairs-MF.h"
#include "Basis/Maths.hpp"
#include <sstream>

namespace MAQUETTE
{
  //=========================================================================//
  // "IPair::UpdateSprdEMA":                                                 //
  //=========================================================================//
  // Returns "true" iff it was successfully updated:
  //
  inline bool Pairs_MF::IPair::UpdateSprdEMA() const
  {
   // NB: AggrPx is adjusted by "AggrQtyFact":
    PriceT passMid =
           ArithmMidPx(m_passOB->GetBestBidPx(), m_passOB->GetBestAskPx());
    PriceT aggrMid =
           ArithmMidPx(m_aggrOB->GetBestBidPx(), m_aggrOB->GetBestAskPx());

    double sprd   = passMid - aggrMid * m_aggrQtyFact;

    if (utxx::unlikely(!IsFinite(sprd)))
    {
      // XXX: What to do in this case? It means that the liquidity was lost in
      // one of the OrderBooks.   For the moment, we simply ignore such  ticks,
      // with a warning. (Separately, we ensure that the Strategy does NOT ope-
      // rate during intra-day trading breaks,  because it is  possible to get
      // crazy mkt data at those times):
      LOG_WARN(2,
        "Pairs-MF::IPair::UpdateSprdEMA: Cannot compute the Sprd: {}-{}",
        m_passOB->GetInstr().m_FullName.data(),
        m_aggrOB->GetInstr().m_FullName.data())
      return false;
    }

    // Otherwise: Update the Spread EMA (with a special init action):
    m_sprdEMA =
      IsFinite(m_sprdEMA)
      ? (m_sprdEMACoeffs[0] * sprd + m_sprdEMACoeffs[1] * m_sprdEMA)
      : sprd;  // Initialisation!
    assert(IsFinite(m_sprdEMA));

    ++m_currSprdTicks;

    LOG_INFO(4,
      "Pairs-MF::IPair::UpdateSprdEMA: {}-{}: Sprd={}, EMA={}, CurrTicks={}"
      ", NPeriods={}",
      m_passOB->GetInstr().m_FullName.data(),
      m_aggrOB->GetInstr().m_FullName.data(), sprd, m_sprdEMA,
      m_currSprdTicks,     m_nSprdPeriods)

    // Still, if too few ticks were yet encountered, the EMA is unreliable:
    return (m_currSprdTicks > m_nSprdPeriods);
  }

  //=========================================================================//
  // "IPair::CancelQuotes":                                                  //
  //=========================================================================//
  inline void Pairs_MF::IPair::CancelQuotes
  (
    utxx::time_val  a_ts_exch,
    utxx::time_val  a_ts_recv,
    utxx::time_val  a_ts_strat
  )
  {
    for (int s = 0; s < 2; ++s)
    CancelOrderSafe<true>   // Buffered
      (m_passAOSes[s], a_ts_exch, a_ts_recv, a_ts_strat);
    m_passOMC->FlushOrders();
  }

  //=========================================================================//
  // "IPair::DoQuotes":                                                      //
  //=========================================================================//
  inline void Pairs_MF::IPair::DoQuotes
  (
    utxx::time_val  a_ts_exch,
    utxx::time_val  a_ts_recv,
    utxx::time_val  a_ts_strat
  )
  {
    //-----------------------------------------------------------------------//
    // Check the Temporal Limits for Quoting:                                //
    //-----------------------------------------------------------------------//
    if (a_ts_strat >= m_enabledUntil)
    {
      // Cancel existing Quotes (if any) -- and nothing else to do:
      CancelQuotes(a_ts_exch, a_ts_recv, a_ts_strat);
      return;
    }

    //-----------------------------------------------------------------------//
    // Update the EMA of (Pass-Aggr) Mid-Point Sprd and do the Quotes:       //
    //-----------------------------------------------------------------------//
    // To begin with, reset the Expected Pxs to NaN:
    m_expCoverPxs   [Ask] = PriceT();
    m_expCoverPxs   [Bid] = PriceT();

    if (utxx::unlikely(!UpdateSprdEMA()))
      return;       // Cannot do anything

    //-----------------------------------------------------------------------//
    // Get the Curr Best Bid and Ask:                                        //
    //-----------------------------------------------------------------------//
    PriceT bestAskPx(m_passOB->GetBestAskPx());
    PriceT bestBidPx(m_passOB->GetBestBidPx());

    if (utxx::unlikely(!(IsFinite(bestBidPx) && IsFinite(bestAskPx))))
    {
      // Dangerous evaporation of liquidity on PassInstr: Cancel active Quotes
      // (if any) for this IPair, but do NOT terminate the Strategy  or cancel
      // quotes belonging to other IPairs:
      //
      bool isBid = !IsFinite(bestBidPx);
      LOG_WARN(2,
        "Pairs-MF::IPair::DoQuotes: PassInstr={}: Insufficient Liquidity on "
        "{} side, Cancelling Both Quotes",        isBid ? "Bid" : "Ask")

      CancelQuotes(a_ts_exch, a_ts_recv, a_ts_strat);
      return;
    }

    //-----------------------------------------------------------------------//
    // Compute Ask and Bid Quote Pxs (on the Passive side):                  //
    //-----------------------------------------------------------------------//
    // NB: All "PriceT"s are initially NaN:
    long   passQtys[2] = { 0, 0 };
    PriceT newQPxs [2];
    PriceT expPPxs [2];
    PriceT aggrPxs [2];  // Expected Cover VWAPs
    bool   unchPxs [2]   { false, false };
    short  aggrLots[2] = { 0, 0 };
    double pxStep      = m_passInstr->m_PxStep;

    for (int s = 0; s < 2; ++s)
    {
      //---------------------------------------------------------------------//
      // First of all, compute the Pass Qty to be Quoted:                    //
      //---------------------------------------------------------------------//
      // XXX: Possible competitors standing in front of us, as given by "lots-
      // Ahead" computed below,  are NOT taken into account here, because that
      // would result in recursive re-calculation of VWAPs w/o any fixed-point
      // guarantees!
      bool isBid   = s;

      // Determine the Qty we will Passively Quote: It is NOT always equal to
      // "m_passQty":
      passQtys[s] =
        m_useFlipFlop
        ? // In the "FlipFlop" mode, we aim at altering the sign  of the curr
          // Pass Pos -- unless it is too small, in which case we extend it to
          // "m_passQty":
          (((m_passPos < 0 && isBid) || (m_passPos > 0 && !isBid))
           ? (2 * std::max<long>(labs(m_passPos), m_passQty))
           : (m_passPos == 0)  ? m_passQty : 0)
        : // In the Normal Mode, we use "m_passQty" but constrain it by the Pos
          // Limit:
          (isBid
           ? std::min<long>(m_passQty, m_passPosSoftLimit - m_passPos)
           : std::min<long>(m_passQty, m_passPosSoftLimit + m_passPos));

      // If we got passQty <= 0, the curr side is not to be quoted, and the
      // correp Px will remain NaN:
      if (passQtys[s] <= 0)
        continue;

      //---------------------------------------------------------------------//
      // On the AggrInstr, compute the AggrQty and its VWAP:                 //
      //---------------------------------------------------------------------//
      // NB: The Aggr Side used in those computations is the SAME as the Pass
      // Side given by "isBid":
      long aggrQty =
        long(Round(double(passQtys[s]) * m_aggrQtyFact * m_aggrQtyReserve));

      long lotSz   = long(Round(m_aggrInstr->m_LotSize));

      if (aggrQty == lotSz)
      {
        // Special Case: No need to compute any VWAPs! Can simply take the corr
        // L1 Px:
        aggrPxs[s] =
          isBid
          ? m_aggrOB->GetBestBidPx()
          : m_aggrOB->GetBestAskPx();
        aggrLots[s] = 1;
      }
      else
      {
        // Generic Case: Proceed with 1-Band WVAP computation:
        OrderBook::ParamsVWAP  <QTU> pv;
        pv.m_bandSizes[0] = Qty<QTU,QR>(aggrQty);

        if (isBid)
          m_aggrOB->GetVWAP<QTU, QR, QTU, QR, Bid>(&pv);
        else
          m_aggrOB->GetVWAP<QTU, QR, QTU, QR, Ask>(&pv);

        aggrPxs[s]  = PriceT(pv.m_vwaps[0]);

        // Memoise "aggrLots" from which the VWAP was computed (for statistical
        // purposes only):
        aggrLots[s] = short(long (pv.m_bandSizes[0] / lotSz));
      }

      //---------------------------------------------------------------------//
      // Check the computed AggrPx:                                          //
      //---------------------------------------------------------------------//
      if (utxx::unlikely(!IsFinite(aggrPxs[s])))
      {
        // Dangerous evaporation of liquidity on AggrInstr? Cancel All Quotes
        // but do not terminate the Strategy. XXX: This could also happen  if
        // we assumed the AggrNomQty=1 lot, and thus subscribed to L1Px  Aggr
        // data only, but in reality, had to create a larger order!
        LOG_WARN(2,
          "Pairs-MF::IPair::DoQuotes: AggrInstr={}: Insufficient liquidity on "
          "{} side for {} Lots, Cancelling All Allfected Quotes. Also check "
          "AggrNomQty and the AggrMktData Subscription!",
          m_aggrInstr->m_FullName.data(), isBid ? "Bid" : "Ask", aggrLots[s])

        // Cancel both quotes (if present):
        CancelQuotes(a_ts_exch, a_ts_recv, a_ts_strat);
        return;
      }

      // In any case -- whether we would actually issue an Order on this Side
      // or not -- install the newly-computed AggrPx in the corresp AOS   (if
      // exists):
      AOS const* aos = m_passAOSes[s];
      if (utxx::likely(aos != nullptr))
        aos->m_userData.Get<OrderInfo>().m_expAggrPxNew = aggrPxs[s];

      // If OK: Transalte AggrPx into the corresp PassivePx. 1st of all,
      // AggrPx corresp to 1 unit of AggrInstr
      //                 = 1/AggrQtyFact units of PassInstr; so
      // (AggrPx * AggrQtyFact) would corresp to 1 unit of PassInstr:
      //
      PriceT passPx(double(aggrPxs[s]) * m_aggrQtyFact);
      PriceT currPx =   m_quotePxs[s];

      //---------------------------------------------------------------------//
      // Apply the Spread Base:                                              //
      //---------------------------------------------------------------------//
      // TODO: Multiple possible modes here!
      // Then apply the EMA of Pass-Aggr Spread. The latter is between mid-pts,
      // so no Bid/Ask sprd yet. NB: If Pass=Spot and Aggr=Fut,  this EMA sprd
      // is typically negative:
      //
      passPx += m_sprdEMA;

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
      if (isBid)
      {
        passPx -= m_adjCoeffs[0];   // Mark-Up!
        if (m_passPos > 0)          // Skewing!
          passPx -= double(m_passPos) * m_adjCoeffs[1];  // Move Bid Down
        passPx.RoundDown(pxStep);
      }
      else
      {
        passPx += m_adjCoeffs[0];   // As above
        if (m_passPos < 0)          //
          passPx -= double(m_passPos) * m_adjCoeffs[1];  // Move Ask Up
        passPx.RoundUp  (pxStep);
      }

      // OK: this is the calculated px which we need to get a Passive Fill at,
      // in order to be able to cover our position with the expected PnL:
      assert(IsFinite(passPx));
      expPPxs[s] = passPx;

      //---------------------------------------------------------------------//
      // Now possibly adjust the PassPx wrt the curr Target OrderBook:       //
      //---------------------------------------------------------------------//
      if (m_targAdjMode != TargAdjModeT::None)
      {
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
        //     "passPx" can actually become aggressive -- but that's OK; it can
        //     also cause a self-cross (handled below):
        //
        if ( isBid && (bestBidPx < passPx))
          passPx =
            (m_targAdjMode == TargAdjModeT::ImprFillRates)
            ? (bestBidPx + pxStep) : bestBidPx;
        else
        if (!isBid && (passPx < bestAskPx))
          passPx =
            (m_targAdjMode == TargAdjModeT::ImprFillRates)
            ? (bestAskPx - pxStep) : bestAskPx;
        else
        // "PassPx" falls onto L1 or deeper. In that case, we need to check if
        // it falls into a "DeadZone" (where most of competitors are lurking);
        // in that case, the Quote will be cancelled.
        // FIXME(?) But if the Quote stays, we currently do NOT re-calculate the
        // VWAP based on the additional potential aggressors lurking in front of
        // us, because such re-calculation would need to be recursive (if ourr
        // quote px increases, more competitors may be lurking in front of us!),
        // which is greatly inefficient, and there is no formal guarantee that a
        // fixed-point exists at all!
        // NB: For efficiency, do nothing if the DeadZone is not effective:
        //
        if (m_dzLotsFrom <= m_dzLotsTo)
        {
          long lotsAhead = GetOthersLotsUpTo<false>(isBid, passPx);
          if (m_dzLotsFrom <= lotsAhead && lotsAhead <= m_dzLotsTo)
            passPx = PriceT(); // NaN!
        }
      }
      //---------------------------------------------------------------------//
      // Apply the Resistances to avoid too frequent PassPx updates:         //
      //---------------------------------------------------------------------//
      // NB: The following only makes sense if "passPx" is valid. If Resistances
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
      if (IsFinite(passPx) && m_resistCoeff > 0.0)
      {
        double dist     = Abs(currPx - passPx); // Dist of the proposed px move
        bool   bothRear =
          ( isBid &&  currPx < bestBidPx  && passPx < bestBidPx) ||
          (!isBid &&  currPx > bestAskPx  && passPx > bestAskPx);

        if (utxx::likely(passPx != currPx && bothRear))
        {
          // Then the Resistance applies. The Resistance itself depends on our
          // curr distance to L1:
          double resist =
            (isBid ? (bestBidPx - currPx) : (currPx - bestAskPx))
            * m_resistCoeff;

          if (dist < resist)
            passPx = currPx;  // Stay put!
        }
      }
      // So: is the price really unchanged (either because it was such by calcu-
      // lation, or forced do stay put)? NB: If "passPx" was not Finite at all,
      // the corresp "unchPxs" flag remains "false", because the equality fails:
      unchPxs[s] = (currPx == passPx);

      // In any case, if we get here, "passPx" must be Finite. Memoise it (NB:
      // it may be NaN!):
      newQPxs[s] = passPx;
    }
    // End of "s" loop

    //-----------------------------------------------------------------------//
    // Check for Self-Crossings (AskPx <= BidPx):                            //
    //-----------------------------------------------------------------------//
    // NB: The following condition can only hold if both pxs are Finite:
    //
    if (utxx::unlikely(newQPxs[Ask] <= newQPxs[Bid]))
    {
      // If only one px was unchanged, keep it unchanged and move the other
      // one away -- this is to avoid unnecessary moves:
      if (unchPxs[Bid] && !(unchPxs[Ask]))
        // Move the Ask up:
        newQPxs[Ask] = newQPxs[Bid] + pxStep;
      else
      if (unchPxs[Ask] && !(unchPxs[Bid]))
        // Move the Bid down:
        newQPxs[Bid] = newQPxs[Ask] - pxStep;
      else
      do
      {
        // Either both pxs are to be changed, or both unchanged. XXX: the lat-
        // ter case is extremely unlikely -- it means that there was ALREADY a
        // cross! But in any case, move the pxs symmetrically into the opposi-
        // te dirs:
        newQPxs[Ask] += pxStep;
        newQPxs[Bid] -= pxStep;
      }
      while (utxx::unlikely(newQPxs[Ask] <= newQPxs[Bid]));

      // So the pxs must now be valid (and both Finite):
      assert(newQPxs[Bid] < newQPxs[Ask]);
    }

    //-----------------------------------------------------------------------//
    // Submit the Quotes:                                                    //
    //-----------------------------------------------------------------------//
    bool done[2]    = { false, false };
    bool doneCancel = false;

    for (int s = 0; s < 2; ++s)
    {
      bool isBid  = s;

      if (IsFinite(newQPxs[s]))
      {
        //-------------------------------------------------------------------//
        // A New Quote (on an empty slot):                                   //
        //-------------------------------------------------------------------//
        if (utxx::unlikely(m_passAOSes[s] == nullptr))
        {
          // Do not place NewOrders if insufficient time has elapsed since the
          // last Passive Fill -- this can give rise to Passive Slippage:
          //
          if ((!m_passLastFillTS.empty()                    &&
              (a_ts_strat - m_passLastFillTS).milliseconds() <
               m_reQuoteDelayMSec))
            continue;

          // Otherwise, Go Ahead:
          assert(IsFinite(newQPxs[s]) && passQtys[s] > 0);

          m_passAOSes[s] = NewOrderSafe<false, true> // !Aggr, Buffered
                           (isBid,    0, newQPxs [s], expPPxs [s], aggrPxs[s],
                            NaN<double>, aggrLots[s], passQtys[s],
                            a_ts_exch,   a_ts_recv,   a_ts_strat);
          done[s] = (m_passAOSes[s] != nullptr);
        }
        else
        //-------------------------------------------------------------------//
        // Existing Quote Modification: But should we move it at all?        //
        //-------------------------------------------------------------------//
        if (newQPxs[s] != m_quotePxs[s])
        {
          // Really try to move the order (the following call will do nothing
          // and return "false" if the order cannot be moved because it is eg
          // Part-Filled and/or being Calcelled):
          //
          done[s] = ModifyQuoteSafe<true>     // Buffered
                    (isBid,       newQPxs [s], expPPxs[s], aggrPxs[s],
                     aggrLots[s], passQtys[s],
                     a_ts_exch,   a_ts_recv,   a_ts_strat);
        }

        // Memoise the New Quote Px (if really quoted or re-quoted):
        if (done[s])
        {
          m_quotePxs[s]    = newQPxs[s];
          assert(IsFinite(m_quotePxs[s]));
        }
      }
      else
      if (m_passAOSes[s] != nullptr)
      {
        //-------------------------------------------------------------------//
        // NewQuotePx is NaN -- Cancel an existing Quote:                    //
        //-------------------------------------------------------------------//
        // NB: "done[s]" remains unset -- there was no quote done:
        //
        CancelOrderSafe<true>   // Buffered
          (m_passAOSes[s], a_ts_exch, a_ts_recv, a_ts_strat);
        doneCancel = true;
      }
    }
    // End of "s" loop

    // Flush the pending orders if any:
    bool doneSomething = done[Ask] || done[Bid] || doneCancel;
    if  (doneSomething)
      m_passOMC->FlushOrders();

    //-----------------------------------------------------------------------//
    // Log the Quote(s):                                                     //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely(m_debugLevel >= 2 && doneSomething))
    {
      // Whether at least 1 of the Quoted Pxs is Tight (ie Fill is likely)
      bool isTight = (newQPxs[Ask] <= bestAskPx) || (newQPxs[Bid] >= bestBidPx);

      Req12 const* reqBid   =
        (m_passAOSes[Bid]  != nullptr) ? m_passAOSes[Bid]->m_lastReq : nullptr;
      Req12 const* reqAsk   =
        (m_passAOSes[Ask]  != nullptr) ? m_passAOSes[Ask]->m_lastReq : nullptr;

      PriceT       actBidPx =
        (reqBid != nullptr) ? PriceT(reqBid->m_px) : PriceT();
      PriceT       actAskPx =
        (reqAsk != nullptr) ? PriceT(reqAsk->m_px) : PriceT();

      OrderID      actBidID = (reqBid != nullptr) ? reqBid->m_id : 0;
      OrderID      actAskID = (reqAsk != nullptr) ? reqAsk->m_id : 0;

      PriceT coverBidPxLast =
        (m_passAOSes[Bid]  != nullptr)
       ? m_passAOSes[Bid]->m_userData.Get<OrderInfo>().m_expAggrPxLast
       : PriceT();
      PriceT coverAskPxLast =
        (m_passAOSes[Ask]  != nullptr)
       ? m_passAOSes[Ask]->m_userData.Get<OrderInfo>().m_expAggrPxLast
       : PriceT();

      PriceT coverBidPxNew =
        (m_passAOSes[Bid]  != nullptr)
       ? m_passAOSes[Bid]->m_userData.Get<OrderInfo>().m_expAggrPxNew
       : PriceT();
      PriceT coverAskPxNew =
        (m_passAOSes[Ask]  != nullptr)
       ? m_passAOSes[Ask]->m_userData.Get<OrderInfo>().m_expAggrPxNew
       : PriceT();

      m_logger->info
        ("Pairs-MF::IPair::DoQuotes: Instr={}{}"
         "\nQuoted: [{}]{}@{} .. {}@{}[{}]"
         "\nBest:          {} .. {}"
         "\nMidPtsSprd:    {}"
         "\nCoverLast:     {} .. {}"
         "\nCoverNew :     {} .. {}",
         m_passInstr->m_FullName.data(), isTight ? " !!!" : "",
         actBidID,      passQtys[Bid],   double(actBidPx),  double(actAskPx),
         passQtys[Ask], actAskID,        double(bestBidPx), double(bestAskPx),
         m_sprdEMA,
         double(coverBidPxLast),         double(coverAskPxLast),
         double(coverBidPxNew),          double(coverAskPxNew));
      m_logger->flush();
    }
  }

  //=========================================================================//
  // "IPair::GetOthersLotsUpTo":                                             //
  //=========================================================================//
  // Computes  the size of all Passive orders which may potentially represent
  // rivals to Pairs-MF. Returns the corresp size (in Base Units) of the Aggr
  // Instr:
  // XXX: This computation is relatively costly!
  //
  template<bool StopAtOurs>
  inline long Pairs_MF::IPair::GetOthersLotsUpTo
  (
    bool   a_is_bid,
    PriceT a_upto_px
  )
  const
  {
    // Traverse the corresp side of the Passive OrderBook, up to the total size
    // equal to the avg trade size:
    long resLots = 0;

    // XXX: The following calculation does NOT use our own quote px as a limit,
    // vecause that px is not known yet. Instead,  we use the avg  Trade  size
    // (which may lead to conservative estimates):
    auto scanner =
      [&resLots,   a_is_bid, a_upto_px]
      (int, PriceT a_px, OrderBook::OBEntry const& a_obe) -> bool
      {
        // All "low" px levels have been traversed?
        if (utxx::unlikely
           ((a_is_bid && a_px < a_upto_px) || (!a_is_bid && a_px > a_upto_px)))
          return false;

        // If the level is still within the range: Traverse all Orders at this
        // level (if available):
        for (OrderBook::OrderInfo const*  order = a_obe.m_frstOrder;
             order != nullptr;    order = order->m_next)
          // Our own orders are to be skipped (that's why we cannot simply use
          // the Aggregated Qtys). For all others:
          if (utxx::likely(order->m_req12 == nullptr))
            resLots += long(order->GetQty<QTU,QR>());
          else
          if (StopAtOurs)
            return false;

        // Continue (will check exit conditions again at next PxLevel):
        return true;
      };

    // Go:
    assert(m_passOB != nullptr);
    if (a_is_bid)
      m_passOB->Traverse<Bid>(0, scanner);
    else
      m_passOB->Traverse<Ask>(0, scanner);

    // The result:
    return resLots;
  }

  //=========================================================================//
  // "IPair::DoCoveringOrder":                                               //
  //=========================================================================//
  inline void Pairs_MF::IPair::DoCoveringOrder
  (
    AOS const*       a_pass_aos,
    long             a_pass_qty,
    utxx::time_val   a_ts_exch,
    utxx::time_val   a_ts_recv,
    utxx::time_val   a_ts_strat
  )
  {
    assert(a_pass_aos != nullptr && a_pass_qty > 0);

    OrderInfo& ori = a_pass_aos->m_userData.Get<OrderInfo>();
    bool   passBid = a_pass_aos->m_isBuy;
    long aggrQty   = long(Round(double(a_pass_qty) * m_aggrQtyFact));
    assert(aggrQty > 0);

    AOS const* aggrAOS =
      (utxx::likely(aggrQty > 0))
      ? // IsAggr=true, Buffered=false, Side=!passBid:
        NewOrderSafe<true, false>
        (
          !passBid,
          a_pass_aos->m_id,
          ori.m_expAggrPxLast,
          ori.m_expAggrPxNew,
          PriceT(),
          ori.m_passSlip,
          ori.m_aggrLotsNew,
          aggrQty,
          a_ts_exch,
          a_ts_recv,
          a_ts_strat
        )
      : nullptr;

    if (utxx::likely(aggrAOS != nullptr))
    {
      // Log the AggrCover if we have done it just now:
      Req12 const*   aggrReq  = aggrAOS->m_lastReq;
      assert(aggrReq != nullptr && aggrReq->m_kind == Req12::KindT::New);

      LOG_INFO(3,
        "Pairs-MF::IPair::DoCoveringOrder: AggrCover: PassAOSID={}, "
        "AggrReqID={}, AggrAOSID={}, Instr={}, {}, Px={}, ExpFillPxLast={}, "
        "ExpFillPxNew={}, Qty={}, PassSlip={}",
        a_pass_aos->m_id, aggrReq->m_id, aggrAOS->m_id,
        aggrAOS->m_instr->m_FullName.data(),
        aggrAOS->m_isBuy ? "Bid" : "Ask",   double(aggrReq->m_px),
        double(ori.m_expAggrPxLast),        double(ori.m_expAggrPxNew),
        aggrQty,                            double(ori.m_passSlip))
    }
  }

  //=========================================================================//
  // "IPair::NewOrderSafe":                                                  //
  //=========================================================================//
  // IsAggr is NOT set (Passive Order):
  // (*) "a_px1" is the new Passive   Quote Px
  // (*) "a_px2" is the Expected Pass Fill  Px (both Last and New, w/o  adjs)
  // (*) "a_px3" is the Expected Aggr Fill  Px (both Last and New)
  // IsAggr is SET     (Aggressive Order):
  // (*) "a_px1" is the Expected Aggr Fill  Px -- Last (NaN for HedgeAdj);
  // (*) "a_px2" is the Expected Aggr Fill  Px -- New  (NaN for HedgeAdj);
  // (*) "a_px3" is NaN;
  // Also:
  // (*) "a_pass_slip" is Finite only if it is an Aggr but not Deep-Aggr Ord;
  // (*) "a_aggr_lots" is the volume (in Lots) on the Aggr side used to compute
  //     the Passive Px; it is then carried into the corresp Aggr order, so it
  //     is always set:
  //
  template<bool IsAggr, bool Buffered>
  inline AOS const* Pairs_MF::IPair::NewOrderSafe
  (
    bool            a_is_bid,
    OrderID         a_ref_aosid,
    PriceT          a_px1,
    PriceT          a_px2,
    PriceT          a_px3,
    double          a_pass_slip,
    short           a_aggr_lots,
    long            a_qty,
    utxx::time_val  a_ts_exch,
    utxx::time_val  a_ts_recv,
    utxx::time_val  a_ts_strat
  )
  {
    // NB: "a_px3" is NaN iff the order is Aggressive; "a_px[12]" must be Fin-
    // ite for Passive Orders:
    assert( (IsAggr || (IsFinite(a_px1) && IsFinite(a_px2))) &&
           (!IsAggr ==  IsFinite(a_px3)));

    // Again, "a_pass_slip" can only be Finite for an Aggr Order (but the conv-
    // erse is not true):
    assert(!IsFinite(a_pass_slip) || IsAggr);

    // RefAOSID is the AOSID of a Passive Order being covered (so it must be
    // non-0 if the curr Order is Aggressive, UNLESS this is a "Hedging Adj-
    // ustment" order). If is always 0 for a Passive Ord:
    //
    assert(IsAggr || (a_ref_aosid == 0));

    //-----------------------------------------------------------------------//
    // Safe Mode?                                                            //
    //-----------------------------------------------------------------------//
    // Then do not even try to issue a New Order -- it will be rejected by the
    // RiskMgr anyway.  Initiate graceful shut-down instead:
    //
    if (utxx::unlikely
       (m_outer->m_riskMgr != nullptr &&
        m_outer->m_riskMgr->GetMode() == RMModeT::Safe))
    {
      Pairs_MF::DelayedGracefulStop hmm
        (m_outer, a_ts_strat,
         "Pairs-MF::IPair::NewOrderSafe: RiskMgr has entered SafeMode");
      return nullptr;
    }

    //-----------------------------------------------------------------------//
    // Stopping?                                                             //
    //-----------------------------------------------------------------------//
    // If so, Passive Orders are not allowed (we should have already issued
    // Cancellation reqs for all Passive Quotes), but Aggressive Orders may
    // in theory happen:
    //
    if (utxx::unlikely(!IsAggr && m_outer->IsStopping()))
      return nullptr;

    //-----------------------------------------------------------------------//
    // Order Too Small?                                                      //
    //-----------------------------------------------------------------------//
    // We better reject it now than wait for an exception from the Risk Mgr:
    //
    // The Instrument to be traded:
    SecDefD const* instr = IsAggr ? m_aggrInstr : m_passInstr;
    assert(instr != nullptr);

    long lotSz = long(Round(instr->m_LotSize));

    if (utxx::unlikely(a_qty < lotSz / 2))
      return nullptr;

    //-----------------------------------------------------------------------//
    // Calculate / Check the Px:                                             //
    //-----------------------------------------------------------------------//
    // NB: "px" is the actual LimitPx (Pass or Aggr) which will be used:
    PriceT px;  // Initially NaN

    if (IsAggr)
    {
      // (*) If the "DeepAggr" mode is set, OR this is a Tail-Adj order (which
      //     means that PassSlip and/or Px2 are not set),  then calculate a
      //     Deeply-Aggressive (but still a Limit)  px, to guarantee a Fill:
      //
      if (m_aggrMode == AggrModeT::DeepAggr || !IsFinite(a_pass_slip) ||
          !IsFinite(a_px2))
        px = MkDeepAggrPx(a_is_bid);
      else
      // (*) Pegged: Place the order at the curr Pass Px (then move it with the
      //     corresp L1 Px):
      //
      if (m_aggrMode == AggrModeT::Pegged)
        px = a_is_bid ? m_aggrOB->GetBestBidPx() : m_aggrOB->GetBestAskPx();
      else
      // (*) Otherwise, use the Expected Aggr Px adjusted for Passive Slippage
      //     and Extra Mark-Up:
      {
        // Use the Passive Slippage; Extra Mark-Up is also given in terms of the
        // PassivePx, so need to apply the Reverse QtyFactor:
        assert(m_aggrMode == AggrModeT::FixedPass  &&
               a_pass_slip    >= 0.0 && IsFinite(a_px2));

        double adj = (a_pass_slip + m_extraMarkUp) / m_aggrQtyFact;

        // NB: here the "px" is rounded towards the "more passve side":
        double pxStep = instr->m_PxStep;
        if (a_is_bid)
        {
          px = a_px2 - adj;
          px.RoundDown(pxStep);
        }
        else
        {
          px = a_px2 + adj;
          px.RoundUp  (pxStep);
        }
      }

      // So finally, have we got the AggrPx? If not, try one again DeepAggr:
      if (utxx::unlikely(!IsFinite(px)))
        px = MkDeepAggrPx(a_is_bid);

      // Still could not compute the AggrPx? XXX: In general, this is a serious
      // error cond, because it probably means that we cannot cover our positi-
      // on. We better shut down now:
      if (utxx::unlikely(!IsFinite(px)))
      {
        LOG_ERROR(1,
          "Pairs-MF::IPair::NewOrderSafe: AggrInstr={}: Cannot generate the "
          "{} AggrPx, EXITING...",
          m_aggrInstr->m_FullName.data(), a_is_bid ? "Bid" : "Ask")

        m_outer->CancelAllQuotes(nullptr, a_ts_exch, a_ts_recv, a_ts_strat);

        DelayedGracefulStop hmm
          (m_outer,  a_ts_strat,
           "Pairs-MF::IPair::NewOrderSafe: Cannot generate the AggrPx");
        return nullptr;
      }
    }
    else
      // For a Passive Order, the Px is explicitly given by the corresp arg:
      px = a_px1;

    // The LimitPx must now be well-definied:
    assert(IsFinite(px));

    //-----------------------------------------------------------------------//
    // Actually issue a New Order:                                           //
    //-----------------------------------------------------------------------//
    // NB: TimeInForce is IOC for Deeply-Aggressive Orders, and the default one
    // (normally Day or GTC, depending on the MDC) for others:
    FIX::TimeInForceT tif =
      (IsAggr && m_aggrMode == AggrModeT::DeepAggr)
      ? FIX::TimeInForceT::ImmedOrCancel
      : FIX::TimeInForceT::UNDEFINED;

    // Capture and log possible "NewOrder" exceptions, then terminate the Stra-
    // tegy. NB: BatchSend=Buffered:
    AOS const* res = nullptr;
    try
    {
      res = (IsAggr ? m_aggrOMC : m_passOMC)->NewOrder
      (
        m_outer,    *instr,     FIX::OrderTypeT::Limit, a_is_bid,
        px,         IsAggr,     QtyAny(a_qty, QTU),     a_ts_exch,
        a_ts_recv,  a_ts_strat, Buffered,               tif
      );
    }
    catch (EPollReactor::ExitRun const&)
    {
      // This exception is always propagated:
      throw;
    }
    catch (std::exception const& exc)
    {
      Pairs_MF::DelayedGracefulStop hmm
        (m_outer, a_ts_strat,
         "Pairs-MF::IPair::NewOrderSafe: Order FAILED: Instr=",
         instr->m_FullName.data(), (a_is_bid ? ", Bid" : ", Ask"),
         ", Px=",  px, ", Qty=",    a_qty,  ": ", exc.what());
      return nullptr;
    }
    assert(res != nullptr);

    //-----------------------------------------------------------------------//
    // Install the OrderInfo:                                                //
    //-----------------------------------------------------------------------//
    // In particular, we memoise (in the AOS) the index of this "IPair" and the
    // expected Fill  Px (for both Pass and Aggr Orders):
    OrderInfo ori
    (
      IsAggr,
      a_ref_aosid,
      short(this - m_outer->m_iPairs.data()), // IPairID
      IsAggr ? PriceT() : a_px2,              // ExpPassPx
      IsAggr ? a_px1    : a_px3,              // ExpAggrPxLast
      IsAggr ? a_px2    : a_px3,              // ExpAggrPxNew
      a_aggr_lots
    );
    res->m_userData.Set(ori);

    //-----------------------------------------------------------------------//
    // For Aggressive Orders:                                                //
    //-----------------------------------------------------------------------//
    if (IsAggr)
    {
      // Save this "aggrAOS" on the transient vector (XXX: it is long enough so
      // it should NOT overflow, but still check):
      if (utxx::unlikely(m_aggrAOSes.size() >= m_aggrAOSes.capacity()))
      {
        Pairs_MF::DelayedGracefulStop hmm
          (m_outer, a_ts_strat,
           "Pairs-MF::IPair::NewOrderSafe: Too Many Aggressive Orders: ",
           (m_aggrAOSes.size() + 1));
        // XXX: But we do not reset "res" to NULL!..
      }
      else
        m_aggrAOSes.push_back(res);
    }

    //-----------------------------------------------------------------------//
    // Log it (taking order data independently -- using Req12):              //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely(res != nullptr))
    {
      Req12 const* req = res->m_lastReq;
      assert(req != nullptr && req->m_kind == Req12::KindT::New  &&
             req->m_status  == Req12::StatusT::New && res->m_omc != nullptr);

      LOG_INFO(3,
        "Pairs-MF::IPair::NewOrderSafe: OMC={}, Instr={}, NewAOSID={}, NewReq"
        "ID={}, {}, QtyLots={}, Px={}",
        res->m_omc->GetName(),       instr->m_FullName.data(),      res->m_id,
        req->m_id,                   res->m_isBuy ? "Bid": "Ask",
        long(req->GetQty<QTU,QR>()), double(req->m_px))
    }
    // All Done:
    return res;
  }

  //=========================================================================//
  // "IPair::CancelOrderSafe":                                               //
  //=========================================================================//
  // For Passive and Non-Deep Aggressive Orders:
  //
  template<bool Buffered>
  inline void Pairs_MF::IPair::CancelOrderSafe
  (
    AOS const*      a_aos,     // Non-NULL
    utxx::time_val  a_ts_exch,
    utxx::time_val  a_ts_recv,
    utxx::time_val  a_ts_strat
  )
  {
    // Simply ignore non-existing Orders and other Degenerate Cases:
    // This is NOT an error:
    if (utxx::unlikely
       (a_aos == nullptr || a_aos->m_isInactive || a_aos->m_isCxlPending != 0))
      return;
    assert(a_aos->m_omc  != nullptr);

    // Generic Case: Yes, really Cancel this Order:
    try
    {
      bool ok = a_aos->m_omc->CancelOrder
               (a_aos, a_ts_exch, a_ts_recv, a_ts_strat, Buffered);

      // "CancelOrder" should not return "false" in this case, because pre-conds
      // were satisfied:
      if (utxx::unlikely(!ok))
        throw utxx::runtime_error("CancelOrder returned False");
    }
    catch (EPollReactor::ExitRun const&)
    {
      // This exception is always propagated -- even for "Cancel":
      throw;
    }
    catch (std::exception const& exc)
    {
      // XXX: Cancellation failure is probably a critical error?
      SecDefD const* instr = a_aos->m_instr;
      DelayedGracefulStop hmm
        (m_outer, a_ts_strat,
         "Pairs-MF::IPair::CancelOrderSafe: FAILED: Instr=",
         instr->m_FullName.data(),      (a_aos->m_isBuy ? ", Bid" : ", Ask"),
         ", AOSID=", a_aos->m_id, ": ", exc.what());
    }

    // If OK: Log it:
    Req12 const* req = a_aos->m_lastReq;
    assert(req != nullptr && req->m_kind == Req12::KindT::Cancel &&
           req->m_status  == Req12::StatusT::New);

    LOG_INFO(3,
      "Pairs-MF::IPair::CancelOrderSafe: OMC={}, Instr={}, {}: "
      "Cancelling AOSID={}, CancelReqID={}",
      a_aos->m_omc->GetName(), a_aos->m_instr->m_FullName.data(),
      a_aos->m_isBuy ? "Bid" : "Ask", a_aos->m_id,    req->m_id)
  }

  //=========================================================================//
  // "IPair::ModifyQuoteSafe":                                               //
  //=========================================================================//
  // For Passive Quotes only -- NOT for eg Semi-Aggressive (Pegged) Covering
  // Orders:
  template<bool Buffered>
  inline bool Pairs_MF::IPair::ModifyQuoteSafe
  (
    bool            a_is_bid,
    PriceT          a_pass_px1, // New Quote Px
    PriceT          a_pass_px2, // Expected  Pass Fill Px (w/o adjs)
    PriceT          a_aggr_px,  // Expected  Cover Px
    short           a_aggr_lots,
    long            a_pass_qty, // NOT in Lots!
    utxx::time_val  a_ts_exch,
    utxx::time_val  a_ts_recv,
    utxx::time_val  a_ts_strat
  )
  {
    assert(IsFinite(a_pass_px1) && IsFinite(a_pass_px2) && IsFinite(a_aggr_px));

    AOS const* aos = m_passAOSes[a_is_bid];

    //-----------------------------------------------------------------------//
    // Check for Degenerate Cases:                                           //
    //-----------------------------------------------------------------------//
    // Check the AOS validity. Also, modifications are not allowed (silently
    // ignored) in the Stopping Mode (because we should have issued Cancella-
    // tion Reqs for all Quotes before entering that mode anyway):
    if (utxx::unlikely
       (aos == nullptr || aos->m_isInactive || aos->m_isCxlPending != 0 ||
        m_outer->IsStopping()))
    {
      LOG_WARN(2,
        "Pairs-MF::ModifyQuoteSafe: Invalid AOSID={}, {}{}{}",
        (aos != nullptr) ?  aos->m_id : 0,       a_is_bid ? "Bid" : "Ask",
        (aos != nullptr  && aos->m_isInactive)   ? ": Inactive"   : "",
        (aos != nullptr  && aos->m_isCxlPending) ? ": CxlPending" : "")

      // If (by any chance) there is a stale non-NULL AOS ptr here, and the AOS
      // is now Inactive, reset it:
      if (aos != nullptr && aos->m_isInactive)
        m_passAOSes[a_is_bid] = nullptr;
      return false;
    }

    // Also, if the new qty is too small, we better Cancel the order than Modify
    // it -- the Modify Req is likely to be rejected by the Risk Mgr anyway:
    //
    assert(aos != nullptr && aos->m_omc != nullptr && aos->m_instr != nullptr);
    SecDefD const& instr = *(aos->m_instr);
    long           lotSz = long(Round(instr.m_LotSize));

    if (utxx::unlikely(a_pass_qty < lotSz / 2))
    {
      CancelOrderSafe<Buffered>(aos, a_ts_exch, a_ts_recv, a_ts_strat);
      return true;    // We still DID something!
    }

    //-----------------------------------------------------------------------//
    // Generic Case: Proceed with "ModifyOrder":                             //
    //-----------------------------------------------------------------------//
    try
    {
      // NB: IsAggr=false, BatchSend=Buffered:
      bool ok = aos->m_omc->ModifyOrder
      (
        aos, a_pass_px1, false, QtyAny(a_pass_qty, QTU),
        a_ts_exch,   a_ts_recv, a_ts_strat,   Buffered
      );

      // "ModifyOrder" should not return "false" in this case, because pre-conds
      // were supposed to be satisfied. But still check:
      if (utxx::unlikely(!ok))
        throw utxx::runtime_error("ModifyOrder returned False");
    }
    catch (EPollReactor::ExitRun const&)
    {
      // This exception is always propagated:
      throw;
    }
    catch (std::exception const& exc)
    {
      // If it has happened, try to cancel this Quote but still continue:
      LOG_WARN(2,
        "Pairs-MF::IPair::ModifyQuoteSafe: FAILED: Instr={}, {}, Px={}, "
        "AOSID={}: {}: Cancelling this Quote...",
        instr.m_FullName.data(), aos->m_isBuy ? "Bid" : "Ask",
        double(a_pass_px1),      aos->m_id,     exc.what())

      CancelOrderSafe<Buffered>(aos, a_ts_exch, a_ts_recv, a_ts_strat);
      return false;
    }

    //-----------------------------------------------------------------------//
    // If successful: Post-Processing:                                       //
    //-----------------------------------------------------------------------//
    // Modify also the OrderInfo:
    OrderInfo&  ori     = aos->m_userData.Get<OrderInfo>();
    ori.m_expPassPx     = a_pass_px2;
    ori.m_expAggrPxLast = a_aggr_px;
    ori.m_expAggrPxNew  = a_aggr_px;
    ori.m_aggrLotsNew   = a_aggr_lots;

    // Log it: NB: No "ModLegN" Reqs here:
    Req12 const* req = aos->m_lastReq;
    assert(req != nullptr && req->m_kind == Req12::KindT::Modify &&
           req->m_status  == Req12::StatusT::New);

    LOG_INFO(3,
      "Pairs-MF::IPair::ModifyQuoteSafe: OMC={}, Instr={}, {}, AOSID={}, "
      "NewReqID={}, QtyLots={}, Px={}",
      aos->m_omc->GetName(),        instr.m_FullName.data(),
      aos->m_isBuy ? "Bid" : "Ask", aos->m_id,    req->m_id,
      long(req->GetQty<QTU,QR>()),  double(req->m_px))

    // All Done:
    return true;
  }

  //=========================================================================//
  // "IPair::EvalStopLoss" (for AggrOrders, to become Deeply-Aggressive):    //
  //=========================================================================//
  // Returns "true" iff such a modification (into a Deeply-Aggressive Order)
  // has indeed been made:
  //
  inline bool Pairs_MF::IPair::EvalStopLoss
  (
    OrderBook const&  a_ob,
    AOS const*        a_aos,
    utxx::time_val    a_ts_exch,
    utxx::time_val    a_ts_recv,
    utxx::time_val    a_ts_strat
  )
  {
    if (utxx::unlikely
       (a_aos == nullptr || a_aos->m_isInactive || a_aos->m_isCxlPending != 0))
      return false;   // Nothing to do

    assert(a_aos->m_instr == &(a_ob.GetInstr()));

    // Get the OrderInfo -- it must be an Aggressive Order, and not yet Inacti-
    // ve:
    OrderInfo const& ori = a_aos->m_userData.Get<OrderInfo>();
    assert(ori.m_isAggr);

    // Compute the VM (FIXME: NOT using VWAP for the moment, so the following
    // logic is not really correct):
    bool   isBid = a_aos->m_isBuy;
    double VM    =
      isBid
      ? (ori.m_expAggrPxNew  - a_ob.GetBestBidPx())
      : (a_ob.GetBestAskPx() - ori.m_expAggrPxNew);

    if (VM < m_aggrStopLoss)
    {
      // XXX: If the order is already Part-Filled, we cannot modify it anymore
      // to make it Deeply-Aggressive: Will have to Cancel it, and issue a new
      // one (for the unfilled Qty) once the Cancel is confirmed:
      //
      long cumQty(a_aos->GetCumFilledQty<QTU,QR>());
      if (utxx::unlikely(cumQty > 0))
      {
        // Buffered (Flush is in the Caller):
        CancelOrderSafe<true>(a_aos, a_ts_exch, a_ts_recv, a_ts_strat);

        // Cannot re-enter this order yet, as it may be Filled before it was
        // Cancelled:
        LOG_INFO(3,
          "Pairs-M::EvalStopLoss: AggrAOSID={}, Instr={}, {}: VM={}: Order "
          "is Part-Filled ({} Lots): Cannot Modify: Cancelling it first",
          a_aos->m_id, a_aos->m_instr->m_FullName.data(),
          isBid ? "Bid" : "Ask",  VM,  long(cumQty))

        return false;
      }

      // Generic case: Modify this order to become Deeply-Aggressive (Buffered,
      // Flush is in the Caller):
      long   remQty = long(a_aos->GetLeavesQty<QTU,QR>());
      assert(remQty > 0);

      // XXX: No exception handing here -- we assume that the following call
      // must succeed:
      bool ok = m_aggrOMC->ModifyOrder
      (
        a_aos,
        MkDeepAggrPx(isBid),
        true,       // IsAggr=true
        QtyAny(remQty, QTU),
        a_ts_exch,
        a_ts_recv,
        a_ts_strat,
        true        // Buffered
      );
      LOG_INFO(3,
        "Pairs-M::EvalStopLoss: AggrAOSID={}, Instr={}, {}: VM={}, Going "
        "Deeply-Aggressive",
        a_aos->m_id, a_aos->m_instr->m_FullName.data(),
        isBid ? "Bid" : "Ask",  VM)

      if (utxx::unlikely(!ok))
      {
        LOG_ERROR(2,
          "Pairs-MF::EvalStopLoss: AggrAOSID={}: Could NOT go Deeply-"
          "Aggressive: ModifyOrder returned False", a_aos->m_id)
        return false;
      }
      // If OK: Yes, the Order has been made Deeply-Aggressive:
      return true;
    }

    // If we got here: No, there was no need (based on the VM) to modify the
    // Order:
    return false;
  }

  //=========================================================================//
  // "IPair::MkDeepAggrPx":                                                  //
  //=========================================================================//
  inline PriceT Pairs_MF::IPair::MkDeepAggrPx(bool a_is_bid) const
  {
    double pxStep = m_aggrInstr->m_PxStep;
    return
      a_is_bid
      ? RoundUp  (1.01 * m_aggrOB->GetBestAskPx(), pxStep)  // Very high Bid
      : RoundDown(0.99 * m_aggrOB->GetBestBidPx(), pxStep); // Very low Ask
  }

  //=========================================================================//
  // "GetOrderDetails":                                                      //
  //=========================================================================//
  inline std::pair<Pairs_MF::OrderInfo*, Pairs_MF::IPair*>
  Pairs_MF::GetOrderDetails
  (
    AOS const*      a_aos,
    utxx::time_val  a_ts_strat,
    char const*     a_where
  )
  {
    assert(a_aos != nullptr && a_where != nullptr);
    OrderInfo& ori = a_aos->m_userData.Get<OrderInfo>();

    // Which IPair is it?
    if (utxx::unlikely
       (ori.m_iPairID < 0 || ori.m_iPairID >= int(m_iPairs.size()) ))
    {
      // Invalid IPairID: This would be a serious error condition:
      DelayedGracefulStop hmm
        (this, a_ts_strat,
         "Pairs-MF::GetOrderDetails(",  a_where,  "): AOSID=",  a_aos->m_id,
         ", Instr=", a_aos->m_instr->m_FullName.data(),
         (a_aos->m_isBuy ? ", Bid" : ", Ask"),    ": Invalid IPairID=",
         ori.m_iPairID);

      return std::make_pair<Pairs_MF::OrderInfo*, Pairs_MF::IPair*>
             (nullptr, nullptr);
    }
    // If OK:
    return std::make_pair(&ori, &(m_iPairs[size_t(ori.m_iPairID)]));
  }

  //=========================================================================//
  // "IPair::RemoveInactiveAggrOrder":                                       //
  //=========================================================================//
  inline void Pairs_MF::IPair::RemoveInactiveAggrOrder
  (
    AOS const*      a_aos,
    char const*     a_where
  )
  {
    // XXX: Do nothing if there is no AOS, or if it is not Inactive yet:
    if (utxx::unlikely(a_aos == nullptr || !(a_aos->m_isInactive)))
      return;
    assert(a_where != nullptr);

    // Otherwise, try to find this AggrOrder:
    auto it    = std::find(m_aggrAOSes.begin(), m_aggrAOSes.end(), a_aos);
    bool found = (it != m_aggrAOSes.end());

    if (utxx::unlikely(!found))
    {
      // AggrOrder not found -- maybe it was already removed due to another
      // event?
      LOG_WARN(2,
        "Pairs-MF::IPair::RemoveInactiveAggrOrder({}): AOSID={}, Instr={}, {}: "
        "Not found in AggrAOSes",
        a_where,   a_aos->m_id,  a_aos->m_instr->m_FullName.data(),
        a_aos->m_isBuy ? "Bid" : "Ask")
      return;
    }
    // If found: Remove it from the list:
    m_aggrAOSes.erase(it);

    LOG_INFO(3,
      "Pairs-MF::IPair::RemoveInactiveAggrOrder({}): AOSID={}, Instr={}, {}: "
      "Removed as Inactive",
      a_where, a_aos->m_id, a_aos->m_instr->m_FullName.data(),
      a_aos->m_isBuy ? "Bid" : "Ask")
  }

  //=========================================================================//
  // "IPair::ModifyPeggedOrder":                                             //
  //=========================================================================//
  inline void Pairs_MF::IPair::ModifyPeggedOrder
  (
    AOS const*        a_aos,      // Non-NULL
    OrderBook const&  a_ob,
    utxx::time_val    a_ts_exch,
    utxx::time_val    a_ts_recv,
    utxx::time_val    a_ts_strat
  )
  {
    // Consistency Checks:
    assert(a_aos != nullptr  && m_aggrMode == AggrModeT::Pegged &&
           a_aos->m_userData.Get<OrderInfo>().m_isAggr          &&
           a_aos->m_omc   == m_aggrOMC                          &&
           a_aos->m_instr == &(a_ob.GetInstr()));

    if (utxx::unlikely(a_aos->m_isInactive || a_aos->m_isCxlPending != 0))
      return;  // Cannot Modify!

    // Get the last Req:
    Req12   const* req   = a_aos->m_lastReq;
    assert(req != nullptr);
    SecDefD const& instr = *(a_aos->m_instr);

    // Guard (once again!) against any unusual Statuses:
    if (utxx::unlikely
       (req->m_kind    == Req12::KindT::Cancel     ||
        req->m_status  >= Req12::StatusT::Cancelled))
      return;

    // Get the NewPx:
    PriceT newPx = a_aos->m_isBuy ? a_ob.GetBestBidPx() : a_ob.GetBestAskPx();
    if (utxx::unlikely(!IsFinite(newPx) || newPx == req->m_px))
      // Missing or unchanged NewPx -- nothing to do:
      return;

    // Qty is the unfilled qty of this order (FIXME: re-issuing of Pegged
    // Orders is required  if Part-Filled ords  cannot be moved -- but in
    // FORTS, they can):
    long   qty(a_aos->GetLeavesQty<QTU,QR>());
    assert(qty > 0);

    // Finally: Do Modification:
    try
    {
      // NB: IsAggr=false, BatchSend=false:
      bool ok = m_aggrOMC->ModifyOrder
      (
        a_aos,     newPx,      false,      QtyAny(qty, QTU),
        a_ts_exch, a_ts_recv,  a_ts_strat, false
      );
      // "ModifyOrder" should not return "false" in this case, because pre-conds
      // were satisfied:
      if (utxx::unlikely(!ok))
        throw utxx::runtime_error("ModifyOrder returned False");
    }
    catch (EPollReactor::ExitRun const&)
    {
      // This exception is always propagated:
      throw;
    }
    catch (std::exception const& exc)
    {
      // XXX: A failure to modify a Pegged Order is probably not critical, be-
      // cause it would only affect the time-to-fill, eventually:
      LOG_WARN(2,
        "Pairs-MF::IPair::ModifyPeggedOrder: FAILED: Instr={}, {}, NewPx={}, "
        "Qty={}, AOSID={}: {}",  instr.m_FullName.data(),
        a_aos->m_isBuy ? "Bid" : "Ask", double(newPx), qty, exc.what())
      return;
    }

    // If OK:
    LOG_INFO(3,
      "Pairs-MF::IPair::ModifyPeggedOrder: Instr={}, {}, NewPx={}, Qty={}, "
      "AOSID={}: NewReqID={}", instr.m_FullName.data(),
      a_aos->m_isBuy ? "Bid" : "Ask", double(newPx), qty, a_aos->m_id,
      req->m_id)
  }

  //=========================================================================//
  // Qty Computations:                                                       //
  //=========================================================================//
} // End namespace MAQUETTE
