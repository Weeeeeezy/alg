// vim:ts=2:et
//===========================================================================//
//                "TradingStrats/3Arb/TriArbC0/CallBacks.hpp":               //
//                 All Call-Backs of the "Strategy" Interface                //
//===========================================================================//
#pragma  once
#include "TradingStrats/3Arb/TriArbC0/TriArbC0.h"
#include <utxx/convert.hpp>

namespace MAQUETTE
{
  //=========================================================================//
  // "OnTradingEvent" Call-Back:                                             //
  //=========================================================================//
  template<ExchC0T X>
  void TriArbC0<X>::OnTradingEvent
  (
    EConnector const& a_conn,     // MDC or OMC
    bool              a_on,
    SecID             UNUSED_PARAM(a_sec_id),
    utxx::time_val    a_ts_exch,
    utxx::time_val    a_ts_conn
  )
  {
    //-----------------------------------------------------------------------//
    // Log the event:                                                        //
    //-----------------------------------------------------------------------//
    utxx::time_val stratTime = utxx::now_utc();

    LOG_INFO(1, "OnTradingEvent: {} is now {}",
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
      if (!(m_mdc->IsActive() || m_omc->IsActive() || m_isInDtor))
      {
        m_riskMgr->OutputAssetPositions("OnTradingEvent", 0);
        m_reactor->ExitImmediately     ("OnTradingEvent");
      }

      // Otherwise: We CANNOT proceed with ANY non-working Connector, so stop.
      // XXX: We can still possibly do a delayed stop. TODO: Automated restart
      // after that:
      LOG_ERROR(2, "OMC: {}, MDC: {}, STOPPING...",
        m_omc->IsActive() ? "Active" : "InActive",
        m_mdc->IsActive() ? "Active" : "InActive")

      DelayedGracefulStop(a_ts_exch, a_ts_conn, stratTime);
      return;
    }
    //-----------------------------------------------------------------------//
    // Otherwise: Some Connector (MDC or OMC) has become Active:             //
    //-----------------------------------------------------------------------//
    // If MDC is now Active, Subscribe to MktData:
    //
    if (&a_conn == m_mdc)
    {
      assert(m_mdc->IsActive());

      // Subscribe to all Instrs, with full OrderBook depth, as we need to com-
      // pute VWAPs:
      for (int I = 0; I < 3; ++I)
      {
        SecDefD const* instr = m_instrs[I];
        assert(instr != nullptr && m_orderBooks[I] != nullptr);

        // NB: RegisterInstrRisks=true:
        m_mdc->SubscribeMktData
          (this, *instr, OrderBook::UpdateEffectT::L2, true);
      }
      // Register Asset Valuators with the RiskMgr. We assume that RFC is one of
      // the Ccys A, B or C, so no extra OrderBooks are required to provide  the
      // Valuators:
      Ccy rfc = m_riskMgr->GetRFC();

      if (IsEQ(rfc, m_A))
      {
        // Then AB is a valuator for B,
        //      AC is a valuator for C:
        m_riskMgr->InstallValuator(m_B.data(), 0, m_orderBooks[AB]);
        m_riskMgr->InstallValuator(m_C.data(), 0, m_orderBooks[AC]);
      }
      else
      if (IsEQ(rfc, m_B))
      {
        // Then AB is a valuator for A,
        //      CB is a valuator for C:
        m_riskMgr->InstallValuator(m_A.data(), 0, m_orderBooks[AB]);
        m_riskMgr->InstallValuator(m_C.data(), 0, m_orderBooks[CB]);
      }
      else
      if (IsEQ(rfc, m_C))
      {
        // Then AC is a valuator for A,
        //      CB is a valuator for B:
        m_riskMgr->InstallValuator(m_A.data(), 0, m_orderBooks[AC]);
        m_riskMgr->InstallValuator(m_B.data(), 0, m_orderBooks[CB]);
      }
      else
        throw utxx::badarg_error("UnSupported RFC in RiskMgr: ", rfc.data());
    }
    //-----------------------------------------------------------------------//
    // Check whether ALL MDCs and OMCs are now Active:                       //
    //-----------------------------------------------------------------------//
    // If so, Steady-Mode Call-Backs (eg "OnOrderBookUpdate") will become avai-
    // lable, and the RiskMgr will be switched on.  For that, we also need all
    // OrderBooks to be available (on both Bid and Ask sides):
    //
    CheckAllConnectors();
  }

  //=========================================================================//
  // "OnOrderBookUpdate" Call-Back:                                          //
  //=========================================================================//
  template<ExchC0T  X>
  void TriArbC0<X>::OnOrderBookUpdate
  (
    OrderBook const&          a_ob,
    bool                      a_is_error,
    OrderBook::UpdatedSidesT  a_sides,
    utxx::time_val            a_ts_exch,
    utxx::time_val            a_ts_conn,
    utxx::time_val            a_ts_strat
  )
  {
    //-----------------------------------------------------------------------//
    // Any error or Safe Mode?                                              //
    //-----------------------------------------------------------------------//
    // (*) If we got an error, OrderBook may not be completey up-to-date  (eg
    //     there was a Bid-Ask collision or something like that). In that case,
    //     we will gracefully shut down the Strategy.
    // (*) Same action if the RiskMgr turned into Safe mode:
    //
    SecDefD const& instr = a_ob.GetInstr();

    if (utxx::unlikely (a_is_error || m_riskMgr->GetMode() == RMModeT::Safe))
    {
      if (m_delayedStopInited.empty() && !m_nowStopInited)
      {
        // First time this condition was recordered -- not stopping yet. XXX:
        // This condition should be very rare, so use of "sprintf" is OK:
        if (a_is_error)
        {
          LOG_ERROR(1, "OrderBook Error: Symbol={}, STOPPING...",
                    instr.m_Symbol.data())
        }
        else
        {
          LOG_ERROR(2, "OrderBook Update: RiskMgr has entered SafeMode, "
                    "STOPPING...")
        }
        // Cancel all passive orders, wait for final reports:
        DelayedGracefulStop(a_ts_exch, a_ts_conn, a_ts_strat);
      }
      return;
    }
    // OR: Delay for "DelayedGracefulStop" has expired?
    if (utxx::unlikely
       (!m_delayedStopInited .empty() && !m_nowStopInited            &&
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

    // Status of Connectors availability may change on OrderBook updates:
    CheckAllConnectors();

    // (*) Not ready to proceed if not all Connectors are Active yet;
    // (*) Also, nothing to do if the max number of Rounds is reached -- all
    //     quotes should have been removed  once we got that, so  nothing to
    //     modify or re-quote:
    if (utxx::unlikely(!m_allConnsActive || m_roundsCount >= m_maxRounds))
      return;

    //-----------------------------------------------------------------------//
    // Normal Mode: Find which OrderBook (I) this update belongs to:         //
    //-----------------------------------------------------------------------//
    // XXX: Just perform direct comparison:
    int I = (m_orderBooks[0] == &a_ob) ? 0 : (m_orderBooks[1] == &a_ob) ? 1 : 2;
    assert  (m_orderBooks[I] == &a_ob);
      
    //-----------------------------------------------------------------------//
    // Re-Quote Synthetic Instrs based on this update:                       //
    //-----------------------------------------------------------------------//
    // Will find out whether we need to re-quote any of the passive instrs:
    // Sides of this OrderBook updated (at least one):
    //
    bool   bidUpd =  OrderBook::IsBidUpdated(a_sides);
    bool   askUpd =  OrderBook::IsAskUpdated(a_sides);
    assert(bidUpd || askUpd);

    switch (I)
    {
    case AB:
      if (bidUpd)
      { // Updated Ask(AB): Re-quote Bid(CB) and Bid(AC):
        Quote<CB,Bid>(a_ts_exch, a_ts_conn, a_ts_strat);
        Quote<AC,Bid>(a_ts_exch, a_ts_conn, a_ts_strat);
      }
      if (askUpd)
      { // Updated Bid(AB): Re-quote Ask(CB) and Ask(AC):
        Quote<CB,Ask>(a_ts_exch, a_ts_conn, a_ts_strat);
        Quote<AC,Ask>(a_ts_exch, a_ts_conn, a_ts_strat);
      }
      break;

    case CB:
      if (bidUpd)
      { // Updated Bid(CB): Re-quote Bid(AB) and Ask(AC):
        Quote<AB,Bid>(a_ts_exch, a_ts_conn, a_ts_strat);
        Quote<AC,Ask>(a_ts_exch, a_ts_conn, a_ts_strat);
      }
      if (askUpd)
      { // Updated Ask(CB): Re-quote Ask(AB) and Bid(AC):
        Quote<AB,Ask>(a_ts_exch, a_ts_conn, a_ts_strat);
        Quote<AC,Bid>(a_ts_exch, a_ts_conn, a_ts_strat);
      }
      break;

    case AC:
      if (bidUpd)
      { // Updated Bid(AC): Re-quote Bid(AB) and Ask(CB):
        Quote<AB,Bid>(a_ts_exch, a_ts_conn, a_ts_strat);
        Quote<CB,Ask>(a_ts_exch, a_ts_conn, a_ts_strat);
      }
      if (askUpd)
      { // Updated Ask(AC): Re-quote Ask(AB) and Bid(CB):
        Quote<AB,Ask>(a_ts_exch, a_ts_conn, a_ts_strat);
        Quote<CB,Bid>(a_ts_exch, a_ts_conn, a_ts_strat);
      }
      break;

    default:
      // Unreachable:
      assert(false);
    }
    // All Done!
  }

  //=========================================================================//
  // "OnOurTrade" Call-Back:                                                 //
  //=========================================================================//
  // Invoked when we got a Fill or PartFill of our order. Will trigger firing
  // up of covering aggressive orders:
  //
  template<ExchC0T X>
  void TriArbC0<X>::OnOurTrade(Trade const& a_tr)
  {
    //-----------------------------------------------------------------------//
    // Get the AOS, TrInfo and Instr:                                        //
    //-----------------------------------------------------------------------//
    utxx::time_val stratTime = utxx::now_utc();
    utxx::time_val exchTime  = a_tr.m_exchTS;
    utxx::time_val connTime  = a_tr.m_recvTS;

    Req12  const*   req  = a_tr.m_ourReq;
    assert(req   != nullptr);
    AOS    const*   aos  = req->m_aos;

    // Obviously, the AOS must be real, and we cannot be in DryRun mode:
    assert(aos   != nullptr && !m_dryRun);

    SecDefD const* instr = aos->m_instr;
    assert(instr != nullptr);

    // Get the OrderInfo:
    AOSExt const& ext    = aos->m_userData.Get<AOSExt>();
    TrInfo*       tri    = ext.m_trInfo;
    assert(tri != nullptr);
    int           J      = ext.m_aggrJ; // -1 for Passive, 0..2 for Aggressive!
    assert(-1 <=  J && J <= 2);
    bool          isAggr = (J >= 0);

    // Locate this "TrInfo" among the Currently-Active ones.   This  applies to
    // both Passive and Aggressive Orders. NB: It might not be there; the Trade
    // may be associated with a previously-active "TrInfo",   in which case the
    // "I" and "S" indices will remain (-1):
    int I = 2;
    int S = 1;
    for (; I >= 0; --I)
    for (; S >= 0; --S)
      if (m_curr3s[I][S] == tri)
        goto Found;
    Found:;
    // NB: Either both indices "I" and "S" are invalid (that is, "tri" was not
    // found among "m_curr3s"), or both of them are valid:
    assert ((I == -1 && S == -1) || (0 <= I && I <= 2 && 0 <= S && S <= 1));
    bool isCurr = (I >= 0);

    //-----------------------------------------------------------------------//
    // Extract the Trade Px and Qty:                                         //
    //-----------------------------------------------------------------------//
    // NB: The "original" Qty Type may be different from ours, and the Px may
    // not be a Px(A/B):  So make sure we convert it into Px(A/B):
    //
    QtyO trQty = a_tr.GetQty<QTO,QRO>();
    QtyN   qty =
      QtyConverter<QTO,QT>::template Convert<QRO,QR>
      (trQty, *instr, a_tr.m_px);    // NB: Use the "raw" Px here!

    PriceT px  = instr->GetPxAB(a_tr.m_px);
    assert(IsFinite(px) && IsPos(qty));

    //-----------------------------------------------------------------------//
    // Passive Quote Filled (or Part-Filled)?                                //
    //-----------------------------------------------------------------------//
    // XXX: It is CRITICALLY important to correctly distinguish Passive and Agg-
    // ressive Fills, otherwise we can create an avalanche of covering orders on
    // covering orders.   Also, for a "current" "TrInfo", its intrinsic indices
    // must coincide with the external ones:
    //
    if (!isAggr)
    {
      // Thus, this AOS MUST be listed as a passive one in "TrInfo". For extra
      // safety, we ALWAYS perform this check explicitly:
      if (utxx::unlikely
         (tri->m_passAOS != aos  ||
         (isCurr                 &&
         (I != tri->m_passI || S != tri->m_passS || bool(S) != aos->m_isBuy))))
      {
        LOG_CRIT(1,
          "Critical Inconsistency: Symbol={}, PassAOSID={}, "
          "PassAOS.IsBuy={}, AOSExt.AggrJ={}, TrInfo.PassAOSID={}, I={}, S={}, "
          "TrInfo.PassI={}, TrInfo.PassS={}, STOPPING...",
          instr->m_Symbol.data(),       aos->m_id,  J,  aos->m_isBuy,
          (tri->m_passAOS != nullptr) ? tri->m_passAOS->m_id : 0,
          I, S, tri->m_passI, tri->m_passS)

        // Initiate ShutDown:
        DelayedGracefulStop(exchTime, connTime, stratTime);
        return;
      }

      //---------------------------------------------------------------------//
      // Complete Fill of a Passive Leg?                                     //
      //---------------------------------------------------------------------//
      if (utxx::likely(aos->m_isInactive))
      {
        // Remove this "TrInfo" from the "m_curr3s", so that a new one will la-
        // ter be created by "Quote":
        if (isCurr)
        {
          // "I" and "S" are valid then:
          m_curr3s[I][S] = nullptr;

          // At this point, we conside the curr Round to be done (even though
          // the Covering Orders are not submitted yet!)
          ++m_roundsCount;

          if (utxx::unlikely(m_roundsCount >= m_maxRounds))
          {
            //---------------------------------------------------------------//
            // Preparing to Stop: Cancel all Passive Orders:                 //
            //---------------------------------------------------------------//
            // (But still do not stop immediately -- we at least need to fire up
            // the covering orders!)
            for (int i = 0; i < 3; ++i)
            for (int s = 0; s < 2; ++s)
            {
              // XXX: We only consider CURRENT "TrInfo"s; all others are presum-
              // ably either inactive, or will be inactive soon  (BETTER VERIFY
              // THIS FORMALLY):
              TrInfo*    tis = m_curr3s[i][s];
              AOS const* ais = (tis != nullptr) ? tis->m_passAOS : nullptr;

              if (ais != nullptr)
                (void) CancelOrderSafe
                  (ais, exchTime, connTime, stratTime, "MaxRounds Reached");
            }
          }
        }
      }
      else
      //---------------------------------------------------------------------//
      // The AOS is NOT Inactive, so it can only be a Partial Fill:          //
      //---------------------------------------------------------------------//
        // Cancel the rest of the order (in case it is not cancelled automatic-
        // ally) because we do not further update Part-Filled Quotes.  However,
        // we do not reset the ptr in "m_curr3s" until cancellation is confirm-
        // ed:
        (void) CancelOrderSafe
               (aos, exchTime, connTime, stratTime, "Part-Fill");

      //---------------------------------------------------------------------//
      // Update the Passive Stats, and log the Trade:                        //
      //---------------------------------------------------------------------//
      // XXX: There is an overhead of doing that BEFORE issueing the Covering
      // orders -- but we need to preserve  the logical sequence of events in
      // logs:
      LOG_INFO(2,
        "PASSIVE FILL,{},{},{},{},{}",
        aos->m_id,     aos->m_instr->m_Symbol.data(),
        aos->m_isBuy ? "Bid" : "Ask",  double(px),  QR(qty))

      double cumPx =
        double(tri->m_passFilledQty) * double(tri->m_passAvgFillPx) +
        double(qty)                  * double(px);
      tri->m_passFilledQty += qty;
      tri->m_passAvgFillPx  = PriceT(cumPx / double(tri->m_passFilledQty));

      //---------------------------------------------------------------------//
      // Now issue the Covering (Aggressive) Orders:                         //
      //---------------------------------------------------------------------//
      switch (tri->m_passI)
      {
      case AB:
        if (bool(S))
          ABCoveringOrders<true> (tri, px, qty, exchTime, connTime, stratTime);
        else
          ABCoveringOrders<false>(tri, px, qty, exchTime, connTime, stratTime);
        break;
      case CB:
        if (bool(S))
          CBCoveringOrders<true> (tri, px, qty, exchTime, connTime, stratTime);
        else
          CBCoveringOrders<false>(tri, px, qty, exchTime, connTime, stratTime);
        break;
      case AC:
        if (bool(S))
          ACCoveringOrders<true> (tri, px, qty, exchTime, connTime, stratTime);
        else
          ACCoveringOrders<false>(tri, px, qty, exchTime, connTime, stratTime);
        break;
      default:
        assert(false);
      }
      // XXX: If it was a Complete Fill, should we Re-Quote the corresp side
      // right now? -- We currently don't do that, rather wait for a MktData
      // update:
      DEBUG_ONLY(m_riskMgr->OutputAssetPositions("OnOurTrade(Passive)", 0);)
    }
    else
    //-----------------------------------------------------------------------//
    // An Aggressive Order was Filled / Part-Filled:                         //
    //-----------------------------------------------------------------------//
    {
      // Unlike Passive Fills, no new orders are issued here, so perform a rel-
      // axed check:
      DEBUG_ONLY(int  aggrI  = tri->m_aggrIs[J];)
      DEBUG_ONLY(int  aggrS  = tri->m_aggrSs[J];)
      assert    (0 <= aggrI && aggrI <= 2 && 0 <= aggrS && aggrS <= 1);

      double cumPx =
        double(tri->m_aggrFilledQtys[J]) * double(tri->m_aggrAvgFillPxs[J]) +
        double(qty)                      * double(px);
      tri->m_aggrFilledQtys[J] += qty;
      tri->m_aggrAvgFillPxs[J]  =
        PriceT(cumPx / double(tri->m_aggrFilledQtys[J]));

      LOG_INFO(2,
        "AGGRESSIVE FILL,{},{},{},{},{}",
        aos->m_id,  aos->m_instr->m_Symbol.data(),
        aos->m_isBuy ? "Bid" : "Ask",  double(px), QR(qty))

      DEBUG_ONLY(m_riskMgr->OutputAssetPositions("OnOurTrade(Aggressive)", 0);)
    }

    //-----------------------------------------------------------------------//
    // Only now: Update the "TrInfo" stats:                                  //
    //-----------------------------------------------------------------------//
    // Signed "delta" of the position wrt to the assets:
    double deltaX = aos->m_isBuy ? double(qty) : - double(qty);
    double deltaY = -     deltaX * double(px);     // NB: "-" !

    // Which Instr has been Traded?
    if (m_instrs[AB] == instr)
    {
      tri->m_posA += deltaX;
      tri->m_posB += deltaY;
    }
    else
    if (m_instrs[CB] == instr)
    {
      tri->m_posC += deltaX;
      tri->m_posB += deltaY;
    }
    else
    {
      assert(m_instrs[AC] == instr);
      tri->m_posA += deltaX;
      tri->m_posC += deltaY;
    }
    // Re-compute the value ("PnL") of this Triangle in RFC:
    double rfcA = NaN<double>;
    double rfcB = NaN<double>;
    double rfcC = NaN<double>;

    // XXX: FullReCalc=true here; performance impact?
    (void) m_assetRisks[A]->ToRFC<true>(tri->m_posA, stratTime, &rfcA);
    (void) m_assetRisks[B]->ToRFC<true>(tri->m_posB, stratTime, &rfcB);
    (void) m_assetRisks[C]->ToRFC<true>(tri->m_posC, stratTime, &rfcC);

    tri->m_PnLRFC  = rfcA + rfcB + rfcC;

    // At the end, flush the Logger:
    m_logger->flush();
  }

  //=========================================================================//
  // "OnCancel" Call-Back:                                                   //
  //=========================================================================//
  template<ExchC0T X>
  inline void TriArbC0<X>::OnCancel
  (
    AOS const&        a_aos,
    utxx::time_val    a_ts_exch,
    utxx::time_val    a_ts_conn
  )
  {
    // If this Call-Back is invoked, the Order must be Inactive:
    assert(!m_dryRun && a_aos.m_isInactive);

    // Generic Impl:
    OrderCanceledOrFailed
    (
      a_aos,
      nullptr,  // No msg -- so it's Cancel, not Error
      a_ts_exch,
      a_ts_conn,
      utxx::now_utc()
    );
  }

  //=========================================================================//
  // "OnConfirm" Call-Back:                                                  //
  //=========================================================================//
  // (This could also be a Confirm for a CancelReq):
  //
  template<ExchC0T X>
  inline void TriArbC0<X>::OnConfirm(Req12 const& a_req)
  {
    // We only handle "Confirm" events in advanced debug modes (>= 3):
    if (utxx::likely(m_debugLevel < 3))
      return;

    // Find which order has been confirmed -- Passive or Aggressive:
    bool          isCancel = (a_req.m_kind == Req12::KindT::Cancel);
    AOS    const* aos      = a_req.m_aos;
    assert       (aos != nullptr && !m_dryRun);

    AOSExt const& ext      = aos->m_userData.Get<AOSExt>();
    bool          isAggr   = (ext.m_aggrJ >= 0);

    assert(( isCancel && IsZero(a_req.m_qty)) ||
           (!isCancel && IsPos (a_req.m_qty)));

    char const* which =
      isCancel ? "CancelReq" : isAggr ? "AggrOrder" : "PassOrder";

    // The Confirmations are only Logged:
    m_logger->info
      ("{} Confirmed,{},{},{},{}",
       which, aos->m_id,         aos->m_instr->m_Symbol.data(),
       aos->m_isBuy ? "Bid"    : "Ask",
       utxx::likely(!isCancel) ? std::to_string(double(a_req.m_px)) : "");
    // No flush here -- this event is of relatively low importance...
  }

  //=========================================================================//
  // "OnOrderError" Call-Back:                                               //
  //=========================================================================//
  template<ExchC0T X>
  inline void TriArbC0<X>::OnOrderError
  (
    Req12 const&    a_req,
    int             UNUSED_PARAM(a_err_code),
    char  const*    a_err_text,
    bool            UNUSED_PARAM(a_prob_filled),  // TODO: Handle it properly!
    utxx::time_val  a_ts_exch,
    utxx::time_val  a_ts_conn
  )
  {
    utxx::time_val  stratTime = utxx::now_utc();
    AOS*       aos  = a_req.m_aos;
    assert    (aos != nullptr && !m_dryRun);

    // Check if the Order is really Inactive (because it can also be a Cancel-
    // Req failure with the AOS still being alive):
    if (aos->m_isInactive)
      OrderCanceledOrFailed
      (
        *aos,
        (a_err_text != nullptr) ? a_err_text : "", // NB: NULL is not OK here
        a_ts_exch,
        a_ts_conn,
        stratTime
      );
    else
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
      bool needReCancel = (aos->m_isCxlPending != 0);
      LOG_INFO(3,
         "NB: CancelReq FAILED: ReqID={}, AOSID={}, IsInactive={}, "
         "IsCxlPending={}{}",
         a_req.m_id, aos->m_id, aos->m_isInactive,  aos->m_isCxlPending,
         needReCancel ? ": Re-Cancelling the Order" : "")

      if (needReCancel)
        // Try again:
        CancelOrderSafe(aos, a_ts_exch, a_ts_conn, stratTime, "Re-Cancel");

      m_logger->flush();
    }
  }

  //=========================================================================//
  // "OnSignal" Call-Back:                                                   //
  //=========================================================================//
  template<ExchC0T X>
  inline void TriArbC0<X>::OnSignal(signalfd_siginfo const& a_si)
  {
    ++m_signalsCount;

    LOG_INFO(1,
      "Got a Signal ({}), Count={}, STOPPING {}",
      a_si.ssi_signo,     m_signalsCount,
      (m_signalsCount == 1) ? "GRACEFULLY" : "IMMEDIATELY")
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
      m_riskMgr->OutputAssetPositions("OnSignal", 0);
      m_reactor->ExitImmediately     ("OnSignal");
    }
  }
} // End namespace MAQUETTE
