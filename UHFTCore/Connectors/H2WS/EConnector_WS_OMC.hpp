// vim:ts=2:et
//===========================================================================//
//                   "Connectors/H2WS/EConnector_WS_OMC.hpp":                //
//===========================================================================//
#pragma once

#include "Connectors/H2WS/EConnector_WS_OMC.h"
#include "Connectors/EConnector_OrdMgmt.hpp"
#include "Connectors/TCP_Connector.hpp"
#include "Basis/Base64.h"
#include "Protocols/H2WS/WSProtoEngine.hpp"
#include <type_traits>
#include <cstring>
#include <sys/random.h>

namespace MAQUETTE
{
  //=========================================================================//
  // Non-Default Ctor:                                                       //
  //=========================================================================//
  template <typename Derived>
  inline EConnector_WS_OMC<Derived>::EConnector_WS_OMC
  (
    boost::property_tree::ptree const& a_params
  )
  : //-----------------------------------------------------------------------//
    // "EConnector_OrdMgmt":                                                 //
    //-----------------------------------------------------------------------//
    // NB: For PipeLinedReqs, we have a conjunction of Static and Dynamic conf
    // params:
    EConnector_OrdMgmt
    (
      true,    // Yes, OMC is enabled!
      a_params,
      Derived::ThrottlingPeriodSec,
      Derived::MaxReqsPerPeriod,
      Derived::PipeLineMode,
      Derived::SendExchOrdIDs,
      Derived::UseExchOrdIDsMap,
      Derived::HasAtomicModify,
      Derived::HasPartFilledModify,
      Derived::HasExecIDs,
      Derived::HasMktOrders
    ),
    //------------------------------------------------------------------------//
    // "H2WS::WSProtoEngine":                                                 //
    //------------------------------------------------------------------------//
    ProWS::WSProtoEngine(static_cast<Derived*>(this)),

    //------------------------------------------------------------------------//
    // "TCP_Connector":                                                       //
    //------------------------------------------------------------------------//
    TCPWS
    (
      EConnector::m_name,  // NB: Available at this point!
      GetH2WSIP(Derived::GetWSConfig(m_cenv), a_params),
      Derived::GetWSConfig(m_cenv).m_Port,     // HTTPS port (normally 443)
      65536 * 1024,                            // BuffSz  = 64M (hard-wired)
      65536,                                   // BuffLWM = 64k (hard-wired)
      a_params.get<std::string>("ClientIP",   ""),
      a_params.get<int>        ("MaxConnAttempts",   5),
      10,    // LogOn  TimeOut, Sec (hard-wired)
      1,     // LogOut TimeOut, Sec (hard-wired)
      1,     // Re-Connect Interval, Sec (SMALL for WS and H2WS!)
      30,    // 30-second periodic "Ping"s
      true,                                    // Yes, use TLS!
      Derived::GetWSConfig(m_cenv).m_HostName, // Required for TLS
      IO::TLS_ALPN::HTTP11,                    // WS is over HTTP/1.1
      a_params.get<bool>       ("VerifyServerCert",  true),
      a_params.get<std::string>("ServerCAFiles",     ""),
      a_params.get<std::string>("ClientCertFile",    ""),
      a_params.get<std::string>("ClientPrivKeyFile", ""),
      a_params.get<std::string>("ProtoLogFile",      "")
    ),
    //-----------------------------------------------------------------------//
    // "EConnector_WS_OMC" Itself:                                           //
    //-----------------------------------------------------------------------//
    m_account(a_params),
    m_rxSN   (m_RxSN),
    m_txSN   (m_TxSN)
  {
    assert(m_rxSN != nullptr && m_txSN != nullptr);
  }

  //=========================================================================//
  // Dtor is Trivial:                                                        //
  //=========================================================================//
  template<typename Derived>
  inline EConnector_WS_OMC<Derived>::~EConnector_WS_OMC() noexcept {}

  //=========================================================================//
  // Starting, Stopping and some "TCP_Connector" Call-Backs:                 //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "Start":                                                                //
  //-------------------------------------------------------------------------//
  // It only initiates the TCP connection -- no msgs are sent at this time (the
  // latter is done by "Derived::InitLogOn"):
  //
  template <typename Derived>
  inline void EConnector_WS_OMC<Derived>::Start()
  {
    // Start the "TCP_Connector" (which will really initiate a TCP Conn):
    TCPWS::Start();
  }

  //-------------------------------------------------------------------------//
  // "SessionInitCompleted":                                                 //
  //-------------------------------------------------------------------------//
  template <typename Derived>
  inline void EConnector_WS_OMC<Derived>::SessionInitCompleted
    (utxx::time_val UNUSED_PARAM(a_ts_recv))
  {
    // HTTP/WS Session Init is done. Notify the "TCP_Connector" about that. It
    // will invoke "InitLogOn" on us:
    TCPWS::SessionInitCompleted();
  }

  //-------------------------------------------------------------------------//
  // "LogOnCompleted":                                                       //
  //-------------------------------------------------------------------------//
  template <typename Derived>
  inline void
  EConnector_WS_OMC<Derived>::LogOnCompleted(utxx::time_val a_ts_recv)
  {
    // Inform the "TCP_Connector" that the LogOn has been successfully comple-
    // ted. This in particular will cancel the Connect/SessionInit/LogOn Time-
    // Out:
    TCPWS::LogOnCompleted();

    // At this stage, the over-all Connector status becomes "Active":
    assert(IsActive());

    // Notify the Strategies that the Connector is now on-line. XXX:  RecvTime
    // is to be used for ExchTime as well, since we don't have the exact value
    // for the latter:
    EConnector::ProcessStartStop(true, nullptr, 0, a_ts_recv, a_ts_recv);

    LOG_INFO(1, "EConnector_WS_OMC::LogOnCompleted: Connector is now ACTIVE")
  }

  //-------------------------------------------------------------------------//
  // "Stop" (graceful, external, virtual):                                   //
  //-------------------------------------------------------------------------//
  template <typename Derived>
  inline void EConnector_WS_OMC<Derived>::Stop()
  {
    // Try to FullStop the "TCP_Connector". Normally, the Stop is graceful (not
    // immediate), but it could sometimes turn into immediate; FullStop=true.
    // TCP_Connector will call back "InitGracefulStop" and "GracefulStopInProg-
    // ress":
    (void) TCPWS::template Stop<true>();
  }

  //-------------------------------------------------------------------------//
  // "IsActive":                                                             //
  //-------------------------------------------------------------------------//
  template <typename Derived>
  inline bool EConnector_WS_OMC<Derived>::IsActive()   const
    { return TCPWS::IsActive();    }

  //-------------------------------------------------------------------------//
  // "SendHeartBeat":                                                        //
  //-------------------------------------------------------------------------//
  template <typename Derived>
  inline void EConnector_WS_OMC<Derived>::SendHeartBeat() const
  {
    // Send the "PING" msg:
    (void) TCPWS::SendImpl(ProWS::PingMsg, sizeof(ProWS::PingMsg)-1);
  }

  //-------------------------------------------------------------------------//
  // "StopNow":                                                              //
  //-------------------------------------------------------------------------//
  template <typename Derived>
  template <bool FullStop>
  inline void EConnector_WS_OMC<Derived>::StopNow
    (char const* a_where, utxx::time_val a_ts_recv)
  {
    assert(a_where != nullptr);

    // DisConnect the "TCP_Connector", it should become "Inactive":
    TCPWS::template DisConnect<FullStop>(a_where);
    assert(TCPWS::IsInactive());

    // Remove the PusherTask Timer (NoOp if it is not active):
    EConnector_OrdMgmt::RemovePusherTimer();

    // Notify the Strategies (On=false):
    EConnector::ProcessStartStop(false, nullptr, 0, a_ts_recv, a_ts_recv);

    LOG_INFO(1,
      "EConnector_WS_OMC({}): Connector STOPPED{}", a_where,
      FullStop ? "." : ", but will re-start")
  }

  //-------------------------------------------------------------------------//
  // "ServerInactivityTimeOut":                                              //
  //-------------------------------------------------------------------------//
  template <typename Derived>
  inline void EConnector_WS_OMC<Derived>::ServerInactivityTimeOut
    (utxx::time_val a_ts_recv)
  {
    // Stop immediately, but will try to re-start (FullStop=false):
    StopNow<false>("ServerInactivityTimeOut", a_ts_recv);
  }

  //-------------------------------------------------------------------------//
  // "InitGracefulStop":                                                     //
  //-------------------------------------------------------------------------//
  template<typename Derived>
  inline void EConnector_WS_OMC<Derived>::InitGracefulStop()
  {
    // Cancel all active orders (to be safe and polite; we may also have auto-
    // matic Cancelon-Disconnect):
    static_cast<Derived*>(this)->CancelAllOrders
      (0, nullptr, FIX::SideT::UNDEFINED, nullptr);
  }

  //-------------------------------------------------------------------------//
  // "GracefulStopInProgress":                                               //
  //-------------------------------------------------------------------------//
  template<typename Derived>
  inline void EConnector_WS_OMC<Derived>::GracefulStopInProgress() const
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
  inline void EConnector_WS_OMC<Derived>::LogMsg
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
} // End namespace MAQUETTE
