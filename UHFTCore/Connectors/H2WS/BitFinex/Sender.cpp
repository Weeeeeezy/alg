// vim:ts=2:et
//===========================================================================//
//                    "Connectors/H2WS/BitFinex/Sender.cpp":                 //
//                Generation and Sending of BitFinex OrdMgmt Msgs            //
//===========================================================================//
#include "Connectors/H2WS/BitFinex/EConnector_WS_BitFinex_OMC.h"
#include "Connectors/EConnector_OrdMgmt.hpp"
#include "Protocols/FIX/Msgs.h"
#include <cassert>
#include <climits>
#include <cstring>
#include <sys/random.h>
#include <utxx/compiler_hints.hpp>
#include <utxx/error.hpp>

namespace MAQUETTE
{
  //=========================================================================//
  // "EncodeReqIDInTIF":                                                     //
  //=========================================================================//
  // Outputs a string like: "tif":"YYYY-MM-DD hh:mm:ss.sss"
  //
  inline char* EConnector_WS_BitFinex_OMC::EncodeReqIDInTIF
    (char* a_buff, OrderID a_clord_id)
  {
    // Check that we are still in the same date as when the OMC was started:
    CHECK_ONLY
    (
      time_t diffSec =  (utxx::now_utc() - m_currDate).sec();
      assert(diffSec >= 0);
      if (utxx::unlikely(diffSec >= 86400))
        throw utxx::runtime_error
              ("EConnector_WS_BitFinex_OMC::EncodeReqIDInTIF: UTC Date rolled,"
               " must re-start the OMC");
    )
    // Now fill in the TiF Fld:
    char* curr = stpcpy(a_buff, "\"tif\":\"");

    // Install the Curr UTC Date (XXX: this may result in unpleasant edge eff-
    // ects if we are close to the end of that date):
    curr = stpcpy(curr, m_currDateStr);

    // Put the Time: " 23:5M:ss.mmm":
    curr = stpcpy(curr, " 23:5");

    // Compute the minutes, seconds and missiseconds encoding "a_clord_id". Leap
    // seconds are not allowed:
    unsigned msec = unsigned(a_clord_id % 1000);
    a_clord_id   /= 1000;
    unsigned sec  = unsigned(a_clord_id % 60);
    unsigned min  = unsigned(a_clord_id / 60);
    assert(msec <= 999 && sec <= 59 && min <= 9);

    (void) utxx::itoa_right<unsigned, 1, char>(curr, min,  '0');
    ++curr;
    *curr = ':';
    ++curr;
    (void) utxx::itoa_right<unsigned, 2, char>(curr, sec,  '0');
    curr += 2;
    *curr = '.';
    ++curr;
    (void) utxx::itoa_right<unsigned, 3, char>(curr, msec, '0');
    curr += 3;

    *curr = '"';  // The following ',' is resp of the CallER
    ++curr;
    return curr;
  }

  //=========================================================================//
  // "FlushOrders":                                                          //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // (1) Internal implementation:                                            //
  //-------------------------------------------------------------------------//
  utxx::time_val EConnector_WS_BitFinex_OMC::FlushOrdersImpl
    (EConnector_WS_BitFinex_OMC* DEBUG_ONLY(a_dummy)) const
  {
    // IMPORTANT: Because BatchSend is NOT supported for this OMC, the buffer
    // should be empty at this point, so there is nothing to do:
    assert(a_dummy == this && m_outBuffDataLen == 0);
    return utxx::time_val();
  }

  //-------------------------------------------------------------------------//
  // (2) The externally-visible (virtual) method:                            //
  //-------------------------------------------------------------------------//
  utxx::time_val EConnector_WS_BitFinex_OMC::FlushOrders()
  {
    // IMPORTANT: Because BatchSend is NOT supported for this OMC, the buffer
    // should be empty at this point, so there is nothing to do:
    assert(m_outBuffDataLen == 0);
    return utxx::time_val();
  }

  //=========================================================================//
  // Application-Level Msgs:                                                 //
  //=========================================================================//
  //=========================================================================//
  // "NewOrder":                                                             //
  //=========================================================================//
  //=========================================================================//
  // External "NewOrder" (virtual overriding) method:                        //
  //=========================================================================//
  AOS const* EConnector_WS_BitFinex_OMC::NewOrder
  (
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
    double            a_peg_offset
  )
  {
    CHECK_ONLY
    (
      // NB: BatchSend is NOT supported for BitFinex, as the latter requires a
      // separate WS Frame, and using IOVecs / WriteV is not possible over GNU-
      // TLS:
      if (utxx::unlikely(a_batch_send))
        LOG_WARN(3,
          "EConnector_WS_BitFinex_OMC::NewOrder: BatchSend is not supported"
          ": Ignored")

      // Only Limit Orders are currently supported, so the Px must be valid:
      if (utxx::unlikely
         (a_ord_type == FIX::OrderTypeT::Limit && !IsFinite(a_px)))
        throw utxx::badarg_error
              ("EConnector_WS_BitFinex_OMC::NewOrder: "
               "Limit Order price invalid!");

      // Check the Qty. Advanced Qty Modes are not supported (XXX QtyMin=0 means
      // no minimum):
      if (utxx::unlikely
         (!(IsPos (a_qty) && (IsPosInf(a_qty_show) || a_qty_show == a_qty) &&
            IsZero(a_qty_min))))
        throw utxx::badarg_error
              ("EConnector_WS_BitFinex_OMC::NewOrder: Invalid/UnSupported Qtys"
               ": Qty=",  QR(a_qty), ", QtyShow=", QR(a_qty_show),  ", QtyMin=",
               QR(a_qty_min));

      // Also, in BitFinex,  due to our embedding  of ReqIDs  in TiF TimeStamps,
      // we effectively have TiF=Day only, and must not run the OMC continuously
      // across UTC date boundaries:
      if (utxx::unlikely
         (a_time_in_force != FIX::TimeInForceT::Day))
        throw utxx::badarg_error
              ("EConnector_WS_BitFinex_OMC::NewOrder: UnSupported TimeInForce "
               "(must be Day)");

      if (utxx::unlikely
         (a_expire_date != 0 && a_expire_date != GetCurrDateInt()))
        throw utxx::badarg_error
              ("EConnector_WS_BitFinex_OMC::NewOrder: UnSupported ExpireDate: ",
               a_expire_date);

      if (utxx::unlikely(IsFinite(a_peg_offset)))
        throw utxx::badarg_error
              ("EConnector_WS_BitFinex_OMC::NewOrder: Pegged Orders: Not "
               "Supported");
    )
    // So BatchSend is always reset:
    a_batch_send = false;

    // Notionally, we set ExpireDate to Today (though it is not used explicitly
    // below):
    a_expire_date  = GetCurrDateInt();

    // XXX: Hmm:
    // 5 significant digits on BitFinex, i.e. 123.45, 1234.5, 12345.0
    // Hope BTC doesnt drop below 1000
    a_px = QR(a_px) >= 10'000 ? Round(a_px, 1.0)
                              : QR(a_px) >= 1'000 ? Round(a_px, 0.1)
                                                  : Round(a_px, 0.01);

    //-----------------------------------------------------------------------//
    // Invoke the Generic Impl:                                              //
    //-----------------------------------------------------------------------//
    // Invoke                EConnector_OrdMgmt    ::NewOrderGen (),
    // which in turn invokes EConnector_WS_BitFinex_OMC::NewOrderImpl()
    //                       (see below):
    a_qty_show = a_qty;
    a_qty_min  = QtyAny::Zero();
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
  // Internal "NewOrderImpl" ("EConnector_OrdMgmt" Call-Back):               //
  //=========================================================================//
  void EConnector_WS_BitFinex_OMC::NewOrderImpl
  (
    EConnector_WS_BitFinex_OMC* DEBUG_ONLY(a_dummy),
    Req12*                      a_new_req,                  // Non-NULL
    bool                        DEBUG_ONLY(a_batch_send)    // Always false
  )
  {
    //-----------------------------------------------------------------------//
    // NewOrder Params:                                                      //
    //-----------------------------------------------------------------------//
    assert(a_dummy == this     && a_new_req != nullptr && !a_batch_send &&
           a_new_req->m_kind   == Req12::KindT::New    &&
           a_new_req->m_status == Req12::StatusT::Indicated);

    // ReqID, AOS and SecDef:
    OrderID           newReqID = a_new_req->m_id;
    assert(newReqID != 0);
    AOS const*        aos      = a_new_req->m_aos;
    assert(aos   != nullptr);
    SecDefD const*    instr    = aos->m_instr;
    assert(instr != nullptr);

    // OrderType: TODO: Only Limit and Market Orders are currently supported:
    FIX::OrderTypeT   ordType  = aos->m_orderType;
    assert(ordType == FIX::OrderTypeT::Limit ||
           ordType == FIX::OrderTypeT::Market);

    // TimeInForce: XXX: MUST currently be "Day" due to ReqID embedding into
    // the TimeStamps:
    DEBUG_ONLY(FIX::TimeInForceT timeInForce = aos->m_timeInForce;)
    assert(timeInForce  ==  FIX::TimeInForceT::Day);

    // Side and Px (for Limit orders only):
    bool   isBuy  = aos->m_isBuy;
    PriceT px     = a_new_req->m_px;
    assert(IsFinite(px) == (ordType == FIX::OrderTypeT::Limit));

    // Qtys:
    QtyN   qty    = a_new_req->GetQty    <QT,QR>();
    assert(IsPos(qty));
#   ifndef NDEBUG
    QtyN  qtyShow = a_new_req->GetQtyShow<QT,QR>();
    QtyN  qtyMin  = a_new_req->GetQtyMin <QT,QR>();
#   endif
    assert(IsZero(qtyMin) && qtyShow == qty);

    // Misc:
    DEBUG_ONLY(int    expireDate = aos->m_expireDate;      )
    assert(expireDate == GetCurrDateInt());
    DEBUG_ONLY(double pegOffset  = a_new_req->m_pegOffset; )
    assert(!IsFinite(pegOffset));

    //-----------------------------------------------------------------------//
    // Output the msg:                                                       //
    //-----------------------------------------------------------------------//
    // Start the msg, appending it to any previously-output ones:
    char* data  = m_outBuff   + OutBuffDataOff;
    char* chunk = data        + m_outBuffDataLen;
    char* curr  = stpcpy(chunk, "[0,\"on\",null,{");

    // ClientOrderID: This is the AOSID, not NewReqID (the latter is encoded in
    // TiF below):
    curr  = stpcpy    (curr, "\"cid\":");
    curr  = utxx::itoa_left<OrderID, 20>(curr, aos->m_id);

    if (ordType == FIX::OrderTypeT::Limit)
      curr = stpcpy(curr, ",\"type\":\"LIMIT\",\"symbol\":\"");
    else
      curr = stpcpy(curr, ",\"type\":\"MARKET\",\"symbol\":\"");

    // Symbol:
    curr  = stpcpy(curr, instr->m_Symbol.data());
    curr  = stpcpy(curr, "\",");

    // IMPORTANT: BitFinex does not change the ClOrdID on Order Modification,
    // so the very first ClOrdID is always returned with all responses. This
    // is contrary to the state mgmt logic of "EConnector_OrdMgmt". So we en-
    // code the ClOrdID also in TimeInForce:   as the number of missiseconds
    // since 23:50:00 UTC of the CurrDate (ie, 600k Reqs per day):
    //
    curr  = this->EncodeReqIDInTIF(curr, newReqID);

    // Price: XXX: using fixed precision of 8 digits. No 0-terminator is requi-
    // red, as we would over-write it anyway by continued output. The floating-
    // point Px is output as JSON string:
    int n = 0;
    if (ordType == FIX::OrderTypeT::Limit)
    {
      // For a Limit order, the Px is mandatory:
      curr = stpcpy(curr, ",\"price\":\"");
      n    = utxx::ftoa_left<false>(double(px),  curr, 32, 8);
      assert(n > 0);
      curr += n;
      curr = stpcpy(curr, "\",\"flags\":4096");
    }
    // Qty: Again, using the JSON string format; no 0-terminator is required.
    // XXX: Use +- sign of Qty for Buy/Sell:
    double  sQty = isBuy ? double(qty) : - double(qty);
    curr =  stpcpy(curr, ",\"amount\":\"");
    n    =  utxx::ftoa_left<false>(sQty, curr, 32, 8);
    assert(n > 0);
    curr += n;

    // End the msg: Wrap it up with the TotalDataLen so far:
    curr  = stpcpy(curr, "\"}]");
    ProWS::WrapTxtData(int(curr - data));
    CHECK_ONLY(OMCWS::LogMsg<true>(chunk, "NEW REQ");)  // IsSend=true

    //-----------------------------------------------------------------------//
    // Send it out (No Delays!):                                             //
    //-----------------------------------------------------------------------//
    // "m_txSN" increment is for compatibility with "EConnector_OrdMgmt" only;
    // it is not really used here:
    SeqNum         seqNum = (*m_txSN)++;
    utxx::time_val sendTS = ProWS::SendTxtFrame();

    //-----------------------------------------------------------------------//
    // Update the "NewReq":                                                  //
    //-----------------------------------------------------------------------//
    a_new_req->m_status  = Req12::StatusT::New;
    a_new_req->m_ts_sent = sendTS;
    a_new_req->m_seqNum  = seqNum;
  }

  //=========================================================================//
  // "CancelOrder":                                                          //
  //=========================================================================//
  //=========================================================================//
  // External "CancelOrder" (virtual overriding) method:                     //
  //=========================================================================//
  bool EConnector_WS_BitFinex_OMC::CancelOrder
  (
    AOS const*     a_aos,       // May be NULL...
    utxx::time_val a_ts_md_exch,
    utxx::time_val a_ts_md_conn,
    utxx::time_val a_ts_md_strat,
    bool           a_batch_send
  )
  {
    // As for "NewOrder", BatchSend is not possible in BitFinex OMC:
    CHECK_ONLY
    (
      if (utxx::unlikely(a_batch_send))
        LOG_WARN(3,
          "EConnector_WS_BitFinex_OMC::CancelOrder: BatchSend is not "
          "supported: Ignored")
    )
    // In any case:
    a_batch_send = false;

    // Invoke                EConnector_OrdMgmt    ::CancelOrderGen (),
    // which in turn invokes EConnector_WS_BitFinex_OMC::CancelOrderImpl()
    //                                               (see below):
    return
      EConnector_OrdMgmt::CancelOrderGen<QT, QR>
      ( // ProtoEng   SessMgr
        this,         this,         const_cast<AOS*>(a_aos),
        a_ts_md_exch, a_ts_md_conn, a_ts_md_strat,   a_batch_send
      );
  }

  //=========================================================================//
  // Internal "CancelOrderImpl" ("EConnector_OrdMgmt" Call-Back):            //
  //=========================================================================//
  void EConnector_WS_BitFinex_OMC::CancelOrderImpl
  (
    EConnector_WS_BitFinex_OMC* DEBUG_ONLY(a_dummy),
    Req12*                      a_clx_req,
    Req12 const*                a_orig_req,
    bool                        DEBUG_ONLY(a_batch_send) // Always false
  )
  {
    //-----------------------------------------------------------------------//
    // Cancel Params:                                                        //
    //-----------------------------------------------------------------------//
    assert(a_dummy == this && a_clx_req != nullptr && a_orig_req != nullptr &&
          !a_batch_send    &&
           a_clx_req ->m_kind   == Req12::KindT::Cancel      &&
           a_clx_req ->m_status == Req12::StatusT::Indicated &&
           a_orig_req->m_status >= Req12::StatusT::New);

    DEBUG_ONLY(OrderID clReqID = a_clx_req->m_id);
    assert(clReqID != 0 && a_orig_req->m_id != 0 &&
           clReqID >       a_orig_req->m_id);

    AOS const* aos =  a_clx_req->m_aos;
    assert(aos != nullptr && aos == a_orig_req->m_aos);

    //-----------------------------------------------------------------------//
    // Output the msg:                                                       //
    //-----------------------------------------------------------------------//
    // Start the msg, appending it to any previously-output ones:
    char* data  = m_outBuff + OutBuffDataOff;
    char* chunk = data      + m_outBuffDataLen;

    // NB: BitFinex ClOrdID is actually the AOSID with CurrDate, not ReqID. The
    // latter is normally encoded in TimeInForce, but that is not applicable to
    // "Cancel", so in this case, ReqID is skipped altogether. But we also send
    // the ExchOrdID for extra reliability:
    //
    char* curr  = stpcpy(chunk, "[0,\"oc\",null,{\"cid\":");
    curr   = utxx::itoa_left<OrderID, 20>(curr, aos->m_id);
    curr   = stpcpy(curr, ",\"cid_date\":\"");
    curr   = stpcpy(curr, m_currDateStr);

    // As "SendExchOrdIDs" is enabled for BitFinex, "EConnector_OrdMgmt" would
    // guarantee that we have an ExchOrdID to send:
    assert(!(a_orig_req->m_exchOrdID.IsEmpty()));
    curr = stpcpy(curr, "\",\"id\":");
    curr = utxx::itoa_left<OrderID, 20>
             (curr, a_orig_req->m_exchOrdID.GetOrderID());

    // NB: For "Cancel", we cannot encode the CxlClOrdID into TIF...
    // End the msg: Wrap it up with the TotalDataLen so far:
    curr  = stpcpy(curr, "}]");
    ProWS::WrapTxtData(int(curr - data));
    CHECK_ONLY(OMCWS::LogMsg<true>(chunk, "CANCEL REQ");)  // IsSend=true

    //-----------------------------------------------------------------------//
    // Send It Out (No Delays!):                                             //
    //-----------------------------------------------------------------------//
    SeqNum         seqNum = (*m_txSN)++;
    utxx::time_val sendTS = ProWS::SendTxtFrame();

    //-----------------------------------------------------------------------//
    // Update the "CxlReq":                                                  //
    //-----------------------------------------------------------------------//
    a_clx_req->m_status   = Req12::StatusT::New;
    a_clx_req->m_ts_sent  = sendTS;
    a_clx_req->m_seqNum   = seqNum;
  }

  //=========================================================================//
  // "ModifyOrder":                                                          //
  //=========================================================================//
  //=========================================================================//
  // External "ModifyOrder" (virtual overriding) method:                     //
  //=========================================================================//
  bool EConnector_WS_BitFinex_OMC::ModifyOrder
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
    CHECK_ONLY
    (
      // NewPx must be valid (as it is a Limit Order):
      if (utxx::unlikely(!IsFinite(a_new_px)))
        throw utxx::badarg_error
              ("EConnector_WS_BitFinex_OMC::ModifyOrder: Invalid NewPx");

      // NewQty must be valid as well. Advanced Qty Modes are not supported:
      if (utxx::unlikely
         (!(IsPos   (a_new_qty)                                      &&
           (IsPosInf(a_new_qty_show) || a_new_qty_show == a_new_qty) &&
            IsZero  (a_new_qty_min))))
        throw utxx::badarg_error
              ("EConnector_WS_BitFinex_OMC::ModifyOrder: Invalid/UnSupported "
               "Qtys: NewQty=",    QR(a_new_qty),   ", NewQtyShow=",
               QR(a_new_qty_show), ", NewQtyMin=",  QR(a_new_qty_min));

      // Again, BatchSend is not possible, but it is not an error:
      if (utxx::unlikely(a_batch_send))
        LOG_WARN(3,
          "EConnector_WS_BitFinex_OMC::ModifyOrder: BatchSend is not "
          "supported: Ignored")
    )
    // In any case:
    a_batch_send = false;

    // XXX: Hmm:
    // 5 significant digits on BitFinex, i.e. 123.45, 1234.5, 12345.0
    // Hope BTC doesnt drop below 1000
    a_new_px = QR(a_new_px) >= 10'000
             ? Round(a_new_px, 1.0)
             : QR(a_new_px) >= 1'000 ? Round(a_new_px, 0.1)
                                     : Round(a_new_px, 0.01);

    // Invoke                EConnector_OrdMgmt    ::ModifyOrderGen (),
    // which in turn invokes EConnector_WS_BitFinex_OMC::ModifyOrderImpl()
    //                                               (see below):
    a_new_qty_show = a_new_qty;
    a_new_qty_min  = QtyAny::Zero();

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
  void EConnector_WS_BitFinex_OMC::ModifyOrderImpl
  (
    EConnector_WS_BitFinex_OMC* DEBUG_ONLY(a_dummy),
    Req12*                      DEBUG_ONLY(a_mod_req0),    // NULL
    Req12*                      a_mod_req1,                // Non-NULL
    Req12 const*                a_orig_req,                // Non-NULL
    bool                        DEBUG_ONLY(a_batch_send)   // Always false
  )
  {
    //-----------------------------------------------------------------------//
    // Modify Params:                                                        //
    //-----------------------------------------------------------------------//
    assert(a_dummy == this       && a_orig_req != nullptr     &&
           a_mod_req0 == nullptr && a_mod_req1 != nullptr     &&
           a_orig_req != nullptr && !a_batch_send             &&
           a_mod_req1->m_kind    == Req12::KindT::Modify      &&
           a_mod_req1->m_status  == Req12::StatusT::Indicated &&
           a_orig_req->m_status  >= Req12::StatusT::New);

    OrderID         modReqID = a_mod_req1->m_id;
    assert(modReqID != 0   &&  a_orig_req->m_id != 0 &&
           modReqID >          a_orig_req->m_id);

    AOS const*      aos      = a_mod_req1->m_aos;
    assert(aos != nullptr  &&  a_orig_req->m_aos == aos);

    PriceT          newPx    = a_mod_req1->m_px;
    assert(IsFinite(newPx));

    QtyN newQty     = a_mod_req1->GetQty    <QT,QR>();
    assert(IsPos(newQty));

#   ifndef NDEBUG
    QtyN newQtyShow = a_mod_req1->GetQtyShow<QT,QR>();
    QtyN newQtyMin  = a_mod_req1->GetQtyMin <QT,QR>();
#   endif
    assert(IsZero(newQtyMin) && newQtyShow == newQty);

    //-----------------------------------------------------------------------//
    // Output the msg:                                                       //
    //-----------------------------------------------------------------------//
    // Start the msg, appending it to any previously-output ones:
    char* data  = m_outBuff + OutBuffDataOff;
    char* chunk = data      + m_outBuffDataLen;

    // NB: BitFinex ClOrdID is the AOSID with CurrDate:
    char* curr  = stpcpy(chunk, "[0,\"ou\",null,{\"cid\":");
    curr  = utxx::itoa_left<OrderID, 20>(curr, aos->m_id);
    curr  = stpcpy(curr, ",\"cid_date\":\"");
    curr  = stpcpy(curr, m_currDateStr);
    curr  = stpcpy(curr, "\",");

    // Similar to "NewOrder", encode the ClOrdID of the Modify Req:
    curr  = this->EncodeReqIDInTIF(curr, modReqID);

    // Px:
    curr  = stpcpy(curr, ",\"price\":\"");
    int n = utxx::ftoa_left<false>(double(newPx), curr, 32, 8);
    assert(n > 0);
    curr += n;

    // Qty (signed according to the Order Side):
    curr  = stpcpy(curr, "\",\"amount\":\"");
    double  sQty = (aos->m_isBuy) ? double(newQty) : - double(newQty);
    n     = utxx::ftoa_left<false>(sQty, curr, 32, 8);
    curr += n;

    // Again, ExchOrdID must be available:
    assert(!(a_orig_req->m_exchOrdID.IsEmpty()));
    curr = stpcpy(curr, "\",\"id\":");
    curr = utxx::itoa_left<OrderID, 20>
           (curr, a_orig_req->m_exchOrdID.GetOrderID());

    // End the msg: Wrap it up with the TotalDataLen so far:
    curr  = stpcpy(curr, ",\"flags\":4096}]");
    // curr  = stpcpy(curr, "}]");
    ProWS::WrapTxtData(int(curr - data));
    CHECK_ONLY(OMCWS::LogMsg<true>(chunk, "REPLACE REQ");)  // IsSend=true

    //-----------------------------------------------------------------------//
    // Send It Out (No Delays!):                                             //
    //-----------------------------------------------------------------------//
    SeqNum         seqNum = (*m_txSN)++;
    utxx::time_val sendTS = ProWS::SendTxtFrame();

    //-----------------------------------------------------------------------//
    // Update the "ModReq1":                                                 //
    //-----------------------------------------------------------------------//
    a_mod_req1->m_status  = Req12::StatusT::New;
    a_mod_req1->m_ts_sent = sendTS;
    a_mod_req1->m_seqNum  = seqNum;
  }

  //=========================================================================//
  // "CancelAllOrders":                                                      //
  //=========================================================================//
  void EConnector_WS_BitFinex_OMC::CancelAllOrders
  (
    unsigned long  UNUSED_PARAM(a_strat_hash48),
    SecDefD const* UNUSED_PARAM(a_instr),
    FIX::SideT     UNUSED_PARAM(a_side),
    char const*    UNUSED_PARAM(a_segm_id)
  )
  {
    // Start the msg, appending it to any previously-output ones:
    char* data  = m_outBuff + OutBuffDataOff;
    char* chunk = data      + m_outBuffDataLen;
    char* curr  = stpcpy(chunk, "[0,\"oc_multi\",null,{\"all\":1}]");

    // NB: There is no ClOrdID here...
    // End the msg: Wrap it up with the TotalDataLen so far:
    ProWS::WrapTxtData(int(curr - data));
    CHECK_ONLY(OMCWS::LogMsg<true>(chunk, "CANCEL ALL REQ");)    // IsSend=true

    // This msg is never buffered -- send it out immediately. We formally still
    // need to implement the counter:
    ++(*m_txSN);
    (void) ProWS::SendTxtFrame();
  }
} // End namespace MAQUETTE
