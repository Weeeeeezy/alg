// vim:ts=2:et
//===========================================================================//
//                    "Connectors/H2WS/EConnector_WS_MDC.h":                 //
//           Framework for MDCs with TLS-WebSocket as the Transport          //
//===========================================================================//
#pragma once

#include "Connectors/EConnector_MktData.h"
#include "Connectors/TCP_Connector.h"
#include "Connectors/H2WS/Configs.hpp"
#include "Protocols/H2WS/WSProtoEngine.h"
#include <vector>

namespace MAQUETTE
{
  //=========================================================================//
  // "EConnector_WS_MDC":                                                    //
  //=========================================================================//
  // NB:
  // (*) "Derived" is the actual Protocol-specific MDC;
  // (*) Application-level protocol is usually some JSON fmt over TLS-WS;
  // (*) This architecture is somewhat similar to   "EConnector_FIX":
  //
  template<typename Derived>
  class    EConnector_WS_MDC:
    public EConnector_MktData,
    public H2WS::WSProtoEngine<Derived>,
    public TCP_Connector  <Derived>
    // NB: It must indeed be  TCP_Connector<Derived>,
    //                    NOT TCP_Connector<EConnector_WS_MDC<Derived>>,
    // because some Call-Backs required by TCP_Connector are defined in the
    // ultimate "Derived"!
  {
  protected:
    //=======================================================================//
    // Types and Consts:                                                     //
    //=======================================================================//
    constexpr static bool IsOMC = false;
    constexpr static bool IsMDC = true;

    using  ProWS = H2WS::WSProtoEngine<Derived>;
    friend class   H2WS::WSProtoEngine<Derived>;

    using  TCPWS = TCP_Connector<Derived>;
    friend class   TCP_Connector<Derived>;

    //=======================================================================//
    // Non-Default Ctor, Dtor:                                               //
    //=======================================================================//
    EConnector_WS_MDC
    (
      EConnector_MktData*                 a_primary_mdc, // Usually NULL
      boost::property_tree::ptree const&  a_params
    );

    // The Dtor must still be "virtual" -- this class is NOT "final" yet:
    virtual ~EConnector_WS_MDC()  noexcept override;

  public:
    //=======================================================================//
    // Virtual Common "EConnector" Methods:                                  //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "Start", "Stop", Properties:                                          //
    //-----------------------------------------------------------------------//
    virtual void Start()          override;
    virtual void Stop ()          override;
    virtual bool IsActive() const override;

    // "Subscribe", "UnSubscribe", "UnSubscribeAll" and "SubscribeMktData" are
    // inherited from "EConnector_MktData"

  protected:
    //======================================================================//
    // Call-Backs required by "TCP_Connector":                              //
    //======================================================================//
    //  "SendHeartBeat": Sends "Ping" to the Server:
    void SendHeartBeat() const;

    // "ServerInactivityTimeOut":
    void ServerInactivityTimeOut     (utxx::time_val a_ts_recv);

    //  "StopNow":
    template<bool FullStop>
    void StopNow(char const* a_where, utxx::time_val a_ts_recv);

    // "InitGracefulStop": For an MDC, it is probably a NoOp: We do not send
    // any explicit "UnSubscribe" msgs to the Server:
    void InitGracefulStop() const {}

    //  "GracefulStopInProgress": Sends "Close" to the Server:
    void GracefulStopInProgress() const;

    // NB: the remaining "TCP_Connector" call-backs are Dialect-specific, so
    // implemented in the "Derived" class:
    // (*) "InitSession"
    // (*) "InitLogOn" (LogOn and/or Protocol-level MktData subscription, if
    //                  the latter was not encoded in HTTP GET)
    // NB: "ReadHandler" is implemented in "Proto"

    // "LogOnCompleted": Notifies the Strategies:
    void LogOnCompleted              (utxx::time_val a_ts_recv);

    //=======================================================================//
    // "H2WS::WSProtoEngine" Call-Backs:                                     //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "SessionInitCompleted":                                               //
    //-----------------------------------------------------------------------//
    // NB: Will perform Call-Fwd into "TCP_Connector":
    //
    void SessionInitCompleted        (utxx::time_val a_ts_recv);

    // NB: Another "H2WS::WSProtoEngine" Call-Back is:
    // "ProcessWSMsg", to be implemented by "Derived"

    void InitSession()
    { static_cast<Derived*>(this)->Derived::InitWSSession(); }

    void InitLogOn()
    { static_cast<Derived*>(this)->Derived::InitWSLogOn(); }

    //=======================================================================//
    // Protocol-Level Logging:                                               //
    //=======================================================================//
    // Log the Msg content (presumably JSON or similar) in ASCII.  NB: The int
    // val is considered to be part of the comment, so is logged only if it is
    // non-0 and the comment is non-NULL and non-empty:
    //
    template <bool IsSend = true>
    void LogMsg
    (
      char const* a_msg,
      char const* a_comment = nullptr,
      long        a_val     = 0
    )
    const;
  };
} // End namespace MAQUETTE
