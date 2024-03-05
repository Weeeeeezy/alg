// vim:ts=2:et
//===========================================================================//
//                "TradingStrats/Misc/Pairs-MF/CallBacks.hpp":               //
//              Implementation of Standard Strategy Call-Backs               //
//===========================================================================//
#pragma once

#include "Pairs-MF.h"
#include "Basis/TimeValUtils.hpp"

namespace MAQUETTE
{
  //=========================================================================//
  // "OnTradingEvent" Call-Back:                                             //
  //=========================================================================//
  inline void Pairs_MF::OnTradingEvent
  (
    EConnector const&   a_conn,
    bool                a_on,
    SecID               a_sec_id,
    utxx::time_val      UNUSED_PARAM(a_ts_exch),
    utxx::time_val      UNUSED_PARAM(a_ts_recv)
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
      for (IPair const& ip: m_iPairs)
        if (ip.m_passInstr->m_SecID == a_sec_id ||
            ip.m_aggrInstr->m_SecID == a_sec_id)
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
      "Pairs-MF::OnTradingEvent: {} is now {}",
      a_conn.GetName(),  a_on ? "ACTIVE" : "INACTIVE")
    m_logger->flush();

    //-----------------------------------------------------------------------//
    // Any Connector has become Inactive?                                    //
    //-----------------------------------------------------------------------//
    if (!a_on)
    {
      m_allConnsActive = false;

      // If all Connectors have actually stopped, exit from the Reactor Loop,
      // unless this call-back is invoked from within the Dtor  (then we are
      // not in the Reactor Loop anyway); "a_conn"  is certainly  one of the
      // Connectors listed below:
      if (// MDCs:
          (m_mdcMICEX_EQ == nullptr || !(m_mdcMICEX_EQ->IsActive())) &&
          (m_mdcMICEX_FX == nullptr || !(m_mdcMICEX_FX->IsActive())) &&
          (m_mdcFORTS    == nullptr || !(m_mdcFORTS   ->IsActive())) &&
          (m_mdcAlfa     == nullptr || !(m_mdcAlfa    ->IsActive())) &&
          (m_mdcNTPro    == nullptr || !(m_mdcNTPro   ->IsActive())) &&
          (m_mdcHSFX_Gen == nullptr || !(m_mdcHSFX_Gen->IsActive())) &&
          (m_mdcCCMA     == nullptr || !(m_mdcCCMA    ->IsActive())) &&
          // OMCs:
          (m_omcMICEX_EQ == nullptr || !(m_omcMICEX_EQ->IsActive())) &&
          (m_omcMICEX_FX == nullptr || !(m_omcMICEX_FX->IsActive())) &&
          (m_omcFORTS    == nullptr || !(m_omcFORTS   ->IsActive())) &&
          (m_omcAlfa     == nullptr || !(m_omcAlfa    ->IsActive())) &&
          (m_omcNTPro    == nullptr || !(m_omcNTPro   ->IsActive())) &&
          (m_omcHSFX_Gen == nullptr || !(m_omcHSFX_Gen->IsActive())) &&
          (m_omcCCMA     == nullptr || !(m_omcCCMA    ->IsActive())) )
      {
        if (m_riskMgr != nullptr)
          m_riskMgr->OutputAssetPositions("Pairs-MF::OnTradingEvent", 0);
        m_reactor->ExitImmediately       ("Pairs-MF::OnTradingEvent");
        // This point is not reached!
        __builtin_unreachable();
      }

      // Otherwise: We cannot proceed with a non-working connector, so initiate
      // a Stop:
      DelayedGracefulStop hmm
        (this, stratTime,
         "Pairs-MF::OnTradingEvent: Connector Stopped: ", a_conn.GetName());
      return;
    }
    // Otherwise: Some Connector has become ACTIVE!..

    //-----------------------------------------------------------------------//
    // An MDC is now Active? -- Subscribe to the corresp MktData:            //
    //-----------------------------------------------------------------------//
    bool isMDC = (&a_conn == m_mdcMICEX_EQ || &a_conn == m_mdcMICEX_FX ||
                  &a_conn == m_mdcFORTS    || &a_conn == m_mdcAlfa     ||
                  &a_conn == m_mdcNTPro    || &a_conn == m_mdcHSFX_Gen ||
                  &a_conn == m_mdcCCMA);
    if (isMDC)
    {
      // Subscribe to MktData for all configured Instrs related  to this MDC:
      // XXX: In general, we must NOT use the MDC's list of SecDefs,  because
      // that list may contain instrs inherited from a prev Strategy invocation
      // (which are now removed). So go over all IPairs instead:
      // NB: RegisterInstrRisks=true in both cases: Ie HERE we add Instrs  and
      // their Assets to the RiskMgr -- NOT when SecDefs were created:
      //
      for (IPair const& ip: m_iPairs)
      {
        // Passive Leg:
        //
        if (&a_conn == ip.m_passMDC)
        {
          assert(ip.m_passInstr != nullptr && ip.m_passOB != nullptr);

          // For the Passive Instr, we need L2 data if the DeadZone is enabled;
          // otherwise, L1Px would do:
          OrderBook::UpdateEffectT passEffect =
            (ip.m_dzLotsFrom <= ip.m_dzLotsTo)
            ? OrderBook::UpdateEffectT::L2
            : OrderBook::UpdateEffectT::L1Px;

          if (stratTime < ip.m_enabledUntil)
            // Yes, really subscribe:
            ip.m_passMDC->SubscribeMktData
              (this, *ip.m_passInstr, passEffect, true);
          else
          if (m_riskMgr != nullptr)
            // We STILL need register the corresp InstrRisks, otherwise the RM
            // may sometimes shut us down!
            m_riskMgr->Register(*ip.m_passInstr, ip.m_passOB);
        }

        // Aggressive Leg (NB: it is NOT exclusive wrt Passive Leg above!!!):
        //
        if (&a_conn == ip.m_aggrMDC)
        {
          assert(ip.m_aggrInstr != nullptr && ip.m_aggrOB != nullptr);

          // XXX: For the Aggressive Instr, we normally need L2 data to calcu-
          // late VWAPs. But if the AggrNomQty is just 1 lot, we will  assume
          // that L1Px is sufficient. (This does not include Hedge Adjustment
          // AggrOrders, but for them, VWAP is not computed anyway):
          //
          long lotSz = long(Round(ip.m_aggrInstr->m_LotSize));
          OrderBook::UpdateEffectT aggrEffect =
            utxx::unlikely (ip.m_aggrNomQty  <= lotSz)
            ? OrderBook::UpdateEffectT::L1Px
            : OrderBook::UpdateEffectT::L2;

          // Again, may need to Register the Aggr InstrRisks even if it's too
          // late to subscribe:
          if (stratTime < ip.m_enabledUntil)
            ip.m_aggrMDC->SubscribeMktData
              (this, *ip.m_aggrInstr, aggrEffect, true);
          else
          if (m_riskMgr != nullptr)
            m_riskMgr->Register(*ip.m_aggrInstr, ip.m_aggrOB);
        }
      }
    }

    //-----------------------------------------------------------------------//
    // Check whether all MDCs and OMCs are now Active:                       //
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
  inline void Pairs_MF::CheckAllConnectors(utxx::time_val a_ts_strat)
  {
    if (utxx::likely(m_allConnsActive))
      return;

    //-----------------------------------------------------------------------//
    // Otherwise: The flag is not set yet. Re-check all Connectors now:      //
    //-----------------------------------------------------------------------//
    if (// MDCs:
        (m_mdcMICEX_EQ != nullptr && !(m_mdcMICEX_EQ->IsActive())) ||
        (m_mdcMICEX_FX != nullptr && !(m_mdcMICEX_FX->IsActive())) ||
        (m_mdcFORTS    != nullptr && !(m_mdcFORTS   ->IsActive())) ||
        (m_mdcAlfa     != nullptr && !(m_mdcAlfa    ->IsActive())) ||
        (m_mdcNTPro    != nullptr && !(m_mdcNTPro   ->IsActive())) ||
        (m_mdcHSFX_Gen != nullptr && !(m_mdcHSFX_Gen->IsActive())) ||
        (m_mdcCCMA     != nullptr && !(m_mdcCCMA    ->IsActive())) ||
        // OMCs:
        (m_omcMICEX_EQ != nullptr && !(m_omcMICEX_EQ->IsActive())) ||
        (m_omcMICEX_FX != nullptr && !(m_omcMICEX_FX->IsActive())) ||
        (m_omcFORTS    != nullptr && !(m_omcFORTS   ->IsActive())) ||
        (m_omcAlfa     != nullptr && !(m_omcAlfa    ->IsActive())) ||
        (m_omcNTPro    != nullptr && !(m_omcNTPro   ->IsActive())) ||
        (m_mdcHSFX_Gen != nullptr && !(m_omcHSFX_Gen->IsActive())) ||
        (m_omcCCMA     != nullptr && !(m_omcCCMA    ->IsActive())) )
      // Still not all Connectors are active:
      return;

    // All created Connectors are nominally Active. Check if all OrderBooks (in
    // MDCs) are ready -- for FAST, this should normally  be the case   (unless
    // the liquidity had evaporated somewhere). But exclude those IPairs  which
    // are not enabled due to time limits:
    //
    for (IPair const& ip: m_iPairs)
      if (a_ts_strat < ip.m_enabledUntil &&
          !(ip.m_passOB->IsReady()       && ip.m_aggrOB->IsReady()))
      {
        // May need to provide some diagnostics in this case, otherwise the
        // user may not know why the Strategy is not starting:
        OrderBook const* notReady =
          !(ip.m_passOB->IsReady()) ? ip.m_passOB : ip.m_aggrOB;
        SecDefD   const& instr    = notReady->GetInstr();
        char const*      conn     = notReady->GetMDC().GetName();

        LOG_INFO(3,
          "Pairs-MF::CheckAllConnectors: MDC={}, Instr={}: OrderBook Not "
          "Ready yet", conn, instr.m_FullName.data())

        // Cannot continue as yet:
        return;
      }

    // If we got here, the result is now positive:
    m_allConnsActive = true;

    //-----------------------------------------------------------------------//
    // We now can start the RiskMgr:                                         //
    //-----------------------------------------------------------------------//
    if (m_riskMgr != nullptr)
    {
      LOG_INFO(2, "Pairs-MF::CheckAllConnectors: Starting the RiskMgr...")
      try
      {
        //-------------------------------------------------------------------//
        // Also install the Valuators for both Assets and SettlCcys:         //
        //-------------------------------------------------------------------//
        // XXX:
        // (*) We do NOT check if such Valuators were installed previously:
        //     any new one will override the old one;
        // (*) Currently, the Valuation Rates are just explicitly-given consts:
        //     this is adequate provided that the total "IPair" position would
        //     be 0 in most cases:
        // (*) If any required Valuator is not installed, the RiskMgr will NOT
        //     start (will throw an exception):
        // (*) Because theses are Low-Precision Fixed Valuation Rates, they will
        //     apply to ALL SettlDates (so this mechanism cannot be used for eg
        //     Futures):
        auto const& ars = m_riskMgr->GetAllAssetRisks(0);

        for (auto const& fr: m_fixedRates)
        {
          char const* asset = fr.first.data();
          assert(*asset != '\0');
          double      rate  = fr.second;

          for (auto const&  arp: ars)
          {
            AssetRisks const& ar = arp.second;
            if (strcmp(ar.m_asset.data(), asset) == 0)
              // XXX: Here we are installing the Valuator on "ar" directly, by-
              // passing the RiskMgr!
              const_cast<AssetRisks&>(ar).InstallValuator(rate);
          }
        }
        //-------------------------------------------------------------------//
        // Now actually start the RiskMgr:                                   //
        //-------------------------------------------------------------------//
        m_riskMgr->Start();
        m_riskMgr->OutputAssetPositions("Initial Positions", 0);
      }
      catch (std::exception const& exc)
      {
        DelayedGracefulStop hmm
          (this, a_ts_strat,
           "Pairs-MF::CheckAllConnectors: Cannot Start the RiskMgr: ",
           exc.what());
        return;
      }
    }
    // Produce a log msg:
    LOG_INFO(1,
      "Pairs-MF::CheckAllConnectors: ALL CONNECTORS ARE NOW ACTIVE{}!",
      (m_riskMgr != nullptr) ? ", RiskMgr Started" : "")
    m_logger->flush();
  }

  //=========================================================================//
  // "OnOrderBookUpdate" Call-Back:                                          //
  //=========================================================================//
  inline void Pairs_MF::OnOrderBookUpdate
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
    // Check for Error Conds first:                                          //
    //-----------------------------------------------------------------------//
    SecDefD const& instr = a_ob.GetInstr();

    if (utxx::unlikely(a_is_error))
    {
      // XXX: This would be a really rare consition, so if it happens, we better
      // terminate the Strategy:
      // FIXME: No, we better cancel all quotes but still continue:
      DelayedGracefulStop hmm
        (this, a_ts_strat,
         "Pairs-MF::OnOrderBookUpdate: OrderBook Error: Instr=",
         instr.m_FullName.data());
      return;
    }

    //-----------------------------------------------------------------------//
    // Other Stopping Conds?                                                 //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely(EvalStopConds(a_ts_exch, a_ts_recv, a_ts_strat)))
      return;

    // Status of Connectors availability may change on OrderBook updates. Will
    // NOT be ready to proceed if not all Connectors are Active yet. (This chk
    // is not done in "EvalStopConds", because it is an Init rather than  Stop
    // Cond):
    CheckAllConnectors(a_ts_strat);
    if (utxx::unlikely(!m_allConnsActive))
      return;

    //-----------------------------------------------------------------------//
    // Traverse all IPairs:                                                  //
    //-----------------------------------------------------------------------//
    // This OrderBook belongds to 1 or more IPairs, where it can be Passive or
    // Aggressive:
    //
    for (IPair& ip: m_iPairs)
    {
      // If this OrderBooks does not correspond to either Pass or Aggr Instr of
      // this "IPair", the latter is completely irrelevant, and will be skipped:
      bool isPass  = (ip.m_aggrOB != &a_ob);
      bool isAggr  = (ip.m_passOB != &a_ob);
      if (!(isPass || isAggr))
        continue;

      //---------------------------------------------------------------------//
      // Do the Quotes:                                                      //
      //---------------------------------------------------------------------//
      // Yes, this OrderBook is for a Pass or Aggr Instr of this "IPair". In
      // EITHER case, Re-Quoting may be necessary (and the computational dif-
      // ference between the cases is minimal, so we will always do it):
      //
      ip.DoQuotes(a_ts_exch, a_ts_recv, a_ts_strat);

      //---------------------------------------------------------------------//
      // Process Not-so-Aggressive Orders:                                   //
      //---------------------------------------------------------------------//
      // (However, there could be no "stuck" orders if DeepAggr mode is used):
      //
      if (isAggr && ip.m_aggrMode != AggrModeT::DeepAggr)
      {
        for (AOS const* aos: ip.m_aggrAOSes)
        {
          // All AOSes here must be Active -- once an AOS becomes Inactive,  it
          // is removed (in "OnOurTrade"), so an Inactive AOS must NOT be found
          // here:
          if (utxx::unlikely(aos == nullptr || aos->m_isInactive))
          {
            DelayedGracefulStop hmm
              (this, a_ts_strat,
               "Pairs-MF::OnOrderBookUpdate: Stale AggrAOS: AOSID=",
               (aos == nullptr) ? 0 : aos->m_id);
            return;
          }

          // Skip this AggrOrder if its Instr does not match the updated OB, or
          // if it is already CxlPending:
          if (aos->m_instr != &(a_ob.GetInstr()) || aos->m_isCxlPending != 0)
            continue;

          // If OK: Check whether this Order needs to be made Deeply-Aggressive
          // (this is a Stop-Loss action):
          bool deepAggr =
            ip.EvalStopLoss(a_ob, aos, a_ts_exch, a_ts_recv, a_ts_strat);

          // If it has not been made Deeply-Aggressive, and it is Pegged, we may
          // need to modify it:
          if (!deepAggr && ip.m_aggrMode == AggrModeT::Pegged)
            ip.ModifyPeggedOrder(aos, a_ob, a_ts_exch, a_ts_recv, a_ts_strat);
        }
        // Flush Stop-Loss Orders which may have been generated above:
        ip.m_aggrOMC->FlushOrders();
      }

      //---------------------------------------------------------------------//
      // Check if any Hedge Adjustments are required:                        //
      //---------------------------------------------------------------------//
      // NB: We do it here simply because invocation  of this method acts as a
      // "ticker". Hedge Skews (Incomplete Hedges requiring an Adjustment) may
      // occur due to various unforeseen reasons, ie an erroneous Strategy ter-
      // mination. But only check for them if there are no currently-active Aggr
      // Orders in this IPair, and control the frequency of such checks:
      //
      if (utxx::unlikely
         (ip.m_aggrAOSes.empty()      &&
         (ip.m_lastAggrPosAdj.empty() ||
         (a_ts_strat - ip.m_lastAggrPosAdj).seconds() >= 5))) // 5 sec delay!!!
      {
        ip.m_lastAggrPosAdj = a_ts_strat;   // Move the TimeStamp forward

        long desiredAggrPos =
          - long(Round(double(ip.m_passPos) * ip.m_aggrQtyFact));

        if (utxx::unlikely(desiredAggrPos != ip.m_aggrPos))
        {
          // Yes, hedging is not perfect:
          bool adjIsBuy =     (ip.m_aggrPos < desiredAggrPos);
          long adjQty   = labs(ip.m_aggrPos - desiredAggrPos);

          // Issue an "Adjustment" AggrOrder (IsAggr=true, Buffered=false). But
          // do NOT do that if its size is smaller than the corresp LotSize, be-
          // cause we could then issue an inlimited number of "flip-flop"  aggr-
          // essive orders:
          long lotSz  = long(Round(ip.m_aggrInstr->m_LotSize));
          if (adjQty >= lotSz)
          {
            AOS const* adjAOS =
              ip.NewOrderSafe<true, false>
              (
                adjIsBuy,  0, PriceT(), PriceT(), PriceT(), 0.0, 0, adjQty,
                a_ts_exch, a_ts_recv,   a_ts_strat
              );

            if (utxx::likely(adjAOS != nullptr))
            {
              Req12 const*   adjReq  = adjAOS->m_lastReq;
              assert(adjReq != nullptr && adjReq->m_kind == Req12::KindT::New);

              LOG_INFO(3,
                "Pairs-MF::OnOrderBookUpdate: HedgeAdjustment AggrOrder: "
                "AOSID={}, ReqID={}, Instr={}, {}, Qty={}",
                adjAOS->m_id, adjReq->m_id, adjAOS->m_instr->m_FullName.data(),
                adjAOS->m_isBuy ? "Bid" : "Ask",   adjQty)
            }
          }
        }
      }
    }
    //-----------------------------------------------------------------------//
    // Optionally, Log the OrderBook Spectrum:                               //
    //-----------------------------------------------------------------------//
    if (m_mktDataLogger != nullptr && m_mktDataDepth > 0)
    {
      long  lat_ns = (a_ts_strat - a_ts_recv).nanoseconds();
      LogOrderBook(a_ob, lat_ns);
    }
    // All Done?
  }

  //=========================================================================//
  // "OnTradeUpdate" Call-Back:                                              //
  //=========================================================================//
  // Generic (not necessarily our) Trade:
  //
  inline void Pairs_MF::OnTradeUpdate(Trade const& a_tr)
  {
    // Optionally, log this trade:
    if (m_mktDataLogger != nullptr)
      LogTrade(a_tr);
  }

  //=========================================================================//
  // "OnOurTrade" Call-Back:                                                 //
  //=========================================================================//
  inline void Pairs_MF::OnOurTrade(Trade const& a_tr)
  {
    utxx::time_val stratTime = utxx::now_utc();
    utxx::time_val exchTime  = a_tr.m_exchTS;
    utxx::time_val recvTime  = a_tr.m_recvTS;

    //-----------------------------------------------------------------------//
    // Get the AOS and the IPair:                                            //
    //-----------------------------------------------------------------------//
    Req12 const* req = a_tr.m_ourReq; // OK, because OurReq ptr is non-NULL!
    assert(req != nullptr);

    AOS const* aos = req->m_aos;
    assert(aos != nullptr);

    bool isBid = aos->m_isBuy;
    long   qty = long(a_tr.GetQty<QTU,QR>());
    PriceT  px = a_tr.m_px;
    assert(qty > 0 && IsFinite(px));
    ExchOrdID const&  execID = a_tr.m_execID;

    // Get the Order Details:
    auto   od  = GetOrderDetails(aos, stratTime, "OnOurTrade");

    if (utxx::unlikely(od.first == nullptr || od.second == nullptr))
      return;   // It's an error cond -- we are already stopping...

    // Otherwise:
    OrderInfo&  ori  = *(od.first);
    IPair&      ip   = *(od.second);

    //-----------------------------------------------------------------------//
    // Passive Order:                                                        //
    //-----------------------------------------------------------------------//
    if (!ori.m_isAggr)
    {
      // XXX: Is it possible that the corresp PassAOSes slot contains a DIFFER-
      // ENT non-NULL AOS ptr? -- Yes, consider the following chain of events:
      // (*) the order was Filled, but we did not receive the corresp Trade evt
      //     yet;
      // (*) there was a Modify Req in the PipeLine, it has now Failed (for ob-
      //     vious reasons), and we got the corresp Fail avant ahead of the Fill
      //     event;
      // (*) moreover, that Fail event "somhow" (from the underlying Protocol)
      //     contained an indication that the Order no longer exists  at  all
      //     (yes -- because it has actually been Filled!);
      // (*) so we have already NULLified the corresp PassAOSes slot in "OnOrd-
      //     erError";
      // (*) then the corresp slot was re-quoted, it now contains a new unrela-
      //     ted AOS ptr;
      // (*) and only after that, the Trade(Fill) event for the prev AOS comes!
      // This is possible but very unlikely -- so produce a warning in such ca-
      // ses, but continue:
      //
      if (utxx::unlikely(ip.m_passAOSes[isBid] != nullptr &&
                         ip.m_passAOSes[isBid] != aos))
        LOG_WARN(2,
          "Pairs-MF::OnOurTrade: Inconsistency: ReqID={}, PassAOSID={}, Instr="
          "{}, {}: Recorded PassAOSID={}",
          req->m_id,  aos->m_id, aos->m_instr->m_FullName.data(),
          isBid ? "Bid" : "Ask", ip.m_passAOSes[isBid]->m_id)

      // OK: If it is a Complete Fill, reset the PassAOS ptr (if it was for THIS
      // AOS!), otherwise Cancel the rest (Buffered=false).
      //
      if (aos->m_isInactive)
      {
        if (utxx::likely(ip.m_passAOSes[isBid] == aos))
          ip.m_passAOSes[isBid] = nullptr;
      }
      else
        ip.CancelOrderSafe<false>(aos, exchTime, recvTime, stratTime);

      // Update the position (though it is maintained by the RiskMgr as well):
      if (isBid)
        ip.m_passPos += qty;
      else
        ip.m_passPos -= qty;

      // Calculate the PassSlip (positive if there was indeed a slip) and save
      // it in the OrderInfo. If it was already there, it cannot change (beca-
      // use Part-Filled orders cannot be moved) -- we do not check that:
      double passSlip =
        isBid
        ? (px - ori.m_expPassPx)
        : (ori.m_expPassPx - px);
      passSlip = std::max<double>(passSlip, 0.0);
      assert(passSlip >= 0.0);
      ori.m_passSlip  = passSlip;

      // Possibly issue the Covereing Order:
      ip.DoCoveringOrder(aos, qty, exchTime, recvTime, stratTime);

      // Put (or adjust) the LastFill TimeStamp:
      ip.m_passLastFillTS = stratTime;

      // Only now, log this Fill itself:
      LOG_INFO(2,
        "Pairs-MF::OnOurTrade: PASSIVE FILL: ReqID={}, PassAOSID={}, Instr={}"
        ", {}, Px={}, ExpPxLast={}, AggrLots={}, Qty={}, ExecID={}, MDEntryID="
        "{}{}: PassPos={}, AggrPos={}",
        req->m_id,  aos->m_id, aos->m_instr->m_FullName.data(),
        isBid ? "Bid" : "Ask", double(px), double(ori.m_expPassPx),
        ori.m_aggrLotsNew,     qty,        execID.ToString(),
        req->m_mdEntryID.GetOrderID(),
        (aos != nullptr  && aos->m_isCxlPending != 0)
          ? ", Cancelling the Rest" : "",
        ip.m_passPos,          ip.m_aggrPos)
    }
    else
    //-----------------------------------------------------------------------//
    // Aggressive Order:                                                     //
    //-----------------------------------------------------------------------//
    {
      // It must the be on the "m_aggrAOSes" list -- will be removed from there
      // if it was completely Filled:
      ip.RemoveInactiveAggrOrder(aos, "OnOurTrade");

      // Update the position (though it is maintained by the RiskMgr as well):
      if (isBid)
        ip.m_aggrPos += qty;
      else
        ip.m_aggrPos -= qty;

      LOG_INFO(2,
        "Pairs-MF::OnOurTrade: AGGRESSIVE FILL: ReqID={}, AggrAOSID={}, "
        "Instr={}, {}, Px={}, RefAOSID={}, ExpPxLast={}, ExpPxNew={}, "
        "AggrLots={}, Qty={}, ExecID={}: PassPos={}, AggrPos={}",
        req->m_id,  aos->m_id, aos->m_instr->m_FullName.data(),
        isBid ? "Bid" : "Ask",       double(px),         ori.m_refAOSID,
        double(ori.m_expAggrPxLast), double(ori.m_expAggrPxNew),
        ori.m_aggrLotsNew,     qty,  execID.ToString(),  ip.m_passPos,
        ip.m_aggrPos)
    }
    m_logger->flush();

    //-----------------------------------------------------------------------//
    // Finally, log this trade along with MktData:                           //
    //-----------------------------------------------------------------------//
    if (m_mktDataLogger != nullptr)
      // NB: Use QtyLots here, not Qty:
      LogOurTrade
        (aos->m_instr->m_FullName.data(), isBid, px,
         long(a_tr.GetQty<QTU,QR>()),     execID);
  }

  //=========================================================================//
  // "EvalStopConds":                                                        //
  //=========================================================================//
  // Returns "true" iff we should NOT proceed any further:
  //
  inline bool Pairs_MF::EvalStopConds
  (
    utxx::time_val  a_ts_exch,
    utxx::time_val  a_ts_recv,
    utxx::time_val  a_ts_strat
  )
  {
    //----------------------------------------------------------------------//
    // Out-of-Trading-Schedule?                                             //
    //----------------------------------------------------------------------//
    // We do NOT exit in that case, but for safety, do not proceed either (NB:
    // we may or may not have received the corresp "TradingStopped" events, so
    // better check the Schedule explicitly):
    //
    utxx::time_val intraDay = a_ts_exch - GetCurrDate();
    bool           ok       = false;      // Found a Session Period

    for (int i = 0; i < NSess; ++i)
      if (utxx::likely
         (m_tradingSched[i][0] <= intraDay && intraDay <= m_tradingSched[i][1]))
      {
        ok = true;
        break;
      }
    if (utxx::unlikely(!ok))
      // Outside of all valid Session Periods: Return without terminating:
      return false;

    //----------------------------------------------------------------------//
    // RiskMgr has entered the Safe Mode?                                   //
    //----------------------------------------------------------------------//
    if (utxx::unlikely
       (m_riskMgr != nullptr && m_riskMgr->GetMode() == RMModeT::Safe))
    {
      // Stop the Strategy gracefully: Cancel all Quotes, wait some time for
      // the final reports:
      DelayedGracefulStop hmm
        (this, a_ts_strat,
         "Pairs-MF::EvalStopConds: RiskMgr entered SafeMode");
      return true;
    }

    //----------------------------------------------------------------------//
    // "DelayedStop" has been initiated?                                    //
    //----------------------------------------------------------------------//
    if (utxx::unlikely(!m_delayedStopInited.empty()))
    {
      // If 1 sec from "m_delayedStopInited" has passed, we explicitly  Cancel
      // all curr Quotes. XXX: It's OK to do so multiple times -- second time,
      // there will be just nothing to Cancel. NB: We do NOT invoke "CancelAll-
      // Quotes" immediately in "DelayedGracefulStop", because the latter  may
      // also be caused by exceeding the Requests Rate, and we don't  want  to
      // exacerbate this cond (otherwise, our Cancels could be rejected):
      //
      if ((a_ts_strat - m_delayedStopInited).sec() > 1)
        CancelAllQuotes(nullptr, a_ts_exch, a_ts_recv, a_ts_strat); // All MDCs

      // If, furhermore, 5 secs have passed, and we are not in "SemiGraceful-
      // Stop" yet, enter it now:
      if (!m_nowStopInited && (a_ts_strat - m_delayedStopInited).sec() > 5)
      {
        SemiGracefulStop();
        assert(m_nowStopInited);
      }
      return true;
    }

    //----------------------------------------------------------------------//
    // In any other stopping mode, or reached the max number of Rounds?     //
    //----------------------------------------------------------------------//
    if (utxx::unlikely (IsStopping() || m_roundsCount >= m_maxRounds))
      return true;

    // Otherwise: NOT stopping:
    return false;
  }

  //=========================================================================//
  // "OnCancel"  Call-Back:                                                  //
  //=========================================================================//
  inline void Pairs_MF::OnCancel
  (
    AOS   const&        a_aos,
    utxx::time_val      a_ts_exch,
    utxx::time_val      a_ts_recv
  )
  {
    //-----------------------------------------------------------------------//
    // Get the "IPair" this Order belonged to:                               //
    //-----------------------------------------------------------------------//
    // This could be a Passive Order, or a Non-Deep Aggressive one:
    //
    utxx::time_val stratTime = utxx::now_utc();
    bool               isBid = a_aos.m_isBuy;
    OrderInfo const&   ori   = a_aos.m_userData.Get<OrderInfo>();

    assert(a_aos.m_omc != nullptr && a_aos.m_instr != nullptr &&
           a_aos.m_isInactive);

    if (utxx::unlikely
       (ori.m_iPairID < 0 || ori.m_iPairID >= int(m_iPairs.size()) ))
    {
      // Invalid IPairID: This would be a serious error condition:
      DelayedGracefulStop hmm
        (this, stratTime,
         "Pairs-MF::OnCancel: AOSID=",     a_aos.m_id,     ", Instr=",
         a_aos.m_instr->m_FullName.data(), (isBid ? ", Bid" : ", Ask"),
         ": Invalid IPairID=",             ori.m_iPairID);
      return;
    }
    // If OK:
    IPair& ip = m_iPairs[size_t(ori.m_iPairID)];

    //-----------------------------------------------------------------------//
    // Non-Deep Aggressive Order?                                            //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely(ori.m_isAggr))
    {
      // Then it MUST be on the "m_aggrAOSes" list -- remove it from there:
      ip.RemoveInactiveAggrOrder(&a_aos, "OnCancel");

      // If the Order is only Part-Filled (and cancelled because we could not
      // then make it Deeply-Aggressive), issue a new Deeply-Aggressive Order
      // for the remaining Qty:
      long   remQty(a_aos.GetLeavesQty<QTU,QR>());
      assert(remQty >= 0);

      if (utxx::unlikely(remQty != 0))
      {
        AOS const* newAOS = ip.NewOrderSafe<true, false>  // Aggr, Non-Buffered
        (
          isBid,
          ori.m_refAOSID,
          ori.m_expAggrPxLast,
          ori.m_expAggrPxNew,
          PriceT(),
          NaN<double>,      // It will be Deeply-Aggressive Order!
          ori.m_aggrLotsNew,
          remQty,
          a_ts_exch,
          a_ts_recv,
          stratTime
        );

        if (utxx::unlikely(newAOS == nullptr))
        {
          // XXX: This is a rare and strange condition; terminate the Strategy:
          DelayedGracefulStop hmm
            (this, stratTime,
             "Pairs-MF::OnCancel: AOSID=",     a_aos.m_id,    ", Instr=",
             a_aos.m_instr->m_FullName.data(), isBid ? ", Bid" : ", Ask",
             ": Could not re-create the Aggr Order");
          return;
        }
        // If OK:
        LOG_INFO(3,
          "Pairs-MF::OnCancel: Instr={}: New Deep-Aggr Order, {}, Qty={}, "
          "AggrAOSID={}",  newAOS->m_instr->m_FullName.data(),
          newAOS->m_isBuy ? "Bid" : "Ask",  remQty,    newAOS->m_id)
      }
    }
    else
    //-----------------------------------------------------------------------//
    // OK, it was Passive. Analyse the IPair:                                //
    //-----------------------------------------------------------------------//
    {
      // NB: It is theoretically possible that this AOS was already removed from
      // its slot before (eg we got a Fail/Inactive event for it),  and somehow,
      // we now also got a Cancel -- but the slot is occupied by another AOS!
      // Yet this would be rare and strange, so produce a warning:
      //
      if (utxx::unlikely(ip.m_passAOSes[isBid] != nullptr &&
                         ip.m_passAOSes[isBid] != &a_aos))
      {
        LOG_WARN(2,
          "Pairs-MF::OnCancel: Inconsistency: PassAOSID={}, Instr={}, {}: "
          "Recorded PassAOSID={}",
          a_aos.m_id, a_aos.m_instr->m_FullName.data(), isBid ? "Bid" : "Ask",
          ip.m_passAOSes[isBid]->m_id)
      }
      else
        // OK, can clear it now:
        ip.m_passAOSes[isBid] = nullptr;

      // NB: Unlike any other cases, here we do NOT put or adjust the Passive
      // LastFill TS, because this is NOT a Fill-type event...
    }

    //-----------------------------------------------------------------------//
    // Finaly, Log it:                                                       //
    //-----------------------------------------------------------------------//
    LOG_INFO(3,
      "Pairs-MF::OnCancel: OMC={}, Instr={}, {}, AOSID={}: CANCELLED",
      a_aos.m_omc->GetName(), a_aos.m_instr->m_FullName.data(),
      isBid ? "Bid" : "Ask",  a_aos.m_id)
  }

  //=========================================================================//
  // "OnOrderError" Call-Back:                                               //
  //=========================================================================//
  inline void Pairs_MF::OnOrderError
  (
    Req12 const&     a_req,
    int              UNUSED_PARAM(a_err_code),
    char  const*     a_err_text,
    bool             UNUSED_PARAM(a_prob_filled), // TODO: Handle it properly!
    utxx::time_val   UNUSED_PARAM(a_ts_exch),
    utxx::time_val   UNUSED_PARAM(a_ts_recv)
  )
  {
    utxx::time_val stratTime = utxx::now_utc();

    //-----------------------------------------------------------------------//
    // Get the Order Details:                                                //
    //-----------------------------------------------------------------------//
    // Get the AOS and the OrderInfo:
    AOS const* aos    = a_req.m_aos;
    assert    (aos   != nullptr);
    bool     isBid = aos->m_isBuy;
    OrderInfo& ori = aos->m_userData.Get<OrderInfo>(); // NB: Ref!

    // Get the IPair:
    if (utxx::unlikely
       (ori.m_iPairID < 0 || ori.m_iPairID >= int(m_iPairs.size()) ))
    {
      // Invalid IPairID: This would be a serious error condition:
      DelayedGracefulStop hmm
        (this, stratTime,
         "Pairs-MF::OnOrderError: AOSID=", aos->m_id,       ", Instr=",
         aos->m_instr->m_FullName.data(),  (isBid ? ", Bid" : ", Ask"),
         ": Invalid IPairID=",             ori.m_iPairID);
      return;
    }
    // If OK:
    IPair& ip = m_iPairs[size_t(ori.m_iPairID)];

    assert(a_err_text != nullptr);

    //-----------------------------------------------------------------------//
    // Passive Order Failed?                                                 //
    //-----------------------------------------------------------------------//
    if (utxx::likely(!ori.m_isAggr))
    {
      // IMPORTANT: In any case, if a Passive Order is now Inactive, we may not
      // get any further Error or Cancel reports -- so must NULLify the corresp
      // slot now to enable re-quoting.
      // BUT BEWARE:  It might be that some *PREVIOUS* order is now reported as
      // Failed, whereas the slot already contains a new AOS ptr, so:
      //
      if (aos->m_isInactive && ip.m_passAOSes[isBid] == aos)
        ip.m_passAOSes[isBid] = nullptr;
    }
    else
    //-----------------------------------------------------------------------//
    // Aggressive Order Failed?                                              //
    //-----------------------------------------------------------------------//
      // FIXME: This is by far insufficient -- very simplistic reaction at the
      // moment:
      ip.RemoveInactiveAggrOrder(aos, "OnOrderError");

    //-----------------------------------------------------------------------//
    // Anyway: Log the Error:                                                //
    //-----------------------------------------------------------------------//
    LOG_ERROR(2,
      "Pairs-MF::OnOrderError: FAILED: ReqID={}, {}AOSID={}, Instr={}, {}: "
      "{}{}",
      a_req.m_id,   ori.m_isAggr ? "Aggr" : "Pass",            aos->m_id,
      aos->m_instr->m_FullName.data(),  isBid ? "Bid" : "Ask", a_err_text,
      aos->m_isInactive ? ": Order Inactive" : "")
  }

  //=========================================================================//
  // "OnSignal" Call-Back:                                                   //
  //=========================================================================//
  inline void Pairs_MF::OnSignal(signalfd_siginfo const& a_si)
  {
    utxx::time_val stratTime = utxx::now_utc();
    ++m_signalsCount;

    LOG_INFO(1,
      "Got a Signal={}, Count={}, Exiting {}", a_si.ssi_signo, m_signalsCount,
      (m_signalsCount == 1)  ?  "Gracefully" : "Immediately")
    m_logger->flush();

    if (m_signalsCount == 1)
    {
      // Initiate a stop:
      DelayedGracefulStop hmm(this, stratTime, "Pairs-MF::OnSignal");
    }
    else
    {
      assert(m_signalsCount >= 2);
      // Throw the "ExitRun" exception to exit the Main Event Loop (if we are
      // not in that loop, it will be handled safely anyway):
      if (m_riskMgr != nullptr)
        m_riskMgr->OutputAssetPositions("Pairs-MF::OnSignal", 0);
      m_reactor->ExitImmediately       ("Pairs-MF::OnSignal");
    }
  }

  //=========================================================================//
  // "LogOrderBook":                                                         //
  //=========================================================================//
  inline void Pairs_MF::LogOrderBook
  (
    OrderBook const& a_ob,
    long             a_lat_ns
  )
  const
  {
    assert(m_mktDataDepth > 0);
    char* curr = m_mktDataBuff;

    // Line Type:
    curr       = stpcpy(curr, "OrderBook ");
    // Instrument:
    curr       = stpcpy(curr, a_ob.GetInstr().m_FullName.data());

    // Action on OrderBook Traversal (each Side):
    auto action =
      [&curr](int, PriceT a_px, OrderBook::OBEntry const& a_obe)->bool
      {
        // Px:
        curr += utxx::ftoa_left(double(a_px), curr, 20, 5);
        *curr = ' ';
        ++curr;
        // Qty:
        long  qtyLots = long(a_obe.GetAggrQty<QTU,QR>());
        (void)utxx::itoa(qtyLots, curr);
        *curr = ' ';
        ++curr;
        return true;
      };

    // Bids:
    curr  = stpcpy(curr, " Bids: ");
    a_ob.Traverse<Bid>(m_mktDataDepth, action);
    // Asks:
    curr  = stpcpy(curr, " Asks: ");
    a_ob.Traverse<Ask>(m_mktDataDepth, action);

    // Finally, Latency:
    (void)utxx::itoa(a_lat_ns, curr);
    *curr = '\0';

    // The "MktDataBuff" must NOT be full:
    assert(size_t(curr + 1 - m_mktDataBuff) < sizeof(m_mktDataBuff));

    // Finally, log it, but do not flush the Logger:
    assert(m_mktDataLogger != nullptr);
    m_mktDataLogger->info(m_mktDataBuff);
  }

  //=========================================================================//
  // "LogTrade":                                                             //
  //=========================================================================//
  inline void Pairs_MF::LogTrade(Trade const& a_tr) const
  {
    char* curr = m_mktDataBuff;

    // Line Type:
    curr  = stpcpy(curr, "Trade ");
    // Instrument:
    curr  = stpcpy(curr, a_tr.m_instr->m_FullName.data());
    *curr = ' ';
    ++curr;
    // Trade Px:
    curr += utxx::ftoa_left(double(a_tr.m_px), curr, 20, 5);
    *curr = ' ';
    ++curr;
    // Trade Qty:
    (void)utxx::itoa(long(a_tr.GetQty<QTU,QR>()), curr);
    *curr = ' ';
    ++curr;
    // Aggressor:
    curr  = stpcpy(curr,
                   (a_tr.m_aggressor == FIX::SideT::Buy)
                   ? "BidAggr"   :
                   (a_tr.m_aggressor == FIX::SideT::Sell)
                    ? "SellAggr" : "???Aggr");
    *curr = ' ';
    ++curr;
    // TradeID:
    curr  = stpcpy(curr, a_tr.m_execID.ToString());
    *curr = '\0';
    ++curr;

    // The "MktDataBuff" must NOT be full:
    assert(size_t(curr - m_mktDataBuff) < sizeof(m_mktDataBuff));

    // Finally, log it, but do not flush the Logger:
    assert(m_mktDataLogger != nullptr);
    m_mktDataLogger->info(m_mktDataBuff);
  }

  //=========================================================================//
  // "LogOurTrade":                                                          //
  //=========================================================================//
  inline void Pairs_MF::LogOurTrade
  (
    char const*       a_symbol,
    bool              a_is_bid,
    PriceT            a_px,
    long              a_qty_lots,
    ExchOrdID const&  a_exec_id
  )
  const
  {
    char* curr = m_mktDataBuff;

    // Line Type:
    curr  = stpcpy(curr, "OurTrade ");
    // Instrument:
    assert(a_symbol != nullptr);
    curr  = stpcpy(curr, a_symbol);
    *curr = ' ';
    ++curr;
    // Trade Px:
    curr += utxx::ftoa_left(double(a_px), curr, 20, 5);
    *curr = ' ';
    ++curr;
    // Trade Qty (in Lots):
    (void)utxx::itoa(a_qty_lots, curr);
    // Side:
    curr  = stpcpy(curr, a_is_bid ? " Buy" : " Sell");
    *curr = ' ';
    ++curr;
    // TradeID:
    curr  = stpcpy(curr, a_exec_id.ToString());
    *curr = '\0';
    ++curr;

    // The "MktDataBuff" must NOT be full:
    assert(size_t(curr - m_mktDataBuff) < sizeof(m_mktDataBuff));

    // Finally, log it, and atthis point, flush the logger:
    assert(m_mktDataLogger != nullptr);
    m_mktDataLogger->info (m_mktDataBuff);
    m_mktDataLogger->flush();
  }
} // End namespace MAQUETTE
