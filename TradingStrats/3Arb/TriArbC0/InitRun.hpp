// vim:ts=2:et
//===========================================================================//
//                  "TradingStrats/3Arb/TriArbC0/InitRun.hpp":               //
//                             Ctor, Dtor, Run                               //
//===========================================================================//
#pragma  once
#include "TradingStrats/3Arb/TriArbC0/TriArbC0.h"
#include "Basis/ConfigUtils.hpp"
#include "InfraStruct/SecDefsMgr.h"
#include "InfraStruct/RiskMgr.h"
#include <utxx/error.hpp>
#include <algorithm>

namespace MAQUETTE
{
  //=========================================================================//
  // "AOSExt::CheckInvarints":                                               //
  //=========================================================================//
  // Only enabled in the DEBUG mode. If this method is invoked, we assume that
  // this "AOSExt" must NOT be empty, ie must be created by a Non-Default Ctor:
  //
  template<ExchC0T X>
  inline void TriArbC0<X>::AOSExt::CheckInvariants() const
  {
# ifndef NDEBUG
    assert(m_thisAOS != nullptr && m_trInfo != nullptr);

    if (m_aggrJ < 0)
    {
      // This AOS must be the passive one in the corresp TrInfo:
      int passI = m_trInfo->m_passI;
      assert(m_trInfo->m_passAOS == m_thisAOS && m_prevAggrAOS == nullptr &&
             0 <= passI && passI <= 2);

      // Also check that all Req12s of that AOS are indeed Passive. XXX: There
      // may be quite a lot of them, due to continuous re-quoting!
      assert(m_thisAOS->m_frstReq != nullptr);
      for (Req12 const* req = m_thisAOS->m_frstReq; req != nullptr;
           req = req->m_next)
        assert(!(req->m_isAggr));
    }
    else
    {
      // There is one or more corresp AggrAOSes. The AOS associated with this
      // "AOSExt" must be one of them:
      assert(0 <= m_aggrJ && m_aggrJ <= 2);
      AOS const* lastAOS  =  m_trInfo->m_aggrLastAOSes[m_aggrJ];
      assert    (lastAOS  != nullptr);

      bool found = false;
      AOS const* prevAOS  = nullptr;

      for (AOS const* aggrAOS = lastAOS; aggrAOS != nullptr;
           aggrAOS  = prevAOS)
      {
        prevAOS = aggrAOS->m_userData.Get<AOSExt>().m_prevAggrAOS;
        if (aggrAOS == m_thisAOS)
        {
          found = true;
          assert(prevAOS == m_prevAggrAOS);   // Of course...
        }
        // Also, since these are Aggressive AOSes, they have only one Req12
        // which is Aggressive:
        Req12 const* req = aggrAOS->m_frstReq;
        assert( req != nullptr && req->m_next == nullptr && req->m_isAggr);
      }
      assert(found);
    }
# endif
  }

  //=========================================================================//
  // "TrInfo": Default Ctor:                                                 //
  //=========================================================================//
  template<ExchC0T X>
  inline TriArbC0<X>::TrInfo::TrInfo()
  : m_id            (0),
    m_passI         (-1),
    m_passS         (-1),
    m_passInstr     (nullptr),
    m_passQty       (),            // 0
    m_passPx        (),            // NaN
    m_passAOS       (nullptr),
    m_aggrIs        { -1,          -1,          -1      },
    m_aggrSs        { -1,          -1,          -1      },
    m_aggrInstrs    { nullptr,     nullptr,     nullptr },
    m_aggrQtys      {},            // 0s
    m_aggrVols      {},            // 0s
    m_aggrSrcPxs    {},            // NaNs
    m_aggrLastAOSes { nullptr,     nullptr,     nullptr },
    m_passFilledQty (0.0),
    m_passAvgFillPx (0.0),
    m_aggrFilledQtys{},            // 0s
    m_aggrAvgFillPxs{ PriceT(0.0), PriceT(0.0), PriceT(0.0) },
    m_posA          (0.0),
    m_posB          (0.0),
    m_posC          (0.0),
    m_PnLRFC        (0.0)
  {}

  //=========================================================================//
  // "TrInfo::CheckInvariants":                                              //
  //=========================================================================//
  // Only enabled in the DEBUG mode. If this method is invoked, we assume that
  // this "TrInfo" must NOT be empty, ie must be created by a Non-Default Ctor:
  //
  template<ExchC0T X>
  inline void TriArbC0<X>::TrInfo::CheckInvariants() const
  {
# ifndef NDEBUG
    //-----------------------------------------------------------------------//
    // Passive Leg:                                                          //
    //-----------------------------------------------------------------------//
    assert(0 <= m_passI && m_passI <= 2 && 0 <= m_passS && m_passS <= 1 &&
           m_passInstr  != nullptr      && IsPos(m_passQty)             &&
           IsPos(m_passPx));

    if (m_passAOS != nullptr)
    {
      // The Instr and Side must match:
      assert(m_passAOS->m_instr == m_passAOS->m_instr &&
             m_passAOS->m_isBuy == bool(m_passS));

      // Also check that all Req12s of that AOS are indeed Passive, and have
      // same PassQty (because we do not re-quote part-filled passive orders,
      // cancel them instead). XXX: There may be quite a lot of "Req12"s here
      // due to continuous re-quoting!
      assert(m_passAOS->m_frstReq != nullptr);
      for (Req12 const* req = m_passAOS->m_frstReq; req != nullptr;
           req = req->m_next)
      {
        QtyN rQty = req->GetQty<QT,QR>();
        assert(!(req->m_isAggr) && rQty == m_passQty);
      }
    }
    //-----------------------------------------------------------------------//
    // Aggressive Legs:                                                      //
    //-----------------------------------------------------------------------//
    for (int j = 0; j <= 2; ++j)
    {
      int            aggrI     = m_aggrIs       [j];
      SecDefD const* aggrInstr = m_aggrInstrs   [j];
      AOS     const* aggrAOS   = m_aggrLastAOSes[j];
      bool           isBuy     = bool(m_aggrSs  [j]);
      QtyN           aggrQty         (m_aggrQtys[j]);

      assert(0 <= aggrI && aggrI <= 2 && (j==0) == (aggrI    ==m_passI)     &&
             aggrInstr  != nullptr    && (j==0) == (aggrInstr==m_passInstr) &&
             IsPos(aggrQty)           && IsFinite  (m_aggrSrcPxs[j]));

      // There could be multiple AggrAOSes, traverse them all (if any):
      for ( AOS const* aAOS = aggrAOS; aAOS != nullptr;
           aAOS = aAOS->m_userData.Get<AOSExt>().m_prevAggrAOS)
      {
        // Again, the Instr and Side must match:
        assert(aAOS->m_instr == aggrInstr && aAOS->m_isBuy == isBuy);

        // Per each AggrAOS, there must be exactly one Aggressive Req12:
        Req12 const* req = aAOS->m_frstReq;
        assert(req  != nullptr && req->m_next == nullptr && req->m_isAggr);
      }
    }
    // Finally: Check that the 0th AggrLeg is same as Passive one, and AggrLegs
    // 1 and 2 are not mixed up (XXX: though we do not provide a detailed Asset
    // check here):
    assert(m_aggrInstrs   [1] != m_aggrInstrs   [2] &&
           m_aggrIs       [1] != m_aggrIs       [2] &&
           m_aggrIs       [1] != m_aggrIs       [2] &&
           m_aggrLastAOSes[1] != m_aggrLastAOSes[2]);
# endif
  }

  //=========================================================================//
  // "TrInfo::GetPassTrades":                                                //
  //=========================================================================//
  template<ExchC0T X>
  inline void TriArbC0<X>::TrInfo::GetPassTrades
    (std::vector<Trade const*>* o_trades) const
  {
    assert(o_trades != nullptr);
    o_trades->clear();
    if (m_passAOS == nullptr)
        return;    // Obviously, no Trades

    // Generic Case:
    for (Trade const* tr = m_passAOS->m_lastTrd; tr != nullptr; tr = tr->m_prev)
      o_trades->push_back(tr);

    // It's better to return the Trades in the chronological order, so:
    std::reverse(o_trades->begin(), o_trades->end());

    // Obviously, all Trades must refer to the PassInstr:
    DEBUG_ONLY
    (
      for (Trade const* tr: *o_trades)
        assert(tr != nullptr && tr->m_instr != nullptr &&
               tr->m_instr   == m_passInstr);
    )
    // All Done!
  }

  //=========================================================================//
  // "TrInfo::GetAggrTrades":                                                //
  //=========================================================================//
  // NB: "a_j" is AggrClosing (0) or AggrLegNo (1|2):
  //
  template<ExchC0T X>
  inline void TriArbC0<X>::TrInfo::GetAggrTrades
    (int a_j, std::vector<Trade const*>* o_trades) const
  {
    assert(0 <= a_j && a_j <= 2 && o_trades != nullptr);
    o_trades->clear();

    // Traverse all AggrAOSes (there could be more than 1). They again come in
    // the reverse order (and Trades per each AOS are reversed as well):
    for (AOS const* aAOS = m_aggrLastAOSes[a_j]; aAOS != nullptr;
        aAOS = aAOS->m_userData.Get<AOSExt>().m_prevAggrAOS)
      for (Trade const* tr = aAOS->m_lastTrd; tr != nullptr; tr = tr->m_prev)
        o_trades->push_back(tr);

    // Again, reverse the Trades so that they are returned in chronological
    // order:
    std::reverse(o_trades->begin(), o_trades->end());

    // And again, all Trades must refer to the corresp AggrInstr:
    DEBUG_ONLY
    (
      SecDefD const* aggrInstr = m_aggrInstrs[a_j];
      assert(aggrInstr != nullptr);
      for (Trade const* tr: *o_trades)
        assert(tr != nullptr && tr->m_instr != nullptr &&
               tr->m_instr   == aggrInstr);
    )
    // All Done!
  }

  //=========================================================================//
  // "TriArbC0" Non-Default Ctor:                                            //
  //=========================================================================//
  template<ExchC0T X>
  TriArbC0<X>::TriArbC0
  (
    EPollReactor  *              a_reactor,
    spdlog::logger*              a_logger,
    boost::property_tree::ptree* a_params   // Must be non-NULL
  )
  : //-----------------------------------------------------------------------//
    // Base Class Ctor:                                                      //
    //-----------------------------------------------------------------------//
    Strategy
    (
      "TriArbC0",
      a_reactor,
      a_logger,
      a_params->get<int>("Main.DebugLevel"),
      { SIGINT }
    ),
    //-----------------------------------------------------------------------//
    // Ccys and InstrNames:                                                  //
    //-----------------------------------------------------------------------//
    m_A (MkCcy(a_params->get<std::string>("Main.Ccy_A"))),
    m_B (MkCcy(a_params->get<std::string>("Main.Ccy_B"))),
    m_C (MkCcy(a_params->get<std::string>("Main.Ccy_C"))),

    m_instrNames
    {
      a_params->get<std::string>("Main.Instr_AB"),
      a_params->get<std::string>("Main.Instr_CB"),
      a_params->get<std::string>("Main.Instr_AC")
    },
    //-----------------------------------------------------------------------//
    // Infrastructure: SecDefsMgr, RiskMgr, Connectors:                      //
    //-----------------------------------------------------------------------//
    // SecDefsMgr:
    m_secDefsMgr(SecDefsMgr::GetPersistInstance
      (a_params->get<std::string>("Main.Env") == "Prod", "")),

    // RiskMgr:
    m_riskMgr(RiskMgr::GetPersistInstance
    (
      a_params->get<std::string>("Main.Env") == "Prod",
      GetParamsSubTree(*a_params, "RiskMgr"),
      *m_secDefsMgr,
      false,          // NOT Observer!
      m_logger,
      m_debugLevel
    )),

    // AssetRisks extracted from RiskMgr:
    m_assetRisks
    {
      &(m_riskMgr->GetAssetRisks(m_A.data(), 0, 0)),
      &(m_riskMgr->GetAssetRisks(m_B.data(), 0, 0)),
      &(m_riskMgr->GetAssetRisks(m_C.data(), 0, 0))
    },

    // MDC:
    m_mdc(new MDC
    (
      m_reactor,
      m_secDefsMgr,
      &m_instrNames,  // Only the requested Instrs
      m_riskMgr,
      SetAccountKey
        (a_params, "MDC",
         a_params->get<std::string>("MDC.AccountPfx") +
           ConnsC0<X>::MDCSuffix + a_params->get<std::string>("Main.Env")),
      nullptr         // No Secondary MDC
    )),

    // OMC:
    m_omc(new OMC
    (
      m_reactor,
      m_secDefsMgr,
      &m_instrNames,  // Only the requested Instrs
      m_riskMgr,
      SetAccountKey
        (a_params, "OMC",
         a_params->get<std::string>("OMC.AccountPfx") +
           ConnsC0<X>::OMCSuffix + a_params->get<std::string>("Main.Env"))
    )),
    //-----------------------------------------------------------------------//
    // Instruments (as "SecDefD"s) and OrderBooks:                           //
    //-----------------------------------------------------------------------//
    // NB: They can only be obtained after the MDC has been created:
    m_instrs
    {
      &(m_secDefsMgr->FindSecDef(m_instrNames[AB].data())),
      &(m_secDefsMgr->FindSecDef(m_instrNames[CB].data())),
      &(m_secDefsMgr->FindSecDef(m_instrNames[AC].data()))
    },

    m_orderBooks
    {
      &(m_mdc->GetOrderBook   (*(m_instrs[AB]))),
      &(m_mdc->GetOrderBook   (*(m_instrs[CB]))),
      &(m_mdc->GetOrderBook   (*(m_instrs[AC])))
    },
    //-----------------------------------------------------------------------//
    // Quantitative Params of the Strategy:                                  //
    //-----------------------------------------------------------------------//
    m_dryRun                  (a_params->get<bool>("Main.DryRun")),
    m_relMarkUps
    {
      a_params->get<double>   ("Main.RelMarkUp_AB"),
      a_params->get<double>   ("Main.RelMarkUp_CB"),
      a_params->get<double>   ("Main.RelMarkUp_AC")
    },

    // NB: The qtys specified in Params are integrals, converted to QtyN:
    m_qtys
    {
      QtyN(a_params->get<double>("Main.Qty_AB")),
      QtyN(a_params->get<double>("Main.Qty_CB")),
      QtyN(a_params->get<double>("Main.Qty_AC"))
    },

    // Qty Reserve Factors (normally > 1):
    m_resrvFactors
    {
      a_params->get<double>   ("Main.QtyReserveFactor_AB"),
      a_params->get<double>   ("Main.QtyReserveFactor_CB"),
      a_params->get<double>   ("Main.QtyReserveFactor_AC")
    },

    // Resistance vals (in PaxSteps) used to prevent too frequent re-quoting:
    m_resistDiffs1
    {
      a_params->get<int>      ("Main.ResistDiff1_AB"),
      a_params->get<int>      ("Main.ResistDiff1_CB"),
      a_params->get<int>      ("Main.ResistDiff1_AC")
    },
    m_resistDepths1
    {
      a_params->get<int>      ("Main.ResistDepth1_AB"),
      a_params->get<int>      ("Main.ResistDepth1_CB"),
      a_params->get<int>      ("Main.ResistDepth1_AC")
    },
    m_resistDiffs2
    {
      a_params->get<int>      ("Main.ResistDiff2_AB"),
      a_params->get<int>      ("Main.ResistDiff2_CB"),
      a_params->get<int>      ("Main.ResistDiff2_AC")
    },

    // Max #Triangles to be completed (must not exceed the capacity of "TrInfo"s
    // in ShM):
    m_maxRounds     (a_params->get<int>("Main.MaxRounds")),

    //-----------------------------------------------------------------------//
    // Run-Time Status:                                                      //
    //-----------------------------------------------------------------------//
    m_allConnsActive(false),  // Not yet, of course!
    m_isInDtor      (false),
    m_roundsCount   (0),
    m_signalsCount  (0),

    //-----------------------------------------------------------------------//
    // TriAngles:                                                            //
    //-----------------------------------------------------------------------//
    // We can now initialise the PersistMgr. This will create a ShM segment or
    // attach to an existing one:
    m_confMax3s     (a_params->get<unsigned>("Main.MaxTriAngles")),
    m_pm            (m_name, a_params, m_confMax3s * sizeof(TrInfo) + 65536,
                     m_debugLevel,     m_logger),

    // Obtain ptrs to the in-ShM Max3s and the currently-used N3s:
    m_Max3s         (m_pm.GetSegm()->find_or_construct<unsigned>("Max3s")
                    (m_confMax3s)),
    m_N3s           (m_pm.GetSegm()->find_or_construct<unsigned>("N3s")(0)),

    // Ptr to the actual array of TriAngles in ShM. NB: If the array does not
    // exists, it is created with the capacity taken from *m_Max3s; if exists,
    // its capacity is stored back in *m_Max3s:
    m_All3s         (m_pm.FindOrConstruct<TrInfo>("TriAngles", m_Max3s)),

    // Array of ptrs (in standard memory) to currently-active TriAngles:
    m_curr3s        { { nullptr, nullptr }, { nullptr, nullptr },
                      { nullptr, nullptr } }
  {
    //-----------------------------------------------------------------------//
    // Verify the params:                                                    //
    //-----------------------------------------------------------------------//
    // Make sure Reactor and Logger are non-NULL ("Strategy" itself does not
    // require that):
    if (utxx::unlikely(m_reactor == nullptr || m_logger == nullptr))
      throw utxx::badarg_error
            ("TriArbC0::Ctor: Reactor and Logger must be non-NULL");

    // MarkUps, Qtys, Depths and Diffs for all Instrs:
    for (int I = 0; I < 3; ++I)
    {
      // NB: Zero mark-ups make little sense from the trading point of view
      // (because we then lose money on TransCosts), but can be provided for
      // debugging purposes:
      if (utxx::unlikely
         (m_relMarkUps  [I] <  0.0 || !IsPos(m_qtys[I]) ||
          m_resrvFactors[I] <= 0.0))
        throw utxx::badarg_error
              ("TriArbC0::Ctor: Invalid Quoting MarkUp and/or Qty: Instr=", I);

      // Depths and Diffs must be valid:
      // "m_resistDiff*"  is the minimum px difference between the curr and new
      // quotes which actually cause re-quoting; so it must be >= 1 (px step);
      // "m_resistDepth1" is the max depth at which "m_resistDiff1" applies (at
      // higher depthes, "m_resistDiff2" applies), so it must be >= 1, because
      // at depthes <= 0, this logic does not apply at all:
      if (utxx::unlikely
         (m_resistDiffs1 [I] <= 0 || m_resistDiffs2[I] <= 0 ||
          m_resistDepths1[I] <= 0))
      throw utxx::badarg_error
            ("TriArbC0::Ctor: Invalid ResistDiff(s)/ResistDepth(s): Must be "
             ">= 1; Instr=", I);
    }
    // Check the ShM setup:
    if (utxx::unlikely
       (m_Max3s == nullptr || m_N3s   == nullptr || *m_Max3s == 0 ||
        *m_N3s  > *m_Max3s || m_All3s == nullptr))
      throw utxx::runtime_error("TriArbC0::Ctor: ShM setup error");

    //-----------------------------------------------------------------------//
    // Subscribe to receive basic TradingEvents from all Connectors:         //
    //-----------------------------------------------------------------------//
    // (XXX: Otherwise we would not even know that the Connectors have become
    // Active!):
    m_mdc->Subscribe(this);
    m_omc->Subscribe(this);

    // All Done:
    m_logger->flush();
  }

  //=========================================================================//
  // Dtor:                                                                   //
  //=========================================================================//
  template<ExchC0T X>
  inline   TriArbC0<X>::~TriArbC0() noexcept
  {
    // Do not allow any exceptions to propagate from this Dtor:
    try
    {
      m_isInDtor   = true;
      GracefulStop<true>();
      // XXX: Connectors are deliberaly NOT de-allocated at this point, which
      // may result in memory leak indications...
    }
    catch (...) {}
  }

  //=========================================================================//
  // "Run":                                                                  //
  //=========================================================================//
  template<ExchC0T X>
  inline void TriArbC0<X>::Run()
  {
    // Start the Connectors:
    m_mdc->Start();
    m_omc->Start();

    // Enter the Inifinite Event Loop (terminate on any unhandled exceptions):
    m_reactor->Run(true);
  }

  //=========================================================================//
  // "DelayedGracefulStop":                                                  //
  //=========================================================================//
  template<ExchC0T X>
  inline void TriArbC0<X>::DelayedGracefulStop
  (
    utxx::time_val  a_ts_exch,
    utxx::time_val  a_ts_conn,
    utxx::time_val  a_ts_strat
  )
  {
    // Record the time stamp when we initiated the Delayed Stop. When non-0, it
    // implies we are in the Stopping Mode already:
    if (utxx::unlikely(!m_delayedStopInited.empty() || m_nowStopInited))
      return;

    m_delayedStopInited = a_ts_strat;
    assert(!m_delayedStopInited.empty());

    LOG_INFO(1, "DelayedGracefulStop: Stop Initiated...")

    // Explicitly Cancel all Passive Quotes (they are first all buffered, then
    // flushed):
    for (int I = 0; I < 3; ++I)
    for (int S = 0; S < 2; ++S)
    {
      TrInfo*    tri  = m_curr3s[I][S];
      AOS const* pAOS = (tri != nullptr) ? tri->m_passAOS : nullptr;

      if (utxx::likely(pAOS != nullptr))
        (void) CancelOrderSafe
          (pAOS, a_ts_exch, a_ts_conn, a_ts_strat, "DelayedGracefulStop");
    }
    // Now flush the orders accumulated:
    (void) m_omc->FlushOrders();

    // XXX: We currently do not setup any firing timer to wait for expiration of
    // the GracefulStop interval (a few sec) -- this is simply done by  the Mkt-
    // Data Call-Back...
    m_logger->flush();
  }

  //=========================================================================//
  // "GracefulStop":                                                         //
  //=========================================================================//
  // NB: This is still a "SEMI-graceful" stop: more rapid than "DelayedGraceful-
  // Stop" above, but still NOT an emergency termination of the Reactor Loop by
  // throwing the "EPollReactor::ExitRun" exception:
  //
  template<ExchC0T X>
  template<bool FromDtor>
  inline void TriArbC0<X>::GracefulStop()
  {
    // Protect against multiple invocations -- can cause infinite recursion via
    // notifications sent by Connector's "Stop" methods:
    if (utxx::unlikely(m_nowStopInited))
      return;
    m_nowStopInited = true;

    // Stop the  Connectors:
    m_mdc->Stop();
    m_omc->Stop();

    if (!FromDtor)
      // Ouput the curr positions for all SettlDates:
      m_riskMgr->OutputAssetPositions("GracefulStop", 0);

    m_logger->flush();
  }

  //=========================================================================//
  // "CheckAllConnectors":                                                   //
  //=========================================================================//
  // Managing the switch to Steady Mode:
  //
  template<ExchC0T X>
  void TriArbC0<X>::CheckAllConnectors()
  {
    if (utxx::likely(m_allConnsActive))
      return;

    // Otherwise: The flag was not set yet, retest it now:
    m_allConnsActive = m_mdc->IsActive() && m_omc->IsActive();

    if (!m_allConnsActive)
      return;   // Not yet

    // Otherwise, we have just made a transition to the Active state. Make sure
    // that all OrderBooks have at least L1 Pxs available:
    //
    for (int I = 0; I < 3; ++I)
    {
      // NB: It might happen that the Connectors are "active" (hence  we have
      // got here), but the corresp TradingEvent(s) were not sent yet, so the
      // OrderBook(s) are not yet constructed:
      OrderBook const* ob = m_orderBooks[I];

      // Check the availability of L1 Pxs:
      m_allConnsActive =
        m_allConnsActive             && ob != nullptr              &&
        IsFinite(ob->GetBestBidPx()) && IsFinite(ob->GetBestAskPx());

      if (!m_allConnsActive)
      {
        // NB: Don't use "ob" here (it may be NULL), use the corresp SecDef
        // instead!
        SecDefD const* notReady = m_instrs[I];
        assert(notReady != nullptr);
        LOG_INFO(3,
          "CheckAllConnectors: Symbol={}, SettlDate={} (I={}): OrderBook not "
          "ready yet", notReady->m_Symbol.data(), notReady->m_SettlDate, I)
        return;
      }
    }
    // If we got here, the result is positive:
    assert(m_allConnsActive);

    // We can now start the RiskMgr:
    try   { m_riskMgr->Start(); }
    catch (std::exception const& exc)
    {
      LOG_CRIT(1,
        "CheckAllConnections: RiskMgr::Start Failed: {}, EXITING...",
        exc.what())
      m_logger ->flush();
      m_reactor->ExitImmediately("RiskMgr::Start Failed");
    }

    // OK: Produce a log msg:
    LOG_INFO(1, "ALL CONNECTORS ARE NOW ACTIVE, RiskMgr Started!")
    m_logger->flush();

    // Output the initial positions:
    m_riskMgr->OutputAssetPositions("CheckAllConnectors", 0);
  }
} // End namespace MAQUETTE
