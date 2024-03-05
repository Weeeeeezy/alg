// vim:ts=2:et
//===========================================================================//
//          "Connectors/H2WS/BitMEX/EConnector_H2WS_BitMEX_OMC.cpp":         //
//                          H2WS-Based OMC for BitMEX                        //
//===========================================================================//
#include "Connectors/H2WS/BitMEX/EConnector_H2WS_BitMEX_OMC.h"
#include "Connectors/H2WS/EConnector_H2WS_OMC.hpp"
#include "Connectors/H2WS/EConnector_WS_OMC.hpp"
#include "InfraStruct/SecDefsMgr.h"
#include "Protocols/H2WS/WSProtoEngine.hpp"
#include "Venues/BitMEX/SecDefs.h"
#include "Venues/BitMEX/Configs_H2WS.h"
#include <gnutls/crypto.h>

using namespace std;

//===========================================================================//
// JSON Parsing Macros:                                                      //
//===========================================================================//
//---------------------------------------------------------------------------//
// "SKP_STR": Skip a fixed (known) string:                                   //
//---------------------------------------------------------------------------//
#ifdef  SKP_STR
#undef  SKP_STR
#endif
#define SKP_STR(Str) \
  { \
    constexpr size_t strLen = sizeof(Str)-1; \
    assert(strncmp(Str, curr, strLen) == 0); \
    curr += strLen; \
    assert(int(curr - a_msg_body) <= a_msg_len); \
  }

//---------------------------------------------------------------------------//
// "GET_STR": "Var" will hold a 0-terminated string:                         //
//---------------------------------------------------------------------------//
#ifdef  GET_STR
#undef  GET_STR
#endif
#define GET_STR(Var) \
  char const* Var = curr;    \
  curr  = strchr(curr, '"'); \
  assert(curr != nullptr && int(curr - a_msg_body) < a_msg_len); \
  /* 0-terminate the Var string: */ \
  *const_cast<char*>(curr) = '\0';  \
  ++curr;

namespace
{
  using namespace MAQUETTE;

  //=========================================================================//
  // Utils:                                                                  //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "SkipNFlds": Skip "a_n" comma-separated fields:                         //
  //-------------------------------------------------------------------------//
  // More precisely, skip "a_n" commas and return thr ptr to the char after the
  // last skipped comma:
  //
  template<unsigned N, bool Strict=true>
  inline char const* SkipNFlds(char const* a_curr)
  {
    if (Strict)
      assert(a_curr != nullptr);
    // XXX: The loop should be unrolled by the optimizer:
    for (unsigned i = 0; i < N; ++i)
    {
      a_curr = strchr(a_curr, ',');
      if (Strict)
        assert(a_curr != nullptr);
      ++a_curr;
    }
    return a_curr;
  }
}

namespace MAQUETTE
{
  //=========================================================================//
  // "GetSecDefs":                                                           //
  //=========================================================================//
  std::vector<SecDefS> const* EConnector_H2WS_BitMEX_OMC::GetSecDefs()
  { return &BitMEX::SecDefs; }

  //=========================================================================//
  // Config and Account Mgmt:                                                //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "GetH2Config":                                                          //
  //-------------------------------------------------------------------------//
  H2WSConfig const& EConnector_H2WS_BitMEX_OMC::GetH2Config(MQTEnvT a_env)
  {
    //NB: For BitMEX, both "Prod" and "Test" are available:
    auto cit = BitMEX::Configs_WS_MDC.find(a_env);

    if (utxx::unlikely(cit == BitMEX::Configs_WS_MDC.cend()))
      throw utxx::badarg_error
            ("EConnector_H2WS_BitMEX_OMC::GetH2Config: UnSupported Env=",
             a_env.to_string());
    return cit->second;
  }

  H2WSConfig const& EConnector_H2WS_BitMEX_OMC::GetWSConfig(MQTEnvT a_env)
  {
    //NB: For BitMEX, both "Prod" and "Test" are available:
    auto cit = BitMEX::Configs_WS_MDC.find(a_env);

    if (utxx::unlikely(cit == BitMEX::Configs_WS_MDC.cend()))
      throw utxx::badarg_error
            ("EConnector_H2WS_BitMEX_OMC::GetWSConfig: UnSupported Env=",
             a_env.to_string());
    return cit->second;
  }

  //=========================================================================//
  // Non-Default Ctor:                                                       //
  //=========================================================================//
  EConnector_H2WS_BitMEX_OMC::EConnector_H2WS_BitMEX_OMC
  (
    EPollReactor*                       a_reactor,
    SecDefsMgr*                         a_sec_defs_mgr,
    vector<string>              const*  a_only_symbols,  // NULL=UseFullList
    RiskMgr*                            a_risk_mgr,
    boost::property_tree::ptree const&  a_params
  )
  : //-----------------------------------------------------------------------//
    // "EConnector": Virtual Base:                                           //
    //-----------------------------------------------------------------------//
    EConnector
    (
      a_params.get<std::string>("AccountKey"),
      "BitMEX",
      0,                                // XXX: No extra ShM data as yet...
      a_reactor,
      false,                            // No BusyWait
      a_sec_defs_mgr,
      BitMEX::SecDefs,
      // NB: If an explicit list of Symbols is provided, use it (eg to prevent
      // construction of all "SecDefD"s/"OrderBook"s with the default SettlDat-
      // es); otherwise, use the full list. In either case, the list may furt-
      // ther be restricted by SettlDates configured in Params:
      a_only_symbols,
      a_params.get<bool>("ExplSettlDatesOnly", false),
      false,                            // Presumably no Tenors in SecIDs
      a_risk_mgr,
      a_params,
      QT,
      std::is_floating_point_v<QR>
    ),
    //-----------------------------------------------------------------------//
    // "EConnector_H2WS_OMC":                                                //
    //-----------------------------------------------------------------------//
    OMCH2WS(a_params)
  {
    //-----------------------------------------------------------------------//
    // Out-Bound Headers:                                                    //
    //-----------------------------------------------------------------------//
    // [0]: ":authoriry" (created by Parent Ctor)
    // [1]: ":scheme"    (ditto)
    // [2]: ":method"    (ditto, name only)
    // [3]: ":path"      (ditto, name only)
    // [4]: "api-key":
    //
    // NB: At this point, all H2Conns are NotActive:
    assert(OMCH2WS::m_activeH2Conns.empty());

    for (auto h2conn: OMCH2WS::m_notActvH2Conns)
    {
      assert(h2conn != nullptr);

      auto& outHdrs       = h2conn->m_outHdrs;    // Ref!
      // [4]: "api-key" (name and value)
      outHdrs[4].name     = const_cast<uint8_t*>
                            (reinterpret_cast<uint8_t const*>("api-key"));
      outHdrs[4].namelen  = 7;
      outHdrs[4].value    = const_cast<uint8_t*>
                            (reinterpret_cast<uint8_t const*>
                            (m_account.m_APIKey.data()));
      outHdrs[4].valuelen = (m_account.m_APIKey.size());

      // [5]: "api-expires"   (name only):
      outHdrs[5].name     = const_cast<uint8_t*>
                            (reinterpret_cast<uint8_t const*>("api-expires"));
      outHdrs[5].namelen  = 11;

      // [6]: "api-signature" (name only):
      outHdrs[6].name     = const_cast<uint8_t*>
                            (reinterpret_cast<uint8_t const*>("api-signature"));
      outHdrs[6].namelen  = 13;

      // [7]: "content-type"
      outHdrs[7].name     = const_cast<uint8_t*>
                            (reinterpret_cast<uint8_t const*>("content-type"));
      outHdrs[7].namelen  = 12;
      outHdrs[7].value    = const_cast<uint8_t*>
                            (reinterpret_cast<uint8_t const*>
                            ("application/json"));
      outHdrs[7].valuelen = 16;
    }

    // The following assert ensures that our static SHA256 sizes are correct:
    assert(gnutls_hmac_get_len(GNUTLS_MAC_SHA256) == 32);

    // Clear handshake buffer
    memset(m_initBuff, '\0', sizeof(m_initBuff));
  }

  //=========================================================================//
  // Session-Level Msg Processing:                                           //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "OnRstStream":                                                          //
  //-------------------------------------------------------------------------//
  void EConnector_H2WS_BitMEX_OMC::OnRstStream
  (
    H2Conn*        a_h2conn,
    uint32_t       a_stream_id,
    uint32_t       a_err_code,
    utxx::time_val a_ts_recv
  )
  {
    assert(a_h2conn != nullptr);
    OrderID reqID = a_h2conn->ReqIDofStreamID(a_stream_id);

    // Generally on BitMEX this is a no-op, since BitMEX tends to clean up
    // older streams. However, errCode 7 (stream refused) actually means
    // rejection:
    if (utxx::likely(reqID != 0) && a_err_code == 7)
    {
      // NB: The following call will  do nothing is the corresp order is
      // already Inactive (eg the Server has closed an old unused Stream):
      (void) OrderRejected<QT, QR>
      (
        this,                   // ProtoEng
        this,                   // SessMgr
        0,                      // No SeqNums here
        reqID,
        0,                      // AOS ID is not known from the Protocol
        FuzzyBool::UNDEFINED,   // Not sure of whole AOS non-existent
        int(a_err_code),
        "",
        a_ts_recv,              // ExchTS is not known
        a_ts_recv
      );
      a_h2conn->template StopNow<false>("OnRstStream", a_ts_recv);
    }

    LOG_WARN(2, "EConnector_H2WS_BitMEX_OMC::OnRstSream: ConnH2: StreamID={}, "
                "ReqID={}", a_stream_id, reqID)
  }

  //-------------------------------------------------------------------------//
  // "OnHeader":                                                             //
  //-------------------------------------------------------------------------//
  // Process specific HTTP codes, e.g. 503, meaning heavy load
  void EConnector_H2WS_BitMEX_OMC::OnHeaders
  (
    H2Conn*        a_h2conn,
    uint32_t       a_stream_id,
    char const*    a_http_code,
    char const*    UNUSED_PARAM(a_headers),
    utxx::time_val a_ts_recv
  )
  {
    assert(a_h2conn != nullptr);
    OrderID reqID = a_h2conn->ReqIDofStreamID(a_stream_id);

    if (utxx::unlikely(strncmp(a_http_code, "50", 2) == 0 ||
                       strncmp(a_http_code, "4", 1)  == 0))
    // Error 503: heavy load on BitMEX, our req has not been processed
      (void) OrderRejected<QT, QR>
      (
        this,                   // ProtoEng
        this,                   // SessMgr
        0,                      // No SeqNums here
        reqID,
        0,                      // AOS ID is not known from the Protocol
        FuzzyBool::UNDEFINED,   // Not sure of whole AOS non-existent
        0,
        "",
        a_ts_recv,              // ExchTS is not known
        a_ts_recv
      );

    LOG_INFO(2, "EConnector_H2WS_BitMEX_OMC::OnHeaders: ConnH2: StreamID={}, "
                "ReqID={}", a_stream_id, reqID)
  }

  //=========================================================================//
  // "TCP_Connector" Call-Backs:                                             //
  //=========================================================================//
  //=========================================================================//
  // "StartWSIfReady":                                                       //
  //=========================================================================//
  inline void EConnector_H2WS_BitMEX_OMC::StartWSIfReady()
  {
    bool ok = !OMCWS::IsActive() && m_posReceived;
    if (ok)
    {
      OMCH2WS::ProWS::InitWSHandShake(m_initBuff);
      // We can now start the WS Leg:
      LOG_INFO(1,
        "EConnector_H2WS_BitMEX_OMC::StartWSIfReady: Started WS Leg.")
    }
    // Clear handshake buffer
    memset(m_initBuff, '\0', sizeof(m_initBuff));
  }

  //-------------------------------------------------------------------------//
  // "InitWSSession":                                                        //
  //-------------------------------------------------------------------------//
  void EConnector_H2WS_BitMEX_OMC::InitWSSession()
  {
    LOG_INFO(2, "EConnector_H2WS_BitMEX_OMC::InitWSSession")

    // Clean up
    m_posReceived = false;
    EConnector_OrdMgmt::m_positions.clear();

    // Create handshake signature
    char tmpBuff[2048];

    time_t   expires    = time(nullptr) + 5;
    char     buff5[16];
    char*    after      = utxx::itoa_left<long>(buff5, long(expires));
    *after = '\0';

    char* curr = stpcpy (tmpBuff, "GET");
    curr       = stpcpy (curr,    "/realtime");
    curr       = stpcpy (curr,    buff5);
    *curr      = '\0';
    size_t tmpBuffLen = size_t(curr - tmpBuff);
    assert(tmpBuffLen < sizeof(tmpBuff));

    // Compute the signature of "tmpBuff" and put it into "signSHA256":
    uint8_t signSHA256[32];

    DEBUG_ONLY(int ok =)
      gnutls_hmac_fast(GNUTLS_MAC_SHA256,
                       m_account.m_APISecret.data(),
                       m_account.m_APISecret.size(),
                       tmpBuff, tmpBuffLen, signSHA256);
    assert(ok == 0);

    // Convert "signSHA256" into a a HEX string ("signSHA256Hex"). SHA256HexLen
    // is 2*32+1=65 bytes  (including the 0-terminator):
    gnutls_datum_t signHMAC{ signSHA256, uint8_t(sizeof(signSHA256)) };
    char   signSHA256Hex[65];
    size_t signSHA256HexLen = sizeof(signSHA256Hex);
    memset(signSHA256Hex, '\0', signSHA256HexLen);

    DEBUG_ONLY(ok =)
      gnutls_hex_encode(&signHMAC, signSHA256Hex, &signSHA256HexLen);

    assert(ok == 0);
    assert(signSHA256HexLen > 0);
    assert(signSHA256HexLen <= sizeof(signSHA256Hex));

    char* b = stpcpy(m_initBuff, "/realtime?api-expires=");
    b = stpcpy(b, buff5);
    b = stpcpy(b, "&api-signature=");
    b = stpcpy(b, signSHA256Hex);
    b = stpcpy(b, "&api-key=");
    b = stpcpy(b, m_account.m_APIKey.data());

    // Do NOT do handshake until the positions have been received
    SetPosTimer();
  }

  //-------------------------------------------------------------------------//
  // "InitWSLogOn":                                                          //
  //-------------------------------------------------------------------------//
  // XXX: Any non-trivial actions required?
  //
  void EConnector_H2WS_BitMEX_OMC::InitWSLogOn()
  {
    CancelAllOrders(0, nullptr);
    // Signal comletion immediately:
    OMCH2WS::WSLogOnCompleted();
  }

  //-------------------------------------------------------------------------//
  // "OnTurningActive":                                                      //
  //-------------------------------------------------------------------------//
  // Nothing to do at the moment:
  void EConnector_H2WS_BitMEX_OMC::OnTurningActive()
  {
    LOG_INFO(2, "EConnector_H2WS_BitMEX_OMC::OnTurningActive")
  }

  //=========================================================================//
  // "ProcessH2Msg":                                                         //
  //=========================================================================//
  // Processing of  msgs received via HTTP/2 and WebSocket occurs here. The me-
  // thod returns "true" to indicate continuation, "false" to stop:
  //
  bool EConnector_H2WS_BitMEX_OMC::ProcessH2Msg
  (
    H2Conn*         a_h2conn,    // Non-NULL
    uint32_t        a_stream_id,
    char const*     a_msg_body,  // Non-NULL
    int             a_msg_len,
    bool            UNUSED_PARAM(a_last_msg),
    utxx::time_val  a_ts_recv,
    utxx::time_val  UNUSED_PARAM(a_ts_handl)
  )
  {
    //-----------------------------------------------------------------------//
    // Get the Msg:                                                          //
    //-----------------------------------------------------------------------//
    // NB: "a_msg_body" should be 0-terminated by the Caller:
    assert(a_h2conn != nullptr  && a_msg_body != nullptr && a_msg_len > 0 &&
           a_msg_body[a_msg_len] == '\0');

    char const* curr = a_msg_body;
    char*       end  = const_cast<char*>(a_msg_body + a_msg_len);

    OrderID reqID = a_h2conn->ReqIDofStreamID(a_stream_id);

    CHECK_ONLY
    (
      LOG_INFO(4,
        "EConnector_H2WS_BitMEX_OMC::ProcessH2Msg: ReqID={}, StreamID={}, "
        "Received msg={}", reqID, a_stream_id, a_msg_body)
    )

    // The messages could be either {...} or [{...}]
    if (utxx::unlikely(*curr != '{' && *curr != '['))
    {
      LOG_ERROR(2, "EConnector_H2WS_BitMEX_OMC::ProcessH2Msg: "
                   "Received UnKnown Msg={}", a_msg_body)
      return true;
    }

    // Check if the msg is complete (if not, it is a malformed msg or a proto-
    // col-level error):
    CHECK_ONLY
    (
      assert(a_msg_len > 0);
      char endSymb = (*curr == '{') ? '}' : ']';
      if (utxx::unlikely(a_msg_body[a_msg_len-1] != endSymb))
      {
        LOG_ERROR(2,
          "EConnector_H2WS_BitMEX_OMC::ProcessH2Msg: Truncated JSON Obj: {}, "
          "Len={}", a_msg_body, a_msg_len)
        // do NOT stop the Connector:
        return true;
      }
    )

    curr = *curr == '[' ? curr + 1 : curr;

    try
    {
      while (curr != nullptr)
      {
        bool isError = strncmp(curr, "{\"error\":", 9) == 0;
        if (utxx::unlikely(isError))
        {
          LOG_ERROR(1, "EConnector_H2WS_BitMEX_OMC::ProcessH2Msg: ReqID={},"
                       "Error msg={}", reqID, a_msg_body)
          return true;
        }
        else
        if (utxx::unlikely(a_h2conn->m_lastPositionsStmID == a_stream_id))
        {
          if (utxx::unlikely(*curr == ']'))
          {
            LOG_INFO(4, "EConnector_H2WS_BitMEX_OMC::ProcessH2Msg: "
                        "No Positions")
            m_posReceived = true;
            StartWSIfReady();
            return true;
          }

          SKP_STR("{\"account\":")
          curr = SkipNFlds<1>(curr);
          SKP_STR("\"symbol\":\"")
          SymKey symb;
          char const* afterSymb = strchr(curr, '\"');
          InitFixedStr(&symb, curr, size_t(afterSymb - curr));

          curr = strstr(curr, "currentQty");
          SKP_STR("currentQty\":")
          char* after = nullptr;
          double pos = strtod(curr, &after);
          assert(after > curr);

          char instrName[32];
          char* nm = instrName;
          nm = stpcpy(nm, symb.data());
          nm = stpcpy(nm, "-BitMEX--SPOT");
          SecDefD const* sd = this->m_secDefsMgr->FindSecDefOpt(instrName);

          if (sd != nullptr)
          {
            double lastPx = 1.0;
            if (pos != 0.0 && !sd->IsContractInA())
            {
              curr = strstr(curr, "avgEntryPrice");
              SKP_STR("avgEntryPrice\":")
              lastPx = strtod(curr, &after);
              assert(after > curr);
            }

            pos /= lastPx;
            this->m_positions[sd] = pos;
            LOG_INFO(4, "EConnector_H2WS_BitMEX_OMC::ProcessH2Msg: "
                        "{} position of {} units", symb.data(), pos)
          }
        }
        else
        {
          if (utxx::unlikely(*curr == ']'))
          {
            LOG_INFO(4, "EConnector_H2WS_BitMEX_OMC::ProcessH2Msg: "
                        "Empty Msg")
            return true;
          }

          SKP_STR("{\"orderID\":\"")
          ExchOrdID exchID(curr, int(strchr(curr, '\"') - curr));
          curr = SkipNFlds<1>(curr);
          SKP_STR("\"clOrdID\":\"")
          OrderID clOrdID = 0;
          curr = utxx::fast_atoi<OrderID, false>(curr, end, clOrdID);

          curr = SkipNFlds<19>(curr);
          SKP_STR("\"ordStatus\":\"")
          char* ordStatus    = const_cast<char*>(curr);
          long  ordStatusLen = strchr(ordStatus, '\"') - ordStatus;

          curr = SkipNFlds<10>(curr);
          bool isAmend = strncmp(curr, "\"text\":\"Amended", 14) == 0;

          curr = SkipNFlds<1>(curr);
          SKP_STR("\"transactTime\":\"")
          utxx::time_val exchTS = DateTimeToTimeValTZ(curr);

          if (strncmp(ordStatus, "New", size_t(ordStatusLen)) == 0)
          {
            if (!isAmend)
            {
              // Order Confirmed
              CHECK_ONLY(OMCWS::LogMsg<false>(a_msg_body, "H2 CONFIRMED");)
              (void) OrderConfirmedNew<QT, QR>
              (
                this,     this,   clOrdID, 0, exchID, ExchOrdID(0),
                PriceT(), QtyN::Invalid(),    exchTS, a_ts_recv
              );
            }
            else
            {
              // Order replaced
              CHECK_ONLY(OMCWS::LogMsg<false>(a_msg_body, "H2 REPLACED");)
              (void) OrderReplaced<QT, QR>
              (
                this,   this, clOrdID,  0,   0,  exchID, ExchOrdID(),
                ExchOrdID(0), PriceT(), QtyN::Invalid(), exchTS,  a_ts_recv
              );
            }
          }
          else if (clOrdID != 0 &&
                   strncmp(ordStatus, "Canceled", size_t(ordStatusLen)) == 0)
          {
            // Order replaced
            CHECK_ONLY(OMCWS::LogMsg<false>(a_msg_body, "H2 CANCELLED");)
            (void) EConnector_OrdMgmt::OrderCancelled
            (
              this,     this,     reqID, clOrdID,   0, exchID, ExchOrdID(0),
              PriceT(), QtyN::Invalid(), a_ts_recv, a_ts_recv
            );
          }
        }
        // Check end-message
        curr = strchr(curr, '{');
      }

      if (utxx::unlikely(a_h2conn->m_lastPositionsStmID == a_stream_id))
      {
        m_posReceived = true;
        a_h2conn->m_lastPositionsStmID = 0;
        StartWSIfReady();
      }
    }
    catch (EPollReactor::ExitRun const&)
    {
      // This exception is propagated: It is not an error:
      throw;
    }
    catch (exception const& exn)
    {
      // Log the exception and stop:
      LOG_ERROR(2, "EConnector_H2WS_BitMEX_OMC::ProcessH2Msg: {}", exn.what())
      return false;
    }
    return true;
  }

  //=========================================================================//
  // "ProcessWSMsg":                                                         //
  //=========================================================================//
  // Processing of  msgs received via HTTP/2 and WebSocket occurs here. The me-
  // thod returns "true" to indicate continuation, "false" to stop:
  //
  bool EConnector_H2WS_BitMEX_OMC::ProcessWSMsg
  (
    char const*     a_msg_body,  // Non-NULL
    int             a_msg_len,
    bool            UNUSED_PARAM(a_last_msg),
    utxx::time_val  a_ts_recv,
    utxx::time_val  UNUSED_PARAM(a_ts_handl)
  )
  {
    //-----------------------------------------------------------------------//
    // Get the Msg:                                                          //
    //-----------------------------------------------------------------------//
    // NB: "a_msg_body" should be 0-terminated by the Caller:
    assert(a_msg_body != nullptr &&
           a_msg_len > 0         &&
           a_msg_body[a_msg_len] == '\0');

    char const* curr = a_msg_body;
    char*       end  = const_cast<char*>(a_msg_body + a_msg_len);

    CHECK_ONLY
    (
      LOG_INFO(4,
        "EConnector_H2WS_BitMEX_OMC::ProcessWSMsg: Received msg={}", a_msg_body)
    )

    // Subscription failure
    if (utxx::unlikely
       (strncmp(a_msg_body, "{\"success\":false", 16) == 0))
    {
      LOG_ERROR(1,
        "EConnector_H2WS_BitMEX_OMC::ProcessWSMsg: Subscription Error: {}, "
        "EXITING...", a_msg_body)
      Stop();
      return false;
    }
    else
    // Welcome message means we can subscribe to execution feed:
    if (utxx::unlikely(strncmp(a_msg_body, "{\"info\":\"Welcome", 16) == 0))
    {
      char  tmpBuff[128];
      char* tmp = tmpBuff;
      tmp =
       stpcpy(tmp, "{\"op\":\"subscribe\",\"args\":[\"execution\"]}");
      ProWS::PutTxtData  (tmpBuff);
      ProWS::SendTxtFrame();
      return true;
    }
    else
    // Execution processing
    if (utxx::likely(strncmp(a_msg_body,
       "{\"table\":\"execution\",\"action\":\"insert\",\"data\":[", 47)) == 0)
    {
      SKP_STR("{\"table\":\"execution\",\"action\":\"insert\",\"data\":[")

      try
      {
        while (curr != nullptr)
        {
          SKP_STR("{\"execID\":")
          curr = SkipNFlds<1>(curr);
          SKP_STR("\"orderID\":\"")
          ExchOrdID exchID(curr, int(strchr(curr, '\"') - curr));
          curr = SkipNFlds<1>(curr);
          SKP_STR("\"clOrdID\":\"")
          OrderID clOrdID = 0;
          curr = utxx::fast_atoi<OrderID, false>(curr, end, clOrdID);

          curr = SkipNFlds<4>(curr);
          SKP_STR("\"side\":\"")
          FIX::SideT side = strncmp(curr, "Buy", 3) == 0 ? FIX::SideT::Buy
                                                         : FIX::SideT::Sell;
          curr = SkipNFlds<1>(curr);
          SKP_STR("\"lastQty\":")
          QR lastQty = NaN<QR>;
          curr = utxx::fast_atoi<QR, false>(curr, end, lastQty);
          curr = SkipNFlds<1>(curr);
          SKP_STR("\"lastPx\":")
          double lastPx = NaN<double>;
          curr = utxx::atof(curr, end, lastPx);

          curr = SkipNFlds<13>(curr);
          SKP_STR("\"execType\":\"")
          char const* execType = curr;

          curr = SkipNFlds<6>(curr);
          SKP_STR("\"ordStatus\":\"")
          char const* ordStatus = curr;

          curr = SkipNFlds<3>(curr);
          SKP_STR("\"ordRejReason\":")

          curr = SkipNFlds<2>(curr);
          SKP_STR("\"leavesQty\":")
          QR leavesQty = NaN<QR>;
          curr = utxx::fast_atoi<QR, false>(curr, end, leavesQty);

          curr = SkipNFlds<4>(curr);
          SKP_STR("\"commission\":")
          double fee = NaN<double>;
          curr = utxx::atof(curr, end, fee);
          fee *= double(lastQty);

          curr = SkipNFlds<10>(curr);
          SKP_STR("\"timestamp\":\"")
          utxx::time_val exchTS = DateTimeToTimeValTZ(curr);

          if (strncmp(execType,  "Trade", 5)     == 0 &&
              strncmp(ordStatus, "Partially", 9) == 0)
          {
            // Order Filled:
            CHECK_ONLY(OMCWS::LogMsg<false>(a_msg_body, "WS PART-FILLED");)
            (void) EConnector_OrdMgmt::OrderTraded
            (
              this,            this,
              nullptr,         clOrdID,        0,        nullptr,
              side,            FIX::SideT::UNDEFINED,    ExchOrdID(exchID),
              ExchOrdID(0),    ExchOrdID(0),   PriceT(),
              PriceT(lastPx),  QtyN(lastQty),  QtyN::Invalid(),
              QtyN::Invalid(), QtyF(fee),      0,        FuzzyBool::False,
              exchTS,          a_ts_recv
            );
          }
          else
          if (strncmp(execType,  "Trade", 5)  == 0 &&
              strncmp(ordStatus, "Filled", 6) == 0)
          {
            // Order Filled:
            CHECK_ONLY(OMCWS::LogMsg<false>(a_msg_body, "WS FILLED");)
            (void) EConnector_OrdMgmt::OrderTraded
            (
              this,            this,
              nullptr,         clOrdID,        0,        nullptr,
              side,            FIX::SideT::UNDEFINED,    ExchOrdID(exchID),
              ExchOrdID(0),    ExchOrdID(0),   PriceT(),
              PriceT(lastPx),  QtyN(lastQty),  QtyN::Invalid(),
              QtyN::Invalid(), QtyF(fee),      0,        FuzzyBool::True,
              exchTS,          a_ts_recv
            );
          }
          else
          if (strncmp(execType, "New", 3) == 0)
          {
            // Order Confirmed
            CHECK_ONLY(OMCWS::LogMsg<false>(a_msg_body, "WS CONFIRMED");)
            (void) OrderConfirmedNew<QT, QR>
            (
              this,     this,  clOrdID, 0, exchID, ExchOrdID(0),
              PriceT(), QtyN::Invalid(),   exchTS, a_ts_recv
            );
          }
          else
          if (strncmp(execType, "Replaced", 8) == 0)
          {
            // Order replaced
            CHECK_ONLY(OMCWS::LogMsg<false>(a_msg_body, "WS REPLACED");)
            (void) OrderReplaced<QT, QR>
            (
              this,   this, clOrdID,  0, 0,    exchID, ExchOrdID(),
              ExchOrdID(0), PriceT(), QtyN::Invalid(), exchTS,  a_ts_recv
            );
          }
          else
          if (clOrdID != 0 && (strncmp(execType, "Canceled", 8) == 0 ||
                               strncmp(ordStatus, "Canceled", 8) == 0))
          {
            // Order replaced
            CHECK_ONLY(OMCWS::LogMsg<false>(a_msg_body, "WS CANCELLED");)
            (void) EConnector_OrdMgmt::OrderCancelled
            (
              this,     this,         0, clOrdID,   0, exchID, ExchOrdID(0),
              PriceT(), QtyN::Invalid(), a_ts_recv, a_ts_recv
            );
          }

          // Check end-message
          curr = strchr(curr, '{');
        }
      }
      catch (EPollReactor::ExitRun const&)
      {
        // This exception is propagated: It is not an error:
        throw;
      }
      catch (exception const& exn)
      {
        // Log the exception and stop:
        LOG_ERROR(2, "EConnector_H2WS_BitMEX_OMC::ProcessWSMsg: {}", exn.what())
        return false;
      }
    }
    return true;
  }

  //=========================================================================//
  // PosTimer FD Helpers:                                                    //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "SetPosTimer":                                                          //
  //-------------------------------------------------------------------------//
  inline void EConnector_H2WS_BitMEX_OMC::SetPosTimer()
  {
    // Just in case, first remove the Timer if it exists:
    RemovePosTimer();
    assert(m_posTimerFD < 0);

    // TimerHandler:
    IO::FDInfo::TimerHandler timerH
    (
      [this](int DEBUG_ONLY(a_fd))->void
      {
        assert(a_fd == m_posTimerFD);
        this->RequestPositions();
      }
    );

    // ErrorHandler:
    IO::FDInfo::ErrHandler   errH
    (
      [this](int a_fd,   int   a_err_code, uint32_t  a_events,
             char const* a_msg) -> void
      { this->PosTimerErrHandler(a_fd, a_err_code, a_events, a_msg); }
    );

    // Period: 30 min = 1800 sec = 1800000 msec:
    constexpr uint32_t USTimerPeriodMSec = 1'800'000;

    // Create the TimerFD and add it to the Reactor:
    m_posTimerFD =
      EConnector::m_reactor->AddTimer
      (
        "InitialPosition",
        3000,               // Initial offset
        USTimerPeriodMSec,  // Period
        timerH,
        errH
      );
  }

  //-------------------------------------------------------------------------//
  // "RemovePosTimer":                                                       //
  //-------------------------------------------------------------------------//
  void EConnector_H2WS_BitMEX_OMC::RemovePosTimer()
  {
    if (m_posTimerFD >= 0)
    {
      if (utxx::likely(EConnector::m_reactor != nullptr))
        EConnector::m_reactor->Remove(m_posTimerFD);

      // Anyway, close the socket:
      (void) close(m_posTimerFD);
      m_posTimerFD = -1;
    }
  }

  //-------------------------------------------------------------------------//
  // "PosTimerErrHandler":                                                   //
  //-------------------------------------------------------------------------//
  inline void EConnector_H2WS_BitMEX_OMC::PosTimerErrHandler
    (int a_fd, int a_err_code, uint32_t a_events, char const* a_msg)
  {
    // There should be NO errors associated with this Timer at all. If one occ-
    // urs, we better shut down completely:
    assert(a_fd == m_posTimerFD);
    LOG_ERROR(1,
      "EConnector_H2WS_BitMEX_OMC::PosTimerErrHandler: TimerFD={}, ErrCode={}, "
      "Events={}, Msg={}: STOPPING",
      a_fd, a_err_code, EConnector::m_reactor->EPollEventsToStr(a_events),
      (a_msg != nullptr) ? a_msg : "")
    Stop();
  }
}
// End namespace MAQUETTE
