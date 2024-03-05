// vim:ts=2:et
//===========================================================================//
//         "Connectors/H2WS/Binance/EConnector_H2WS_Binance_OMC.cpp":        //
//                         H2WS-Based OMC for Binance                        //
//===========================================================================//
#include "Connectors/H2WS/Binance/EConnector_H2WS_Binance_OMC.h"
#include "Connectors/H2WS/Binance/Sender_OMC.hpp"
#include "Connectors/H2WS/Binance/Processor_OMC.hpp"
#include "Venues/Binance/SecDefs.h"
#include "Venues/Binance/Configs_H2WS.h"
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
  template <Binance::AssetClassT::type AC>
  std::vector<SecDefS> const* EConnector_H2WS_Binance_OMC<AC>::GetSecDefs()
    { return &Binance::GetSecDefs(AC); }

  //=========================================================================//
  // Config and Account Mgmt:                                                //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "GetWSConfig":                                                          //
  //-------------------------------------------------------------------------//
  template <Binance::AssetClassT::type AC>
  H2WSConfig const& EConnector_H2WS_Binance_OMC<AC>::GetWSConfig
  (
    MQTEnvT UNUSED_PARAM(a_env)
  )
  {
    auto cit = Binance::Configs_WS_OMC.find(AC);
    if (cit == Binance::Configs_WS_OMC.cend())
      throw utxx::runtime_error("Binance OMC: WS Config not found.");
    return cit->second;
  }

  //-------------------------------------------------------------------------//
  // "GetH2Config":                                                          //
  //-------------------------------------------------------------------------//
  template <Binance::AssetClassT::type AC>
  H2WSConfig const& EConnector_H2WS_Binance_OMC<AC>::GetH2Config(MQTEnvT a_env)
  {
    auto cit = Binance::Configs_H2_OMC.find(std::make_pair(AC, a_env));
    if (cit == Binance::Configs_H2_OMC.cend())
      throw utxx::runtime_error("Binance OMC: H2 Config not found.");
    return cit->second;
  }

  //=========================================================================//
  // Non-Default Ctor:                                                       //
  //=========================================================================//
  template <Binance::AssetClassT::type AC>
  EConnector_H2WS_Binance_OMC<AC>::EConnector_H2WS_Binance_OMC
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
      "Binance",
      0,                            // XXX: No extra ShM data at the moment...
      a_reactor,
      false,                        // No need fora BusyWait
      a_sec_defs_mgr,
      Binance::GetSecDefs(AC),
      a_only_symbols,               // Restricting full Binnace list
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
    OMCH2WS(a_params),
    //-----------------------------------------------------------------------//
    // This Class:                                                           //
    //-----------------------------------------------------------------------//
    m_userStreamStr    (),
    m_path             (),
    m_signSHA256       (),
    m_signHMAC         { m_signSHA256, uint8_t(sizeof(m_signSHA256)) },
    m_usTimerFD        (-1)
  {
    //-----------------------------------------------------------------------//
    // Out-Bound Headers:                                                    //
    //-----------------------------------------------------------------------//
    // [0]: ":authoriry" (created by Parent Ctor)
    // [1]: ":scheme"    (ditto)
    // [2]: ":method"    (ditto, name only)
    // [3]: ":path"      (ditto, name only)
    // [4]: "a-mbx-apikey":
    //
    // At this point, there are no Active "H2Conn"s, so install Headers for the
    // NotActive ones:
    assert(OMCH2WS::m_activeH2Conns.empty());

    for (auto h2conn: OMCH2WS::m_notActvH2Conns)
    {
      auto& outHdrs       = h2conn->m_outHdrs;   // Ref!
      outHdrs[4].name     = const_cast<uint8_t*>
                            (reinterpret_cast<uint8_t const*>("x-mbx-apikey"));
      outHdrs[4].namelen  = 12;
      outHdrs[4].value    = const_cast<uint8_t*>
                            (reinterpret_cast<uint8_t const*>
                            (OMCH2WS::m_account.m_APIKey.data()));
      outHdrs[4].valuelen =  OMCH2WS::m_account.m_APIKey.size();
    }
    // The following assert ensures that our static SHA256 sizes are correct:
    assert(gnutls_hmac_get_len(GNUTLS_MAC_SHA256) == sizeof(m_signSHA256));
  }

  //=========================================================================//
  // Dtor, Starting and Stopping:                                            //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // Dtor:                                                                   //
  //-------------------------------------------------------------------------//
  template <Binance::AssetClassT::type AC>
  EConnector_H2WS_Binance_OMC<AC>::~EConnector_H2WS_Binance_OMC() noexcept
  {
    // Remove the UserStream Timer, catching any possible exns:
    try  { RemoveUSTimer(); }
    catch(...){}
  }

  //-------------------------------------------------------------------------//
  // "Start":                                                                //
  //-------------------------------------------------------------------------//
  template <Binance::AssetClassT::type AC>
  void EConnector_H2WS_Binance_OMC<AC>::Start()
  {
    LOG_INFO(2, "EConnector_H2WS_Binance_OMC::Start: Starting Up")
    OMCH2WS::Start();
  }

  //-------------------------------------------------------------------------//
  // "Stop":                                                                 //
  //-------------------------------------------------------------------------//
  template <Binance::AssetClassT::type AC>
  void EConnector_H2WS_Binance_OMC<AC>::Stop()
  {
    LOG_INFO(2, "EConnector_H2WS_Binance_OMC::Stop: Stopping")
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
  template <Binance::AssetClassT::type AC>
  void EConnector_H2WS_Binance_OMC<AC>::InitWSSession()
  {
    LOG_INFO(2, "EConnector_H2WS_Binance_OMC::InitWSSession")

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
  template <Binance::AssetClassT::type AC>
  void EConnector_H2WS_Binance_OMC<AC>::InitWSLogOn()
  {
    LOG_INFO(2, "EConnector_H2WS_Binance_OMC::InitWSLogOn")
    OMCH2WS::WSLogOnCompleted();

    CancelAllOrders(0, nullptr);
  }

  //-------------------------------------------------------------------------//
  // "OnTurningActive":                                                      //
  //-------------------------------------------------------------------------//
  template <Binance::AssetClassT::type AC>
  void EConnector_H2WS_Binance_OMC<AC>::OnTurningActive()
  {
    LOG_INFO(2, "EConnector_H2WS_Binance_OMC::OnTurningActive")
  }

  //=========================================================================//
  // UserStream Timer FD Helpers:                                            //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "SetUSTimer":                                                           //
  //-------------------------------------------------------------------------//
  template <Binance::AssetClassT::type AC>
  inline void EConnector_H2WS_Binance_OMC<AC>::SetUSTimer()
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
  template <Binance::AssetClassT::type AC>
  inline void EConnector_H2WS_Binance_OMC<AC>::RemoveUSTimer()
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
  template <Binance::AssetClassT::type AC>
  inline void EConnector_H2WS_Binance_OMC<AC>::USTimerErrHandler
    (int a_fd, int a_err_code, uint32_t a_events, char const* a_msg)
  {
    // There should be NO errors associated with this Timer at all. If one occ-
    // urs, we better shut down completely:
    assert(a_fd == m_usTimerFD);
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
  template class EConnector_H2WS_Binance_OMC<Binance::AssetClassT::Spt>;
  template class EConnector_H2WS_Binance_OMC<Binance::AssetClassT::FutT>;
  template class EConnector_H2WS_Binance_OMC<Binance::AssetClassT::FutC>;
}
// End namespace MAQUETTE
