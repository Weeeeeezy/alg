// vim:ts=2:et
//===========================================================================//
//                 "TradingStrats/MM-NTProgress/CallBacks.hpp":              //
//                 All Call-Backs of the "Strategy" Interface                //
//===========================================================================//
#pragma  once

#include "MM-NTProgress.h"
#include <utxx/convert.hpp>
#include <sstream>

namespace MAQUETTE
{
  //=========================================================================//
  // "OnTradingEvent" Call-Back:                                             //
  //=========================================================================//
  void MM_NTProgress::OnTradingEvent
  (
    EConnector const& a_conn,
    bool              a_on,
    SecID             a_sec_id,
    utxx::time_val    UNUSED_PARAM(a_ts_exch),
    utxx::time_val    UNUSED_PARAM(a_ts_recv)
  )
  {
    utxx::time_val stratTime = utxx::now_utc();

    //-----------------------------------------------------------------------//
    // Is it SecID-specific?                                                 //
    //-----------------------------------------------------------------------//
    // If this SecID is not ours, ignore the event altogether. Also ignore it
    // (XXX: is this logic correct?) if "a_on" is set:
    //
    if (utxx::unlikely(a_sec_id != 0))
    {
      if (a_on)
        return;

      bool isOurs = false;
      for (int T = 0; T < 2; ++T)
      for (int I = 0; I < 2; ++I)
        // XXX: We check all configured SecIDs, though we could select just one
        // group of them (NTProgress or MICEX) based on "a_conn": The condition
        // a_sec_id != 0 is so rare that it does not really matter:
        //
        if ((m_instrsNTPro[T][I] != nullptr            &&
             m_instrsNTPro[T][I]->m_SecID == a_sec_id) ||
            (m_instrsMICEX[T][I] != nullptr            &&
             m_instrsMICEX[T][I]->m_SecID == a_sec_id))
        {
          isOurs = true;
          break;
        }
      if (!isOurs)
        return;

      // If we got here: This is our SecID, and "a_on" is reset. Then fall thru:
      // it is the same cond as a full Connector outage:
      assert(isOurs && !a_on);
    }

    //-----------------------------------------------------------------------//
    // Log the event:                                                        //
    //-----------------------------------------------------------------------//
    LOG_INFO(1,
      "MM-NTProgress::OnTradingEvent: {} is now {}",
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
      if (!(m_mdcMICEX.IsActive() || m_omcNTPro.IsActive() ||
            m_omcMICEX.IsActive() || m_isInDtor))
      {
        m_riskMgr->OutputAssetPositions("MM-NTProgress::OnTradingEvent", 0);
        m_reactor->ExitImmediately     ("MM-NTProgress::OnTradingEvent");
      }

      // Otherwise: We CANNOT proceed with ANY non-working Connector, so stop
      // (still, possibly receive final data from other Connectors):
      DelayedGracefulStop hmm
        (this, stratTime,
         "MM-NTProgress::OnTradingEvent: Connector Stopped: ",
         a_conn.GetName());
      return;
    }
    // Otherwise: Some Connector has become Active...

    //-----------------------------------------------------------------------//
    // MICEX MDC is now Active?                                              //
    //-----------------------------------------------------------------------//
    if (&a_conn == &m_mdcMICEX)
    {
      assert(m_mdcMICEX.IsActive());

      //---------------------------------------------------------------------//
      // Subscribe to the corresp MktData:                                   //
      //---------------------------------------------------------------------//
      for (int T = 0; T < 2; ++T)
      for (int I = 0; I < 3; ++I)  // NB: Incl AC!
      {
        // NB: The Instrument will be skipped if it is disabled:
        if (!m_isInstrEnabled[T][I])
          continue;

        // Generic Case: Proceed with MktData subscription:
        SecDefD const* instrM = m_instrsMICEX[T][I];
        assert(instrM != nullptr);

        // Subscribe to MICEX Mkt Data; this will also register the corresp
        // MICEX instrument with the RiskMgr; RegisterInstrRisks=true:
        m_mdcMICEX.SubscribeMktData
          (this, *instrM, OrderBook::UpdateEffectT::L2, true);

        LOG_INFO(2, "Subscribed: {}", instrM->m_FullName.data())

        if (I < 2)
        {
          // NB: But for the corresp NTPro instr, we do NOT have a MktData Con-
          // nector (because that would be inefficient and misleading anyway),
          // so we must regieter it with the RiskMgr explicitly, using the MICEX
          // MktData for risks evaluation:
          SecDefD const* instrN = m_instrsNTPro[T][I];
          assert(instrN != nullptr && m_riskMgr != nullptr);
          m_riskMgr->Register
            (*instrN, &(m_mdcMICEX.GetOrderBook(*instrM)));

          LOG_INFO(2, "Registered: {}", instrN->m_FullName.data())
        }
      }
    }
    // End of MDC-Active case
    // Nothing special for OMCs...

    //-----------------------------------------------------------------------//
    // Check whether ALL MDCs and OMCs are now Active:                       //
    //-----------------------------------------------------------------------//
    // If so, Steady-Mode Call-Backs (eg "OnOrderBookUpdate") will become avai-
    // lable, and the RiskMgr will be switched on.  For that, we also need all
    // OrderBooks to be available (on both Bid and Ask sides):
    //
    CheckAllConnectors(stratTime);
  }

  //=========================================================================//
  // "CheckAllConnectors":                                                   //
  //=========================================================================//
  // Checks that all MDCs and OMCs are Active, and all configured OrderBooks are
  // fully ready:
  //
  void MM_NTProgress::CheckAllConnectors(utxx::time_val a_ts_strat)
  {
    // Already All-Active?
    if (utxx::likely(m_allConnsActive))
      return;

    //-----------------------------------------------------------------------//
    // Otherwise: The flag is not set yet. Re-check the Connectors:          //
    //-----------------------------------------------------------------------//
    if (!(m_mdcMICEX.IsActive() && m_omcNTPro.IsActive() &&
          m_omcMICEX.IsActive()))
      // Still not all Connectors are active:
      return;

    // Otherwise: Check if all OrderBooks are ready (except those which are not
    // enabled):
    for (int T = 0; T < 2; ++T)
    for (int I = 0; I < 3; ++I)    // NB: Incl AC!
    {
      // Similar skipping criteria as in "OnTradingEvent" above: First, check
      // for end-of_TOD conds. NB: This is for the general instr availability
      // only, not for our quoting rules -- so we don't check TOMs here, they
      // are always available:
      if (utxx::unlikely(!m_isInstrEnabled[T][I]))
        continue;

      // Now get the OrderBooks for MICEX (XXX: NTProgress OrderBooks are curr-
      // ently not used at all; we quote into NTProgress disregarding the Bids
      // and Asks there!):
      OrderBook const* ob  =  m_obsMICEX[T][I];
      assert(ob != nullptr);

      if (!(ob->IsReady()))
      {
        // Which Instrument / Connector is not ready yet?
        SecDefD   const& instr     = ob->GetInstr();
        char const*      symbol    = instr.m_Symbol.data();
        int              settlDate = instr.m_SettlDate;

        LOG_INFO(3,
          "MM-NTProgress::CheckAllConnectors: MICEX_MDC: Symbol={}, "
          "SettlDate={}: OrderBook not ready yet", symbol, settlDate)
        return;
      }
    }
    //-----------------------------------------------------------------------//
    // Start the RiskMgr in the Normal Mode:                                 //
    //-----------------------------------------------------------------------//
    // NB:
    // (*) It would be Relaxed Mode if we were not covering our positions;
    // (*) Catch all exceptions and immediately terminate the Strategy if they
    //     occur -- otherwise we would be repeatedly trying to start the Risk-
    //     Mgr:
    LOG_INFO(2, "MM-NTProgress::CheckAllConnectors: Starting the RiskMgr...")
    try
    {
      // Register Ccy Converters for TOD & TOM with the RiskMgr -- the corresp
      // OrderBooks are now subscribed for sure (and ready):
      //
      if (utxx::unlikely(!RegisterConverters(a_ts_strat)))
        return; // Would already be stopping if unsuccessful...

      // Now really start the RiskMgr:
      m_riskMgr->Start();

      // Take ptrs to the InstrRisks in the RiskMgr -- they will be used to get
      // the Positional Info:
      for (int T = 0; T < 2; ++T)
      for (int I = 0; I < 2; ++I)  // NB: No AC here  -- it will NOT be traded!
        if (utxx::likely(m_isInstrEnabled[T][I]))
        {
          assert(m_instrsNTPro[T][I] != nullptr &&
                 m_instrsMICEX[T][I] != nullptr);
          m_irsNTPro[T][I] =
            &(m_riskMgr->GetInstrRisks(*(m_instrsNTPro[T][I]), 0));
          m_irsMICEX[T][I] =
            &(m_riskMgr->GetInstrRisks(*(m_instrsMICEX[T][I]), 0));
        }
    }
    catch (std::exception const& exc)
    {
      DelayedGracefulStop hmm
        (this, a_ts_strat,
         "MM-NTProgress: Cannot Start the RiskMgr: ", exc.what());
      return;
    }

    // If OK:
    LOG_INFO(1, "ALL CONNECTORS ARE NOW ACTIVE, RiskMgr Started!")
    m_logger->flush();

    // Output the initial positions:
    m_riskMgr->OutputAssetPositions("MM-NTProgress::CheckAllConnectors", 0);

    //-----------------------------------------------------------------------//
    // Activate the Periodic Timer:                                          //
    //-----------------------------------------------------------------------//
    // (Used to enforce "MaxInterQuote" limit):
    //
    m_timerFD = m_reactor->AddTimer
    (
      "MaxInterQuoteTimer",
      0,
      uint32_t(m_maxInterQuote),

      // Timer Handler:
      [this](int DEBUG_ONLY(a_fd))->void
      {
        assert(a_fd == m_timerFD);
        this->OnTimer();
      },

      // Error Handler:
      // Timer errors are severe error conditions:
      [this, a_ts_strat]
      (int DEBUG_ONLY(a_fd), int a_err_code, uint32_t a_events,
       char const* a_msg) -> void
      {
        assert(a_fd == m_timerFD);

        DelayedGracefulStop hmm
          (this, a_ts_strat,
           "MM-NTProgress: PeriodicTimer Error: Code=",  a_err_code,
           ", Events=", m_reactor->EPollEventsToStr(a_events), ": ",
           (a_msg != nullptr) ? a_msg : "");
      }
    );
    //-----------------------------------------------------------------------//
    // If we got here, the result is now positive:                           //
    //-----------------------------------------------------------------------//
    m_allConnsActive = true;
  }

  //=========================================================================//
  // "CheckTOD":                                                             //
  //=========================================================================//
  // Maybe TOD instrs are already finished?
  //
  void MM_NTProgress::CheckTOD(utxx::time_val a_now)
  {
    if (!(m_isInstrEnabled[TOD][AB] || m_isInstrEnabled[TOD][CB]))
    {
      // Both AB and CB are already disabled for TOD, nothing to check -- they
      // will NOT be enabled back simply by the timer. AC is not available ei-
      // ther, then:
      assert(!m_isInstrEnabled[TOD][AC]);
      return;
    }

    // Otherwise: At least one TOD instr WAS enabled -- check if it is enabled
    // still (based on the Time-of-Day):
    utxx::time_val time = GetCurrTime();

    for (int I = 0; I < 2; ++I)
    {
      int    nBands = m_nBands[TOD][I];
      assert(0 <= nBands && nBands <= MaxBands);

      if (utxx::unlikely
         (nBands > 0 && m_isInstrEnabled[TOD][I] &&
         (time   > m_lastQuoteTimeTOD[I])))
      {
        // Switching OFF TOD/I: Cancel all Quotes on NTProgress (Buffered); and
        // modify the corresp Pegged Orders on MICEX (if any) into Market ones:
        for (int S = 0; S < 2;      ++S)
        {
          for (int B = 0; B < nBands; ++B)
            CancelQuoteSafe<true>(TOD, I, S, B, a_now, a_now, a_now);

          // XXX: For Pegged Orders, it would be better to turn them into Market
          // ones, but for now, we cancel them as well:
          CancelPeggedSafe <true>(TOD, I, S,    a_now, a_now, a_now);
        }
        // Now disabled:
        m_isInstrEnabled[TOD][I] = false;
      }
    }
    // AC is then disabled for TOD as well:
    m_isInstrEnabled[TOD][AC] = false;

    // Now flush the orders accumulated:
    (void) m_omcNTPro.FlushOrders();
    (void) m_omcMICEX.FlushOrders();
  }

  //=========================================================================//
  // "OnOrderBookUpdate" Call-Back:                                          //
  //=========================================================================//
  void MM_NTProgress::OnOrderBookUpdate
  (
    OrderBook const&          a_ob,
    bool                      a_is_error,
    OrderBook::UpdatedSidesT  UNUSED_PARAM(a_sides),
    utxx::time_val            a_ts_exch,
    utxx::time_val            a_ts_recv,
    utxx::time_val            a_ts_strat
  )
  {
    //-----------------------------------------------------------------------//
    // This must be the MICEX MDC:                                           //
    //-----------------------------------------------------------------------//
    assert(&(a_ob.GetMDC()) == &m_mdcMICEX);

    SecDefD const& instr = a_ob.GetInstr();

    // If this is one of the Risk-Only (MICEX EUR/USD) Instruments, do not
    // process this update further (XXX: even if it was an error):
    if (utxx::unlikely
       (&a_ob == m_obsMICEX[TOD][AC] || &a_ob == m_obsMICEX[TOM][AC]))
      return;

    //-----------------------------------------------------------------------//
    // Main OrderBook error (eg Bid-Ask collision or so)?                    //
    //-----------------------------------------------------------------------//
    // NB: { Ask, Bid }:
    PriceT currBestPxs[2] { a_ob.GetBestAskPx(), a_ob.GetBestBidPx() };

    if (utxx::unlikely
       (a_is_error  ||
       !(IsFinite(currBestPxs[Bid]) && IsFinite(currBestPxs[Ask]))))
    {
      // NB: Do NOT stop the whole Strategy here. Simply, remove the Quotes
      // corresp to this Instrument, because we are unable to update them:
      LOG_ERROR(2,
        "MM-NTProgress::OnOrderBookUpdate: OrderBook Error: Symbol={}, "
        "SettlDate={}: Cancelling the Quotes on both Sides, all Bands...",
        instr.m_Symbol.data(), instr.m_SettlDate)

      for (int T = 0; T < 2; ++T)
      for (int I = 0; I < 2; ++I)
      {
        int nBands = m_nBands[T][I];
        assert (0 <= nBands && nBands <= MaxBands);
        if (utxx::unlikely(nBands == 0))
          continue;

        if (m_obsMICEX[T][I] == &a_ob)
        {
          for (int S = 0; S < 2;      ++S)
          for (int B = 0; B < nBands; ++B)
            // Perform Buffered Cancel:
            CancelQuoteSafe<true>
              (T, I, S, B, a_ts_exch, a_ts_recv, a_ts_strat);

          (void) m_omcNTPro.FlushOrders();
          return;
        }
      }
      // If we got here: The OrderBook was not found? This is a really seriois
      // error condition:
      DelayedGracefulStop hmm
        (this, a_ts_strat,
         "MM-NTProgress::OnOrderBookUpdate: Erroneous OrderBook Not Found: "
         "Symbol=", instr.m_Symbol.data(), ", SettlDate=", instr.m_SettlDate);
      return;
    }

    //-----------------------------------------------------------------------//
    // Other Stopping Conditions?                                            //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely(EvalStopConds(a_ts_exch, a_ts_recv, a_ts_strat)))
      return;

    // Status of Connectors availability may change on OrderBook updates. Will
    // NOT be ready to proceed if not all Connectors are Active yet. (This chk
    // is not done in "EvalStopConds" because it is not done in "OnTimer"):
    //
    CheckAllConnectors(a_ts_strat);
    if (utxx::unlikely(!m_allConnsActive))
      return;

    //-----------------------------------------------------------------------//
    // Generic Case:                                                         //
    //-----------------------------------------------------------------------//
    // Determine the Tenor (T) and the CcyPair (I) of the instr for which this
    // update was received:
    int const T = (instr.m_SettlDate == m_TOD)                ? TOD : TOM;
    int const I = (strcmp(instr.m_AssetA.data(), "EUR") == 0) ? AB  : CB;

    // Verify that it is indeed that OrderBook. If not, it is a serious error
    // condition -- we will shut down the Strategy:
    OrderBook const*   expectedOB =  m_obsMICEX[T][I];

    if (utxx::unlikely(expectedOB != &a_ob))
    {
      SecDefD const& expectedDef = expectedOB->GetInstr();
      DelayedGracefulStop hmm
        (this, a_ts_strat, "MM-NTProgress::OnOrderBookUpdate: Inconsistency"
         " on MICEX MDC: Expected: ", expectedDef.m_FullName.data(),  ", Got: ",
         instr.m_FullName.data());
      return;
    }

    //-----------------------------------------------------------------------//
    // If OK: Re-Quote the corresp NTProgress Instr:                         //
    //-----------------------------------------------------------------------//
    // XXX: At the moment, we do not determine on which side of "a_ob" the upd-
    // ate has actually occurred, and re-calculate both Bid and Ask quoting pxs.
    // This is because a Quote Px update on one side may cause an update on the
    // other side if a Bid-Ask collision possibility is detected. So:
    //
    DoQuotes(T, I, a_ts_exch, a_ts_recv, a_ts_strat);

    //-----------------------------------------------------------------------//
    // Adjust the corresp MICEX Pegged Orders (if any):                      //
    //-----------------------------------------------------------------------//
    bool doneMICEX = false;
    for (int S = 0; S < 2; ++S)
    {
      AOS const* peggedAOS = m_aosesPegMICEX[T][I][S];
      PriceT prevBestPx    = m_prevBestPxs  [T][I][S];
      PriceT currBestPx    = currBestPxs          [S];

      if (utxx::unlikely
         (peggedAOS  != nullptr          && !peggedAOS->m_isInactive &&
          peggedAOS->m_isCxlPending == 0 && IsFinite(prevBestPx)    &&
          prevBestPx != currBestPx))
        doneMICEX    |=
          // Modify the Pegged Order (Buffered=true); Qty is unchanged:
          ModifyPassOrderSafe<true>
            (peggedAOS, currBestPx, QtyO::Invalid(),
             a_ts_exch, a_ts_recv,  a_ts_strat);
    }
    if (doneMICEX)
      (void) m_omcMICEX.FlushOrders();

    //-----------------------------------------------------------------------//
    // Memoise the Best MICEX Pxs:                                           //
    //-----------------------------------------------------------------------//
    m_prevBestPxs[T][I][Bid] = currBestPxs[Bid];
    m_prevBestPxs[T][I][Ask] = currBestPxs[Ask];

    // All Done!
  }

  //=========================================================================//
  // "OnOurTrade" Call-Back:                                                 //
  //=========================================================================//
  // Invoked when we got a Fill or PartFill of our order:
  //
  void MM_NTProgress::OnOurTrade(Trade const& a_tr)
  {
    utxx::time_val stratTime = utxx::now_utc();
    utxx::time_val exchTime  = a_tr.m_exchTS;
    utxx::time_val recvTime  = a_tr.m_recvTS;

    //-----------------------------------------------------------------------//
    // Get the Req and AOS:                                                  //
    //-----------------------------------------------------------------------//
    Req12 const* req = a_tr.m_ourReq;
    assert(req != nullptr);
    AOS   const* aos = req->m_aos;
    assert(aos != nullptr &&
          (aos->m_omc  == &m_omcNTPro || aos->m_omc == &m_omcMICEX));

    PriceT px  = a_tr.m_px;
    QtyO   qty = a_tr.GetQty<QTO,QRO>();
    assert(IsFinite(px) && IsPos(qty));

    // XXX: Here we use a simplistic criterion -- iff a Fill was on the MICEX
    // side, it was an Aggr Order:
    bool isAggr = (aos->m_omc == &m_omcMICEX);

    // Log the Trade: XXX: There is an overhead for doing that BEFORE doing Co-
    // vering Orders or ReQuotes etc -- but we need to preserve the logical se-
    // quence of events in the log:
    if (m_debugLevel >= 2)
    {
      // XXX: "Aggressive" Fill may be a misnomer -- this also includes Pegged
      // Fills, but we use this term for historical reasons:
      m_logger->info
        ("{} FILL: AOSID={}, ReqID={}, Symbol={}, {}, Px={}, Qty={}, "
         "ExecID={}",
         isAggr ? "AGGRESSIVE" : "PASSIVE", aos->m_id, req->m_id,
         aos->m_instr->m_Symbol.data(),     aos->m_isBuy ? "Bid" : "Ask",
         double(px),   QRO(qty),  a_tr.m_execID.ToString());

      m_riskMgr->OutputAssetPositions("MM-NTProgress::OnOurTrade", 0);
    }
    //-----------------------------------------------------------------------//
    // Was it a Covering Order on MICEX?                                     //
    //-----------------------------------------------------------------------//
    OrderInfo const& ori = aos->m_userData.Get<OrderInfo>();
    if (isAggr)
    {
      // A Covering Order on MICEX has been Filled or Part-Filled. Its Qty must
      // be removed from the FlyingQtys:
      assert(ori.m_B == -1);   // No Bands for Aggr Orders

      if (aos->m_isBuy)
        m_flyingCovDeltas[ori.m_T][ori.m_I] -= qty;
      else
        m_flyingCovDeltas[ori.m_T][ori.m_I] += qty;

      // If it was a Market Order (ie NOT Pegged), nothing to do anymore:
      if (!ori.m_isPegged)
        return;
      // But for Pegged Orders, fall through!
    }

    //-----------------------------------------------------------------------//
    // A Passive (Quote or Pegged) Order: Get the Indices:                   //
    //-----------------------------------------------------------------------//
    int  T = -1, I = -1, S = -1, B = -1;  // TBD
    bool wasCP = false;                   // TBD

    if (utxx::unlikely
       (!CheckPassAOS("OnOurTrade", aos, req->m_id, nullptr, &T, &I, &S, &B,
                      &wasCP, stratTime)))
      return;

    assert(T == ori.m_T && I == ori.m_I && S == aos->m_isBuy);

    // If it was a Partial Fill (ie the order is still Active), and the AOS was
    // not on the CxlPending List yet, Cancel it, because Partially-Filled ords
    // cannot be modified. This applies to BOTH NTPro and MICEX Sides:
    //
    if (!(aos->m_isInactive || wasCP))
    {
      // XXX: How to formallt prove the invariant below?
      assert((!isAggr && aos == GetMainAOS  (T, I, S, B)) ||
             ( isAggr && aos == GetPeggedAOS(T, I, S)));

      // Buffered=true, as will most likely be followed by a Re-Quote or a new
      // Pegged order:
      if (!isAggr)
        // Cancel the remainder of the Quote:
        CancelQuoteSafe <true>
          (T, I, S, B, exchTime, recvTime, stratTime);
      else
        // Cancel the remainder  of the Pegged Order. But do NOT adjust the
        // "FlyingCovDeltas" yet -- the Pegged Order may be filled again before
        // the CancelReq takes effect!
        CancelPeggedSafe<true>
          (T, I, S,    exchTime, recvTime, stratTime);

      // If CancelReq successful, we will get GetMainAOS  (T, I, S, B) == NULL
      // or                                   GetPeggedAOS(T, I, S)    == NULL
    }

    // Check the Position and possibly issue the corresp Covering Order. This
    // applies to both Quote Fills and Pegged Order Fills. In the latter case,
    // if a pos is still not 0 (eg in case of Partial Pegged Fill and the re-
    // mainder cancellation), a new Pegged Order will be issued:
    //
    MayBeDoCoveringOrder(T, I, exchTime, recvTime, stratTime);

    //-----------------------------------------------------------------------//
    // If it was a Quote Fill / Part-Fill: Round Done:                       //
    //-----------------------------------------------------------------------//
    if (!isAggr)
    {
      // At this point, we conside the curr Round to be done (even though the
      // Covering Orders are only submitted, not filled yet):
      //
      if (utxx::unlikely(++m_roundsCount >= m_maxRounds))
        // Preparing to Stop: Cancel all Quotes, but NOT the Pegged Orders. And
        // still do NOT stop immediately --  it can only be done manually later
        // on; eg the user may want to wait until all orders are confirmed to be
        // Filled or Cancelled:
        CancelAllOrders<false>(exchTime, recvTime, stratTime);
      else
        // Try to Re-Quote now at the same px (because MktData have not cnhgd):
        // "Submit1Quote" will take full care of the AOS. It will normally work
        // for both Complete and Partial Fill (provided that the Slot was freed
        // up; otherwise, we will get a "MustWait" status):
        // But do NOT Re-Quote if "aos" was found on the CxlPending List; it was
        // already Re-Quoted when first placed on that List -- so the Main TISB
        // Slot may now contain a new quote already. The following conds are al-
        // ways conservatibe and safe:
        //
        if (utxx::likely(!wasCP && GetMainAOS(T, I, S, B) == nullptr))
          Do1Quote(T, I, S, B, exchTime, recvTime, stratTime);
    }
    //-----------------------------------------------------------------------//
    // Do Flushes on both OMCs for certainty:                                //
    //-----------------------------------------------------------------------//
    (void) m_omcNTPro.FlushOrders();
    (void) m_omcMICEX.FlushOrders();
    m_logger->flush();
  }

  //=========================================================================//
  // "OnCancel" Call-Back:                                                   //
  //=========================================================================//
  inline void MM_NTProgress::OnCancel
  (
    AOS const&     a_aos,
    utxx::time_val a_ts_exch,
    utxx::time_val a_ts_recv
  )
  {
    //-----------------------------------------------------------------------//
    // Checks:                                                               //
    //-----------------------------------------------------------------------//
    utxx::time_val stratTime = utxx::now_utc();

    // If it is a Cancel, the AOS must be Inactive:
    assert(a_aos.m_isInactive);

    // If a MICEX Market Order got Cancelled, it is serious error cond:
    bool isNTPro  = (a_aos.m_omc == &m_omcNTPro);
    bool isPegged =  a_aos.m_userData.Get<OrderInfo>().m_isPegged;

    // Pegged Orders are only available on MICEX: isPegged => !isNTPro:
    assert(!isPegged && isNTPro);

    // Furthermore, if is was a Market (not Pegged) order on MICEX which was
    // Cancelled, it is an error cond:
    if (utxx::unlikely(!(isNTPro || isPegged)))
    {
      DelayedGracefulStop hmm
        (this, stratTime, "MM-NTProgress::OnCancel: AOSID=",  a_aos.m_id,
         ": MICEX MarketOrder was Cancelled???");
      return;
    }
    // Thus, at this point, the following invariant holds:
    assert(!isNTPro == isPegged);

    //-----------------------------------------------------------------------//
    // Generic Case: NTPro Quote or MICEX Pegged Order was Cancelled:        //
    //-----------------------------------------------------------------------//
    int  T = -1, I = -1, S = -1, B = -1;
    bool wasCP = false;
    if (utxx::unlikely
       (!CheckPassAOS("OnCancel", &a_aos, 0, nullptr, &T, &I, &S, &B, &wasCP,
                      stratTime)))
      return;

    // Log the event:
    QtyO   remQty =  a_aos.GetLeavesQty<QTO,QRO>(); // Still valid after Cancel!
    assert(!IsNeg(remQty));

    LOG_INFO(3,
      "MM-NTProgress::OnCancel: {}: AOSID={}, {}: T={}, I={}, IsBid={}: "
      "Cancelled; UnFilledQty={}",
      isNTPro  ? "NTProgress"  : "MICEX", a_aos.m_id,
      isPegged ? "PeggedOrder" : "Quote", T, I, S, QRO(remQty))

    // If this an NTPro AOS, it has most likely come from CxlPending List, so
    // it was already Re-Quoted.   So use the following conservative cond for
    // Re-Quoting:
    //
    if (utxx::likely(isNTPro))
    {
      if (utxx::unlikely(!wasCP && GetMainAOS(T, I, S, B) == nullptr))
        Do1Quote(T, I, S, B, a_ts_exch, a_ts_recv, stratTime);
    }
    // A Pegged MICEX Order has been cancelled: Must update "FlyingCovDeltas"
    // and possibly re-issue the corresp Covering Order:
    else
    {
      assert(isPegged);

      if (a_aos.m_isBuy)
        m_flyingCovDeltas[T][I] -= remQty;
      else
        m_flyingCovDeltas[T][I] += remQty;

      // This may require us to re-issue the Pegged Order:
      MayBeDoCoveringOrder(T, I, a_ts_exch, a_ts_recv, stratTime);
    }

    //-----------------------------------------------------------------------//
    // Safety Flushes:                                                       //
    //-----------------------------------------------------------------------//
    (void) m_omcNTPro.FlushOrders();
    (void) m_omcMICEX.FlushOrders();

    m_logger->flush();
  }

  //=========================================================================//
  // "OnOrderError" Call-Back:                                               //
  //=========================================================================//
  inline void MM_NTProgress::OnOrderError
  (
    Req12 const&    a_req,
    int             UNUSED_PARAM(a_err_code),
    char  const*    a_err_text,
    bool            a_prob_filled,
    utxx::time_val  a_ts_exch,
    utxx::time_val  a_ts_recv
  )
  {
    //-----------------------------------------------------------------------//
    // Checks:                                                               //
    //-----------------------------------------------------------------------//
    assert(a_err_text != nullptr);
    utxx::time_val  stratTime = utxx::now_utc();

    // Get the AOS:
    AOS const* aos = a_req.m_aos;
    assert(aos != nullptr);

    // If a MICEX Market Order got Cancelled, it is serious error cond:
    bool        isNTPro  = (aos->m_omc == &m_omcNTPro);
    bool        isPegged =  aos->m_userData.Get<OrderInfo>().m_isPegged;
    char const* omcStr   = (isNTPro ? "NTProgress" : "MICEX");

    // Pegged Orders are only available on MICEX: isPegged => !isNTPro:
    assert(!isPegged && isNTPro);

    // Furthermore, if is was a Market (not Pegged) order on MICEX which was
    // Cancelled, it is an error cond:
    if (utxx::unlikely(!(isNTPro || isPegged)))
    {
      DelayedGracefulStop hmm
        (this, stratTime, "MM-NTProgress::OnOrderError: AOSID=", aos->m_id,
         ", ReqID=",  a_req.m_id,   ": MICEX Market Order has FAILED: ",
         (a_prob_filled ? "Probably Filled: " : ""),    a_err_text);
      return;
    }
    // Thus, at this point, the following invariant holds:
    assert(!isNTPro == isPegged);

    //-----------------------------------------------------------------------//
    // Generic Case: NTPro Quote or MICEX Pegged Order Failure:              //
    //-----------------------------------------------------------------------//
    // Log the event:
    LOG_ERROR(2,
      "MM-NTProgress::OnCancel: {}: AOSID={}, ReqID={}, {}: Order FAILED: {}",
      omcStr, aos->m_id, a_req.m_id, isPegged ? "PeggedOrder" : "Quote",
      a_err_text)

    // Get the Indices. NB: Here ErrMsg != null, to indicate a Failure:
    char const* errMsg = (a_err_text != nullptr) ? a_err_text : "";

    int  T = -1, I = -1, S = -1, B = -1;
    bool wasCP = false;
    if (utxx::unlikely
       (!CheckPassAOS("OnOrderError", aos, a_req.m_id, errMsg, &T, &I, &S, &B,
                      &wasCP, stratTime)))
      return;

    //-----------------------------------------------------------------------//
    // NB: It could also be a CancelReq failure, eg:                         //
    //-----------------------------------------------------------------------//
    // (*) we issued a Move before Part-Fill was reported;
    // (*) then got a Part-Fill event;
    // (*) so issued a Cancel (after Part-Fill) relative to the prev Move;
    // (*) then got a Move failure (because Part-Filled orders cannot be moved);
    // (*) and finally got a Cancel failure:
    // In that case, a part-filled order will continue to exist. So we must exp-
    // licitly re-cancel it:
    //
    if (utxx::unlikely(a_req.m_kind == Req12::KindT::Cancel))
    {
      // This AOS needs re-cancel if it is not Inactive, and has lost its Cxl-
      // Pending status:
      bool needReCancel = !(aos->m_isInactive) && (aos->m_isCxlPending == 0);

      LOG_WARN(2,
        "MM-NTProgress::OnOrderError: {}: CancelReq FAILED: ReqID={}, AOSID="
        "{}, IsInactive={}, IsCxlPending={}{}",    omcStr,  a_req.m_id,
        aos->m_id, aos->m_isInactive, aos->m_isCxlPending,
        needReCancel ? ": Re-Cancelling the Order" : "")

      if (needReCancel)
        // Currently, we do cancellations on the NTProgress Connector only (NOT
        // Buffered -- single Cancel, and NOT using TISB -- most likely, the
        // AOS in question was on CxlPending List):
        (void) CancelPassOrderSafe<true>(aos, a_ts_exch, a_ts_recv, stratTime);

      // XXX: Should we do anything specific if this AOS  was indeed on the
      // CxlPending" List? -- Probably not (since we re-canceled it if nece-
      // ssary), but still produce a warning:
      //
      if (wasCP)
        LOG_WARN(2,
        "MM-NTProgress::OnOrderError: {}: AOSID={}: Was on the CxlPending List,"
        " and its CancelReq={} has Failed...", omcStr, aos->m_id, a_req.m_id)
    }
    else
    //-----------------------------------------------------------------------//
    // NTPro Quote Failed? Re-Quote under the conservative conds:            //
    //-----------------------------------------------------------------------//
    // (See "OnCancel" above):
    //
    if (isNTPro && !wasCP && GetMainAOS(T, I, S, B) == nullptr)
      Do1Quote(T, I, S, B, a_ts_exch, a_ts_recv, stratTime);

    // Safety flush (on BOTH NTPro and MICEX):
    (void) m_omcNTPro.FlushOrders();
    (void) m_omcMICEX.FlushOrders();
    m_logger->flush();

    // TODO: Proper handling of "a_prob_filled" flag (need to fire up covering
    // orders in that case!)
  }

  //=========================================================================//
  // "OnSignal" Call-Back:                                                   //
  //=========================================================================//
  inline void MM_NTProgress::OnSignal(signalfd_siginfo const& a_si)
  {
    utxx::time_val stratTime = utxx::now_utc();
    ++m_signalsCount;

    LOG_INFO(1,
      "Got a Signal ({}), Count={}, Exiting {}", a_si.ssi_signo,
      m_signalsCount,  (m_signalsCount == 1)  ? "Gracefully" : "Immediately")
    m_logger->flush();

    if (m_signalsCount == 1)
    {
      // Initiate a stop:
      DelayedGracefulStop hmm(this, stratTime, "MM-NTProgress::OnSignal");
    }
    else
    if (utxx::likely(!m_isInDtor))
    {
      assert(m_signalsCount >= 2);
      // Throw the "ExitRun" exception to exit the Main Event Loop (if we are
      // not in that loop, it will be handled safely anyway):
      m_riskMgr->OutputAssetPositions("MM-NTProgress::OnSignal", 0);
      m_reactor->ExitImmediately     ("MM-NTProgress::OnSignal");
    }
  }

  //=========================================================================//
  // "OnTimer" Call-Back: our own Periodic Timer:                            //
  //=========================================================================//
  inline void MM_NTProgress::OnTimer()
  {
    utxx::time_val stratTime = utxx::now_utc();

    // Not going further?
    if (utxx::unlikely(EvalStopConds(stratTime, stratTime, stratTime)))
      return;

    // NB: Do NOT invoke "CheckAllConnectors" here, because the Timer itself is
    // activated only when all Connectors become available!

    // Go through all Instruments:
    for (int T = 0; T < 2; ++T)
    for (int I = 0; I < 2; ++I)
    {
      int nBands = m_nBands[T][I];
      assert(0  <= nBands && nBands <= MaxBands);
      if (utxx::unlikely(nBands == 0))
        continue;

      // (*) Are there any Missing Quotes (on any Side or Band)?
      // (*) If so, for how long has it been missing for (find the EARLIST
      //     LastQuoteTime for this Instrument):
      //
      utxx::time_val missingSince;  // Initially empty

      for (int S = 0; S < 2;      ++S)
      for (int B = 0; B < nBands; ++B)
      {
        AOS const*& aos = GetMainAOS(T, I, S, B);  // NB: Ref to a Ptr!

        if (utxx::unlikely(aos == nullptr || aos->m_isInactive))
        {
          // NB: The Inactive cond above should not happen (then AOS would be
          // NULL anyway -- but we check it for extra safety):
          missingSince =
            (missingSince.empty())
            ? m_lastQuoteTimes[T][I][S][B]
            : std::min<utxx::time_val>
              (missingSince, m_lastQuoteTimes[T][I][S][B]);
          aos = nullptr;
          goto  Break;
        }
      }
      Break:;

      if (utxx::unlikely
         (!missingSince.empty()     &&
         (stratTime - missingSince).milliseconds() > m_maxInterQuote))
        // Yes, the time is high to replace the missing quotes, even though
        // the MktData has not change since then:
        DoQuotes(T, I, stratTime, stratTime, stratTime);
    }
  }

  //=========================================================================//
  // "EvalStopConds":                                                        //
  //=========================================================================//
  // Common check performed by Event-Driven ("OnOrderBookUpdate") and Time-
  // Driven ("OnTimer") Call_Backs.
  // Returns "true" iff we should NOT proceed any further:
  //
  inline bool MM_NTProgress::EvalStopConds
  (
    utxx::time_val  a_ts_exch,
    utxx::time_val  a_ts_recv,
    utxx::time_val  a_ts_strat
  )
  {
    //-----------------------------------------------------------------------//
    // RiskMgr has entered the Safe Mode?                                    //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely
       (m_riskMgr != nullptr && m_riskMgr->GetMode() == RMModeT::Safe))
    {
      // Stop the Strategy gracefully: Cancel all Quotes, wait some time for
      // the final reports:
      DelayedGracefulStop hmm(this, a_ts_strat, "RiskMgr entered SafeMode");
      return true;
    }

    //-----------------------------------------------------------------------//
    // "DelayedStop" has been initiated?                                     //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely(!m_delayedStopInited.empty()))
    {
      // If 1 sec from "m_delayedStopInited" has passed, we explicitly Cancel
      // all curr Quotes and Pegged Orders (the latter, upon confirming Canc-
      // ellation, will be turned into Mkt Orders). XXX: It's OK to do so mul-
      // tiple times -- second time, there will be just nothing to Cancel:
      //
      if ((a_ts_strat - m_delayedStopInited).sec() > 1)
        CancelAllOrders<true>(a_ts_exch, a_ts_recv, a_ts_strat);

      // If, furhermore, 5 secs have passed, and we are not in "GracefulStop"
      // yet, enter it now:
      if (!m_nowStopInited && (a_ts_strat - m_delayedStopInited).sec() > 5)
      {
        SemiGracefulStop();
        assert(m_nowStopInited);
      }
      return true;
    }

    //-----------------------------------------------------------------------//
    // In any other stopping mode, or reached the max number of Rounds?      //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely (IsStopping() || m_roundsCount >= m_maxRounds))
      return true;

    // Generic Case:
    // If we got here: "CheckTOD" will only set flags (and possibly cancel some
    // Quotes) related to TOD --  evaluate it now:
    CheckTOD(a_ts_strat);

    // So NOT stopping:
    return false;
  }

  //=========================================================================//
  // "RegisterConverters":                                                   //
  //=========================================================================//
  bool MM_NTProgress::RegisterConverters(utxx::time_val a_ts_strat)
  {
    // NB: We can use those OrderBooks in "InstallValuator" (provided that they
    // exist, of course) not waiting for them to become "Ready",    because the
    // FAST Connector had already properly unitialised them:
    //
    OrderBook const* convRUB_TOD = m_obsMICEX[TOD][CB];
    OrderBook const* convRUB_TOM = m_obsMICEX[TOM][CB];

    OrderBook const* convEUR_TOD = m_obsMICEX[TOD][AC];
    OrderBook const* convEUR_TOM = m_obsMICEX[TOM][AC];

    //-----------------------------------------------------------------------//
    // RUB(TOM):                                                             //
    //-----------------------------------------------------------------------//
    // XXX: RUB positions come from both AB and CB; TOD is obviously same for
    // both of them, but TOMs may be different. STILL, at the moment, we  use
    // same "convRUB_TOM" for both of them:
    //
    if (utxx::likely(convRUB_TOM != nullptr))
    {
      m_riskMgr->InstallValuator  (m_RUB.data(), m_TOM_CB, convRUB_TOM);

      if (utxx::unlikely(m_TOM_AB != m_TOM_CB))
        m_riskMgr->InstallValuator(m_RUB.data(), m_TOM_AB, convRUB_TOM);
    }
    else
    if (utxx::likely(m_fixedRUB_USD > 0.0))
    {
      // Use an approximate rate if available:
      m_riskMgr->InstallValuator(m_RUB.data(), m_TOM_CB, m_fixedRUB_USD);

      if (utxx::unlikely(m_TOM_AB != m_TOM_CB))
        m_riskMgr->InstallValuator(m_RUB.data(), m_TOM_AB, m_fixedRUB_USD);
    }
    else
    {
      // No live USD/RUB_TOM feed, and no fixed rate: Cannot proceed:
      DelayedGracefulStop hmm
        (this, a_ts_strat,
         "MM-NTProgress::OnTradingEvent: No MICEX USD/RUB_TOM feed and no "
         "fixed rate either: Cannot proceed!");
      return false;
    }

    //-----------------------------------------------------------------------//
    // RUB(TOD):                                                             //
    //-----------------------------------------------------------------------//
    // Because the live USD/RUB_TOD rates are not available after the cut-off
    // time, but the positions may remain, need to use swap rates:
    //
    if (utxx::likely(convRUB_TOD != nullptr && convRUB_TOM != nullptr))
      // Both USD/RUB_TOD and USD/RUB_TOM are available:
      m_riskMgr->InstallValuator
      (
        // Use direct TOD rates for now:
        m_RUB.data(), m_TOD, convRUB_TOD, 0.0, 0.0,
        // Use TOM and swap rates after the Roll-Over Time:
        m_lastQuoteTimeTOD[CB],    convRUB_TOM,
        - m_swapRates[CB][Ask],    - m_swapRates[CB][Bid]
      );
    else
    if (utxx::likely(convRUB_TOM != nullptr))
      // Only USD/RUB_TOM is available -- use swap rates straight away:
      m_riskMgr->InstallValuator
      (
        m_RUB.data(), m_TOD, convRUB_TOM,
        - m_swapRates[CB][Ask],    - m_swapRates[CB][Bid]
      );
    else
    if (utxx::likely(m_fixedRUB_USD > 0.0))
      // Live USD/RUB feeds not  available at all:
      // Use an approximate rate if available:
      m_riskMgr->InstallValuator(m_RUB.data(), m_TOD, m_fixedRUB_USD);
    else
    {
      // No live USD/RUB_TOD feed, and no fixed rate: Cannot proceed:
      DelayedGracefulStop hmm
        (this, a_ts_strat,
         "MM-NTProgress::OnTradingEvent: No MICEX USD/RUB_TOD feed and no "
         "fixed rate either: Cannot proceed!");
      return false;
    }

    //-----------------------------------------------------------------------//
    // EUR(TOM):                                                             //
    //-----------------------------------------------------------------------//
    // Similar to RUB(TOM), but only for 1 SettlDate (m_TOM_AB):
    //
    if (utxx::likely(convEUR_TOM != nullptr))
      m_riskMgr->InstallValuator(m_EUR.data(), m_TOM_AB, convEUR_TOM);
    else
    if (utxx::likely(m_fixedEUR_USD > 0.0))
      // Use an approximate rate if available:
      m_riskMgr->InstallValuator(m_EUR.data(), m_TOM_AB, m_fixedEUR_USD);
    else
    {
      // No live EUR/USD_TOM feed, and no fixed rate: Cannot proceed:
      DelayedGracefulStop hmm
        (this, a_ts_strat,
         "MM-NTProgress::OnTradingEvent: No MICEX EUR/USD_TOM feed and no "
         "fixed rate either: Cannot proceed!");
      return false;
    }

    //-----------------------------------------------------------------------//
    // EUR(TOD):                                                             //
    //-----------------------------------------------------------------------//
    // Similar to RUB(TOD):
    //
    if (utxx::likely(convEUR_TOD != nullptr && convEUR_TOM != nullptr))
      m_riskMgr->InstallValuator
      (
        // Use direct  TOD rates for now:
        m_EUR.data(), m_TOD, convEUR_TOD, 0.0, 0.0,
        // Use TOM and swap rates after the Roll-Over Time:
        std::min(m_lastQuoteTimeTOD[AB], m_lastQuoteTimeTOD[CB]),
        convEUR_TOM,
        - m_swapRates[AC][Ask],    - m_swapRates[AC][Bid]
      );
    else
    if (utxx::likely(convEUR_TOM != nullptr))
      // Only EUR/USD_TOM is available -- use swap rates straight away:
      m_riskMgr->InstallValuator
      (
        m_EUR.data(), m_TOD, convEUR_TOM,
        - m_swapRates[AC][Ask],    - m_swapRates[AC][Bid]
      );
    else
    if (utxx::likely(m_fixedEUR_USD > 0.0))
      // Live EUR/USD feeds not  available at all:
      // Use an approximate rate if available:
      m_riskMgr->InstallValuator(m_EUR.data(), m_TOD, m_fixedEUR_USD);
    else
    {
      // No live EUR/USD_TOD feed, and no fixed rate: Cannot proceed:
      DelayedGracefulStop hmm
        (this, a_ts_strat,
         "MM-NTProgress::OnTradingEvent: No MICEX EUR/USD_TOD feed and no "
         "fixed rate either: Cannot proceed!");
      return false;
    }

    // All Done:
    return true;
  }
} // End namespace MAQUETTE
