//===========================================================================//
//                  "Connectors/H2WS/Binance/Sender_OMC.hpp":                //
//                 H2WS-Based OMC for Binance: Sending Orders                //
//===========================================================================//
#pragma  once
#include "Connectors/H2WS/Binance/EConnector_H2WS_Binance_OMC.h"
#include "Connectors/EConnector_OrdMgmt.hpp"
#include <gnutls/crypto.h>

namespace MAQUETTE
{
  //=========================================================================//
  // "SendReq":                                                              //
  //=========================================================================//
  // Construct the Headers, sign the Req and and send out the  Headers Frame.
  // NB: Binance OMC currently does not use the  Data Frame -- all Req Params
  // are encoded via the Path in the Headers Frame:
  //
  template <Binance::AssetClassT::type AC>
  template<size_t QueryOff>
  inline std::tuple<utxx::time_val, SeqNum, uint32_t>
  EConnector_H2WS_Binance_OMC<AC>::SendReq
  (
    char const* a_method,  // "GET", "PUT", "POST" or "DELETE"
    OrderID     a_req_id,  // If 0,  we use the next available StreamID
    char const* a_path     // If NULL, m_path is used (via m_outHdrs[3])
  )
  {
    assert(a_method != nullptr);

    //-----------------------------------------------------------------------//
    // Checks:                                                               //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely(OMCH2WS::m_currH2Conn == nullptr ||
                      !OMCH2WS::m_currH2Conn->IsActive()))
      throw utxx::runtime_error
            ("EConnector_H2WS_Binance_OMC::SendReq: No Active H2Conns");

    //-----------------------------------------------------------------------//
    // Header Values:                                                        //
    //-----------------------------------------------------------------------//
    auto& outHdrsFr = OMCH2WS::m_currH2Conn->m_outHdrsFrame; // Ref!
    auto& outHdrs   = OMCH2WS::m_currH2Conn->m_outHdrs;      // ditto

    // Generic HTTP/2 Part:
    // [2]: Method:
    outHdrs[2].value     = const_cast<uint8_t*>
                           (reinterpret_cast<uint8_t const*>(a_method));
    outHdrs[2].valuelen  = strlen(a_method);

    // [3]: Path: Must already be in, as outHdrs[3], unless a_path is given:
    uint8_t* &pathRef    = outHdrs[3].value;
    size_t   &pathLenRef = outHdrs[3].valuelen;

    if (utxx::unlikely(a_path != nullptr && *a_path != '\0'))
    {
      // "a_path" should be used. We can use it as-is only if QueryOff==0, ie
      // if we do not need to sign the Query:
      if constexpr (QueryOff == 0)
      {
        // Point "pathRef" to "a_path":
        pathRef    = const_cast<uint8_t*>
                     (reinterpret_cast<uint8_t const*>(a_path));
        pathLenRef = strlen(a_path);
      }
      else
      {
        // Copy "a_path" to "pathRef":
        char*   to = reinterpret_cast<char*>(pathRef);
        char*  end = stpcpy(to, a_path);
        pathLenRef = size_t(end - to);
      }
    }
    else
      assert(pathRef != nullptr && pathLenRef > 0);

    //-----------------------------------------------------------------------//
    // Sign the Req:                                                         //
    //-----------------------------------------------------------------------//
    // Compute the signature of QueryString and put it into "m_signSHA256":
    if (QueryOff > 0)
    {
      assert(QueryOff < pathLenRef && pathRef[QueryOff-1] == '?');
      uint8_t const* query    = pathRef     + QueryOff;
      size_t         queryLen = pathLenRef  - QueryOff;

      DEBUG_ONLY(int ok =)
        gnutls_hmac_fast
          (GNUTLS_MAC_SHA256,  OMCH2WS::m_account.m_APISecret.data(),
           m_APISecret.size(), query,   queryLen, m_signSHA256);
      assert(ok == 0);

      // Append the Signature fld at the end of the curr Path+Query:
      char* end =  reinterpret_cast<char*>(pathRef) + pathLenRef;
      end       =  stpcpy (end, "&signature=");

      // Convert "m_signSHA256" into a HEX string and output it directly at the
      // end of the Path; "m_signHMAC" contains a ptr to "m_signSHA256" and the
      // input length:
      size_t signSHA256HexLen = 65;    // Incl 0-terminator
      DEBUG_ONLY(ok =)  gnutls_hex_encode(&m_signHMAC, end, &signSHA256HexLen);
      assert(ok == 0 && 0 < signSHA256HexLen && signSHA256HexLen <= 65);

      // The len increment w/o the 0-terminator:
      --signSHA256HexLen;
      end += signSHA256HexLen;
      assert(*end == '\0');
      // We have appended (11 + signSHA256HexLen) bytes (w/o the 0-terminator):
      pathLenRef  += (11 + signSHA256HexLen);
    }
    //-----------------------------------------------------------------------//
    // Deflate the Headers and put the resulting string into Hdrs Frame:     //
    //-----------------------------------------------------------------------//
    // NB: There are currently 5 Hdrs being sent in Binance. The compressed Hdrs
    // come at offset 0 in the Data(PayLoad) section of the HdrsFrame as we do
    // NOT use Paddings or Priorities:
    //
    constexpr size_t NHeaders = 5;
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
    outHdrsFr.m_type  = H2Conn::H2FrameTypeT::Headers;

    // END_HEADERS (0x04) and END_STREAM (0x01) are always set, the latter be-
    // cause there is no Data Frame. No PADDED or PRIORITY flags are currently
    // used:
    outHdrsFr.m_flags = 0x05;

    // Set the StreamID (computed from ReqID, or set automatically if ReqID=0):
    uint32_t   streamID = OMCH2WS::m_currH2Conn->StreamIDofReqID(a_req_id);
    outHdrsFr.SetStreamID(streamID);

    // XXX: TxSN is not actually used, but we still increment it for formal
    // consistency:
    SeqNum txSN = (*OMCWS::m_txSN)++;

    //-----------------------------------------------------------------------//
    // Log it:                                                               //
    //-----------------------------------------------------------------------//
    CHECK_ONLY
    (
      OMCWS::template LogMsg<true>
      (
        const_cast<const char*>(reinterpret_cast<char*>(outHdrs[3].value)),
        a_method,
        streamID
      );
    )
    //-----------------------------------------------------------------------//
    // Actually send it out, and increment the counter:                      //
    //-----------------------------------------------------------------------//
    utxx::time_val sendTS = OMCH2WS::m_currH2Conn->template SendFrame<false>();
    ++(OMCH2WS::m_currH2Conn->m_currReqs);

    // Is it time to ReStart the curr H2Conn?
    if (OMCH2WS::m_currH2Conn->m_currReqs >
        OMCH2WS::m_currH2Conn->MaxReqsPerH2Session)
    {
      // Yes, initiate a graceful Stop with a subsequent ReStart:
      // FullStop=false:
      (void) OMCH2WS::m_currH2Conn->template Stop<false>();
      OMCH2WS::m_currH2Conn->m_currReqs = 0;

      // Move this H2Conn to NotActive list, but do NOT rotate it yet:
      OMCH2WS::MoveH2ConnToNotActive(OMCH2WS::m_currH2Conn);
    }
    return std::make_tuple(sendTS, txSN, streamID);
  }

  //=========================================================================//
  // "NewOrder":                                                             //
  //=========================================================================//
  //=========================================================================//
  // "NewOrder" (external, virtual overriding):                              //
  //=========================================================================//
  template <Binance::AssetClassT::type AC>
  AOS const* EConnector_H2WS_Binance_OMC<AC>::NewOrder
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
      LOG_WARN(2, "EConnector_H2WS_Binance_OMC::NewOrder: No Active H2Conns")
      return nullptr;
    }

    // // For Futures use GoodTillX == Post-Only, iff not Aggr
    // a_time_in_force =
    //   IsFut
    //   ? (a_is_aggr ? FIX::TimeInForceT::GoodTillCancel
    //                : FIX::TimeInForceT::GoodTillX)
    //   :  FIX::TimeInForceT::GoodTillCancel;

    return
      EConnector_OrdMgmt::NewOrderGen<QT,QR>
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
  template <Binance::AssetClassT::type AC>
  void EConnector_H2WS_Binance_OMC<AC>::NewOrderImpl
  (
    EConnector_H2WS_Binance_OMC*  DEBUG_ONLY(a_dummy),
    Req12*                        a_new_req,
    bool                          CHECK_ONLY(a_batch_send)
  )
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

    //-----------------------------------------------------------------------//
    // NewOrder Params:                                                      //
    //-----------------------------------------------------------------------//
    OrderID           newReqID    = a_new_req->m_id;
    assert(newReqID != 0);

    utxx::time_val    createTS    = a_new_req->m_ts_created;
    assert(!createTS.empty());
    AOS const*        aos         = a_new_req->m_aos;
    assert(aos != nullptr);
    SecDefD const*    instr       = aos->m_instr;
    assert(instr != nullptr);

    FIX::OrderTypeT   ordType     = aos->m_orderType;
    FIX::TimeInForceT timeInForce = aos->m_timeInForce;
    bool              isBuy       = aos->m_isBuy;
    CHECK_ONLY(int    expireDate  = aos->m_expireDate;  )

    PriceT            px          = a_new_req->m_px;
    assert(IsFinite(px));
    QtyN              qty          (a_new_req->m_qty);
    assert(IsPos(qty));
    QtyN              qtyShow      (a_new_req->m_qtyShow);
    assert(IsSpecial0(qtyShow)  || (!IsNeg(qtyShow) && qtyShow <= qty));
    CHECK_ONLY(QtyN   qtyMin       (a_new_req->m_qtyMin);   )
    CHECK_ONLY(double pegOffset   = a_new_req->m_pegOffset; )

    CHECK_ONLY
    (
      // NB: BatchSend is NOT supported for Binance,    as the latter requires a
      // sequence of separate H2 Frames, all of different size, and using IOVecs
      // or WriteV is not possible over GNUTLS. The corresp flag is simply igno-
      // red:
      if (utxx::unlikely(a_batch_send))
      {
        LOG_WARN(4,
          "EConnector_H2WS_Binance_OMC::NewOrderImpl: BatchSend is not "
          "supported: Flag Ignored")
        a_batch_send = false;
      }
      // OrderType: Currently, only Limit and Market orders are supported:
      if (utxx::unlikely
         (ordType != FIX::OrderTypeT::Limit &&
          ordType != FIX::OrderTypeT::Market))
          throw utxx::badarg_error
                ("EConnector_H2WS_Binance_OMC::NewOrderImpl: "
                 "UnSupported OrderType=",   char(ordType));

      // Only the following TiFs are supported for Limit Orders; XXX: Market
      // orders are always IoCs:
      if (utxx::unlikely
         (ordType     == FIX::OrderTypeT::Limit            &&
          timeInForce != FIX::TimeInForceT::GoodTillCancel &&
          timeInForce != FIX::TimeInForceT::FillOrKill     &&
          timeInForce != FIX::TimeInForceT::ImmedOrCancel  &&
          timeInForce != FIX::TimeInForceT::GoodTillX))
      {
        LOG_WARN(4,
          "EConnector_H2WS_Binance_OMC::NewOrderImpl: "
          "UnSupported Limit TimeInForce: {}, changing to GTC", timeInForce)
        timeInForce = FIX::TimeInForceT::GoodTillCancel;
        expireDate  = 0;
      }
      // The following params are unsupported, and should not be set:
      // QtyShow, QtyMin, ExpireDate, PegOffset:
      //
      if (utxx::unlikely
         (!IsZero(qtyMin) || expireDate != 0 || IsFinite(pegOffset)))
        throw utxx::badarg_error
              ("EConnector_H2WS_Binance_OMC::NewOrderImpl: UnSupported "
               "Param(s): QtyMin|ExpireDate|PegOffset");
    )
    //-----------------------------------------------------------------------//
    // Fill in the Path Hdr:                                                 //
    //-----------------------------------------------------------------------//
    char* msg = m_path;

    // NB: QueryStrOff is 14 in this case (PathLen+'?'):
    msg  = stpcpy(msg, QueryPfx);
    msg  = stpcpy(msg, "order?symbol=");
    msg  = stpcpy(msg, instr->m_Symbol.data());

    char const* side = isBuy ? "BUY" : "SELL";
    msg  = stpcpy(msg, "&side=");
    msg  = stpcpy(msg, side);

    // Depending on OrderType, TIF might be forbidden by Binance
    bool withTiF = true;

    msg  = stpcpy(msg, "&type=");
    if constexpr (IsFut)
      // Post-Only for FUT is defined by TIF
      msg  = stpcpy(msg, (ordType == FIX::OrderTypeT::Limit)
                         ? "LIMIT" : "MARKET");
    else
      // Post-Only for SPOT is defined by order type
      if (a_new_req->m_isAggr)
        // This can hit the market
        msg  = stpcpy(msg, "LIMIT");
      else
      {
        // This is Post-Only for SPOT
        msg  = stpcpy(msg, "LIMIT_MAKER");
        withTiF = false;
      }

    msg  = stpcpy(msg, "&quantity=");
    msg += utxx::ftoa_left(double(qty), msg, 16, 8);

    // Iceberg orders are actually supported in Binance:
    if (utxx::unlikely(qtyShow < qty))
    {
      msg  = stpcpy(msg, "&icebergQty=");
      msg += utxx::ftoa_left(double(qty), msg, 16, 8);
    }
    // Px and TimeInForce are specified unless it is a Market Order:
    if (utxx::likely(ordType != FIX::OrderTypeT::Market))
    {
      assert(IsFinite(px));
      msg  = stpcpy(msg, "&price=");
      msg += utxx::ftoa_left(double(px),  msg, 16, 8);

      if (utxx::likely(withTiF))
      {
        char const* tif  =
          (timeInForce  == FIX::TimeInForceT::GoodTillX)
          ? "GTX" :
          (timeInForce  == FIX::TimeInForceT::FillOrKill)
          ? "FIK" :
          (timeInForce  == FIX::TimeInForceT::ImmedOrCancel)
          ? "IOC"
          : "GTC";      // Default in all other cases, GTX == Post-Only

        msg  = stpcpy(msg, "&timeInForce=");
        msg  = stpcpy(msg, tif);
      }
    }
    msg  = stpcpy(msg, "&newClientOrderId=");
    msg  = utxx::itoa_left<OrderID, 16>(msg, newReqID);

    // if order expires immediately (post only and price moved), Binance
    // wouldn't tell us unless we specify this
    msg  = stpcpy(msg, "&newOrderRespType=RESULT");

    // Technical:
    msg  = stpcpy(msg, "&recvWindow=5000&timestamp=");
    msg  = utxx::itoa_left<long,    13>(msg, createTS.milliseconds());
    // NB: msec timestamp contains  13 digits

    //-----------------------------------------------------------------------//
    // Point the Path Hdr Val to "m_path":                                   //
    //-----------------------------------------------------------------------//
    *msg = '\0';     // 0-terminate it for logging
    size_t pathLen = size_t(msg - m_path);
    assert(pathLen < sizeof(m_path));

    auto& outHdrs       = OMCH2WS::m_currH2Conn->m_outHdrs;    // Ref!
    outHdrs[3].value    = reinterpret_cast<uint8_t*>(m_path);
    outHdrs[3].valuelen = pathLen;

    // QueryPfx + "order?":
    auto res = SendReq<QueryPfxLen + 6>("POST", newReqID);

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
  template <Binance::AssetClassT::type AC>
  bool EConnector_H2WS_Binance_OMC<AC>::CancelOrder
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
      LOG_WARN(2, "EConnector_H2WS_Binance_OMC::CancelOrder: "
                  "No Active H2Conns")
      return false;
    }

    // Invoke                EConnector_OrdMgmt    ::CancelOrderGen (),
    // which in turn invokes EConnector_WS_BitFinex_OMC::CancelOrderImpl()
    //                                               (see below):
    return
      EConnector_OrdMgmt::CancelOrderGen<QT, QR>
      (
        this,   this, const_cast<AOS*>(a_aos),
        a_ts_md_exch, a_ts_md_conn,    a_ts_md_strat, a_batch_send
      );
  }

  //=========================================================================//
  // "CancelOrderImpl" (Internal, "EConnector_OrdMgmt" Call-Back):           //
  //=========================================================================//
  template <Binance::AssetClassT::type AC>
  void EConnector_H2WS_Binance_OMC<AC>::CancelOrderImpl
  (
    EConnector_H2WS_Binance_OMC* DEBUG_ONLY(a_dummy),
    Req12*                       a_clx_req,
    Req12 const*                 a_orig_req,
    bool                         CHECK_ONLY(a_batch_send)
  )
  {
    //-----------------------------------------------------------------------//
    // Checks:                                                               //
    //-----------------------------------------------------------------------//
    assert(a_dummy == this       && a_clx_req != nullptr      &&
           a_orig_req != nullptr                              &&
          (a_clx_req ->m_kind    == Req12::KindT::Cancel      ||
           a_clx_req ->m_kind    == Req12::KindT::ModLegC)    &&
           a_clx_req ->m_status  == Req12::StatusT::Indicated &&
           a_orig_req->m_status  >= Req12::StatusT::New);

    // Check once again for unavailable "m_currH2Conn" (as this method may be
    // invoked by-passing "CancelOrder" above).  Throw an exception if cannot
    // send, as there is no other way of reporting a failure at this point:
    //
    if (utxx::unlikely(OMCH2WS::m_currH2Conn == nullptr))
      throw utxx::runtime_error
            ("EConnector_H2WS_Binance_OMC::CancelOrderImpl: No Active H2Conns");

    //-----------------------------------------------------------------------//
    // Cancel Params:                                                        //
    //-----------------------------------------------------------------------//
    OrderID    clReqID   = a_clx_req ->m_id;
    OrderID    origReqID = a_orig_req->m_id;
    assert(clReqID != 0 && origReqID != 0 && clReqID > origReqID);

    utxx::time_val createTS = a_clx_req->m_ts_created;
    assert(!createTS.empty());

    AOS const* aos      = a_clx_req->m_aos;
    assert(aos != nullptr);

    CHECK_ONLY
    (
      // NB: BatchSend is NOT supported for Binance,    as the latter requires a
      // sequence of separate H2 Frames, all of different size, and using IOVecs
      // or WriteV is not possible over GNUTLS. The corresp flag is simply igno-
      // red:
      if (utxx::unlikely(a_batch_send))
      {
        LOG_WARN(1,
          "EConnector_H2WS_Binance_OMC::CancelOrderImpl: BatchSend is not "
          "supported: Flag Ignored")
        a_batch_send = false;
      }
    )
    //-----------------------------------------------------------------------//
    // Fill in the Path Hdr:                                                 //
    //-----------------------------------------------------------------------//
    char* msg = m_path;

    msg = stpcpy(msg, QueryPfx);
    msg = stpcpy(msg, "order?symbol=");
    msg = stpcpy(msg, aos->m_instr->m_Symbol.data());

    msg = stpcpy(msg, "&origClientOrderId=");
    msg = utxx::itoa_left<OrderID, 16>(msg, origReqID);

    if constexpr (!IsFut)
    {
      msg = stpcpy(msg, "&newClientOrderId=");
      msg = utxx::itoa_left<OrderID, 16>(msg, clReqID);
    }

    msg = stpcpy(msg, "&recvWindow=5000&timestamp=");
    msg = utxx::itoa_left<long,    13>(msg, createTS.milliseconds());

    //-----------------------------------------------------------------------//
    // Point the Path Hdr Val to "m_path":                                   //
    //-----------------------------------------------------------------------//
    *msg = '\0';     // 0-terminate it for logging
    size_t pathLen = size_t(msg - m_path);
    assert(pathLen < sizeof(m_path));

    auto& outHdrs       = OMCH2WS::m_currH2Conn->m_outHdrs;   // Ref!
    outHdrs[3].value    = reinterpret_cast<uint8_t*>(m_path);
    outHdrs[3].valuelen = pathLen;

    // QueryPfx + "order?":
    auto res = SendReq<QueryPfxLen + 6>("DELETE", clReqID);

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
  template <Binance::AssetClassT::type AC>
  bool EConnector_H2WS_Binance_OMC<AC>::ModifyOrder
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
      LOG_WARN(1, "EConnector_H2WS_Binance_OMC::ModifyOrder: "
                  "No Active H2Conns")
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
  template <Binance::AssetClassT::type AC>
  void EConnector_H2WS_Binance_OMC<AC>::ModifyOrderImpl
  (
    EConnector_H2WS_Binance_OMC* DEBUG_ONLY(a_dummy),
    Req12*                       a_mod_req0,
    Req12*                       a_mod_req1,
    Req12 const*                 a_orig_req,
    bool                         CHECK_ONLY(a_batch_send)
  )
  {
    //-----------------------------------------------------------------------//
    // Checks:                                                               //
    //-----------------------------------------------------------------------//
    // In Binance, both "ModReq0" and "ModReq1" are non-NULL:
    //
    assert(a_dummy    == this    && a_mod_req0 != nullptr &&
           a_mod_req1 != nullptr && a_orig_req != nullptr &&
           a_mod_req1->m_id      >  a_orig_req->m_id);

    // Check once again for unavailable "m_currH2Conn" (as this method may be
    // invoked by-passing "ModifyOrder" above).  Throw an exception if cannot
    // send, as there is no other way of reporting a failure at this point:
    //
    if (utxx::unlikely(OMCH2WS::m_currH2Conn == nullptr))
      throw utxx::runtime_error
            ("EConnector_H2WS_Binance_OMC::ModifyOrderImpl: No Active H2Conns");

    CHECK_ONLY
    (
      // NB: BatchSend is NOT supported for Binance,    as the latter requires a
      // sequence of separate H2 Frames, all of different size, and using IOVecs
      // or WriteV is not possible over GNUTLS. The corresp flag is simply igno-
      // red:
      if (utxx::unlikely(a_batch_send))
      {
        LOG_WARN(1,
          "EConnector_H2WS_Binance_OMC::ModifyOrderImpl: BatchSend is not "
          "supported: Flag Ignored")
        a_batch_send = false;
      }
    )
    //-----------------------------------------------------------------------//
    // The "Cancel" Leg: Re-use "CancelOrderImpl":                           //
    //-----------------------------------------------------------------------//
    // NB: CxlReq=ModReq0, BatchSend=false:
    CancelOrderImpl(this, a_mod_req0, a_orig_req, false);

    //-----------------------------------------------------------------------//
    // The "New" Leg:    Re-use "NewOrderImpl":                              //
    //-----------------------------------------------------------------------//
    // NB: NewReq=ModReq1, BatchSend=false:
    NewOrderImpl   (this, a_mod_req1, false);
  }

  //=========================================================================//
  // "CancelAllOrders":                                                      //
  //=========================================================================//
  //=========================================================================//
  // "CancelAllOrders" (external, virtual overriding):                       //
  //=========================================================================//
  template <Binance::AssetClassT::type AC>
  void EConnector_H2WS_Binance_OMC<AC>::CancelAllOrders
  (
    unsigned long  a_strat_hash48,
    SecDefD const* a_instr,
    FIX::SideT     a_side,
    char const*    a_segm_id
  )
  {
    if (utxx::unlikely(OMCH2WS::m_currH2Conn == nullptr))
      throw utxx::runtime_error("EConnector_H2WS_Binance_OMC::CancelAllOrders: "
                                "No Active H2Conns");

    // In case of Futures the below will invoke CancelAllOrdersImpl
    EConnector_OrdMgmt::CancelAllOrdersGen<QT, QR>
    (
      this, this, a_strat_hash48, a_instr, a_side, a_segm_id
    );
  }

  //=========================================================================//
  // "CancelAllOrdersImpl":                                                  //
  //=========================================================================//
  template <Binance::AssetClassT::type AC>
  void EConnector_H2WS_Binance_OMC<AC>::CancelAllOrdersImpl
  (
    EConnector_H2WS_Binance_OMC<AC>* UNUSED_PARAM(a_sess),
    unsigned long  UNUSED_PARAM(a_strat_hash48),
    SecDefD const* a_instr,
    FIX::SideT     UNUSED_PARAM(a_side),
    char const*    UNUSED_PARAM(a_segm_id)
  )
  {
    // Check once again for unavailable "m_currH2Conn"    (as this method may be
    // invoked by-passing "CancelAllOrders" above). Throw an exception if cannot
    // send, as there is no other way of reporting a failure at this point:
    //
    if (utxx::unlikely(OMCH2WS::m_currH2Conn == nullptr))
      throw utxx::runtime_error
            ("EConnector_H2WS_Binance_OMC::CancelAllOrdersImpl: "
             "No Active H2Conns");
    try
    {
      // If a_instr is NOT specified (nullptr), cancel all orders
      // There is native Cancel-All functionality common for SPT and FUT:
      for (SecDefD const* sd: EConnector::m_usedSecDefs)
      {
        if (a_instr != nullptr && a_instr != sd)
          continue;

        char* msg = m_path;
        msg  = stpcpy(msg, QueryPfx);
        if constexpr (IsFut)
          msg  = stpcpy(msg, "allOpenOrders?recvWindow=5000&timestamp=");
        else
          msg  = stpcpy(msg, "openOrders?recvWindow=5000&timestamp=");

        msg  = utxx::itoa_left<long, 13>(msg, utxx::now_utc().milliseconds());
        msg  = stpcpy(msg, "&symbol=");
        msg  = stpcpy(msg, sd->m_Symbol.data());
        *msg = '\0';       // 0-terminate it for logging

        size_t pathLen = size_t(msg - m_path);
        assert(pathLen < sizeof(m_path));
        auto& outHdrs       = OMCH2WS::m_currH2Conn->m_outHdrs;    // Ref!
        outHdrs[3].value    = reinterpret_cast<uint8_t*>(m_path);
        outHdrs[3].valuelen = pathLen;

        // QueryPfx + "allOpenOrders?"
        // or QueryPfx + "openOrders?"
        // NB: Passing ReqID=0, which will automatically create an unmapped
        // StreamID -- no response is expected:
        uint32_t streamID = 0;

        if constexpr (IsFut)
        {
          auto res = SendReq<QueryPfxLen + 14>("DELETE", 0);
          streamID = std::get<2>(res);
        }
        else
        {
          auto res = SendReq<QueryPfxLen + 11>("DELETE", 0);
          streamID = std::get<2>(res);
        }

        OMCH2WS::m_currH2Conn->m_lastCancelAllStmID = streamID;

        LOG_INFO(2, "EConnector_H2WS_Binance_OMC::CancelAllOrders: ConnID={}, "
                    "Symbol={}: StreamID={}",
                    OMCH2WS::m_currH2Conn->m_id,
                    sd->m_Symbol.data(), streamID)

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

  //-------------------------------------------------------------------------//
  // "RequestAPIKey":                                                        //
  //-------------------------------------------------------------------------//
  template <Binance::AssetClassT::type AC>
  void EConnector_H2WS_Binance_OMC<AC>::RequestAPIKey()
  {
    if (utxx::unlikely(OMCH2WS::m_currH2Conn == nullptr))
      throw utxx::runtime_error
            ("EConnector_H2WS_Binance_OMC::RequestAPIKey: No Active H2Conns");
    try
    {
      char* msg = m_path;

      msg  = stpcpy(msg, "/sapi/v1/margin/");
      msg  = stpcpy(msg, "apiKey?apiName=tdigital&timestamp=");
      msg  = utxx::itoa_left<long, 13>(msg, utxx::now_utc().milliseconds());

      //---------------------------------------------------------------------//
      // Point the Path Hdr Val to "m_path":                                 //
      //---------------------------------------------------------------------//
      *msg = '\0';     // 0-terminate it for logging
      size_t pathLen = size_t(msg - m_path);
      assert(pathLen < sizeof(m_path));

      auto& outHdrs       = OMCH2WS::m_currH2Conn->m_outHdrs;    // Ref!
      outHdrs[3].value    = reinterpret_cast<uint8_t*>(m_path);
      outHdrs[3].valuelen = pathLen;

      // QueryPfx + "apiKey?":
      auto     res        = SendReq<QueryPfxLen + 7>("POST", 0);
      uint32_t streamID   = std::get<2>(res);
      OMCH2WS::m_currH2Conn->m_lastAPIKeyStmID = streamID;

      LOG_INFO(2, "EConnector_H2WS_Binance_OMC::RequestAPIKey: ConnID={}, "
                  "StreamID={}", OMCH2WS::m_currH2Conn->m_id, streamID)

      OMCH2WS::RotateH2Conns();
    }
    catch (std::exception const& exn)
    {
      LOG_WARN(2, "EConnector_H2WS_Binance_OMC::RequestAPIKey, Exception={}",
                  exn.what())
      throw;
    }
  }

  //-------------------------------------------------------------------------//
  // "RequestPositions":                                                     //
  //-------------------------------------------------------------------------//
  template <Binance::AssetClassT::type AC>
  void EConnector_H2WS_Binance_OMC<AC>::RequestPositions()
  {
    if (utxx::unlikely(OMCH2WS::m_currH2Conn == nullptr))
      throw utxx::runtime_error
            ("EConnector_H2WS_Binance_OMC::RequestPositions: "
             "No Active H2Conns");

    // Positions are only available for Futures
    if constexpr (IsFut)
      try
      {
        char* msg = m_path;
        msg  = stpcpy(msg, QueryPfx);
        msg  = stpcpy(msg, "positionRisk?recvWindow=5000&timestamp=");
        msg  = utxx::itoa_left<long, 13>(msg, utxx::now_utc().milliseconds());
        *msg = '\0';         // 0-terminate it for logging

        size_t pathLen = size_t(msg - m_path);
        assert(pathLen < sizeof(m_path));
        auto& outHdrs        = OMCH2WS::m_currH2Conn->m_outHdrs;    // Ref!
        outHdrs[3].value     = reinterpret_cast<uint8_t*>(m_path);
        outHdrs[3].valuelen  = pathLen;

        // QueryPfx + "positionRisk?"
        auto     res         = SendReq<QueryPfxLen + 13>("GET", 0);
        uint32_t streamID    = std::get<2>(res);
        OMCH2WS::m_currH2Conn->m_lastPositionsStmID = streamID;

        LOG_INFO(2, "EConnector_H2WS_Binance_OMC::RequestPositions: ConnID={}, "
                    "StreamID={}", OMCH2WS::m_currH2Conn->m_id, streamID)

        OMCH2WS::RotateH2Conns();
      }
      catch (std::exception const& exn)
      {
        LOG_WARN(2, "EConnector_H2WS_Binance_OMC::RequestPositions: "
                    "Exception={}", exn.what())
        throw;
      }
  }

  //-------------------------------------------------------------------------//
  // "RequestBalance":                                                       //
  //-------------------------------------------------------------------------//
  template <Binance::AssetClassT::type AC>
  void EConnector_H2WS_Binance_OMC<AC>::RequestBalance()
  {
    if (utxx::unlikely(OMCH2WS::m_currH2Conn == nullptr))
      throw utxx::runtime_error
            ("EConnector_H2WS_Binance_OMC::RequestBalance: No Active H2Conns");

    // Balances only make sense for Spot
    if constexpr (!IsFut)
      try
      {
        char* msg = m_path;
        msg  = stpcpy(msg, QueryPfx);
        msg  = stpcpy(msg, "account?recvWindow=5000&timestamp=");
        msg  = utxx::itoa_left<long, 13>(msg, utxx::now_utc().milliseconds());
        *msg = '\0';     // 0-terminate it for logging

        size_t pathLen = size_t(msg - m_path);
        assert(pathLen < sizeof(m_path));
        auto& outHdrs       = OMCH2WS::m_currH2Conn->m_outHdrs;   // Ref!
        outHdrs[3].value    = reinterpret_cast<uint8_t*>(m_path);
        outHdrs[3].valuelen = pathLen;

        // QueryPfx + "account?":
        auto     res        = SendReq<QueryPfxLen + 8>("GET", 0);
        uint32_t streamID   = std::get<2>(res);
        OMCH2WS::m_currH2Conn->m_lastBalanceStmID = streamID;

        LOG_INFO(2, "EConnector_H2WS_Binance_OMC::RequestBalance: ConnID={}, "
                    "StreamID={}", OMCH2WS::m_currH2Conn->m_id, streamID)

        OMCH2WS::RotateH2Conns();
      }
      catch (std::exception const& exn)
      {
        LOG_WARN(2, "EConnector_H2WS_Binance_OMC::RequestBalance: "
                    "Exception={}", exn.what())
        throw;
      }
  }

  //-------------------------------------------------------------------------//
  // "RequestListenKey":                                                     //
  //-------------------------------------------------------------------------//
  template <Binance::AssetClassT::type AC>
  void EConnector_H2WS_Binance_OMC<AC>::RequestListenKey()
  {
   if (utxx::unlikely(OMCH2WS::m_currH2Conn == nullptr))
      throw utxx::runtime_error
            ("EConnector_H2WS_Binance_OMC::RequestListenKey: "
             "No Active H2Conns");

    // Send out a req for a ListenKey:
    try
    {
      auto     res      = SendReq<0>("POST", 0, ListenKey);
      uint32_t streamID = std::get<2>(res);
      OMCH2WS::m_currH2Conn->m_lastListenKeyStmID = streamID;

      LOG_INFO(2, "EConnector_H2WS_Binance_OMC::RequestListenKey: ConnID={}, "
                  "StreamID={}", OMCH2WS::m_currH2Conn->m_id, streamID)

      OMCH2WS::RotateH2Conns();

      // XXX: NB: We may also make other reqs here:
      if (!m_balOrPosReceived)
      {
        if constexpr (IsFut)
          RequestPositions();
        else
          RequestBalance();
      }
    }
    catch (std::exception const& exn)
    {
      LOG_WARN(2, "EConnector_H2WS_Binance_OMC::RequestListenKey: Exception={}",
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
  template <Binance::AssetClassT::type AC>
  utxx::time_val EConnector_H2WS_Binance_OMC<AC>::FlushOrders()
    { return utxx::time_val(); }

  template <Binance::AssetClassT::type AC>
  utxx::time_val EConnector_H2WS_Binance_OMC<AC>::FlushOrdersImpl
    (EConnector_H2WS_Binance_OMC* UNUSED_PARAM(a_dummy)) const
    { return utxx::time_val(); }
}
// End namespace MAQUETTE
