// vim:ts=2:et
//===========================================================================//
//                   "TradingStrats/Pairs-MF/InitRun.hpp":                   //
//                              Ctor, Dtor, Run                              //
//===========================================================================//
#pragma once

#include "Basis/ConfigUtils.hpp"
#include "Basis/TimeValUtils.hpp"
#include "Pairs-MF.h"
#include "InfraStruct/SecDefsMgr.h"
#include "InfraStruct/RiskMgr.h"
#include "Basis/Maths.hpp"
#include <cstring>

namespace
{
  using namespace MAQUETTE;

  //=========================================================================//
  // Utils:                                                                  //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "GetEnabledUntil":                                                      //
  //-------------------------------------------------------------------------//
  inline utxx::time_val GetEnabledUntil
    (boost::property_tree::ptree const&  a_params)
  {
    std::string mskStr = a_params.get<std::string>("EnabledUntil_MSK");
    if (utxx::unlikely(mskStr.size() != 5 || mskStr[2] != ':'))
      return utxx::time_val();

    // Otherwise: Get HH and MM:
    mskStr[2] = '\0';
    int hh = atoi(mskStr.data());
    int mm = atoi(mskStr.data() + 3);
    if (utxx::unlikely(hh < 0 || mm < 0 || hh > 23 || mm > 59))
      return utxx::time_val();

    // Compute the time limit in UTC = MSK-3:
    return GetCurrDate() + utxx::secs((hh-3) * 3600 + mm * 60);
  }
}

namespace MAQUETTE
{
  //=========================================================================//
  // "IPair" Default Ctor:                                                   //
  //=========================================================================//
  inline Pairs_MF::IPair::IPair()
  : // The Strategy itself:
    m_outer            (nullptr),
    // Logging:
    m_logger           (nullptr),
    m_debugLevel       (0),
    m_enabledUntil     (),
    // Passive Leg:
    m_passMDC          (nullptr),
    m_passOMC          (nullptr),
    m_passInstr        (nullptr),
    m_passOB           (nullptr),
    m_passQty          (0),
    m_passMode         (PassModeT::Normal),
    m_passAOSes        { nullptr, nullptr },
    m_passPos          (0),
    m_passPosSoftLimit (0),
    m_passLastFillTS   (),
    m_reQuoteDelayMSec (0),
    // Spread Base:
    m_sprdEMACoeffs    { NaN<double>, NaN<double> },
    m_sprdEMA          ( NaN<double> ),       // NB: This is OK!
    m_swapMDC          (nullptr),
    m_swapInstr        (nullptr),
    m_swapOB           (nullptr),
    m_swapQtyFact      ( NaN<double> ),
    // Aggressive Leg:
    m_aggrMDC          (nullptr),
    m_aggrOMC          (nullptr),
    m_aggrInstr        (nullptr),
    m_aggrOB           (nullptr),
    m_aggrQtyFact      ( NaN<double> ),
    m_aggrQtyReserve   ( NaN<double> ),
    m_aggrNomQty       (0),
    m_aggrMode         (AggrModeT::DeepAggr), // Reasonable default!
    m_aggrPos          (0),
    // Quant Params:
    m_adjCoeffs        { NaN<double>, NaN<double> },
    m_quotePxs         (), // NaNs
    m_expCoverPxs      (), // NaNs
    m_targAdjMode      (TargAdjModeT::None),
    m_resistCoeff      ( NaN<double> ),
    m_extraMarkUp      (0.0),
    m_aggrStopLoss     (0.0),
    m_nSprdPeriods     (0),
    m_currSprdTicks    (0),
    m_dzLotsFrom       (0),
    m_dzLotsTo         (0),
    m_dzAggrQty        (0),
    m_useFlipFlop      (false),
    // Large vector at the end:
    m_aggrAOSes        (),
    m_lastAggrPosAdj   ()
  {}

  //=========================================================================//
  // "IPair" Non-Default Ctor:                                               //
  //=========================================================================//
  inline Pairs_MF::IPair::IPair
  (
    Pairs_MF*                    a_outer,
    boost::property_tree::ptree& a_params // XXX: Mutable!
  )
  : // The Strategy itself:
    m_outer            (a_outer),
    // Logging:
    m_logger           (a_outer->m_logger),
    m_debugLevel       (a_outer->m_debugLevel),
    // Until what time (UTC) this IPair is enabled?
    m_enabledUntil     (GetEnabledUntil(a_params)),
    // Passive Leg:
    m_passMDC          (a_outer->GetMDC(a_params.get<std::string>("PassMDC"))),
    m_passOMC          (a_outer->GetOMC(a_params.get<std::string>("PassOMC"))),
    m_passInstr        (&(a_outer->m_secDefsMgr->FindSecDef
                         (a_params.get<std::string>("PassInstr").data()))),
    m_passOB           (&(m_passMDC->GetOrderBook  (*m_passInstr))),
    m_passQty          (a_params.get<long>         ("PassQty")),
    m_passMode         (PassModeT::from_string
                       (a_params.get<std::string>  ("PassMode"))),
    m_passAOSes        {nullptr, nullptr},
    m_passPos          (a_params.get<long>         ("InitPassPos", 0)),
    m_passPosSoftLimit (a_params.get<long>("PassPosSoftLimit")),
    m_passLastFillTS   (),
    m_reQuoteDelayMSec (a_params.get<int> ("ReQuoteDelayMSec")),
    // Spread Base:
    m_sprdEMACoeffs    {a_params.get<double>("SprdEMACoeff"),
                        1.0 - m_sprdEMACoeffs[0]            },
    m_sprdEMA          (NaN<double>),
    m_swapMDC          ((a_params.get<std::string>("SwapMDC", "") != "")
                        ? a_outer->GetMDC(a_params.get<std::string>("SwapMDC"))
                        : nullptr),
    m_swapInstr        (a_outer->m_secDefsMgr->FindSecDefOpt
                         (a_params.get<std::string>("SwapInstr", "").data())),
                         // (SwapInstr string may be empty => SecDefD* == NULL)
    m_swapOB           ((m_swapMDC != nullptr && m_swapInstr != nullptr)
                        ? &(m_swapMDC->GetOrderBook(*m_swapInstr))
                        : nullptr),
    m_swapQtyFact      ((m_swapMDC != nullptr && m_swapInstr != nullptr)
                        ? a_params.get<double>("SwapQtyFactor")
                        : NaN<double>),
    // Aggressive Leg:
    m_aggrMDC          (a_outer->GetMDC(a_params.get<std::string>("AggrMDC"))),
    m_aggrOMC          (a_outer->GetOMC(a_params.get<std::string>("AggrOMC"))),
    m_aggrInstr        (&(a_outer->m_secDefsMgr->FindSecDef
                         (a_params.get<std::string>("AggrInstr" ).data())   )),
    m_aggrOB           (&(m_aggrMDC->GetOrderBook  (*m_aggrInstr))),
    m_aggrQtyFact      (a_params.get<double>       ("AggrQtyFactor" )),
    m_aggrQtyReserve   (a_params.get<double>       ("AggrQtyReserve")),
    m_aggrNomQty       (long(Round(double(m_passQty) * m_aggrQtyFact *
                                   m_aggrQtyReserve))),
    m_aggrMode         (AggrModeT::from_string
                       (a_params.get<std::string>  ("AggrMode") )),
    m_aggrPos          (a_params.get<long>         ("InitAggrPos", 0)),
    // Quant Params:
    m_adjCoeffs        {a_params.get<double>("AdjCoeff0"),
                        a_params.get<double>("AdjCoeff1")},
    m_quotePxs         (), // NaNs
    m_expCoverPxs      (), // NaNs
    m_targAdjMode      (TargAdjModeT::from_string
                         (a_params.get<std::string>("TargAdjMode"))),
    m_resistCoeff      (a_params.get<double>("ResistCoeff" )),
    m_extraMarkUp      (a_params.get<double>("ExtraMarkUp" )),
    m_aggrStopLoss     (a_params.get<double>("AggrStopLoss")),
    m_nSprdPeriods     (int(Ceil(2.0 / m_sprdEMACoeffs[0])) - 1),
    m_currSprdTicks    (0),
    m_dzLotsFrom       (a_params.get<long>  ("DeadZoneLotsFrom")),
    m_dzLotsTo         (a_params.get<long>  ("DeadZoneLotsTo")),
    m_dzAggrQty        (long(Round(double(m_dzLotsFrom)   *
                                   m_passInstr->m_LotSize * m_aggrQtyFact))),
    m_useFlipFlop      (a_params.get<bool>  ("UseFlipFlop")),
    // Finally, large vector at the end (for better cache performance):
    m_aggrAOSes        (),
    m_lastAggrPosAdj   ()
  {
    // Verify the EMA Coeffs: NB: emaCoeffs[0] == 1 is theoretically OK, which
    // means that the history is completely disregarded:
    if (utxx::unlikely
       (m_sprdEMACoeffs [0] <= 0.0 || m_sprdEMACoeffs [0] > 1.0))
      throw utxx::badarg_error
            ("Pairs-MF::IPair::Ctor: Invalid SprdEMACoeff0=",
             m_sprdEMACoeffs[0]);
    assert(m_nSprdPeriods >= 1);

    // Check the signs of other params:
    if (utxx::unlikely(m_resistCoeff < 0.0 || m_aggrStopLoss >= 0.0))
      throw utxx::badarg_error
            ("Pairs-MF::IPair::Ctor: Invalid ResistCoeff=", m_resistCoeff,
             " and/or AggrStopLoss=", m_aggrStopLoss);

    // Verify the Qtys:
    if (utxx::unlikely
       (m_passQty        <= 0   || m_aggrQtyFact      <= 0.0 ||
        m_aggrQtyReserve <= 0.0 || m_passPosSoftLimit <= 0   ||
        m_dzLotsFrom     <  0   || m_swapQtyFact      == 0.0 || // NaN is OK!
        m_aggrNomQty     <= 0))
      throw utxx::badarg_error
            ("Pairs-MF::IPair::Ctor: Invalid Qty(s) or Limit(s)");

    // XXX: If Flip-Flop mode is used, AggrNomQty needs to be multiplied by 2;
    // but in any case, it is INDICATIVE only:
    if (m_useFlipFlop)
      m_aggrNomQty *= 2;

    // Verify the AggrMode:
    if (utxx::unlikely(m_aggrMode == AggrModeT::UNDEFINED))
      throw utxx::badarg_error
            ("Pairs-MF::IPair::Ctor: Invalid AggrMode=",
             a_params.get<std::string>("AggrMode"));
  }

  //=========================================================================//
  // "IPair" Equality:                                                       //
  //=========================================================================//
  // XXX: It is probably sufficient to compare only the Instruments (by ptr):
  //
  inline bool Pairs_MF::IPair::operator==(Pairs_MF::IPair const& right) const
  {
    return this->m_passInstr == right.m_passInstr &&
           this->m_aggrInstr == right.m_aggrInstr;
  }

  //=========================================================================//
  // "Pairs_MF::Get{MDC,OMC}":                                               //
  //=========================================================================//
  // These methods either throw an exception, or return a non-NULL ptr:
  //
  inline EConnector_MktData* Pairs_MF::GetMDC
         (std::string const& a_conn_section) const
  {
    char const* cs = a_conn_section.data();
    return
      (strcmp(cs,  "EConnector_FAST_MICEX_EQ")      == 0)
      ? static_cast<EConnector_MktData*>(m_mdcMICEX_EQ) :
      (strcmp(cs,  "EConnector_FAST_MICEX_FX")      == 0)
      ? static_cast<EConnector_MktData*>(m_mdcMICEX_FX) :
      (strcmp(cs,  "EConnector_FAST_FORTS")         == 0)
      ? static_cast<EConnector_MktData*>(m_mdcFORTS)    :
      (strcmp(cs,  "EConnector_FIX_AlfaFIX2_MKT")   == 0)
      ? static_cast<EConnector_MktData*>(m_mdcAlfa)     :
      (strcmp(cs,  "EConnector_FIX_NTProgress_MKT") == 0)
      ? static_cast<EConnector_MktData*>(m_mdcNTPro)    :
      (strcmp(cs,  "EConnector_ITCH_HotSpotFX_Gen") == 0)
      ? static_cast<EConnector_MktData*>(m_mdcHSFX_Gen) :
      (strcmp(cs,  "EConnector_ITCH_CCMAlpha")      == 0)
      ? static_cast<EConnector_MktData*>(m_mdcCCMA)     :
      throw utxx::badarg_error
            ("Pairs_MF::GetMDC: Invalid ConnSection=", a_conn_section);
  }

  inline EConnector_OrdMgmt* Pairs_MF::GetOMC
         (std::string const& a_conn_section) const
  {
    char const* cs = a_conn_section.data();
    return
      (strcmp(cs,  "EConnector_FIX_MICEX_EQ")       == 0)
      ? static_cast<EConnector_OrdMgmt*>(m_omcMICEX_EQ) :
      (strcmp(cs,  "EConnector_FIX_MICEX_FX")       == 0)
      ? static_cast<EConnector_OrdMgmt*>(m_omcMICEX_FX) :
      (strcmp(cs,  "EConnector_TWIME_FORTS")        == 0)
      ? static_cast<EConnector_OrdMgmt*>(m_omcFORTS)    :
      (strcmp(cs,  "EConnector_FIX_AlfaFIX2_ORD")   == 0)
      ? static_cast<EConnector_OrdMgmt*>(m_omcAlfa)     :
      (strcmp(cs,  "EConnector_FIX_NTProgress_ORD") == 0)
      ? static_cast<EConnector_OrdMgmt*>(m_omcNTPro)    :
      (strcmp(cs,  "EConnector_FIX_HotSpotFX_Gen")  == 0)
      ? static_cast<EConnector_OrdMgmt*>(m_omcHSFX_Gen) :
      (strcmp(cs,  "EConnector_FIX_CCMAlpha")       == 0)
      ? static_cast<EConnector_OrdMgmt*>(m_omcCCMA)     :
      throw utxx::badarg_error
            ("Pairs_MF::GetOMC: Invalid ConnSection=", a_conn_section);
  }

  //=========================================================================//
  // "Pairs_MF": Non-Default Ctor:                                           //
  //=========================================================================//
  inline Pairs_MF::Pairs_MF
  (
    EPollReactor  *              a_reactor,
    spdlog::logger*              a_logger,
    boost::property_tree::ptree& a_params   // XXX: Mutable!
  )
  : //-----------------------------------------------------------------------//
    // Declarative Initialisations:                                          //
    //-----------------------------------------------------------------------//
    // Base Strategy Ctor:
    Strategy
    (
      "Pairs-MF",
      a_reactor,
      a_logger,
      a_params.get<int>("Main.DebugLevel"),
      { SIGINT }
    ),
    // DryRun?
    m_dryRun    (a_params.get<bool>("Main.DryRun")),

    // SecDefsMgr:
    m_secDefsMgr(SecDefsMgr::GetPersistInstance
      (a_params.get<std::string>("Main.Env") == "Prod", "")),

    // RiskMgr:
    m_riskMgr
    (a_params.get<bool>("RiskMgr.IsEnabled")
     ? RiskMgr::GetPersistInstance
       (
         a_params.get<std::string> ("Main.Env") == "Prod",
         GetParamsSubTree          (a_params,  "RiskMgr"),
         *m_secDefsMgr,
         false,                    // NOT Observer!
         m_logger,
         m_debugLevel
       )
     : nullptr
    ),

    // MktData Logger (optional):
    m_mktDataLoggerShP
    (
      !a_params.get<std::string>   ("Exec.MktDataLogFile").empty()
      ? IO::MkLogger
        (
          "MktData",
          a_params.get<std::string>("Exec.MktDataLogFile")
        )
      : nullptr
    ),
    m_mktDataLogger     (m_mktDataLoggerShP.get()),
    m_mktDataDepth      (a_params.get<int>("Exec.MktDataDepth")),
    m_mktDataBuff       (),

    // Connectors (MDCs and OMCs): Initially all NULLs, later assembled by
    // scanning the "Pairs" Sections:
    m_mdcMICEX_EQ       (nullptr),
    m_mdcMICEX_FX       (nullptr),
    m_mdcFORTS          (nullptr),
    m_mdcAlfa           (nullptr),
    m_mdcNTPro          (nullptr),
    m_mdcHSFX_Gen       (nullptr),
    m_mdcCCMA           (nullptr),

    m_omcMICEX_EQ       (nullptr),
    m_omcMICEX_FX       (nullptr),
    m_omcFORTS          (nullptr),
    m_omcAlfa           (nullptr),
    m_omcNTPro          (nullptr),
    m_omcHSFX_Gen       (nullptr),
    m_omcCCMA           (nullptr),

    // Infrastructure:
    m_statusLineFile    (a_params.get<std::string>("Exec.StatusLine")),
    m_maxRounds         (a_params.get<int> ("Main.MaxRounds")),
    m_roundsCount       (0),
    m_allConnsActive    (false),
    m_signalsCount      (0),

    // Trading schedule -- the worst case of FORTS gaps (as on IMM dates); MIECX
    // is always continuous. All times are in UTC:
    m_tradingSched
    {
      { TimeToTimeValFIX("07:00:00"), TimeToTimeValFIX("11:00:00") },
      { TimeToTimeValFIX("11:05:00"), TimeToTimeValFIX("15:45:00") },
      { TimeToTimeValFIX("16:05:00"), TimeToTimeValFIX("20:50:00") }
    },

    // "IPair"s -- initially empty, assembled below. Same for Asset->USD conv
    // rates:
    m_iPairs            (),
    m_fixedRates        ()
  {
    //-----------------------------------------------------------------------//
    // Initialise the Connectors and "IPair"s:                               //
    //-----------------------------------------------------------------------//
    // Make sure Reactor and Logger are non-NULL ("Strategy" itself does not
    // require that):
    if (utxx::unlikely(m_reactor == nullptr || m_logger == nullptr))
      throw utxx::badarg_error
            ("Pairs-MF::Ctor: Reactor and Logger must be non-NULL");

    // Go through all "Pair*" sections of the Config:
    bool isProd =
      (strcmp(a_params.get<std::string>("Main.Env").data(), "Prod") == 0);

    for (auto& sect: a_params)
      if (strncmp(sect.first.data(), "Pair", 4) == 0)
      {
        // This is an "Pair{N}" section:
        auto& ps = sect.second;

        // Get the names of all Connectors: [MDC,OMC][Pass,Aggr]:
        std::string cns[2][2]
        {
          { ps.get<std::string>("PassMDC"), ps.get<std::string>("AggrMDC") },
          { ps.get<std::string>("PassOMC"), ps.get<std::string>("AggrOMC") }
        };

        //-------------------------------------------------------------------//
        // Pass and Aggr MDCs for this Pair:                                 //
        //-------------------------------------------------------------------//
        for (int j = 0; j < 2; ++j)
        {
          std::string const& mdcName = cns[0][j];

          //-----------------------------------------------------------------//
          // FAST_MICEX_EQ:                                                  //
          //-----------------------------------------------------------------//
          if (mdcName == "EConnector_FAST_MICEX_EQ")
          {
            if (m_mdcMICEX_EQ == nullptr)
              m_mdcMICEX_EQ = new EConnector_FAST_MICEX_EQ
              (
                isProd ? FAST::MICEX::EnvT::Prod1 : FAST::MICEX::EnvT::TestL,
                m_reactor,
                m_secDefsMgr,
                nullptr,    // XXX: Instrs are configured via Params
                m_riskMgr,
                GetParamsSubTree(a_params, mdcName),
                nullptr     // TODO: Secondary MDC!
              );
          }
          else
          //-----------------------------------------------------------------//
          // FAST_MICEX_FX:                                                  //
          //-----------------------------------------------------------------//
          if (mdcName == "EConnector_FAST_MICEX_FX")
          {
            if (m_mdcMICEX_FX == nullptr)
              m_mdcMICEX_FX = new EConnector_FAST_MICEX_FX
              (
                isProd ? FAST::MICEX::EnvT::Prod1 : FAST::MICEX::EnvT::TestL,
                m_reactor,
                m_secDefsMgr,
                nullptr,    // XXX: Instrs are configured via Params
                m_riskMgr,
                GetParamsSubTree(a_params, mdcName),
                nullptr     // TODO: Secondary MDC!
              );
          }
          else
          //-----------------------------------------------------------------//
          // FAST_FORTS:                                                     //
          //-----------------------------------------------------------------//
          if (mdcName == "EConnector_FAST_FORTS")
          {
            if (m_mdcFORTS == nullptr)
              m_mdcFORTS = new EConnector_FAST_FORTS_FOL
              (
                isProd ? FAST::FORTS::EnvT::ProdF : FAST::FORTS::EnvT::TestL,
                m_reactor,
                m_secDefsMgr,
                nullptr,    // XXX: Instrs are configured via Params
                m_riskMgr,
                GetParamsSubTree(a_params, mdcName),
                nullptr     // TODO: Secondary MDC!
              );
          }
          else
          //-----------------------------------------------------------------//
          // AlfaFIX2 (MKT):                                                 //
          //-----------------------------------------------------------------//
          if (mdcName == "EConnector_FIX_AlfaFIX2_MKT")
          {
            if (m_mdcAlfa == nullptr)
            {
              auto const& mdcParams = GetParamsSubTree(a_params, mdcName);
              std::string acctKey   =
                mdcParams.get<std::string>("AccountPfx") + "-FIX-AlfaFIX2-" +
                (isProd ? "ProdMKT" : "TestMKT"); // XXX: TestMKT not working!

              SetAccountKey(&mdcParams, acctKey);
              m_mdcAlfa =
                // XXX: Instrs are configured via Params:
                new EConnector_FIX_AlfaFIX2
                    (m_reactor, m_secDefsMgr, nullptr, m_riskMgr, mdcParams,
                     nullptr);
            }
          }
          else
          //-----------------------------------------------------------------//
          // NTProgress (MKT):                                               //
          //-----------------------------------------------------------------//
          if (mdcName == "EConnector_FIX_NTProgress_MKT")
          {
            if (m_mdcNTPro == nullptr)
            {
              auto const& mdcParams = GetParamsSubTree(a_params, mdcName);
              std::string acctKey   =
                mdcParams.get<std::string>("AccountPfx") + "-FIX-NTProgress-" +
                (isProd ? "ProdMKT" : "TestMKT");

              SetAccountKey(&mdcParams, acctKey);
              m_mdcNTPro =
                // XXX: Instrs are configured via Params:
                new EConnector_FIX_NTProgress
                    (m_reactor, m_secDefsMgr, nullptr, m_riskMgr, mdcParams,
                     nullptr);
            }
          }
          else
          //-----------------------------------------------------------------//
          // Generic HotSpotFX:                                              //
          //-----------------------------------------------------------------//
          if (mdcName == "EConnector_ITCH_HotSpotFX_Gen")
          {
            if (m_mdcHSFX_Gen == nullptr)
            {
              auto const& mdcParams = GetParamsSubTree(a_params, mdcName);
              std::string acctKey   =
                mdcParams.get<std::string>("AccountPfx") +
                "-TCP_ITCH-HotSpotFX-" + (isProd ? "Prod" : "Test");

              SetAccountKey(&mdcParams, acctKey);
              m_mdcHSFX_Gen =
                // XXX: Instrs are configured via Params:
                new EConnector_ITCH_HotSpotFX_Gen
                    (m_reactor,  m_secDefsMgr, nullptr, m_riskMgr, mdcParams,
                     nullptr);
            }
          }
          else
          //-----------------------------------------------------------------//
          // CCMAlpha (over HotSpotFX Link):                                 //
          //-----------------------------------------------------------------//
          if (mdcName == "EConnector_ITCH_CCMAlpha")
          {
            if (m_mdcCCMA == nullptr)
            {
              auto const& mdcParams = GetParamsSubTree(a_params, mdcName);
              std::string acctKey   =
                mdcParams.get<std::string>("AccountPfx") +
                "-TCP_ITCH-HotSpotFX-" + (isProd ? "Prod" : "Test");

              SetAccountKey(&mdcParams, acctKey);
              m_mdcCCMA =
                // XXX: Instrs are configured via Params:
                new EConnector_ITCH_HotSpotFX_Link
                    (m_reactor,  m_secDefsMgr, nullptr, m_riskMgr, mdcParams,
                     nullptr);
            }
          }
          //-----------------------------------------------------------------//
          else
          //-----------------------------------------------------------------//
            throw utxx::badarg_error("Pairs-MF::Ctor: Invalid MDC=", mdcName);

          LOG_INFO(2,"Pairs-MF::Ctor: Created MDC: {}", mdcName)
          m_logger->flush();
        }

        //-------------------------------------------------------------------//
        // Pass and Aggr OMCs for this Pair:                                 //
        //-------------------------------------------------------------------//
        for (int j = 0; j < 2; ++j)
        {
          std::string const& omcName = cns[1][j];

          //-----------------------------------------------------------------//
          // FIX_MICEX_EQ:                                                   //
          //-----------------------------------------------------------------//
          if (omcName == "EConnector_FIX_MICEX_EQ")
          {
            if (m_omcMICEX_EQ == nullptr)
            {
              auto const& omcParams = GetParamsSubTree(a_params, omcName);
              std::string acctKey   =
                omcParams.get<std::string>("AccountPfx") + "-FIX-MICEX-EQ-" +
                (isProd ? "Prod20" : "TestL1");

              SetAccountKey(&omcParams, acctKey);
              m_omcMICEX_EQ =
                // XXX: Instrs are configured via Params:
                new EConnector_FIX_MICEX
                    (m_reactor, m_secDefsMgr, nullptr, m_riskMgr, omcParams,
                     nullptr);
            }
          }
          else
          //-----------------------------------------------------------------//
          // FIX_MICEX_FX:                                                   //
          //-----------------------------------------------------------------//
          if (omcName == "EConnector_FIX_MICEX_FX")
          {
            if (m_omcMICEX_FX == nullptr)
            {
              auto const& omcParams = GetParamsSubTree(a_params, omcName);
              std::string acctKey   =
                omcParams.get<std::string>("AccountPfx") + "-FIX-MICEX-FX-" +
                (isProd ? "Prod20" : "TestL1");

              SetAccountKey(&omcParams, acctKey);
              m_omcMICEX_FX =
                // XXX: Instrs are configured via Params:
                new EConnector_FIX_MICEX
                    (m_reactor, m_secDefsMgr, nullptr, m_riskMgr, omcParams,
                     nullptr);
            }
          }
          else
          //-----------------------------------------------------------------//
          // TWIME_FORTS (same for Fut and Opt, but only Fut are used):      //
          //-----------------------------------------------------------------//
          if (omcName == "EConnector_TWIME_FORTS")
          {
            if (m_omcFORTS == nullptr)
            {
              auto const& omcParams = GetParamsSubTree(a_params, omcName);
              std::string acctKey   =
                omcParams.get<std::string>("AccountPfx") + "-TWIME-FORTS-" +
                (isProd ? "Prod" : "TestL");

              SetAccountKey(&omcParams, acctKey);
              m_omcFORTS =
                // XXX: Instrs are configured via Params:
                new EConnector_TWIME_FORTS
                    (m_reactor, m_secDefsMgr, nullptr, m_riskMgr, omcParams);
            }
          }
          else
          //-----------------------------------------------------------------//
          // AlfaFIX2 (ORD):                                                 //
          //-----------------------------------------------------------------//
          if (omcName == "EConnector_FIX_AlfaFIX2_ORD")
          {
            if (m_omcAlfa == nullptr)
            {
              auto const& omcParams = GetParamsSubTree(a_params, omcName);
              std::string acctKey   =
                omcParams.get<std::string>("AccountPfx") + "-FIX-AlfaFIX2-" +
                (isProd ? "ProdORD" : "TestORD"); // XXX: TestORD not working?

              SetAccountKey(&omcParams, acctKey);
              m_omcAlfa =
                // XXX: Instrs are configured via Params:
                new EConnector_FIX_AlfaFIX2
                    (m_reactor, m_secDefsMgr, nullptr, m_riskMgr, omcParams,
                     nullptr);
            }
          }
          else
          //-----------------------------------------------------------------//
          // NTProgress (ORD):                                               //
          //-----------------------------------------------------------------//
          if (omcName == "EConnector_FIX_NTProgress_ORD")
          {
            if (m_omcNTPro == nullptr)
            {
              auto const& omcParams = GetParamsSubTree(a_params, omcName);
              std::string acctKey   =
                omcParams.get<std::string>("AccountPfx") + "-FIX-NTProgress-" +
                (isProd ? "ProdORD" : "TestORD");

              SetAccountKey(&omcParams, acctKey);
              m_omcNTPro =
                // XXX: Instrs are configured via Params:
                new EConnector_FIX_NTProgress
                    (m_reactor, m_secDefsMgr, nullptr, m_riskMgr, omcParams,
                     nullptr);
            }
          }
          else
          //-----------------------------------------------------------------//
          // Generic HotSpotFX:                                              //
          //-----------------------------------------------------------------//
          if (omcName == "EConnector_FIX_HotSpotFX_Gen")
          {
            if (m_omcHSFX_Gen == nullptr)
            {
              auto const& omcParams = GetParamsSubTree(a_params, omcName);
              std::string acctKey   =
                omcParams.get<std::string>("AccountPfx") + "-FIX-HotSpotFX-" +
                (isProd ? "Prod" : "Test");

              SetAccountKey(&omcParams, acctKey);
              m_omcHSFX_Gen =
                // XXX: Instrs are configured via Params:
                new EConnector_FIX_HotSpotFX_Gen
                    (m_reactor,  m_secDefsMgr, nullptr, m_riskMgr, omcParams,
                     nullptr);
            }
          }
          else
          //-----------------------------------------------------------------//
          // CCMAlpha (over HotSpotLink):                                    //
          //-----------------------------------------------------------------//
          if (omcName == "EConnector_FIX_CCMAlpha")
          {
            if (m_omcCCMA == nullptr)
            {
              auto const& omcParams = GetParamsSubTree(a_params, omcName);
              std::string acctKey   =
                omcParams.get<std::string>("AccountPfx") + "-FIX-HotSpotFX-" +
                (isProd ? "Prod" : "Test");

              SetAccountKey(&omcParams, acctKey);
              m_omcCCMA =
                // XXX: Instrs are configured via Params:
                new EConnector_FIX_HotSpotFX_Link
                    (m_reactor,  m_secDefsMgr, nullptr, m_riskMgr, omcParams,
                     nullptr);
            }
          }
          //-----------------------------------------------------------------//
          else
          //-----------------------------------------------------------------//
            throw utxx::badarg_error("Pairs-MF::Ctor: Invalid OMC=", omcName);

          LOG_INFO(2, "Pairs-MF::Ctor: Created OMC: {}", omcName)
          m_logger->flush();
        }

        //-------------------------------------------------------------------//
        // Create and install the "IPair":                                   //
        //-------------------------------------------------------------------//
        IPair pair(this, ps);

        // Check that it is not already installed, by any chance:
        int  nPairs = int(m_iPairs.size());
        auto prev   = std::find(m_iPairs.cbegin(), m_iPairs.cend(), pair);

        if (utxx::unlikely(prev != m_iPairs.cend()))
          throw utxx::badarg_error
            ("Pairs-MF::Ctor: Duplicate Pair", nPairs + 1,
             ", Prev Occurrence: ", int(prev - m_iPairs.cbegin()) + 1);

        if (utxx::unlikely(nPairs >= MaxPairs))
        {
          int const maxPairs = MaxPairs;   // To avoid a linking error!
          throw utxx::badarg_error
            ("Pairs-MF::Ctor: Too Many Pairs: MaxAllowed=", maxPairs);
        }
        // If OK:
        m_iPairs.push_back(pair);
      }

    //-----------------------------------------------------------------------//
    // Link MDCs to OMCs:                                                    //
    //-----------------------------------------------------------------------//
    // On MICEX (EQ and FX):
    if (m_omcMICEX_EQ != nullptr && m_mdcMICEX_EQ != nullptr)
      m_omcMICEX_EQ->LinkMDC(m_mdcMICEX_EQ);

    if (m_omcMICEX_FX != nullptr && m_mdcMICEX_FX != nullptr)
      m_omcMICEX_FX->LinkMDC(m_mdcMICEX_FX);

    // On FORTS:
    if (m_omcFORTS    != nullptr && m_mdcFORTS    != nullptr)
      m_omcFORTS   ->LinkMDC(m_mdcFORTS);

    // TODO: For other Connectors, MDC-OMC links are not implemented yet...

    //-----------------------------------------------------------------------//
    // Collect the Fixed Valuation Rates for all Assets and SettlCcys:       //
    //-----------------------------------------------------------------------//
    // Format of entries we are lookig for:
    // FixedRate_{Asset}/USD or FixedRate_USD/{Asset}:
    // XXX: It destructively updates "a_params", which is NOT GOOD:
    //
    TraverseParamsSubTree
    (
      GetParamsSubTree(a_params, "Main"),

      [this](std::string const& a_key, std::string const& a_val)->bool
      {
        char const* skey = a_key.data();
        if (strncmp(skey, "FixedRate_", 10) != 0)
          return true;  // Not our entry -- search further!

        // Strip the prefix:
        char*  pair  = const_cast<char*>(skey    + 10);
        char*  pEnd  = pair + (int(a_key.size()) - 10);
        assert(pair <= pEnd && *pEnd == '\0');

        // Between "pair" and "pEnd", there must be at least 5 chars: either
        // "A/USD" or "USD/A" as a minimum:
        if (utxx::unlikely(pEnd - pair < 5))
          return true;

        // If OK: Check the format: First, look for a '/'; it must be present,
        // and NOT at the beginning or at the end of "pair":
        char* sl = strchr(pair, '/');
        if (utxx::unlikely(sl == nullptr || sl == pair || sl == pEnd - 1))
          return true;  // Not our entry (though strange) -- search further!

        // Split the Pair by putting a '\0' in place of '/':
        *sl = '\0';

        // The value must be a positive "double":
        char*  dend = nullptr;
        double dval = strtod(a_val.data(), &dend);
        if (utxx::unlikely
           (dend != a_val.data() + a_val.size() || !(dval > 0.0)))
          return true;  // Again, very strange -- but skip it!

        // Then, either the part before '/' or the part after '/' must be
        // "USD":
        if (sl == pair + 3 && strcmp(pair, "USD") == 0)
        {
          // This is an unverse rate (USD/Asset), so:
          char const* asset = sl + 1;
          assert(*asset != '\0');
          this->m_fixedRates[MkSymKey(asset)] = 1.0 / dval;
        }
        else
        if (sl == pEnd - 4 && strcmp(sl + 1, "USD") == 0)
        {
          // This is a direct rate   (Asset/USD), so:
          char const* asset = pair;
          assert(*asset != '\0');
          this->m_fixedRates[MkSymKey(asset)] = dval;
        }
        // All other cases are skipped, so in any case, continue:
        return true;
      }
    );

    //-----------------------------------------------------------------------//
    // Subscribe to receive basic TradingEvents from all Connectors:         //
    //-----------------------------------------------------------------------//
    // (XXX: Otherwise we would not even know that the Connectors have become
    // Active!):
    // MDCs:
    if (m_mdcMICEX_EQ != nullptr)
      m_mdcMICEX_EQ->Subscribe(this);

    if (m_mdcMICEX_FX != nullptr)
      m_mdcMICEX_FX->Subscribe(this);

    if (m_mdcFORTS    != nullptr)
      m_mdcFORTS   ->Subscribe(this);

    if (m_mdcAlfa     != nullptr)
      m_mdcAlfa    ->Subscribe(this);

    if (m_mdcNTPro    != nullptr)
      m_mdcNTPro   ->Subscribe(this);

    if (m_mdcHSFX_Gen != nullptr)
      m_mdcHSFX_Gen->Subscribe(this);

    if (m_mdcCCMA     != nullptr)
      m_mdcCCMA    ->Subscribe(this);

    // OMCs:
    if (m_omcMICEX_EQ != nullptr)
      m_omcMICEX_EQ->Subscribe(this);

    if (m_omcMICEX_FX != nullptr)
      m_omcMICEX_FX->Subscribe(this);

    if (m_omcFORTS    != nullptr)
      m_omcFORTS   ->Subscribe(this);

    if (m_omcAlfa     != nullptr)
      m_omcAlfa    ->Subscribe(this);

    if (m_omcNTPro    != nullptr)
      m_omcNTPro   ->Subscribe(this);

    if (m_omcHSFX_Gen != nullptr)
      m_omcHSFX_Gen->Subscribe(this);

    if (m_omcCCMA     != nullptr)
      m_omcCCMA    ->Subscribe(this);

    //-----------------------------------------------------------------------//
    // For MktData Logging:                                                  //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely(m_mktDataDepth > 50))
      throw utxx::badarg_error
            ("Pairs-MF Ctor:: Too large MktDataDepth: ", m_mktDataDepth,
             " (limit: 50)");
    memset(m_mktDataBuff, '\0', sizeof(m_mktDataBuff));
  }

  //=========================================================================//
  // Dtor:                                                                   //
  //=========================================================================//
  inline Pairs_MF::~Pairs_MF() noexcept
  {
    // XXX: We do NOT invoke "SemiGracefulStop" from the Dtor -- doing so could
    // cause more harm than good.
    // Just de-allocate all Connectors (it's OK if they are already NULL):
    // MDCs:
    delete m_mdcMICEX_EQ;         m_mdcMICEX_EQ = nullptr;
    delete m_mdcMICEX_FX;         m_mdcMICEX_FX = nullptr;
    delete m_mdcFORTS;            m_mdcFORTS    = nullptr;
    delete m_mdcAlfa;             m_mdcAlfa     = nullptr;
    delete m_mdcNTPro;            m_mdcNTPro    = nullptr;
    delete m_mdcHSFX_Gen;         m_mdcHSFX_Gen = nullptr;
    delete m_mdcCCMA;             m_mdcCCMA     = nullptr;

    // OMCs:
    delete m_omcMICEX_EQ;         m_omcMICEX_EQ = nullptr;
    delete m_omcMICEX_FX;         m_omcMICEX_FX = nullptr;
    delete m_omcFORTS;            m_omcFORTS    = nullptr;
    delete m_omcAlfa;             m_omcAlfa     = nullptr;
    delete m_omcNTPro;            m_omcNTPro    = nullptr;
    delete m_omcHSFX_Gen;         m_omcHSFX_Gen = nullptr;
    delete m_omcCCMA;             m_omcCCMA     = nullptr;
  }

  //=========================================================================//
  // "Run":                                                                  //
  //=========================================================================//
  inline void Pairs_MF::Run()
  {
    // Start the Connectors:
    // MDCs:
    if (m_mdcMICEX_EQ != nullptr) m_mdcMICEX_EQ->Start();
    if (m_mdcMICEX_FX != nullptr) m_mdcMICEX_FX->Start();
    if (m_mdcFORTS    != nullptr) m_mdcFORTS   ->Start();
    if (m_mdcAlfa     != nullptr) m_mdcAlfa    ->Start();
    if (m_mdcNTPro    != nullptr) m_mdcNTPro   ->Start();
    if (m_mdcHSFX_Gen != nullptr) m_mdcHSFX_Gen->Start();
    if (m_mdcCCMA     != nullptr) m_mdcCCMA    ->Start();

    // OMCs:
    if (m_omcMICEX_EQ != nullptr) m_omcMICEX_EQ->Start();
    if (m_omcMICEX_FX != nullptr) m_omcMICEX_FX->Start();
    if (m_omcFORTS    != nullptr) m_omcFORTS   ->Start();
    if (m_omcAlfa     != nullptr) m_omcAlfa    ->Start();
    if (m_omcNTPro    != nullptr) m_omcNTPro   ->Start();
    if (m_omcHSFX_Gen != nullptr) m_omcHSFX_Gen->Start();
    if (m_omcCCMA     != nullptr) m_omcCCMA    ->Start();

    // Enter the Inifinite Event Loop (terminate it on any exception which pro-
    // pagates to that level):
    m_reactor->Run(true);
  }

  //=========================================================================//
  // "SemiGracefulStop":                                                     //
  //=========================================================================//
  // NB: This is still a "SEMI-graceful" stop: more rapid than "DelayedGraceful-
  // Stop" above, but still NOT an emergency termination of the Reactor Loop by
  // throwing the "EPollReactor::ExitRun" exception:
  //
  inline void Pairs_MF::SemiGracefulStop()
  {
    // Protect against multiple invocations -- can cause infinite recursion via
    // notifications sent by Connector's "Stop" methods:
    if (utxx::unlikely(m_nowStopInited))
      return;
    m_nowStopInited = true;

    // Stop the Connectors:
    // MDCs:
    if (m_mdcMICEX_EQ != nullptr) m_mdcMICEX_EQ->Stop();
    if (m_mdcMICEX_FX != nullptr) m_mdcMICEX_FX->Stop();
    if (m_mdcFORTS    != nullptr) m_mdcFORTS   ->Stop();
    if (m_mdcAlfa     != nullptr) m_mdcAlfa    ->Stop();
    if (m_mdcNTPro    != nullptr) m_mdcNTPro   ->Stop();
    if (m_mdcHSFX_Gen != nullptr) m_mdcHSFX_Gen->Stop();
    if (m_mdcCCMA     != nullptr) m_mdcCCMA    ->Stop();

    // OMCs:
    if (m_omcMICEX_EQ != nullptr) m_omcMICEX_EQ->Stop();
    if (m_omcMICEX_FX != nullptr) m_omcMICEX_FX->Stop();
    if (m_omcFORTS    != nullptr) m_omcFORTS   ->Stop();
    if (m_omcAlfa     != nullptr) m_omcAlfa    ->Stop();
    if (m_omcNTPro    != nullptr) m_omcNTPro   ->Stop();
    if (m_omcHSFX_Gen != nullptr) m_omcHSFX_Gen->Stop();
    if (m_omcCCMA     != nullptr) m_omcCCMA    ->Stop();

    // Output the curr positions for all SettlDates (by convention, UserID is
    // set to 0 here, but it does not really matter):
    if (m_riskMgr != nullptr)
      m_riskMgr->OutputAssetPositions("Pairs-MF::SemiGracefulStop", 0);

    m_logger->flush();
  }

  //=========================================================================//
  // "DelayedGracefulStop":                                                  //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "Out": Trivial and Non-Trivial Cases:                                   //
  //-------------------------------------------------------------------------//
  inline void Pairs_MF::DelayedGracefulStop::Out()
    { m_msg << ": EXITING..."; }

  template<typename T, typename... Args>
  inline void Pairs_MF::DelayedGracefulStop::Out
    (T const& a_t, Args&&... a_args)
  {
    m_msg << a_t;
    this->Out(std::forward<Args>(a_args)...);
  }

  //-------------------------------------------------------------------------//
  // Non-Default Ctor:                                                       //
  //-------------------------------------------------------------------------//
  template<typename... Args>
  inline Pairs_MF::DelayedGracefulStop::DelayedGracefulStop
  (
    Pairs_MF*       a_outer,
    utxx::time_val  a_ts_strat,
    Args&&... args
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
    Out(args...);
    std::string msg  = m_msg.str();

    // Log it:
    m_outer->m_logger->critical (msg);
    m_outer->m_logger->flush();

    // Save the msg in the StatusLine: FIXME: This is a VERY BAD idea -- use
    // ShM instead!
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
  // "CancelAllQuotes":                                                      //
  //=========================================================================//
  inline void Pairs_MF::CancelAllQuotes
  (
    EConnector_MktData const* a_mdc,      // If NULL, then for all MDCs
    utxx::time_val            a_ts_exch,
    utxx::time_val            a_ts_recv,
    utxx::time_val            a_ts_strat
  )
  {
    // Travserse all "IPair"s and Cancel Passive Orders on both Sides if rela-
    // ted in any way to the MDC specified; Buffered=true in all cases:
    for (IPair& ip: m_iPairs)
      if (a_mdc == nullptr || a_mdc == ip.m_passMDC || a_mdc == ip.m_aggrMDC)
      {
        // NB: If the corresp AOS is NULL, "CancelOrderSafe" does nothing:
        ip.CancelOrderSafe<true>
          (ip.m_passAOSes[Ask], a_ts_exch, a_ts_recv, a_ts_strat);
        ip.CancelOrderSafe<true>
          (ip.m_passAOSes[Bid], a_ts_exch, a_ts_recv, a_ts_strat);
        ip.m_passOMC->FlushOrders();
      }
  }
} // End namespace MAQUETTE
