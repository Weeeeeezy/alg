// vim:ts=2:et
//===========================================================================//
//                    "Connectors/H2WS/BitMEX/Sender.cpp":                   //
//                  H2WS-Based OMC for BitMEX: Sending Orders                //
//===========================================================================//
#include "Connectors/H2WS/BitMEX/EConnector_H2WS_BitMEX_OMC.h"
#include "Connectors/EConnector_OrdMgmt.hpp"
#include "Connectors/H2WS/EConnector_H2WS_OMC.hpp"
#include "Basis/PxsQtys.h"
#include <utxx/convert.hpp>
#include <gnutls/crypto.h>
#include <nghttp2/nghttp2.h>
#include <ctime>

namespace MAQUETTE
{
  //=========================================================================//
  // "SendReq":                                                              //
  //=========================================================================//
  // Construct the Headers, sign the Req and and send out the Headers and Data
  // Frames:
  //
  inline std::tuple<utxx::time_val, SeqNum, uint32_t>
  EConnector_H2WS_BitMEX_OMC::SendReq
  (
    char const* a_method,   // "GET", "PUT" or "POST"
    char const* a_path,
    OrderID     a_req_id,
    char const* a_data      // If NULL, data are in "m_conn2.m_outDataFrame"
  )
  {
    if (utxx::unlikely(OMCH2WS::m_currH2Conn == nullptr))
      throw utxx::runtime_error
            ("EConnector_H2WS_BitMEX_OMC::SendReq: No Active H2Conns");

    //-----------------------------------------------------------------------//
    // If the Explicit Data are provided:                                    //
    //-----------------------------------------------------------------------//
    // Aassume "a_data" to be 0-terminated and put them into the Data Frame. In
    // that case, any data already there will be overwritten:
    //
    bool  explData  = (a_data != nullptr && *a_data != '\0');
    int   dataLen   = 0;
    auto& outDataFr = OMCH2WS::m_currH2Conn->m_outDataFrame; // Ref!
    auto& outHdrsFr = OMCH2WS::m_currH2Conn->m_outHdrsFrame; // ditto
    auto& outHdrs   = OMCH2WS::m_currH2Conn->m_outHdrs;      // ditto
    auto& tmpBuff   = OMCH2WS::m_currH2Conn->m_tmpBuff;      // ditto

    if (explData)
    {
      // NB: No Padding is used:
      char const* end =
        stpncpy(outDataFr.m_data, a_data, H2Conn::MaxOutDataDataLen);

      // Save the DataLength of the DataFrame:
      dataLen = int(end - outDataFr.m_data);
      outDataFr.SetDataLen(dataLen);
    }
    else
      // The data are already there (if at all):
      dataLen = outDataFr.GetDataLen();

    //-----------------------------------------------------------------------//
    // Header Values:                                                        //
    //-----------------------------------------------------------------------//
    // Generic HTTP/2 Part:
    assert(a_method != nullptr && a_path != nullptr);
    // [2]: Method:
    outHdrs[2].value    = const_cast<uint8_t*>
                          (reinterpret_cast<uint8_t const*>(a_method));
    outHdrs[2].valuelen = strlen(a_method);
    // [3]: Path:
    outHdrs[3].value    = const_cast<uint8_t*>
                          (reinterpret_cast<uint8_t const*>(a_path));
    outHdrs[3].valuelen = strlen(a_path);

    // BitMEX-Specific Part:
    // [5]: API-Expires: NB: This is NOT the Order expiration time, but rather
    //      the expiration time of the HTTP/2 Req before it reaches the Match-
    //      ing Engine, for security purposes. So allow only 5 secs.  The time
    //      is in UTC:
    time_t   expires    = time(nullptr) + 10;
    char     buff5[16];
    char*    after      = utxx::itoa_left<long>(buff5, long(expires));
    *after = '\0';
    outHdrs[5].value    = reinterpret_cast<uint8_t*>(buff5);
    outHdrs[5].valuelen = size_t(after - buff5);

    // [6]: API-Signature: Compute the HMAC signature of the Req and Data. XXX:
    // Unfortunately, this means that we need to copy all data  into  a single
    // buffer. However, the cost of copying is still small compared to that of
    // computing the crypto-signature:
    //
    // Method + Path + Expires + Data:
    //
    char* curr = stpcpy (tmpBuff, a_method);
    curr       = stpcpy (curr,    a_path);
    curr       = stpcpy (curr,    buff5);
    curr       = stpncpy(curr, outDataFr.m_data, H2Conn::MaxOutDataDataLen);
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

    // So the value of "API-Signature" header is:
    outHdrs[6].value    = reinterpret_cast<uint8_t*>(signSHA256Hex);
    outHdrs[6].valuelen = sizeof(signSHA256Hex) - 1;

    //-----------------------------------------------------------------------//
    // Deflate the Headers and put the resulting string into Hdrs Frame:     //
    //-----------------------------------------------------------------------//
    // NB: There are currently 6 Hdrs being sent in BitMEX. The compressed Hdrs
    // come at offset 0 in the Data(PayLoad) section of the HdrsFrame as we do
    // NOT use Paddings or Priorities:
    //
    constexpr size_t NHeaders = 8;
    long outLen =
      nghttp2_hd_deflate_hd
      (
        OMCH2WS::m_currH2Conn->m_deflater,
        reinterpret_cast<uint8_t*>(outHdrsFr.m_data),
        size_t(H2Conn::MaxOutHdrsDataLen),
        outHdrs,
        NHeaders
      );
    // Check for compression failure or overflow:
    CHECK_ONLY
    (
      if (utxx::unlikely(outLen <= 0 || outLen >= H2Conn::MaxOutHdrsDataLen))
        throw utxx::runtime_error
              ("EConnector_H2WS_OMC::SendReq: Invalid CompressedHdrsLen=",
               outLen, ": Valid Range=1..",  H2Conn::MaxOutHdrsDataLen-1);
    )
    //-----------------------------------------------------------------------//
    // Fill in all flds of the Headers Frame, and send it out:               //
    //-----------------------------------------------------------------------//
    outHdrsFr.SetDataLen(int(outLen));
    outHdrsFr.m_type = H2Conn::H2FrameTypeT::Headers;

    // END_HEADERS (0x04) flag is always set; END_STREAM (0x01) is also set if
    // there is no Data. No PADDED or PRIORITY flags are currently used:
    outHdrsFr.m_flags = (dataLen != 0) ? 0x04 : 0x05;

    // Set the StreamID (computed from ReqID):
    uint32_t  streamID =  OMCH2WS::m_currH2Conn->StreamIDofReqID(a_req_id);
    outHdrsFr.SetStreamID(streamID);

    // Send the Headers Frame out (IsData = false):
    utxx::time_val lastSendTS = OMCH2WS::m_currH2Conn->SendFrame<false>();
    //-----------------------------------------------------------------------//
    // If there are Data to send: Fill in and send out the Data Frame:       //
    //-----------------------------------------------------------------------//
    if (dataLen != 0)
    {
      //---------------------------------------------------------------------//
      // Log it:                                                             //
      //---------------------------------------------------------------------//
      CHECK_ONLY
      (
        OMCWS::template LogMsg<true>
        (
          outDataFr.m_data,
          a_method,
          streamID
        );
      )
      // NB: DataLen of the DataFrame is already set in any case...
      outDataFr.m_type  = H2Conn::H2FrameTypeT::Data;

      // We normally send only 1 Frame of Data, so set the END_STREAM Flag
      // (0x01):
      outDataFr.m_flags = 0x01;
      outDataFr.SetStreamID(OMCH2WS::m_currH2Conn->StreamIDofReqID(a_req_id));

      // Send the Data Frame out (IsData = true):
      lastSendTS = OMCH2WS::m_currH2Conn->SendFrame<true>();
    }
    // XXX: TxSN is not actually used, but we still increment it -- it is the
    // count of sent HTTP/2 Frames:
    SeqNum txSN = (*OMCWS::m_txSN)++;

    return std::make_tuple(lastSendTS, txSN, streamID);
  }

  //=========================================================================//
  // "NewOrder" (external, virtual overriding):                              //
  //=========================================================================//
  AOS const* EConnector_H2WS_BitMEX_OMC::NewOrder
  (
    Strategy*           a_strategy,
    SecDefD const&      a_instr,
    FIX::OrderTypeT     a_ord_type,
    bool                a_is_buy,
    PriceT              a_px,
    bool                a_is_aggr,
    QtyAny              a_qty,
    utxx::time_val      a_ts_md_exch,
    utxx::time_val      a_ts_md_conn,
    utxx::time_val      a_ts_md_strat,
    bool                a_batch_send,
    FIX::TimeInForceT   a_time_in_force,
    int                 a_expire_date,
    QtyAny              a_qty_show,
    QtyAny              a_qty_min,
    bool                a_peg_side,
    double              a_peg_offset
  )
  {
    if (utxx::unlikely(OMCH2WS::m_currH2Conn == nullptr))
    {
      LOG_WARN(2, "EConnector_H2WS_BitMEX_OMC::NewOrder: "
                  "No Active H2Conns")
      return nullptr;
    }

    return EConnector_OrdMgmt::NewOrderGen<QT,QR>
    ( // ProtoEng   SessMgr
      this,         this,            a_strategy,    a_instr,
      a_ord_type,   a_is_buy,        a_px,          a_is_aggr,
      a_qty,        a_ts_md_exch,    a_ts_md_conn,  a_ts_md_strat,
      a_batch_send, a_time_in_force, a_expire_date, a_qty_show,
      a_qty_min,    a_peg_side,      a_peg_offset
    );
  }

  //=========================================================================//
  // "NewOrderImpl" (Internal, "EConnector_OrdMgmt" Call-Back):              //
  //=========================================================================//
  void EConnector_H2WS_BitMEX_OMC::NewOrderImpl
  (
    EConnector_H2WS_BitMEX_OMC* DEBUG_ONLY(a_dummy),
    Req12*                      a_new_req,
    bool                        CHECK_ONLY(a_batch_send)
  )
  {
    if (utxx::unlikely(OMCH2WS::m_currH2Conn == nullptr))
      throw utxx::runtime_error
            ("EConnector_H2WS_BitMEX_OMC::NewOrderImpl: No Active H2Conns");

    //-----------------------------------------------------------------------//
    // "NewOrder" Params:                                                    //
    //-----------------------------------------------------------------------//
    assert(a_dummy == this && a_new_req != nullptr);

    OrderID           newReqID = a_new_req->m_id;
    assert(newReqID != 0);
    AOS const* aos = a_new_req->m_aos;
    assert(aos   != nullptr);
    SecDefD const*    instr    = aos->m_instr;
    assert(instr != nullptr);

    FIX::OrderTypeT   ordType     = aos->m_orderType;
    FIX::TimeInForceT timeInForce = aos->m_timeInForce;
    bool              isBuy       = aos->m_isBuy;
    PriceT            px          = a_new_req->m_px;
    QtyN              qty         = a_new_req->GetQty<QT,QR>();
    assert(IsPos(qty));

#   if !UNCHECKED_MODE
    // NB: BatchSend is NOT supported for BitMEX,   as the latter requires a
    // separate H2 Frame, and using IOVecs / WriteV is not possible over GNU-
    // TLS. The corresp flag is simply ignored:
    if (utxx::unlikely(a_batch_send))
    {
      LOG_WARN(1,
        "EConnector_H2WS_BitMEX_OMC::NewOrder: BatchSend is not supported"
        ": Flag Ignored")
      a_batch_send = false;
    }
    // Currently, only Limit and Market Orders are supported by this OMC, and
    // only GoodTillCancel and Day TiFs:
    if (utxx::unlikely
       (ordType != FIX::OrderTypeT::Limit &&
        ordType != FIX::OrderTypeT::Market))
      throw utxx::badarg_error
            ("EConnector_H2WS_BitMEX_OMC::NewOrder: UnSupported OrderType=",
             char(ordType));

    if (utxx::unlikely
       (ordType     == FIX::OrderTypeT::Limit            &&
        timeInForce != FIX::TimeInForceT::GoodTillCancel &&
        timeInForce != FIX::TimeInForceT::Day            &&
        timeInForce != FIX::TimeInForceT::GoodTillX))
    {
      LOG_WARN(4,
        "EConnector_H2WS_BitMEX_OMC::NewOrderImpl: "
        "UnSupported Limit TimeInForce: {}, changing to GTC", timeInForce)
      timeInForce = FIX::TimeInForceT::GoodTillCancel;
    }

    // Apparently, MinQty and Icebergs (ShowQty) are not supported in BitMEX.
    // Again. if they are specified, produce a warning but allow the order to
    // go:
    QtyN qtyShow = a_new_req->GetQtyShow<QT,QR>();
    QtyN qtyMin  = a_new_req->GetQtyMin <QT,QR>();

    if (utxx::unlikely(!IsSpecial0(qtyMin) || qtyShow != qty))
    {
      LOG_WARN(1,
        "EConnector_H2WS_BitMEX_OMC::NewOrder: Non-Trivial QtyMin={} and/or "
        "QtyShow={} not supported, ignored", QR(qtyMin), QR(qtyShow))
    }
    // TODO: Pegged Orders are supported by BitMEX but currently unimplemented:
    if (utxx::unlikely(IsFinite(a_new_req->m_pegOffset)))
      throw utxx::badarg_error
            ("EConnector_H2WS_BitMEX_OMC::NewOrder: Pegged Orders are not "
             "implemented yet");
#   endif  // !UNCHECKED_MODE

    //-----------------------------------------------------------------------//
    // Generate the Wire Msg:                                                //
    //-----------------------------------------------------------------------//
    char* curr = OMCH2WS::m_currH2Conn->m_outDataFrame.m_data;
    char* msg  = curr;

    // Symbol and ClOrdID:
    curr = stpcpy(curr, "{\"symbol\":\"");
    curr = stpcpy(curr, instr->m_Symbol.data());

    curr = stpcpy(curr, "\",\"clOrdID\":\"");
    curr = utxx::itoa_left<OrderID, 16>(curr, newReqID);

    // Side and Qty:
    curr = stpcpy(curr, "\",\"side\":\"");
    curr = (isBuy) ? stpcpy(curr, "Buy\"")
                   : stpcpy(curr, "Sell\"");

    if (utxx::unlikely(ordType == FIX::OrderTypeT::Market))
    {
      curr = stpcpy(curr, ",\"ordType\":\"Market\"}");
      // NB: TimeInForce is not specified for Market ords (it's actually IoC):
      assert(timeInForce == FIX::TimeInForceT::ImmedOrCancel);
      // Needless no say, Px is not for Market ords either!
    }
    else
    {
      assert(ordType == FIX::OrderTypeT::Limit);
      curr = stpcpy(curr, ",\"ordType\":\"Limit\",\"orderQty\":");
      curr = utxx::itoa_left<QR, 16>(curr, QR(Abs(qty)));

      // TimeInForce:
      curr = (timeInForce == FIX::TimeInForceT::GoodTillCancel)
           ? stpcpy(curr, ",\"timeInForce\":\"GoodTillCancel\",")
           : stpcpy(curr, ",\"timeInForce\":\"Day\",");
      // Px:
      curr = stpcpy(curr, "\"price\":");
      int n= utxx::ftoa_left<true>(double(px), curr, 16, 8);
      assert(n > 0);
      curr += n;
      // If the order is NOT intentionally-aggressive (although Limited),  we
      // set the flag which cancels it automatically if it becomes aggressive
      // due to a market move:
      if (!(a_new_req->m_isAggr))
        curr = stpcpy(curr, ",\"execInst\":\"ParticipateDoNotInitiate\"");
    }
    // Close the JSON obj:
    curr = stpcpy(curr, "}");

    //-----------------------------------------------------------------------//
    // Log it and Send it out:                                               //
    //-----------------------------------------------------------------------//
    CHECK_ONLY(OMCWS::LogMsg<true>(msg, "NEW ORDER");)

    OMCH2WS::m_currH2Conn->m_outDataFrame.SetDataLen(int(curr - msg));
    auto res = SendReq("POST", "/api/v1/order", newReqID, nullptr);

    //-----------------------------------------------------------------------//
    // Update the "NewReq":                                                  //
    //-----------------------------------------------------------------------//
    a_new_req->m_status  = Req12::StatusT::New;
    a_new_req->m_ts_sent = std::get<0>(res);
    a_new_req->m_seqNum  = std::get<1>(res);

    OMCH2WS::RotateH2Conns();
  }

  //=========================================================================//
  // "CancelOrder" (external, virtual overriding):                           //
  //=========================================================================//
  bool EConnector_H2WS_BitMEX_OMC::CancelOrder
  (
    AOS const*      a_aos,
    utxx::time_val  a_ts_md_exch,
    utxx::time_val  a_ts_md_conn,
    utxx::time_val  a_ts_md_strat,
    bool            a_batch_send
  )
  {
    if (utxx::unlikely(OMCH2WS::m_currH2Conn == nullptr))
    {
      LOG_WARN(2, "EConnector_H2WS_BitMEX_OMC::CancelOrder: No Active H2Conns")
      return false;
    }

    // Invoke                EConnector_OrdMgmt    ::CancelOrderGen (),
    // which in turn invokes EConnector_WS_BitFinex_OMC::CancelOrderImpl()
    //                                               (see below):
    return
      EConnector_OrdMgmt::CancelOrderGen<QT, QR>
      (
        this,   this, const_cast<AOS*>(a_aos),
        a_ts_md_exch, a_ts_md_conn, a_ts_md_strat, a_batch_send
      );
  }

  //=========================================================================//
  // "CancelOrderImpl" (Internal, "EConnector_OrdMgmt" Call-Back):           //
  //=========================================================================//
  void EConnector_H2WS_BitMEX_OMC::CancelOrderImpl
  (
    EConnector_H2WS_BitMEX_OMC* DEBUG_ONLY(a_dummy),
    Req12*                      a_clx_req,
    Req12 const*                a_orig_req,
    bool                        CHECK_ONLY(a_batch_send)
  )
  {
    if (utxx::unlikely(OMCH2WS::m_currH2Conn == nullptr))
      throw utxx::runtime_error
            ("EConnector_H2WS_BitMEX_OMC::CancelOrderImpl: No Active H2Conns");

    //-----------------------------------------------------------------------//
    // Cancel Params:                                                        //
    //-----------------------------------------------------------------------//
    assert(a_dummy == this                                    &&
           a_clx_req  != nullptr && a_orig_req != nullptr     &&
           a_clx_req ->m_kind    == Req12::KindT::Cancel      &&
           a_clx_req ->m_status  == Req12::StatusT::Indicated &&
           a_orig_req->m_status >=  Req12::StatusT::New       &&
           a_clx_req ->m_aos     == a_orig_req->m_aos);

    OrderID clReqID   = a_clx_req ->m_id;
    OrderID origReqID = a_orig_req->m_id;
    assert( clReqID != 0 && origReqID != 0 && clReqID > origReqID);

    CHECK_ONLY
    (
      // NB: BatchSend is NOT supported for BitMEX, as the latter requires a se-
      // quence of separate H2 Frames,  all of different size, and using IOVecs
      // or WriteV is not possible over GNUTLS. The corresp flag is simply igno-
      // red:
      if (utxx::unlikely(a_batch_send))
      {
        LOG_WARN(1,
          "EConnector_H2WS_BitMEX_OMC::CancelOrderImpl: BatchSend is not "
          "supported: Flag Ignored")
        a_batch_send = false;
      }
    )
    //-----------------------------------------------------------------------//
    // Generate the Wire Msg:                                                //
    //-----------------------------------------------------------------------//
    char* curr = OMCH2WS::m_currH2Conn->m_outDataFrame.m_data;
    char* msg  = curr;

    // We will "DELETE" the OrigID:
    curr = stpcpy(curr, "{\"clOrdID\":\"");
    curr = utxx::itoa_left<OrderID, 16>(curr, origReqID);
    curr = stpcpy(curr, "\"}");

    //-----------------------------------------------------------------------//
    // Log it and Send it out:                                               //
    //-----------------------------------------------------------------------//
    CHECK_ONLY(OMCWS::LogMsg<true>(msg, "CANCEL ORDER");)


    OMCH2WS::m_currH2Conn->m_outDataFrame.SetDataLen(int(curr - msg));
    auto res = SendReq("DELETE", "/api/v1/order", clReqID, nullptr);

    //-----------------------------------------------------------------------//
    // Update the "CxlReq":                                                  //
    //-----------------------------------------------------------------------//
    a_clx_req->m_status  = Req12::StatusT::New;
    a_clx_req->m_ts_sent = std::get<0>(res);
    a_clx_req->m_seqNum  = std::get<1>(res);

    OMCH2WS::RotateH2Conns();
  }

  //=========================================================================//
  // "ModifyOrder":                                                          //
  //=========================================================================//
  //=========================================================================//
  // External "ModifyOrder" (virtual overriding) method:                     //
  //=========================================================================//
  bool EConnector_H2WS_BitMEX_OMC::ModifyOrder
  (
    AOS const*     a_aos,       // Non-NULL
    PriceT         a_new_px,
    bool           a_is_aggr,
    QtyAny         a_new_qty,
    utxx::time_val a_ts_md_exch,
    utxx::time_val a_ts_md_conn,
    utxx::time_val a_ts_md_strat,
    bool           a_batch_send,
    QtyAny         a_new_qty_show,
    QtyAny         a_new_qty_min
  )
  {
    if (utxx::unlikely(OMCH2WS::m_currH2Conn == nullptr))
    {
      LOG_WARN(2, "EConnector_H2WS_BitMEX_OMC::ModifyOrder: No Active H2Conns")
      return false;
    }

    // Invoke                EConnector_OrdMgmt    ::ModifyOrderGen (),
    // which in turn invokes EConnector_WS_BitFinex_OMC::ModifyOrderImpl()
    //                                               (see below):
    return EConnector_OrdMgmt::ModifyOrderGen<QT,QR>
    ( // ProtoEngine SessMgr
      this,          this,           const_cast<AOS*>(a_aos),
      a_new_px,      a_is_aggr,      a_new_qty,       a_ts_md_exch,
      a_ts_md_conn,  a_ts_md_strat,  a_batch_send,    a_new_qty_show,
      a_new_qty_min
    );
  }

  //=========================================================================//
  // Internal "ModifyOrderImpl" ("EConnector_OrdMgmt" Call-Back):            //
  //=========================================================================//
  void EConnector_H2WS_BitMEX_OMC::ModifyOrderImpl
  (
    EConnector_H2WS_BitMEX_OMC* DEBUG_ONLY(a_dummy),
    Req12*                      DEBUG_ONLY(a_mod_req0),
    Req12*                      a_mod_req1,
    Req12 const*                a_orig_req,
    bool                        CHECK_ONLY(a_batch_send)
  )
  {
    if (utxx::unlikely(OMCH2WS::m_currH2Conn == nullptr))
      throw utxx::runtime_error
            ("EConnector_H2WS_BitMEX_OMC::ModifyOrderImpl: No Active H2Conns");

    //-----------------------------------------------------------------------//
    // Modify Params:                                                        //
    //-----------------------------------------------------------------------//
    assert(a_dummy    == this    && a_mod_req0 == nullptr     &&
           a_mod_req1 != nullptr && a_orig_req != nullptr     &&
           a_mod_req1->m_kind    == Req12::KindT::Modify      &&
           a_mod_req1->m_status  == Req12::StatusT::Indicated &&
           a_orig_req->m_status  >= Req12::StatusT::New       &&
           a_mod_req1->m_aos     == a_orig_req->m_aos);

    OrderID modReqID  = a_mod_req1->m_id;
    assert( modReqID != 0 && modReqID > a_orig_req->m_id      &&
            a_orig_req->m_id > 0);

    PriceT  newPx     = a_mod_req1->m_px;
    assert(IsFinite(newPx));
    QtyN     newQty   = a_mod_req1->GetQty<QT,QR>();
    assert(IsPos(newQty));
    CHECK_ONLY
    (
      // NB: BatchSend is NOT supported for BitMEX, as the latter requires a se-
      // quence of separate H2 Frames, all of different size,  and using IOVecs
      // or WriteV is not possible over GNUTLS. The corresp flag is simply igno-
      // red:
      if (utxx::unlikely(a_batch_send))
      {
        LOG_WARN(1,
          "EConnector_H2WS_BitMEX_OMC::ModifyOrderImpl: BatchSend is not "
          "supported: Flag Ignored")
        a_batch_send = false;
      }
    )
    //-----------------------------------------------------------------------//
    // Generate the Wire Msg:                                                //
    //-----------------------------------------------------------------------//
    char* curr = OMCH2WS::m_currH2Conn->m_outDataFrame.m_data;
    char* msg  = curr;

    curr = stpcpy(curr, "{\"origClOrdID\":\"");
    curr = utxx::itoa_left<OrderID, 16>(curr, a_orig_req->m_id);
    curr = stpcpy(curr, "\",\"clOrdID\":\"");
    curr = utxx::itoa_left<OrderID, 16>(curr, modReqID);
    curr = stpcpy(curr, "\",\"orderQty\":");
    curr = utxx::itoa_left<QR, 16>(curr, QR(newQty));
    curr = stpcpy(curr, ",\"price\":");
    int n = utxx::ftoa_left<false>(double(newPx), curr, 16, 8);
    assert(n > 0);
    curr += n;
    curr = stpcpy(curr, "}");

    //-----------------------------------------------------------------------//
    // Log it and Send it out:                                               //
    //-----------------------------------------------------------------------//
    CHECK_ONLY(OMCWS::LogMsg<true>(msg, "REPLACE ORDER");)

    OMCH2WS::m_currH2Conn->m_outDataFrame.SetDataLen(int(curr - msg));
    auto res = SendReq("PUT", "/api/v1/order", modReqID, nullptr);

    //-----------------------------------------------------------------------//
    // Update the "ModReq1":                                                 //
    //-----------------------------------------------------------------------//
    a_mod_req1->m_status  = Req12::StatusT::New;
    a_mod_req1->m_ts_sent = std::get<0>(res);
    a_mod_req1->m_seqNum  = std::get<1>(res);

    OMCH2WS::RotateH2Conns();
  }

  //=========================================================================//
  // "CancelAllOrders":                                                      //
  //=========================================================================//
  //=========================================================================//
  // "CancelAllOrders" (external, virtual overriding):                       //
  //=========================================================================//
  void EConnector_H2WS_BitMEX_OMC::CancelAllOrders
  (
    unsigned long  a_strat_hash48,
    SecDefD const* a_instr,
    FIX::SideT     a_side,
    char const*    a_segm_id
  )
  {
    if (utxx::unlikely(OMCH2WS::m_currH2Conn == nullptr))
      throw utxx::runtime_error
            ("EConnector_H2WS_BitMEX_OMC::CancelAllOrders: No Active H2Conns");

    EConnector_OrdMgmt::CancelAllOrdersGen<QT, QR>
    (
      this, this, a_strat_hash48, a_instr, a_side, a_segm_id
    );
  }

  //=========================================================================//
  // "CancelAllOrdersImpl":                                                  //
  //=========================================================================//
  void EConnector_H2WS_BitMEX_OMC::CancelAllOrdersImpl
  (
    EConnector_H2WS_BitMEX_OMC* UNUSED_PARAM(a_sess),
    unsigned long  UNUSED_PARAM(a_strat_hash48),
    SecDefD const* a_instr,
    FIX::SideT     UNUSED_PARAM(a_side),
    char const*    UNUSED_PARAM(a_segm_id)
  )
  {
    if (utxx::unlikely(OMCH2WS::m_currH2Conn == nullptr))
      throw utxx::runtime_error
            ("EConnector_H2WS_BitMEX_OMC::CancelAllOrdersImpl: "
             "No Active H2Conns");
    try
    {
      for (SecDefD const* sd: EConnector::m_usedSecDefs)
      {
        if (a_instr != nullptr && a_instr != sd)
          continue;

        char clxBuff[128];
        char* curr = clxBuff;

        curr = stpcpy(curr, "/api/v1/order/all?symbol=");
        curr = stpcpy(curr, sd->m_Symbol.data());

        auto res = SendReq("DELETE", clxBuff, 0, nullptr);
        uint32_t streamID = std::get<2>(res);

        LOG_INFO(2, "EConnector_H2WS_BitMEX_OMC::CancelAllOrders, Symbol={}, "
                    "StreamID={}, Msg={}",
                    sd->m_Symbol.data(), streamID, clxBuff)
        OMCH2WS::RotateH2Conns();
      }
    }
    catch (std::exception const& exn)
    {
      LOG_WARN(2, "EConnector_H2WS_Binance_OMC::CancelAllOrders: Exception={}",
                  exn.what())
      throw;
    }
  }

  //=========================================================================//
  // "FlushOrders":                                                          //
  //=========================================================================//
  // IMPORTANT: Because BatchSend is NOT supported for this OMC, the buffer
  // should be empty at this point, so there is nothing to do:
  //
  utxx::time_val EConnector_H2WS_BitMEX_OMC::FlushOrders()
    { return utxx::time_val(); }

  utxx::time_val EConnector_H2WS_BitMEX_OMC::FlushOrdersImpl
    (EConnector_H2WS_BitMEX_OMC* UNUSED_PARAM(a_dummy)) const
    { return utxx::time_val(); }

  //=========================================================================//
  // "RequestPositions":                                                     //
  //=========================================================================//
  void EConnector_H2WS_BitMEX_OMC::RequestPositions()
  {
    if (utxx::unlikely(OMCH2WS::m_currH2Conn == nullptr))
      throw utxx::runtime_error
            ("EConnector_H2WS_BitMEX_OMC::RequestPositions: No Active H2Conns");

    RemovePosTimer();
    try
    {
      auto res = SendReq("GET", "/api/v1/position", 0, nullptr);
      uint32_t streamID = std::get<2>(res);
      OMCH2WS::m_currH2Conn->m_lastPositionsStmID = streamID;

      LOG_INFO(2, "EConnector_H2WS_BitMEX_OMC::RequestPositions, StreamID={}",
                  streamID)

      OMCH2WS::RotateH2Conns();
    }
    catch (std::exception const& exn)
    {
      LOG_WARN(2, "EConnector_H2WS_Binance_OMC::CancelAllOrders: Exception={}",
                  exn.what())
      throw;
    }
  }
 }
// End namespace MAQUETTE
