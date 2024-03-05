// vim:ts=2:et
//===========================================================================//
//                   "Connectors/H2WS/EConnector_H2WS_MDC.h":                //
//             Framework for MDCs with TLS-HTTP2-WS as the Transport         //
//===========================================================================//
// Normally, HTTP/2 (H2) is used for sending Orders, whereas WebSockets (WS) is
// used for receiving asynchronous responses (eg ExecReports):
//
#pragma once

#include "Connectors/H2WS/EConnector_WS_MDC.h"
#include "Connectors/H2WS/EConnector_WS_MDC.hpp"
#include "Connectors/H2WS/H2Connector.h"
#include "Connectors/H2WS/H2Connector.hpp"
#include "Connectors/H2WS/Configs.hpp"
#include <cstdint>
#include <cstddef>

namespace MAQUETTE
{
  //=========================================================================//
  // "EConnector_H2WS_MDC":                                                  //
  //=========================================================================//
  // NB:
  // (*) "Derived" is the actual Protocol-specific MDC;
  // (*) "H2Conn"  is "H2Connector" or its extension; the latter may also cont-
  //     ain some Protocol-specific features;
  // (*) Application-level protocol is usually some JSON fmt over TLS-H2 (order
  //     placement) and TLS-WS (receiving order status updates)
  // (*) XXX: Unlike "EConnector_WS_MDC", there is currently no separate Proto-
  //     Engine class for H2
  // (*) EConnector_H2WS_MDC INHERITS from TCP_Connector via EConnector_WS_MDC,
  //     and CONTAINS another TCP_Connectors whih implements the H2 leg
  //
  template<typename Derived, typename H2Conn>
  class    EConnector_H2WS_MDC:
    public EConnector_WS_MDC<Derived>
  {
  protected:
    //=======================================================================//
    // Types:                                                                //
    //=======================================================================//
    // NB: The parent classes need to be made "friend"s, as they invoke private
    // methods of this class via template APIs:
    //
    using  TCPWS = TCP_Connector    <Derived>;
    using  MDCWS = EConnector_WS_MDC<Derived>;
    friend class   EConnector_WS_MDC<Derived>;
    friend class   EConnector_MktData;

    //=======================================================================//
    // The TCP_Connector for the H2 Leg:                                     //
    //=======================================================================//
    // NB: Multiple "H2Connector"s per OMC may ne used. This allows us to send
    // packets via different IPs, to mitigate the problem of rate limits. With
    // each SendReq, we choose a rotated curr "H2Connector":
    //
    constexpr static int  MaxH2Conns = 20;
    boost::container::static_vector<H2Conn*, MaxH2Conns>
                          m_h2conns;
    // Rotation of "H2Conn"s:
    mutable int           m_currH2Idx;    // Rotating idx
    mutable H2Conn*       m_currH2Conn;   // Rotating ptr

    // XXX: The following is a stub only, required by "H2Connector": NULL:
    boost::container::static_vector<Req12*, 1>*
                          m_allInds;

    //=======================================================================//
    // Non-Default Ctor, Dtor:                                               //
    //=======================================================================//
    EConnector_H2WS_MDC
    (
      EConnector_MktData*                 a_primary_mdc,  // Usually NULL
      boost::property_tree::ptree const&  a_params
    );

    // Dtor (not virtual anymore):
    ~EConnector_H2WS_MDC() noexcept override;

  public:
     //=======================================================================//
    // Virtual Common "EConnector" Methods:                                  //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "Start", "Stop", Properties:                                          //
    //-----------------------------------------------------------------------//
    virtual void Start()  override;
    virtual void Stop ()  override;

    // NB: "IsActive" is NOT "virtual" anymore:
    bool IsActive() const override;

    //  "StopNow":
    template<bool FullStop>
    void StopNow(char const* a_where, utxx::time_val a_ts_recv);

    // Methods to be provided by "Derived":
    // (*) InitSession
    // (*) InitLogOn
    // (*) ProcessH2Msg
    //
    // XXX: Currently, "InitGracefulStop" is provided by "EConnector_WS_MDC".

  public:
    // FIXME: Do all these methods really need to be "public"?
    //=======================================================================//
    // H2 and WS Session Mgmt:                                               //
    //=======================================================================//
    void WSLogOnCompleted ();
    void H2LogOnCompleted (H2Conn const*  a_h2conn);
    void H2WSTurnedActive ();

    template<bool FullStop>
    void H2ConnStopped(H2Conn const* a_h2conn, char const* a_where);

    // (*) "OnRstStream" and "OnHeaders" (required by "H2Connector") are deleg-
    //     ated to the "Derived"
    // (*) "OnTurningActive": to be provided by "Derived" as well

  protected:
    // H2 Rotation / Status:
    void RotateH2Conns   () const;
  };
} // End namespace MAQUETTE
