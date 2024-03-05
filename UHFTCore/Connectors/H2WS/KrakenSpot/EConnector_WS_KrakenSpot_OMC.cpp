// vim:ts=2:et
//===========================================================================//
//         "Connectors/H2WS/KrakenSpot/EConnector_WS_KrakenSpot_OMC.cpp":        //
//                         H2WS-Based OMC for KrakenSpot                        //
//===========================================================================//
#include "Connectors/H2WS/KrakenSpot/EConnector_WS_KrakenSpot_OMC.h"
#include "Connectors/H2WS/KrakenSpot/Sender_OMC.hpp"
#include "Connectors/H2WS/KrakenSpot/Processor_OMC.hpp"
#include "Venues/KrakenSpot/SecDefs.h"
#include "Venues/KrakenSpot/Configs_H2WS.h"
#include "Connectors/H2WS/EConnector_WS_OMC.hpp"
#include "Protocols/H2WS/WSProtoEngine.hpp"
#include <gnutls/crypto.h>

namespace MAQUETTE
{
  using namespace std;

  //=========================================================================//
  // Config and Account Mgmt:                                                //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "GetAllSecDefs":                                                        //
  //-------------------------------------------------------------------------//
  std::vector<SecDefS> const* EConnector_WS_KrakenSpot_OMC::GetSecDefs()
    { return &KrakenSpot::GetSecs; }

  //=========================================================================//
  // Config and Account Mgmt:                                                //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "GetWSConfig":                                                          //
  //-------------------------------------------------------------------------//
  H2WSConfig const& EConnector_WS_KrakenSpot_OMC::GetWSConfig
  (
    MQTEnvT a_env
  )
  {
    auto cit = KrakenSpot::Configs_WS_OMC.find(a_env);
    if (cit == KrakenSpot::Configs_WS_OMC.cend())
      throw utxx::runtime_error("KrakenSpot OMC: WS Config not found.");
    return cit->second;
  }

  //=========================================================================//
  // Non-Default Ctor:                                                       //
  //=========================================================================//
  EConnector_WS_KrakenSpot_OMC::EConnector_WS_KrakenSpot_OMC
  (
    EPollReactor*                       a_reactor,
    SecDefsMgr*                         a_sec_defs_mgr,
    vector<string>              const*  a_only_symbols,  // NULL=UseFullList
    RiskMgr*                            a_risk_mgr,
    boost::property_tree::ptree const&  a_params
  )
  : //-----------------------------------------------------------------------//
    // "EConnector": Virtual Base:                                           //
    //-----------------------------------------------------------------------//
    EConnector
    (
      a_params.get<std::string>("AccountKey"),
      "KrakenSpot",
      0,                            // XXX: No extra ShM data at the moment...
      a_reactor,
      false,                        // No need fora BusyWait
      a_sec_defs_mgr,
      KrakenSpot::GetSecs,
      a_only_symbols,               // Restricting "KrakenSpot::GetSecs"
      a_params.get<bool>("ExplSettlDatesOnly", false),
      false,                        // No Tenors
      a_risk_mgr,
      a_params,
      QT,
      std::is_floating_point_v<QR>
    ),
    //-----------------------------------------------------------------------//
    // "EConnector_H2WS_OMC":                                                //
    //-----------------------------------------------------------------------//
    OMCWS(a_params),
    //-----------------------------------------------------------------------//
    // This Class:                                                           //
    //-----------------------------------------------------------------------//
    m_userStreamStr    (),
    m_path             (),
    m_signSHA256       (),
    m_signHMAC         { m_signSHA256, uint8_t(sizeof(m_signSHA256)) },
    m_usTimerFD        (-1)
  {
    // The following assert ensures that our static SHA256 sizes are correct:
    assert(gnutls_hmac_get_len(GNUTLS_MAC_SHA256) == sizeof(m_signSHA256));
  }

  //=========================================================================//
  // Dtor, Starting and Stopping:                                            //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // Dtor:                                                                   //
  //-------------------------------------------------------------------------//
  EConnector_WS_KrakenSpot_OMC::~EConnector_WS_KrakenSpot_OMC() noexcept
  {
    // Remove the UserStream Timer, catching any possible exns:
    try  { RemoveUSTimer(); }
    catch(...){}
  }

  //-------------------------------------------------------------------------//
  // "Start":                                                                //
  //-------------------------------------------------------------------------//
  void EConnector_WS_KrakenSpot_OMC::Start()
  {
    LOG_INFO(2, "EConnector_WS_KrakenSpot_OMC::Start: Starting Up")
    OMCH2WS::Start();
  }

  //-------------------------------------------------------------------------//
  // "Stop":                                                                 //
  //-------------------------------------------------------------------------//
  void EConnector_WS_KrakenSpot_OMC::Stop()
  {
    LOG_INFO(2, "EConnector_WS_KrakenSpot_OMC::Stop: Stopping")
    // Remove the USTimer:
    RemoveUSTimer();

    // Invoke "Stop" on the Parent:
    OMCH2WS::Stop();
  }

  //=========================================================================//
  // Session Mgmt:                                                           //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "InitWSSession":                                                        //
  //-------------------------------------------------------------------------//
  // The path is obtained dynamically from an HTTP/2 session, and is assumed to
  // be in "m_userStreamStr" by now:
  //
  void EConnector_WS_KrakenSpot_OMC::InitWSSession()
  {
    LOG_INFO(2, "EConnector_WS_KrakenSpot_OMC::InitWSSession")

    // Always start by cleaning up and firing up H2 Leg to get open orders,
    // balances and listenKey
    m_listenKeyReceived  = false;
    m_balOrPosReceived   = false;

    // Clear the open orders (XXX: "m_openOrders" potentially contains OrderIDs
    // or irders placed via the UI, ie not available in "Req12"s. This structu-
    // re is rather odd):
    EConnector_OrdMgmt::m_positions.clear();
    EConnector_OrdMgmt::m_balances.clear();

    // Set the timer to request the listenKey:
    SetUSTimer();
  }

  //-------------------------------------------------------------------------//
  // "InitWSLogOn":                                                          //
  //-------------------------------------------------------------------------//
  // No LogOn is required (the ListenKey was sent at SessionInit time), so not-
  // ify the WS LogOn Completion right now:
  //
  void EConnector_WS_KrakenSpot_OMC::InitWSLogOn()
  {
    LOG_INFO(2, "EConnector_WS_KrakenSpot_OMC::InitWSLogOn")
    OMCH2WS::WSLogOnCompleted();

    CancelAllOrders(0, nullptr);
  }

  //=========================================================================//
  // UserStream Timer FD Helpers:                                            //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "SetUSTimer":                                                           //
  //-------------------------------------------------------------------------//
  inline void EConnector_WS_KrakenSpot_OMC::SetUSTimer()
  {
    // Just in case, first remove the Timer if it exists:
    RemoveUSTimer();
    assert(m_usTimerFD < 0);

    // TimerHandler: Calls "RequestListenKey" periodically:
    IO::FDInfo::TimerHandler timerH
    (
      [this](int DEBUG_ONLY(a_fd))->void
      {
        assert(a_fd == m_usTimerFD);
        this->RequestListenKey();
      }
    );

    // ErrorHandler:
    IO::FDInfo::ErrHandler   errH
    (
      [this](int a_fd,   int   a_err_code, uint32_t  a_events,
             char const* a_msg) -> void
      { this->USTimerErrHandler(a_fd, a_err_code, a_events, a_msg); }
    );

    // Period: 30 min = 1800 sec = 1800000 msec:
    constexpr uint32_t USTimerPeriodMSec = 300'000;

    // Create the TimerFD and add it to the Reactor:
    char const* timerName = IsFut ? "USTimerFUT" : "USTimerSPT";
    m_usTimerFD =
      EConnector::m_reactor->AddTimer
      (
        timerName,
        2000,               // Initial offset
        USTimerPeriodMSec,  // Period
        timerH,
        errH
      );
  }

  //-------------------------------------------------------------------------//
  // "RemoveUSTimer":                                                        //
  //-------------------------------------------------------------------------//
  inline void EConnector_WS_KrakenSpot_OMC::RemoveUSTimer()
  {
    if (m_usTimerFD >= 0)
    {
      if (utxx::likely(EConnector::m_reactor != nullptr))
        EConnector::m_reactor->Remove(m_usTimerFD);

      // Anyway, close the socket:
      (void) close(m_usTimerFD);
      m_usTimerFD = -1;
    }
  }

  //-------------------------------------------------------------------------//
  // "USTimerErrHandler":                                                    //
  //-------------------------------------------------------------------------//
  inline void EConnector_WS_KrakenSpot_OMC::USTimerErrHandler
    (int a_fd, int a_err_code, uint32_t a_events, char const* a_msg)
  {
    // There should be NO errors associated with this Timer at all. If one occ-
    // urs, we better shut down completely:
    assert(a_fd == m_usTimerFD);
    LOG_ERROR(1,
      "EConnector_WS_KrakenSpot_OMC::USTimerErrHandler: TimerFD={}, ErrCode={}, "
      "Events={}, Msg={}: STOPPING",
      a_fd, a_err_code, EConnector::m_reactor->EPollEventsToStr(a_events),
      (a_msg != nullptr) ? a_msg : "")
    Stop();
  }
}
// End namespace MAQUETTE
