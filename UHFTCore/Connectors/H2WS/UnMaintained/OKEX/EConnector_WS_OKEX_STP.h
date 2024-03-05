// vim:ts=2:et
//===========================================================================//
//              "Connectors/H2WS/OKEX/EConnector_WS_OKEX_STP.h":             //
//                           WS-Based STP for OKEX                           //
//===========================================================================//
#pragma once

#include "Connectors/EConnector.h"
#include <utility>

namespace MAQUETTE
{
  //=========================================================================//
  // "EConnector_WS_OKEX_STP" Class:                                        //
  //=========================================================================//
  class EConnector_WS_OKEX_STP final:
    public EConnector,
    public H2WS::WSProtoEngine<EConnector_WS_OKEX_STP>,
    public TCP_Connector      <EConnector_WS_OKEX_STP>
  {
  public:
    //=======================================================================//
    // Types and Consts:                                                     //
    //=======================================================================//
    // The Native Qty type: OKEX uses Fractional QtyA:
    constexpr static QtyTypeT QT   = QtyTypeT::QtyA;
    using                     QR   = double;
    using                     QtyN = Qty<QT,QR>;

  private:
    //=======================================================================//
    // Configuration:                                                        //
    //=======================================================================//
    // "TCP_Connector" and "H2WS::WSProtoEngine" must have full access to this
    // class:
    friend class H2WS::WSProtoEngine<EConnector_WS_OKEX_STP>;
    friend class TCP_Connector      <EConnector_WS_OKEX_STP>;

  public:
    //=======================================================================//
    // Non-Default Ctor, Dtor:                                               //
    //=======================================================================//
    EConnector_WS_OKEX_STP
    (
      EPollReactor*                       a_reactor,       // Non-NULL
      SecDefsMgr*                         a_sec_defs_mgr,  // Non-NULL
      std::vector<std::string> const*     a_only_symbols,  // NULL=UseFullList
      RiskMgr*                            a_risk_mgr,      // Non-NULL
      boost::property_tree::ptree const&  a_params
    );

    // The Dtor is trivial, and non-virtual (because this class is "final"):
    ~EConnector_WS_OKEX_STP() override noexcept {}

  private:
    //=======================================================================//
    // Virtual Common "EConnector" Methods:                                  //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "Start", "Stop", Properties:                                          //
    //-----------------------------------------------------------------------//
    void Start()    override;
    void Stop ()    override;

    // The following is needed for technical reasons (ambiguity resolution in
    // multiple inheritance):
    bool IsActive() const override;

    // NB: "Subscribe", "UnSubscribe", "UnSubscribeAll" are all  inherited from
    // "EConnector", and not actually required (because there are no Strategies
    // subscribing to this STP)...

    //=======================================================================//
    // "H2WS::WSProtoEngine" Call-Backs:                                     //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "ProcessWSMsg":                                                       //
    //-----------------------------------------------------------------------//
    //  Returns "true" to continue receiving msgs, "false" to stop:
    //
    bool ProcessWSMsg
    (
      char const*     a_msg_body,
      int             a_msg_len,
      bool            a_last_msg,
      utxx::time_val  a_ts_recv,
      utxx::time_val  a_ts_handl
    );

    //-----------------------------------------------------------------------//
    // "SessionInitCompleted":                                               //
    //-----------------------------------------------------------------------//
    // NB: Will perform Call-Fwd into "TCP_Connector":
    //
    void SessionInitCompleted        (utxx::time_val a_ts_recv);

    //=======================================================================//
    // "TCP_Connector" Session-Level Call-Backs:                             //
    //=======================================================================//
    //  "InitSession": Performs HTTP connection and WS upgrade:
    void InitSession     ()       const;

    //  "InitLogOn" : For OKEX, signals completion immediately:
    void InitLogOn       ();

    //  "InitGracefulStop": NoOp for an STP -- no Orders to Cancel etc:
    void InitGracefulStop()       const {}

    //  "GracefulStopInProgress": Sends a "Close" Msg:
    void GracefulStopInProgress() const;

    //  "SendHeartBeat": Sends a "Ping" Msg:
    void SendHeartBeat()          const;

    // "ServerInactivityTimeOut":
    void ServerInactivityTimeOut     (utxx::time_val a_ts_recv);

    //  "StopNow":
    template<bool FullStop>
    void StopNow(char const* a_where, utxx::time_val a_ts_recv);
  };
} // End namespace MAQUETTE
