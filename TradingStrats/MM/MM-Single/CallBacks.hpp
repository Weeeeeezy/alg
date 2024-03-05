// vim:ts=2:et
//===========================================================================//
//                  "TradingStrats/MM/MM-Single/CallBacks.hpp":              //
//                Implementation of Standard Strategy Call-Backs             //
//===========================================================================//
#pragma once

#include "MM-Single.h"
#include "Connectors/OrderBook.hpp"
#include "Basis/TimeValUtils.hpp"

namespace MAQUETTE {
  //=========================================================================//
  // "OnTradingEvent" Call-Back:                                             //
  //=========================================================================//
  template<class MDC, class OMC> inline void
  MM_Single<MDC, OMC>::OnTradingEvent(EConnector const& a_conn,
                            bool              a_on,
                            SecID             a_sec_id,
                            utxx::time_val    UNUSED_PARAM(a_ts_exch),
                            utxx::time_val    UNUSED_PARAM(a_ts_recv)) {
    utxx::time_val stratTime = utxx::now_utc();

    //-----------------------------------------------------------------------//
    // Is it SecID-specific?                                                 //
    //-----------------------------------------------------------------------//
    // If this SecID is not ours, ignore the event altogether. Also ignore it
    // (XXX: is this logic correct?) if "a_on" is set:
    //
    if (utxx::unlikely(a_sec_id != 0)) {
      if (a_on)
        return;

      bool isOurs = false;
      for (IPair const& ip : m_iPairs)
        if (ip.m_Instr->m_SecID == a_sec_id) {
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
    LOG_INFO(1, "MM-Single::OnTradingEvent: {} is now {}", a_conn.GetName(),
             a_on ? "ACTIVE" : "INACTIVE")
    m_logger->flush();

    //-----------------------------------------------------------------------//
    // Any Connector has become Inactive?                                    //
    //-----------------------------------------------------------------------//
    if (!a_on) {
      m_allConnsActive = false;

      // If all Connectors have actually stopped, exit from the Reactor Loop,
      // unless this call-back is invoked from within the Dtor  (then we are
      // not in the Reactor Loop anyway); "a_conn"  is certainly  one of the
      // Connectors listed below:
      if (  // MDCs:
          (m_mdc == nullptr || !(m_mdc->IsActive())) &&
          // OMCs:
          (m_omc == nullptr || !(m_omc->IsActive()))) {
        if (m_riskMgr != nullptr)
          m_riskMgr->OutputAssetPositions("MM-Single::OnTradingEvent", 0);
        m_reactor->ExitImmediately("MM-Single::OnTradingEvent");
        // This point is not reached!
        __builtin_unreachable();
      }

      // Otherwise: We cannot proceed with a non-working connector, so initiate
      // a Stop:
      DelayedGracefulStop hmm(
          this, stratTime,
          "MM-Single::OnTradingEvent: Connector Stopped: ", a_conn.GetName());
      return;
    }
    // Otherwise: Some Connector has become ACTIVE!..

    //-----------------------------------------------------------------------//
    // An MDC is now Active? -- Subscribe to the corresp MktData:            //
    //-----------------------------------------------------------------------//
    bool isMDC = (&a_conn == m_mdc);
    if (isMDC) {
      // Subscribe to MktData for all configured Instrs related  to this MDC:
      // XXX: In general, we must NOT use the MDC's list of SecDefs,  because
      // that list may contain instrs inherited from a prev Strategy invocation
      // (which are now removed). So go over all IPairs instead:
      // NB: RegisterInstrRisks=true in both cases: Ie HERE we add Instrs  and
      // their Assets to the RiskMgr -- NOT when SecDefs were created:
      //
      for (IPair const& ip : m_iPairs) {
        assert(ip.m_Instr != nullptr && ip.m_OB != nullptr);

        // For the Passive Instr, we need L2 data if the DeadZone is enabled;
        // otherwise, L1Px would do:
        OrderBook::UpdateEffectT passEffect = OrderBook::UpdateEffectT::L1Px;

        if (stratTime < ip.m_enabledUntil)
          // Yes, really subscribe:
          ip.m_MDC->SubscribeMktData(this, *ip.m_Instr, passEffect, true);
        else if (m_riskMgr != nullptr)
          // We STILL need register the corresp InstrRisks, otherwise the RM
          // may sometimes shut us down!
          m_riskMgr->Register(*ip.m_Instr, ip.m_OB);
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
  }  // namespace MAQUETTE

  //=========================================================================//
  // "CheckAllConnectors":                                                   //
  //=========================================================================//
  // Checks that all MDCs and OMCs are Active, and all configured OrderBooks are
  // fully ready:
  //
  template<class MDC, class OMC>
    inline void MM_Single<MDC, OMC>::CheckAllConnectors(utxx::time_val a_ts_strat) {
    if (utxx::likely(m_allConnsActive))
      return;

    //-----------------------------------------------------------------------//
    // Otherwise: The flag is not set yet. Re-check all Connectors now:      //
    //-----------------------------------------------------------------------//
    if (  // MDCs:
        (m_mdc != nullptr && !(m_mdc->IsActive())) ||
        // OMCs:
        (m_omc != nullptr && !(m_omc->IsActive())))
      // Still not all Connectors are active:
      return;

    // All created Connectors are nominally Active. Check if all OrderBooks (in
    // MDCs) are ready -- for FAST, this should normally  be the case   (unless
    // the liquidity had evaporated somewhere). But exclude those IPairs  which
    // are not enabled due to time limits:
    //
    for (IPair const& ip : m_iPairs)
      if (a_ts_strat < ip.m_enabledUntil && !ip.m_OB->IsReady()) {
        // May need to provide some diagnostics in this case, otherwise the
        // user may not know why the Strategy is not starting:
        OrderBook const* notReady = ip.m_OB;
        SecDefD const&   instr    = notReady->GetInstr();
        char const*      conn     = notReady->GetMDC().GetName();

        LOG_INFO(
            3,
            "MM-Single::CheckAllConnectors: MDC={}, Instr={}: OrderBook Not "
            "Ready yet",
            conn, instr.m_FullName.data())

        // Cannot continue as yet:
        return;
      }

    // If we got here, the result is now positive:
    m_allConnsActive = true;

    //-----------------------------------------------------------------------//
    // We now can start the RiskMgr:                                         //
    //-----------------------------------------------------------------------//
    if (m_riskMgr != nullptr) {
      LOG_INFO(2, "MM-Single::CheckAllConnectors: Starting the RiskMgr...")
      try {
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

        for (auto const& fr : m_fixedRates) {
          char const* asset = fr.first.data();
          assert(*asset != '\0');
          double rate = fr.second;

          for (auto const& arp : ars) {
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
      } catch (std::exception const& exc) {
        DelayedGracefulStop hmm(
            this, a_ts_strat,
            "MM-Single::CheckAllConnectors: Cannot Start the RiskMgr: ",
            exc.what());
        return;
      }
    }
    // Produce a log msg:
    LOG_INFO(1,
             "MM-Single::CheckAllConnectors: ALL CONNECTORS ARE NOW ACTIVE{}!",
             (m_riskMgr != nullptr) ? ", RiskMgr Started" : "")
    m_logger->flush();
  }

  //=========================================================================//
  // "OnOrderBookUpdate" Call-Back:                                          //
  //=========================================================================//
  template<class MDC, class OMC>
    inline void MM_Single<MDC, OMC>::OnOrderBookUpdate
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
    // SecDefD const& instr = a_ob.GetInstr();

    if (utxx::unlikely(a_is_error)) {
      // XXX: This would be a really rare consition, so if it happens, we better
      // terminate the Strategy:
      // DelayedGracefulStop hmm(
      //     this, a_ts_strat,
      //     "MM-Single::OnOrderBookUpdate: OrderBook Error: Instr=",
      //     instr.m_FullName.data());
      // return;

      // FIXME: No, we better cancel all quotes but still continue:
      for (IPair& ip : m_iPairs) {
        if (ip.m_OB != &a_ob) {
          ip.CancelQuotes(a_ts_exch, a_ts_recv, a_ts_strat);
          return;
        }
      }
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
    for (IPair& ip : m_iPairs) {
      // If this OrderBooks does not correspond to
      // this "IPair", the latter is completely irrelevant, and will be skipped:
      if (ip.m_OB != &a_ob)
        continue;

      //---------------------------------------------------------------------//
      // Do the Quotes:                                                      //
      //---------------------------------------------------------------------//
      // Yes, this OrderBook is for a Pass or Aggr Instr of this "IPair". In
      // EITHER case, Re-Quoting may be necessary (and the computational dif-
      // ference between the cases is minimal, so we will always do it):
      //
      ip.DoQuotes(a_ts_exch, a_ts_recv, a_ts_strat);
    }
    //-----------------------------------------------------------------------//
    // Optionally, Log the OrderBook Spectrum:                               //
    //-----------------------------------------------------------------------//
    if (m_mktDataLogger != nullptr && m_mktDataDepth > 0) {
      long lat_ns = (a_ts_strat - a_ts_recv).nanoseconds();
      LogOrderBook(a_ob, lat_ns);
    }
    // All Done?
  }

  //=========================================================================//
  // "OnTradeUpdate" Call-Back:                                              //
  //=========================================================================//
  // Generic (not necessarily our) Trade:
  //
  template<class MDC, class OMC>
    inline void MM_Single<MDC, OMC>::OnTradeUpdate(Trade const& a_tr) {
    // Optionally, log this trade:
    if (m_mktDataLogger != nullptr)
      LogTrade(a_tr);
  }

  //=========================================================================//
  // "OnOurTrade" Call-Back:                                                 //
  //=========================================================================//
  template<class MDC, class OMC>
    inline void MM_Single<MDC, OMC>::OnOurTrade(Trade const& a_tr) {
    utxx::time_val stratTime = utxx::now_utc();
    // utxx::time_val exchTime  = a_tr.m_exchTS;
    // utxx::time_val recvTime  = a_tr.m_recvTS;

    //-----------------------------------------------------------------------//
    // Get the AOS and the IPair:                                            //
    //-----------------------------------------------------------------------//
    Req12 const* req = a_tr.m_ourReq;  // OK, because OurReq ptr is non-NULL!
    assert(req != nullptr);

    AOS const* aos = req->m_aos;
    assert(aos != nullptr);

    bool   isBid = aos->m_isBuy;
    QR   qty     = QR(a_tr.GetQty<QT,QR>());
    PriceT px    = a_tr.m_px;
    assert(qty > 0 && IsFinite(px));
    ExchOrdID const& execID = a_tr.m_execID;

    // Get the Order Details:
    auto od = GetOrderDetails(aos, stratTime, "OnOurTrade");

    if (utxx::unlikely(od.first == nullptr || od.second == nullptr))
      return;  // It's an error cond -- we are already stopping...

    // Otherwise:
    OrderInfo& ori = *(od.first);
    IPair&     ip  = *(od.second);

    if (utxx::unlikely(ip.m_AOSes[isBid] != nullptr &&
                       ip.m_AOSes[isBid] != aos))
      LOG_WARN(2,
               "MM-Single::OnOurTrade: Inconsistency: ReqID={}, "
               "PassAOSID={}, Instr="
               "{}, {}: Recorded PassAOSID={}",
               req->m_id, aos->m_id, aos->m_instr->m_FullName.data(),
               isBid ? "Bid" : "Ask", ip.m_AOSes[isBid]->m_id)

    // OK: If it is a Complete Fill, reset the PassAOS ptr (if it was for THIS
    // AOS!), otherwise Cancel the rest (Buffered=false).
    //
    if (aos->m_isInactive) {
      if (utxx::likely(ip.m_AOSes[isBid] == aos))
        ip.m_AOSes[isBid] = nullptr;
    }

    // Update the position (though it is maintained by the RiskMgr as well):
    if (isBid)
      ip.m_Pos += qty;
    else
      ip.m_Pos -= qty;

    // Calculate the PassSlip (positive if there was indeed a slip) and save
    // it in the OrderInfo. If it was already there, it cannot change (beca-
    // use Part-Filled orders cannot be moved) -- we do not check that:
    double slip = isBid ? (px - ori.m_expPx) : (ori.m_expPx - px);
    slip        = std::max<double>(slip, 0.0);
    assert(slip >= 0.0);
    ori.m_Slip = slip;

    // Put (or adjust) the LastFill TimeStamp:
    ip.m_lastFillTS = stratTime;

    // Only now, log this Fill itself:
    LOG_INFO(2,
             "MM-Single::OnOurTrade: FILL: ReqID={}, AOSID={}, "
             "Instr={}, Side={}, Px={}, ExpPxLast={}, Qty={}, ExecID={}, "
             "MDEntryID="
             "{}: Pos={}",
             req->m_id, aos->m_id, aos->m_instr->m_FullName.data(),
             isBid ? "Bid" : "Ask", double(px), double(ori.m_expPx), qty,
             execID.ToString(), req->m_mdEntryID.GetOrderID(),
             ip.m_Pos)
    m_logger->flush();

    //-----------------------------------------------------------------------//
    // Finally, log this trade along with MktData:                           //
    //-----------------------------------------------------------------------//
    if (m_mktDataLogger != nullptr)
      // NB: Use QtyLots here, not Qty:
      LogOurTrade(aos->m_instr->m_FullName.data(), isBid, px,
                  QR(a_tr.GetQty<QT,QR>()), execID);
  }

  //=========================================================================//
  // "EvalStopConds":                                                        //
  //=========================================================================//
  // Returns "true" iff we should NOT proceed any further:
  //
  template<class MDC, class OMC>
    inline bool MM_Single<MDC, OMC>::EvalStopConds(utxx::time_val a_ts_exch,
                                       utxx::time_val a_ts_recv,
                                       utxx::time_val a_ts_strat) {
    //----------------------------------------------------------------------//
    // RiskMgr has entered the Safe Mode?                                   //
    //----------------------------------------------------------------------//
    if (utxx::unlikely(m_riskMgr != nullptr &&
                       m_riskMgr->GetMode() == RMModeT::Safe)) {
      // Stop the Strategy gracefully: Cancel all Quotes, wait some time for
      // the final reports:
      DelayedGracefulStop hmm(
          this, a_ts_strat,
          "MM-Single::EvalStopConds: RiskMgr entered SafeMode");
      return true;
    }

    //----------------------------------------------------------------------//
    // "DelayedStop" has been initiated?                                    //
    //----------------------------------------------------------------------//
    if (utxx::unlikely(!m_delayedStopInited.empty())) {
      // If 1 sec from "m_delayedStopInited" has passed, we explicitly  Cancel
      // all curr Quotes. XXX: It's OK to do so multiple times -- second time,
      // there will be just nothing to Cancel. NB: We do NOT invoke "CancelAll-
      // Quotes" immediately in "DelayedGracefulStop", because the latter  may
      // also be caused by exceeding the Requests Rate, and we don't  want  to
      // exacerbate this cond (otherwise, our Cancels could be rejected):
      //
      if ((a_ts_strat - m_delayedStopInited).sec() > 1)
        CancelAllQuotes(nullptr, a_ts_exch, a_ts_recv, a_ts_strat);  // All MDCs

      // If, furhermore, 5 secs have passed, and we are not in "SemiGraceful-
      // Stop" yet, enter it now:
      if (!m_nowStopInited && (a_ts_strat - m_delayedStopInited).sec() > 5) {
        SemiGracefulStop();
        assert(m_nowStopInited);
      }
      return true;
    }

    //----------------------------------------------------------------------//
    // In any other stopping mode, or reached the max number of Rounds?     //
    //----------------------------------------------------------------------//
    if (utxx::unlikely(IsStopping() || m_roundsCount >= m_maxRounds))
      return true;

    // Otherwise: NOT stopping:
    return false;
  }

  //=========================================================================//
  // "OnCancel"  Call-Back:                                                  //
  //=========================================================================//
  template<class MDC, class OMC>
    inline void MM_Single<MDC, OMC>::OnCancel(AOS const&     a_aos,
                                  utxx::time_val UNUSED_PARAM(a_ts_exch),
                                  utxx::time_val UNUSED_PARAM(a_ts_recv)) {
    //-----------------------------------------------------------------------//
    // Get the "IPair" this Order belonged to:                               //
    //-----------------------------------------------------------------------//
    utxx::time_val   stratTime = utxx::now_utc();
    bool             isBid     = a_aos.m_isBuy;
    OrderInfo const& ori       = a_aos.m_userData.Get<OrderInfo>();

    assert(a_aos.m_omc != nullptr && a_aos.m_instr != nullptr &&
           a_aos.m_isInactive);

    if (utxx::unlikely(ori.m_iPairID < 0 ||
                       ori.m_iPairID >= int(m_iPairs.size()))) {
      // Invalid IPairID: This would be a serious error condition:
      DelayedGracefulStop hmm(
          this, stratTime, "MM-Single::OnCancel: AOSID=", a_aos.m_id,
          ", Instr=", a_aos.m_instr->m_FullName.data(),
          (isBid ? ", Bid" : ", Ask"), ": Invalid IPairID=", ori.m_iPairID);
      return;
    }
    // If OK:
    IPair& ip = m_iPairs[size_t(ori.m_iPairID)];

    //-----------------------------------------------------------------------//
    // OK, it was Passive. Analyse the IPair:                                //
    //-----------------------------------------------------------------------//
    // NB: It is theoretically possible that this AOS was already removed from
    // its slot before (eg we got a Fail/Inactive event for it),  and somehow,
    // we now also got a Cancel -- but the slot is occupied by another AOS!
    // Yet this would be rare and strange, so produce a warning:
    //
    if (utxx::unlikely(ip.m_AOSes[isBid] != nullptr &&
                       ip.m_AOSes[isBid] != &a_aos)) {
      LOG_WARN(
          2,
          "MM-Single::OnCancel: Inconsistency: AOSID={}, Instr={}, {}: "
          "Recorded PassAOSID={}",
          a_aos.m_id, a_aos.m_instr->m_FullName.data(), isBid ? "Bid" : "Ask",
          ip.m_AOSes[isBid]->m_id)
    } else
      // OK, can clear it now:
      ip.m_AOSes[isBid] = nullptr;

    // NB: Unlike any other cases, here we do NOT put or adjust the Passive
    // LastFill TS, because this is NOT a Fill-type event...

    //-----------------------------------------------------------------------//
    // Finaly, Log it:                                                       //
    //-----------------------------------------------------------------------//
    LOG_INFO(3,
             "MM-Single::OnCancel: OMC={}, Instr={}, {}, AOSID={}: CANCELLED",
             a_aos.m_omc->GetName(), a_aos.m_instr->m_FullName.data(),
             isBid ? "Bid" : "Ask", a_aos.m_id)
  }

  //=========================================================================//
  // "OnOrderError" Call-Back:                                               //
  //=========================================================================//
  template<class MDC, class OMC>
  inline void MM_Single<MDC, OMC>::OnOrderError
  (
    Req12 const&   a_req,
    int            UNUSED_PARAM(a_err_code),
    char const*    a_err_text,
    bool           UNUSED_PARAM(a_prob_filled), // TODO: Handle it properly!
    utxx::time_val UNUSED_PARAM(a_ts_exch),
    utxx::time_val UNUSED_PARAM(a_ts_recv))
  {
    utxx::time_val stratTime = utxx::now_utc();

    //-----------------------------------------------------------------------//
    // Get the Order Details:                                                //
    //-----------------------------------------------------------------------//
    // Get the AOS and the OrderInfo:
    AOS const* aos = a_req.m_aos;
    assert(aos != nullptr);
    bool       isBid = aos->m_isBuy;
    OrderInfo& ori   = aos->m_userData.Get<OrderInfo>();  // NB: Ref!

    // Get the IPair:
    if (utxx::unlikely(ori.m_iPairID < 0 ||
                       ori.m_iPairID >= int(m_iPairs.size()))) {
      // Invalid IPairID: This would be a serious error condition:
      DelayedGracefulStop hmm(
          this, stratTime, "MM-Single::OnOrderError: AOSID=", aos->m_id,
          ", Instr=", aos->m_instr->m_FullName.data(),
          (isBid ? ", Bid" : ", Ask"), ": Invalid IPairID=", ori.m_iPairID);
      return;
    }
    // If OK:
    IPair& ip = m_iPairs[size_t(ori.m_iPairID)];

    assert(a_err_text != nullptr);

    // IMPORTANT: In any case, if a Passive Order is now Inactive, we may not
    // get any further Error or Cancel reports -- so must NULLify the corresp
    // slot now to enable re-quoting.
    // BUT BEWARE:  It might be that some *PREVIOUS* order is now reported as
    // Failed, whereas the slot already contains a new AOS ptr, so:
    //
    if (aos->m_isInactive && ip.m_AOSes[isBid] == aos)
      ip.m_AOSes[isBid] = nullptr;

    //-----------------------------------------------------------------------//
    // Anyway: Log the Error:                                                //
    //-----------------------------------------------------------------------//
    LOG_ERROR(
        2,
        "MM-Single::OnOrderError: FAILED: ReqID={}, AOSID={}, Instr={}, {}: "
        "{}{}",
        a_req.m_id, aos->m_id,
        aos->m_instr->m_FullName.data(), isBid ? "Bid" : "Ask", a_err_text,
        aos->m_isInactive ? ": Order Inactive" : "")
  }

  //=========================================================================//
  // "OnSignal" Call-Back:                                                   //
  //=========================================================================//
  template<class MDC, class OMC>
    inline void MM_Single<MDC, OMC>::OnSignal(signalfd_siginfo const& a_si) {
    utxx::time_val stratTime = utxx::now_utc();
    ++m_signalsCount;

    LOG_INFO(1, "Got a Signal={}, Count={}, Exiting {}", a_si.ssi_signo,
             m_signalsCount,
             (m_signalsCount == 1) ? "Gracefully" : "Immediately")
    m_logger->flush();

    if (m_signalsCount == 1) {
      // Initiate a stop:
      DelayedGracefulStop hmm(this, stratTime, "MM-Single::OnSignal");
    } else {
      assert(m_signalsCount >= 2);
      // Throw the "ExitRun" exception to exit the Main Event Loop (if we are
      // not in that loop, it will be handled safely anyway):
      if (m_riskMgr != nullptr)
        m_riskMgr->OutputAssetPositions("MM-Single::OnSignal", 0);
      m_reactor->ExitImmediately("MM-Single::OnSignal");
    }
  }

  //=========================================================================//
  // "LogOrderBook":                                                         //
  //=========================================================================//
  template<class MDC, class OMC>
    inline void MM_Single<MDC, OMC>::LogOrderBook(OrderBook const& a_ob,
                                      long             a_lat_ns) const {
    assert(m_mktDataDepth > 0);
    char* curr = m_mktDataBuff;

    // Line Type:
    curr = stpcpy(curr, "OrderBook ");
    // Instrument:
    curr = stpcpy(curr, a_ob.GetInstr().m_FullName.data());

    // Action on OrderBook Traversal (each Side):
    auto action = [&curr]
                  (int, PriceT a_px, OrderBook::OBEntry const& a_obe) -> bool
    {
      // Px:
      curr += utxx::ftoa_left(double(a_px), curr, 20, 5);
      *curr = ' ';
      ++curr;
      // Qty:
      long qtyLots = long(a_obe.GetAggrQty<QT,QR>());
      (void)utxx::itoa(qtyLots, curr);
      *curr = ' ';
      ++curr;
      return true;
    };

    // Bids:
    curr = stpcpy(curr, " Bids: ");
    a_ob.Traverse<Bid>(m_mktDataDepth, action);
    // Asks:
    curr = stpcpy(curr, " Asks: ");
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
  template<class MDC, class OMC>
    inline void MM_Single<MDC, OMC>::LogTrade(Trade const& a_tr) const {
    char* curr = m_mktDataBuff;

    // Line Type:
    curr = stpcpy(curr, "Trade ");
    // Instrument:
    curr  = stpcpy(curr, a_tr.m_instr->m_FullName.data());
    *curr = ' ';
    ++curr;
    // Trade Px:
    curr += utxx::ftoa_left(double(a_tr.m_px), curr, 20, 5);
    *curr = ' ';
    ++curr;
    // Trade Qty:
    (void)utxx::itoa(long(a_tr.GetQty<QT,QR>()), curr);
    *curr = ' ';
    ++curr;
    // Aggressor:
    curr =
        stpcpy(curr, (a_tr.m_aggressor == FIX::SideT::Buy)
                         ? "BidAggr"
                         : (a_tr.m_aggressor == FIX::SideT::Sell) ? "SellAggr"
                                                                  : "???Aggr");
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
  template<class MDC, class OMC>
    inline void MM_Single<MDC, OMC>::LogOurTrade(char const*      a_symbol,
                                     bool             a_is_bid,
                                     PriceT           a_px,
                                     QR               a_qty_lots,
                                     ExchOrdID const& a_exec_id) const {
    char* curr = m_mktDataBuff;

    // Line Type:
    curr = stpcpy(curr, "OurTrade ");
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
    (void)utxx::ftoa_left(a_qty_lots, curr, 20, 5);
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
    m_mktDataLogger->info(m_mktDataBuff);
    m_mktDataLogger->flush();
  }
}  // End namespace MAQUETTE
