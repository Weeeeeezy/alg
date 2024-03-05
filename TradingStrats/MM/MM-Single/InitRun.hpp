// vim:ts=2:et
//===========================================================================//
//                   "TradingStrats/MM-Single/InitRun.hpp":                  //
//                              Ctor, Dtor, Run                              //
//===========================================================================//
#pragma once

#include "Basis/ConfigUtils.hpp"
#include "Basis/TimeValUtils.hpp"
#include "MM-Single.h"
#include "InfraStruct/SecDefsMgr.h"
#include "InfraStruct/RiskMgr.h"
#include "Basis/Maths.hpp"
#include <cstring>

namespace {
  using namespace MAQUETTE;

  //=========================================================================//
  // Utils:                                                                  //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "GetEnabledUntil":                                                      //
  //-------------------------------------------------------------------------//
  inline utxx::time_val
  GetEnabledUntil(boost::property_tree::ptree const& a_params) {
    std::string mskStr = a_params.get<std::string>("EnabledUntil_MSK");
    if (utxx::unlikely(mskStr.size() != 5 || mskStr[2] != ':'))
      return utxx::time_val();

    // Otherwise: Get HH and MM:
    mskStr[2] = '\0';
    int hh    = atoi(mskStr.data());
    int mm    = atoi(mskStr.data() + 3);
    if (utxx::unlikely(hh < 0 || mm < 0 || hh > 23 || mm > 59))
      return utxx::time_val();

    // Compute the time limit in UTC = MSK-3:
    return GetCurrDate() + utxx::secs((hh - 3) * 3600 + mm * 60);
  }
}  // namespace

namespace MAQUETTE {
  //=========================================================================//
  // "IPair" Default Ctor:                                                   //
  //=========================================================================//
  template <class MDC, class OMC>
  inline MM_Single<MDC, OMC>::IPair::IPair() :
      // The Strategy itself:
      m_outer(nullptr),
      // Logging:
      m_logger(nullptr),
      m_debugLevel(0),
      m_enabledUntil(),
      m_MDC(nullptr),
      m_OMC(nullptr),
      m_Instr(nullptr),
      m_OB(nullptr),
      m_Qty(0.0),
      m_AOSes{nullptr, nullptr},
      m_Pos(0),
      m_posSoftLimit(0),
      m_lastFillTS(),
      m_reQuoteDelayMSec(0),
      // Quant Params:
      m_adjCoeffs{NaN<double>, NaN<double>},
      m_quotePxs(),  // NaNs
      m_targAdjMode(TargAdjModeT::None),
      m_resistCoeff(NaN<double>),
      m_nSprdPeriods(0),
      m_currSprdTicks(0),
      m_volTarget(0),
      m_useFlipFlop(false),
      m_bpsSpreadThresh(NaN<double>),
      m_forcePxStep(NaN<double>) {}

  //=========================================================================//
  // "IPair" Non-Default Ctor:                                               //
  //=========================================================================//
  template <class MDC, class OMC>
  inline MM_Single<MDC, OMC>::IPair::IPair(
      MM_Single*                   a_outer,
      boost::property_tree::ptree& a_params  // XXX: Mutable!
      ) :
      // The Strategy itself:
      m_outer(a_outer),
      // Logging:
      m_logger(a_outer->m_logger), m_debugLevel(a_outer->m_debugLevel),
      // Until what time (UTC) this IPair is enabled?
      m_enabledUntil(GetEnabledUntil(a_params)),
      // Passive Leg:
      m_MDC(a_outer->GetMDC(a_params.get<std::string>("MDC"))),
      m_OMC(a_outer->GetOMC(a_params.get<std::string>("OMC"))),
      m_Instr(&(a_outer->m_secDefsMgr->FindSecDef(
          a_params.get<std::string>("Instr").data()))),
      m_OB(&(m_MDC->GetOrderBook(*m_Instr))),
      m_Qty(a_params.get<QR>("Qty")),
      m_AOSes{nullptr, nullptr},
      m_Pos(a_params.get<QR>("InitPos", 0.0)),
      m_posSoftLimit(a_params.get<QR>("PosSoftLimit")),
      m_lastFillTS(),
      m_reQuoteDelayMSec(a_params.get<int>("ReQuoteDelayMSec")),
      // Quant Params:
      m_adjCoeffs{a_params.get<double>("AdjCoeff0"),
                  a_params.get<double>("AdjCoeff1")},
      m_quotePxs(),  // NaNs
      m_targAdjMode(TargAdjModeT::from_string
                         (a_params.get<std::string>("TargAdjMode"))),
      m_resistCoeff(a_params.get<double>("ResistCoeff")),
      m_currSprdTicks(0),
      m_volTarget(a_params.get<double>("VolTarget")),
      m_useFlipFlop(a_params.get<bool>("UseFlipFlop")),
      m_bpsSpreadThresh(a_params.get<int>("BpsSpreadThresh")),
      m_forcePxStep(a_params.get<double>("ForcePxStep")) {

    // Check the signs of other params:
    if (utxx::unlikely(m_resistCoeff < 0.0))
      throw utxx::badarg_error("MM-Single::IPair::Ctor: Invalid ResistCoeff=",
                               m_resistCoeff);

    // Verify the Qtys:
    if (utxx::unlikely(m_Qty <= 0 || m_posSoftLimit <= 0))
      throw utxx::badarg_error(
          "MM-Single::IPair::Ctor: Invalid Qty(s) or Limit(s)");
  }

  //=========================================================================//
  // "IPair" Equality:                                                       //
  //=========================================================================//
  // XXX: It is probably sufficient to compare only the Instruments (by ptr):
  //
  template<class MDC, class OMC>
  inline bool MM_Single<MDC, OMC>::IPair::operator==(MM_Single::IPair
      const& right) const {
    return this->m_Instr == right.m_Instr &&
           this->m_Instr == right.m_Instr;
  }

  //=========================================================================//
  // "MM_Single::Get{MDC,OMC}":                                       //
  //=========================================================================//
  // These methods either throw an exception, or return a non-NULL ptr:
  //
  template<class MDC, class OMC>
  inline EConnector_MktData*
  MM_Single<MDC, OMC>::GetMDC(std::string const& a_conn_section) const {
    char const* cs = a_conn_section.data();
    return (strcmp(cs, "EConnector_WS_BitFinex_MDC") == 0)
               ? m_mdc
               : throw utxx::badarg_error(
                     "MM_Single::GetMDC: Invalid ConnSection=", a_conn_section);
  }

  template<class MDC, class OMC>
  inline EConnector_OrdMgmt*
  MM_Single<MDC, OMC>::GetOMC(std::string const& a_conn_section) const {
    char const* cs = a_conn_section.data();
    return (strcmp(cs, "EConnector_WS_BitFinex_OMC") == 0)
               ? m_omc
               : throw utxx::badarg_error(
                     "MM_Single::GetOMC: Invalid ConnSection=", a_conn_section);
  }

  //=========================================================================//
  // "MM_Single": Non-Default Ctor:                                          //
  //=========================================================================//
  template<class MDC, class OMC>
  inline MM_Single<MDC, OMC>::MM_Single(
      EPollReactor*                a_reactor,
      spdlog::logger*              a_logger,
      boost::property_tree::ptree& a_params  // XXX: Mutable!
      ) :
      //---------------------------------------------------------------------//
      // Declarative Initialisations:                                        //
      //---------------------------------------------------------------------//
      // Base Strategy Ctor:
      Strategy
      (
        "MM-Single",
        a_reactor,
        a_logger,
        a_params.get<int>("Main.DebugLevel"),
        {SIGINT}
      ),
      // DryRun?
      m_dryRun(a_params.get<bool>("Main.DryRun")),

      // SecDefsMgr:
      m_secDefsMgr(SecDefsMgr::GetPersistInstance(
          a_params.get<std::string>("Main.Env") == "Prod",
          "")),

      // RiskMgr:
      m_riskMgr(a_params.get<bool>("RiskMgr.IsEnabled")
                    ? RiskMgr::GetPersistInstance(
                          a_params.get<std::string>("Main.Env") == "Prod",
                          GetParamsSubTree(a_params, "RiskMgr"),
                          *m_secDefsMgr,
                          false,  // NOT Observer!
                          m_logger,
                          m_debugLevel)
                    : nullptr),

      // MktData Logger (optional):
      m_mktDataLoggerShP(
          !a_params.get<std::string>("Exec.MktDataLogFile").empty()
              ? IO::MkLogger("MktData",
                             a_params.get<std::string>("Exec.MktDataLogFile"))
              : nullptr),
      m_mktDataLogger(m_mktDataLoggerShP.get()),
      m_mktDataDepth(a_params.get<int>("Exec.MktDataDepth")),
      m_mktDataBuff(),

      // Connectors (MDCs and OMCs): Initially all NULLs, later assembled by
      // scanning the "Pairs" Sections:
      m_mdc(nullptr),
      m_omc(nullptr),

      // Infrastructure:
      m_statusLineFile(a_params.get<std::string>("Exec.StatusLine")),
      m_maxRounds(a_params.get<int>("Main.MaxRounds")),
      m_roundsCount(0),
      m_allConnsActive(false),
      m_signalsCount(0),

      // "IPair"s -- initially empty, assembled below. Same for Asset->USD conv
      // rates:
      m_iPairs(),
      m_fixedRates() {
    //-----------------------------------------------------------------------//
    // Initialise the Connectors and "IPair"s:                               //
    //-----------------------------------------------------------------------//
    // Make sure Reactor and Logger are non-NULL ("Strategy" itself does not
    // require that):
    if (utxx::unlikely(m_reactor == nullptr || m_logger == nullptr))
      throw utxx::badarg_error(
          "MM-Single::Ctor: Reactor and Logger must be non-NULL");

    // Go through all "Pair*" sections of the Config:
    bool isProd =
        (strcmp(a_params.get<std::string>("Main.Env").data(), "Prod") == 0);

    for (auto& sect : a_params)
      if (strncmp(sect.first.data(), "Pair", 4) == 0) {
        // This is an "Pair{N}" section:
        auto& ps = sect.second;

        //-------------------------------------------------------------------//
        // MDC for this Pair:                                                //
        //-------------------------------------------------------------------//
        if (m_mdc == nullptr) {
          auto const& mdcParams = GetParamsSubTree(a_params, MDC::Name);
          std::string acctKey   = mdcParams.template
            get<std::string>("AccountPfx") + "-" + MDC::Name + "-" +
            (isProd ? "Prod" : "Test");

          SetAccountKey(&mdcParams, acctKey);
          m_mdc = new MDC(m_reactor, m_secDefsMgr, nullptr,
                          m_riskMgr, mdcParams,    nullptr);
        }
        LOG_INFO(2, "MM-Single::Ctor: Created MDC: {}", MDC::Name)
        m_logger->flush();

        //------------------------------------------------------------------//
        // OMC:                                                             //
        //------------------------------------------------------------------//
        if (m_omc == nullptr) {
          auto const& omcParams = GetParamsSubTree(a_params, OMC::Name);
          std::string acctKey   = omcParams.template
            get<std::string>("AccountPfx") + "-" + OMC::Name + "-" +
            (isProd ? "Prod" : "Test");

          SetAccountKey(&omcParams, acctKey);
          m_omc = new OMC(m_reactor, m_secDefsMgr, nullptr,
                          m_riskMgr, omcParams);
        }
        LOG_INFO(2, "MM-Single::Ctor: Created OMC: {}", OMC::Name)
        m_logger->flush();

        //-------------------------------------------------------------------//
        // Create and install the "IPair":                                   //
        //-------------------------------------------------------------------//
        IPair pair(this, ps);

        // Check that it is not already installed, by any chance:
        int  nPairs = int(m_iPairs.size());
        auto prev   = std::find(m_iPairs.cbegin(), m_iPairs.cend(), pair);

        if (utxx::unlikely(prev != m_iPairs.cend()))
          throw utxx::badarg_error(
              "MM-Single::Ctor: Duplicate Pair", nPairs + 1,
              ", Prev Occurrence: ", int(prev - m_iPairs.cbegin()) + 1);

        if (utxx::unlikely(nPairs >= MaxPairs)) {
          int const maxPairs = MaxPairs;  // To avoid a linking error!
          throw utxx::badarg_error(
              "MM-Single::Ctor: Too Many Pairs: MaxAllowed=", maxPairs);
        }
        // If OK:
        m_iPairs.push_back(pair);
      }

    //-----------------------------------------------------------------------//
    // Link MDCs to OMCs:                                                    //
    //-----------------------------------------------------------------------//

    // TODO: For other Connectors, MDC-OMC links are not implemented yet...

    //-----------------------------------------------------------------------//
    // Subscribe to receive basic TradingEvents from all Connectors:         //
    //-----------------------------------------------------------------------//
    // (XXX: Otherwise we would not even know that the Connectors have become
    // Active!):
    // MDCs:
    if (m_mdc != nullptr)
      m_mdc->Subscribe(this);

    // OMCs:
    if (m_omc != nullptr)
      m_omc->Subscribe(this);

    //-----------------------------------------------------------------------//
    // For MktData Logging:                                                  //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely(m_mktDataDepth > 50))
      throw utxx::badarg_error("MM-Single Ctor:: Too large MktDataDepth: ",
                               m_mktDataDepth, " (limit: 50)");
    memset(m_mktDataBuff, '\0', sizeof(m_mktDataBuff));
  }

  //=========================================================================//
  // Dtor:                                                                   //
  //=========================================================================//
  template<class MDC, class OMC>
  inline MM_Single<MDC, OMC>::~MM_Single() noexcept
  {
    // XXX: We do NOT invoke "SemiGracefulStop" from the Dtor -- doing so could
    // cause more harm than good.
    // Just de-allocate all Connectors (it's OK if they are already NULL):
    // MDCs:
    delete m_mdc;
    m_mdc = nullptr;
    // OMCs:
    delete m_omc;
    m_omc = nullptr;
  }

  //=========================================================================//
  // "Run":                                                                  //
  //=========================================================================//
  template<class MDC, class OMC>
  inline void MM_Single<MDC, OMC>::Run() {
    // Start the Connectors:
    // MDCs:
    if (m_mdc != nullptr)
      m_mdc->Start();
    // OMCs:
    if (m_omc != nullptr)
      m_omc->Start();

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
  template<class MDC, class OMC>
  inline void MM_Single<MDC, OMC>::SemiGracefulStop() {
    // Protect against multiple invocations -- can cause infinite recursion via
    // notifications sent by Connector's "Stop" methods:
    if (utxx::unlikely(m_nowStopInited))
      return;
    m_nowStopInited = true;

    // Stop the Connectors:
    // MDCs:
    if (m_mdc != nullptr)
      m_mdc->Stop();

    // OMCs:
    if (m_omc != nullptr)
      m_omc->Stop();

    // Output the curr positions for all SettlDates (by convention, UserID is
    // set to 0 here, but it does not really matter):
    if (m_riskMgr != nullptr)
      m_riskMgr->OutputAssetPositions("MM-Single::SemiGracefulStop", 0);

    m_logger->flush();
  }

  //=========================================================================//
  // "DelayedGracefulStop":                                                  //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "Out": Trivial and Non-Trivial Cases:                                   //
  //-------------------------------------------------------------------------//
  template<class MDC, class OMC>
  inline void MM_Single<MDC, OMC>::DelayedGracefulStop::Out()
  { m_msg << ": EXITING..."; }

  template<class MDC, class OMC>
  template <typename T, typename... Args>
  inline void MM_Single<MDC, OMC>::DelayedGracefulStop::Out(T const& a_t,
                                                  Args&&... a_args) {
    m_msg << a_t;
    this->Out(std::forward<Args>(a_args)...);
  }

  //-------------------------------------------------------------------------//
  // Non-Default Ctor:                                                       //
  //-------------------------------------------------------------------------//
  template<class MDC, class OMC>
  template <typename... Args>
  inline MM_Single<MDC, OMC>::DelayedGracefulStop::DelayedGracefulStop(
      MM_Single<MDC, OMC>* a_outer,
      utxx::time_val a_ts_strat,
      Args&&... args) :
      m_outer(a_outer),
      m_msg() {
    assert(m_outer != nullptr);

    // Record the time stamp when we initiated the Delayed or Now-Stop. If so,
    // we are now in the Stopping Mode -- nothing more to do:
    if (utxx::unlikely(m_outer->IsStopping()))
      return;

    m_outer->m_delayedStopInited = a_ts_strat;
    assert(!m_outer->m_delayedStopInited.empty() && m_outer->IsStopping());

    // Form the msg:
    Out(args...);
    std::string msg = m_msg.str();

    // Log it:
    m_outer->m_logger->critical(msg);
    m_outer->m_logger->flush();

    // Save the msg in the StatusLine: FIXME: This is a VERY BAD idea -- use
    // ShM instead!
    if (!m_outer->m_statusLineFile.empty()) {
      FILE* f = fopen(m_outer->m_statusLineFile.data(), "w");
      (void)fwrite(msg.data(), 1, msg.size(), f);
      (void)fclose(f);
    }

    // XXX: Do NOT issue any Cancellations etc yet, because we may have exceeded
    // the MaxMsgRate for the OMC Connector -- so do not exacerbate this condit-
    // ion!..
  }

  //=========================================================================//
  // "CancelAllQuotes":                                                      //
  //=========================================================================//
  template<class MDC, class OMC>
  inline void MM_Single<MDC, OMC>::CancelAllQuotes(
      EConnector_MktData const* a_mdc,  // If NULL, then for all MDCs
      utxx::time_val            a_ts_exch,
      utxx::time_val            a_ts_recv,
      utxx::time_val            a_ts_strat) {
    // Travserse all "IPair"s and Cancel Passive Orders on both Sides if rela-
    // ted in any way to the MDC specified; Buffered=true in all cases:
    for (IPair& ip : m_iPairs)
      if (a_mdc == nullptr || a_mdc == ip.m_MDC) {
        // NB: If the corresp AOS is NULL, "CancelOrderSafe" does nothing:
        ip.template CancelOrderSafe<true>(ip.m_AOSes[Ask], a_ts_exch,
                                 a_ts_recv, a_ts_strat);
        ip.template CancelOrderSafe<true>(ip.m_AOSes[Bid], a_ts_exch,
                                 a_ts_recv, a_ts_strat);
        ip.m_OMC->FlushOrders();
      }
  }
}  // End namespace MAQUETTE