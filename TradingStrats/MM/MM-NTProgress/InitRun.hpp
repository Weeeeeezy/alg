// vim:ts=2:et
//===========================================================================//
//                  "TradingStrats/MM-NTProgress/InitRun.hpp":               //
//                             Ctor, Dtor, Run                               //
//===========================================================================//
#pragma  once

#include "MM-NTProgress.h"
#include "Basis/ConfigUtils.hpp"
#include "InfraStruct/SecDefsMgr.h"
#include "InfraStruct/RiskMgr.h"
#include <utxx/error.hpp>
#include <boost/algorithm/string.hpp>
#include <utility>
#include <cstdlib>
#include <cstring>

namespace MAQUETTE
{
  //=========================================================================//
  // Non-Default Ctor:                                                       //
  //=========================================================================//
  MM_NTProgress::MM_NTProgress
  (
    EPollReactor*                a_reactor,
    spdlog::logger*              a_logger,
    boost::property_tree::ptree& a_params   // XXX: Mutable!
  )
  : //-----------------------------------------------------------------------//
    // Base Class Ctor:                                                      //
    //-----------------------------------------------------------------------//
    Strategy
    (
      "MM-NTProgress",
      a_reactor,
      a_logger,
      a_params.get<int> ("Main.DebugLevel"),
      { SIGINT }
    ),
    //-----------------------------------------------------------------------//
    // Ccys:                                                                 //
    //-----------------------------------------------------------------------//
    m_EUR   (MkCcy("EUR")),
    m_RUB   (MkCcy("RUB")),
    m_USD   (MkCcy("USD")),

    //-----------------------------------------------------------------------//
    // Infrastructure: Reactor, Logger, RiskMgr, Connectors:                 //
    //-----------------------------------------------------------------------//
    // Dry-Run Mode?
    m_dryRunMode(a_params.get<bool>("Main.DryRunMode")),

    // SecDefsMgr:
    m_secDefsMgr(SecDefsMgr::GetPersistInstance
      (a_params.get<std::string>("Main.Env") == "Prod", "")),

    // RiskMgr:
    m_riskMgr(RiskMgr::GetPersistInstance
    (
      a_params.get<std::string>("Main.Env") == "Prod",
      GetParamsSubTree         (a_params,  "RiskMgr"),
      *m_secDefsMgr,
      false,                   // NOT Observer!
      m_logger,
      m_debugLevel
    )),

    // (1) MDC MICEX: [EConnector_FAST_MICEX_FX]
    m_mdcMICEX
    (
      FAST::MICEX::EnvT::Prod1,
      m_reactor,
      m_secDefsMgr,
      nullptr,        // XXX: Instrs are configured via Params
      m_riskMgr,
      GetParamsSubTree(a_params, "EConnector_FAST_MICEX_FX"),
      nullptr
    ),

    // (2) OMC NTProgress:  [EConnector_FIX_NTProgress_ORD]
    //     AccountKey    = {AccountPfx}-FIX-NTProgress-{Prod|Test}ORD:
    m_omcNTPro_AccountPfx
      (a_params.get<std::string>("EConnector_FIX_NTProgress_ORD.AccountPfx")),
    m_omcNTPro
    (
      m_reactor,
      m_secDefsMgr,
      nullptr,        // XXX: Instrs are configured via Params
      m_riskMgr,
      SetAccountKey
      (
        &a_params,
        "EConnector_FIX_NTProgress_ORD",
        (a_params.get<std::string>("Main.Env") == "Prod")
          ? (m_omcNTPro_AccountPfx + "-FIX-NTProgress-ProdORD")
          : (m_omcNTPro_AccountPfx + "-FIX-NTProgress-TestORD")
      ),
      nullptr
    ),

    // (3) OMC MICEX: [EConnector_FIX_MICEX_FX]
    // AccountKey  = {AccountPfx}-FIX-MICEX-FX-{Prod30|TestL1}:
    //
    m_omcMICEX_AccountPfx
      (a_params.get<std::string>("EConnector_FIX_MICEX_FX.AccountPfx")),
    m_omcMICEX
    (
      m_reactor,
      m_secDefsMgr,
      nullptr,        // XXX: Instrs are configured via Params
      m_riskMgr,
      SetAccountKey
      (
        &a_params,
        "EConnector_FIX_MICEX_FX",
        (a_params.get<std::string>("Main.Env") == "Prod")
          ? (m_omcMICEX_AccountPfx + "-FIX-MICEX-FX-Prod30")
          : (m_omcMICEX_AccountPfx + "-FIX-MICEX-FX-TestL1")
      ),
      nullptr
    ),
    //-----------------------------------------------------------------------//
    // Instruments and OrderBooks:                                           //
    //-----------------------------------------------------------------------//
    // Get the SettlDates as Integers:
    // NB: due to national holidays, "TOM" may be different for EUR/RUB and
    // USD/RUB, so get 2 resp vals here:
    m_TOD                   (a_params.get<int>("Main.TOD")),
    m_TOM_AB                (a_params.get<int>("Main.TOM_EUR/RUB")),
    m_TOM_CB                (a_params.get<int>("Main.TOM_USD/RUB")),

    // Fixed Exchange Rates (for very rough conversion only):
    m_fixedRUB_USD(1.0 / a_params.get<double>("RiskMgr.USD/RUB_Rough")),
    m_fixedEUR_USD(      a_params.get<double>("RiskMgr.EUR/USD_Rough")),

    // Last quoting time (UTC) for TOD instrs (FIX-type format by default):
    m_lastQuoteTimeTOD
    {
      TimeToTimeValFIX(a_params.get<std::string>
                      ("Main.EUR/RUB_LastQuoteTime_UTC").data()),
      TimeToTimeValFIX(a_params.get<std::string>
                      ("Main.USD/RUB_LastQuoteTime_UTC").data())
    },
    m_startTime    (GetCurrTime()),

    // An Instrument is enabled IFF it is configured as such, and (for TODs),
    // the curr time still allows us to quote it (the latter will be re-checked
    // at run-time as well):
    m_isInstrEnabled
    { { // TOD Instrs may be available for a part of the day only:
        a_params.get<bool>("Main.EUR/RUB_TOD_Enabled") &&
          m_startTime < m_lastQuoteTimeTOD[AB],

        a_params.get<bool>("Main.USD/RUB_TOD_Enabled") &&
          m_startTime < m_lastQuoteTimeTOD[CB],

        // EUR/USD_TOD is enabled iff both EUR/RUB and USD/RUB are enabled:
        m_isInstrEnabled[TOD][AB] && m_isInstrEnabled[TOD][CB]
      },
      { // TOM Instrs, if enabled, have no intra-day time limits:
        a_params.get<bool>("Main.EUR/RUB_TOM_Enabled"),
        a_params.get<bool>("Main.USD/RUB_TOM_Enabled"),
        // Similar to TOD above:
        m_isInstrEnabled[TOM][AB] && m_isInstrEnabled[TOM][CB]
    } },

    // NTProgress Instrs: For Quoting only. NB: Some TOD instrs may be unavail-
    // able, this is OK; TOM instrs should always be available:
    m_instrsNTPro
    { { // TOD, AB:
        m_isInstrEnabled[TOD][AB]
        ? &(m_secDefsMgr->FindSecDef("EUR/RUB_TOD-NTProgress--TOD"))
        : nullptr,
        // TOD, CB:
        m_isInstrEnabled[TOD][CB]
        ? &(m_secDefsMgr->FindSecDef("USD/RUB_TOD-NTProgress--TOD"))
        : nullptr
      },
      { // TOM, AB:
        m_isInstrEnabled[TOM][AB]
        ? &(m_secDefsMgr->FindSecDef("EUR/RUB_TOM-NTProgress--TOM"))
        : nullptr,
        // TOM, CB:
        m_isInstrEnabled[TOM][CB]
        ? &(m_secDefsMgr->FindSecDef("USD/RUB_TOM-NTProgress--TOM"))
        : nullptr
    } },

    // NTProgress AOSes: Initialised to NULL below:
    m_aosesNTPro (),

    // Curr QuotePxs: Initialised to NaN by default:
    m_currQPxs   (),

    // Prev Best Pxs on MICEX: Also initialised to NaN:
    m_prevBestPxs(),

    // MICEX Instrs: Main src of pricing info:
    m_instrsMICEX
    { { // TOD, AB:
        m_isInstrEnabled[TOD][AB]
        ? &(m_secDefsMgr->FindSecDef("EUR_RUB__TOD-MICEX-CETS-TOD"))
        : nullptr,
        // TOD, CB:
        m_isInstrEnabled[TOD][CB]
        ? &(m_secDefsMgr->FindSecDef("USD000000TOD-MICEX-CETS-TOD"))
        : nullptr,
        // TOD, AC: Available iff both AB, CB (TOD) are available:
        m_isInstrEnabled[TOD][AC]
        ? &(m_secDefsMgr->FindSecDef("EURUSD000TOD-MICEX-CETS-TOD:"))
        : nullptr
      },
      { // TOM, AB:
        m_isInstrEnabled[TOM][AB]
        ? &(m_secDefsMgr->FindSecDef("EUR_RUB__TOM-MICEX-CETS-TOM"))
        : nullptr,
        // TOM, CB:
        m_isInstrEnabled[TOM][CB]
        ? &(m_secDefsMgr->FindSecDef("USD000UTSTOM-MICEX-CETS-TOM"))
        : nullptr,
        // TOM, AC: Available iff both AB, CB (TOM) are available:
        m_isInstrEnabled[TOM][AC]
        ? &(m_secDefsMgr->FindSecDef("EURUSD000TOM-MICEX-CETS-TOM"))
        : nullptr
      }
    },

    // MICEX OrderBooks (Main Px Srcs):
    m_obsMICEX
    { { // TOD, AB:
        m_isInstrEnabled[TOD][AB]
        ? &(m_mdcMICEX.GetOrderBook(*(m_instrsMICEX[TOD][AB])))
        : nullptr,
        // TOD, CB:
        m_isInstrEnabled[TOD][CB]
        ? &(m_mdcMICEX.GetOrderBook(*(m_instrsMICEX[TOD][CB])))
        : nullptr,
        // TOD, AC:
        m_isInstrEnabled[TOD][AC]
        ? &(m_mdcMICEX.GetOrderBook(*(m_instrsMICEX[TOD][AC])))
        : nullptr
      },
      { // TOM, AB:
        m_isInstrEnabled[TOM][AB]
        ? &(m_mdcMICEX.GetOrderBook(*(m_instrsMICEX[TOM][AB])))
        : nullptr,
        // TOM, CB:
        m_isInstrEnabled[TOM][CB]
        ? &(m_mdcMICEX.GetOrderBook(*(m_instrsMICEX[TOM][CB])))
        : nullptr,
        // TOM, AC:
        m_isInstrEnabled[TOM][AC]
        ? &(m_mdcMICEX.GetOrderBook(*(m_instrsMICEX[TOM][AC])))
        : nullptr
    } },

    // AOSes for Pegged MICEX Covering Orders: Initially all NULL:
    m_aosesPegMICEX
    {
      { { nullptr, nullptr }, { nullptr, nullptr } },
      { { nullptr, nullptr }, { nullptr, nullptr } }
    },

    // Cancel-Pending AOSes (initially empty):
    m_cpAOSes          (),

    // Signed Qtys of Covering Orders (on MICEX) currently "on-the-fly":
    m_flyingCovDeltas  { { QtyO(), QtyO() }, { QtyO(), QtyO() } },

    //-----------------------------------------------------------------------//
    // Quantitative and Other Params:                                        //
    //-----------------------------------------------------------------------//
    m_swapRates        { { 0.0, 0.0 }, { 0.0, 0.0 }, { 0.0, 0.0 } },
    m_nBands           { { 0, 0 }, { 0, 0 } },       // Initialised below
    m_qtys             (),                           // ditto
    m_markUps          (),                           // ditto

    // Convexity params for Quotes Skewing:
    m_betas
    {
      a_params.get<double>("Main.Beta_EUR/RUB"),
      a_params.get<double>("Main.Beta_USD/RUB")
    },

    m_maxQImprs
    {
      a_params.get<double>("Main.MaxQuoteImprovement_EUR/RUB"),
      a_params.get<double>("Main.MaxQuoteImprovement_USD/RUB"),
    },

    // InstrRisks ptrs will be taken when the RiskMgr is initialised:
    m_irsNTPro         { { nullptr, nullptr }, { nullptr, nullptr } },
    m_irsMICEX         { { nullptr, nullptr }, { nullptr, nullptr } },

    // Position Limits:
    m_posLimits
    { { // TOD, AB:
        m_isInstrEnabled[TOD][AB]
        ? QtyO(a_params.get<long>("Main.PosLimit_EUR/RUB_TOD"))
        : QtyO(),
        // TOD, CB:
        m_isInstrEnabled[TOD][CB]
        ? QtyO(a_params.get<long>("Main.PosLimit_USD/RUB_TOD"))
        : QtyO(),
      },
      { // TOM, AB:
        m_isInstrEnabled[TOM][AB]
        ? QtyO(a_params.get<long>("Main.PosLimit_EUR/RUB_TOM"))
        : QtyO(),
        // TOM, CB:
        m_isInstrEnabled[TOM][CB]
        ? QtyO(a_params.get<long>("Main.PosLimit_USD/RUB_TOM"))
        : QtyO()
    } },

    m_coverWholePos    (a_params.get<bool>  ("Main.CoverWholePos")),
    m_skewBothSides    (a_params.get<bool>  ("Main.SkewBothSides")),
    m_useSymmetricBands(a_params.get<bool>  ("Main.UseSymmetricBands")),

    m_usePegging
    { { a_params.get<bool>("Main.UsePegging_EUR/RUB_TOD"),
        a_params.get<bool>("Main.UsePegging_USD/RUB_TOD")
      },
      { a_params.get<bool>("Main.UsePegging_EUR/RUB_TOM"),
        a_params.get<bool>("Main.UsePegging_USD/RUB_TOM")
    } },

    // Misc:
    m_qPxRanges        (),       // Initialised below
    m_qPxResists       (),       // Initialised below

    m_manipRedCoeff    (a_params.get<double>("Main.ManipRedCoeff")),
    m_manipRedOnlyL1   (a_params.get<bool>  ("Main.ManipRedOnlyL1")),

    m_minInterQuote    (a_params.get<int>   ("Main.MinQuoteInterval_msec")),
    m_maxInterQuote    (a_params.get<int>   ("Main.MaxQuoteInterval_msec")),
    m_lastQuoteTimes   {},
    m_timerFD          (-1),     // Not yet
    m_maxRounds        (a_params.get<int>   ("Exec.MaxRounds")),
    m_maxReqsPerSec    (a_params.get<double>("Exec.MaxReqsPerSec_SoftLimit")),

    m_statusLineFile   (a_params.get<std::string>("Exec.StatusLine")),

    //-----------------------------------------------------------------------//
    // Run-Time Status:                                                      //
    //-----------------------------------------------------------------------//
    m_allConnsActive   (false),  // Not yet, of course!
    m_isInDtor         (false),
    m_roundsCount      (0),
    m_signalsCount     (0)
  {
    //-----------------------------------------------------------------------//
    // Extra Checks:                                                         //
    //-----------------------------------------------------------------------//
    // Make sure Reactor and Logger are non-NULL ("Strategy" itself does not
    // require that):
    if (utxx::unlikely(m_reactor == nullptr || m_logger == nullptr))
      throw utxx::badarg_error
            ("MM-NTProgress::Ctor: Reactor and Logger must be non-NULL");

    //-----------------------------------------------------------------------//
    // Quoting Params:                                                       //
    //-----------------------------------------------------------------------//
    // "ManipRedCoeff" must be within [0..1]:
    if (utxx::unlikely(m_manipRedCoeff  < 0.0 || m_manipRedCoeff  > 1.0))
      throw utxx::badarg_error
            ("MM-NTProgress::Ctor: ManipRedCoeff must be in [0..1]");

    // Set up the Quote Px Limits (to detect abnormalities):
    // AB = EUR/RUB:
    double  er = m_fixedEUR_USD / m_fixedRUB_USD;
    m_qPxRanges[AB][0] = PriceT(0.8 * er);
    m_qPxRanges[AB][1] = PriceT(1.2 * er);

    // CB = USD/RUB:
    double  ur = 1.0 / m_fixedRUB_USD;
    m_qPxRanges[CB][0] = PriceT(0.8 * ur);
    m_qPxRanges[CB][1] = PriceT(1.2 * ur);

    //-----------------------------------------------------------------------//
    // FX Swap Rates:                                                        //
    //-----------------------------------------------------------------------//
    for (int I = 0; I < 3; ++I)
    {
      std::vector<std::string> swrVec;
      std::string swrKey =
        std::string("Main.SwapRates_") +
        ((I == 0) ? "EUR/RUB" : (I == 1) ? "USD/RUB" : "EUR/USD") + "_TOD_TOM";

      std::string  swrStr = a_params.get<std::string>(swrKey);
      boost::split(swrVec,  swrStr, boost::is_any_of(";,"));

      // There must be exatly 2 vals -- Bid and Ask:
      if (utxx::unlikely(swrVec.size() != 2))
        throw utxx::badarg_error
              ("MM-NTProgress::Ctor: Invalid ", swrKey, '=', swrStr,
               " must be: BidRate,AskRate");

      m_swapRates[I][Bid] = atof(swrVec[0].data());
      m_swapRates[I][Ask] = atof(swrVec[1].data());

      if (utxx::unlikely
         (!(m_swapRates[I][Bid] <= m_swapRates[I][Ask]))) // Inc NaNs
        throw utxx::badarg_error
              ("MM-NTProgress::Ctor: Invalid ", swrKey,   '=',  swrStr,
               ": Invalid val(s): ", m_swapRates[I][Bid], ", ",
               m_swapRates[I][Ask]);
    }
    //-----------------------------------------------------------------------//
    // Parse and install the Quote Qtys, Mark-Ups, Px Resistances, etc:      //
    //-----------------------------------------------------------------------//
    for (int I = 0; I < 2; ++I)
    {
      // Check the Betas:
      if (utxx::unlikely(m_betas[I] <= 0.0))
        throw utxx::badarg_error
              ("MM-NTProgress::Ctor: I=", I, ": Invalid Beta=", m_betas[I],
               ": Must be > 0");

      for (int T = 0; T < 2; ++T)
      {
        //-------------------------------------------------------------------//
        // Quote Qtys:                                                       //
        //-------------------------------------------------------------------//
        std::vector<std::string> qVec;
        std::string bandsKey =
          std::string("Main.Qtys_")   + ((I == 0) ? "EUR" : "USD") +
          "/RUB_" + ((T == 0) ? "TOD" : "TOM");

        std::string  qStr = a_params.get<std::string>(bandsKey);
        boost::split(qVec,  qStr, boost::is_any_of(";,"));

        // The number of Bands requested:
        m_nBands[T][I] = qStr.empty() ? 0 : int(qVec.size());

        // Set the Quotes Qtys for all Bands:
        for (int B = 0; B < m_nBands[T][I]; ++B)
        {
          QtyO   qty(atol(qVec[size_t(B)].data()));
          if (utxx::unlikely(!IsPos(qty)))
            throw utxx::badarg_error(bandsKey, ": Invalid Qty=", QRO(qty));
          m_qtys[T][I][B] = qty;
        }

        //-------------------------------------------------------------------//
        // Mark-Ups:                                                         //
        //-------------------------------------------------------------------//
        std::vector<std::string> mVec;
        std::string markUpsKey =
          std::string("Main.MarkUps_") + ((I == 0) ? "EUR" : "USD") +
          "/RUB_" + ((T == 0) ? "TOD"  : "TOM");

        std::string  mStr = a_params.get<std::string>(markUpsKey);
        boost::split(mVec,  mStr, boost::is_any_of(";,"));

        // The number of Mark-Ups must be same as the number of Bands:
        if (utxx::unlikely(int(mVec.size()) != m_nBands[T][I]))
          throw utxx::badarg_error
                (markUpsKey, ": Invalid Number of Mark-Ups: ", mVec.size(),
                 ", must be", m_nBands[T][I]);

        // All Mark-Ups must be semi-monotonic (NB: negative vals are allowed,
        // but not recommended):
        double prevMarkUp = NaN<double>;
        for (int B = 0; B < m_nBands[T][I]; ++B)
        {
          double markUp = atof(mVec[size_t(B)].data());
          if (utxx::unlikely(markUp < prevMarkUp))
            throw utxx::badarg_error
                  (markUpsKey, ": Band=", B, ": Non-Monotonic Mark-Up: ",
                   markUp,     ", Prev=", prevMarkUp);
          prevMarkUp         = markUp;
          m_markUps[T][I][B] = markUp;
        }
      }
      //---------------------------------------------------------------------//
      // Quote Px Resistances:                                               //
      //---------------------------------------------------------------------//
      // XXX: The Resistances are currently same for both Tenors,
      // so must be as long as the longest number of Bands for that Instr:
      std::vector<std::string> rVec;
      std::string resistKey =
        std::string("Main.Resistances_") + ((I == 0) ? "EUR" : "USD") +
        "/RUB";
      std::string  rStr = a_params.get<std::string>(resistKey);
      boost::split(rVec,  rStr, boost::is_any_of(";,"));

      int maxBands = std::max<int>(m_nBands[TOD][I], m_nBands[TOM][I]);
      if (utxx::unlikely(int(rVec.size()) != maxBands))
        throw utxx::badarg_error
              ("MM-NTProgress::Ctor: Invalid ResistancesVec for ",
               resistKey, ": Length must be ", maxBands, ", got ", rVec.size());

      // Set the Px Resistances for all Bands:
      for (int B = 0; B < maxBands; ++B)
      {
        double res = atof(rVec[size_t(B)].data());
        if (utxx::unlikely(res < 0.0))
          throw utxx::badarg_error("Invalid Resistance=", res);
        m_qPxResists[I][B] = res;
      }

      // All AOS ptrs set to NULL initially, and Curr QuotePxs to NaN:
      // LastQuoteTimes are set to the curr time (leaving them empty may not be
      // a good idea -- can get stuck in empty state):
      utxx::time_val now = utxx::now_utc();

      for (int T = 0;   T < 2;              ++T)
      {
        for (int B = 0; B < m_nBands[T][I]; ++B)
        for (int S = 0; S < 2;              ++S)
        {
          m_aosesNTPro      [T][I][S][B] = nullptr;
          m_lastQuoteTimes  [T][I][S][B] = now;
          assert(!IsFinite(m_currQPxs[T][I][S][B])); // Must be NaN
        }

        // Also verify the Position Limits: they must all be non-negative:
        if (utxx::unlikely(m_posLimits[T][I] < 0))
          throw utxx::badarg_error
                ("Invalid PosLimit for T=", T, ", I=", I, ": ",
                 QRO(m_posLimits[T][I]));
      }
    }
    // End of "I" loop

    //-----------------------------------------------------------------------//
    // Verify the SettlDates and Bands:                                      //
    //-----------------------------------------------------------------------//
    // NB: TOD SecDefs may sometimes be absent (eg in case of US or European
    // public holidays); TOD SecDefs are always present,  through the actual
    // SettlDate may of course vary:
    //
    if (utxx::unlikely(m_TOD >= m_TOM_AB || m_TOD >= m_TOM_CB))
      throw utxx::badarg_error
            ("MM-NTProgress::Ctor: Inconsistent SettlDates: TOD=", m_TOD,
             ", TOM_EUR/RUB=",  m_TOM_AB, ", TOM_USD/RUB=",     m_TOM_CB);

    for (int T = 0; T < 2; ++T)
    for (int I = 0; I < 2; ++I) // No AC here!
    {
      if (utxx::unlikely(!m_isInstrEnabled[T][I]))
        continue;

      // Generic Case:
      SecDefD const* instrNTPro = m_instrsNTPro[T][I];
      SecDefD const* instrMICEX = m_instrsMICEX[T][I];
      assert(instrNTPro != nullptr && instrMICEX != nullptr);

      // Now check the SettlDates:
      if (utxx::unlikely
         ((T == TOD &&
          (instrNTPro->m_SettlDate != m_TOD      ||
           instrMICEX->m_SettlDate != m_TOD))    ||
          (T == TOM && I == AB &&
          (instrNTPro->m_SettlDate != m_TOM_AB   ||
           instrMICEX->m_SettlDate != m_TOM_AB)) ||
          (T == TOM && I == CB &&
          (instrNTPro->m_SettlDate != m_TOM_CB   ||
           instrMICEX->m_SettlDate != m_TOM_CB)) ))
        throw utxx::badarg_error
              ("MM-NTProgress::Ctor: Invalid SettlDate(s) for ",
               (I == AB ) ? "EUR/RUB" : "USD/RUB",
               (T == TOM) ? " TOD"    : " TOM");

      // If the Instrument is enabled, it MUST have Bands configured:
      if (utxx::unlikely(m_nBands[T][I] <= 0))
        throw utxx::badarg_error
          ("MM-NTProgress::Ctor: No Bands configured for ",
           (I == AB ) ? "EUR/RUB" : "USD/RUB",
           (T == TOM) ? " TOD"    : " TOM");
    }

    // Also verify the Quote Intervals:
    if (utxx::unlikely
       (m_minInterQuote < 0 || m_minInterQuote >= m_maxInterQuote))
      throw utxx::badarg_error
            ("MM-NTProgress::Ctor: Invalid Quote Interval(s): Min=",
             m_minInterQuote, ", Max=", m_maxInterQuote);

    //-----------------------------------------------------------------------//
    // Subscribe to receive basic TradingEvents from all Connectors:         //
    //-----------------------------------------------------------------------//
    // (XXX: Otherwise we would not even know that the Connectors have become
    // Active!):
    m_mdcMICEX.Subscribe(this);
    m_omcNTPro.Subscribe(this);
    m_omcMICEX.Subscribe(this);

    //-----------------------------------------------------------------------//
    // Produce a summary of instruments to be used:                          //
    //-----------------------------------------------------------------------//
    LOG_INFO(2,
      "MM-NTProgress::Ctor: StartTime={} sec, LastQuoteTime_TOD_EUR/RUB={} "
      "sec, LastQuoteTime_TOD_USD/RUB={} sec",
      m_startTime.seconds(), m_lastQuoteTimeTOD[AB].seconds(),
      m_lastQuoteTimeTOD[CB].seconds())

    for (int T = 0; T < 2; ++T)
    for (int I = 0; I < 2; ++I)
      LOG_INFO(2,
        "MM-NTProgress::Ctor: {}/RUB_{}: {}",
        (I == 0) ? "EUR" : "USD", (T == 0)  ? "TOD" : "TOM",
        m_isInstrEnabled[T][I]  ? "ENABLED" : "Disabled")

    // All Done:
    m_logger->flush();
  }

  //=========================================================================//
  // Dtor:                                                                   //
  //=========================================================================//
  inline MM_NTProgress::~MM_NTProgress() noexcept
  {
    // Do not allow any exceptions to propagate from this Dtor:
    try
    {
      m_isInDtor = true;
      SemiGracefulStop();
    }
    catch (...) {}
  }

  //=========================================================================//
  // "Run":                                                                  //
  //=========================================================================//
  inline void MM_NTProgress::Run()
  {
    // Start the Connectors:
    // NB: MDC_NTProgress is not much used as yet, but still needs to be Start-
    // ed, at least to Register its Instrs with the RiskMgr:
    m_mdcMICEX.Start();
    m_omcNTPro.Start();
    m_omcMICEX.Start();

    // Enter the Inifinite Event Loop (terminate on any unhandled exceptions):
    m_reactor->Run(true);
  }

  //=========================================================================//
  // "DelayedGracefulStop":                                                  //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "Out": Trivial and Non-Trivial Cases:                                   //
  //-------------------------------------------------------------------------//
  inline void MM_NTProgress::DelayedGracefulStop::Out()
    { m_msg << ": EXITING..."; }

  template<typename T, typename... Args>
  inline void MM_NTProgress::DelayedGracefulStop::Out
    (T const& a_t, Args&&... a_args)
  {
    m_msg << a_t;
    this->Out(std::forward<Args>(a_args)...);
  }

  //-------------------------------------------------------------------------//
  // Non-Default Ctor:                                                       //
  //-------------------------------------------------------------------------//
  template<typename... Args>
  inline MM_NTProgress::DelayedGracefulStop::DelayedGracefulStop
  (
    MM_NTProgress*  a_outer,
    utxx::time_val  a_ts_strat,
    Args&&...       a_args
  )
  : m_outer(a_outer),
    m_msg()
  {
    assert (m_outer != nullptr);

    // Record the time stamp when we initiated the Delayed or Now-Stop. If so,
    // we are now in the Stopping Mode -- nothing more to do:
    if (utxx::unlikely(m_outer->IsStopping()))
      return;

    m_outer->m_delayedStopInited = a_ts_strat;
    assert(!m_outer->m_delayedStopInited.empty() && m_outer->IsStopping());

    // Form the msg:
    Out(a_args...);
    std::string msg  = m_msg.str();

    // Log it:
    m_outer->m_logger->critical (msg);
    m_outer->m_logger->flush();

    // Save the msg in the StatusLine:
    if (!m_outer->m_statusLineFile.empty())
    {
      FILE* f = fopen(m_outer->m_statusLineFile.data(), "w");
      (void) fwrite(msg.data(), 1, msg.size(), f);
      (void) fclose(f);
    }

    // XXX: Do NOT issue any Cancellations etc yet, because we may have exceeded
    // the MaxMsgRate for the OMC Connector -- so do not exacerbate this condit-
    // ion!..
  }

  //=========================================================================//
  // "SemiGracefulStop":                                                     //
  //=========================================================================//
  // NB: This is still a "SEMI-graceful" stop: more rapid than "DelayedGraceful-
  // Stop" above, but still NOT an emergency termination of the Reactor Loop by
  // throwing the "EPollReactor::ExitRun" exception:
  //
  inline void MM_NTProgress::SemiGracefulStop()
  {
    // Protect against multiple invocations -- can cause infinite recursion via
    // notifications sent by Connector's "Stop" methods:
    if (utxx::unlikely(m_nowStopInited))
      return;
    m_nowStopInited = true;

    // Stop the Connectors:
    m_mdcMICEX.Stop();
    m_omcNTPro.Stop();
    m_omcMICEX.Stop();

    // Detach and close the Periodic TimerFD:
    if (m_timerFD >= 0)
    {
      m_reactor->Remove(m_timerFD);
      m_timerFD = -1;
    }

    // Output the curr positions for all SettlDates:
    if (!m_isInDtor)
      m_riskMgr->OutputAssetPositions("MM-NTProgress::SemiGracefulStop", 0);

    m_logger->flush();
  }

  //=========================================================================//
  // "CancelAllOrders":                                                      //
  //=========================================================================//
  // Cancelling Quotes and (if exist) Pegged Orders:
  //
  template<bool WithPegged>
  inline void MM_NTProgress::CancelAllOrders
  (
    utxx::time_val  a_ts_exch,
    utxx::time_val  a_ts_recv,
    utxx::time_val  a_ts_strat
  )
  {
    for (int T = 0; T < 2; ++T)
    for (int I = 0; I < 2; ++I)
    for (int S = 0; S < 2; ++S)
    {
      // Cancel the Quotes on NTProgress in all Bands (will automatically check
      // for NULLs / CxlPending):
      for (int B = 0; B < m_nBands[T][I]; ++B)
        CancelQuoteSafe <true>  // Buffered!
          (T, I, S, B, a_ts_exch, a_ts_recv, a_ts_strat);

      if (WithPegged)
        // Cancel the corresp Pegged Order on MICEX (if available) -- when the
        // cancellation is confirmed, they will be replaced by the corresp Mkt
        // Orders:
        CancelPeggedSafe<true>  // Buffered!
          (T, I, S,    a_ts_exch, a_ts_recv, a_ts_strat);
    }
    // Flushes:
    (void) m_omcNTPro.FlushOrders();

    if (WithPegged)
      (void) m_omcMICEX.FlushOrders();

    m_logger->flush();
  }
} // End namespace MAQUETTE
