// vim:ts=2:et
//===========================================================================//
//                 "Connectors/WS/EConnector_WS_MDC.hpp":                    //
//===========================================================================//
#pragma once

#include "Connectors/H2WS/EConnector_WS_MDC.h"
#include "Connectors/EConnector_MktData.hpp"
#include "Connectors/H2WS/EConnector_WS_OMC.hpp" // For possible MDC-OMC Links
#include "Protocols/H2WS/WSProtoEngine.hpp"
#include "Connectors/TCP_Connector.hpp"
#include <type_traits>

namespace MAQUETTE
{
  //=========================================================================//
  // Non-Default Ctor:                                                       //
  //=========================================================================//
  template<typename Derived>
  inline EConnector_WS_MDC<Derived>::EConnector_WS_MDC
  (
    EConnector_MktData*                 a_primary_mdc,
    boost::property_tree::ptree const&  a_params
  )
  : //-----------------------------------------------------------------------//
    // "EConnector_MKtData":                                                 //
    //-----------------------------------------------------------------------//
    EConnector_MktData
    (
      true,                                // Yes, this MDCC is Enabled!
      a_primary_mdc,
      a_params,
      false,                               // No FullAmt, presumably!
      Derived::IsSparse,                   // Use Sparse OrderBooks?
      Derived::HasSeqNums,
      false,                               // And no RptSeqs  either
      false,                               // So  no "continuous" RptSeqs
      Derived::MktDepth,
      // Trades must be enabled both statically and dynamically:
      Derived::HasTrades && a_params.get<bool>("WithTrades"),
      false,                               // Trades are NOT from FOL normally
      false                                // No  DynInitMode for TCP
    ),
    //-----------------------------------------------------------------------//
    // "H2WS::WSProtoEngine":                                                //
    //-----------------------------------------------------------------------//
    ProWS::WSProtoEngine(static_cast<Derived*>(this)),

    //-----------------------------------------------------------------------//
    // "TCP_Connector":                                                      //
    //-----------------------------------------------------------------------//
    // NB: Derived::GetWSConfig(MQTEnvT) must return a "H2WSConfig" obj. Note
    // that ClientIP is not part of the WSConfig:
    TCPWS
    (
      EConnector::m_name,                  // NB: Available at this point!
      GetH2WSIP(Derived::GetWSConfig(m_cenv), a_params),
      Derived::GetWSConfig(m_cenv).m_Port, // HTTPS port (normally 443)
      65536 * 1024,                        // BuffSz  = 64M (hard-wired)
      65536,                               // BuffLWM = 64k (hard-wired)
      a_params.get<std::string>("ClientIP", ""),   // ClientIP may be empty
      a_params.get<int>        ("MaxConnAttempts", 5),
      10,                                  // LogOn  TimeOut, Sec (hard-wired)
      1,                                   // LogOut TimeOut, Sec (hard-wired)
      1,                                   // Re-Connect Interval, Sec (similar)
      30,                                  // 30-second  periodic  Pings
      true,                                // Yes, use TLS!
      Derived::GetWSConfig(m_cenv).m_HostName,     // Required for TLS
      IO::TLS_ALPN::HTTP11,                        // WS is over HTTP/1.1
      a_params.get<bool>       ("VerifyServerCert",  true),
      a_params.get<std::string>("ServerCAFiles",     ""),
      a_params.get<std::string>("ClientCertFile",    ""),
      a_params.get<std::string>("ClientPrivKeyFile", ""),
      a_params.get<std::string>("ProtoLogFile",      "")
    )
  {}

  //=========================================================================//
  // Dtor is Trivial:                                                        //
  //=========================================================================//
  template<typename Derived>
  inline EConnector_WS_MDC<Derived>::~EConnector_WS_MDC() noexcept {}

  //=========================================================================//
  // Starting, Stopping and some "TCP_Connector" Call-Backs:                 //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "Start":                                                                //
  //-------------------------------------------------------------------------//
  // It only initiates the TCP connection -- no msgs are sent at this time (the
  // latter is done by "InitSession"):
  //
  template<typename Derived>
  inline void EConnector_WS_MDC<Derived>::Start()
  {
    // In case if it is a RE-Start, do a clean-up first (incl the subscr info):
    EConnector_MktData::ResetOrderBooksAndOrders();

    // Then start the "TCP_Connector" (which will really initiate a TCP Conn):
    TCPWS::Start();

    // XXX: There might be some "Derived"-specific Start procedures, but they
    // can be implemented in Derived::InitSession -- probably no need for yet
    // another call-back (eg Derived::Start)
  }

  //-------------------------------------------------------------------------//
  // "IsActive":                                                             //
  //-------------------------------------------------------------------------//
  template<typename Derived>
  inline bool EConnector_WS_MDC<Derived>::IsActive() const
    { return TCPWS::IsActive(); }

  //-------------------------------------------------------------------------//
  // "SessionInitCompleted":                                                 //
  //-------------------------------------------------------------------------//
  template<typename Derived>
  inline void EConnector_WS_MDC<Derived>::SessionInitCompleted
    (utxx::time_val UNUSED_PARAM(a_ts_recv))
  {
    // HTTP/WS Session Init is done. Notify the "TCP_Connector" about that:
    TCPWS::SessionInitCompleted();
  }

  //-------------------------------------------------------------------------//
  // "LogOnCompleted":                                                       //
  //-------------------------------------------------------------------------//
  template<typename Derived>
  inline void EConnector_WS_MDC<Derived>::LogOnCompleted
    (utxx::time_val a_ts_recv)
  {
    // Signal LogOn Completion at this point, this will cancel the LogOn Timer:
    TCPWS::LogOnCompleted();

    // Therefore, we should be Active now:
    assert(this->IsActive());

    // Notify the Strategies that the Connector is now on-line:
    EConnector::ProcessStartStop(true, nullptr, 0, a_ts_recv, a_ts_recv);

    LOG_INFO(1,
      "EConnector_WS_MDC::LogOnCompleted: Connector is now ACTIVE")
  }

  //-------------------------------------------------------------------------//
  // "SendHeartBeat":                                                        //
  //-------------------------------------------------------------------------//
  template<typename Derived>
  inline void EConnector_WS_MDC<Derived>::SendHeartBeat() const
    { (void) TCPWS::SendImpl(ProWS::PingMsg, sizeof(ProWS::PingMsg)-1); }

  //-------------------------------------------------------------------------//
  // "Stop" (graceful, external, virtual):                                   //
  //-------------------------------------------------------------------------//
  template<typename Derived>
  inline void EConnector_WS_MDC<Derived>::Stop()
  {
    // Try to FullStop the "TCP_Connector". The latter will invoke the call-
    // backs: "InitGracefulStop" and "GracefulStopInProgress":
    //
    (void) TCPWS::template Stop<true>();
  }

  //-------------------------------------------------------------------------//
  // "StopNow":                                                              //
  //-------------------------------------------------------------------------//
  template<typename Derived>
  template<bool     FullStop>
  inline void EConnector_WS_MDC<Derived>::StopNow
    (char const* a_where, utxx::time_val a_ts_recv)
  {
    assert(a_where != nullptr);

    // Stop the "TCP_Connector". It will become "InActive":
    TCPWS::template DisConnect<FullStop>(a_where);
    assert(TCPWS::IsInactive());

    // For safety, once again invalidate the OrderBooks (because "StopNow" is
    // not always called sfter "Stop"), incl the subscr info:
    EConnector_MktData::ResetOrderBooksAndOrders();

    // Notify the Strategies (On=false):
    EConnector::ProcessStartStop(false, nullptr, 0, a_ts_recv, a_ts_recv);

    LOG_INFO(1,
      "EConnector_WS_MDC: Connector STOPPED{}",
      FullStop ? "." : ", but will re-start")
  }

  //-------------------------------------------------------------------------//
  // "ServerInactivityTimeOut":                                              //
  //-------------------------------------------------------------------------//
  template<typename Derived>
  inline void EConnector_WS_MDC<Derived>::ServerInactivityTimeOut
    (utxx::time_val a_ts_recv)
  {
    // Stop immediately, but will try to re-start (FullStop=false):
    StopNow<false>("ServerInactivityTimeOut", a_ts_recv);
  }

  //-------------------------------------------------------------------------//
  // "GracefulStopInProgress":                                               //
  //-------------------------------------------------------------------------//
  template<typename Derived>
  inline void EConnector_WS_MDC<Derived>::GracefulStopInProgress() const
  {
    // Send a "CLOSE" msg to the Server:
    (void) TCPWS::SendImpl(ProWS::CloseMsg, sizeof(ProWS::CloseMsg)-1);
  }

  //=========================================================================//
  // Protocol-Level Logging:                                                 //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "LogMsg":                                                               //
  //-------------------------------------------------------------------------//
  // Logs the Msg content (presumably JSON or similar) in ASCII:
  //
  template<typename Derived>
  template<bool     IsSend>
  inline void EConnector_WS_MDC<Derived>::LogMsg
    (char const* a_msg, char const* a_comment, long a_val) const
  {
    if (TCPWS::m_protoLogger != nullptr)
    {
      // The comment and the int param may or may not be present. The latter,
      // if present, is part of the comment:
      if (a_comment != nullptr && *a_comment != '\0')
      {
        if (utxx::likely(a_val == 0))
          TCPWS::m_protoLogger->info
            ("{} {} {}",   a_comment,        IsSend ? "-->" : "<==", a_msg);
        else
          TCPWS::m_protoLogger->info
            ("{}{} {} {}", a_comment, a_val, IsSend ? "-->" : "<==", a_msg);
      }
      else
        TCPWS::m_protoLogger->info("{} {}",  IsSend ? "-->" : "<==", a_msg);

      // XXX: Flushing the logger may be expensive, but will ensure that import-
      // ant msgs are not lost in case of a crash:
      TCPWS::m_protoLogger->flush();
    }
  }
}
// End namespace MAQUETTE
