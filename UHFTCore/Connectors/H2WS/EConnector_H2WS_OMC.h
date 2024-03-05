// vim:ts=2:et
//===========================================================================//
//                   "Connectors/H2WS/EConnector_H2WS_OMC.h":                //
//             Framework for OMCs with TLS-HTTP2-WS as the Transport         //
//===========================================================================//
// Normally, HTTP/2 (H2) is used for sending Orders, whereas WebSockets (WS) is
// used for receiving asynchronous responses (eg ExecReports):
//
#pragma once

#include "Connectors/H2WS/EConnector_WS_OMC.h"
#include "Connectors/H2WS/EConnector_WS_OMC.hpp"
#include "Connectors/H2WS/H2Connector.h"
#include "Connectors/H2WS/H2Connector.hpp"
#include "Connectors/H2WS/Configs.hpp"
#include <boost/container/static_vector.hpp>

namespace MAQUETTE
{
  //=========================================================================//
  // "EConnector_H2WS_OMC":                                                  //
  //=========================================================================//
  // NB:
  // (*) "Derived" is the actual Protocol-specific OMC;
  // (*) "H2Conn"  is some "H2Connector" or its extension (which may also cont-
  //               ain some Protocol-specific features);
  // (*) Application-level protocol is usually some JSON fmt over TLS-H2 (order
  //     placement) and TLS-WS (receiving order status updates);
  // (*) EConnector_H2WS_OMC INHERITS from TCP_Connector via EConnector_WS_OMC,
  //     and CONTAINS another TCP_Connector whih implements the H2 leg (via the
  //     H2Conn):
  //
  template<typename Derived, typename H2Conn>
  class    EConnector_H2WS_OMC:
    public EConnector_WS_OMC<Derived>
  {
  protected:
    //=======================================================================//
    // Types:                                                                //
    //=======================================================================//
    // NB: The parent classes need to be made "friend"s, as they invoke private
    // methods of this class via template APIs:
    //
    using  TCPWS  = TCP_Connector     <Derived>;
    using  OMCWS  = EConnector_WS_OMC <Derived>;
    friend class    EConnector_WS_OMC <Derived>;
    friend class    EConnector_OrdMgmt;

    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    // Account Info:
    H2WSAccount const     m_account;

    // NB: Multiple "H2Connector"s per OMC may be used.  This allows us to send
    // packets via different IPs, to mitigate the problem of rate limits.  With
    // each SendReq, we choose a rotated curr "H2Connector".
    // Furthermore,  we maintain the lists of both Active  (and so rotated) and
    // NotActive "H2Connector"s. NB: NotActive == !Active,   it does  NOT imply
    // InActive  (because there are other !Active states, eg Starting etc):
    //
    constexpr static unsigned  MaxH2Conns = 20;
    unsigned              m_numH2Conns;   // Must be <= MaxH2Conns
    mutable boost::container::static_vector<H2Conn*, MaxH2Conns>
                          m_activeH2Conns;
    mutable boost::container::static_vector<H2Conn*, MaxH2Conns>
                          m_notActvH2Conns;
    // Rotation of Active "H2Conn"s:
    mutable int           m_currH2Idx;    // Rotating idx
    mutable H2Conn*       m_currH2Conn;   // Rotating ptr

    //=======================================================================//
    // Non-Default Ctor, Dtor:                                               //
    //=======================================================================//
    EConnector_H2WS_OMC(boost::property_tree::ptree const&  a_params);

    // Dtor:
    virtual ~EConnector_H2WS_OMC() noexcept override;

  public:
    //=======================================================================//
    // Virtual Common "EConnector" Methods:                                  //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "Start", "Stop", Properties:                                          //
    //-----------------------------------------------------------------------//
    virtual void Start()            override;
    virtual void Stop ()            override;

    // NB: "IsActive" is NOT "virtual" anymore:
    bool IsActive()           const override;

    //  "StopNow":
    template<bool FullStop>
    void StopNow(char const* a_where, utxx::time_val a_ts_recv);

    // NB: "Subscribe", "UnSubscribe" and "UnSubscribeAll" are inherited from
    // "EConnector_OrdMgmt"
    //
    // Methods to be provided by "Derived":
    // (*) InitSession
    // (*) InitLogOn
    // (*) ProcessH2Msg
    //
    // XXX: Currently, "InitGracefulStop" is provided by "EConnector_WS_OMC".
    // It invokes "CancelAllOrders"...

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
    //-----------------------------------------------------------------------//
    // H2 Conns Rotation / Status:                                           //
    //-----------------------------------------------------------------------//
    void RotateH2Conns        ()                     const;
    void CheckActiveH2Conns   ()                     const;
    void CheckNotActiveH2Conns()                     const;
    void MoveH2ConnToActive   (H2Conn*     a_h2conn) const;
    void MoveH2ConnToNotActive(H2Conn*     a_h2conn) const;
    void CheckH2ConnsIntegrity(char const* a_where)  const;
  };
} // End namespace MAQUETTE
