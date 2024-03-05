// vim:ts=2:et
//===========================================================================//
//          "Connectors/H2WS/Binance/EConnector_H2WS_Binance_OMC.h":         //
//                           H2WS-Based OMC for Binance                      //
//===========================================================================//
#pragma once
#include "Connectors/H2WS/EConnector_H2WS_OMC.h"
#include "Connectors/H2WS/EConnector_H2WS_OMC.hpp"
#include "Venues/Binance/Configs_H2WS.h"
#include <tuple>
#include <gnutls/gnutls.h>

namespace MAQUETTE
{
  //=========================================================================//
  // Struct "H2BinanceOMC": Extended "H2Connector" (IsOMC=true):             //
  //=========================================================================//
  template<typename PSM>
  struct H2BinanceOMC final: public H2Connector<true, PSM>
  {
    //-----------------------------------------------------------------------//
    // Data Flds (StreamID Counters):                                        //
    //-----------------------------------------------------------------------//
    // StreamIDs for making various Session-level reqs:
    mutable uint32_t      m_lastAPIKeyStmID;
    mutable uint32_t      m_lastListenKeyStmID;
    mutable uint32_t      m_lastPositionsStmID;
    mutable uint32_t      m_lastBalanceStmID;
    mutable uint32_t      m_lastCancelAllStmID;

    // The following is required by "EConnector_H2WS_OMC": ReStart policy:
    constexpr static int  MaxReqsPerH2Session = 950;    // Actually 1000
    mutable int           m_currReqs;

    //-----------------------------------------------------------------------//
    // Non-Default Ctor, Dtor:                                               //
    //-----------------------------------------------------------------------//
    H2BinanceOMC
    (
      PSM*                                a_psm,            // Must be non-NULL
      boost::property_tree::ptree const&  a_params,
      uint32_t                            a_max_reqs,
      std::string                 const&  a_client_ip  = ""
    )
    : H2Connector<true, PSM>(a_psm, a_params, a_max_reqs, a_client_ip)

      // XXX: NOT initialising the mutable flds -- that is done dynamically by
      // "OnTurningActive"...
    {
      LOG_INFO(3, "Starting H2BinanceOMC, IP={}", a_client_ip)
    }

    template <bool FullStop>
    void Stop()
    {
      LOG_INFO(2, "H2BinanceOMC::Stop()")
      m_lastAPIKeyStmID    = 0;
      m_lastBalanceStmID   = 0;
      m_lastListenKeyStmID = 0;
      m_lastPositionsStmID = 0;
      m_lastCancelAllStmID = 0;
      H2Connector<true, PSM>::template Stop<FullStop>();
    }

    // Dtor is auto-generated
  };

  //=========================================================================//
  // "EConnector_H2WS_Binance_OMC" Class:                                    //
  //=========================================================================//
  template <Binance::AssetClassT::type AC>
  class  EConnector_H2WS_Binance_OMC final:
  public EConnector_H2WS_OMC
        <EConnector_H2WS_Binance_OMC<AC>,
         H2BinanceOMC<EConnector_H2WS_Binance_OMC<AC>>> // IsOMC=true
  {
  public:
    // Are we trading Spot?
    static constexpr bool IsSpt  = (AC == Binance::AssetClassT::Spt);
    static constexpr bool IsFutC = (AC == Binance::AssetClassT::FutC);

    // NB: "Fut" is a union of "FutT" and "FutC":
    static constexpr bool IsFut  = (AC != Binance::AssetClassT::Spt);

    static constexpr char const* Name =
        IsSpt ? "H2WS-Binance-OMC-Spt"
              : IsFutC ? "H2WS-Binance-OMC-FutC"
                       : "H2WS-Binance-OMC-FutT";

    // Different queries for different AssetClassT
    static constexpr int      QueryPfxLen  = IsSpt ? 8 : 9;
    static constexpr char const* QueryPfx  = IsSpt ? "/api/v3/"
                                                   : IsFutC ? "/dapi/v1/"
                                                            : "/fapi/v1/";
    static constexpr char const* ListenKey =
        IsSpt ? "/api/v3/userDataStream"
              : IsFutC ? "/dapi/v1/listenKey"
                       : "/fapi/v1/listenKey";

    //=======================================================================//
    // Types and Consts:                                                     //
    //=======================================================================//
    // The Native Qty type: Binance uses Fractional QtyB, not QtyA!
    constexpr static QtyTypeT QT     = QtyTypeT::QtyA;
    using QR                         = double;
    constexpr static bool     IsFrac = true;
    using QtyN                       = Qty<QT, QR>;
    using QtyF                       = Qty<QtyTypeT::QtyB, double>; // Fee

  private:
    // This class must be accessible by the parent classes:
    // IsOMC=true:
    friend class     EConnector_OrdMgmt;
    friend class     TCP_Connector      <EConnector_H2WS_Binance_OMC<AC>>;
    friend struct    H2Connector  <true, EConnector_H2WS_Binance_OMC<AC>>;
    friend struct    H2BinanceOMC       <EConnector_H2WS_Binance_OMC<AC>>;
    friend class     H2WS::WSProtoEngine<EConnector_H2WS_Binance_OMC<AC>>;
    friend class     EConnector_WS_OMC  <EConnector_H2WS_Binance_OMC<AC>>;

    friend class     EConnector_H2WS_OMC
                    <EConnector_H2WS_Binance_OMC<AC>,
                     H2BinanceOMC<EConnector_H2WS_Binance_OMC<AC>>>;

    using  OMCWS   = EConnector_WS_OMC  <EConnector_H2WS_Binance_OMC<AC>>;
    using  H2Conn  = H2BinanceOMC       <EConnector_H2WS_Binance_OMC<AC>>;

    using  OMCH2WS = EConnector_H2WS_OMC
                    <EConnector_H2WS_Binance_OMC<AC>,
                     H2BinanceOMC<EConnector_H2WS_Binance_OMC<AC>>>;

    //-----------------------------------------------------------------------//
    // Features required by "EConnector_H2WS_OMC":                           //
    //-----------------------------------------------------------------------//
    constexpr static int    ThrottlingPeriodSec = IsSpt ?  10 :   60;
    constexpr static int    MaxReqsPerPeriod    = IsSpt ? 100 : 1200;
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
    static H2WSConfig  const& GetH2Config(MQTEnvT a_env);

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
    EConnector_H2WS_Binance_OMC
    (
      EPollReactor*                       a_reactor,
      SecDefsMgr*                         a_sec_defs_mgr,
      std::vector<std::string>    const*  a_only_symbols,  // NULL=UseFullList
      RiskMgr*                            a_risk_mgr,
      boost::property_tree::ptree const&  a_params
    );

    // Dtor is non-virtual as this class is "final":
    ~EConnector_H2WS_Binance_OMC() noexcept override;

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

    // No-op here unless we need to react to specific headers
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
    // Both Processors return "true" to continue and "false" to stop gracefully:
    // For HTTP/2-delivered Msgs:
    // "H2Conn*" provides the context (as there may be multiple H2Connectors in
    // a single OMC):
    //
    bool ProcessH2Msg
    (
      H2Conn*         a_h2conn,    // Non-NULL
      uint32_t        a_stream_id,
      char const*     a_msg_body,  // Non-NULL
      int             a_msg_len,
      bool            a_last_msg,  // No more msgs in input buffer
      utxx::time_val  a_ts_recv,
      utxx::time_val  a_ts_handl
    );

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
      EConnector_H2WS_Binance_OMC* a_dummy,
      Req12*                       a_new_req,              // Non-NULL
      bool                         a_batch_send            // Always false
    );

  public:
    //-----------------------------------------------------------------------//
    // "CancelOrder":                                                        //
    //-----------------------------------------------------------------------//
    bool CancelOrder
    (
      AOS const*                   a_aos,                  // Non-NULL
      utxx::time_val               a_ts_md_exch  = utxx::time_val(),
      utxx::time_val               a_ts_md_conn  = utxx::time_val(),
      utxx::time_val               a_ts_md_strat = utxx::time_val(),
      bool                         a_batch_send  = false   // Always false
    )
    override;

  private:
    //-----------------------------------------------------------------------//
    // "CancelOrderImpl": "EConnector_OrdMgmt" Call-Back (Wire Sending):     //
    //-----------------------------------------------------------------------//
    void CancelOrderImpl
    (
      EConnector_H2WS_Binance_OMC* a_dummy,
      Req12*                       a_clx_req,              // Non-NULL,
      Req12 const*                 a_orig_req,             // Non-NULL
      bool                         a_batch_send            // Always false
    );

  public:
    //-----------------------------------------------------------------------//
    // "ModifyOrder":                                                        //
    //-----------------------------------------------------------------------//
    bool ModifyOrder
    (
      AOS const*                   a_aos,                  // Non-NULL
      PriceT                       a_new_px,
      bool                         a_is_aggr,              // Becoming Aggr?
      QtyAny                       a_new_qty,
      utxx::time_val               a_ts_md_exch   = utxx::time_val(),
      utxx::time_val               a_ts_md_conn   = utxx::time_val(),
      utxx::time_val               a_ts_md_strat  = utxx::time_val(),
      bool                         a_batch_send   = false, // Ignored: False
      QtyAny                       a_new_qty_show = QtyAny::PosInf(),
      QtyAny                       a_new_qty_min  = QtyAny::Zero  ()
    )
    override;

  private:
    //-----------------------------------------------------------------------//
    // "ModifyOrderImpl": "EConnector_OrdMgmt" Call-Back (Wire Sending):     //
    //-----------------------------------------------------------------------//
    void ModifyOrderImpl
    (
      EConnector_H2WS_Binance_OMC* a_dummy,
      Req12*                       a_mod_req0,             // Non-NULL
      Req12*                       a_mod_req1,             // Non-NULL
      Req12 const*                 a_orig_req,             // Non-NULL
      bool                         a_batch_send            // Always false
    );

    //-----------------------------------------------------------------------//
    // "CancelAllOrders":                                                    //
    //-----------------------------------------------------------------------//
  public:
    void CancelAllOrders
    (
      unsigned long               a_strat_hash48 = 0,             // All  Strats
      SecDefD const*              a_instr        = nullptr,       // All  Instrs
      FIX::SideT                  a_side = FIX::SideT::UNDEFINED, // Both Sides
      char const*                 a_segm_id = nullptr             // All  Segms
    )
    override;

  private:
    void CancelAllOrdersImpl
    (
      EConnector_H2WS_Binance_OMC* a_dummy,
      unsigned long  a_strat_hash48,
      SecDefD const* a_instr,
      FIX::SideT     a_side,
      char const*    a_segm_id
    );

  public:
    //-----------------------------------------------------------------------//
    // "FlushOrders":                                                        //
    //-----------------------------------------------------------------------//
    utxx::time_val FlushOrders() override;

  private:
    utxx::time_val FlushOrdersImpl(EConnector_H2WS_Binance_OMC* a_dummy) const;

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

  //-------------------------------------------------------------------------//
  // Aliases:                                                                //
  //-------------------------------------------------------------------------//
  using EConnector_H2WS_Binance_OMC_Spt =
    EConnector_H2WS_Binance_OMC<Binance::AssetClassT::Spt>;

  using EConnector_H2WS_Binance_OMC_FutT =
    EConnector_H2WS_Binance_OMC<Binance::AssetClassT::FutT>;

  using EConnector_H2WS_Binance_OMC_FutC =
    EConnector_H2WS_Binance_OMC<Binance::AssetClassT::FutC>;
}
// End namespace MAQUETTE
