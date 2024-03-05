// vim:ts=2:et
//===========================================================================//
//           "Connectors/H2WS/BitMEX/EConnector_H2WS_BitMEX_OMC.h":          //
//                           H2WS-Based OMC for BitMEX                       //
//===========================================================================//
#pragma once

#include "Connectors/H2WS/EConnector_H2WS_OMC.h"
#include "Connectors/H2WS/Configs.hpp"
#include <utility>

namespace MAQUETTE
{
  //=========================================================================//
  // Struct "H2BitMEXOMC": Extended "H2Connector" (IsOMC=true):              //
  //=========================================================================//
  template<typename PSM>
  struct H2BitMEXOMC final: public H2Connector<true, PSM>
  {
    //-----------------------------------------------------------------------//
    // Data Flds (StreamID Counters):                                        //
    //-----------------------------------------------------------------------//
    // StreamIDs for making various Session-level reqs:
    mutable uint32_t      m_lastPositionsStmID;

    //-----------------------------------------------------------------------//
    // Non-Default Ctor, Dtor:                                               //
    //-----------------------------------------------------------------------//
    H2BitMEXOMC
    (
      PSM*                                a_psm,           // Must be non-NULL
      boost::property_tree::ptree const&  a_params,
      uint32_t                            a_max_reqs,
      std::string                 const&  a_client_ip = ""
    )
    : H2Connector<true, PSM>(a_psm, a_params, a_max_reqs, a_client_ip)

      // XXX: NOT initialising the mutable flds -- that is done dynamically by
      // "OnTurningActive"...
    {}

    template <bool FullStop>
    void Stop()
    {
      LOG_INFO(2, "H2BitMEXOMC::Stop()")
      m_lastPositionsStmID = 0;
      H2Connector<true, PSM>::template Stop<FullStop>();
    }
    // Dtor is auto-generated
  };

  //=========================================================================//
  // "EConnector_H2WS_BitMEX_OMC" Class:                                     //
  //=========================================================================//
  class  EConnector_H2WS_BitMEX_OMC final:
  public EConnector_H2WS_OMC
        <EConnector_H2WS_BitMEX_OMC,
         H2BitMEXOMC<EConnector_H2WS_BitMEX_OMC>> // IsOMC=true
  {
  public:
    constexpr static char const Name[] = "H2WS-BitMEX-OMC";
    //=======================================================================//
    // Types and Consts:                                                     //
    //=======================================================================//
    constexpr static QtyTypeT QT = QtyTypeT::Contracts;
    using QR                     = long;
    using QtyN                   = Qty<QT, QR>;
    using QtyF                   = Qty<QtyTypeT::QtyB, double>; // Fees

  private:
    // This class must be accessible by the parent classes:
    // IsOMC=true:
    friend class     EConnector_OrdMgmt;
    friend class     TCP_Connector      <EConnector_H2WS_BitMEX_OMC>;
    friend struct    H2Connector<true,   EConnector_H2WS_BitMEX_OMC>;
    friend struct    H2BitMEXOMC        <EConnector_H2WS_BitMEX_OMC>;
    friend class     H2WS::WSProtoEngine<EConnector_H2WS_BitMEX_OMC>;
    friend class     EConnector_WS_OMC  <EConnector_H2WS_BitMEX_OMC>;

    friend class     EConnector_H2WS_OMC
                    <EConnector_H2WS_BitMEX_OMC,
                     H2BitMEXOMC       <EConnector_H2WS_BitMEX_OMC>>;

    using  H2Conn  = H2BitMEXOMC        <EConnector_H2WS_BitMEX_OMC>;

    using  OMCH2WS = EConnector_H2WS_OMC
                    <EConnector_H2WS_BitMEX_OMC,
                     H2BitMEXOMC       <EConnector_H2WS_BitMEX_OMC>>;

    //-----------------------------------------------------------------------//
    // Features required by "EConnector_H2WS_OMC":                           //
    //-----------------------------------------------------------------------//
    // Throttling period is 1 min for BitMEX, with 60 Reqs/Period allowed:
    constexpr static int  ThrottlingPeriodSec   = 60;
    constexpr static int  MaxReqsPerPeriod      = 60;
    // Other Features (XXX: CHECK!):
    constexpr static PipeLineModeT PipeLineMode = PipeLineModeT::Wait0;
    constexpr static bool SendExchOrdIDs        = false;
    constexpr static bool UseExchOrdIDsMap      = false;
    constexpr static bool HasAtomicModify       = true;
    constexpr static bool HasPartFilledModify   = true;
    constexpr static bool HasExecIDs            = true;
    constexpr static bool HasMktOrders          = true;
    constexpr static bool HasFreeCancel         = false; // Cancels do NOT count
    constexpr static bool HasNativeMassCancel   = true;

    //-----------------------------------------------------------------------//
    // "Get{WS,H2}Config":                                                   //
    //-----------------------------------------------------------------------//
    static H2WSConfig  const& GetH2Config(MQTEnvT a_env);
    static H2WSConfig  const& GetWSConfig(MQTEnvT a_env);

    int  m_posTimerFD  = -1;
    bool m_posReceived = false;

    // Buffer for WS handshake
    char m_initBuff[2048];

  public:
    static std::vector<SecDefS> const* GetSecDefs();
    //=======================================================================//
    // Non-Default Ctor, Dtor:                                               //
    //=======================================================================//
    EConnector_H2WS_BitMEX_OMC
    (
      EPollReactor*                       a_reactor,
      SecDefsMgr*                         a_sec_defs_mgr,
      std::vector<std::string>    const*  a_only_symbols,  // NULL=UseFullList
      RiskMgr*                            a_risk_mgr,
      boost::property_tree::ptree const&  a_params
    );

    // (*) Dtor is non-virtual (this class is "final"), and is auto-generated

  private:
    //=======================================================================//
    // Session Mgmt:                                                         //
    //=======================================================================//
    // This class is final, so the following method must finally be implemented:
    void EnsureAbstract()   const override {}

    void InitWSSession  ();
    void InitWSLogOn    ();
    void OnTurningActive();

    //=======================================================================//
    // Processing and Sending Msgs:                                          //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // Msg Processors required by "H2Connector":                             //
    //-----------------------------------------------------------------------//
    void OnRstStream
    (
      H2Conn*        a_h2conn,     // Non-NULL
      uint32_t       a_stream_id,
      uint32_t       a_err_code,
      utxx::time_val a_ts_recv
    );

    // Headers may contain err codes that require action
    void OnHeaders
    (
      H2Conn*        a_h2conn,     // Non-NULL
      uint32_t       a_stream_id,
      char const*    a_http_code,
      char const*    a_headers,
      utxx::time_val a_ts_recv
    );

    //-----------------------------------------------------------------------//
    // Msg Processors required by "EConnector_H2WS_OMC":                     //
    //-----------------------------------------------------------------------//
    // Perform processing of HTTP/2- and WebSocket-delivred Msgs. Both methods
    // return "true" to continue and "false" to stop gracefully:
    //
    bool ProcessH2Msg
    (
      H2Conn*         a_h2conn,    // Non-NULL
      uint32_t        a_stream_id,
      char const*     a_msg_body,  // Non-NULL
      int             a_msg_len,
      bool            a_last_msg,
      utxx::time_val  a_ts_recv,
      utxx::time_val  a_ts_handl
    );

    bool ProcessWSMsg
    (
      char const*     a_msg_body,  // Non-NULL
      int             a_msg_len,
      bool            a_last_msg,
      utxx::time_val  a_ts_recv,
      utxx::time_val  a_ts_handl
    );

    //-----------------------------------------------------------------------//
    // Sender:                                                               //
    //-----------------------------------------------------------------------//
    std::tuple<utxx::time_val, SeqNum, uint32_t> SendReq
    (
      char const*     a_method,    // Non-NULL: "GET", "PUT" or "POST"
      char const*     a_path,      // Non-NULL
      OrderID         a_req_id,
      char const*     a_data       // May be NULL
    );

    //=======================================================================//
    // Implementation of virtual methods from "EConnector_OrdMgmt":          //
    //=======================================================================//
  public:
    //-----------------------------------------------------------------------//
    // "NewOrder":                                                           //
    //-----------------------------------------------------------------------//
    AOS const* NewOrder
    (
      Strategy*         a_strategy,
      SecDefD const&    a_instr,
      FIX::OrderTypeT   a_ord_type,
      bool              a_is_buy,
      PriceT            a_px,
      bool              a_is_aggr,
      QtyAny            a_qty,
      utxx::time_val    a_ts_md_exch    = utxx::time_val(),
      utxx::time_val    a_ts_md_conn    = utxx::time_val(),
      utxx::time_val    a_ts_md_strat   = utxx::time_val(),
      bool              a_batch_send    = false,
      FIX::TimeInForceT a_time_in_force = FIX::TimeInForceT::GoodTillCancel,
      int               a_expire_date   = 0,
      QtyAny            a_qty_show      = QtyAny::PosInf(),
      QtyAny            a_qty_min       = QtyAny::Zero  (),
      bool              a_peg_side      = true,
      double            a_peg_offset    = NaN<double>
    )
    override;

  private:
    //-----------------------------------------------------------------------//
    // "NewOrderImpl": "EConnector_OrdMgmt" Call-Back (Wire Protocol):       //
    //-----------------------------------------------------------------------//
    void NewOrderImpl
    (
      EConnector_H2WS_BitMEX_OMC* a_dummy,
      Req12*                      a_new_req,          // Non-NULL
      bool                        a_batch_send        // Always false
    );

  public:
    //-----------------------------------------------------------------------//
    // "CancelOrder":                                                        //
    //-----------------------------------------------------------------------//
    bool CancelOrder
    (
      AOS const*          a_aos,
      utxx::time_val      a_ts_md_exch  = utxx::time_val(),
      utxx::time_val      a_ts_md_conn  = utxx::time_val(),
      utxx::time_val      a_ts_md_strat = utxx::time_val(),
      bool                a_batch_send  = false
    )
    override;

  private:
    //-----------------------------------------------------------------------//
    // "CancelOrderImpl": "EConnector_OrdMgmt" Call-Back (Wire Protocol):    //
    //-----------------------------------------------------------------------//
    void CancelOrderImpl
    (
      EConnector_H2WS_BitMEX_OMC* a_dummy,
      Req12*                      a_clx_req,          // Non-NULL
      Req12 const*                a_orig_req,         // Non-NULL
      bool                        a_batch_send        // Always false
    );

  public:
    //-----------------------------------------------------------------------//
    // "ModifyOrder":                                                        //
    //-----------------------------------------------------------------------//
    bool ModifyOrder
    (
      AOS const*          a_aos,
      PriceT              a_new_px,
      bool                a_is_aggr,
      QtyAny              a_new_qty,
      utxx::time_val      a_ts_md_exch   = utxx::time_val(),
      utxx::time_val      a_ts_md_conn   = utxx::time_val(),
      utxx::time_val      a_ts_md_strat  = utxx::time_val(),
      bool                a_batch_send   = false,
      QtyAny              a_new_qty_show = QtyAny::PosInf(),
      QtyAny              a_new_qty_min  = QtyAny::Zero  ()
    )
    override;

  private:
    //-----------------------------------------------------------------------//
    // "ModifyOrderImpl": "EConnector_OrdMgmt" Call-Back (Wire Protocol):    //
    //-----------------------------------------------------------------------//
    void ModifyOrderImpl
    (
      EConnector_H2WS_BitMEX_OMC* a_dummy,
      Req12*                      a_mod_req0,         // NULL in BitMEX
      Req12*                      a_mod_req1,         // Non-NULL
      Req12 const*                a_orig_req,         // Non-NULL
      bool                        a_batch_send        // Always false
    );

  public:
    //-----------------------------------------------------------------------//
    // "CancelAllOrders":                                                    //
    //-----------------------------------------------------------------------//
    void CancelAllOrders
    (
      unsigned long       a_strat_hash48  = 0,
      SecDefD const*      a_instr         = nullptr,
      FIX::SideT          a_side          = FIX::SideT::UNDEFINED,
      char const*         a_segm_id       = nullptr
    )
    override;

  private:
    void CancelAllOrdersImpl
    (
      EConnector_H2WS_BitMEX_OMC* a_sess,
      unsigned long               a_strat_hash48,
      SecDefD const*              a_instr,
      FIX::SideT                  a_side,
      char const*                 a_segm_id
    );

    //-----------------------------------------------------------------------//
    // "FlushOrders" and "FlushOrdersImpl":                                  //
    //-----------------------------------------------------------------------//
  public:
    utxx::time_val FlushOrders() override;

  private:
    utxx::time_val FlushOrdersImpl(EConnector_H2WS_BitMEX_OMC* a_dummy) const;

    //-----------------------------------------------------------------------//
    // Helpers:                                                              //
    //-----------------------------------------------------------------------//
    void SetPosTimer ();
    void RemovePosTimer ();
    void PosTimerErrHandler
      (int a_fd, int a_err_code, uint32_t a_events, char const* a_msg);

    void RequestPositions();
    void StartWSIfReady();
  };
}
// End namespace MAQUETTE
