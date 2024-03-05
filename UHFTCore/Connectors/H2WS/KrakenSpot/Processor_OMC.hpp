// vim:ts=2:et
//===========================================================================//
//        "Connectors/H2WS/Binance/EConnector_H2WS_Binance_OMC.cpp":         //
//                        H2WS-Based OMC for Binance                         //
//===========================================================================//
#pragma once

#include <utxx/compiler_hints.hpp>

#include "Connectors/H2WS/Binance/EConnector_H2WS_Binance_OMC.h"
#include "Protocols/JSONParseMacros.h"

namespace MAQUETTE
{
  //=========================================================================//
  // "StartWSIfReady":                                                       //
  //=========================================================================//
  template <Binance::AssetClassT::type AC>
  inline void EConnector_H2WS_Binance_OMC<AC>::StartWSIfReady()
  {
    bool ok = !OMCWS::IsActive() && m_listenKeyReceived   &&
              m_balOrPosReceived;
    if (ok)
    {
      OMCH2WS::ProWS::InitWSHandShake(m_userStreamStr);

      // We can now start the WS Leg:
      LOG_INFO(1,
        "EConnector_H2WS_Binance_OMC::StartWSIfReady: Started WS Leg.")
    }
  }

  //=========================================================================//
  // "OnHeader":                                                             //
  //=========================================================================//
  // Process specific HTTP codes, e.g. 500, unknown error:
  //
# if defined(__GNUC__) && !defined(__clang__)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wunused-but-set-parameter"
# endif
  template <Binance::AssetClassT::type AC>
  inline void EConnector_H2WS_Binance_OMC<AC>::OnHeaders
  (
                     H2Conn*        a_h2conn,
                     uint32_t       a_stream_id,
                     char const*    a_http_code,
                     char const*    UNUSED_PARAM(a_headers),
    [[maybe_unused]] utxx::time_val a_ts_recv
  )
  {
    assert(a_h2conn != nullptr);

    // Get the ReqID (it will be 0 if "a_stream_id" does not correspond to any
    // Req12, which might be OK):
    OrderID reqID = a_h2conn->ReqIDofStreamID(a_stream_id);

    if (utxx::unlikely(strncmp(a_http_code, "50", 2) == 0 ||
                       strncmp(a_http_code, "4", 1)  == 0))
    {
      // A listenKey req rejected, try again
      if (utxx::unlikely(a_stream_id == a_h2conn->m_lastListenKeyStmID))
      {
        LOG_WARN(1, "EConnector_H2WS_Binance_OMC::OnHeaders: StreamID={}, "
                    "ReqID={}: ListenKey Rejected, retrying now...",
                    a_stream_id, reqID)
        RequestListenKey();
        return;
      }

      //---------------------------------------------------------------------//
      // Only interpret headers as rejections on Futures                     //
      //---------------------------------------------------------------------//
      if constexpr (IsFut)
      {
        // Error 503: heavy load on BitMEX, our req has not been processed
        // Error 400: some other reject
        (void) this->template OrderRejected<QT, QR>
        (
          this,                   // ProtoEng
          this,                   // SessMgr
          0,                      // No SeqNums here
          reqID,
          0,                      // AOS ID is not known from the Protocol
          FuzzyBool::True,
          0,
          "",
          a_ts_recv,              // ExchTS is not known
          a_ts_recv
        );
        LOG_INFO(2, "EConnector_H2WS_Binance_OMC::OnHeaders: ConnH2={}, "
                    "StreamID={}, ReqID={}",
                    a_h2conn->m_id, a_stream_id, reqID)
      }
    }
  }
# if defined(__GNUC__) && !defined(__clang__)
# pragma GCC diagnostic pop
# endif

  //=========================================================================//
  // "OnRstStream":                                                          //
  //=========================================================================//
  template <Binance::AssetClassT::type AC>
  void EConnector_H2WS_Binance_OMC<AC>::OnRstStream
  (
    H2Conn*        a_h2conn,
    uint32_t       a_stream_id,
    uint32_t       a_err_code,
    utxx::time_val a_ts_recv
  )
  {
    assert(a_h2conn != nullptr);

    // In Binance, this is an undesirable condition which warrants a full OMC
    // restart
    //
    // XXX: As only one Stream is reset, not the whole connection,  we do
    // NOT stop the whole Connector, nor there is a need to restart it at
    // the TCP level. However, the Order corresponding to this Stream  is
    // considered to have Failed:
    // (*) For OMCs, the corresp Req has Failed, although not necessarily
    //     the whole AOS is Inactive;
    // (*) MDCs need to implement a similar CallBack in case of H2, which
    //     typically means that the corresp Subscr Req has Failed:
    //
    if (utxx::likely(a_err_code == 7))
    {
      OrderID reqID = a_h2conn->ReqIDofStreamID(a_stream_id);

      // A listenKey req rejected, try again
      if (utxx::unlikely(a_stream_id == a_h2conn->m_lastListenKeyStmID))
      {
        LOG_WARN(1, "EConnector_H2WS_Binance_OMC::OnRstStream: StreamID={}, "
                    "ReqID={}: ListenKey Rejected, retrying now...",
                    a_stream_id, reqID)
        OMCH2WS::RotateH2Conns();
        RequestListenKey();
      }

      if (utxx::likely(reqID != 0))
      {
        // NB: The following call will  do nothing is the corresp order is
        // already Inactive (eg the Server has closed an old unused Stream):
        (void) this->template OrderRejected<QT, QR>
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
        LOG_WARN(2, "EConnector_H2WS_Binance_OMC::OnRstSream: ConnH2={}, "
                    "StreamID={}, ReqID={}",
                    a_h2conn->m_id, a_stream_id, reqID)
      }

      // Yes, initiate a graceful Stop with a subsequent ReStart:
      // FullStop=false:
      (void) a_h2conn->template Stop<false>();

      // Move this H2Conn to NotActive list, but do NOT rotate it yet:
      OMCH2WS::MoveH2ConnToNotActive(a_h2conn);
    }
  }

  //=========================================================================//
  // "ProcessH2Msg":                                                         //
  //=========================================================================//
  // For HTTP/2-delivered Msgs. It is expected that exactly 1 msg (as a JSON
  // obj) is to be parsed:
  template <Binance::AssetClassT::type AC>
  bool EConnector_H2WS_Binance_OMC<AC>::ProcessH2Msg
  (
    H2Conn*         a_h2conn,
    uint32_t        a_stream_id,
    char const*     a_msg_body,
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
    assert(a_h2conn  != nullptr  && a_msg_body != nullptr && a_msg_len > 0 &&
           a_msg_body[a_msg_len] == '\0');

    char const* curr = a_msg_body;
    char*       end  = const_cast<char*>(a_msg_body + a_msg_len);

    OrderID   reqID  = a_h2conn->ReqIDofStreamID(a_stream_id);
    // NB: reqID==0 might be perfectly OK, and in any case, the warning was al-
    // ready logged in "H2Connector", so don't do it once again...

    OrderID clOrdID     = 0;
    OrderID origClOrdID = 0;
    OrderID orderID     = 0;
    SymKey  symbol;

    LOG_INFO(4,
      "EConnector_H2WS_Binance_OMC::ProcessH2Msg: ConnID={}, StreamID={}, "
      "ReqID={}, Received Msg=[{}]", a_h2conn->m_id, a_stream_id,
      reqID, a_msg_body)
    assert(curr < end);

    // Check if the msg is complete (if not, it is a malformed msg or a proto-
    // col-level error):
    CHECK_ONLY
    (
      assert(a_msg_len > 0);
      if (utxx::unlikely(a_msg_body[a_msg_len-1] != '}' &&
                         a_msg_body[a_msg_len-1] != ']'))
      {
        LOG_ERROR(2,
          "EConnector_H2WS_Binance_OMC::ProcessH2Msg: Truncated JSON Obj: {}, "
          "Len={}", a_msg_body, a_msg_len)
        // XXX: In does happen with Binance from time to time, so just ignore
        // this msg -- do NOT stop the Connector:
        return true;
      }
    )

    //-----------------------------------------------------------------------//
    // Catch and process errors:                                             //
    //-----------------------------------------------------------------------//
    try
    {
      if (*curr == '[')
        ++curr;

      //---------------------------------------------------------------------//
      // Balances (SPOT) or Positions (FUT)                                  //
      //---------------------------------------------------------------------//
      if (utxx::unlikely(a_stream_id == a_h2conn->m_lastBalanceStmID  ||
                         a_stream_id == a_h2conn->m_lastPositionsStmID))
      {
        a_h2conn->m_lastPositionsStmID = 0;
        a_h2conn->m_lastBalanceStmID = 0;
        m_balOrPosReceived = true;

          // Empty list
        if (*curr == ']')
        {
          StartWSIfReady();
          return true;
        }
        else
        if constexpr (IsFut)
        {
          //-----------------------------------------------------------------//
          // Positions (FUT):                                                //
          //-----------------------------------------------------------------//
          while (curr != nullptr)
          {
            curr = strchr(curr, '{');
            SKP_STR("{\"symbol\":\"")
            SymKey ccy;
            char const* afterSymb = strchr(curr, '\"');
            InitFixedStr(&ccy, curr, size_t(afterSymb - curr));

            SKP_FLD(1)
            SKP_STR("\"positionAmt\":\"")
            char* after = nullptr;
            double pos = strtod(curr, &after);
            assert(after > curr);

            char instrName[32];
            char* nm = instrName;
            nm = stpcpy(nm, ccy.data());
            nm = stpcpy(nm, "-Binance-FUT");
            SecDefD const* sd = this->m_secDefsMgr->FindSecDefOpt(instrName);

            if (sd != nullptr)
            {
              this->m_positions[sd] = pos;
              LOG_INFO(4, "EConnector_H2WS_Binance_OMC::ProcessH2Msg: "
                          "{} position of {} units", ccy.data(), pos)
            }
            curr = strstr(curr, "},{");
          }
        }
        else
        {
          //-----------------------------------------------------------------//
          // Balances (SPOT):                                                //
          //-----------------------------------------------------------------//
          SKP_STR("{\"makerCommission\":")
          int makerFee = INT_MIN;
          curr = utxx::fast_atoi<int, false>(curr, end, makerFee);
          SKP_FLD(1)
          SKP_STR("\"takerCommission\":")
          int takerFee = INT_MIN;
          curr = utxx::fast_atoi<int, false>(curr, end, takerFee);

          LOG_INFO(2, "EConnector_H2WS_Binance_OMC::ProcessH2Msg: "
                      "Maker Fee={}bps, Taker Fee={}bps", makerFee, takerFee)

          SymKey ccy;
          double freeAmt = NaN<double>;
          double lockAmt = NaN<double>;

          curr = strstr(curr, "\"balances");
          SKP_STR("\"balances\":")
          while (*curr != ']')
          {
            ++curr;

            SKP_STR("{\"asset\":\"")
            char const* after = strchr(curr, '\"');
            InitFixedStr(&ccy, curr, size_t(after - curr));
            curr = after;

            char* after1 = nullptr;
            SKP_STR("\",\"free\":\"")
            freeAmt = strtod(curr, &after1);
            assert(after1 > curr);
            curr = after1;

            SKP_STR("\",\"locked\":\"")
            lockAmt = strtod(curr, &after1);
            assert(after1 > curr);
            curr = after1;

            SKP_STR("\"}")

            this->m_balances[ccy] = freeAmt + lockAmt;
            LOG_INFO(4, "EConnector_H2WS_Binance_OMC::ProcessH2Msg: "
                        "{} balance of {} units", ccy.data(),
                        freeAmt + lockAmt)
          }
        }
        StartWSIfReady();
      }
      else
      //---------------------------------------------------------------------//
      // ListenKey:                                                          //
      //---------------------------------------------------------------------//
      if (utxx::unlikely(a_stream_id == a_h2conn->m_lastListenKeyStmID))
      {
        a_h2conn->m_lastListenKeyStmID = 0;
        m_listenKeyReceived = true;

        SKP_STR("{\"listenKey\":\"")
        char const* after = const_cast<char*>(strstr(curr, "\"}"));

        // After ListenKey is received,
        // we can do the WS handshake for UserFeed
        // So save the corresp req in "m_userStreamStr":
        char*   tmp = stpcpy(m_userStreamStr, "/stream?streams=");
        strncpy(tmp,  curr, size_t(after - curr));

        // We can now start the WS Leg:
        LOG_INFO(1,
          "EConnector_H2WS_Binance_OMC::ProcessH2Msg: Got ListenKey={}",
          m_userStreamStr + 16)

        StartWSIfReady();
      }
      else
      //---------------------------------------------------------------------//
      // An Error Msg?                                                       //
      //---------------------------------------------------------------------//
      if (utxx::likely(strncmp(curr, "{\"code\":", 8) == 0))
      {
        GET_VAL()
        int errCode = 0;
        curr = utxx::fast_atoi<int, false>(curr, end, errCode);
        if (errCode == 200)
        {
          GET_VAL()
          if (strncmp(curr, "The operation", 13) == 0)
            LOG_INFO(2, "EConnector_H2WS_Binance_OMC::ProcessH2: "
                        "Cancelled orders, StreamID={}", a_stream_id)
          // E.g. CancelAll succeeded
          return true;
        }
        else
        if (errCode == -2011)
        {
          if (a_stream_id == a_h2conn->m_lastCancelAllStmID)
          {
            LOG_INFO(2, "EConnector_H2WS_Binance_OMC::ProcessH2: "
                        "Nothing to cancel, StreamID={}", a_stream_id)
            a_h2conn->m_lastCancelAllStmID = 0;
            return true;
          }

          CHECK_ONLY
          (
            OMCWS::template LogMsg<false>
              (a_msg_body, "H2 CAN/MOD REJECTED: ReqID=", long(reqID));
          )
          // "Cancel" (or "ModLegC") failed( most likely, the order has already
          // been Filled):
          (void) EConnector_OrdMgmt::OrderCancelOrReplaceRejected<QT, QR>
             (
               this, this,    reqID, 0, 0, nullptr, FuzzyBool::UNDEFINED,
               IsFut ? FuzzyBool::True : FuzzyBool::UNDEFINED,
               0, a_msg_body, a_ts_recv, a_ts_recv
             );
        }
        else if (errCode == -2010 || errCode == -1013)
        {
          CHECK_ONLY
          (
            OMCWS::template LogMsg<false>
              (a_msg_body, "H2 REJECTED: ReqID=", long(reqID));
          )
          // Unsuccessful order placement (eg broken margin requirements?):
          (void) EConnector_OrdMgmt::OrderRejected<QT, QR>
                 (
                   this, this, 0,  reqID, 0, FuzzyBool::True, 0,
                   a_msg_body, a_ts_recv, a_ts_recv
                 );
        }
        else if (errCode == -1003)
        {
          LOG_ERROR(1,
              "EConnector_H2WS_Binance_OMC::ProcessH2Msg: RateLimits error: "
              "{}", a_msg_body)
          // Unsuccessful order placement (eg broken margin requirements?):
          (void) EConnector_OrdMgmt::OrderRejected<QT, QR>
                 (
                   this, this, 0,  reqID, 0, FuzzyBool::UNDEFINED, 0,
                   a_msg_body, a_ts_recv, a_ts_recv
                 );
        }
        else
        {
          LOG_ERROR(1,
              "EConnector_H2WS_Binance_OMC::ProcessH2Msg: Unprocessed error: "
              "{}", a_msg_body)

          CHECK_ONLY
          (
            OMCWS::template LogMsg<false>
              (a_msg_body, "H2 REJECTED: ReqID=", long(reqID));
          )
          // Unsuccessful order placement (eg broken margin requirements?):
          (void) EConnector_OrdMgmt::OrderRejected<QT, QR>
                 (
                   this, this, 0,  reqID, 0, FuzzyBool::True, 0,
                   a_msg_body, a_ts_recv, a_ts_recv
                 );
        }
      }
      else
      //---------------------------------------------------------------------//
      // FUTURES:                                                            //
      //---------------------------------------------------------------------//
      if constexpr (IsFut)
      {
        //-------------------------------------------------------------------//
        // Orders:                                                           //
        //-------------------------------------------------------------------//
        if (utxx::likely(strncmp(curr, "{\"orderId", 9) == 0))
        {
          GET_VAL()
          curr = utxx::fast_atoi<OrderID, false>(curr, end, orderID);

          SKP_FLD(1)
          SKP_STR("\"symbol\":\"")
          char const* afterSymb = strchr(curr, '\"');
          InitFixedStr(&symbol, curr, size_t(afterSymb - curr));

          SKP_FLD(1)
          SKP_STR("\"status\":\"")
          char const* status = curr;

          SKP_FLD(1)
          SKP_STR("\"clientOrderId\":\"")
          curr = utxx::fast_atoi<OrderID, false>(curr, end, origClOrdID);

          SKP_STR("\",\"price\":\"")
          char* after = nullptr;
          double px = strtod(curr, &after);
          assert(after > curr);

          SKP_FLD(2)
          SKP_STR("\"origQty\":\"")
          double qty = strtod(curr, &after);
          assert(after > curr);

          SKP_FLD(14)
          SKP_STR("\"updateTime\":")
          uint64_t msTs;
          curr = utxx::fast_atoi<uint64_t, false>(curr, end, msTs);

          // Cancellation branch
          if (strncmp(status, "CANCELED", 8) == 0 && reqID > 0)
          {
            CHECK_ONLY
            (
              OMCWS::template LogMsg<false>(a_msg_body, "H2 CANCELED");
            )

            (void) EConnector_OrdMgmt::OrderCancelled
            ( this,         this,
              reqID,        origClOrdID, 0,         ExchOrdID(orderID),
              ExchOrdID(0), PriceT(px),  QtyN(qty),
              utxx::time_val(utxx::msecs(msTs)),    a_ts_recv
            );
          }
          else
          // Confirmation branch
          if (strncmp(status, "NEW", 3) == 0)
          {
            CHECK_ONLY
            (
              OMCWS::template LogMsg<false>(a_msg_body, "H2 CONFIRMED");
            )

            (void) this->template OrderConfirmedNew<QT, QR>
            (
              this,     this,     origClOrdID,       0,
              ExchOrdID(orderID), ExchOrdID(0),
              PriceT(px), QtyN(qty),
              utxx::time_val(utxx::msecs(msTs)), a_ts_recv
            );
          }
        }
        else
        //-------------------------------------------------------------------//
        // Unknown msg:                                                      //
        //-------------------------------------------------------------------//
        {
          LOG_ERROR(1,
            "EConnector_H2WS_Binance_OMC::ProcessH2Msg: Got UnKnown Msg={}, "
            "Check fields order.", a_msg_body)
          return false;
        }
      }
      else
      //---------------------------------------------------------------------//
      // SPOT:                                                               //
      //---------------------------------------------------------------------//
      if constexpr (!IsFut)
      {
        //--------------------------------------------------------------------//
        // Orders:                                                            //
        //--------------------------------------------------------------------//
        if (utxx::likely(strncmp(curr, "{\"symbol", 8) == 0))
        {
          SKP_STR("{\"symbol\":\"")
          char const* afterSymb = strchr(curr, '\"');
          InitFixedStr(&symbol, curr, size_t(afterSymb - curr));
          SKP_FLD(1)

          // Here we have different branches
          bool isCancel  = strncmp(curr, "\"orig", 5) == 0;
          bool isConfirm = strncmp(curr, "\"orderId", 8) == 0;

          if (utxx::unlikely(!(isCancel || isConfirm)))
            UTXX_THROW_RUNTIME_ERROR("EConnector_H2WS_Binance_OMC:: "
              "ProcessH2Msg: Unknown Msg={}", a_msg_body);

          if (isCancel)
          {
            // Cancellation branch
            GET_VAL()
            curr = utxx::fast_atoi<OrderID, false>(curr, end, origClOrdID);
            // This parse goes wrong if clOrdID is a string with
            // a digit in 1st position
            if (utxx::unlikely(strchr(curr, '\"') - curr > 1))
              origClOrdID = 0;
            SKP_FLD(1)

            SKP_STR("\"orderId\":")
            curr = utxx::fast_atoi<OrderID, false>(curr, end, orderID);
            SKP_FLD(2)

            SKP_STR("\"clientOrderId\":\"")
            curr = utxx::fast_atoi<OrderID, false>(curr, end, clOrdID);
            // This parse goes wrong if clOrdID is a string with
            // a digit in 1st position
            if (utxx::unlikely(strchr(curr, '\"') - curr > 1))
              clOrdID = 0;

            CHECK_ONLY
            (
              OMCWS::template LogMsg<false>(a_msg_body, "H2 CANCELED");
            )

            if (utxx::likely(clOrdID != 0 && origClOrdID != 0))
              (void) EConnector_OrdMgmt::OrderCancelled
              (
                this,      this,    clOrdID,      origClOrdID,            0,
                ExchOrdID(orderID), ExchOrdID(0), PriceT(), QtyN::Invalid(),
                a_ts_recv, a_ts_recv
              );
          }
          else
          {
            // Confirmation branch
            GET_VAL()
            curr = utxx::fast_atoi<OrderID, false>(curr, end, orderID);
            SKP_FLD(2)

            SKP_STR("\"clientOrderId\":\"")
            curr = utxx::fast_atoi<OrderID, false>(curr, end, clOrdID);
            SKP_FLD(1)

            SKP_STR("\"transactTime\":")
            uint64_t msTs;
            curr = utxx::fast_atoi<uint64_t, false>(curr, end, msTs);

            CHECK_ONLY
            (
              OMCWS::template LogMsg<false>(a_msg_body, "H2 CONFIRMED");
            )

            (void) this->template OrderConfirmedNew<QT, QR>
            (
              this,   this, clOrdID,  0, ExchOrdID(orderID),
              ExchOrdID(0), PriceT(), QtyN::Invalid(),
              utxx::time_val(utxx::msecs(msTs)),  a_ts_recv
            );
          }
        }
        else
        //-----------------------------------------------------------------//
        // Unknown msg:                                                    //
        //-----------------------------------------------------------------//
        {
          LOG_ERROR(1,
            "EConnector_H2WS_Binance_OMC::ProcessH2Msg: Got UnKnown Msg={}, "
            "Check fields order.", a_msg_body)
          return false;
        }
      }
    }
    catch (EPollReactor::ExitRun const&)
    {
      // This exception is propagated: It is not an error:
      throw;
    }
    catch (std::exception const& exn)
    {
      // Log the exception and stop:
      LOG_ERROR(2,
        "EConnector_H2WS_Binance_OMC::ProcessH2Msg: {} in: Msg=[{}], "
        "ConnID={}, StreamID={}, ReqID={}, ClOrdID={}, OrigClOrdID={}, "
        "OrderID={}",
        exn.what(), a_msg_body,  a_h2conn->m_id, a_stream_id, reqID,
        clOrdID,    origClOrdID, orderID)
      return false;
    }
    // All Done:
    return true;
  }

  //=========================================================================//
  // "ProcessWSMsg":                                                         //
  //=========================================================================//
  // For WebSocket-delivered Msgs:
  template <Binance::AssetClassT::type AC>
  bool EConnector_H2WS_Binance_OMC<AC>::ProcessWSMsg
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
    // NB: "a_msg_body" should be non-empty, and 0-terminated by the Caller:
    assert(a_msg_body != nullptr && a_msg_len > 0 &&
           a_msg_body[a_msg_len] == '\0');

    char const* curr = a_msg_body;
    char*       end  = const_cast<char*>(a_msg_body + a_msg_len);
    assert(curr < end);

    LOG_INFO(4,
      "EConnector_H2WS_Binance_OMC::ProcessWSMsg: Received msg={}", a_msg_body)

    OrderID clOrdID     = 0;
    OrderID origClOrdID = 0;
    OrderID orderID     = 0;   // Exch-assigned

    uint64_t    msTs      = 0;
    FIX::SideT  side      = FIX::SideT::UNDEFINED;
    char const* execType  = nullptr;
    char const* ordStatus = nullptr;
    QR          lastQty   = NaN<QR>;
    double      lastPx    = NaN<double>;
    double      fee       = NaN<double>;
    int         errCode   = 0;
    // XXX: GCC may generate false warnings here!
#   if defined(__GNUC__) && !defined(__clang__)
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#   endif
    QR          origQty   = NaN<QR>;
    double      orderPx   = NaN<double>;
#   if defined(__GNUC__) && !defined(__clang__)
#   pragma GCC diagnostic pop
#   endif
    try
    {
      while (curr != nullptr)
      {
        curr = strstr(curr, "\"data\":");
        if (utxx::unlikely(curr == nullptr))
          throw utxx::runtime_error("EConnector_H2WS_Binance_OMC::"
                "ProcessWSMsg: No data in Msg=", a_msg_body);
        GET_VAL()

        //-------------------------------------------------------------------//
        // SPOT:                                                             //
        //-------------------------------------------------------------------//
        if constexpr (!IsFut)
        {
          bool isExecReport = strncmp(curr,"{\"e\":\"executionReport", 21) == 0;
          bool isAccountInf = strncmp(curr,"{\"e\":\"outboundAccount", 21) == 0;
          // WS reply:
          // "e": Event type
          if (utxx::unlikely(!isExecReport && !isAccountInf))
          {
            // We might have multiple msgs
            curr = strstr(curr, "\"stream\"");
            continue;
          }

          if (utxx::likely(isExecReport))
          {
            SKP_FLD(1)

            // "E": Event time
            SKP_STR("\"E\":")
            curr = utxx::fast_atoi<uint64_t, false>(curr, end, msTs);
            SKP_FLD(2)

            // "c": ClOrdID
            SKP_STR("\"c\":\"")
            curr = utxx::fast_atoi<OrderID, false>(curr, end, clOrdID);
            // This parse goes wrong if clOrdID is a string with
            // a digit in 1st position
            if (utxx::unlikely(strchr(curr, '\"') - curr > 1))
              clOrdID = 0;
            SKP_FLD(1)
            SKP_STR("\"S\":\"")

            side = strncmp(curr, "BUY", 3) == 0 ? FIX::SideT::Buy
                                                : FIX::SideT::Sell;
            SKP_FLD(8)

            // "C": origClOrdID
            SKP_STR("\"C\":\"")
            curr = utxx::fast_atoi<OrderID, false>(curr, end, origClOrdID);
            // This parse goes wrong if clOrdID is a string with
            // a digit in 1st position
            if (utxx::unlikely(strchr(curr, '\"') - curr > 1))
              origClOrdID = 0;
            SKP_FLD(1)
            SKP_STR("\"x\":\"")

            // "x": execution type, save pointer
            // NEW, CANCELED, REJECTED, TRADE, EXPIRED
            execType = curr;

            // "X": order status, save pointer
            // NEW, PARTIALLY_FILLED, FILLED, CANCELED, REJECTED, EXPIRED
            GET_VAL()
            ordStatus = curr;

            // "r": order rejection reason code; however, it can be "NONE", in
            // which case errCode remains 0:
            GET_VAL()
            errCode = 0;
            curr = utxx::fast_atoi<int, false>(curr, end, errCode);

            // "i": orderID
            GET_VAL()
            curr = utxx::fast_atoi<OrderID, false>(curr, end, orderID);
            SKP_FLD(1)

            char* after = nullptr;
            // "l": last executed qty
            SKP_STR("\"l\":\"")
            lastQty = strtod(curr, &after);
            assert(after > curr);
            SKP_FLD(2)

            // "L": last exec px
            SKP_STR("\"L\":\"")
            lastPx = strtod(curr, &after);
            assert(after > curr);
            SKP_FLD(1)

            // "n": commission amnt
            SKP_STR("\"n\":\"")
            fee = strtod(curr, &after);
            assert(after > curr);
            fee = side == FIX::SideT::Buy ? fee * lastPx : fee;
          }
          else
          // Account info with balances
          if (isAccountInf)
          {
            // We might have multiple msgs
            curr = strstr(curr, "\"stream\"");
            continue;
/*            curr = strstr(curr, "\"B\"");

            // SymKey ccy;
            double freeAmt = NaN<double>;
            double lockAmt = NaN<double>;

            SKP_STR("\"B\":")
            while (*curr != ']')
            {
              ++curr;

              SKP_STR("{\"a\":\"")
              char* after = strchr(curr, '\"');
              InitFixedStr(&ccy, curr, size_t(after - curr));
              curr = after;

              SKP_STR("\",\"f\":\"")
              freeAmt = strtod(curr, &after);
              assert(after > curr);
              curr = after;

              SKP_STR("\",\"l\":\"")
              lockAmt = strtod(curr, &after);
              assert(after > curr);
              curr = after;

              SKP_STR("\"}")

              if (freeAmt + lockAmt > 1e-7)
              {
                this->m_Balance[ccy] = freeAmt + lockAmt;
                LOG_INFO(4, "EConnector_H2WS_Binance_OMC::ProcessWSMsg: "
                            "{} balance of {} units", ccy.data(),
                            freeAmt + lockAmt)
              }
            } */
          }
        }
        else
        //-------------------------------------------------------------------//
        // FUTURES:                                                          //
        //-------------------------------------------------------------------//
        {
          if (utxx::unlikely(strncmp(curr, "{\"e\":\"ORDER", 11) != 0))
          {
            // We might have multiple msgs
            curr = strstr(curr, "\"stream\"");
            continue;
          }

          SKP_FLD(2)

          // "E": event time
          SKP_STR("\"E\":")
          curr = utxx::fast_atoi<uint64_t, false>(curr, end, msTs);

          SKP_STR(",\"o\":{")
          SKP_FLD(1)

          // "c": ClOrdID
          SKP_STR("\"c\":\"")
          curr = utxx::fast_atoi<OrderID, false>(curr, end, clOrdID);
          SKP_FLD(1)

          // "S": side
          SKP_STR("\"S\":\"")
          side = strncmp(curr, "BUY", 3) == 0 ? FIX::SideT::Buy
                                              : FIX::SideT::Sell;
          SKP_FLD(3)

          // "q": origQty
          char* after = nullptr;
          SKP_STR("\"q\":\"")
          origQty = strtod(curr, &after);
          assert(curr < after);
          SKP_FLD(1)

          // 'p': origPx
          SKP_STR("\"p\":\"")
          orderPx = strtod(curr, &after);
          assert(curr < after);
          SKP_FLD(3)

          // "x": execution type, save pointer
          // NEW, CANCELED, REJECTED, TRADE, EXPIRED
          SKP_STR("\"x\":\"")
          execType = curr;
          SKP_FLD(1)

          // "X": order status, save pointer
          // NEW, PARTIALLY_FILLED, FILLED, CANCELED, REJECTED, EXPIRED
          SKP_STR("\"X\":\"")
          ordStatus = curr;
          SKP_FLD(1)

          // "i": orderID
          SKP_STR("\"i\":")
          curr = utxx::fast_atoi<OrderID, false>(curr, end, orderID);
          SKP_FLD(1)

          // "l": last executed qty
          SKP_STR("\"l\":\"")
          lastQty = strtod(curr, &after);
          assert(after > curr);
          SKP_FLD(2)

          // "L": last exec px
          SKP_STR("\"L\":\"")
          lastPx = strtod(curr, &after);
          assert(after > curr);

          if (strncmp(execType, "TRADE", 5) == 0)
          {
            SKP_FLD(1)
            // "n": commission amt. For Futures -- in QuoteCcy
            SKP_STR("\"n\":\"")
            fee = strtod(curr, &after);
            assert(after > curr);
          }

          // Process Cancel separately since there is no clOrdID here
          // clOrdID == 0 iff Cancelled from UI
          if (clOrdID != 0 && strncmp(ordStatus, "CANCELED", 8) == 0)
          {
            CHECK_ONLY
            (
              OMCWS::template LogMsg<false>(a_msg_body, "WS CANCELLED");
            )

            constexpr unsigned OrigMask = unsigned(Req12::KindT::New)    |
                                          unsigned(Req12::KindT::Modify) |
                                          unsigned(Req12::KindT::ModLegN);

            Req12* origReq = this->template GetReq12<OrigMask, false, QT, QR>
                   (clOrdID, 0, PriceT(orderPx), QtyN(origQty),
                    "EConnector_H2WS_Binance_OMC::ProcessWSMsg");

            Req12* clxReq = origReq;
            bool found = false;
            while (clxReq->m_next != nullptr)
            {
              clxReq = clxReq->m_next;
              if ((clxReq->m_kind == Req12::KindT::Cancel   ||
                   clxReq->m_kind == Req12::KindT::ModLegC) &&
                  clxReq->m_status != Req12::StatusT::Failed &&
                  clxReq->m_status != Req12::StatusT::Indicated)
              {
                found = true;
                break;
              }
            }
            OrderID clxID = found ? clxReq->m_id : 0;

            (void) EConnector_OrdMgmt::OrderCancelled<QT, QR>
            (
              this,   this, clxID,  clOrdID,  0, ExchOrdID(orderID),
              ExchOrdID(0), PriceT(orderPx),     QtyN(origQty),
              utxx::time_val(utxx::msecs(msTs)), a_ts_recv
            );

            // We might have multiple msgs
            curr = strstr(curr, "\"stream\"");
            continue;
          }
        }

        // Now determine what has happened:
        //
        if (strncmp(execType, "TRADE", 5) == 0)
        {
          if (strncmp(ordStatus, "PARTIALLY_FILLED", 16) == 0)
          {
            CHECK_ONLY
            (
              OMCWS::template LogMsg<false>(a_msg_body, "WS PART-FILLED");
            )

            (void) EConnector_OrdMgmt::OrderTraded
            (
              this,             this,
              nullptr,          clOrdID,        0,    nullptr,  side,
              FIX::SideT::UNDEFINED,            ExchOrdID(orderID),
              ExchOrdID(0),     ExchOrdID(0),   PriceT(),
              PriceT(lastPx),   QtyN(lastQty),  QtyN::Invalid(),
              QtyN::Invalid(),  QtyF(fee),      0,
              FuzzyBool::False, utxx::time_val(utxx::msecs(msTs)), a_ts_recv
            );
          }
          else
          if (strncmp(ordStatus, "FILLED", 6) == 0)
          {
            CHECK_ONLY
            (
              OMCWS::template LogMsg<false>(a_msg_body, "WS FILLED");
            )

            (void) EConnector_OrdMgmt::OrderTraded
            (
              this,            this,
              nullptr,         clOrdID,        0,    nullptr,  side,
              FIX::SideT::UNDEFINED,           ExchOrdID(orderID),
              ExchOrdID(0),    ExchOrdID(0),   PriceT(),
              PriceT(lastPx),  QtyN(lastQty),  QtyN::Invalid(),
              QtyN::Invalid(), QtyF(fee),      0,
              FuzzyBool::True, utxx::time_val(utxx::msecs(msTs)), a_ts_recv
            );
          }
          else
            LOG_ERROR(1, "EConnector_H2WS_Binance_OMC::ProcessWSMsg: "
                         "Unknown \"x\":\"X\" combo in {}", a_msg_body)
        }
        else
        {
          if (strncmp(ordStatus, "NEW", 3) == 0)
          {
            // New order confirmed; LeavesQty is not directly available from the
            // Protocol, hence Invalid:
            CHECK_ONLY
            (
              OMCWS::template LogMsg<false>(a_msg_body, "WS CONFIRMED");
            )

            (void) EConnector_OrdMgmt::OrderConfirmedNew<QT, QR>
            (
               this,   this, clOrdID,    0,    ExchOrdID(orderID),
               ExchOrdID(0), PriceT(),   QtyN::Invalid(),
               utxx::time_val(utxx::msecs(msTs)), a_ts_recv
            );
          }
          else
          if (strncmp(ordStatus, "CANCELED", 8) == 0)
          {
            CHECK_ONLY
            (
              OMCWS::template LogMsg<false>(a_msg_body, "WS CANCELED");
            )

            if (utxx::likely(clOrdID != 0 && origClOrdID != 0))
              (void) EConnector_OrdMgmt::OrderCancelled<QT, QR>
              (
                this,         this,
                clOrdID,      origClOrdID, 0, ExchOrdID(orderID),
                ExchOrdID(0), PriceT(),    QtyN::Invalid(),
                utxx::time_val(utxx::msecs(msTs)), a_ts_recv
              );
          }
          else
          if (strncmp(ordStatus, "REJECTED", 8) == 0)
          {
            CHECK_ONLY
            (
              OMCWS::template LogMsg<false>(a_msg_body,  "WS REJECTED");
            )

            (void) EConnector_OrdMgmt::OrderRejected<QT, QR>
            (
              this,        this,
              0, clOrdID,  0, FuzzyBool::True,   errCode, nullptr,
              utxx::time_val(utxx::msecs(msTs)), a_ts_recv
            );
          }
        }

        // We might have multiple msgs:
        curr = strstr(curr, "\"stream\"");
      }
    }
    catch (EPollReactor::ExitRun const&)
    {
      // This exception is propagated: It is not an error:
      throw;
    }
    catch (std::exception const& exn)
    {
      // Log the exception and stop:
      LOG_ERROR(2,
        "EConnector_H2WS_Binance_OMC::ProcessWSMsg: Exception: {} in "
        "Msg=[{}], ClOrdID={}, OrigClOrdID={}, OrderID={}",
        exn.what(), a_msg_body, clOrdID, origClOrdID, orderID)
      return false;
    }

    //-----------------------------------------------------------------------//
    // All Done:                                                             //
    //-----------------------------------------------------------------------//
    return true;
  }
}
// End namespace MAQUETTE
