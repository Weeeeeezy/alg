// vim:ts=2:et
//===========================================================================//
//          "Connectors/H2WS/Binance/EConnector_H2WS_Binance_MDC.cpp":       //
//                          H2WS-Based MDC for Binance                       //
//===========================================================================//
#include "Connectors/H2WS/Binance/EConnector_H2WS_Binance_MDC.h"
#include "Connectors/H2WS/Binance/Sender_MDC.hpp"
#include "Connectors/H2WS/Binance/Processor_MDC.hpp"
#include "Protocols/H2WS/WSProtoEngine.hpp"
#include "Venues/Binance/SecDefs.h"
#include "Venues/Binance/Configs_H2WS.h"
#include <chrono>
#include <thread>
#include <utxx/error.hpp>

using namespace std;

namespace MAQUETTE
{
  using namespace std;
  //=========================================================================//
  // "GetAllSecDefs":                                                        //
  //=========================================================================//
  template <Binance::AssetClassT::type AC>
  std::vector<SecDefS> const* EConnector_H2WS_Binance_MDC<AC>::GetSecDefs()
    { return &Binance::GetSecDefs(AC); }

  //=========================================================================//
  // "GetWSConfig":                                                          //
  //=========================================================================//
  template <Binance::AssetClassT::type AC>
  H2WSConfig const& EConnector_H2WS_Binance_MDC<AC>::GetWSConfig
  (
    MQTEnvT UNUSED_PARAM(a_env)
  )
  {
    // XXX: For Binance, only "Prod" is actually supported, but for an MDC,
    // this does not matter, so we always return the "Prod" config here:
    //
    auto   cit  = Binance::Configs_WS_MDC.find(AC);
    if (cit == Binance::Configs_WS_MDC.cend())
      throw utxx::runtime_error("Binance MDC: WS Config not found.");
    return cit->second;
  }

  //=========================================================================//
  // "GetH2Config":                                                          //
  //=========================================================================//
  template <Binance::AssetClassT::type AC>
  H2WSConfig const& EConnector_H2WS_Binance_MDC<AC>::GetH2Config(MQTEnvT a_env)
  {
    auto   cit  = Binance::Configs_H2_MDC.find(std::make_pair(AC, a_env));
    if (cit == Binance::Configs_H2_MDC.cend())
      throw utxx::runtime_error("Binance MDC: H2 Config not found.");
    return cit->second;
  }

  //=========================================================================//
  // Non-Default Ctor:                                                       //
  //=========================================================================//
  template <Binance::AssetClassT::type AC>
  EConnector_H2WS_Binance_MDC<AC>::EConnector_H2WS_Binance_MDC
  (
    EPollReactor*                       a_reactor,
    SecDefsMgr*                         a_sec_defs_mgr,
    vector<string>              const*  a_only_symbols,   // NULL=UseFullList
    RiskMgr*                            a_risk_mgr,
    boost::property_tree::ptree const&  a_params,
    EConnector_MktData*                 a_primary_mdc     // Normally NULL
  )
  : //-----------------------------------------------------------------------//
    // "EConnector": Virtual Base:                                           //
    //-----------------------------------------------------------------------//
    // NB: As usual, AccountKey is the Connector Instance Name.
    // It may look like {AccountPfx}-H2WS-Binance-MDC-{Env}:
    EConnector
    (
      a_params.get<std::string>("AccountKey"),
      "Binance",
      0,                            // XXX: No extra ShM data at the moment...
      a_reactor,
      false,                        // No need fora BusyWait
      a_sec_defs_mgr,
      Binance::GetSecDefs(AC),
      a_only_symbols,               // Restricting the full Binnace list
      a_params.get<bool>("ExplSettlDatesOnly", false),
      false,                        // No Tenors
      a_risk_mgr,
      a_params,
      QT,
      std::is_floating_point_v<QR>
    ),
    //-----------------------------------------------------------------------//
    // "EConnector_H2WS_MDC":                                                //
    //-----------------------------------------------------------------------//
    MDCH2WS
    (
      a_primary_mdc,
      a_params
    ),
    //-----------------------------------------------------------------------//
    // "EConnector_H2WS_Binance_MDC" Itself:                                 //
    //-----------------------------------------------------------------------//
    m_useBBA            (a_params.get<bool>("UseBBA", false)),
    m_snapShotFreqSeq   (a_params.get<unsigned>("SnapShotFreqSec", 3600)),
    m_updateMissedNotify(true),
    m_lastUpdateID      (),
    m_mdMsg             (),
    m_ssTimerFD         (-1)
  {
    //-----------------------------------------------------------------------//
    // Install empty entries in "m_lastUpdateIDs" for all H2Conns:           //
    //-----------------------------------------------------------------------//
    for (SecDefD const* instr: EConnector::m_usedSecDefs)
    {
      assert(instr != nullptr);
      m_lastUpdateID.insert({instr->m_Symbol, 0});
    }
  }

  //=========================================================================//
  // Dtor, Start and Stop                                                    //
  //=========================================================================//
  template <Binance::AssetClassT::type AC>
  EConnector_H2WS_Binance_MDC<AC>::~EConnector_H2WS_Binance_MDC() noexcept
    { RemoveSnapShotTimer(); }

  template <Binance::AssetClassT::type AC>
  void EConnector_H2WS_Binance_MDC<AC>::Start()
  {
    // We only start the H2 leg at the moment; the WS leg will be started when
    // the ListenKey becomes available (via the H2 leg):
    LOG_INFO(2, "EConnector_H2WS_Binance_MDC::Start: Starting Up")
    MDCH2WS::Start();
  }

  template <Binance::AssetClassT::type AC>
  void EConnector_H2WS_Binance_MDC<AC>::Stop()
  {
    LOG_INFO(2, "EConnector_H2WS_Binance_MDC::Stop: Stopping")
    RemoveSnapShotTimer();
    MDCH2WS::Stop();
  }

  //=========================================================================//
  // "InitWSSession":                                                        //
  //=========================================================================//
  // XXX: At the moment, we subscribe to Depth-20 SnapShots rather than Full-
  // Depth Incremental Updates. The reasons are the following:
  // (*) Full-Depth is still Aggregated, not FullOrdersLog -- no advantage in
  //     the level of detalization we would get from it;
  // (*) In both cases, MktData are sent with Period=1 sec, ie Increments are
  //     not coming in Full Real Time;
  // (*) We are not much interested in updates at far-away OrderBook levels;
  //     they may be problematic to handle, and could generate extra traffic
  //     whih reduces the savings from receiving Increments compared to Snap-
  //     Shots;
  // (*) Incr Updates require initialization via a REST req,  which is extra
  //     implementation complexity:
  //
  template <Binance::AssetClassT::type AC>
  void EConnector_H2WS_Binance_MDC<AC>::InitWSSession()
  {
    MDCH2WS::ProWS::InitWSHandShake("/ws");

    //-----------------------------------------------------------------------//
    // State Reset:                                                          //
    //-----------------------------------------------------------------------//
    // XXX: We probably need to reset the following state at the beginning of a
    // New Session:
    m_updateMissedNotify = true;
    m_lastUpdateID.clear();

    for (auto h2conn: MDCH2WS::m_h2conns)
      h2conn->m_snapStreamID.clear();
  }

  //=========================================================================//
  // "OnTurningActive":                                                      //
  //=========================================================================//
  template <Binance::AssetClassT::type AC>
  void EConnector_H2WS_Binance_MDC<AC>::OnTurningActive()
  {
    LOG_INFO(2, "EConnector_H2WS_Binance_OMC::OnTurningActive")

    //-----------------------------------------------------------------------//
    // Reset the H2Conns' states:                                            //
    //-----------------------------------------------------------------------//
    for (auto h2conn: MDCH2WS::m_h2conns)
      h2conn->m_snapStreamID.clear();

    //-----------------------------------------------------------------------//
    // Renew the SnapShots Timer:                                            //
    //-----------------------------------------------------------------------//
    if (!m_useBBA)
      AddSnapShotTimer();
  }

  //-------------------------------------------------------------------------//
  // "InitWSLogOn":                                                          //
  //-------------------------------------------------------------------------//
  template <Binance::AssetClassT::type AC>
  void EConnector_H2WS_Binance_MDC<AC>::InitWSLogOn()
  {
    RemoveSnapShotTimer();
    assert(m_ssTimerFD == -1);

    const auto &all_sec_defs = EConnector::m_usedSecDefs;

    size_t total_subscriptions =
        all_sec_defs.size() * (EConnector_MktData::HasTrades() ? 2 : 1);

    bool sub_to_all_ticker = total_subscriptions > MaxSubscriptions;

    if (sub_to_all_ticker && !m_useBBA)
      throw utxx::runtime_error("Too many subscriptions to subscribe "
          "to depth, set UseBBA = true");

    if (EConnector_MktData::HasTrades() &&
        all_sec_defs.size() > MaxSubscriptions)
      throw utxx::runtime_error("Too many subscriptions to subscribe "
          "to trades");

    if (sub_to_all_ticker && !EConnector_MktData::HasTrades()) {
      std::string req_str = "{\"method\":\"SUBSCRIBE\",\"params\":["
          "\"!bookTicker\"],\"id\":1}";

      CHECK_ONLY(
        MDCWS::template LogMsg<true>(req_str.c_str(), nullptr, 0);
      )

      MDCH2WS::ProWS::PutTxtData(req_str.c_str());
      MDCH2WS::ProWS::SendTxtFrame();
    } else {
      // subscribe in batches of 100
      constexpr size_t batch_size = 100;
      int id = 1;
      for (size_t i = 0; i < all_sec_defs.size(); i += batch_size) {
        stringstream req;
        req << "{\"method\":\"SUBSCRIBE\",\"params\":[";

        if (sub_to_all_ticker && (i == 0))
          req << "\"!bookTicker\",";

        bool more = true;
        for (size_t j = 0; j < batch_size; ++j) {
          if ((i + j) >= all_sec_defs.size()) {
            more = false;
            break;
          }

          auto instr = all_sec_defs[i + j];
          assert(instr != nullptr);

          if (!sub_to_all_ticker)
            req << "\"" << instr->m_AltSymbol.data()
                << (m_useBBA ? "@bookTicker\"," : "@depth@100ms\",");


          if (EConnector_MktData::HasTrades())
            req << "\"" << instr->m_AltSymbol.data()
                << (IsFut ? "@aggTrade\"," : "@trade\",");
        }

        // remove last comma
        auto req_str = req.str();
        if (req_str.back() == ',')
          req_str.pop_back();

        req_str += "],\"id\":" + std::to_string(id) + "}";
        ++id;

        CHECK_ONLY(
          MDCWS::template LogMsg<true>(req_str.c_str(), nullptr, 0);
        )

        MDCH2WS::ProWS::PutTxtData(req_str.c_str());
        MDCH2WS::ProWS::SendTxtFrame();

        // don't exceed rate limit of 5 reqs per sec
        if (more)
          std::this_thread::sleep_for(std::chrono::milliseconds(250));
      }
    }

    // req << "\"!bookTicker\",\"!trade\"";

    // Signal completion
    MDCH2WS::WSLogOnCompleted();
  }

  //=========================================================================//
  // SnapShot Helpers:                                                       //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "AddSnapShotTimer":                                                     //
  //-------------------------------------------------------------------------//
  template <Binance::AssetClassT::type AC>
  inline void EConnector_H2WS_Binance_MDC<AC>::AddSnapShotTimer()
  {
    if (utxx::likely(m_ssTimerFD == -1))
    {
      // TimerHandler:
      IO::FDInfo::TimerHandler timerH
        ([this](int a_fd)->void   { this->SnapShotTimerHandler (a_fd); });

      // ErrorHandler:
      IO::FDInfo::ErrHandler   errH
      (
        [this](int a_fd,   int   a_err_code, uint32_t  a_events,
               char const* a_msg) -> void
        { this->SnapShotTimerErrHandler(a_fd, a_err_code, a_events, a_msg); }
      );

      // Period: 10 min = 600 sec = 600000 msec:
      constexpr uint32_t USTimerPeriodMSec = 21'600'000;

      // Create the TimerFD and add it to the Reactor:
      assert(EConnector::m_reactor != nullptr);
      m_ssTimerFD =
        EConnector::m_reactor->AddTimer
        (
          "UserStreamTimer",
          3000,  // Initial offset
          USTimerPeriodMSec,  // Period
          timerH,
          errH
        );
    }
  }

  //-------------------------------------------------------------------------//
  // "RemoveSnapShotTimer":                                                  //
  //-------------------------------------------------------------------------//
  template <Binance::AssetClassT::type AC>
  inline void EConnector_H2WS_Binance_MDC<AC>::RemoveSnapShotTimer()
  {
    if (m_ssTimerFD >= 0)
    {
      if (utxx::likely(EConnector::m_reactor != nullptr))
        EConnector::m_reactor->Remove(m_ssTimerFD);

      // Anyway, close the socket:
      (void) close(m_ssTimerFD);
      m_ssTimerFD = -1;
    }
  }

  //-------------------------------------------------------------------------//
  // "RequestSnapShots":                                                     //
  //-------------------------------------------------------------------------//
  template <Binance::AssetClassT::type AC>
  inline void EConnector_H2WS_Binance_MDC<AC>::RequestSnapShots()
  {
    if (utxx::unlikely(MDCH2WS::m_currH2Conn == nullptr))
      throw utxx::runtime_error
            ("EConnector_H2WS_Binance_MDC::RequestSnapShots: No H2Conns");

    // Send one request per each Symbol:
    std::string query = std::string(QueryPfx)
                      + "depth?limit="
                      + to_string(SnapDepth)
                      + "&symbol=";
    for (SecDefD const* instr: EConnector::m_usedSecDefs)
    {
      uint32_t streamID =
        SendReq("GET", (query + ToString(instr->m_Symbol)).data());

      MDCH2WS::m_currH2Conn->m_snapStreamID[instr->m_Symbol] = streamID;

      LOG_INFO(3, "EConnector_H2WS_Binance_MDC::RequestSnapShots for {}: "
                  "StreamID={}", instr->m_Symbol.data(), streamID)
    }
  }

  //-------------------------------------------------------------------------//
  // "SnapShotTimerHandler":                                                 //
  //-------------------------------------------------------------------------//
  // Time-Driven SnapShot requests:
  //
  template <Binance::AssetClassT::type AC>
  inline void EConnector_H2WS_Binance_MDC<AC>::SnapShotTimerHandler
    (int DEBUG_ONLY(a_fd))
  {
    assert(a_fd == m_ssTimerFD);
    RequestSnapShots();
  }

  //-------------------------------------------------------------------------//
  // "SnapShotTimerErrHandler":                                              //
  //-------------------------------------------------------------------------//
  template <Binance::AssetClassT::type AC>
  inline void EConnector_H2WS_Binance_MDC<AC>::SnapShotTimerErrHandler
    (int a_fd, int a_err_code, uint32_t a_events, char const* a_msg)
  {
    // There should be NO error associated with this Timer at all. If one occ-
    // urs, we better shut down completely:
    assert(a_fd == m_ssTimerFD);
    LOG_ERROR(1,
      "EConnector_H2WS_Binance_OMC::USTimerErrHandler: TimerFD={}, ErrCode={}, "
      "Events={}, Msg={}: STOPPING",
      a_fd, a_err_code, EConnector::m_reactor->EPollEventsToStr(a_events),
      (a_msg != nullptr) ? a_msg : "")
   Stop();
  }

  //=========================================================================//
  // Explicit Instantiation:                                                 //
  //=========================================================================//
  template class EConnector_H2WS_Binance_MDC<Binance::AssetClassT::Spt>;
  template class EConnector_H2WS_Binance_MDC<Binance::AssetClassT::FutT>;
  template class EConnector_H2WS_Binance_MDC<Binance::AssetClassT::FutC>;
}
// End namespace MAQUETTE
