// vim:ts=2:et
//===========================================================================//
//        "Connectors/H2WS/KrakenSpot/EConnector_WS_KrakenSpot_OMC.h":       //
//                         H2WS-Based OMC for KrakenSpot                     //
//===========================================================================//
#pragma once

#include "Connectors/H2WS/EConnector_WS_OMC.h"
#include "Connectors/H2WS/EConnector_WS_OMC.hpp"
#include "Venues/KrakenSpot/Configs_WS.h"
#include <gnutls/gnutls.h>
#include <tuple>

namespace MAQUETTE
{
  //=========================================================================//
  // "EConnector_WS_KrakenSpot_OMC" Class:                                   //
  //=========================================================================//
  class  EConnector_WS_KrakenSpot_OMC final:
  public EConnector_WS_OMC<EConnector_WS_KrakenSpot_OMC>
  {
  public:
    //=======================================================================//
    // Types and Consts:                                                     //
    //=======================================================================//
    // The Native Qty type: KrakenSpot uses Fractional QtyB, not QtyA!
    constexpr static QtyTypeT QT = QtyTypeT::QtyA;
    using QR                     = double;
    using QtyN                   = Qty<QT, QR>;
    using QtyF                   = Qty<QtyTypeT::QtyB, double>; // Fee

  private:
    // This class must be accessible by the parent classes:
    // IsOMC=true:
    friend class     EConnector_OrdMgmt;
    friend class     H2WS::WSProtoEngine<EConnector_WS_KrakenSpot_OMC>;
    friend class     TCP_Connector      <EConnector_WS_KrakenSpot_OMC>;
    friend class     EConnector_WS_OMC  <EConnector_WS_KrakenSpot_OMC>;
    using  OMCWS   = EConnector_WS_OMC  <EConnector_WS_KrakenSpot_OMC>;

    static constexpr char const* Name = "WS-KrakenSpot-OMC";

    //-----------------------------------------------------------------------//
    // Features required by "EConnector_WS_OMC":                             //
    //-----------------------------------------------------------------------//
    constexpr static int    ThrottlingPeriodSec = 10; // TODO check
    constexpr static int    MaxReqsPerPeriod    = 100;
    // Other Features (XXX: CHECK!):
    constexpr static        EConnector_OrdMgmt::PipeLineModeT PipeLineMode =
                            EConnector_OrdMgmt::PipeLineModeT::Wait1;
    constexpr static bool   SendExchOrdIDs      = false;
    constexpr static bool   UseExchOrdIDsMap    = false;
    constexpr static bool   HasAtomicModify     = false; // Use ModLegC+ModLegN
    constexpr static bool   HasPartFilledModify = true;  // so  PartFill is OK!
    constexpr static bool   HasExecIDs          = false;
    constexpr static bool   HasMktOrders        = true;
    constexpr static bool   HasBatchSend        = false; // Not with HTTPS
    constexpr static bool   HasNativeMassCancel = true;
    constexpr static bool   HasFreeCancel       = false;

    //-----------------------------------------------------------------------//
    // "Get{WS,H2}Config":                                                   //
    //-----------------------------------------------------------------------//
    static H2WSConfig  const& GetWSConfig(MQTEnvT a_env);

    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    mutable char            m_userStreamStr[128];  // For WS initiation
    mutable char            m_path         [2048]; // H2Path?ReqStrings
    mutable uint8_t         m_signSHA256   [32];   // 256 bits = 32 bytes
    mutable gnutls_datum_t  m_signHMAC;
    // Yet another TimerFD required for periodc re-confirmation of WS-based
    // UserStreams:
    int                     m_usTimerFD;

    // We start the WS leg only with these three conditions satisfied:
    mutable bool            m_listenKeyReceived  = false;
    mutable bool            m_balOrPosReceived   = false;

  public:
    static std::vector<SecDefS> const* GetSecDefs();
    //=======================================================================//
    // Non-Default Ctor, Dtor, Start, Stop, Properties:                      //
    //=======================================================================//
    // Ctor:
    EConnector_WS_KrakenSpot_OMC
    (
      EPollReactor*                       a_reactor,
      SecDefsMgr*                         a_sec_defs_mgr,
      std::vector<std::string>    const*  a_only_symbols,  // NULL=UseFullList
      RiskMgr*                            a_risk_mgr,
      boost::property_tree::ptree const&  a_params
    );

    // Dtor is non-virtual as this class is "final":
    ~EConnector_WS_KrakenSpot_OMC() noexcept override;

    void Start() override;
    void Stop () override;

    // "IsActive" is provided by "EConnector_H2WS_OMC"

  private:
    // This class is final, so the following method must finally be implemented:
    void EnsureAbstract() const override {}

    //=======================================================================//
    // Session Mgmt:                                                         //
    //=======================================================================//
    void InitWSSession    ();
    void InitWSLogOn      ();
    void OnTurningActive  ();

    //=======================================================================//
    // Processing and Sending Msgs:                                          //
    //=======================================================================//
    // For WebSocket-delivered Msgs:
    bool ProcessWSMsg
    (
      char const*     a_msg_body,  // Non-NULL
      int             a_msg_len,
      bool            a_last_msg,  // No more msgs in input buffer
      utxx::time_val  a_ts_recv,
      utxx::time_val  a_ts_handl
    );

    //-----------------------------------------------------------------------//
    // Sender:                                                               //
    //-----------------------------------------------------------------------//
    // Returns (SendTS, SeqNum, StreamID):
    //
    template<size_t QueryOff>       // Offset of QueryStr in Path, 0 if none
    std::tuple<utxx::time_val, SeqNum, uint32_t> SendReq
    (
      char const* a_method,         // "GET", "PUT", "POST" or "DELETE"
      OrderID     a_req_id,         //
      char const* a_path = nullptr  // If NULL, "m_path" will be used
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
    // "NewOrderImpl": "EConnector_OrdMgmt" Call-Back (Wire Sending):        //
    //-----------------------------------------------------------------------//
    void NewOrderImpl
    (
      EConnector_WS_KrakenSpot_OMC* UNUSED_PARAM(a_dummy),
      Req12*                        UNUSED_PARAM(a_new_req),     // Non-NULL
      bool                          UNUSED_PARAM(a_batch_send)   // Always false
    )
    {}

  public:
    //-----------------------------------------------------------------------//
    // "CancelOrder":                                                        //
    //-----------------------------------------------------------------------//
    bool CancelOrder
    (
      AOS const*                    a_aos,                       // Non-NULL
      utxx::time_val                a_ts_md_exch  = utxx::time_val(),
      utxx::time_val                a_ts_md_conn  = utxx::time_val(),
      utxx::time_val                a_ts_md_strat = utxx::time_val(),
      bool                          a_batch_send  = false        // Always false
    )
    override;

  private:
    //-----------------------------------------------------------------------//
    // "CancelOrderImpl": "EConnector_OrdMgmt" Call-Back (Wire Sending):     //
    //-----------------------------------------------------------------------//
    void CancelOrderImpl
    (
      EConnector_WS_KrakenSpot_OMC* UNUSED_PARAM(a_dummy),
      Req12*                        UNUSED_PARAM(a_clx_req),     // Non-NULL
      Req12 const*                  UNUSED_PARAM(a_orig_req),    // Non-NULL
      bool                          UNUSED_PARAM(a_batch_send)   // Always false
    )
    {}

  public:
    //-----------------------------------------------------------------------//
    // "ModifyOrder":                                                        //
    //-----------------------------------------------------------------------//
    bool ModifyOrder
    (
      AOS const*                    a_aos,                  // Non-NULL
      PriceT                        a_new_px,
      bool                          a_is_aggr,              // Becoming Aggr?
      QtyAny                        a_new_qty,
      utxx::time_val                a_ts_md_exch   = utxx::time_val(),
      utxx::time_val                a_ts_md_conn   = utxx::time_val(),
      utxx::time_val                a_ts_md_strat  = utxx::time_val(),
      bool                          a_batch_send   = false, // Ignored: False
      QtyAny                        a_new_qty_show = QtyAny::PosInf(),
      QtyAny                        a_new_qty_min  = QtyAny::Zero  ()
    )
    override;

  private:
    //-----------------------------------------------------------------------//
    // "ModifyOrderImpl": "EConnector_OrdMgmt" Call-Back (Wire Sending):     //
    //-----------------------------------------------------------------------//
    void ModifyOrderImpl
    (
      EConnector_WS_KrakenSpot_OMC* UNUSED_PARAM(a_dummy),
      Req12*                        UNUSED_PARAM(a_mod_req0),    // Non-NULL
      Req12*                        UNUSED_PARAM(a_mod_req1),    // Non-NULL
      Req12 const*                  UNUSED_PARAM(a_orig_req),    // Non-NULL
      bool                          UNUSED_PARAM(a_batch_send)   // Always false
    )
    {}

    //-----------------------------------------------------------------------//
    // "CancelAllOrders":                                                    //
    //-----------------------------------------------------------------------//
  public:
    void CancelAllOrders
    (
      unsigned long                 a_strat_hash48 = 0,             // All Strs
      SecDefD const*                a_instr        = nullptr,       // All Ins
      FIX::SideT                    a_side = FIX::SideT::UNDEFINED, // Both Sds
      char const*                   a_segm_id = nullptr             // All Sgms
    )
    override;

  private:
    void CancelAllOrdersImpl
    (
      EConnector_WS_KrakenSpot_OMC* UNUSED_PARAM(a_dummy),
      unsigned long                 UNUSED_PARAM(a_strat_hash48),
      SecDefD const*                UNUSED_PARAM(a_instr),
      FIX::SideT                    UNUSED_PARAM(a_side),
      char const*                   UNUSED_PARAM(a_segm_id)
    )
    {}

  public:
    //-----------------------------------------------------------------------//
    // "FlushOrders":                                                        //
    //-----------------------------------------------------------------------//
    utxx::time_val FlushOrders() override;

  private:
    utxx::time_val FlushOrdersImpl
      (EConnector_WS_KrakenSpot_OMC* UNUSED_PARAM(a_dummy)) const
      { return utxx::time_val(); }

    //=======================================================================//
    // Other Helpers:                                                        //
    //=======================================================================//
    void RequestAPIKey();
    void RequestPositions();
    void RequestBalance();
    void RequestListenKey();

    void StartWSIfReady();
    void SetUSTimer ();
    void RemoveUSTimer ();
    void USTimerErrHandler
      (int a_fd, int a_err_code, uint32_t a_events, char const* a_msg);
  };
}
// End namespace MAQUETTE
