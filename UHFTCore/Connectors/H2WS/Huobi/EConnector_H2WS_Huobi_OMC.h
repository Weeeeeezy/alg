// vim:ts=2:et
//===========================================================================//
//            "Connectors/H2WS/Huobi/EConnector_H2WS_Huobi_OMC.h"            //
//===========================================================================//
#pragma once

#include "Connectors/H2WS/Huobi/Traits.h"
#include "Connectors/H2WS/EConnector_H2WS_OMC.h"
#include "Connectors/H2WS/Huobi/EConnector_Huobi_Common.hpp"

#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>

namespace MAQUETTE
{
namespace Huobi
{
  struct zlib;

  template<uint32_t size>
  struct json_parser
  {
    struct elem
    {
      std::string_view name;
      char const* from;
      char const* to;

      // Prevent a bogus warning here:
#     ifdef  __clang__
#     pragma clang diagnostic push
#     pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"
#     endif
      bool operator<(elem const& e) const
      {
        return name < e.name;
      }
#     ifdef  __clang__
#     pragma clang diagnostic pop
#     endif
    };

    elem elems[size];
    elem tmp;

    void set(char const* i, char const* ie)
    {
      char const* it = i;
      uint32_t count = 0;
      for(;it != ie; ++count)
      {
        if(utxx::unlikely(count > size))
          throw utxx::runtime_error(
              "EConnector_H2WS_Huobi_OMC::ProcessH2Msg: json_parser<",
              size,
              "> parse error:",
              std::string(i, ie));
        skip_fixed(it, "\"");
        char const* ne = std::find(it, ie, '\"');
        elem& e = elems[count];
        e.name = std::string_view(it, uint32_t(ne - it));
        it = ne + 1;
        skip_fixed(it, ":");
        e.from = it;
        it = std::find(it, ie, ',');
        e.to = it;
        if(it != ie)
          ++it;
      }
      if(utxx::unlikely(it != ie || count != size))
        throw utxx::runtime_error("EConnector_H2WS_Huobi_OMC::ProcessH2Msg: json_parser<", size, "> parse error:", std::string(i, ie));
      std::sort(elems, elems + size);
    }

    std::string_view get(std::string_view n, bool remove_quotes = false)
    {
      tmp.name = n;
      elem* e  = std::lower_bound(elems, elems + size, tmp);
      if (utxx::unlikely(e == elems + size || e->name != n))
        throw utxx::runtime_error(
            "EConnector_H2WS_Huobi_OMC::ProcessH2Msg: json_parser<",
            size,
            "> element not found:", n);
      if (remove_quotes)
      {
        if (utxx::unlikely(*(e->from) != '\"' || *(e->to - 1) != '\"'))
          throw utxx::runtime_error(
              "EConnector_H2WS_Huobi_OMC::ProcessH2Msg: json_parser<",
              size,
              "> quotes not found for:", n);
        else
          return std::string_view(e->from + 1, size_t(e->to - e->from - 2));
      }
      else
        return std::string_view(e->from, size_t(e->to - e->from));
    }

    template<typename func>
    auto get(std::string_view n, func f, bool remove_quotes = false)
    {
      auto v = get(n, remove_quotes);
      return f(v.data(), v.data() + v.size());
    }
  };
} // namespace Huobi

  //=========================================================================//
  // Struct "H2HuobiOMC": Extended "H2Connector" (IsOMC=true):               //
  //=========================================================================//
  template <typename PSM>
  struct H2HuobiOMC final : public H2Connector<true, PSM> {
    //-----------------------------------------------------------------------//
    // Non-Default Ctor, Dtor:                                               //
    //-----------------------------------------------------------------------//
    H2HuobiOMC
    (
      PSM*                                a_psm,      // Must be non-NULL
      boost::property_tree::ptree const&  a_params,
      uint32_t                            a_max_reqs,
      std::string                 const&  a_client_ip = ""
    )
    : H2Connector<true, PSM>(a_psm, a_params, a_max_reqs, a_client_ip)
    {
      LOG_INFO(3, "Starting H2HuobiOMC, IP={}", a_client_ip)
    }
  };

  // OMC connector to HUOBI market
  template <Huobi::AssetClassT::type AC>
  class EConnector_H2WS_Huobi_OMC final :
  public EConnector_H2WS_OMC<EConnector_H2WS_Huobi_OMC<AC>,
                             H2HuobiOMC<EConnector_H2WS_Huobi_OMC<AC>>>
  {
  public:
    constexpr static char const *Name = Huobi::Traits<AC>::OmcName;

    constexpr static QtyTypeT QT = QtyTypeT::QtyA;
    using QR = double;
    using QtyN = Qty<QT, QR>;

  private:
    static constexpr bool IsSpt = (AC == Huobi::AssetClassT::Spt);
    static constexpr bool IsFut = (AC == Huobi::AssetClassT::Fut);
    static constexpr bool IsSwp = (AC == Huobi::AssetClassT::CSwp ||
                                   AC == Huobi::AssetClassT::USwp);

    enum class ReqType {
      RequestAccounts,
      RequestBalance,
      RequestOrders,
      PlaceOrder,
      CancelOrder,
      CancelAllOrders
    };

    // IsOMC=true:
    friend class    EConnector_OrdMgmt;
    friend class    TCP_Connector       <EConnector_H2WS_Huobi_OMC<AC>>;
    friend struct   H2Connector   <true, EConnector_H2WS_Huobi_OMC<AC>>;
    friend struct   H2HuobiOMC          <EConnector_H2WS_Huobi_OMC<AC>>;
    friend class    H2WS::WSProtoEngine <EConnector_H2WS_Huobi_OMC<AC>>;
    friend class    EConnector_WS_OMC   <EConnector_H2WS_Huobi_OMC<AC>>;
    friend class    EConnector_H2WS_OMC <EConnector_H2WS_Huobi_OMC<AC>,
                                         H2HuobiOMC<EConnector_H2WS_Huobi_OMC<AC>>>;

    using H2Conn  = H2HuobiOMC          <EConnector_H2WS_Huobi_OMC<AC>>;
    using OMCH2WS = EConnector_H2WS_OMC <EConnector_H2WS_Huobi_OMC<AC>,
                                         H2HuobiOMC<EConnector_H2WS_Huobi_OMC<AC>>>;
    using ProWS   = typename OMCH2WS::ProWS;

    using PipeLineModeT = EConnector_OrdMgmt::PipeLineModeT;

    constexpr static int           ThrottlingPeriodSec = 1;
    constexpr static int           MaxReqsPerPeriod    = 10;
    constexpr static PipeLineModeT PipeLineMode        = PipeLineModeT::Wait1;
    constexpr static bool          SendExchOrdIDs      = false;
    constexpr static bool          UseExchOrdIDsMap    = false;
    constexpr static bool          HasAtomicModify     = false;
    constexpr static bool          HasPartFilledModify = true;
    constexpr static bool          HasExecIDs          = true;
    constexpr static bool          HasMktOrders        = true;
    constexpr static bool          HasNativeMassCancel = true;
    constexpr static bool          HasFreeCancel       = false;

    //=====================================================================//
    // Configuration accessors (for base class):                           //
    //=====================================================================//
    static H2WSConfig const&  GetH2Config(MQTEnvT a_env);
    static H2WSConfig const&  GetWSConfig(MQTEnvT a_env);

  public:
    //=======================================================================//
    // Ctor, Dtor, Start                                                     //
    //=======================================================================//
    EConnector_H2WS_Huobi_OMC
    (
      EPollReactor*                       a_reactor,
      SecDefsMgr*                         a_sec_defs_mgr,
      std::vector<std::string>    const*  a_only_symbols,  // NULL=UseFullList
      RiskMgr*                            a_risk_mgr,
      boost::property_tree::ptree const&  a_params
    );
    ~EConnector_H2WS_Huobi_OMC() noexcept override;

  private:
    /*
      * We use the following initialization sequence:
      * - clear all orders, set init state to "not initialized"
      * - initialize websoket handshake (InitWSSession)
      * - perform websocket authorization (InitWSLogOn)
      * - retrieve data (open orders, account, balance or position) (OnTurningActive)
      * - subscribe to orders and balanse (for spot) notifications
      */
    //=====================================================================//
    // Initialization methods                                              //
    //=====================================================================//
    // This class is final, so the following method must finally be implemen-
    // ted:
    void EnsureAbstract() const override {}

    void InitWSSession()  const;
    void InitWSLogOn();
    void OnTurningActive();

    enum InitState
    {
      NotInitialized    = 0x00,
      OrdersReceived    = 0x01,
      AccountReceived   = 0x02,
      BalanceReceived   = 0x04
    };

    static constexpr uint8_t Initialized =
        AC == Huobi::AssetClassT::Spt
              ? (
                  InitState::OrdersReceived  |
                  InitState::BalanceReceived |
                  InitState::AccountReceived
                )
              : InitState::OrdersReceived;
    mutable uint8_t m_currentInitState;

    void SubscribeWsUpdates();
    void RequestBalances();

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
      H2Conn*        UNUSED_PARAM(a_h2conn),     // Non-NULL
      uint32_t       UNUSED_PARAM(a_stream_id),
      char const*    UNUSED_PARAM(a_http_code),
      char const*    UNUSED_PARAM(a_headers),
      utxx::time_val UNUSED_PARAM(a_ts_recv)
    ) {}

    //=======================================================================//
    // Websocket stream processors                                           //
    //=======================================================================//
    bool ProcessH2Msg(
        H2Conn*        a_h2_conn,
        uint32_t       a_stream_id,
        char const*    a_msg_body,
        int            a_msg_len,
        bool           a_last_msg,
        utxx::time_val a_ts_recv,
        utxx::time_val a_ts_handl);

    bool ProcessWSMsg(
        char const*    a_msg_body,
        int            a_msg_len,
        bool           a_last_msg,
        utxx::time_val a_ts_recv,
        utxx::time_val a_ts_handl);

  public:
    //=======================================================================//
    // Virtual methods from "EConnector_OrdMgmt":                            //
    //=======================================================================//
    AOS const* NewOrder(
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
        double            a_peg_offset    = NaN<double>) override;

    bool CancelOrder(
        AOS const*     a_aos,
        utxx::time_val a_ts_md_exch   = utxx::time_val(),
        utxx::time_val a_ts_md_conn   = utxx::time_val(),
        utxx::time_val a_ts_md_strat  = utxx::time_val(),
        bool           a_batch_send   = false) override;

    void CancelAllOrders(
        unsigned long  a_strat_hash48 = 0,
        SecDefD const* a_instr        = nullptr,
        FIX::SideT     a_side         = FIX::SideT::UNDEFINED,
        char const*    a_segm_id      = nullptr
        ) override;

    bool ModifyOrder(
        AOS const*     a_aos, // Non-NULL
        PriceT         a_new_px,
        bool           a_is_aggr,
        QtyAny         a_new_qty,
        utxx::time_val a_ts_md_exch   = utxx::time_val(),
        utxx::time_val a_ts_md_conn   = utxx::time_val(),
        utxx::time_val a_ts_md_strat  = utxx::time_val(),
        bool           a_batch_send   = false,
        QtyAny         a_new_qty_show = QtyAny::PosInf(),
        QtyAny         a_new_qty_min  = QtyAny::Zero  ()) override;

    utxx::time_val FlushOrders() override;

    //=====================================================================//
    // Implementations for <Method>OrderGen                                //
    //=====================================================================//
    void NewOrderImpl(
        EConnector_H2WS_Huobi_OMC* a_dummy,
        Req12*                     a_new_req,     // Non-NULL
        bool                       a_batch_send
    );

    void CancelOrderImpl(
        EConnector_H2WS_Huobi_OMC* a_dummy,
        Req12*                     a_clx_req,     // Non-NULL,
        Req12 const*               a_orig_req,    // Non-NULL
        bool                       a_batch_send  // Always false
    );

    void CancelAllOrdersImpl(
        EConnector_H2WS_Huobi_OMC* a_dummy,
        unsigned long              a_strat_hash48,
        SecDefD const*             a_instr,
        FIX::SideT                 a_side,
        char const*                a_segm_id);

    void ModifyOrderImpl(
        EConnector_H2WS_Huobi_OMC* a_dummy,
        Req12*                     a_mod_req0,    // Non-NULL
        Req12*                     a_mod_req1,    // Non-NULL
        Req12 const*               a_orig_req,    // Non-NULL
        bool                       a_batch_send   // Always false
    );

    utxx::time_val FlushOrdersImpl(EConnector_H2WS_Huobi_OMC*) const;

  private:
    //---------------------------------------------------------------------//
    // Send methods:                                                       //
    //---------------------------------------------------------------------//
    // Send request, return stream id for request
    uint32_t SendReq(
        std::string_view const& method,
        std::string_view const& req,
        std::string_view const& params);

    // Send signed request, return stream id for request
    void SignAndSendReq(
        std::string_view const& method,
        ReqType                 type,
        std::string_view const& req,
        std::string_view const& params = std::string_view());

    // Create escaped string from @a a_url
    std::string_view EscapeUrl(std::string_view const& a_url);

    std::string_view CreateSignature20(
        std::string_view const& method,
        std::string_view const& host,
        std::string_view const& adress,
        tm*                     a_local,
        std::string_view const& params = std::string_view());
    std::string_view CreateSignature21(tm* a_local);

    //-----------------------------------------------------------------------//
    // Implementations:                                                      //
    //-----------------------------------------------------------------------//
    void PlaceNewOrder(const SecDefD& instr, QtyN amount, bool buy, PriceT price);

    void CancelOrderByClientId(uint64_t client_order_id);
    void RequestOpenOrders(const SymKey& symbol);
    void dropOrders();

  private:
    std::string_view m_h2Host;
    std::string_view m_wsHost;
    std::string_view m_apiKey;
    std::string_view m_apiSecret;

    mutable char m_buf_params[512], m_buf_url[512], m_buf_req[512];
    char         m_sign[64];

    uint32_t m_account_id;

    mutable uint8_t                                       m_signSHA256[32];
    mutable boost::container::flat_map<uint32_t, ReqType> m_streams;

    // client-order-id is a OrigReq number.
    // When we send cancel request we have to pass both orig and cancel requests
    // so we save originalRequest->cancelRequest mapping
    boost::container::flat_map<OrderID, OrderID> m_cancels;
    boost::container::flat_map<OrderID, SymKey>  m_orders;

    char           pong_msg[33] = R"({"op":"pong","ts":0000000000000})";
    utxx::time_val m_lastSend;
    utxx::time_val m_lastWsMsg;
    std::unique_ptr<Huobi::zlib> m_zlib;
  };

  //-------------------------------------------------------------------------//
  // Aliases:                                                                //
  //-------------------------------------------------------------------------//
  using EConnector_H2WS_Huobi_OMC_Spt =
    EConnector_H2WS_Huobi_OMC<Huobi::AssetClassT::Spt>;

  using EConnector_H2WS_Huobi_OMC_Fut =
    EConnector_H2WS_Huobi_OMC<Huobi::AssetClassT::Fut>;

  using EConnector_H2WS_Huobi_OMC_CSwp =
    EConnector_H2WS_Huobi_OMC<Huobi::AssetClassT::CSwp>;

  using EConnector_H2WS_Huobi_OMC_USwp =
    EConnector_H2WS_Huobi_OMC<Huobi::AssetClassT::USwp>;

} // namespace MAQUETTE

