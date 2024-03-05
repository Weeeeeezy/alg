// vim:ts=2:et
//===========================================================================//
//              "Connectors/H2WS/TDAmeritrade/Sender_OMC.cpp":               //
//            Generation and Sending of TDAmeritrade OrdMgmt Msgs            //
//===========================================================================//
#include "Basis/BaseTypes.hpp"
#include "Connectors/EConnector_OrdMgmt.hpp"
#include "Connectors/H2WS/TDAmeritrade/EConnector_H1WS_TDAmeritrade.h"
#include "Protocols/FIX/Msgs.h"
#include <cassert>
#include <climits>
#include <cstring>
#include <sys/random.h>
#include <utxx/compiler_hints.hpp>
#include <utxx/error.hpp>

namespace MAQUETTE {
//=========================================================================//
// "FlushOrders":                                                          //
//=========================================================================//
//-------------------------------------------------------------------------//
// (1) Internal implementation:                                            //
//-------------------------------------------------------------------------//
utxx::time_val EConnector_H1WS_TDAmeritrade::FlushOrdersImpl(
    EConnector_H1WS_TDAmeritrade *DEBUG_ONLY(a_dummy)) const {
  // IMPORTANT: Because BatchSend is NOT supported for this OMC, the buffer
  // should be empty at this point, so there is nothing to do:
  assert(a_dummy == this && m_outBuffDataLen == 0);
  return utxx::time_val();
}

//-------------------------------------------------------------------------//
// (2) The externally-visible (virtual) method:                            //
//-------------------------------------------------------------------------//
utxx::time_val EConnector_H1WS_TDAmeritrade::FlushOrders() {
  // IMPORTANT: Because BatchSend is NOT supported for this OMC, the buffer
  // should be empty at this point, so there is nothing to do:
  assert(m_outBuffDataLen == 0);
  return utxx::time_val();
}

//=========================================================================//
// Application-Level Msgs:                                                 //
//=========================================================================//
const char *EConnector_H1WS_TDAmeritrade::WriteOutOrder(
    char *a_chunk, SecDefD const *a_instr, FIX::OrderTypeT a_ord_type,
    bool a_is_buy, PriceT a_px, QtyN a_qty, FIX::TimeInForceT a_time_in_force,
    int a_expire_date) {
  char *curr = stpcpy(a_chunk, "{\"orderStrategyType\":\"SINGLE\",");

  // Market and Stop orders are only allowed during the regular
  // (NORMAL) session, Limit orders are allowed in all sessions,
  // so use SEAMLESS for them
  if (a_ord_type == FIX::OrderTypeT::Market) {
    curr = stpcpy(curr, "\"session\":\"NORMAL\","
                        "\"orderType\":\"MARKET\",");
    // no price or stopPrice
  } else {
    if (a_ord_type == FIX::OrderTypeT::Stop)
      curr = stpcpy(curr, "\"session\":\"NORMAL\","
                          "\"orderType\":\"STOP\","
                          "\"stopPrice\":");
    else
      curr = stpcpy(curr, "\"session\":\"SEAMLESS\","
                          "\"orderType\":\"LIMIT\","
                          "\"price\":");

    int n = utxx::ftoa_left<false>(double(a_px), curr, 12, 4);
    assert(n > 0);
    curr += n;
    curr = stpcpy(curr, ",");
  }

  if ((a_time_in_force == FIX::TimeInForceT::Day) ||
      (a_ord_type == FIX::OrderTypeT::Market))
    curr = stpcpy(curr, "\"duration\":\"DAY\",");
  else if (a_time_in_force == FIX::TimeInForceT::GoodTillCancel)
    curr = stpcpy(curr, "\"duration\":\"GOOD_TILL_CANCEL\",");
  else if (a_time_in_force == FIX::TimeInForceT::FillOrKill)
    curr = stpcpy(curr, "\"duration\":\"FILL_OR_KILL\",");

  if (a_expire_date > 0) {
    curr = stpcpy(curr, "\"cancelTime\":\"");
    int yr = a_expire_date / 10000;
    a_expire_date -= yr * 10000;
    int mon = a_expire_date / 100;
    a_expire_date -= mon * 100;
    int day = a_expire_date;

    char buf[32];
    sprintf(buf, "%04i-%02i-%02i\",", yr, mon, day);
    curr = stpcpy(curr, buf);
  }

  curr = stpcpy(curr, "\"orderLegCollection\":[{\"instruction\":\"");
  curr = stpcpy(curr, a_is_buy ? "BUY\"," : "SELL\",");
  curr = stpcpy(curr, "\"quantity\":");
  curr = utxx::itoa_left<QR, 10>(curr, QR(a_qty));
  curr = stpcpy(curr, ",\"instrument\":{\"assetType\":\"");
  curr = stpcpy(curr, a_instr->m_SessOrSegmID.data());
  curr = stpcpy(curr, "\",\"symbol\":\"");
  curr = stpcpy(curr, a_instr->m_Symbol.data());
  curr = stpcpy(curr, "\"}}]}");

  return curr;
}

//=========================================================================//
// "NewOrder":                                                             //
//=========================================================================//
//=========================================================================//
// External "NewOrder" (virtual overriding) method:                        //
//=========================================================================//
AOS const* EConnector_H1WS_TDAmeritrade::NewOrder(
    Strategy *a_strategy, SecDefD const &a_instr, FIX::OrderTypeT a_ord_type,
    bool a_is_buy, PriceT a_px, bool a_is_aggr, QtyAny a_qty,
    utxx::time_val a_ts_md_exch, utxx::time_val a_ts_md_conn,
    utxx::time_val a_ts_md_strat, bool a_batch_send,
    FIX::TimeInForceT a_time_in_force, int a_expire_date, QtyAny a_qty_show,
    QtyAny a_qty_min, bool a_peg_side,
    double a_peg_offset) {
  CHECK_ONLY(
      // NB: BatchSend is NOT supported for TDAmeritrade
      if (utxx::unlikely(a_batch_send)) LOG_WARN(
          3,
          "EConnector_H1WS_TDAmeritrade::NewOrder: BatchSend is not supported"
          ": Ignored")

      // TDAmeritrade supports Market, Stop, and Limit orders (it also supports
      // StopLimit, TrailingStop, TrailingStopLimit, but MAQUETTE doesn't
      // support these yet)
      if (utxx::unlikely((a_ord_type != FIX::OrderTypeT::Market) &&
                         (a_ord_type != FIX::OrderTypeT::Limit) &&
                         (a_ord_type != FIX::OrderTypeT::Stop))) throw utxx::
          badarg_error("EConnector_H1WS_TDAmeritrade::NewOrder: "
                       "Unsupported order type!");

      // TDAmeritrade supports Day, GoodTillCancel, FillOrKill
      if (utxx::unlikely(
              (a_time_in_force != FIX::TimeInForceT::Day) &&
              (a_time_in_force != FIX::TimeInForceT::GoodTillCancel) &&
              (a_time_in_force != FIX::TimeInForceT::FillOrKill) &&
              (a_time_in_force !=
               FIX::TimeInForceT::ImmedOrCancel))) throw utxx::
          badarg_error("EConnector_H1WS_TDAmeritrade::NewOrder: "
                       "Unsupported time in force!");

      if (utxx::unlikely((a_expire_date > 0) &&
                         (a_time_in_force !=
                          FIX::TimeInForceT::GoodTillCancel))) throw utxx::
          badarg_error("EConnector_H1WS_TDAmeritrade::NewOrder: "
                       "Got expiry date but not GTC!");

      // Check the Qty. Advanced Qty Modes are not supported (XXX QtyMin=0 means
      // no minimum):
      if (utxx::unlikely
         (!(IsPos(a_qty) &&
           (IsPosInf(a_qty_show) || a_qty_show == a_qty) && IsZero(a_qty_min))))
        throw utxx::badarg_error(
              "EConnector_H1WS_TDAmeritrade::NewOrder: Invalid/UnSupported Qtys"
              ": Qty=",
              QR(a_qty), ", QtyShow=", QR(a_qty_show),
              ", QtyMin=", QR(a_qty_min));

      if (utxx::unlikely(IsFinite(a_peg_offset))) throw utxx::badarg_error(
          "EConnector_H1WS_TDAmeritrade::NewOrder: Pegged Orders: Not "
          "Supported");)
  // So BatchSend is always reset:
  a_batch_send = false;

  //-----------------------------------------------------------------------//
  // Invoke the Generic Impl:                                              //
  //-----------------------------------------------------------------------//
  // Invoke                EConnector_OrdMgmt    ::NewOrderGen (),
  // which in turn invokes EConnector_H1WS_TDAmeritrade::NewOrderImpl()
  //                       (see below):
  a_qty_show = a_qty;
  a_qty_min = QtyAny::Zero();
  auto aos = EConnector_OrdMgmt::NewOrderGen<QT, QR>( // ProtoEng     SessMgr
      this, this, a_strategy, a_instr, a_ord_type, a_is_buy, a_px, a_is_aggr,
      a_qty, a_ts_md_exch, a_ts_md_conn, a_ts_md_strat, a_batch_send,
      a_time_in_force, a_expire_date, a_qty_show, a_qty_min, a_peg_side,
      a_peg_offset);

  auto req = aos->m_lastReq;

  // process HTTP response to our sent order
  if ((GetStatusCode() == 200) || (GetStatusCode() == 201)) {
    // sending order succeeded, we need to set the ExchOrdID
    auto loc = GetLocation();
    auto end = loc + strlen(loc);
    auto path_start = strstr(loc, "/orders/");
    assert((path_start != nullptr) && (path_start >= loc));

    auto exch_ord_id_start = path_start + 8;
    ExchOrdID exch_ord_id(exch_ord_id_start, int(end - exch_ord_id_start));

    LOG_INFO(2,
             "EConnector_H1WS_TDAmeritrade::NewOrder: "
             "New order confirmed, exch_ord_id = {}",
             exch_ord_id.ToString())

    OrderConfirmedNew<QT, QR>
      (this, this, req->m_id, aos->m_id, exch_ord_id, ExchOrdID(), req->m_px,
       req->GetQty<QT,QR>(),  utxx::now_utc(), utxx::now_utc());
  } else {
    // the order was rejected, signal that
    OrderRejected<QT, QR>(this, this, req->m_seqNum, req->m_id, aos->m_id,
                          FuzzyBool::True, int(GetStatusCode()), GetResponse(),
                          utxx::now_utc(), utxx::now_utc());
  }

  return aos;
}

//=========================================================================//
// Internal "NewOrderImpl" ("EConnector_OrdMgmt" Call-Back):               //
//=========================================================================//
void EConnector_H1WS_TDAmeritrade::NewOrderImpl(
    EConnector_H1WS_TDAmeritrade *DEBUG_ONLY(a_dummy),
    Req12 *a_new_req,               // Non-NULL
    bool DEBUG_ONLY(a_batch_send)   // Always false
) {
  //-----------------------------------------------------------------------//
  // NewOrder Params:                                                      //
  //-----------------------------------------------------------------------//
  assert(a_dummy == this && a_new_req != nullptr && !a_batch_send &&
         a_new_req->m_kind == Req12::KindT::New &&
         a_new_req->m_status == Req12::StatusT::Indicated);

  // ReqID, AOS and SecDef:
  DEBUG_ONLY(OrderID newReqID = a_new_req->m_id;)
  assert(newReqID != 0);
  AOS const *aos = a_new_req->m_aos;
  assert(aos != nullptr);
  SecDefD const *instr = aos->m_instr;
  assert(instr != nullptr);

  // OrderType: TODO: Only Market, Limit, Stop orders are currently supported:
  FIX::OrderTypeT ordType = aos->m_orderType;
  assert(ordType == FIX::OrderTypeT::Market ||
         ordType == FIX::OrderTypeT::Limit || ordType == FIX::OrderTypeT::Stop);

  FIX::TimeInForceT timeInForce = aos->m_timeInForce;
  assert(timeInForce == FIX::TimeInForceT::Day ||
         timeInForce == FIX::TimeInForceT::GoodTillCancel ||
         timeInForce == FIX::TimeInForceT::FillOrKill ||
         timeInForce == FIX::TimeInForceT::ImmedOrCancel);

  // Side and Px:
  bool isBuy = aos->m_isBuy;
  PriceT px = a_new_req->m_px;
  assert(IsFinite(px) == (ordType != FIX::OrderTypeT::Market));

  // Qtys:
  QtyN qty     = a_new_req->GetQty<QT,QR>();
  assert(IsPos(qty));
# ifndef NDEBUG
  QtyN qtyShow = a_new_req->GetQtyShow<QT,QR>();
  QtyN qtyMin  = a_new_req->GetQtyMin <QT,QR>();
# endif
  assert(qtyShow == qty && IsZero(qtyMin));

  // Misc:
  int expireDate = aos->m_expireDate;
  DEBUG_ONLY(double pegOffset = a_new_req->m_pegOffset;)
  assert(!IsFinite(pegOffset));

  //-----------------------------------------------------------------------//
  // Output the msg:                                                       //
  //-----------------------------------------------------------------------//
  char *data = m_H1_out_buf;
  char *chunk = data;
  WriteOutOrder(chunk, instr, ordType, isBuy, px, qty, timeInForce, expireDate);
  CHECK_ONLY(TDH1WS::LogMsg<true>(chunk, "[HTTP] NEW REQ");) // IsSend=true

  //-----------------------------------------------------------------------//
  // Update the "NewReq":                                                  //
  //-----------------------------------------------------------------------//
  a_new_req->m_status = Req12::StatusT::New;
  a_new_req->m_ts_sent = utxx::now_utc();
  SeqNum seqNum = (*m_txSN)++;
  a_new_req->m_seqNum = seqNum;

  // now send it out
  char path[32];
  auto curr = stpcpy(path, "/v1/accounts/");
  curr = stpcpy(curr, m_account_id.c_str());
  curr = stpcpy(curr, "/orders");

  POST(path);
}

//=========================================================================//
// "CancelOrder":                                                          //
//=========================================================================//
//=========================================================================//
// External "CancelOrder" (virtual overriding) method:                     //
//=========================================================================//
bool EConnector_H1WS_TDAmeritrade::CancelOrder(AOS const *a_aos, // May be NULL...
                                               utxx::time_val a_ts_md_exch,
                                               utxx::time_val a_ts_md_conn,
                                               utxx::time_val a_ts_md_strat,
                                               bool a_batch_send) {
  // As for "NewOrder", BatchSend is not possible in TDAmeritrade OMC:
  CHECK_ONLY(if (utxx::unlikely(a_batch_send)) LOG_WARN(
      3, "EConnector_H1WS_TDAmeritrade::CancelOrder: BatchSend is not "
         "supported: Ignored"))
  // In any case:
  a_batch_send = false;
  auto orig_req = a_aos->m_lastReq;

  // Invoke                EConnector_OrdMgmt    ::CancelOrderGen (),
  // which in turn invokes EConnector_H1WS_TDAmeritrade::CancelOrderImpl()
  //                                               (see below):
  auto success = EConnector_OrdMgmt::CancelOrderGen<QT, QR>(
      this,   this, const_cast<AOS*>(a_aos),
      a_ts_md_exch, a_ts_md_conn, a_ts_md_strat, a_batch_send);

  auto req = a_aos->m_lastReq;
  assert(orig_req->m_id == req->m_origID);

  // process HTTP response to our sent order
  if ((GetStatusCode() == 200) || (GetStatusCode() == 201)) {
    LOG_INFO(2, "EConnector_H1WS_TDAmeritrade::CancelOrder: Successful")

    OrderCancelled<QT, QR>(this, this, req->m_id, req->m_origID, a_aos->m_id,
                           orig_req->m_exchOrdID, orig_req->m_mdEntryID,
                           orig_req->m_px, QtyN::Invalid(), utxx::now_utc(),
                           utxx::now_utc());
  } else {
    // sending the cancel failed
    LOG_ERROR(1,
              "EConnector_H1WS_TDAmeritrade::CancelOrder: Failed "
              "{}: {}",
              GetStatusCode(), GetResponse())
    return false;
  }

  return success;
}

//=========================================================================//
// Internal "CancelOrderImpl" ("EConnector_OrdMgmt" Call-Back):            //
//=========================================================================//
void EConnector_H1WS_TDAmeritrade::CancelOrderImpl(
    EConnector_H1WS_TDAmeritrade *DEBUG_ONLY(a_dummy), Req12 *a_clx_req,
    Req12 const *a_orig_req,
    bool DEBUG_ONLY(a_batch_send) // Always false
) {
  //-----------------------------------------------------------------------//
  // Cancel Params:                                                        //
  //-----------------------------------------------------------------------//
  assert(a_dummy == this && a_clx_req != nullptr && a_orig_req != nullptr &&
         !a_batch_send   && a_clx_req->m_kind == Req12::KindT::Cancel &&
         a_clx_req->m_status == Req12::StatusT::Indicated &&
         a_orig_req->m_status >= Req12::StatusT::New);

  DEBUG_ONLY(OrderID clReqID = a_clx_req->m_id;)
  assert(clReqID != 0 && a_orig_req->m_id != 0 && clReqID > a_orig_req->m_id);

  DEBUG_ONLY(AOS const *aos = a_clx_req->m_aos;)
  assert(aos != nullptr && aos == a_orig_req->m_aos);
  assert(!(a_orig_req->m_exchOrdID.IsEmpty()));

  // to cancel an order, send DELETE request to:
  // /v1/accounts/{accountId}/orders/{orderId}
  char path[64];
  auto curr = stpcpy(path, "/v1/accounts/");
  curr = stpcpy(curr, m_account_id.c_str());
  curr = stpcpy(curr, "/orders/");
  curr = stpcpy(curr, a_orig_req->m_exchOrdID.ToString());

  //-----------------------------------------------------------------------//
  // Send It Out (No Delays!):                                             //
  //-----------------------------------------------------------------------//
  SeqNum seqNum = (*m_txSN)++;

  LOG_INFO(2,
           "EConnector_H1WS_TDAmeritrade::CancelOrderImpl: Sending cancel for "
           "order {}",
           a_orig_req->m_exchOrdID.ToString())
  DELETE(path);

  //-----------------------------------------------------------------------//
  // Update the "CxlReq":                                                  //
  //-----------------------------------------------------------------------//
  a_clx_req->m_status = Req12::StatusT::New;
  a_clx_req->m_ts_sent = utxx::now_utc();
  a_clx_req->m_seqNum = seqNum;
}

//=========================================================================//
// "ModifyOrder":                                                          //
//=========================================================================//
//=========================================================================//
// External "ModifyOrder" (virtual overriding) method:                     //
//=========================================================================//
bool EConnector_H1WS_TDAmeritrade::ModifyOrder(
    AOS const* a_aos, // Non-NULL
    PriceT a_new_px, bool a_is_aggr, QtyAny a_new_qty,
    utxx::time_val a_ts_md_exch, utxx::time_val a_ts_md_conn,
    utxx::time_val a_ts_md_strat, bool a_batch_send, QtyAny a_new_qty_show,
    QtyAny a_new_qty_min) {
  CHECK_ONLY(
      // Again, BatchSend is not possible, but it is not an error:
      if (utxx::unlikely(a_batch_send)) LOG_WARN(
          3, "EConnector_H1WS_TDAmeritrade::ModifyOrder: BatchSend is not "
             "supported: Ignored")

      // NewQty must be valid as well. Advanced Qty Modes are not supported:
      if (utxx::unlikely
         (!(IsPos   (a_new_qty)                                      &&
           (IsPosInf(a_new_qty_show) || a_new_qty_show == a_new_qty) &&
            IsZero  (a_new_qty_min))))
        throw utxx::badarg_error(
              "EConnector_H1WS_TDAmeritrade::ModifyOrder: Invalid/UnSupported "
              "Qtys: NewQty=",
              QR(a_new_qty), ", NewQtyShow=", QR(a_new_qty_show),
              ", NewQtyMin=", QR(a_new_qty_min));)
  // In any case:
  a_batch_send = false;

  // Invoke                EConnector_OrdMgmt    ::ModifyOrderGen (),
  // which in turn invokes EConnector_H1WS_TDAmeritrade::ModifyOrderImpl()
  //                                               (see below):
  a_new_qty_show = a_new_qty;
  a_new_qty_min = QtyAny::Zero();

  auto orig_req = a_aos->m_lastReq;

  bool success =
      EConnector_OrdMgmt::ModifyOrderGen<QT, QR>( // ProtoEngine  SessMgr
          this, this, const_cast<AOS*>(a_aos), a_new_px, a_is_aggr,
          a_new_qty,  a_ts_md_exch, a_ts_md_conn, a_ts_md_strat, a_batch_send,
          a_new_qty_show, a_new_qty_min);

  auto req = a_aos->m_lastReq;
  assert(orig_req->m_id == req->m_origID);

  // process HTTP response to our sent order
  if ((GetStatusCode() == 200) || (GetStatusCode() == 201)) {
    // sending order succeeded, we need to set the ExchOrdID
    auto loc = GetLocation();
    auto end = loc + strlen(loc);
    auto path_start = strstr(loc, "/orders/");
    assert((path_start != nullptr) && (path_start >= loc));

    auto exch_ord_id_start = path_start + 8;
    ExchOrdID new_exch_ord_id(exch_ord_id_start, int(end - exch_ord_id_start));

    ExchOrdID old_exch_ord_id = orig_req->m_exchOrdID;

    LOG_INFO(2,
             "EConnector_H1WS_TDAmeritrade::ModifyOrder: "
             "Successfully replaced {} by {}",
             old_exch_ord_id.ToString(), new_exch_ord_id.ToString())

    OrderReplaced<QT, QR>
      (this, this,  req->m_id, req->m_origID, a_aos->m_id, new_exch_ord_id,
       old_exch_ord_id, ExchOrdID(), req->m_px, req->GetQty<QT,QR>(),
       utxx::now_utc(), utxx::now_utc());
  } else {
    // sending the cancel failed
    LOG_ERROR(1,
              "EConnector_H1WS_TDAmeritrade::ModifyOrder: Failed "
              "{}: {}",
              GetStatusCode(), GetResponse())
    return false;
  }

  return success;
}

//=========================================================================//
// Internal "ModifyOrderImpl" ("EConnector_OrdMgmt" Call-Back):            //
//=========================================================================//
void EConnector_H1WS_TDAmeritrade::ModifyOrderImpl(
    EConnector_H1WS_TDAmeritrade *DEBUG_ONLY(a_dummy),
    Req12 *DEBUG_ONLY(a_mod_req0),  // NULL
    Req12 *a_mod_req1,              // Non-NULL
    Req12 const *a_orig_req,        // Non-NULL
    bool DEBUG_ONLY(a_batch_send)   // Always false
) {
  //-----------------------------------------------------------------------//
  // Modify Params:                                                        //
  //-----------------------------------------------------------------------//
  assert(a_dummy == this && a_orig_req != nullptr && a_mod_req0 == nullptr &&
         a_mod_req1 != nullptr && a_orig_req != nullptr && !a_batch_send   &&
         a_mod_req1->m_kind == Req12::KindT::Modify &&
         a_mod_req1->m_status == Req12::StatusT::Indicated &&
         a_orig_req->m_status >= Req12::StatusT::New);

  DEBUG_ONLY(OrderID modReqID = a_mod_req1->m_id;)
  assert(modReqID != 0 && a_orig_req->m_id != 0 && modReqID > a_orig_req->m_id);

  AOS const *aos = a_mod_req1->m_aos;
  assert(aos != nullptr && a_orig_req->m_aos == aos);

  PriceT newPx = a_mod_req1->m_px;
  assert(IsFinite(newPx) == (aos->m_orderType != FIX::OrderTypeT::Market));

  QtyN newQty     = a_mod_req1->GetQty<QT,QR>();
  assert(IsPos(newQty));

# ifndef NDEBUG
  QtyN newQtyShow = a_mod_req1->GetQtyShow<QT,QR>();
  QtyN newQtyMin  = a_mod_req1->GetQtyMin <QT,QR>();
# endif
  assert(newQtyShow == newQty && IsZero(newQtyMin));

  //-----------------------------------------------------------------------//
  // Output the msg:                                                       //
  //-----------------------------------------------------------------------//
  char *data = m_H1_out_buf;
  char *chunk = data;
  WriteOutOrder(chunk, aos->m_instr, aos->m_orderType, aos->m_isBuy, newPx,
                newQty, aos->m_timeInForce, aos->m_expireDate);
  CHECK_ONLY(TDH1WS::LogMsg<true>(chunk, "[HTTP] MOD REQ");) // IsSend=true

  //-----------------------------------------------------------------------//
  // Update the "ModReq1":                                                 //
  //-----------------------------------------------------------------------//
  a_mod_req1->m_status = Req12::StatusT::New;
  a_mod_req1->m_ts_sent = utxx::now_utc();
  SeqNum seqNum = (*m_txSN)++;
  a_mod_req1->m_seqNum = seqNum;

  // now send it out
  char path[64];
  auto curr = stpcpy(path, "/v1/accounts/");
  curr = stpcpy(curr, m_account_id.c_str());
  curr = stpcpy(curr, "/orders/");
  curr = stpcpy(curr, a_orig_req->m_exchOrdID.ToString());

  PUT(path);
}

//=========================================================================//
// "CancelAllOrders":                                                      //
//=========================================================================//
void EConnector_H1WS_TDAmeritrade::CancelAllOrders(unsigned long a_strat_hash48,
                                                   SecDefD const *a_instr,
                                                   FIX::SideT a_side,
                                                   char const *a_segm_id) {
  EConnector_OrdMgmt::CancelAllOrdersGen<QT, QR>(this, this, a_strat_hash48,
                                                 a_instr, a_side, a_segm_id);
}
} // End namespace MAQUETTE
