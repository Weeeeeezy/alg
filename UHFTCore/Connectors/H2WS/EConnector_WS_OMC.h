// vim:ts=2:et
//===========================================================================//
//                     "Connectors/H2WS/EConnector_WS_OMC.h":                //
//            Framework for OMCs with TLS-WebSocket as the Transport         //
//===========================================================================//
#pragma once

#include "Connectors/EConnector_OrdMgmt.h"
#include "Connectors/TCP_Connector.h"
#include "Connectors/H2WS/Configs.hpp"
#include "Protocols/H2WS/WSProtoEngine.h"

namespace MAQUETTE
{
  //=========================================================================//
  // "EConnector_WS_OMC":                                                    //
  //=========================================================================//
  // NB:
  // (*) "Derived" is the actual Protocol-specific OMC;
  // (*) Application-level protocol is usually some JSON fmt over TLS-WS;
  //
  template<typename Derived>
  class EConnector_WS_OMC:
    public EConnector_OrdMgmt,
    public H2WS::WSProtoEngine<Derived>,
    public TCP_Connector      <Derived>
    // NB: It must indeed be  TCP_Connector<Derived>,
    //                    NOT TCP_Connector<EConnector_WS_OMC<Derived>>,
    // because some Call-Backs required by TCP_Connector are defined in the
    // ultimate "Derived"!
  {
  protected:
    //=======================================================================//
    // Types and Consts:                                                     //
    //=======================================================================//
    constexpr static bool IsOMC = true;
    constexpr static bool IsMDC = false;

    using  ProWS = H2WS::WSProtoEngine<Derived>;
    friend class   H2WS::WSProtoEngine<Derived>;

    using  TCPWS = TCP_Connector<Derived>;
    friend class   TCP_Connector<Derived>;

    // XXX: We assume that "BatchSend" functionality (buffering several raw msgs
    // and sending them out at once) is never available in WS or H2WS OMCs,  due
    // to the structure of H2 and WS Frames, and the presence of TLS:
    //
    constexpr static bool HasBatchSend = false;

    //=======================================================================//
    // Non-Default Ctor, Dtor:                                               //
    //=======================================================================//
    EConnector_WS_OMC(boost::property_tree::ptree const&  a_params);

    // The Dtor must still be "virtual" -- this class is NOT "final" yet:
    virtual ~EConnector_WS_OMC()  noexcept override;

    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    // Account Info:
    H2WSAccount const  m_account;

    // NB: "m_[TR]xSN" (pointing to ShM) are provided by "EConnector_OrdMgmt";
    // but as this class provides  SessionMgmt functionality  required  by the
    // ProtoEngine, we also need to provide similar ptrs "m_[tx]SN" here:
    SeqNum*            m_rxSN;
    SeqNum*            m_txSN;

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

    // NB: "Subscribe", "UnSubscribe" and "UnSubscribeAll" are inherited from
    // "EConnector_OrdMgmt"

protected:
    //=======================================================================//
    // Call-Backs/Forwards for the static interface of "TCP_Connector":      //
    //=======================================================================//
    //  "SendHeartBeat": It might be a good idea to send periodic  "Ping" msgs
    //  to the Server to keep the session alive (in addition to TCP-level Keep-
    //  Alive in "TCP_Connector"):
    void SendHeartBeat() const;

    //  "ServerInactivityTimeOut":
    void ServerInactivityTimeOut     (utxx::time_val a_ts_recv);

    //  "StopNow":
    template<bool FullStop>
    void StopNow(char const* a_where, utxx::time_val a_ts_recv);

    //  "SessionInitCompleted":
    void SessionInitCompleted        (utxx::time_val a_ts_recv);

    //  "LogOnCompleted": Notifies the Strategies that the Connector is ACTIVE:
    void LogOnCompleted              (utxx::time_val a_ts_recv);

    //  "InitGracefulStop": In particular, Cancel all Active Orders:
    void InitGracefulStop();

    //  "GracefulStopInProgress": Sends "Close" to the Server:
    void GracefulStopInProgress() const;

    // NB: the remaining "TCP_Connector" call-backs are Dialect-specific, so
    // are forwarded to the "Derived" class:
    //  "InitSession":
    void InitSession()
      { static_cast<Derived*>(this)->Derived::InitWSSession(); }

    // "InitLogOn":
    void InitLogOn  ()
      { static_cast<Derived*>(this)->Derived::InitWSLogOn();   }

    // NB: "ReadHandler" is implemented in "Proto", and invokes
    // Derived::ProcessWSMsg()

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
