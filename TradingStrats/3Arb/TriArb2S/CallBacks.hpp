// vim:ts=2:et
//===========================================================================//
//                "TradingStrats/3Arb/TriArb2S/CallBacks.hpp":               //
//                 All Call-Backs of the "Strategy" Interface                //
//===========================================================================//
#pragma  once
#include "TradingStrats/3Arb/TriArb2S/TriArb2S.h"
#include <utxx/convert.hpp>

namespace MAQUETTE
{
  //=========================================================================//
  // "OnTradingEvent" Call-Back:                                             //
  //=========================================================================//
  void TriArb2S::OnTradingEvent
  (
    EConnector const& a_conn,
    bool              a_on,
    SecID             UNUSED_PARAM(a_sec_id),
    utxx::time_val    a_ts_exch,
    utxx::time_val    a_ts_recv
  )
  {
    //-----------------------------------------------------------------------//
    // Log the event:                                                        //
    //-----------------------------------------------------------------------//
    utxx::time_val stratTime = utxx::now_utc();

    LOG_INFO(1,
      "TriArb2S::OnTradingEvent: {} is now {}",
      a_conn.GetName(), a_on ? "ACTIVE" : "INACTIVE")
    m_logger->flush();

    //-----------------------------------------------------------------------//
    // Any Connector has become Inactive?                                    //
    //-----------------------------------------------------------------------//
    // If this is not a previously-initiated stop, then terminate now  -- we
    // cannot continue with non-working Connector(s).   XXX: We do not check
    // whether this condition applies to one SecID or all of them:
    //
    if (!a_on)
    {
      // Certainly, some Connector has become Inactive:
      m_allConnsActive = false;

      // In any case: If all Connectors have actually stopped, exit from the
      // Reactor Loop, unless this call-back is invoked from within the Dtor:
      if (!(m_mdcMICEX.IsActive()  || m_mdcAlfa.IsActive()   ||
            m_omcMICEX0.IsActive() || m_omcMICEX1.IsActive() ||
            m_omcAlfa.IsActive()   || m_isInDtor))
      {
        m_riskMgr->OutputAssetPositions("TriArb2S::OnTradingEvent", 0);
        m_reactor->ExitImmediately     ("TriArb2S::OnTradingEvent");
      }

      // Otherwise: We CANNOT proceed with ANY non-working Connector, so stop.
      // XXX: We can still possibly do a delayed stop:
      DelayedGracefulStop(a_ts_exch, a_ts_recv, stratTime);
      return;
    }

    //-----------------------------------------------------------------------//
    // Otherwise: Some Connector has become Active:                          //
    //-----------------------------------------------------------------------//
    // Will need the curr time-of-the-day:
    utxx::time_val currTime = GetCurrTime();

    bool  isFinishedTOD =
          currTime >= m_rollOverTimeRUB || currTime >= m_rollOverTimeEUR ||
          m_isFinishedQuotesTOD;
    // Whatever is earlier... But in fact, LastQuoteTime must precede RollOver-
    // Time

    //-----------------------------------------------------------------------//
    // MICEX MDC is now Active?                                              //
    //-----------------------------------------------------------------------//
    if (&a_conn == &m_mdcMICEX)
    {
      assert(m_mdcMICEX.IsActive());

      // Subscribe to all MICEX Instrs (can use "m_passIntrs" for that).  XXX:
      // Currently, we only use L1 Px or Qty Update events (but not L2) -- but
      // eventually, may need full OrderBook depth:
      //
      for (int p = 0; p < 4; ++p)
      {
        int T = p / 2;                            // 0=TOD,     1=TOM
        int I = p % 2;                            // 0=EUR/RUB, 1=USD/RUB
        SecDefD const* instr = m_passInstrs[T][I];
        assert(instr != nullptr);

        bool skip = (T == TOD && isFinishedTOD);
        if  (skip)
          // De-configure the corresp OrderBook -- it will not be used:
          m_orderBooks[p] = nullptr;
        else
        {
          assert(m_orderBooks[p] != nullptr);
          // NB: RegisterInstrRisks=true:
          m_mdcMICEX.SubscribeMktData
            (this, *instr, OrderBook::UpdateEffectT::L1Qty, true);
        }
      }
      // Register MICEX RUB->USD Converters for TOD and TOM with the RiskMgr,
      // using the corresp OrderBooks.
      // NB: We can use those OrderBooks in "InstallValuator" (provided that
      // they exist, of course) not waiting for them to become "Ready", because
      // the FAST Connector had already properly unitialised them:
      //
      OrderBook const* convRUB_TOD = m_orderBooks[2*TOD + CB];  // May be NULL
      OrderBook const* convRUB_TOM = m_orderBooks[2*TOM + CB];  // Non-NULL
      assert(convRUB_TOM != nullptr);

      if (convRUB_TOD == nullptr)
        // USD/RUB_TOD is already unavailable -- but still need to  value our
        // (notionally TOD)  RUB positions, so use TOM and swap rates:
        m_riskMgr->InstallValuator
        (
          m_RUB.data(),
          m_TOD,
          convRUB_TOM,
          - m_CBSwapPxs[Ask],
          - m_CBSwapPxs[Bid]
        );
      else
        // USD/RUB_TOD is available for now,  but  the Strategy may continue to
        // operate past the cut-off time, it would become unavailable; so regis-
        // ter BOTH OrderBooks -- TOM will be used with a swap rate:
        m_riskMgr->InstallValuator
        (
          m_RUB.data(),
          m_TOD,
          convRUB_TOD,
          0.0,
          0.0,
          m_rollOverTimeRUB,
          convRUB_TOM,
          - m_CBSwapPxs[Ask],
          - m_CBSwapPxs[Bid]
        );

      // Then install a converter for RUB_TOM -- always available:
      m_riskMgr->InstallValuator(m_RUB.data(), m_TOM_UR, convRUB_TOM);
    }
    else
    //-----------------------------------------------------------------------//
    // AlfaFIX2 MDC is now Active?                                            //
    //-----------------------------------------------------------------------//
    if (&a_conn == &m_mdcAlfa)
    {
      assert(m_mdcAlfa.IsActive());

      // Subscribe to the AlfaFIX2 Instrs. XXX: Unlike MICEX, AlfaFIX2 MktData
      // are not used yet beyond L1; even L1 qtys are currently not used (deem-
      // ed to be always sufficient). Such instrs are only used as "covers" for
      // the corresp Passive orders, so for them, TOD is limited  by  its last
      // quoting time:
      //
      for (int p = 0; p < 4; ++p)
      {
        int T = p / 2;                            // 0=TOD,     1=TOM
        int I = p % 2;                            // 0=EUR/RUB, 1=USD/RUB
        SecDefD const*  instr = m_aggrInstrs[T][I];
        assert(m_useOptimalRouting == (instr != nullptr));

        // NB: AlfaFIX2 instrs are (apart from AC) are not required if optimal
        // routing is not used (and may be unavailable in that case):
        //
        if (!m_useOptimalRouting || (T == TOD && isFinishedTOD))
          // De-configure the OrderBook which will not be used anyway:
          m_orderBooks[4 + p] = nullptr;
        else
        {
          assert(m_orderBooks[4 + p] != nullptr);
          // XXX: RegisterInstrRisks=true here as well, ALTHOUGH it would be
          // better to use MICEX for all AB abd CB risks evaluation:
          m_mdcAlfa.SubscribeMktData
            (this, *instr, OrderBook::UpdateEffectT::L1Px, true);
        }
      }

      // And finally AC = EUR/USD_SPOT on AlfaFIX2 -- always available, and
      // always required; again, RegisterInstrRisks=true:
      assert(m_orderBooks[8] != nullptr);
      m_mdcAlfa.SubscribeMktData
        (this, *m_AC, OrderBook::UpdateEffectT::L1Px, true);

      // Register EUR->USD Converters for TOD:
      m_riskMgr->InstallValuator
      (
        m_EUR.data(),
        m_TOD,
        m_orderBooks[8],    // EUR/USD_SPOT
        - m_ACSwapPxs[TOD][Ask],
        - m_ACSwapPxs[TOD][Bid]
      );

      // Similar for EUR_TOM. NB: the SettlDate here is "m_TOM_ER" because
      // EUR/ROM can only occur in EUR/RUB_TOM:
      m_riskMgr->InstallValuator
      (
        m_EUR.data(),
        m_TOM_ER,
        m_orderBooks[8],
        - m_ACSwapPxs[TOM][Ask],
        - m_ACSwapPxs[TOM][Bid]
      );

      // And, because we will get covering positions in EUR/USD_SPOT as well,
      // need to convert EUR_SPOT into USD -- using the very same instrument:
      //
      m_riskMgr->InstallValuator(m_EUR.data(), m_SPOT, m_orderBooks[8]);
    }

    //-----------------------------------------------------------------------//
    // Check whether ALL MDCs and OMCs are now Active:                       //
    //-----------------------------------------------------------------------//
    // If so, Steady-Mode Call-Backs (eg "OnOrderBookUpdate") will become avai-
    // lable, and the RiskMgr will be switched on.  For that, we also need all
    // OrderBooks to be available (on both Bid and Ask sides):
    //
    CheckAllConnectors(currTime);
  }

  //=========================================================================//
  // "CheckAllConnectors":                                                   //
  //=========================================================================//
  // NB: The arg in a INTRA-DAY time:
  //
  void TriArb2S::CheckAllConnectors(utxx::time_val a_curr_time)
  {
    if (utxx::likely(m_allConnsActive))
      return;

    // Otherwise: The flag is not set yet:
    m_allConnsActive =
      m_mdcMICEX.IsActive()  && m_mdcAlfa.IsActive()   &&
      m_omcMICEX0.IsActive() && m_omcMICEX1.IsActive() &&
      m_omcAlfa.IsActive();

    if (!m_allConnsActive)
      return;   // Not yet

    // Otherwise, make sure that all OrderBooks have L1 Pxs available (except
    // for TOD instrs if they are no longer availabe for today):
    //
    // The following flags are similar to those used in "OnTradingEvent":
    //
    bool  isFinishedTOD  =
          a_curr_time >= m_rollOverTimeRUB ||
          a_curr_time >= m_rollOverTimeEUR || m_isFinishedQuotesTOD;

    for (int p = 0; p < 9; ++p)
    {
      // TOD instrs correspond to p = 0, 1, 4 and 5:
      // p = 0: AB passive
      // p = 4: AB aggressive
      // p = 1: CB passive
      // p = 5: CB aggressive
      // The following time limits apply:
      //
      if (((p == 0 || p == 4 || p == 1 || p == 5) && isFinishedTOD) ||
          ( 0 <= p && p <= 7 && !m_useOptimalRouting))
        // Then do not wait for that OrderBook to be ready -- this will NOT
        // happen!
        continue;

      // NB: It might happen that the Connectors are "active" (hence  we have
      // got here), but the corresp TradingEvent(s) were not sent yet, so the
      // OrderBook(s) are not yet constructed:
      OrderBook const*   ob = m_orderBooks[p];

      // Check the availability of L1 Pxs:
      m_allConnsActive =
        m_allConnsActive             && ob != nullptr             &&
        IsFinite(ob->GetBestBidPx()) && IsFinite(ob->GetBestAskPx());

      if (!m_allConnsActive)
      {
        // NB: Don't use "ob" here (it may be NULL), use the corresp SecDef
        // instead!
        SecDefD const& notReady =
          (p <= 3)
          ? *m_passInstrs[p/2][p%2] :
          (p <= 7)
          ? *m_aggrInstrs[(p-4)/2][(p-4)%2]
          : *m_AC;

        LOG_INFO(3,
          "TriArb2S::CheckAllConnectors: Symbol={}, SettlDate={} (Idx={}): "
          "OrderBook not ready yet",
          notReady.m_Symbol.data(), notReady.m_SettlDate, p)

        return;
      }
    }
    // If we got here, the result is positive:
    assert(m_allConnsActive);

    // We can now set up the "m_risks{EUR,RUB}_TOM" ptrs (will be needed  for
    // Ccy->USD conversions in "AdjustCcyTails") and enable the RiskMgr.  It
    // does not matter much which SettlDate we use here -- just make sure they
    // are available. If this fails for any reason, terminate immediately:
    try
    {
      m_risksEUR_TOM = &(m_riskMgr->GetAssetRisks(m_EUR.data(), m_TOM_ER, 0));
      m_risksRUB_TOM = &(m_riskMgr->GetAssetRisks(m_RUB.data(), m_TOM_UR, 0));
      m_riskMgr->Start();
    }
    catch (std::exception const& exc)
    {
      LOG_CRIT(1,
        "TriArb2S::CheclAllConnections: RiskMgr::Start Failed: {}, EXITING...",
        exc.what())
      m_logger ->flush();
      m_reactor->ExitImmediately("RiskMgr::Start Failed");
    }

    // Produce a log msg:
    LOG_INFO(1, "ALL CONNECTORS ARE NOW ACTIVE, RiskMgr Started!")
    m_logger->flush();

    // Output the initial positions:
    m_riskMgr->OutputAssetPositions("TriArb2S::CheckAllConnectors", 0);
  }

  //=========================================================================//
  // "CheckTOD":                                                             //
  //=========================================================================//
  // Maybe TOD instrs are already finished?
  //
  void TriArb2S::CheckTOD(utxx::time_val a_now)
  {
    if (m_isFinishedQuotesTOD)
      return;    // Already finished

    if (utxx::unlikely(a_now - GetCurrDate() > m_lastQuoteTimeTOD))
    {
      // TOD corresponds  to the initial 4 Passive AOSes. These instrs are no
      // longer available for today. Explicitly cancel all outstanding TOD or-
      // ders:
      // XXX: For clarity, we still use a double-loop -- this is an "unlikely"
      // (so not a time-critical) branch:
      //
      for (int I = 0; I < 2; ++I)
      for (int S = 0; S < 2; ++S)
      {
        AOS const* todAOS  = m_passAOSes[TOD][I][S];
        if  (todAOS != nullptr)
          // This must be a MICEX OMC:
          (void) CancelOrderSafe<true>   // Buffered
                 (GetOMCMX(todAOS), todAOS, a_now, a_now, a_now);
      }
      // Now flush the orders accumulated:
      (void) m_omcMICEX0.FlushOrders();
      (void) m_omcMICEX1.FlushOrders();

      // There will be no more quotes for TOD:
      m_isFinishedQuotesTOD = true;
    }
  }

  //=========================================================================//
  // "OnOrderBookUpdate" Call-Back:                                          //
  //=========================================================================//
  void TriArb2S::OnOrderBookUpdate
  (
    OrderBook const&          a_ob,
    bool                      a_is_error,
    OrderBook::UpdatedSidesT  UNUSED_PARAM(a_sides),
    utxx::time_val            a_ts_exch,
    utxx::time_val            a_ts_recv,
    utxx::time_val            a_ts_strat
  )
  {
    RefreshThrottlers(a_ts_strat);

    //-----------------------------------------------------------------------//
    // Is it time to stop now?                                               //
    //-----------------------------------------------------------------------//
    // (*) If we got an error, OrderBook may not be completey up-to-date  (eg
    //     there was a Bid-Ask collision or something like that). In that case,
    //     we will gracefully shut down the Strategy.
    // (*) Same action if the RiskMgr turned into Safe mode:
    //
    SecDefD const& instr = a_ob.GetInstr();

    if (utxx::unlikely(a_is_error || m_riskMgr->GetMode() == RMModeT::Safe))
    {
      if (m_delayedStopInited.empty() && !m_nowStopInited)
      {
        // First time this condition was recordered -- not stopping yet. XXX:
        // This condition should be very rare, so use of "sprintf" is OK:
        if (a_is_error)
        {
          char    buff[256];
          sprintf(buff, "OrderBook Error: Symbol=%s, SettlDate=%d",
                        instr.m_Symbol.data(), instr.m_SettlDate);
          WriteStatusLine(buff);
        }
        else
          WriteStatusLine("OrderBook Update: RiskMgr has entered SafeMode");

        // Cancel all passive orders, wait for final reports:
        DelayedGracefulStop(a_ts_exch, a_ts_recv, a_ts_strat);
      }
      return;
    }

    // Delay for "DelayedGracefulStop" has expired?
    if (utxx::unlikely
       (!m_delayedStopInited.empty()           && !m_nowStopInited  &&
       (a_ts_strat - m_delayedStopInited).sec() > GracefulStopTimeSec))
    {
      GracefulStop();
      assert(m_nowStopInited);
      return;
    }

    // In any case: Do not go any further if in any stopping mode:
    if (utxx::unlikely(!m_delayedStopInited.empty() || m_nowStopInited))
      return;
    assert(!a_is_error && m_riskMgr->GetMode() != RMModeT::Safe);

    //-----------------------------------------------------------------------//
    // No stopping: Go ahead:                                                //
    //-----------------------------------------------------------------------//
    // Can we still do TOD? NB: the following call only updates the flags:
    CheckTOD(a_ts_strat);

    // Status of Connectors availability may change on OrderBook updates. NB:
    // this check is to be done AFTER CheckTOD():
    CheckAllConnectors(a_ts_strat - GetCurrDate());

    // (*) Not ready to proceed if not all Connectors are Active yet;
    // (*) Also, nothing to do if the max number of Rounds is reached -- all
    //     quotes should have been removed  once we got that, so  nothing to
    //     modify or re-quote:
    if (utxx::unlikely
       (!m_allConnsActive || m_roundsCount >= m_maxRounds))
      return;

    //-----------------------------------------------------------------------//
    // Find which OrderBook this update belongs to:                          //
    //-----------------------------------------------------------------------//
    // NB: OrderBooks currently used for extraction of base pxs for Quoting are
    // currently [0..3] for PassInstrs and [8] for AC; [4..7] are used for spo-
    // radic covering orders only, so their updates are NOT used to recalculate
    // the Quotes:
    int n = 0;
    for (; n < 9 && m_orderBooks[n] != &a_ob; ++n);

    if (n > 3 && n != 8)
      return; // Ignore updates to those books (XXX: also if Book not found!)

    //-----------------------------------------------------------------------//
    // Re-Quote Synthetic Instrs based on this update:                       //
    //-----------------------------------------------------------------------//
    // Will find out whether we need to re-quote any of the passive instrs:
    PriceT  bidPx;        // NaN
    PriceT  askPx;        // NaN
    PriceT& prevBidPx     = m_prevBasePxs[n][Bid];               // Ref!
    PriceT& prevAskPx     = m_prevBasePxs[n][Ask];               // Ref!

    // Also, have the corresp L1 pxs changed?
    PriceT  bestBidPx     = a_ob.GetBestBidPx();
    PriceT  bestAskPx     = a_ob.GetBestAskPx();
    PriceT& prevBestBidPx = m_prevL1Pxs  [n][Bid];               // Ref!
    PriceT& prevBestAskPx = m_prevL1Pxs  [n][Ask];               // Ref!
    bool    bestBidUpd    = (bestBidPx != prevBestBidPx);
    bool    bestAskUpd    = (bestAskPx != prevBestAskPx);

    // Memoise the new L1 pxs:
    prevBestBidPx        = bestBidPx;
    prevBestAskPx        = bestAskPx;
    bool    ok           = false;  // Any quotes actually placed?

    switch (n)
    {
    case 0:
    {
      //---------------------------------------------------------------------//
      // T==0 (TOD), I==0 (AB = EUR/RUB) updated on MICEX:                   //
      //---------------------------------------------------------------------//
      // May need to re-quote  TOD CB (same side(s) as AB):
      // So re-calculate  the VWAP-type base pxs    of AB:
      bidPx = GetVWAP(TOD, AB, Bid, m_reserveQtys[AB]);
      askPx = GetVWAP(TOD, AB, Ask, m_reserveQtys[AB]);

      // Have the Base and L1 Px(s) changed?
      bool   bidPxUpd = (bidPx != prevBidPx);
      bool   askPxUpd = (askPx != prevAskPx);

      // Try to re-quote CB if the Effective Base Px of AB has changed:
      if (bidPxUpd)
        ok |= Quote<TOD, CB, Bid, MICEX>
                   (bidPx,    a_ts_exch, a_ts_recv, a_ts_strat);
      if (askPxUpd)
        ok |= Quote<TOD, CB, Ask, MICEX>
                   (askPx,    a_ts_exch, a_ts_recv, a_ts_strat);

      // May also need to re-quote AB itself because the OrderBook constraints
      // have changed:
      if (bestBidUpd)
        ok |= Quote<TOD, AB, Bid, MICEX>
                   (PriceT(), a_ts_exch, a_ts_recv, a_ts_strat);
      if (bestAskUpd)
        ok |= Quote<TOD, AB, Ask, MICEX>
                   (PriceT(), a_ts_exch, a_ts_recv, a_ts_strat);
      break;
    }
    case 1:
    {
      //---------------------------------------------------------------------//
      // T==0 (TOD), I==1 (CB = USD/RUB) updated on MICEX:                   //
      //---------------------------------------------------------------------//
      // May need to re-quote  TOD AB (same side(s) as CB):
      // So re-calculate  the VWAP-type base pxs    of CB:
      bidPx = GetVWAP(TOD, CB, Bid, m_reserveQtys[CB]);
      askPx = GetVWAP(TOD, CB, Ask, m_reserveQtys[CB]);

      // Have the base px(s) changed?
      bool   bidPxUpd = (bidPx != prevBidPx);
      bool   askPxUpd = (askPx != prevAskPx);

      // Try to re-quote AB if the Effective Base Px of CB has changed:
      if (bidPxUpd)
        ok |= Quote<TOD, AB, Bid, MICEX>
                   (bidPx,    a_ts_exch, a_ts_recv, a_ts_strat);
      if (askPxUpd)
        ok |= Quote<TOD, AB, Ask, MICEX>
                   (askPx,    a_ts_exch, a_ts_recv, a_ts_strat);

      // May also need to re-quote CB itself because the OrderBook constraints
      // have changed:
      if (bestBidUpd)
        ok |= Quote<TOD, CB, Bid, MICEX>
                   (PriceT(), a_ts_exch, a_ts_recv, a_ts_strat);
      if (bestAskUpd)
        ok |= Quote<TOD, CB, Ask, MICEX>
                   (PriceT(), a_ts_exch, a_ts_recv, a_ts_strat);
      break;
    }
    case 2:
    {
      //---------------------------------------------------------------------//
      // T==1 (TOM), I==0 (AB = EUR/RUB) updated on MICEX:                   //
      //---------------------------------------------------------------------//
      // Simular to n==0 but for TOM:
      bidPx = GetVWAP(TOM, AB, Bid, m_reserveQtys[AB]);
      askPx = GetVWAP(TOM, AB, Ask, m_reserveQtys[AB]);

      // Have the base px(s) changed?
      bool   bidPxUpd = (bidPx != prevBidPx);
      bool   askPxUpd = (askPx != prevAskPx);

      // Try to re-quote CB if the Effective Base Px of AB has changed:
      if (bidPxUpd)
        ok |= Quote<TOM, CB, Bid, MICEX>
                   (bidPx,    a_ts_exch, a_ts_recv, a_ts_strat);
      if (askPxUpd)
        ok |= Quote<TOM, CB, Ask, MICEX>
                   (askPx,    a_ts_exch, a_ts_recv, a_ts_strat);

      // May also need to re-quote AB itself because the OrderBook constraints
      // have changed:
      if (bestBidUpd)
        ok |= Quote<TOM, AB, Bid, MICEX>
                   (PriceT(), a_ts_exch, a_ts_recv, a_ts_strat);
      if (bestAskUpd)
        ok |= Quote<TOM, AB, Ask, MICEX>
                   (PriceT(), a_ts_exch, a_ts_recv, a_ts_strat);
      break;
    }
    case 3:
    {
      //---------------------------------------------------------------------//
      // T==1 (TOM), I==1 (CB = USD/RUB) updated on MICEX:                   //
      //---------------------------------------------------------------------//
      // Similar to n==1 but for TOM:
      bidPx = GetVWAP(TOM, CB, Bid, m_reserveQtys[CB]);
      askPx = GetVWAP(TOM, CB, Ask, m_reserveQtys[CB]);

      // Have the base px(s) changed?
      bool   bidPxUpd = (bidPx != prevBidPx);
      bool   askPxUpd = (askPx != prevAskPx);

      // Try to re-quote AB if the Effective Base Px of CB has changed:
      if (bidPxUpd)
        Quote<TOM, AB, Bid, MICEX>(bidPx, a_ts_exch, a_ts_recv, a_ts_strat);
      if (askPxUpd)
        Quote<TOM, AB, Ask, MICEX>(askPx, a_ts_exch, a_ts_recv, a_ts_strat);

      // May also need to re-quote CB itself because the OrderBook constraints
      // have changed:
      if (bestBidUpd)
        ok |= Quote<TOM, CB, Bid, MICEX>
                   (PriceT(), a_ts_exch, a_ts_recv, a_ts_strat);
      if (bestAskUpd)
        ok |= Quote<TOM, CB, Ask, MICEX>
                   (PriceT(), a_ts_exch, a_ts_recv, a_ts_strat);
      break;
    }
    case 8:
    {
      //---------------------------------------------------------------------//
      // AC = EUR/USD_SPOT updated on AlfaFIX2:                               //
      //---------------------------------------------------------------------//
      // XXX: Because quotes for AC are already banded with sufficiently wide
      // bands (normally significantly wider than the cover order size), just
      // take the BestBid and BestAsk here:
      bidPx = bestBidPx;
      askPx = bestAskPx;

      if (bestBidUpd)
      {
        // Re-quote (TOD, TOM) AB Bid and (TOD, TOM) CB Ask:
        // NB: AC itself is not quoted, of course!
        // The following invariant is consistent with "Quote" algorithm:
        static_assert
          ((AB ^ Bid) & (CB ^ Ask),
           "TriArb2S::OnOrderBookUpdate: AC Bid Invariant Failure");

        ok |= Quote<TOD, AB, Bid, AlfaFIX2>
                   (bidPx, a_ts_exch, a_ts_recv, a_ts_strat);
        ok |= Quote<TOD, CB, Ask, AlfaFIX2>
                   (bidPx, a_ts_exch, a_ts_recv, a_ts_strat);

        ok |= Quote<TOM, AB, Bid, AlfaFIX2>
                   (bidPx, a_ts_exch, a_ts_recv, a_ts_strat);
        ok |= Quote<TOM, CB, Ask, AlfaFIX2>
                   (bidPx, a_ts_exch, a_ts_recv, a_ts_strat);
      }
      if (bestAskUpd)
      {
        // Re-quote (TOD, TOM) A/B Ask and (TOD, TOM) C/B Bid:
        // NB: A/C itself is not quoted, of course!
        // The following invariant is consistent with "Quote" algorithm:
        static_assert
          (!(AB ^ Ask) & !(CB ^ Bid),
           "TriArb2S::OnOrderBookUpdate: AC Ask Invariant Failure");

        ok |= Quote<TOD, AB, Ask, AlfaFIX2>
                   (askPx, a_ts_exch, a_ts_recv, a_ts_strat);
        ok |= Quote<TOD, CB, Bid, AlfaFIX2>
                   (askPx, a_ts_exch, a_ts_recv, a_ts_strat);

        ok |= Quote<TOM, AB, Ask, AlfaFIX2>
                   (askPx, a_ts_exch, a_ts_recv, a_ts_strat);
        ok |= Quote<TOM, CB, Bid, AlfaFIX2>
                   (askPx, a_ts_exch, a_ts_recv, a_ts_strat);
      }
      break;
    }
    default:
      // Unreachible:
      __builtin_unreachable();
    }

    // Memoise the new Base Pxs:
    prevBidPx = bidPx;
    prevAskPx = askPx;

    // Log the curr OrderBook L1 Data -- only AFTER all processing has been
    // done.   If no quote is placed,  output the info only if the L1 Px(s)
    // have changed:
    if (ok || bestBidUpd || bestAskUpd)
    {
      QtyM bestBidQty = a_ob.GetBestBidQty<QTM,QRM>();
      QtyM bestAskQty = a_ob.GetBestAskQty<QTM,QRM>();
      LOG_INFO(3,
        "{}: {}: [{}] {} .. {} [{}], Latency={} usec{}",
        ok ? "(In response to: OrderBookUpdate: " : "",
        a_ob.GetInstr().m_Symbol.data(),
        QRO(ToContracts::Convert<QRM,QRO>(bestBidQty, instr, PriceT())),
        double(a_ob.GetBestBidPx()),
        double(a_ob.GetBestAskPx()),
        QRO(ToContracts::Convert<QRM,QRO>(bestAskQty, instr, PriceT())),
        (a_ts_strat - a_ts_recv).microseconds(),
        ok ? ")" : "")
      // No flush here -- this output could be very frequent
    }
    // All Done!
  }

  //=========================================================================//
  // "OnTradeUpdate" Call-Back:                                              //
  //=========================================================================//
  // NB: This method is a PLACEHOLDER -- it is required by virtual interface,
  // but it is not actually used as yet. One way it could be used is to compu-
  // te Mkt Pressure for cont px prediction (of src pxs  from which synthetic
  // quote pxs are generated):
  //
  inline void TriArb2S::OnTradeUpdate(Trade const&) {}

  //=========================================================================//
  // "OnOurTrade" Call-Back:                                                 //
  //=========================================================================//
  // Invoked when we got a Fill or PartFill of our order:
  //
  void TriArb2S::OnOurTrade(Trade const& a_tr)
  {
    //-----------------------------------------------------------------------//
    // Get the AOS and OrderInfo:                                            //
    //-----------------------------------------------------------------------//
    utxx::time_val    stratTime = utxx::now_utc();
    utxx::time_val    exchTime  = a_tr.m_exchTS;
    utxx::time_val    recvTime  = a_tr.m_recvTS;
    RefreshThrottlers(stratTime);

    Req12 const* req = a_tr.m_ourReq;
    assert(req != nullptr);
    AOS const* aos   = req->m_aos;
    assert(aos != nullptr);
    PriceT  px       = a_tr.m_px;
    QtyO   qty  = a_tr.GetQty<QTO,QRO>();
    assert(IsFinite(px) && IsPos(qty));

    // Get the OrderInfo:
    OrderInfo& ori = aos->m_userData.Get<OrderInfo>();
    TrInfo*    tri = ori.m_trInfo;

    //-----------------------------------------------------------------------//
    // Passive Quote Filled (or Part-Filled)?                                //
    //-----------------------------------------------------------------------//
    if (!ori.m_isAggr)
    {
      assert((ori.m_passT == TOD || ori.m_passT == TOM) &&
             (ori.m_passI == AB  || ori.m_passI == CB));

      if (utxx::likely(aos->m_isInactive))
        // Complete Fill (or the order has become Inactive for any other reas-
        // on): Remove this "aos" from "m_passAOSes":
        m_passAOSes[ori.m_passT][ori.m_passI][int(aos->m_isBuy)] = nullptr;
      else
      {
        // Partial Fill:
        // Cancel the rest of the order (in case it is not cancelled automat-
        // ically) but do NOT reset the corresp "m_passAOSes" ptr yet -- this
        // will be done when the Cancellation is confirmed:
        // This must be a MICEX OMC:
        (void) CancelOrderSafe<false>    // Not buffered
               (GetOMCMX(aos), aos, exchTime, recvTime, stratTime);
      }

      //---------------------------------------------------------------------//
      // Update the TrInfo and Log:                                          //
      //---------------------------------------------------------------------//
      if (utxx::likely(tri != nullptr))
      {
        // NB: "tri" may be NULL if the AOS is an "old" one (created by a pre-
        // vious run):
        // Otherwise :
        // tri->m_passTradedPx is an AvgPx (in case of multiple Fills, though
        // this is very unlikely -- we cancel the order after the 1st partial
        // fill):
        double val =
          double(tri->m_passTradedPx) * double(tri->m_passQtyFilled) +
          double(px) * double(qty);

        tri->m_passQtyFilled += qty;
        tri->m_passTradedPx   = PriceT(val / double(tri->m_passQtyFilled));
      }

      // Log the Trade:  XXX:  There is an overhead for doing that BEFORE doing
      // the Covering orders -- but we need to preserve the logical sequence of
      // events in logs:
      LOG_INFO(2,
        "PASSIVE FILL,{},{},{},{},{}",
        aos->m_id,     aos->m_instr->m_Symbol.data(),
        aos->m_isBuy ? "Bid" : "Ask",  double(px),  QRO(qty))

      //---------------------------------------------------------------------//
      // Now issue the new orders:                                           //
      //---------------------------------------------------------------------//
      // Covering Aggressive Orders:
      DoCoveringOrders(*aos, px, qty, exchTime, recvTime, stratTime);

      //---------------------------------------------------------------------//
      // At this point, we conside the curr Round to be done:                //
      //---------------------------------------------------------------------//
      // (Even though the Covering Orders are only submitted, not filled yet):
      ++m_roundsCount;

      if (utxx::unlikely(m_roundsCount >= m_maxRounds))
      {
        //-------------------------------------------------------------------//
        // Preparing to Stop: Cancel all Passive Orders:                     //
        //-------------------------------------------------------------------//
        // (But still do not stop immediately -- it can only be done manually
        // later on; eg the user may want to wait  until all orders are conf-
        // irmed to be cancelled):
        AOS const** passBegin = &(m_passAOSes[0][0][0]);
        AOS const** passBack  = &(m_passAOSes[1][1][1]);
        for (AOS const** passIt = passBegin; passIt <= passBack; ++passIt)
        {
          AOS const* passAOS = *passIt;
          if (passAOS != nullptr)
          {
            // This must be a MICEX OMC:
            (void) CancelOrderSafe<false>        // Not buffered
                   (GetOMCMX(passAOS), passAOS,
                    exchTime,          recvTime, stratTime);

            LOG_INFO(2,
              "All Rounds Done: Cancelling Quote,{},{},{}",
              passAOS->m_id,    passAOS->m_instr->m_Symbol.data(),
              passAOS->m_isBuy ? "Bid," : "Ask")
          }
        }
      }
      // XXX: If it was a Complete Fill, should we Re-Quote the corresp side
      // right now? -- We currently don't do that:
      // All Done:
      m_riskMgr->OutputAssetPositions("TriArb2S::OnOurTrade(Passive)", 0);

      // At the end, flush the Logger:
      m_logger->flush();
      return;
    }

    //-----------------------------------------------------------------------//
    // If we got here: It should be an Aggressive Order which was Filled:    //
    //-----------------------------------------------------------------------//
    RemoveAggrOrder(*aos, exchTime, recvTime, stratTime);

    LOG_INFO(2,
      "AGGRESSIVE FILL,{},{},{},{},{}",
      aos->m_id,  aos->m_instr->m_Symbol.data(),
      aos->m_isBuy ? "Bid" : "Ask",  double(px), QRO(qty))

    m_riskMgr->OutputAssetPositions("TriArb2S::OnOurTrade(Aggressive)", 0);
    m_logger->flush();
  }

  //=========================================================================//
  // "OnCancel" Call-Back:                                                   //
  //=========================================================================//
  inline void TriArb2S::OnCancel
  (
    AOS const&        a_aos,
    utxx::time_val    a_ts_exch,
    utxx::time_val    a_ts_recv
  )
  {
    utxx::time_val    stratTime = utxx::now_utc();
    RefreshThrottlers(stratTime);

    RemoveOrder
    (
      a_aos,
      nullptr,  // No msg -- so it's Cancel, not Error
      a_ts_exch,
      a_ts_recv,
      stratTime
    );
  }

  //=========================================================================//
  // "OnConfirm" Call-Back:                                                  //
  //=========================================================================//
  // (This could also be for a Cancel):
  //
  inline void TriArb2S::OnConfirm(Req12 const& a_req)
  {
    utxx::time_val    stratTime = utxx::now_utc();
    RefreshThrottlers(stratTime);

    // We only handle "Confirm" events in advanced debug modes (>= 3):
    if (utxx::likely(m_debugLevel < 3))
      return;

    // Find which order has been confirmed -- Passive or aggressive:
    bool   isCancel = (a_req.m_kind == Req12::KindT::Cancel);
    AOS const* aos  = a_req.m_aos;
    assert    (aos != nullptr);
    QtyO       qty  = a_req.GetQty<QTO,QRO>();
    assert((isCancel && IsZero(qty)) || (!isCancel && IsPos(qty)));

    AOS const** passBegin = &(m_passAOSes[0][0][0]);
    AOS const** passEnd   = &(m_passAOSes[1][1][1]) + 1;
    bool  isPassive = (std::find(passBegin, passEnd, aos) != passEnd);

    char const* which =
      isCancel
      ? "Order Cancel"  :
      isPassive
      ? "Passive Order" :
      (std::find(m_aggrAOSes.cbegin(), m_aggrAOSes.cend(), aos) !=
            m_aggrAOSes.cend())
      ? "Aggressive Order"
      : "Unknown Order";

    // The Confirmations are only Logged:
    m_logger->info
      ("{} Confirmed,{},{},{},{},{}",
       which, aos->m_id,         aos->m_instr->m_Symbol.data(),
       aos->m_isBuy ? "Bid"    : "Ask",
       utxx::likely(!isCancel) ? std::to_string(double(a_req.m_px)) : "",
       utxx::likely(!isCancel) ? std::to_string(QRO(qty))           : "");
    // No flush here -- this event is of relatively low importance
  }

  //=========================================================================//
  // "OnOrderError" Call-Back:                                               //
  //=========================================================================//
  inline void TriArb2S::OnOrderError
  (
    Req12 const&    a_req,
    int             UNUSED_PARAM(a_err_code),
    char  const*    a_err_text,
    bool            UNUSED_PARAM(a_prob_filled),  // TODO: Handle it properly!
    utxx::time_val  a_ts_exch,
    utxx::time_val  a_ts_recv
  )
  {
    utxx::time_val    stratTime = utxx::now_utc();
    RefreshThrottlers(stratTime);

    AOS const* aos = a_req.m_aos;
    assert (aos != nullptr);

    // Possibly remove a failed order:
    RemoveOrder
    (
      *aos,
      (a_err_text != nullptr) ? a_err_text : "", // NB: NULL is not OK here
      a_ts_exch,
      a_ts_recv,
      stratTime
    );

    // But it could be a CancelReq failure, eg:
    // (*) we issued a Move before Part-Fill was reported;
    // (*) then got a Part-Fill event;
    // (*) so issued a Cancel (after Part-Fill) relative to the prev Move;
    // (*) then got a Move failure (because Part-Filled orders cannot be moved);
    // (*) and finally got a Cancel failure:
    // a part-filled order will continue to exist. So must explicitly re-cancel
    // it:
    if (utxx::unlikely(a_req.m_kind == Req12::KindT::Cancel))
    {
      bool needReCancel = !(aos->m_isInactive) || (aos->m_isCxlPending != 0);
      LOG_INFO(3,
         "NB: CancelReq FAILED: ReqID={}, AOSID={}, IsInactive={}, "
         "IsCxlPending={}{}",
         a_req.m_id, aos->m_id, aos->m_isInactive,  aos->m_isCxlPending,
         needReCancel ? ": Re-Cancelling the Order" : "")

      if (needReCancel)
      {
        // Currently, we do cancellations on the MICEX Connector only (not buf-
        // fered):
        CancelOrderSafe<false>
          (GetOMCMX(aos), aos, a_ts_exch, a_ts_recv, stratTime);
      }
    }
    else
    if (strstr(a_err_text, "United limit for Bank Account") != nullptr)
    {
      // IMPORTANT:
      // Detect violation of limits on MICEX: Stop the Strategy; however, if it
      // was an AGGRESSIVE Order which has failed this way, redo it over Alfa-
      // ECN to avoid skewed positions:
      // XXX: A corrective action is only possible if OptimalRouting is used:
      //
      OrderInfo const& ori = aos->m_userData.Get<OrderInfo>();
        if (ori.m_isAggr       && m_useOptimalRouting &&
           (ori.m_passT == TOD || ori.m_passT == TOM) &&
           (ori.m_passI == AB  || ori.m_passI == CB))
        {
          // Redo this aggr order but using another instr:
        // NB: [T][I] is for the PASSIVE order being covered, so the Aggr one is
        //     [T][I^1]:
        SecDefD const* newInstr =
            m_aggrInstrs[ori.m_passT][ori.m_passI ^ 1];
        assert(newInstr != nullptr);

        // XXX: All OMCs use same QtyO type here:
        QtyO qty = a_req.GetQty<QTO,QRO>();

        AOS const* aggrAOS =
          NewOrderSafe<true> // IsAggr=true
            (m_omcAlfa,   *newInstr,   aos->m_isBuy, a_req.m_px,  qty,
             a_ts_exch,   a_ts_recv,   stratTime,
             ori.m_passT, ori.m_passI, ori.m_trInfo);

        if (aggrAOS != nullptr)
          LOG_INFO(3,
            "Re-Doing Aggressive Covering Order,AlfaFIX2,{},{}",
            newInstr->m_Symbol.data(), aos->m_isBuy ? "Bid" : "Ask")
      }
      // Save this as a StatusLine:
      WriteStatusLine(a_err_text);

      // In any case, initiate graceful strategy termination (ExecReports from
      // pending orders will still be delivered):
      DelayedGracefulStop(a_ts_exch, a_ts_recv, stratTime);
    }
    // For safety, because it is an error handler, flush the logger:
    m_logger->flush();
  }

  //=========================================================================//
  // "OnSignal" Call-Back:                                                   //
  //=========================================================================//
  inline void TriArb2S::OnSignal(signalfd_siginfo const& a_si)
  {
    ++m_signalsCount;

    LOG_INFO(1,
      "Got a Signal ({}), Count={}, Exiting {}",
      a_si.ssi_signo,     m_signalsCount,
      (m_signalsCount == 1) ? "Gracefully" : "Immediately")
    m_logger->flush();

    if (m_signalsCount == 1)
    {
      // Stop now -- still do a delayed stop:
      utxx::time_val now = utxx::now_utc();
      DelayedGracefulStop  (now, now, now);
    }
    else
    if (utxx::likely(!m_isInDtor))
    {
      assert(m_signalsCount >= 2);
      // Throw the "ExitRun" exception to exit the Main Event Loop (if we are
      // not in that loop, it will be handled safely anyway):
      m_riskMgr->OutputAssetPositions("TriArb2S::OnSignal", 0);
      m_reactor->ExitImmediately     ("TriArb2S::OnSignal");
    }
  }
} // End namespace MAQUETTE
