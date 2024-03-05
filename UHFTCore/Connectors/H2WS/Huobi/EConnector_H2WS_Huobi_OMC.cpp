// vim:ts=2:et
//===========================================================================//
//           "Connectors/H2WS/Huobi/EConnector_H2WS_Huobi_OMC.cpp"           //
//===========================================================================//
#include <algorithm>

#include <utxx/error.hpp>

#include "Connectors/H2WS/Huobi/EConnector_H2WS_Huobi_OMC.h"
#include "Connectors/EConnector_OrdMgmt.hpp"
#include "Connectors/H2WS/EConnector_H2WS_OMC.hpp"
#include "Connectors/H2WS/EConnector_WS_OMC.hpp"
#include "EConnector_Huobi_Common.hpp"
#include "Protocols/H2WS/WSProtoEngine.hpp"
#include "Venues/Huobi/Configs_H2WS.h"
#include "Venues/Huobi/SecDefs.h"
#include <gnutls/crypto.h>

namespace MAQUETTE
{
  using namespace Huobi;

  template<AssetClassT::type> struct typed_false : std::false_type {};

  namespace
  {
    /**
     * Get error-code field from message. it will point to the symbol
     * right after code value
     */
    int GetErrorCode(char const*& it, char const* ie)
    {
      skip_to_next(it, ie, R"("err-code":)");
      char const* ne   = std::find(it, ie, ',');
      int         code = 0;
      it  = utxx::fast_atoi<int, false>(it, ne, code);
      return code;
    }

    char* print_time(char* i, tm* a_local, bool a_url_encoded)
    {
      if (a_url_encoded)
        i += sprintf(
            i,
            "%04d-%02d-%02dT%02d%%3A%02d%%3A%02d",
            a_local->tm_year + 1900,
            a_local->tm_mon + 1,
            a_local->tm_mday,
            a_local->tm_hour,
            a_local->tm_min,
            a_local->tm_sec);
      else
        i += sprintf(
            i,
            "%04d-%02d-%02dT%02d:%02d:%02d",
            a_local->tm_year + 1900,
            a_local->tm_mon + 1,
            a_local->tm_mday,
            a_local->tm_hour,
            a_local->tm_min,
            a_local->tm_sec);
      return i;
    }

    std::string_view const msg_get  = "GET";
    std::string_view const msg_post = "POST";
    std::string_view const msg_nl   = "\n";

#   if defined(__GNUC__) && !defined(__clang__)
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wstringop-overflow"
#   endif
    char* operator<<(char* i, std::string_view const& s)
    {
      return stpncpy(i, s.data(), s.size());
    }

    char* operator<<(char* i, SymKey const& s)
    {
      return stpncpy(i, s.data(), s.size());
    }

#   if defined(__GNUC__) && !defined(__clang__)
#   pragma GCC diagnostic pop
#   endif

    char dec2hexChar(uint16_t n)
    {
      if (n <= 9)
        return char(short('0') + n);
      else if (10 <= n && n <= 15)
        return char(short('A') + n - 10);
      else
        return char();
    }
  }

  //=======================================================================//
  // Configuration accessors (for base class):                             //
  //=======================================================================//
  template <AssetClassT::type AC>
  H2WSConfig const& EConnector_H2WS_Huobi_OMC<AC>::GetWSConfig(MQTEnvT a_env)
  {
    auto cit = Configs_WS_OMC.find(std::make_pair(AC, a_env));
    if (utxx::unlikely(cit == Configs_WS_OMC.cend()))
    {
      throw utxx::badarg_error(
          "EConnector_H2WS_Huobi_OMC::GetWSConfig: UnSupported Env=",
          a_env.to_string());
    }
    return cit->second;
  }

  template <Huobi::AssetClassT::type AC>
  H2WSConfig const& EConnector_H2WS_Huobi_OMC<AC>::GetH2Config(MQTEnvT a_env)
  {
    auto cit = Configs_H2_OMC.find(std::make_pair(AC, a_env));
    if (utxx::unlikely(cit == Configs_H2_OMC.cend()))
    {
      throw utxx::badarg_error(
        "EConnector_H2WS_Huobi_OMC::GetH2Config: UnSupported Env=",
        a_env.to_string());
    }
    return cit->second;
  }

    //=======================================================================//
    // Ctor, Dtor, Start                                                     //
    //=======================================================================//
    template <Huobi::AssetClassT::type AC>
    EConnector_H2WS_Huobi_OMC<AC>::EConnector_H2WS_Huobi_OMC(
        EPollReactor*                      a_reactor,
        SecDefsMgr*                        a_sec_defs_mgr,
        std::vector<std::string>           const* a_only_symbols,
        RiskMgr*                           a_risk_mgr,
        boost::property_tree::ptree const& a_params) :
      EConnector
      (
        a_params.get<std::string>("AccountKey"),
        "Huobi",
        0,     // a_extra_shm_size
        a_reactor,
        false, // No BusyWait
        a_sec_defs_mgr,
        GetSecDefs(AC),
        a_only_symbols,                  // Restricting full Huobi list
        a_params.get<bool>("ExplSettlDatesOnly",   false),
        false,                           // Presumably no Tenors in SecIDs
        a_risk_mgr,
        a_params,
        QT,
        std::is_floating_point_v<QR>
      ),
      OMCH2WS(a_params),

      // Take h2 host from connectiion
      m_h2Host(
          reinterpret_cast<char const*>(
              OMCH2WS::m_notActvH2Conns[0]->m_outHdrs[0].value),
          uint32_t(OMCH2WS::m_notActvH2Conns[0]->m_outHdrs[0].valuelen)),
      m_wsHost(
          OMCH2WS::m_serverName.c_str(),
          uint32_t(OMCH2WS::m_serverName.size())),
      m_apiKey    (OMCH2WS::m_account.m_APIKey.data()),
      m_apiSecret (OMCH2WS::m_account.m_APISecret.data()),
      m_lastSend  (utxx::now_utc()),
      m_lastWsMsg (m_lastSend),
      m_zlib      (new Huobi::zlib)
    {
      assert(gnutls_hmac_get_len(GNUTLS_MAC_SHA256) == sizeof(m_signSHA256));

      //---------------------------------------------------------------------//
      // Out-Bound Headers:                                                  //
      //---------------------------------------------------------------------//
      // [0]: ":authoriry" (created by Parent Ctor)
      // [1]: ":scheme"    (ditto)
      // [2]: ":method"    (ditto, name only)
      // [3]: ":path"      (ditto, name only)
      // [4]: "content-type": "application/json"
      //
      // At this point, there are no Active "H2Conn"s, so install Headers for
      // the NotActive ones:
      assert(OMCH2WS::m_activeH2Conns.empty());

      std::string_view const json_name  = "content-type";
      std::string_view const json_value = "application/json";
      for (auto h2conn: OMCH2WS::m_notActvH2Conns)
      {
        auto& outHdrs   = h2conn->m_outHdrs; // Ref!
        outHdrs[4].name = const_cast<uint8_t*>(
            reinterpret_cast<uint8_t const*>(json_name.data()));
        outHdrs[4].namelen = json_name.size();
        outHdrs[4].value   = const_cast<uint8_t*>(
            reinterpret_cast<uint8_t const*>(json_value.data()));
        outHdrs[4].valuelen = json_value.size();
      }

      m_account_id = a_params.get<uint32_t>("AccountID");
    }

    template <Huobi::AssetClassT::type AC>
    EConnector_H2WS_Huobi_OMC<AC>::~EConnector_H2WS_Huobi_OMC() noexcept =
        default;

    //=======================================================================//
    // Initialization methods                                                //
    //=======================================================================//
    template <Huobi::AssetClassT::type AC>
    void EConnector_H2WS_Huobi_OMC<AC>::InitWSSession() const
    {
      LOG_INFO(2, "EConnector_H2WS_Huobi_OMC::InitWSession()")

      m_currentInitState = InitState::NotInitialized;
      OMCH2WS::m_positions.clear();
      OMCH2WS::m_balances.clear();

      ProWS::InitWSHandShake(Huobi::Traits<AC>::OmcEndpoint);
    }

    /**
     * We are to send auth request
     */
    template <Huobi::AssetClassT::type AC>
    void EConnector_H2WS_Huobi_OMC<AC>::InitWSLogOn()
    {
      LOG_INFO(2, "EConnector_H2WS_Huobi_OMC::InitWSLogOn()")

      time_t t     = time(nullptr);
      tm*    local = gmtime(&t);
      char* i = m_buf_params;

      auto signature = CreateSignature20(msg_get, m_wsHost,
          std::string_view(Huobi::Traits<AC>::OmcEndpoint), local);
      i = i << std::string_view(R"({"op":"auth",)");
      if (!IsSpt)
        i = i << std::string_view(R"("type":"api",)");
      i = i << std::string_view(R"("AccessKeyId":")") << m_apiKey
            << std::string_view(
                  R"(","SignatureMethod":"HmacSHA256","SignatureVersion":"2","Timestamp":")");
      i = print_time(i, local, false);
      i = i << std::string_view(R"(","Signature":")") << signature
            << std::string_view(R"("})");

      ProWS::PutTxtData(m_buf_params, size_t(i - m_buf_params));
      ProWS::SendTxtFrame();
    }

    template <Huobi::AssetClassT::type AC>
    void EConnector_H2WS_Huobi_OMC<AC>::OnTurningActive()
    {
      LOG_INFO(3, "EConnector_H2WS_Huobi_OMC::OnTurningActive()")
      if (m_currentInitState == Initialized)
      {
        SubscribeWsUpdates();
        return;
      }

      if constexpr (IsSpt)
      {
        if (!(m_currentInitState & InitState::AccountReceived))
        {
          SignAndSendReq(
              msg_get,
              ReqType::RequestAccounts,
              std::string_view("/v1/account/accounts"));
          // To get balance and open orders we need to have account
          return;
        }
        if (!(m_currentInitState & InitState::BalanceReceived))
        {
          RequestBalances();
        }
      }

      if (!(m_currentInitState & InitState::OrdersReceived))
      {
        for (auto s: OMCH2WS::m_usedSecDefs)
        {
          RequestOpenOrders(s->m_Symbol);
        }
      }
    }

    template<Huobi::AssetClassT::type AC>
    void EConnector_H2WS_Huobi_OMC<AC>::RequestBalances()
    {
      // GET /v1/account/accounts/{account-id}/balance
      char* i = m_buf_req;
      i = i << std::string_view(R"(/v1/account/accounts/)");
      i = utxx::itoa_left<uint32_t, 10>(i, m_account_id);
      i << std::string_view(R"(/balance)");

      SignAndSendReq(msg_get, ReqType::RequestBalance, m_buf_req);
    }

    template<Huobi::AssetClassT::type AC>
    void EConnector_H2WS_Huobi_OMC<AC>::SubscribeWsUpdates()
    {
      // Subscribe to orders update
      std::string sub = "{\"op\":\"sub\",\"topic\":\"orders.*";
      sub += (IsSpt ? ".update" : "");
      sub += "\"}";

      LOG_INFO(3, "EConnector_H2WS_Huobi_OMC::Subscribe {}", sub)
      ProWS::PutTxtData(sub.c_str());
      ProWS::SendTxtFrame();

      // Subscribe to balance/position updates
      sub = "{\"op\":\"sub\",\"topic\":\"";
      sub += (IsSpt ? "accounts" : "positions.*");
      sub += "\"}";

      LOG_INFO(3, "EConnector_H2WS_Huobi_OMC::Subscribe {}", sub)
      ProWS::PutTxtData(sub.c_str());
      ProWS::SendTxtFrame();
    }

    template <Huobi::AssetClassT::type AC>
    void EConnector_H2WS_Huobi_OMC<AC>::OnRstStream(
        H2Conn*        a_h2_conn,
        uint32_t       a_stream_id,
        uint32_t       a_err_code,
        utxx::time_val a_ts_recv)
    {
      OrderID reqID = a_h2_conn->ReqIDofStreamID(a_stream_id);

      LOG_WARN(2, "EConnector_H2WS_Huobi_OMC::OnRstSream: ConnH2: StreamID={}, "
                  "ReqID={}", a_stream_id, reqID)

      // Drop request order of current stream
      EConnector_OrdMgmt::OrderRejected<QT, QR>(
          this,
          this,
          0,
          reqID,
          0,
          FuzzyBool::True,
          int(a_err_code),
          "",
          a_ts_recv,
          a_ts_recv);

      // Initiate stop
      (void) a_h2_conn->template Stop<false>();
      // Move this H2Conn to NotActive list, but do NOT rotate it yet:
      OMCH2WS::MoveH2ConnToNotActive(a_h2_conn);
    }
    //=======================================================================//
    // Implementation of virtual methods from "EConnector_OrdMgmt":          //
    //=======================================================================//
    template <Huobi::AssetClassT::type AC>
    AOS const* EConnector_H2WS_Huobi_OMC<AC>::NewOrder(
        Strategy*         a_strategy,
        SecDefD const&    a_instr,
        FIX::OrderTypeT   a_ord_type,
        bool              a_is_buy,
        PriceT            a_px,
        bool              a_is_aggr,
        QtyAny            a_qty,
        utxx::time_val    a_ts_md_exch,
        utxx::time_val    a_ts_md_conn,
        utxx::time_val    a_ts_md_strat,
        bool              a_batch_send,
        FIX::TimeInForceT a_time_in_force,
        int               a_expire_date,
        QtyAny            a_qty_show,
        QtyAny            a_qty_min,
        bool              a_peg_side,
        double            a_peg_offset)
    {
      LOG_INFO(3, "EConnector_H2WS_Huobi_OMC::NewOrder()")
      return EConnector_OrdMgmt::NewOrderGen<QT, QR>(
          this,
          this,
          a_strategy,
          a_instr,
          a_ord_type,
          a_is_buy,
          a_px,
          a_is_aggr,
          a_qty,
          a_ts_md_exch,
          a_ts_md_conn,
          a_ts_md_strat,
          a_batch_send,
          a_time_in_force,
          a_expire_date,
          a_qty_show,
          a_qty_min,
          a_peg_side,
          a_peg_offset);
    }

    template <Huobi::AssetClassT::type AC>
    bool EConnector_H2WS_Huobi_OMC<AC>::CancelOrder(
        AOS const*     a_aos,
        utxx::time_val a_ts_md_exch,
        utxx::time_val a_ts_md_conn,
        utxx::time_val a_ts_md_strat,
        bool           a_batch_send)
    {
      LOG_INFO(3, "EConnector_H2WS_Huobi_OMC::CancelOrder()")
      return EConnector_OrdMgmt::CancelOrderGen<QT, QR>(
          this,
          this,
          const_cast<AOS*>(a_aos),
          a_ts_md_exch,
          a_ts_md_conn,
          a_ts_md_strat,
          a_batch_send);
    }

    template <Huobi::AssetClassT::type AC>
    void EConnector_H2WS_Huobi_OMC<AC>::CancelAllOrders(
        unsigned long  a_strat_hash48,
        SecDefD const* a_instr,
        FIX::SideT     a_side,
        char const*    a_segm_id)
    {
      LOG_INFO(3, "EConnector_H2WS_Huobi_OMC::CancelAllOrders()")
      EConnector_OrdMgmt::CancelAllOrdersGen<QT, QR>(
          this,
          this,
          a_strat_hash48,
          a_instr,
          a_side,
          a_segm_id);
    }

    template <Huobi::AssetClassT::type AC>
    bool EConnector_H2WS_Huobi_OMC<AC>::ModifyOrder(
        AOS const*     a_aos,
        PriceT         a_new_px,
        bool           a_is_aggr,
        QtyAny         a_new_qty,
        utxx::time_val a_ts_md_exch,
        utxx::time_val a_ts_md_conn,
        utxx::time_val a_ts_md_strat,
        bool           a_batch_send,
        QtyAny         a_new_qty_show,
        QtyAny         a_new_qty_min)
    {
      assert(a_aos != nullptr);
      LOG_INFO(3, "EConnector_H2WS_Huobi_OMC::ModifyOrder()")
      return EConnector_OrdMgmt::ModifyOrderGen<QT, QR>(
          this,
          this,
          const_cast<AOS*>(a_aos),
          a_new_px,
          a_is_aggr,
          a_new_qty,
          a_ts_md_exch,
          a_ts_md_conn,
          a_ts_md_strat,
          a_batch_send,
          a_new_qty_show,
          a_new_qty_min);
    }

    template <Huobi::AssetClassT::type AC>
    utxx::time_val EConnector_H2WS_Huobi_OMC<AC>::FlushOrders()
    {
      return utxx::now_utc();
    }

    //=======================================================================//
    // Implementations for <Method>OrderGen                                  //
    //=======================================================================//
    template <Huobi::AssetClassT::type AC>
    void EConnector_H2WS_Huobi_OMC<AC>::NewOrderImpl(
        EConnector_H2WS_Huobi_OMC* DEBUG_ONLY(a_dummy),
        Req12*                     a_new_req,
        bool                       UNUSED_PARAM(a_batch_send))
    {
      //-----------------------------------------------------------------------//
      // Checks:                                                               //
      //-----------------------------------------------------------------------//
      assert(a_dummy == this       && a_new_req != nullptr);
      assert(a_new_req->m_kind     == Req12::KindT::New ||
             a_new_req->m_kind     == Req12::KindT::ModLegN);
      assert(a_new_req->m_status   == Req12::StatusT::Indicated);

      // Check once again for unavailable "m_currH2Conn"   (as this method may be
      // invoked by-passing "NewOrder" above). Throw an exception if cannot send,
      // as there is no other way of reporting a failure at this point:
      //
      if (utxx::unlikely(OMCH2WS::m_currH2Conn == nullptr))
        throw utxx::runtime_error
              ("EConnector_H2WS_Binance_OMC::NewOrderImpl: No Active H2Conns");

      //---------------------------------------------------------------------//
      // NewOrder Params:                                                    //
      //---------------------------------------------------------------------//
      OrderID newReqID = a_new_req->m_id;
      assert(newReqID != 0);
      AOS const* aos = a_new_req->m_aos;
      assert(aos != nullptr);
      SecDefD const* instr = aos->m_instr;
      assert(instr != nullptr);

      QtyN  qty = a_new_req->GetQty<QT,QR>();
      assert(IsPos(qty));
      PriceT px = a_new_req->m_px;
      assert(IsFinite(px));
      m_orders[newReqID] = instr->m_Symbol;

      std::swap(*OMCH2WS::m_ReqN, newReqID);
      PlaceNewOrder(*instr, qty, aos->m_isBuy, px);
      std::swap(*OMCH2WS::m_ReqN, newReqID);

      // Update the "NewReq":
      a_new_req->m_status  = Req12::StatusT::New;
      a_new_req->m_ts_sent = m_lastSend;
      a_new_req->m_seqNum  = (*OMCH2WS::m_txSN) - 1;
    }

    template <Huobi::AssetClassT::type AC>
    void EConnector_H2WS_Huobi_OMC<AC>::CancelOrderImpl(
        EConnector_H2WS_Huobi_OMC* DEBUG_ONLY(a_dummy),
        Req12*                     a_clx_req,
        Req12 const*               a_orig_req,
        bool                       DEBUG_ONLY(a_batch_send))
    {
      assert(
          a_dummy == this && a_clx_req != nullptr && a_orig_req != nullptr
          && (a_clx_req->m_kind == Req12::KindT::Cancel
              || a_clx_req->m_kind == Req12::KindT::ModLegC)
          && a_clx_req->m_status == Req12::StatusT::Indicated
          && a_orig_req->m_status >= Req12::StatusT::New
          && a_clx_req->m_aos == a_orig_req->m_aos && !a_batch_send);

      OrderID clReqID   = a_clx_req->m_id;
      OrderID origReqID = a_orig_req->m_id;
      assert(clReqID != 0 && origReqID != 0 && clReqID > origReqID);

      DEBUG_ONLY(AOS const* aos = a_clx_req->m_aos;)
      assert(aos != nullptr);

      m_cancels[origReqID] = clReqID;
      std::swap(*OMCH2WS::m_ReqN, clReqID);
      CancelOrderByClientId(origReqID);
      std::swap(*OMCH2WS::m_ReqN, clReqID);

      // Update the "CxlReq":
      a_clx_req->m_status  = Req12::StatusT::New;
      a_clx_req->m_ts_sent = m_lastSend;
      a_clx_req->m_seqNum  = (*OMCH2WS::m_txSN) - 1;
    }

    template <Huobi::AssetClassT::type AC>
    void EConnector_H2WS_Huobi_OMC<AC>::CancelAllOrdersImpl(
        EConnector_H2WS_Huobi_OMC* DEBUG_ONLY(a_dummy),
        unsigned long              UNUSED_PARAM(a_strat_hash48),
        const SecDefD*             a_instr,
        FIX::SideT                 a_side,
        const char*                UNUSED_PARAM(a_segm_id))
    {
      assert(a_dummy == this);

      if constexpr (IsSpt)
      {
        /*
         * POST /v1/order/orders/batchcancelopenorders
         * '{
              "account-id": "100009",
              "symbol": "btcusdt",
              "side": "buy"
            }'
         */
        char* i = m_buf_req;

        i = i << std::string_view(R"({"account-id":")");
        i = utxx::itoa_left<uint32_t, 10>(i, m_account_id);

        if (a_instr != nullptr)
        {
          i = i << std::string_view(R"(","symbol":"})");
          i = i << a_instr->m_Symbol;
        }
        if (a_side != FIX::SideT::UNDEFINED)
        {
          i = i << std::string_view(R"(","side":"})");
          i = i
              << (a_side == FIX::SideT::Buy ? std::string_view("buy") :
                                              std::string_view("sell"));
        }
        i = i << std::string_view(R"("})");

        SignAndSendReq(
            msg_post,
            ReqType::CancelAllOrders,
            std::string_view("/v1/order/orders/batchcancelopenorders"),
            std::string_view(m_buf_req, uint32_t(i - m_buf_req)));
      }
      else if constexpr (IsFut)
      {
        // We do not use a_side for futures contracts
        (void)a_side;
        /*
          {
            "symbol": "BTC",
            "contract_type": "this_week"
          }
        */
        char* i = m_buf_req;

        if (a_instr == nullptr)
        {
          throw utxx::runtime_error(
              "EConnector_H2WS_Huobi_OMC::CancelAllOrdersImpl: Invalid instrument");
        }

        i = i << std::string_view(R"({"symbol":")");
        i = i << a_instr->m_Symbol;
        i = i << std::string_view(R"(","contract_type":"})");
        i = i << a_instr->m_Tenor;
        i = i << std::string_view(R"("})");

        SignAndSendReq(
            msg_post,
            ReqType::CancelAllOrders,
            std::string_view("/api/v1/contract_cancelall"),
            std::string_view(m_buf_req, uint32_t(i - m_buf_req)));
      }
      else if constexpr (IsSwp)
      {
        // We do not use a_side for futures contracts
        (void)a_side;
        /*
         * POST /swap-api/v1/swap_cancelall
          {
            "contract_code": "BTC-USD"
          }
        */
        if (a_instr == nullptr)
        {
          throw utxx::runtime_error(
              "EConnector_H2WS_Huobi_OMC::CancelAllOrdersImpl: Invalid instrument");
        }

        char* i = m_buf_req;
        i       = i << std::string_view(R"({"contract_code":")");
        i       = i << a_instr->m_AltSymbol;
        i       = i << std::string_view(R"("})");
        SignAndSendReq(
            msg_post,
            ReqType::CancelAllOrders,
            std::string_view("/swap-api/v1/swap_cancelall"),
            std::string_view(m_buf_req, uint32_t(i - m_buf_req)));
      }
      else
      {
        static_assert(typed_false<AC>::value, "Unsupported asset");
      }
    }

    template <Huobi::AssetClassT::type AC>
    void EConnector_H2WS_Huobi_OMC<AC>::ModifyOrderImpl(
        EConnector_H2WS_Huobi_OMC* DEBUG_ONLY(a_dummy),
        Req12*                     a_mod_req0,
        Req12*                     a_mod_req1,
        Req12 const*               a_orig_req,
        bool                       a_batch_send)
    {
      assert(
          a_dummy == this && a_mod_req0 != nullptr && a_mod_req1 != nullptr
          && a_orig_req != nullptr && a_mod_req0->m_kind == Req12::KindT::ModLegC
          && a_mod_req1->m_kind == Req12::KindT::ModLegN
          && a_mod_req0->m_status == Req12::StatusT::Indicated
          && a_mod_req1->m_status == Req12::StatusT::Indicated
          && a_orig_req->m_status >= Req12::StatusT::New
          && a_mod_req0->m_aos == a_mod_req1->m_aos
          && a_mod_req1->m_aos == a_orig_req->m_aos
          && a_mod_req1->m_id == a_mod_req0->m_id + 1
          && a_mod_req0->m_id > a_orig_req->m_id && !a_batch_send);

      // No native modify order supported
      CancelOrderImpl(this, a_mod_req0, a_orig_req, a_batch_send);
      NewOrderImpl(this, a_mod_req1, a_batch_send);
    }

    template <Huobi::AssetClassT::type AC>
    utxx::time_val EConnector_H2WS_Huobi_OMC<AC>::FlushOrdersImpl(
        EConnector_H2WS_Huobi_OMC*) const
    {
      return utxx::now_utc();
    }


    //-----------------------------------------------------------------------//
    // Send methods:                                                         //
    //-----------------------------------------------------------------------//
    template <Huobi::AssetClassT::type AC>
    uint32_t EConnector_H2WS_Huobi_OMC<AC>::SendReq(
        const std::string_view& method,
        const std::string_view& req,
        const std::string_view& params)
    {
      auto& connection = *OMCH2WS::m_currH2Conn;
      if (utxx::unlikely(!connection.IsActive()))
        throw utxx::runtime_error(
            "EConnector_H2WS_Huobi_OMC: sending request to non-active connection");

      auto& outHdrs   = connection.m_outHdrs;
      auto& outHdrsFrame = connection.m_outHdrsFrame; // Ref!

      outHdrs[2].value =
          const_cast<uint8_t*>(reinterpret_cast<uint8_t const*>(method.data()));
      outHdrs[2].valuelen = method.size();
      outHdrs[3].value =
          const_cast<uint8_t*>(reinterpret_cast<uint8_t const*>(req.data()));
      outHdrs[3].valuelen = req.size();

      long outLen = nghttp2_hd_deflate_hd(
          connection.m_deflater,
          reinterpret_cast<uint8_t*>(outHdrsFrame.m_data),
          size_t(H2Conn::MaxOutHdrsDataLen),
          outHdrs,
          params.size() ? 5 : 4);
      outHdrsFrame.SetDataLen(int(outLen));
      outHdrsFrame.m_type  = H2Conn::H2FrameTypeT::Headers;
      outHdrsFrame.m_flags = params.size() ? 0x04 : 0x05;

      uint32_t   streamID = connection.StreamIDofReqID(*OMCH2WS::m_ReqN);
      outHdrsFrame.SetStreamID(streamID);

      if (!params.empty())
      {
        connection.m_outDataFrame.SetDataLen(int(params.size()));
        connection.m_outDataFrame.m_type  = H2Conn::H2FrameTypeT::Data;
        connection.m_outDataFrame.m_flags = 0x01;
        connection.m_outDataFrame.SetStreamID(
            connection.StreamIDofReqID(*OMCH2WS::m_ReqN));

        memcpy(
            connection.m_outHdrsFrame.m_data + outLen,
            &connection.m_outDataFrame,
            connection.m_outDataFrame.PrefixLen);
        memcpy(
            connection.m_outHdrsFrame.m_data + outLen
                + connection.m_outDataFrame.PrefixLen,
            params.data(),
            params.size());
        connection.SendImpl(
            reinterpret_cast<char const*>(&outHdrsFrame),
            outHdrsFrame.GetFrameLen()
                + connection.m_outDataFrame.GetFrameLen());
      }
      else
      {
        connection.template SendFrame<false>();
      }

      m_lastSend = utxx::now_utc();
      OMCH2WS::RotateH2Conns();

      // XXX: Do we really need this?
      ++(*OMCH2WS::m_ReqN);
      ++(*OMCH2WS::m_txSN);

      return streamID;
    }

    template <Huobi::AssetClassT::type AC>
    void EConnector_H2WS_Huobi_OMC<AC>::SignAndSendReq(
        std::string_view const& method,
        ReqType                 type,
        std::string_view const& req,
        std::string_view const& params)
    {
      time_t t     = time(nullptr);
      tm*    local = gmtime(&t);

      auto signature =
          CreateSignature20(method, m_h2Host, req, local, params);
      signature = EscapeUrl(signature);

      char* i   = m_buf_params;
      i = i << req << std::string_view("?AccessKeyId=") << m_apiKey
            << std::string_view(
                   "&SignatureMethod=HmacSHA256&SignatureVersion=2&Timestamp=");
      i = print_time(i, local, true);
      if (!params.empty() && method.size() == 3)
      {
        i = i << std::string_view("&") << params;
      }
      i = i << std::string_view("&Signature=") << signature;
      auto streamId = SendReq(
          method,
          std::string_view(m_buf_params, uint32_t(i - m_buf_params)),
          method.size() == 3 ? std::string_view() : params);
      m_streams[streamId] = type;
    }

    template <Huobi::AssetClassT::type AC>
    std::string_view EConnector_H2WS_Huobi_OMC<AC>::EscapeUrl(
        const std::string_view& a_url)
    {
      char* r = m_buf_url;
      for (auto c: a_url)
      {
        if (('0' <= c && c <= '9') || ('a' <= c && c <= 'z')
            || ('A' <= c && c <= 'Z') || c == '/' || c == '.')
        {
          *(r++) = c;
        }
        else
        {
          int j = int(c);
          if (j < 0)
          {
            j += 256;
          }
          int i1, i0;
          i1     = j / 16;
          i0     = j - i1 * 16;
          *(r++) = '%';
          *(r++) = dec2hexChar(uint16_t(i1));
          *(r++) = dec2hexChar(uint16_t(i0));
        }
      }
      return std::string_view(m_buf_url, uint32_t(r - m_buf_url));
    }

    template <Huobi::AssetClassT::type AC>
    std::string_view EConnector_H2WS_Huobi_OMC<AC>::CreateSignature20(
        std::string_view const& method,
        std::string_view const& host,
        std::string_view const& adress,
        tm*                     a_local,
        std::string_view const& params)
    {
      char  signature[256];
      char* i = signature;
      i       = i << method << msg_nl << host << msg_nl << adress << msg_nl;
      i       = i << std::string_view("AccessKeyId=") << m_apiKey
            << std::string_view(
                   "&SignatureMethod=HmacSHA256&SignatureVersion=2&Timestamp=");
      i = print_time(i, a_local, true);
      if (!params.empty() && method.size() == 3)
      {
        i = i << std::string_view("&") << params;
      }

      DEBUG_ONLY(int ok =)
      gnutls_hmac_fast(
          GNUTLS_MAC_SHA256,
          m_apiSecret.data(),
          m_apiSecret.size(),
          signature,
          size_t(i - signature),
          m_signSHA256);
      assert(ok == 0);

      int sz = Base64EnCodeUnChecked(
          m_signSHA256,
          sizeof(m_signSHA256),
          m_sign,
          true);
      return std::string_view(m_sign, uint32_t(sz));
    }

    template <Huobi::AssetClassT::type AC>
    std::string_view EConnector_H2WS_Huobi_OMC<AC>::CreateSignature21(
        tm* a_local)
    {
      char  signature[256];
      char* i = signature;
      i       = i
          << msg_get << msg_nl << m_wsHost << msg_nl
          << std::string_view("/ws/v2\naccessKey=") << m_apiKey
          << std::string_view(
                 "&signatureMethod=HmacSHA256&signatureVersion=2.1&timestamp=");
      i = print_time(i, a_local, true);
      OMCH2WS::RotateH2Conns();

      DEBUG_ONLY(int ok =)
      gnutls_hmac_fast(
          GNUTLS_MAC_SHA256,
          m_apiSecret.data(),
          m_apiSecret.size(),
          signature,
          size_t(i - signature),
          m_signSHA256);
      assert(ok == 0);

      int sz = Base64EnCodeUnChecked(
          m_signSHA256,
          sizeof(m_signSHA256),
          m_sign,
          true);
      return std::string_view(m_sign, uint32_t(sz));
    }
     //-----------------------------------------------------------------------//
     // Implementations:                                                      //
     //-----------------------------------------------------------------------//
     template <Huobi::AssetClassT::type AC>
     void EConnector_H2WS_Huobi_OMC<AC>::PlaceNewOrder(
         SecDefD const& instr,
         QtyN           amount,
         bool           buy,
         PriceT         price)
     {
       if constexpr (IsSpt)
       {
         /*
          * POST v1/order/orders/place
          * '{
             "account-id": "100009",
             "amount": "10.1",
             "price": "100.1",
             "source": "api",
             "symbol": "ethusdt",
             "type": "buy-limit",
             "client-order-id": "a0001"
          }'
          */
          char* i = m_buf_req;
          i       = i << std::string_view(R"({"account-id":")");
          i       = utxx::itoa_left<uint32_t, 10>(i, m_account_id);
          i       = i << std::string_view(R"(","amount":")");
          i += utxx::ftoa_left(double(amount), i, 16, 8);
          i = i << std::string_view(R"(","price":")");
          i += utxx::ftoa_left(double(price), i, 16, 8);
          i = i << std::string_view(R"(","source":"api","symbol":")")
                << instr.m_Symbol << std::string_view(R"(","type":")");
          i = i << std::string_view(buy ? "buy-limit": "sell-limit");
          i = i << std::string_view(R"(","client-order-id":")");
          i = utxx::itoa_left<uint64_t, 10>(i, *OMCH2WS::m_ReqN); // ReqID
          i = i << std::string_view(R"("})");

          SignAndSendReq(
              msg_post,
              ReqType::PlaceOrder,
              std::string_view("/v1/order/orders/place"),
              std::string_view(m_buf_req, uint32_t(i - m_buf_req)));
       }
       else
       {
         /*
          * POST api/v1/contract_order
          {
            "symbol": "BTC",
            "contract_type": "this_week"
            "client-order-id": "a0001"
            "price": 100.1
            "volume":  100
            "direction":  "buy"
            "offset":  "open" (or maybe close?)
            "lever_rate": 1
            "order_price_type":  "limit”
          }

          * POST /swap-api/v1/swap_order
          {
            "contract_code": "BTC-USD",
            "client-order-id": "a0001"
            "price": 100.1
            "volume":  100
            "direction":  "buy"
            "offset":  "open" (or maybe close?)
            "lever_rate": 1
            "order_price_type":  "limit”
          }
          */

          char* i = m_buf_req;

          if constexpr (IsFut) {
            i = i << std::string_view(R"({"symbol":")")
                  << instr.m_Symbol;
            i = i << std::string_view(R"(","contract_type":")")
                  << instr.m_Tenor;
          } else if constexpr (IsSwp) {
            i = i << std::string_view(R"({"contract_code":")")
                  << instr.m_AltSymbol;
          } else {
            static_assert(typed_false<AC>::value, "Unsupported asset");
          }

          i = i << std::string_view(R"(","client-order-id":")");
          i = utxx::itoa_left<uint64_t, 10>(i, *OMCH2WS::m_ReqN); // ReqID
          i = i << std::string_view(R"(","price":)");
          i += utxx::ftoa_left(double(price), i, 16, 8);
          i = i << std::string_view(R"(,"volume":)");
          i += utxx::ftoa_left(double(amount), i, 16, 8);
          i = i << std::string_view(R"(,"direction":")");
          i = i << std::string_view(buy ? "buy" : "sell");
          i = i << std::string_view(
                  R"(","offset":"open","lever_rate":1,"order_price_type":"limit”})");

          auto url = IsFut ? std::string_view("/api/v1/contract_order")
                           : (AC == Huobi::AssetClassT::CSwp
                              ? std::string_view("/swap-api/v1/swap_order")
                              : std::string_view("/linear-swap-api/v1/swap_order"));
          SignAndSendReq(msg_post, ReqType::PlaceOrder, url,
              std::string_view(m_buf_req, uint32_t(i - m_buf_req)));
       }
     }

     template <Huobi::AssetClassT::type AC>
     void EConnector_H2WS_Huobi_OMC<AC>::CancelOrderByClientId(
         uint64_t   client_order_id)
     {
       if constexpr (IsSpt)
       {
          /*
            * POST v1/order/orders/submitCancelClientOrder
              '{
                "client-order-id": "a0001"
              }'
            */
          char* i = m_buf_req;
          i       = i << std::string_view(R"({"client-order-id":")");
          i       = utxx::itoa_left<uint64_t, 20>(i, client_order_id);
          i       = i << std::string_view(R"("})");
          SignAndSendReq(
              msg_post,
              ReqType::CancelOrder,
              std::string_view("/v1/order/orders/submitCancelClientOrder"),
              std::string_view(m_buf_req, uint32_t(i - m_buf_req)));
       }
       else if constexpr (IsFut)
       {
          /*
            * POST api/v1/contract_cancel
              '{
                "client-order-id": "a0001",
                "symbol": "BTC"
              }'
            */
          auto const& symbol = m_orders[client_order_id];

          char* i = m_buf_req;
          i       = i << std::string_view(R"({"client-order-id":")");
          i       = utxx::itoa_left<uint64_t, 20>(i, client_order_id);
          i       = i << std::string_view(R"(","symbol":")") << symbol;
          i       = i << std::string_view(R"("})");
          SignAndSendReq(
              msg_post,
              ReqType::CancelOrder,
              std::string_view("/api/v1/contract_cancel"),
              std::string_view(m_buf_req, uint32_t(i - m_buf_req)));
       }
       else if constexpr (IsSwp)
       {
          /*
            * POST /swap-api/v1/swap_cancel
              '{
                "client-order-id": "a0001",
                "contract_code": "BTC-USD"
              }'
            */
          auto const& symbol = m_orders[client_order_id];

          char* i = m_buf_req;
          i       = i << std::string_view(R"({"client-order-id":")");
          i       = utxx::itoa_left<uint64_t, 20>(i, client_order_id);
          i       = i << std::string_view(R"(","contract_code":")") << symbol;
          i       = i << std::string_view(R"("})");

          auto url = AC == Huobi::AssetClassT::CSwp
                      ? std::string_view("/swap-api/v1/swap_cancel")
                      : std::string_view("/linear-swap-api/v1/swap_cancel");
          SignAndSendReq(
              msg_post,
              ReqType::CancelOrder,
              url,
              std::string_view(m_buf_req, uint32_t(i - m_buf_req)));
       }
       else
       {
         static_assert(typed_false<AC>::value, "Unsupported asset");
       }
     }

     template <Huobi::AssetClassT::type AC>
     void EConnector_H2WS_Huobi_OMC<AC>::RequestOpenOrders(
         const SymKey& symbol)
     {
       if constexpr (IsSpt)
       {
          /*
            * GET v1/order/openOrders
            * account-id
            * symbol
            */
          char* i = m_buf_req;
          i       = i << std::string_view("account-id=");
          i       = utxx::itoa_left<uint32_t, 10>(i, m_account_id);
          i       = i << std::string_view("&symbol=") << symbol;
          SignAndSendReq(
              msg_get,
              ReqType::RequestOrders,
              std::string_view("/v1/order/openOrders"),
              std::string_view(m_buf_req, uint32_t(i - m_buf_req)));
       }
       else if constexpr (IsFut)
       {
          /*
            * POST api/v1/contract_openorders
            * {"symbol": "BTC"}
            */
          char* i = m_buf_req;
          i       = i << std::string_view(R"({"symbol":")");
          i       = i << symbol;
          i       = i << std::string_view(R"("})");
          SignAndSendReq(
              msg_post,
              ReqType::RequestOrders,
              std::string_view("/api/v1/contract_openorders"),
              std::string_view(m_buf_req, uint32_t(i - m_buf_req)));
       }
       else
       {
          /*
            * POST api/v1/swap_openorders
            * {"contract_code": "BTC-USD"}
            */
          char* i = m_buf_req;
          i       = i << std::string_view(R"({"contract_code":")");
          i       = i << symbol;
          i       = i << std::string_view(R"("})");

          auto url = AC == Huobi::AssetClassT::CSwp
            ? std::string_view("/swap-api/v1/swap_openorders")
            : std::string_view("/linear-swap-api/v1/swap_openorders");
          SignAndSendReq(
              msg_post,
              ReqType::RequestOrders,
              url,
              std::string_view(m_buf_req, uint32_t(i - m_buf_req)));
       }
     }

     template<Huobi::AssetClassT::type AC>
     void EConnector_H2WS_Huobi_OMC<AC>::dropOrders()
     {
        for (auto const& v: m_orders)
        {
          CancelOrderByClientId(v.first);
        }
     }

     //=======================================================================//
     // HTTP message processor                                                //
     //=======================================================================//
     template <Huobi::AssetClassT::type AC>
     bool EConnector_H2WS_Huobi_OMC<AC>::ProcessH2Msg(
         H2Conn*        a_h2_conn,
         uint32_t       a_stream_id,
         char const*    a_msg_body,
         int            a_msg_len,
         bool           UNUSED_PARAM(a_last_msg),
         utxx::time_val a_ts_recv,
         utxx::time_val)
     {
       OrderID reqID = a_h2_conn->ReqIDofStreamID(a_stream_id);
       LOG_INFO(
           4,
           "EConnector_H2WS_Huobi_OMC::ProcessH2Msg: StreamID={}, ReqID={}, "
           "Received Msg=[{}]", a_stream_id, reqID, a_msg_body)

       // Check if the msg is complete (if not, it is a malformed msg or a
       // proto- col-level error):
       CHECK_ONLY(assert(a_msg_len > 0); if (utxx::unlikely(
                                                 a_msg_body[a_msg_len - 1] != '}'
                                                 && a_msg_body[a_msg_len - 1]
                                                        != ']')) {
         LOG_ERROR(
             2,
             "EConnector_H2WS_Huobi_OMC::ProcessH2Msg: Truncated JSON Obj: {}, "
             "Len={}",
             a_msg_body,
             a_msg_len)
         // Ignore the message
         return true;
       })

       const char *it = a_msg_body, *ie = it + a_msg_len;

       auto reqIt = m_streams.find(a_stream_id);
       assert(reqIt != m_streams.end());
       ReqType reqType = reqIt->second;
       m_streams.erase(reqIt);

       if (utxx::unlikely(reqType == ReqType::RequestAccounts))
       {
         /* Received account id
          {
            "status":"ok",
            "data":[{"id":11682900,"type":"spot","subtype":"","state":"working"}]
          }*/
         skip_fixed(it, R"({"status":"ok","data":[{"id":)");
         it = utxx::fast_atoi<decltype(m_account_id), false>(
             it,
             std::find(it, ie, ','),
             m_account_id);

         m_currentInitState |= InitState::AccountReceived;
         OnTurningActive();
         return true;
       }

       if (utxx::unlikely(reqType == ReqType::RequestBalance))
       {
         /* Account balances received
         {"status":"ok","data":{"id":11682900,"type":"spot","state":"working",
          "list":[{"currency":"lun","type":"trade","balance":"0"},..]}
         */
         it = std::find(it, ie, '[');
         ++it;

         SymKey ccy;
         while (*it != ']')
         {
           if (*it == ',') it++;
           skip_fixed(it, R"({"currency":")");
           char const* ne = std::find(it, ie, '"');
           InitFixedStr(&ccy, it, size_t(ne - it));
           it = ne;

           if (skip_if_fixed(it, R"(","type":"trade","balance":")"))
           {
             char* after = nullptr;
             double value = strtod(it, &after);
             assert(after > it);
             OMCH2WS::m_balances[ccy] = value;
             it = after;
             skip_fixed(it, R"("})");
           }
           else
           {
             it = std::find(it, ie, '}');
             it++;
             continue;
           }
         }
         m_currentInitState |= InitState::BalanceReceived;
         OnTurningActive();
         return true;
       }

       if (utxx::unlikely(reqType == ReqType::RequestOrders))
       {
         constexpr char ClientOrderId[] = "client-order-id:";
         it = std::find(it, ie, '[');
         ++it;
         SymKey ccy;

         if constexpr (IsSpt)
         {
           /*
            * {"status": "ok",
            * "data": [
               {
                 "id": 5454937,
                 "symbol": "ethusdt",
                 "account-id": 30925,
                 "amount": "1.000000000000000000",
                 "price": "0.453000000000000000",
                 "created-at": 1530604762277,
                 "type": "sell-limit",
                 "filled-amount": "0.0",
                 "filled-cash-amount": "0.0",
                 "filled-fees": "0.0",
                 "source": "web",
                 "state": "submitted"}]}
            */
           while (*it != ']')
           {
             skip_to_next(it, ie, ClientOrderId);
             ++it; // Skip quota
             OrderID orderId;
             it = utxx::fast_atoi<decltype(orderId), false>(
                 it,
                 std::find(it, ie, '"'),
                 orderId);

             skip_fixed(it, R"({","symbol":")");
             char const* ne = std::find(it, ie, '"');
             InitFixedStr(&ccy, it, size_t(ne - it));
             m_orders[orderId] = ccy;
           }
         }
         else
         {
           /* {
               "status": "ok",
               "data":{
                 "orders":[
                   {
                      "symbol": "BTC",
                      "contract_type": "this_week",
                      "contract_code": "BTC180914",
                      "volume": 111,
                      "price": 1111,
                      "order_price_type": "limit",
                      "order_type": 1,
                      "direction": "buy",
                      "offset": "open",
                      "lever_rate": 10,
                      "order_id": 633766664829804544,
                      "order_id_str": "633766664829804544",
                      "client_order_id": 10683,
                      "order_source": "web",
                      "created_at": 1408076414000,
                      "trade_volume": 1,
                      "trade_turnover": 1200,
                      "fee": 0,
                      "trade_avg_price": 10,
                      "margin_frozen": 10,
                      "profit": 0,
                      "status": 1,
                      "fee_asset":"BTC"
                     }
                    ],
                 "total_page":15,
                 "current_page":3,
                 "total_size":3
                },
               "ts": 1490759594752
             } */
           constexpr char Symbol[] = "symbol:";
           while (*it != ']')
           {
             skip_to_next(it, ie, Symbol);
             ++it; // Skip quota
             char const* ne = std::find(it, ie, '"');
             InitFixedStr(&ccy, it, size_t(ne - it));

             skip_to_next(it, ie, ClientOrderId);
             OrderID orderId;
             it = utxx::fast_atoi<decltype(orderId), false>(
                 it,
                 std::find(it, ie, ','),
                 orderId);
             m_orders[orderId] = ccy;
           }
         }
         if (std::none_of(m_streams.begin(), m_streams.end(), [](auto const& v) {
               return v.second == ReqType::RequestOrders;
             }))
         {
            m_currentInitState |= InitState::OrdersReceived;
            dropOrders();
            OnTurningActive();
         }
         return true;
       }

       if (reqType == ReqType::PlaceOrder)
       {
         if constexpr (IsSpt)
         {
           //{"status":"ok","data":"65855194285"}
           if (utxx::likely(skip_if_fixed(it, R"({"status":"ok","data":")")))
           {
             uint64_t order_id = 0;
             it = utxx::fast_atoi<uint64_t, false>(it, ie, order_id);
             EConnector_OrdMgmt::OrderConfirmedNew<QT, QR>(
                 this,
                 this,
                 reqID,
                 0,
                 ExchOrdID(order_id),
                 ExchOrdID(0),
                 PriceT(),
                 QtyN::Invalid(),
                 a_ts_recv,
                 a_ts_recv);
           }
           else
           {
             EConnector_OrdMgmt::OrderRejected<QT, QR>(
                 this,
                 this,
                 0,
                 reqID,
                 0,
                 FuzzyBool::UNDEFINED,
                 0,
                 a_msg_body,
                 a_ts_recv,
                 a_ts_recv);
           }
         }
         else
         {
           /*
            {"status": "ok",
            "order_id": 633766664829804544,
            "order_id_str": "633766664829804544",
            "client_order_id": 9086,
            "ts": 158797866555}
            */
           if (utxx::likely(skip_if_fixed(it, R"({"status":"ok")")))
           {
             constexpr char clientOrderId[] = R"("client_order_id":)";

             skip_to_next(it, ie, clientOrderId);
             uint64_t order_id = 0;
             it = utxx::fast_atoi<uint64_t, false>(it, ie, order_id);
             EConnector_OrdMgmt::OrderConfirmedNew<QT, QR>(
                 this,
                 this,
                 reqID,
                 0,
                 ExchOrdID(order_id),
                 ExchOrdID(0),
                 PriceT(),
                 QtyN::Invalid(),
                 a_ts_recv,
                 a_ts_recv);
           }
           else
           {
             EConnector_OrdMgmt::OrderRejected<QT, QR>(
                 this,
                 this,
                 0,
                 reqID,
                 0,
                 FuzzyBool::UNDEFINED,
                 0,
                 a_msg_body,
                 a_ts_recv,
                 a_ts_recv);
           }
         }
         return true;
       }

       // Notify if CancelOrder or CancelAllOrders has errors
       if (reqType == ReqType::CancelOrder
           || reqType == ReqType::CancelAllOrders)
       {
         if (utxx::unlikely(!skip_if_fixed(it, R"({"status":"ok")")))
         {
           LOG_INFO(
               3,
               "EConnector_H2WS_Huobi_OMC::ProcessH2Msg: cancel error.",
               a_msg_body)
         }
         return true;
       }

         LOG_INFO(
             3,
             "EConnector_H2WS_Huobi_OMC::ProcessH2Msg: unsupported message: {}",
             a_msg_body)
       return true;
     }

     //=======================================================================//
     // Websocket stream processor                                            //
     //=======================================================================//
     template <Huobi::AssetClassT::type AC>
     bool EConnector_H2WS_Huobi_OMC<AC>::ProcessWSMsg(
         char const* a_msg_body,
         int         a_msg_len,
         bool,
         utxx::time_val a_ts_recv,
         utxx::time_val)
     {
       LOG_INFO(3, "EConnector_H2WS_Huobi_OMC::ProcessWSMsg()")
       m_lastWsMsg = a_ts_recv;

       auto        c  = m_zlib->decompress(a_msg_body, uint32_t(a_msg_len));
       char const *it = c.first, *ie = c.second;

       static constexpr char Notify[] = R"(notify",)";
       static constexpr char Ping[]   = R"(ping","ts":)";
       static constexpr char Sub[]    = R"(sub",)";
       static constexpr char Auth[]   = R"(auth",)";

       skip_fixed(it, "{\"");
       if (skip_if_fixed(it, R"(op":")"))
       {
         if (skip_if_fixed(it, Notify))
         {
           if constexpr (IsSpt)
           {
             skip_fixed(it, R"("ts":)");
             uint64_t msTs;
             it = utxx::fast_atoi<uint64_t, false>(it, it + 13, msTs);
             constexpr char OrdersUpdate[] =
                 R"(","topic":"orders.*.update","data":{")";
             constexpr char AccountsUpdate[] =
                 R"(","topic":"accounts","data":{")";
             if (skip_if_fixed(it, OrdersUpdate))
             {
               // Orders update:
               /*{
                 "op": "notify",
                 "ts": 1522856623232,
                 "topic": "orders.htusdt.update",
                 "data": {
                   "unfilled-amount": "0.000000000000000000",
                   "filled-amount": "5000.000000000000000000",
                   "price": "1.662100000000000000",
                   "order-id": 2039498445,
                   "symbol": "htusdt",
                   "match-id": 94984,
                   "filled-cash-amount": "8301.357280000000000000",
                   "role": "taker|maker",
                   "order-state": "filled",
                   "client-order-id": "a0001",
                   "order-type": "buy-limit"
                 }}
                */
               char const* ne = ie - 2;
               skip_fixed(ne, "}}");
               Huobi::json_parser<11> orders_update;
               orders_update.set(it, ie - 2);

               QtyN filled_amount =
                   orders_update.get("filled-amount", read_count, true);
               PriceT   price = orders_update.get("price", read_price, true);
               uint64_t order_id =
                   orders_update.get("order-id", read_int<uint64_t>);
               uint32_t client_order_id =
                   orders_update.get("client-order-id", read_int<uint32_t>, true);
               auto order_type =
                   orders_update.get("order-type", true); // sell-limit,
                                                            // buy-limit,
                                                            // ...

               QtyN unfilled_amount =
                   orders_update.get("unfilled-amount", read_count, true);
               auto role = orders_update.get("role", true); // maker, taker
               uint64_t match_id =
                   orders_update.get("match-id", read_int<uint64_t>);
               auto order_state =
                   orders_update.get("order-state", true); // submitted, canceled,
                                                             // partial-filled,
                                                             // filled,
                                                             // partial-canceled

               if (order_state == "submitted")
               {
                 EConnector_OrdMgmt::OrderConfirmedNew<QT, QR>(
                     this,
                     this,
                     client_order_id,
                     0,
                     ExchOrdID(order_id),
                     ExchOrdID(0),
                     price,
                     unfilled_amount,
                     utxx::time_val(utxx::msecs(msTs)),
                     a_ts_recv);
               }
               else if (order_state == "canceled" || order_state == "partial_canceled")
               {
                 OrderID clx_id = 0;
                 auto    itc    = m_cancels.find(client_order_id);
                 if (itc != m_cancels.end())
                 {
                   clx_id = itc->second;
                   m_cancels.erase(itc);
                 }

                 EConnector_OrdMgmt::OrderCancelled<QT, QR>(
                     this,
                     this,
                     clx_id,
                     client_order_id,
                     0,
                     ExchOrdID(0),
                     ExchOrdID(0),
                     price,
                     unfilled_amount,
                     utxx::time_val(utxx::msecs(msTs)),
                     a_ts_recv);
               }
               else
               {
                 FIX::SideT side, aggr_side;
                 if (order_type == "sell-limit")
                 {
                   side = FIX::SideT::Sell;
                   if (role == "maker")
                   {
                     aggr_side = FIX::SideT::Buy;
                   }
                   else
                   {
                     assert(role == "taker");
                     aggr_side = FIX::SideT::Sell;
                   }
                 }
                 else if (order_type == "buy-limit")
                 {
                   side = FIX::SideT::Buy;
                   if (role == "maker")
                   {
                     aggr_side = FIX::SideT::Sell;
                   }
                   else
                   {
                     assert(role == "taker");
                     aggr_side = FIX::SideT::Buy;
                   }
                 }
                 else
                   throw utxx::runtime_error(
                       "EConnector_H2WS_Huobi_OMC::ProcessWSMsg: add support for order-type: ",
                       std::string(c.first, c.second));

                 FuzzyBool filled =
                     (order_state == "partial-filled" ? FuzzyBool::False :
                                                        FuzzyBool::True);
                 if (order_state == "filled")
                 {
                   auto itc = m_cancels.find(client_order_id);
                   if (itc != m_cancels.end())
                   {
                     auto itn = m_orders.find(itc->second + 1);
                     if (itn != m_orders.end())
                     {
                       // possible order loss, try cancel
                       LOG_INFO(
                           3,
                           "EConnector_H2WS_Huobi_OMC::ProcessWSMsg: cancel new order after modify and fill: {} -> {}",
                           itc->first,
                           itn->first)
                       CancelOrderByClientId(itn->first);
                       m_orders.erase(itn);
                     }
                     m_cancels.erase(itc);
                   }
                   m_orders.erase(client_order_id);
                 }

                 EConnector_OrdMgmt::OrderTraded<QT, QR, QT, QR>(
                     this,
                     this,
                     nullptr,
                     client_order_id,
                     0,
                     nullptr,
                     side,
                     aggr_side,
                     ExchOrdID(order_id),
                     ExchOrdID(0),
                     ExchOrdID(match_id),
                     PriceT(),
                     price,
                     filled_amount,
                     unfilled_amount,
                     QtyN::Invalid(),
                     QtyN::Invalid(),
                     0,
                     filled,
                     utxx::time_val(utxx::msecs(msTs)),
                     a_ts_recv);
               }

             }
             else if (skip_if_fixed(it, AccountsUpdate))
             {
               // Account update
               /**{"op": "notify",
                "ts": 1522856623232,
                "topic": "accounts",
                "data": {
                  "event": "order.place",
                  "list": [
                    { "account-id": 419013,
                      "currency": "usdt",
                      "type": "trade",
                      "balance": "500009195917.4362872650"
                    }]}}*/

               it = std::find(it, ie, '[');
               SymKey ccy;
               while (*it != ']')
               {
                 if (*it == ',')
                   it++;
                 skip_fixed(it, R"({"account-id":")");
                 uint32_t account_id = 0;
                 it = utxx::fast_atoi<decltype(m_account_id), false>(
                     it,
                     std::find(it, ie, ','),
                     account_id);
                 if (utxx::unlikely(account_id != m_account_id))
                 {
                   throw utxx::runtime_error(
                       "EConnector_H2WS_Huobi_OMC::ProcessWSMsg: invalid account id");
                 }

                 skip_fixed(it, R"({,"currency":")");
                 char const* ne = std::find(it, ie, '"');
                 InitFixedStr(&ccy, it, size_t(ne - it));
                 it = ne;

                 skip_fixed(it, R"({,"type":")");
                 skip_to_next(it, ie, ',');

                 skip_fixed(it, R"(,"balance":")");
                 char*  after = nullptr;
                 double value = strtod(it, &after);
                 assert(after > it);
                 OMCH2WS::m_balances[ccy] = value;
                 it                     = after;
                 skip_fixed(it, R"("})");
               }
             }
           }
           else
           {
             constexpr char OrdersUpdate[] =
                 R"("topic":"orders.*",)";
             constexpr char PositionsUpdate[] =
                 R"("topic":"positions")";
             if (skip_if_fixed(it, OrdersUpdate))
             {
               /* {"op": "notify",
                    "topic": "orders.*",
                    "ts": 1489474082831,
                    "symbol": "BTC",
                    "contract_type": "this_week",
                    "contract_code": "BTC180914",
                    "volume": 111,
                    "price": 1111,
                    "order_price_type": "limit",
                    "direction": "buy",
                    "offset": "open",
                    "status": 6,
                    "lever_rate": 10,
                    "order_id": 633989207806582784,
                    "order_id_str": "633989207806582784",
                    "client_order_id": 10683,
                    "order_source": "web",
                    "order_type": 1,
                    "created_at": 1408076414000,
                    "trade_volume": 1,
                    "trade_turnover": 1200,
                    "fee": 0,
                    "trade_avg_price": 10,
                    "margin_frozen": 10,
                    "profit": 2,
                    "fee_asset":"BTC",
                    "trade": [{
                        "id": "2131234825-6124591349-1",
                        "trade_id": 112,
                        "trade_volume": 1,
                        "trade_price": 123.4555,
                        "trade_fee": 0.234,
                        "trade_turnover": 34.123,
                        "created_at": 1490759594752,
                        "role": "maker"
                    }]}
               */
                skip_fixed(it, R"("ts":)");
                uint64_t msTs;
                it = utxx::fast_atoi<uint64_t, false>(it, it + 13, msTs);

                skip_to_next(it, ie, R"("volume":)");
                QtyN volume = read_count(it, std::find(it, ie, ','));

                skip_to_next(it, ie, R"("price":)");
                PriceT price = read_price(it, std::find(it, ie, ','));

                skip_to_next(it, ie, R"("status":)");
                int status = read_int<int>(it, std::find(it, ie, ','));

                skip_to_next(it, ie, R"("order-id":)");
                uint64_t order_id = read_int<uint64_t>(it, std::find(it, ie, ','));

                skip_to_next(it, ie, R"("client-order-id":)");
                uint64_t client_order_id =
                    read_int<uint64_t>(it, std::find(it, ie, ','));

                skip_to_next(it, ie, R"("fee":)");
                QtyN fee = read_count(it, std::find(it, ie, ','));

                // 3. Placed to order book
                if (status == 3)
                {
                  EConnector_OrdMgmt::OrderConfirmedNew<QT, QR>(
                      this,
                      this,
                      client_order_id,
                      0,
                      ExchOrdID(order_id),
                      ExchOrdID(0),
                      price,
                      volume,
                      utxx::time_val(utxx::msecs(msTs)),
                      a_ts_recv);
                }

                // 5 partially fulfilled but cancelled by client;
                // 7. Cancelled;
                if (status == 5 || status == 7)
                {
                  OrderID clx_id = 0;
                  auto    itc    = m_cancels.find(client_order_id);
                  if (itc != m_cancels.end())
                  {
                    clx_id = itc->second;
                    m_cancels.erase(itc);
                  }

                  EConnector_OrdMgmt::OrderCancelled<QT, QR>(
                      this,
                      this,
                      clx_id,
                      client_order_id,
                      0,
                      ExchOrdID(0),
                      ExchOrdID(0),
                      price,
                      volume,
                      utxx::time_val(utxx::msecs(msTs)),
                      a_ts_recv);
                }

                // 4. Partially fulfilled;
                // 6. Fully fulfilled;
                if (status == 4 || status == 6)
                {
                  FuzzyBool filled =
                      (status == 4 ? FuzzyBool::False :
                                                         FuzzyBool::True);
                  if (status == 6)
                  {
                    auto itc = m_cancels.find(client_order_id);
                    if (itc != m_cancels.end())
                    {
                      auto itn = m_orders.find(itc->second + 1);
                      if (itn != m_orders.end())
                      {
                        // possible order loss, try cancel
                        LOG_INFO(
                            3,
                            "EConnector_H2WS_Huobi_OMC::ProcessWSMsg: cancel new order after modify and fill: {} -> {}",
                            itc->first,
                            itn->first)
                        CancelOrderByClientId(itn->first);
                        m_orders.erase(itn);
                      }
                      m_cancels.erase(itc);
                    }
                    m_orders.erase(client_order_id);
                  }

                  EConnector_OrdMgmt::OrderTraded<QT, QR, QT, QR>(
                      this,
                      this,
                      nullptr,
                      client_order_id,
                      0,
                      nullptr,
                      FIX::SideT::UNDEFINED,
                      FIX::SideT::UNDEFINED,
                      ExchOrdID(order_id),
                      ExchOrdID(0),
                      ExchOrdID(0),
                      PriceT(),
                      price,
                      volume,
                      QtyN::Invalid(),
                      QtyN::Invalid(),
                      QtyN(fee),
                      0,
                      filled,
                      utxx::time_val(utxx::msecs(msTs)),
                      a_ts_recv);
                }
                return true;
             }
             else if (skip_if_fixed(it, PositionsUpdate))
             {
               if constexpr (IsFut)
               {
                 // Positions update
                 /*
                  {
                      "op": "notify",
                      "topic": "positions",
                      "ts": 1489474082831,
                      "event": "order.match",
                      "data": [{
                          "symbol": "BTC",
                          "contract_code": "BTC180914",
                          "contract_type": "this_week",
                          "volume": 1,
                          "available": 0,
                          "frozen": 1,
                          "cost_open": 422.78,
                          "cost_hold": 422.78,
                          "profit_unreal": 0.00007096,
                          "profit_rate": 0.07,
                          "profit": 0.97,
                          "position_margin": 3.4,
                          "lever_rate": 10,
                          "direction": "buy",
                          "last_price": 9584.41
                      }]
                  }
                */
                 it = std::find(it, ie, '[');
                 ++it;
                 SymKey ccy;
                 while (*it != ']')
                 {
                   if (*it == ',')
                     it++;
                   skip_fixed(it, R"({"symbol":")");
                   char const* ne = std::find(it, ie, '"');
                   InitFixedStr(&ccy, it, size_t(ne - it));
                   it = ne;

                   SymKey contractType;
                   skip_to_next(it, ie, R"("contract_type":")");
                   ne = std::find(it, ie, '"');
                   InitFixedStr(&contractType, it, size_t(ne - it));
                   it = ne;

                   skip_to_next(it, ie, R"("volume":)");
                   uint16_t volume = 0;
                   it = utxx::fast_atoi<decltype(volume), false>(
                       it,
                       std::find(it, ie, ','),
                       volume);

                   // Instrument name is <symbol>-<exchange>-<tenor> for Futures market
                   char instrName[32];
                   char* nm = instrName;
                   nm = stpcpy(nm, ccy.data());
                   nm = stpcpy(nm, "-Huobi-");
                   nm = stpcpy(nm, contractType.data());

                   SecDefD const* sd = this->m_secDefsMgr->FindSecDefOpt(instrName);
                   if (sd != nullptr)
                   {
                     OMCH2WS::m_positions[sd] = volume;
                   }
                   skip_to_next(it, ie, '}');
                 }
               }
               else
               {
                 // Positions update
                 /*
                  {
                      "op": "notify",
                      "topic": "positions",
                      "ts": 1489474082831,
                      "event": "order.match",
                      "data": [{
                          "symbol": "BTC",
                          "contract_code": "BTC-USD",
                          "volume": 1,
                          "available": 0,
                          "frozen": 1,
                          "cost_open": 422.78,
                          "cost_hold": 422.78,
                          "profit_unreal": 0.00007096,
                          "profit_rate": 0.07,
                          "profit": 0.97,
                          "position_margin": 3.4,
                          "lever_rate": 10,
                          "direction": "buy",
                          "last_price": 9584.41
                      }]
                  }
                */
                 it = std::find(it, ie, '[');
                 ++it;
                 SymKey ccy;
                 while (*it != ']')
                 {
                   if (*it == ',')
                     it++;
                   skip_fixed(it, R"({"symbol":")");
                   char const* ne = std::find(it, ie, '"');
                   InitFixedStr(&ccy, it, size_t(ne - it));
                   it = ne;

                   SymKey contractType;
                   skip_to_next(it, ie, R"("contract_code":")");
                   ne = std::find(it, ie, '"');
                   InitFixedStr(&contractType, it, size_t(ne - it));
                   it = ne;

                   skip_to_next(it, ie, R"("volume":)");
                   uint16_t volume = 0;
                   it              = utxx::fast_atoi<decltype(volume), false>(
                       it,
                       std::find(it, ie, ','),
                       volume);

                   // Instrument name is <symbol>-<exchange>-SWP for Swap
                   // market
                   char  instrName[32];
                   char* nm = instrName;
                   nm       = stpcpy(nm, ccy.data());
                   nm       = stpcpy(nm, "-Huobi-SWP");

                   SecDefD const* sd =
                       this->m_secDefsMgr->FindSecDefOpt(instrName);
                   if (sd != nullptr)
                   {
                     OMCH2WS::m_positions[sd] = volume;
                   }
                   skip_to_next(it, ie, '}');
                 }
               }
             }
           }
           return true;
         }
         else if (skip_if_fixed(it, Ping))
         {
           /*
             Ping message (both Spot and Futures):
             {"op": "ping",
             "ts": 1492420473058}

             We are to reply with pong message:
             {"op": "pong",
             "ts": 1492420473058}
             NB: for unknown reason futures market message has quotas in "ts"
             field.
            */
           if (*it == '\"')
             it++;
           strncpy(pong_msg + 18, it, 13);

           ProWS::PutTxtData(pong_msg);
           ProWS::SendTxtFrame();
           return true;
         }
         else if (skip_if_fixed(it, Sub))
         {
           // Checks if subscription is succesful
           int code = GetErrorCode(it, ie);
           if (utxx::unlikely(code != 0))
           {
             throw utxx::runtime_error(
                 "EConnector_H2WS_Huobi_OMC::ProcessWSMsg: Subscribe failure: ",
                 std::string(c.first, c.second));
           }
           else
           {
             LOG_INFO(
                 4,
                 "EConnector_H2WS_Huobi_OMC::ProcessWSMsg: Subscription successful: {}",
                 c.first)
           }
           return true;
         }
         else if (skip_if_fixed(it, Auth))
         {
           // Skip till err-code field
           int code = GetErrorCode(it, ie);
           if (utxx::unlikely(code != 0))
           {
             throw utxx::runtime_error(
                   "EConnector_H2WS_Huobi_OMC::ProcessWSMsg: Auth failure: ",
                   std::string(c.first, c.second));
           }
           LOG_INFO(3, "EConnector_H2WS_Huobi_OMC::ProcessWSMsg: Auth success")

           OMCH2WS::WSLogOnCompleted();
           return true;
         }
       }

       *(const_cast<char*>(reinterpret_cast<char const*>(c.second))) = '\0';
       LOG_INFO(
           3,
           "EConnector_H2WS_Huobi_OMC::ProcessWSMsg: unsupported message: {}",
           c.first)
       return true;
     }

  //-------------------------------------------------------------------------//
  // Explicit Instances:                                                     //
  //-------------------------------------------------------------------------//
  template class EConnector_H2WS_Huobi_OMC<Huobi::AssetClassT::Spt>;
  template class EConnector_H2WS_Huobi_OMC<Huobi::AssetClassT::Fut>;
  template class EConnector_H2WS_Huobi_OMC<Huobi::AssetClassT::CSwp>;
  template class EConnector_H2WS_Huobi_OMC<Huobi::AssetClassT::USwp>;
}
// namespace MAQUETTE
