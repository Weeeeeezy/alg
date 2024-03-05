// vim:ts=2:et
//===========================================================================//
//                 "TradingStrats/3Arb/TriArb2S/InitRun.hpp":                //
//                             Ctor, Dtor, Run                               //
//===========================================================================//
#pragma  once
#include "TradingStrats/3Arb/TriArb2S/TriArb2S.h"
#include "Basis/ConfigUtils.hpp"
#include "InfraStruct/SecDefsMgr.h"
#include "InfraStruct/RiskMgr.h"
#include <utxx/error.hpp>

namespace MAQUETTE
{
  //=========================================================================//
  // Non-Default Ctor:                                                       //
  //=========================================================================//
  TriArb2S::TriArb2S
  (
    EPollReactor  *              a_reactor,
    spdlog::logger*              a_logger,
    boost::property_tree::ptree* a_params   // XXX: Mutable!
  )
  : //-----------------------------------------------------------------------//
    // Base Class Ctor:                                                      //
    //-----------------------------------------------------------------------//
    Strategy
    (
      "TriArb2S",
      a_reactor,
      a_logger,
      a_params->get<int>("Main.DebugLevel"),
      { SIGINT }
    ),
    //-----------------------------------------------------------------------//
    // Ccys:                                                                 //
    //-----------------------------------------------------------------------//
    m_EUR     (MkCcy("EUR")),
    m_RUB     (MkCcy("RUB")),
    m_USD     (MkCcy("USD")),

    //-----------------------------------------------------------------------//
    // Infrastructure: Reactor, Logger, RiskMgr, Connectors:                 //
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
      false,            // NOT Observer!
      m_logger,
      m_debugLevel
    )),

    // [EConnector_FAST_MICEX_FX]     (MktData):
    // TODO: Support Secondary FAST MICEX Connector as well!
    m_mdcMICEX
    (
      (a_params->get<std::string>("Main.Env") == "Prod") // Non-Prods are Test!
        ? FAST::MICEX::EnvT::Prod1
        : FAST::MICEX::EnvT::TestL,
      m_reactor,
      m_secDefsMgr,
      nullptr,       // XXX: Instrs are configured via SettlDates in Params
      m_riskMgr,
      GetParamsSubTree(*a_params, "EConnector_FAST_MICEX_FX"),
      nullptr
    ),

    // [EConnector_FIX_AlfaFIX2_MKT]   (MktData):
    // AccountKey = {AccountPfx}-FIX-AlfaFIX2-{Env}MKT:
    m_mdcAlfa
    (
      m_reactor,
      m_secDefsMgr,
      nullptr,       // XXX: Instrs are configured via SettlDates in Params
      m_riskMgr,
      SetAccountKey
        (a_params, "EConnector_FIX_AlfaFIX2_MKT",
         a_params->get<std::string>("EConnector_FIX_AlfaFIX2_MKT.AccountPfx") +
           "-FIX-AlfaFIX2-" + a_params->get<std::string>("Main.Env") + "MKT"),
      nullptr
    ),

    // [EConnector_FIX_MICEX_FX]    (OrdMgmt):
    // 2 Connectors, for load balancing (ALFA01, ALFA02):
    m_omcMICEX0
    {
      m_reactor,
      m_secDefsMgr,
      nullptr,       // XXX: Instrs are configured via SettlDates in Params
      m_riskMgr,
      SetAccountKey
        (a_params, "EConnector_FIX_MICEX_FX",
         a_params->get<std::string>("EConnector_FIX_MICEX_FX.AccountPfx1") +
           "-FIX-MICEX-FX-" + // All non-Prod envs are Test
           ((a_params->get<std::string>("Main.Env") == "Prod")
            ? "Prod20" : "TestL1")),
      nullptr
    },
    m_omcMICEX1
    {
      m_reactor,
      m_secDefsMgr,
      nullptr,       // XXX: Instrs are configured via SettlDates in Params
      m_riskMgr,
      SetAccountKey
        (a_params, "EConnector_FIX_MICEX_FX",
         a_params->get<std::string>("EConnector_FIX_MICEX_FX.AccountPfx2") +
           "-FIX-MICEX-FX-" + // All non-Prod envs are Test
           ((a_params->get<std::string>("Main.Env") == "Prod")
           ? "Prod21" : "TestL1")),
      nullptr
    },

    // [EConnector_FIX_AlfaFIX2_ORD] (OrdMgmt):
    // AccountKey = {AccountPfx}-FIX-AlfaFIX2-{Env}ORD:
    m_omcAlfa
    (
      m_reactor,
      m_secDefsMgr,
      nullptr,       // XXX: Instrs are configured via SettlDates in Params
      m_riskMgr,
      SetAccountKey
        (a_params, "EConnector_FIX_AlfaFIX2_ORD",
         a_params->get<std::string>("EConnector_FIX_AlfaFIX2_ORD.AccountPfx")  +
           "-FIX-AlfaFIX2-" + a_params->get<std::string>("Main.Env") + "ORD"),
      nullptr
    ),

    // StatusLine File (XXX):
    m_statusFile(a_params->get<std::string>("Exec.StatusLine")),

    //-----------------------------------------------------------------------//
    // Instruments and OrderBooks:                                           //
    //-----------------------------------------------------------------------//
    // Get the SettlDates as Integers:
    // NB: due to national holidays, "TOM" may be different for EUR/RUB and
    // USD/RUB (but still both not exceeding SPOT), so get 2 resp vals here:
    m_TOD   (a_params->get<int>("Main.TOD")),
    m_TOM_ER(a_params->get<int>("Main.TOM_EUR/RUB")),
    m_TOM_UR(a_params->get<int>("Main.TOM_USD/RUB")),
    m_SPOT  (a_params->get<int>("Main.SPOT")),

    // Last quoting time (UTC) for TOD instrs on MICEX -- min of that for EUR
    // and USD, and giving allowance for manual swapping! The time is in UTC;
    // fortunately, MICEX has a fixed time-zone offset to UTC: 14:30:00 MSK =
    // 11:30:00 UTC (which is the typical value of this param):
    m_lastQuoteTimeTOD
      (TimeToTimeValFIX(a_params->get<std::string>
      ("Main.LastQuoteTimeTOD_UTC").data())),

    m_isFinishedQuotesTOD (GetCurrTime() >= m_lastQuoteTimeTOD),

    m_rollOverTimeEUR(TimeToTimeValFIX("11:59:00.000")),  // Before 15:00 MSK
    m_rollOverTimeRUB(TimeToTimeValFIX("14:14:00.000")),  // Before 17:15 MSK

    // Can get them now, once the Connectors have been created (but obviously,
    // no OrderBooks yet):
    // "Passive" Instrs are all from MICEX (use MktSegment=CETS):
    m_passInstrs
    {
      {
        &(m_secDefsMgr->FindSecDef("EUR_RUB__TOD-MICEX-CETS-TOD")), // TOD, AB
        &(m_secDefsMgr->FindSecDef("USD000000TOD-MICEX-CETS-TOD"))  // TOD, CB
      },
      {
        &(m_secDefsMgr->FindSecDef("EUR_RUB__TOM-MICEX-CETS-TOM")), // TOM, AB
        &(m_secDefsMgr->FindSecDef("USD000UTSTOM-MICEX-CETS-TOM"))  // TOM, CB
      }
    },

    // Now "Aggressive" AlfaFIX2 Instrs -- in this case, use SettlDates  to find
    // the corresp SecDefs. Only used if OptimalRouting is used:
    // XXX: In the past, Aggressive Instrs were using SettlDates as Segments,
    // but for now, because of Server-side issues,   we switched to symbolic
    // Tenors instead:
    m_useOptimalRouting(a_params->get<bool>("Main.UseOptimalRouting")),
    m_aggrInstrs
    {
      {
        m_useOptimalRouting
          ? &(m_secDefsMgr->FindSecDef("EUR/RUB-AlfaFIX2--TOD")) : nullptr,
        m_useOptimalRouting
          ? &(m_secDefsMgr->FindSecDef("USD/RUB-AlfaFIX2--TOD")) : nullptr
      },
      {
        m_useOptimalRouting
          ? &(m_secDefsMgr->FindSecDef("EUR/RUB-AlfaFIX2--TOM")) : nullptr,
        m_useOptimalRouting
          ? &(m_secDefsMgr->FindSecDef("USD/RUB-AlfaFIX2--TOM")) : nullptr
      }
    },
    m_AC   (&(m_secDefsMgr->FindSecDef("EUR/USD-AlfaFIX2--SPOT"))),

    // OrderBook ptrs:
    m_orderBooks
    {
      // Passive    OrderBooks (MICEX):    [0..3]:
      &(m_mdcMICEX.GetOrderBook(*(m_passInstrs[TOD][AB]))),
      &(m_mdcMICEX.GetOrderBook(*(m_passInstrs[TOD][CB]))),
      &(m_mdcMICEX.GetOrderBook(*(m_passInstrs[TOM][AB]))),
      &(m_mdcMICEX.GetOrderBook(*(m_passInstrs[TOM][CB]))),
      // Aggressive OrderBooks (AlfaFIX2): [4..7]:
      // NB: If OptimalRounting is not used, then those OrderBooks (except AC
      // below) are all NULL:
      m_useOptimalRouting
        ? &(m_mdcAlfa .GetOrderBook(*(m_aggrInstrs[TOD][AB]))) : nullptr,
      m_useOptimalRouting
        ? &(m_mdcAlfa .GetOrderBook(*(m_aggrInstrs[TOD][CB]))) : nullptr,
      m_useOptimalRouting
        ? &(m_mdcAlfa .GetOrderBook(*(m_aggrInstrs[TOM][AB]))) : nullptr,
      m_useOptimalRouting
        ? &(m_mdcAlfa .GetOrderBook(*(m_aggrInstrs[TOM][CB]))) : nullptr,
      // AC: Aggressive on AlfaFIX2:       [8]
      &(m_mdcAlfa .GetOrderBook(*m_AC))
    },

    // "AssetTotalRisks" used in "AdjustCcyTails" -- cannot be initialised as
    // yet, because no OrderBooks are registered yet with the RiskMgr:
    m_risksEUR_TOM (nullptr),
    m_risksRUB_TOM (nullptr),

    //-----------------------------------------------------------------------//
    // Quantitative Params:                                                  //
    //-----------------------------------------------------------------------//
    m_ACSwapPxs     { { a_params->get<double>("Main.SwapAsk_EUR/USD_TOD_SPOT"),
                        a_params->get<double>("Main.SwapBid_EUR/USD_TOD_SPOT")
                      },
                      { a_params->get<double>("Main.SwapAsk_EUR/USD_TOM_SPOT"),
                        a_params->get<double>("Main.SwapBid_EUR/USD_TOM_SPOT")
                    } },
    m_CBSwapPxs     {   a_params->get<double>("Main.SwapAsk_USD/RUB_TOD_TOM"),
                        a_params->get<double>("Main.SwapBid_USD/RUB_TOD_TOM")
                    },
    m_sprds         {   a_params->get<double>("Main.QuotingSpread_EUR/RUB"),
                        a_params->get<double>("Main.QuotingSpread_USD/RUB")
                    },
    m_qtys          {   QtyO(a_params->get<long>("Main.Qty_EUR/RUB")),
                        QtyO(a_params->get<long>("Main.Qty_USD/RUB"))
                    },
    m_reserveQtys   { MkReserveQty<AB>(*a_params),
                      MkReserveQty<CB>(*a_params)
                    },
    m_resistDiff1      (a_params->get<int>   ("Main.ResistDiff1" )),
    m_resistDepth1     (a_params->get<int>   ("Main.ResistDepth1")),
    m_resistDiff2      (a_params->get<int>   ("Main.ResistDiff2" )),
    m_manipDiscount    (a_params->get<double>("Main.ManipDiscount")),

    m_maxRounds        (a_params->get<int>   ("Main.MaxRounds"   )),

    // Tails Adjustment and its Throttler:
    m_adjustCcyTails   (a_params->get<bool>  ("Main.AdjustCcyTails")),
    m_tailsThrottler   (TailsThrottlerPeriodSec),

    //-----------------------------------------------------------------------//
    // Run-Time Status:                                                      //
    //-----------------------------------------------------------------------//
    m_allConnsActive   (false),  // Not yet, of course!
    m_isInDtor         (false),
    m_roundsCount      (0),
    m_signalsCount     (0),
    m_passAOSes        {},       // All NULLs
    m_prevBasePxs      (),       // NaNs
    m_prevL1Pxs        (),       // NaNs
    m_synthPxs         (),       // NaNs
    m_aggrAOSes        (),
    m_trInfos          ()
  {
    //-----------------------------------------------------------------------//
    // Verify the params:                                                    //
    //-----------------------------------------------------------------------//
    // Make sure Reactor and Logger are non-NULL ("Strategy" itself does not
    // require that):
    if (utxx::unlikely(m_reactor == nullptr || m_logger == nullptr))
      throw utxx::badarg_error
            ("TriArb2S::Ctor: Reactor and Logger must be non-NULL");

    // In the RiskMgr, the Risk-Free Ccy MUST be USD:
    if (utxx::unlikely(m_riskMgr->GetRFC() != MkCcy("USD")))
      throw utxx::badarg_error
            ("TriArb2S::Ctor: UnExpected Risk-Free Ccy: ",
             m_riskMgr->GetRFC().data());

    // NB: Zero spreads make little sense from the trading point of view  (be-
    // cause we then lose money on TransCosts), but can be provided for debug-
    // ing purposes:
    if (utxx::unlikely
       (m_sprds[AB] < 0    || m_sprds[CB] < 0   ||
        !IsPos(m_qtys[AB]) || !IsPos(m_qtys[CB])))
      throw utxx::badarg_error
            ("TriArb2S::Ctor: Invalid Quoting Spread(s) and/or Qty(s)");

    // Check that the Swap Rates have correct Bid-Ask Spreads:
    if (utxx::unlikely
       (m_ACSwapPxs[TOD][Bid] >= m_ACSwapPxs[TOD][Ask] ||
        m_ACSwapPxs[TOM][Bid] >= m_ACSwapPxs[TOM][Ask]))
      throw utxx::badarg_error
            ("TriArb2S::Ctor: Invalid SwapPxs for EUR/USD: "
             "Bid-Ask Collision");

    // Output the calculated "ReserveQtys"
    // (NB: we can still use "m_passInstrs") :
    // XXX: Use TOM as TOD may be unavailable!
    LOG_INFO(2,
      "TriArb2S::Ctor: ReserveQtys of AggrInstrs: {}: {}; {}: {}",
      m_passInstrs[TOM][AB]->m_Symbol.data(), QRO(m_reserveQtys[AB]),
      m_passInstrs[TOM][CB]->m_Symbol.data(), QRO(m_reserveQtys[CB]))

    // Pre-allocate a sufficiently large "m_trInfo"s vector:
    long maxTriangles =
      std::max<long>
      (a_params->get<long>("Main.MaxTriangles"), long(m_maxRounds));
    m_trInfos.reserve(size_t(maxTriangles));

    // Depths and Diffs must be valid:
    // "m_resistDiff*"  is the minimum px difference between the curr and new
    // quotes which actually cause re-quoting; so it must be >= 1 (px step);
    // "m_resistDepth1" is the max depth at which "m_resistDiff1" applies (at
    // higher depthes, "m_resistDiff2" applies), so it must be >= 1, because
    // at depthes <= 0, this logic does not apply at all:
    if (utxx::unlikely
       (m_resistDiff1 <= 0 || m_resistDiff2 <= 0 || m_resistDepth1 <= 0))
      throw utxx::badarg_error
            ("TriArb2S::Ctor: Invalid ResistDiff(s)/ResistDepth(s): Must be "
             ">= 1");

    // "ManipDiscount" must be in 0..1:
    if (utxx::unlikely(m_manipDiscount < 0.0 || m_manipDiscount > 1.0))
      throw utxx::badarg_error
            ("TriArb2S::Ctor: Invalid ManipDiscount=", m_manipDiscount,
             ": Must be in [0.0 .. 1.0]");

    // Verify the SettlDates:
    // NB: PassInstrs are always available (non-NULL), AggrInstrs will be NULL
    // if OptimalRouting is noty used:
    if (utxx::unlikely
       // TOD:
       ((m_passInstrs[TOD][AB]->m_SettlDate != m_TOD     ||
         m_passInstrs[TOD][CB]->m_SettlDate != m_TOD     ||

        (m_aggrInstrs[TOD][AB] != nullptr                &&
         m_aggrInstrs[TOD][AB]->m_SettlDate != m_TOD)    ||

        (m_aggrInstrs[TOD][CB] != nullptr                &&
         m_aggrInstrs[TOD][CB]->m_SettlDate != m_TOD)    ||

        // TOM:
         m_passInstrs[TOM][AB]->m_SettlDate != m_TOM_ER  ||
         m_passInstrs[TOM][CB]->m_SettlDate != m_TOM_UR  ||

        (m_aggrInstrs[TOM][AB] != nullptr                &&
         m_aggrInstrs[TOM][AB]->m_SettlDate != m_TOM_ER) ||

        (m_aggrInstrs[TOM][CB] != nullptr                &&
         m_aggrInstrs[TOM][CB]->m_SettlDate != m_TOM_UR)) ||

         m_TOD    >= m_TOM_ER  ||  m_TOD    >= m_TOM_UR ||
         m_TOM_ER >  m_SPOT    ||  m_TOM_UR >  m_SPOT))
      throw utxx::badarg_error("TriArb2S::Ctor: Inconsistent SettlDates");

    //-----------------------------------------------------------------------//
    // Subscribe to receive basic TradingEvents from all Connectors:         //
    //-----------------------------------------------------------------------//
    // (XXX: Otherwise we would not even know that the Connectors have become
    // Active!):
    m_mdcMICEX .Subscribe(this);
    m_mdcAlfa  .Subscribe(this);
    m_omcMICEX0.Subscribe(this);
    m_omcMICEX1.Subscribe(this);
    m_omcAlfa  .Subscribe(this);

    // All Done:
    m_logger->flush();
  }

  //=========================================================================//
  // Dtor:                                                                   //
  //=========================================================================//
  inline TriArb2S::~TriArb2S() noexcept
  {
    // Do not allow any exceptions to propagate from this Dtor:
    try
    {
      m_isInDtor   = true;
      GracefulStop<true>();
      m_secDefsMgr = nullptr;
      m_riskMgr    = nullptr;
    }
    catch (...) {}
  }

  //=========================================================================//
  // "Run":                                                                  //
  //=========================================================================//
  inline void TriArb2S::Run()
  {
    // Start the Connectors:
    m_mdcMICEX .Start();
    m_mdcAlfa  .Start();
    m_omcMICEX0.Start();
    m_omcMICEX1.Start();
    m_omcAlfa  .Start();

    // Enter the Inifinite Event Loop (terminate on any unhandled exceptions):
    m_reactor->Run(true);
  }

  //=========================================================================//
  // "MkReserveQty":                                                         //
  //=========================================================================//
  // Calculate the "ReserveQty" which will be used at run-time to determine the
  // AvgVolPx of instrument "I" (A/B or C/B) which will in turn be used to cal-
  // culate the synthetic quoting px of the other instrument (C/B or A/B, resp):
  //
  template<int A>
  inline TriArb2S::QtyO TriArb2S::MkReserveQty
    (boost::property_tree::ptree const& a_params) const
  {
    // "A" is Aggressive, P=(A^1) is Passive;
    // AggrQty = PassQty   * (PassPx / AggrPx) = m_qtys[P] * PxFrac,  where
    // PxFrac  = Px[P] /  Px[A]:
    // So:
    // (*) A==AB: P==CB, PxFrac = Px(CB)/Px(AB) = Px(CA) = 1/Px(AC),  where
    //     Px(AC) is say 1.1 .. 1.4 historically, so MaxPxFrac = 0.9;
    // (*) A==CB: P==AB, PxFrac = Px(AB)/Px(CB) = Px(AC) = 1.0 .. 1.4, so
    //     MaxPxFrac = 1.4:
    //
    constexpr int P            =  A ^ 1;
    constexpr double MaxPxFact = (A == AB) ? 0.9 : 1.4;

    // Take the Reserves into account. XXX: LiquidityReserveAmount must be in
    // Contracts!
    double  const reserveFact  =
      a_params.get<double>("Main.LiquidityReserveFactor");
    double const  reserveAmt   =
      a_params.get<double>("Main.LiquidityReserveAmount");

    // The result will also be rounded up to a Lot Multiple of A; we can get
    // it from "PassInstrs" (yes, Passive!); TOD or TOM does not matter: XXX:
    // But use TOM here because TOD may be unavailable!
    double const  aggrLotSz    = m_passInstrs[TOM][A]->m_LotSize;
    return
      QtyO(long
          (Ceil((double(m_qtys[P]) * MaxPxFact * reserveFact + reserveAmt) /
                 aggrLotSz) * aggrLotSz));
  }

  //=========================================================================//
  // "WriteStatusLine" (XXX: Searching for a better place, if any):          //
  //=========================================================================//
  inline void TriArb2S::WriteStatusLine(char const* a_msg) const
  {
    // Open the file, silently return on any errors:
    FILE* f = fopen(m_statusFile.data(), "w");
    if (f == nullptr)
      return;

    (void) fputs(a_msg, f);
    fclose(f);
  }

  //=========================================================================//
  // "DelayedGracefulStop":                                                  //
  //=========================================================================//
  inline void TriArb2S::DelayedGracefulStop
  (
    utxx::time_val  a_ts_exch,
    utxx::time_val  a_ts_recv,
    utxx::time_val  a_ts_strat
  )
  {
    // Record the time stamp when we initiated the Delayed Stop. When non-0, it
    // implies we are in the Stopping Mode already:
    if (utxx::unlikely(!m_delayedStopInited.empty() || m_nowStopInited))
      return;

    m_delayedStopInited = a_ts_strat;
    assert(!m_delayedStopInited.empty());

    LOG_INFO(1, "TriArb2S::DelayedGracefulStop: Stop Initiated...")

    // Explicitly Cancel all Passive Quotes (they are first all buffered, then
    // flushed):
    for (int T = 0; T < 2; ++T)
    for (int I = 0; I < 2; ++I)
    for (int S = 0; S < 2; ++S)
    {
      AOS const* pAOS = m_passAOSes[T][I][S];

      if (utxx::likely(pAOS != nullptr))
        // This must be a MICEX OMC:
        (void) CancelOrderSafe<true>        // Buffered!
          (GetOMCMX(pAOS), pAOS, a_ts_exch, a_ts_recv, a_ts_strat);
    }
    // Now flush the orders accumulated:
    (void) m_omcMICEX0.FlushOrders();
    (void) m_omcMICEX1.FlushOrders();

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
  template<bool FromDtor>
  inline void TriArb2S::GracefulStop()
  {
    // Protect against multiple invocations -- can cause infinite recursion via
    // notifications sent by Connector's "Stop" methods:
    if (utxx::unlikely(m_nowStopInited))
      return;
    m_nowStopInited = true;

    // Stop the  Connectors:
    m_mdcMICEX .Stop();
    m_mdcAlfa  .Stop();
    m_omcMICEX0.Stop();
    m_omcMICEX1.Stop();
    m_omcAlfa  .Stop();

    if (!FromDtor)
      // Ouput the curr positions for all SettlDates:
      m_riskMgr->OutputAssetPositions("TriArb2S::GracefulStop", 0);

    m_logger->flush();
  }

  //=========================================================================//
  // "GetOMCMX(AOS)":                                                        //
  //=========================================================================//
  // It is assumed that the given AOS refers to one of "m_omcMCEX" Connectors;
  // the method returs a ref to the actual Connector (or throws an exception if
  // it was in fact not a MICEX one). For use in "{Cancel|Modify}OrderSafe":
  //
  inline EConnector_FIX_MICEX& TriArb2S::GetOMCMX(AOS const* a_aos)
  {
    assert(a_aos != nullptr);

    // The ptr stored in the AOS is a generic "EConnector_OrdMgmt" one, and we
    // need a specific "EConnector_FIX_MICEX" ptr. For efficiency, we will NOT
    // use "dynamic_cast" -- will do comparisons instead:
    //
    EConnector_OrdMgmt const* omc = a_aos->m_omc;
    assert(omc != nullptr);

    if (omc == &m_omcMICEX0)
      return m_omcMICEX0;
    else
    if (omc == &m_omcMICEX1)
      return m_omcMICEX1;
    else
      throw utxx::badarg_error("TriArb2S::GetOMCMX(AOS): Not MICEX OMC");
  }

  //=========================================================================//
  // "GetOMCMX(LoadBalancing)":                                              //
  //=========================================================================//
  // For use with "NewOrderSafe":
  //
  inline EConnector_FIX_MICEX& TriArb2S::GetOMCMX(utxx::time_val a_ts)
  {
    assert(!a_ts.empty());
    int     s = int(a_ts.microseconds() & 1L);
    assert (s == 0 || s == 1);

    return (s == 0) ? m_omcMICEX0 : m_omcMICEX1;
  }

  //=========================================================================//
  // "RefreshThrottlers":                                                    //
  //=========================================================================//
  // To be invoked as frequently as reasonably possible:
  //
  inline void TriArb2S::RefreshThrottlers(utxx::time_val a_ts) const
  {
    if (!a_ts.empty())
      m_tailsThrottler.refresh (a_ts);
  }

  //=========================================================================//
  // "IsStopping":                                                           //
  //=========================================================================//
  inline bool TriArb2S::IsStopping() const
  {
    return m_nowStopInited              ||
           !m_delayedStopInited.empty() ||
           m_riskMgr->GetMode() == RMModeT::Safe;
  }
} // End namespace MAQUETTE
