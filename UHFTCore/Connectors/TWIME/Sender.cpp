// vim:ts=2:et
//===========================================================================//
//                     "Connectors/TWIME/TWIME/Sender.cpp":                  //
//       "EConnector_TWIME_FORTS": Generation and Sending of TWIME Msgs      //
//===========================================================================//
// NB:
// (*) All msgs are formed in the Buffer; they are NOT sent out unless "Flush"
//     is invoked explicitly;
// (*) Unlike FIX (which was designed for versatility), in TWIME Client we do
//     NOT separate ProtocolEngine, SessionMgr and the EConnector itself: all
//     of them are implemented by this class:
//
#include "Connectors/TWIME/EConnector_TWIME_FORTS.h"
#include "Connectors/EConnector_OrdMgmt.hpp"
#include "Connectors/TCP_Connector.hpp"
#include "Basis/BaseTypes.hpp"
#include "Protocols/TWIME/Msgs.h"
#include <utxx/compiler_hints.hpp>
#include <utxx/error.hpp>
#include <cstring>
#include <climits>
#include <cassert>
#include <sstream>

namespace MAQUETTE
{
  //=========================================================================//
  // Utility Methods and Session-Level Msgs:                                 //
  //=========================================================================//
  //=========================================================================//
  // "MkMsgHdr":                                                             //
  //=========================================================================//
  template<typename T>
  inline T* EConnector_TWIME_FORTS::MkMsgHdr() const
  {
    // Check for Buffer Over-Flow:
    int after = m_outBuffLen + int(sizeof(T));
    CHECK_ONLY
    (
      if (utxx::unlikely(after > int(sizeof(m_outBuff))))
        throw utxx::runtime_error
              ("EConnector_TWIME_FORTS::MkMsgHdr(",
               TWIME::GetMsgTypeName(T::s_id),   "): OutBuff OverFlow");
    )
    // Ptr to the (typed) msg in OutBuff:
    T* tmsg = reinterpret_cast<T*>(m_outBuff + m_outBuffLen);

    // Fill in the MsgHdr:
    // NB:  BlockLength does NOT include the size of MsgHdr!
    tmsg->m_BlockLength  = sizeof(T) - sizeof(TWIME::MsgHdr);
    tmsg->m_TemplateID   = T::s_id;
    tmsg->m_SchemaID     = TWIME::SchemaID;
    tmsg->m_Version      = TWIME::SchemaVer;

    // Although the msg is not complete yet, advance the OutBuffLen:
    m_outBuffLen  = after;

    // Return the typed ptr:
    return tmsg;
  }

  //=========================================================================//
  // "SendEstablish":                                                        //
  //=========================================================================//
  void EConnector_TWIME_FORTS::SendEstablish()
  {
    TWIME::Establish* tmsg    = MkMsgHdr<TWIME::Establish>();
    tmsg->m_TimeStamp         = TWIME::MkTimeStamp(utxx::now_utc());
    tmsg->m_KeepAliveInterval = m_heartBeatDecl;
    // NB: The actual period   (m_heartBeatMSec) will normally be shorter than
    // the negotiated one (m_heartBeatDecl), by 1 sec, to avoid spurious disc-
    // onnects caused by missed HeartBeat deadlines...
    StrNCpy<false>(tmsg->m_Credentials, m_clientID.data());

    (void) FlushOrders();
    CHECK_ONLY(LogMsg(*tmsg);)
  }

  //=========================================================================//
  // "SendTerminate":                                                        //
  //=========================================================================//
  void EConnector_TWIME_FORTS::SendTerminate()
  {
    TWIME::Terminate* tmsg    = MkMsgHdr<TWIME::Terminate>();
    tmsg->m_TerminationCode   = m_termCode;
    (void) FlushOrders();
    CHECK_ONLY(LogMsg(*tmsg);)
  }

  //=========================================================================//
  // "SendSequence":                                                         //
  //=========================================================================//
  void EConnector_TWIME_FORTS::SendSequence()
  {
    TWIME::Sequence*  tmsg    = MkMsgHdr<TWIME::Sequence>();
    tmsg->m_NextSeqNo         = TWIME::EmptyUInt64;
    (void) FlushOrders();
    CHECK_ONLY(LogMsg(*tmsg);)
  }

  //=========================================================================//
  // "SendReTransmitReq":                                                    //
  //=========================================================================//
  void EConnector_TWIME_FORTS::SendReTransmitReq()
  {
    // If this method was invoked, we indeed have determined a re-transmission
    // range which must be valid (1..MaxReSends msgs):
    assert(0 < m_reSendCountCurr && m_reSendCountCurr <= m_reSendCountTotal);

    SeqNum RxSN = *m_RxSN;

    LOG_INFO(2,
      "EConnector_TWIME_FORTS::SendReTransmitReq: RxSN={}, TotalReTransmit="
      "{}, CurrReTransmit={}", RxSN, m_reSendCountTotal, m_reSendCountCurr)

    TWIME::ReTransmitRequest* tmsg = MkMsgHdr<TWIME::ReTransmitRequest>();
    tmsg->m_TimeStamp              = TWIME::MkTimeStamp(utxx::now_utc());
    tmsg->m_FromSeqNo              = uint64_t(RxSN);
    tmsg->m_Count                  = uint32_t(m_reSendCountCurr);

    (void) FlushOrders();
    CHECK_ONLY(LogMsg(*tmsg);)
  }

  //=========================================================================//
  // "FlushOrders":                                                          //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // (1) Internal implementation:                                            //
  //-------------------------------------------------------------------------//
  utxx::time_val EConnector_TWIME_FORTS::FlushOrdersImpl
    (EConnector_TWIME_FORTS* DEBUG_ONLY(a_dummy))
  const
  {
    assert(a_dummy == this && 0 <= m_outBuffLen);

    CHECK_ONLY
    (
      if (utxx::unlikely(m_outBuffLen > int(sizeof(m_outBuff))))
        throw utxx::runtime_error
              ("EConnector_TWIME_FORTS::FlushOrdersImpl: OutFull OverFlow: "
               "CurrSize=", m_outBuffLen, ", MaxSize=", sizeof(m_outBuff));
    )
    if (utxx::unlikely(m_outBuffLen == 0))
      return utxx::time_val();   // Nothing to send

    // Generic Case: Invoke the implementation provided by "TCP_Connector":
    //
    utxx::time_val sendTS = SendImpl(m_outBuff, m_outBuffLen);

    // XXX: "sendTS" may still be empty if (part of) the data were once again
    // buffered in the Reactor's internal buffer, rather than sent out direct-
    // ly, but that is extremely unlikely, and not harmful in any case... And
    // unless there was an exception in "SendImpl" (again, extremely unlikely,
    // which would mean that the Reactor's internal buffer is also full!), the
    // whole data are now sent out, or buffered in the Reactor for sending on-
    // ce the socket becomes writable again: So reset the buffer:
    //
    m_outBuffLen = 0;
    return sendTS;
  }

  //-------------------------------------------------------------------------//
  // (2) The externally-visible (virtual) method:                            //
  //-------------------------------------------------------------------------//
  utxx::time_val EConnector_TWIME_FORTS::FlushOrders()
  {
    // Invoke the generic implementation (which in turn calls the above 1-arg
    // method): NB: Here both the ProtoEng and SessMgr are in fact this class
    // (EConnector_TWIME_FORTS):
    return FlushOrdersGen
          <EConnector_TWIME_FORTS, EConnector_TWIME_FORTS>(this, this);
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
  AOS const* EConnector_TWIME_FORTS::NewOrder
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
    //-----------------------------------------------------------------------//
    // Checks:                                                               //
    //-----------------------------------------------------------------------//
    CHECK_ONLY
    (
      assert(a_strategy != nullptr);

      // Only Limit Orders are supported, so the Px must be finite:
      if (utxx::unlikely
         (a_ord_type != FIX::OrderTypeT::Limit) || !IsFinite(a_px))
        throw utxx::badarg_error
              ("EConnector_TWIME_FORTS::NewOrder: Must be a Limit Order");

      // Check the Qty. Advanced Qty Modes are not supported (XXX QtyMin=0 means
      // no minimum):
      if (utxx::unlikely
         (! (IsPos(a_qty) && (IsPosInf(a_qty_show) || a_qty_show == a_qty) &&
            IsZero(a_qty_min))))
        throw utxx::badarg_error
              ("EConnector_TWIME_FORTS::NewOrder: Invalid/UnSupported Qtys"
               ": Qty=", QR(a_qty), ", QtyShow=", QR(a_qty_show),  ", QtyMin=",
               QR(a_qty_min));

      // Only the following TimeInForce vals are supported (as we have Limit
      // orders only):
      if (utxx::unlikely
         (a_time_in_force != FIX::TimeInForceT::Day           &&
          a_time_in_force != FIX::TimeInForceT::ImmedOrCancel &&
          a_time_in_force != FIX::TimeInForceT::FillOrKill))
        throw utxx::badarg_error
              ("EConnector_TWIME_FORTS::NewOrder: UnSupported TimeInForce");

      // Pegged Orders are NOT supported in TWIME of course:
      if (utxx::unlikely(IsFinite(a_peg_offset)))
        throw utxx::badarg_error
              ("EConnector_TWIME_FORTS::NewOrder: Pegged Orders: Not "
               "supported");
    )
    //-----------------------------------------------------------------------//
    // Imvoke the Generic Impl:                                              //
    //-----------------------------------------------------------------------//
    // Invoke                EConnector_OrdMgmt    ::NewOrderGen (),
    // which in turn invokes EConnector_TWIME_FORTS::NewOrderImpl()
    //                       (see below):
    return EConnector_OrdMgmt::NewOrderGen<QT,QR>
    (
      // ProtoEng    SessMgr
      this,          this,
      a_strategy,    a_instr,       a_ord_type,   a_is_buy,
      a_px,          a_is_aggr,     a_qty,        a_ts_md_exch,
      a_ts_md_conn,  a_ts_md_strat, a_batch_send, a_time_in_force,
      a_expire_date, a_qty_show,    a_qty_min,    a_peg_side,
      a_peg_offset
    );
  }

  //=========================================================================//
  // Internal "NewOrderImpl" ("EConnector_OrdMgmt" Call-Back):               //
  //=========================================================================//
  void EConnector_TWIME_FORTS::NewOrderImpl
  (
    EConnector_TWIME_FORTS* DEBUG_ONLY(a_dummy),
    Req12*                  a_new_req,
    bool                    a_batch_send  // To send multiple msgs at once
  )
  {
    //----------------------------------------------------------------------//
    // Params:                                                              //
    //----------------------------------------------------------------------//
    assert(a_dummy == this     && a_new_req != nullptr     &&
           a_new_req->m_kind   == Req12::KindT::New        &&
           a_new_req->m_status == Req12::StatusT::Indicated);

    OrderID                 newReqID    = a_new_req->m_id;
    assert(newReqID  != 0);
    AOS const*              aos         = a_new_req->m_aos;
    assert(aos != nullptr   &&   aos->m_instr != nullptr);

    Strategy*               strategy    = aos->m_strategy;
    assert(strategy != nullptr);
    SecDefD const*          instr       = aos->m_instr;
    assert(instr    != nullptr);

    // Only Limit Orders are supported in TWIME:
    DEBUG_ONLY(FIX::OrderTypeT ordType  = aos->m_orderType;)
    assert(ordType == FIX::OrderTypeT::Limit);

    FIX::TimeInForceT       timeInForce = aos->m_timeInForce;
    bool                    isBuy       = aos->m_isBuy;
    PriceT                  px          = a_new_req->m_px;
    assert(IsFinite(px));

    QtyN  qty     = a_new_req->GetQty    <QT,QR>();
#   ifndef NDEBUG
    QtyN  qtyShow = a_new_req->GetQtyShow<QT,QR>();
    QtyN  qtyMin  = a_new_req->GetQtyMin <QT,QR>();
#   endif
    assert(IsPos(qty) && !IsNeg(qtyShow) && qtyShow <= qty && !IsNeg(qtyMin) &&
           qtyMin <= qty);

    //-----------------------------------------------------------------------//
    // Fill in a "NewOrderSingle" Msg:                                       //
    //-----------------------------------------------------------------------//
    TWIME::NewOrderSingle* tmsg = MkMsgHdr<TWIME::NewOrderSingle>();

    tmsg->m_ClOrdID     = newReqID;

    // XXX: Because GTD Orders are not supported at the moment, ExpireDate is
    // always Empty:
    tmsg->m_ExpireDate  = TWIME::EmptyTimeStamp;
    tmsg->m_Price       = TWIME::Decimal5T(px);

    // In FORTS, SecIDs are static, and must fit in a 32-bit format. NB: the
    // SecIDs coming from the corresp SecDef MUST BE MODIFIED:
    tmsg->m_SecurityID  = int32_t(instr->m_SecID);

    // XXX: "ClOrdLinkID" will be a 32-bit Stategy Hash, useful eg in MassCancel
    // Reqs:
    tmsg->m_ClOrdLinkID = int32_t (strategy->GetHash48() & Mask32);
    tmsg->m_OrderQty    = uint32_t(QR(qty));
    tmsg->m_TimeInForce =
      TWIME::TimeInForceE::type(char(timeInForce) - '0');
    tmsg->m_Side        = isBuy ? TWIME::SideE::Buy : TWIME::SideE::Sell;

    // XXX: "CheckLimit" is for Options only; in any case, we turn if OFF (we
    // must know what we are doing):
    tmsg->m_CheckLimit  = TWIME::CheckLimitE::DontCheck;

    // Finally, the Account:
    StrNCpy<false>(tmsg->m_Account, m_account.data());

    //-----------------------------------------------------------------------//
    // Possibly Send It Out:                                                 //
    //-----------------------------------------------------------------------//
    // For compatibility with "EConnector_OrdMgmt", we formally need to incre-
    // ment the TxSN, even though it is not used in TWIME at all:
    SeqNum txSN = (*m_txSN)++;

    utxx::time_val sendTS = !a_batch_send ? FlushOrders() : utxx::time_val();
    CHECK_ONLY(LogMsg(*tmsg);)

    //-----------------------------------------------------------------------//
    // Update the "NewReq":                                                  //
    //-----------------------------------------------------------------------//
    a_new_req->m_status  = Req12::StatusT::New;
    a_new_req->m_ts_sent = sendTS;
    a_new_req->m_seqNum  = txSN;
  }

  //=========================================================================//
  // "CancelOrder":                                                          //
  //=========================================================================//
  //=========================================================================//
  // External "CancelOrder" (virtual overriding) method:                     //
  //=========================================================================//
  bool EConnector_TWIME_FORTS::CancelOrder
  (
    AOS const*          a_aos,          // Non-NULL
    utxx::time_val      a_ts_md_exch,
    utxx::time_val      a_ts_md_conn,
    utxx::time_val      a_ts_md_strat,
    bool                a_batch_send
  )
  {
    // Invoke                EConnector_OrdMgmt    ::CancelOrderGen (),
    // which in turn invokes EConnector_TWIME_FORTS::CancelOrderImpl()
    //                                               (see below):
    return EConnector_OrdMgmt::CancelOrderGen<QT,QR>
    ( // ProtoEng   SessMgr
      this,         this,         const_cast<AOS*>(a_aos),
      a_ts_md_exch, a_ts_md_conn, a_ts_md_strat,   a_batch_send
    );
  }

  //=========================================================================//
  // Internal "CancelOrderImpl" ("EConnector_OrdMgmt" Call-Back):            //
  //=========================================================================//
  void EConnector_TWIME_FORTS::CancelOrderImpl
  (
    EConnector_TWIME_FORTS* DEBUG_ONLY  (a_dummy),
    Req12*                  a_clx_req,
    Req12 const*            a_orig_req,
    bool                    a_batch_send      // To send multiple msgs at once
  )
  {
    //-----------------------------------------------------------------------//
    // Checks:                                                               //
    //-----------------------------------------------------------------------//
    assert(a_dummy == this && a_clx_req != nullptr && a_orig_req != nullptr &&
           a_clx_req ->m_kind    == Req12::KindT::Cancel      &&
           a_clx_req ->m_status  == Req12::StatusT::Indicated &&
           a_orig_req->m_status  >= Req12::StatusT::New);

    //-----------------------------------------------------------------------//
    // Fill in an "OrderCancelRequest" Msg:                                  //
    //-----------------------------------------------------------------------//
    TWIME::OrderCancelRequest* tmsg = MkMsgHdr<TWIME::OrderCancelRequest>();

    // ClOrdID of the Cancellation Request itself:
    OrderID clOrdID  = a_clx_req->m_id;
    assert( clOrdID != 0 && a_orig_req->m_id != 0 &&
            clOrdID >       a_orig_req->m_id);
    tmsg->m_ClOrdID  = clOrdID;

    // But the target to Cancel is identified NOT by its ClOrdID; rather, we
    // must use the Exchange-assigned OrderID. It must be available since we
    // disallow PipeLining:
    CHECK_ONLY
    (
      if (utxx::unlikely
         (a_orig_req->m_exchOrdID.IsEmpty() ||
         !a_orig_req->m_exchOrdID.IsNum()))
        throw utxx::badarg_error
              ("EConnector_TWIME_FORTS::CancelOrderImpl: OrigReqID=",
               a_orig_req->m_id,
               ": Cannot Cancel: Missing or Invalid ExchOrdID");
    )
    // If OK:
    tmsg->m_OrderID = int64_t(a_orig_req->m_exchOrdID.GetOrderID());

    // Also need an Account:
    StrNCpy<false>(tmsg->m_Account, m_account.data());

    // For compatibility with "EConnector_OrdMgmt", we formally need to incre-
    // ment the TxSN, even though it is not used in TWIME at all:
    SeqNum txSN = (*m_txSN)++;

    //-----------------------------------------------------------------------//
    // Possibly Send It Out:                                                 //
    //-----------------------------------------------------------------------//
    utxx::time_val sendTS = !a_batch_send ? FlushOrders() : utxx::time_val();
    CHECK_ONLY(LogMsg(*tmsg);)

    //-----------------------------------------------------------------------//
    // Update the "CxlReq":                                                  //
    //-----------------------------------------------------------------------//
    a_clx_req->m_status  = Req12::StatusT::New;
    a_clx_req->m_ts_sent = sendTS;
    a_clx_req->m_seqNum  = txSN;
  }

  //=========================================================================//
  // "ModifyOrder":                                                          //
  //=========================================================================//
  //=========================================================================//
  // External "ModifyOrder" (virtual overriding) method:                     //
  //=========================================================================//
  bool EConnector_TWIME_FORTS::ModifyOrder
  (
    AOS const*          a_aos,          // Non-NULL
    PriceT              a_new_px,
    bool                a_is_aggr,
    QtyAny              a_new_qty,
    utxx::time_val      a_ts_md_exch,
    utxx::time_val      a_ts_md_conn,
    utxx::time_val      a_ts_md_strat,
    bool                a_batch_send,
    QtyAny              a_new_qty_show,
    QtyAny              a_new_qty_min
  )
  {
    // Checks:
    CHECK_ONLY
    (
      // NewPx must be valid (as it is a Limit Order):
      if (utxx::unlikely(!IsFinite(a_new_px)))
        throw utxx::badarg_error
              ("EConnector_TWIME_FORTS::ModifyOrder: Invalid NewPx");

      // NewQty must be valid as well. Advanced Qty Modes are not supported:
      if (utxx::unlikely
         (!(IsPos   (a_new_qty)                                      &&
           (IsPosInf(a_new_qty_show) || a_new_qty_show == a_new_qty) &&
            IsZero  (a_new_qty_min))))
        throw utxx::badarg_error
              ("EConnector_TWIME_FORTS::ModifyOrder: Invalid/UnSupported "
               "Qtys: NewQty=",    QR(a_new_qty),   ", NewQtyShow=",
               QR(a_new_qty_show), ", NewQtyMin=",  QR(a_new_qty_min));
    )
    // Invoke                EConnector_OrdMgmt    ::ModifyOrderGen (),
    // which in turn invokes EConnector_TWIME_FORTS::ModifyOrderImpl()
    //                                               (see below):
    return EConnector_OrdMgmt::ModifyOrderGen<QT,QR>
    ( // ProtoEngine  SessMgr
      this,           this,           const_cast<AOS*>(a_aos),
      a_new_px,       a_is_aggr,      a_new_qty,       a_ts_md_exch,
      a_ts_md_conn,   a_ts_md_strat,  a_batch_send,    a_new_qty_show,
      a_new_qty_min
    );
  }

  //=========================================================================//
  // Internal "ModifyOrderImpl" ("EConnector_OrdMgmt" Call-Back):            //
  //=========================================================================//
  void EConnector_TWIME_FORTS::ModifyOrderImpl
  (
    EConnector_TWIME_FORTS* DEBUG_ONLY(a_dummy),
    Req12*                  DEBUG_ONLY(a_mod_req0), // NULL in TWIME
    Req12*                  a_mod_req1,             // Non-NULL
    Req12 const*            a_orig_req,
    bool                    a_batch_send
  )
  {
    //-----------------------------------------------------------------------//
    // Checks and Modify Params:                                             //
    //-----------------------------------------------------------------------//
    assert(a_dummy    == this    && a_mod_req0 == nullptr     &&
           a_mod_req1 != nullptr && a_orig_req != nullptr     &&
           a_mod_req1->m_kind    == Req12::KindT::Modify      &&
           a_mod_req1->m_status  == Req12::StatusT::Indicated &&
           a_orig_req->m_status  >= Req12::StatusT::New);

    OrderID                 modReqID = a_mod_req1->m_id;
    assert(modReqID != 0 && a_orig_req->m_id != 0 &&
           modReqID >       a_orig_req->m_id);
    AOS const*              aos      = a_mod_req1->m_aos;
    assert(aos != nullptr);
    PriceT                  newPx    = a_mod_req1->m_px;
    assert(IsFinite(newPx));

    QtyN newQty     = a_mod_req1->GetQty    <QT,QR>();
    assert(IsPos(newQty));
#   ifndef NDEBUG
    QtyN newQtyShow = a_mod_req1->GetQtyShow<QT,QR>();
    QtyN newQtyMin  = a_mod_req1->GetQtyMin <QT,QR>();
#   endif
    assert(newQtyShow == newQty && IsZero(newQtyMin));

    //-----------------------------------------------------------------------//
    // Fill in an "OrderReplaceRequest" Msg:                                 //
    //-----------------------------------------------------------------------//
    TWIME::OrderReplaceRequest* tmsg = MkMsgHdr<TWIME::OrderReplaceRequest>();

    // ClOrdID of the "Modify" Req itself:
    tmsg->m_ClOrdID     = modReqID;

    // But the target to Modify is identified NOT by its ClOrdID; rather, we
    // must use the Exchange-assigned OrderID. Again, it mus be available as
    // we have disallowed PipeLining:
    CHECK_ONLY
    (
      if (utxx::unlikely
         (a_orig_req->m_exchOrdID.IsEmpty() ||
         !a_orig_req->m_exchOrdID.IsNum()))
        throw utxx::badarg_error
              ("EConnector_TWIME_FORTS::ReplaceOrder: OrigReqID=",
               a_orig_req->m_id,
               ": Cannot Cancel: Missing or Invalid ExchOrdID");
    )
    // If OK:
    tmsg->m_OrderID     = int64_t (a_orig_req->m_exchOrdID.GetOrderID());
    tmsg->m_Price       = TWIME::Decimal5T(newPx);
    tmsg->m_OrderQty    = uint32_t(QR(newQty));

    // XXX: "ClOrdLinkID" will be a 32-bit Stategy Hash, useful eg in MassCancel
    // Reqs. It remains unchanged:
    tmsg->m_ClOrdLinkID = int32_t (aos->m_strategy->GetHash48() & Mask32);

    // Modificaton Mode: The difference between the modes is subtle.  For the
    // moment, we generally allow any Qty updates (though the new Qty may be
    // same as the old one):
    tmsg->m_Mode        = TWIME::ModeE::ChangeOrderQty;

    // XXX: "CheckLimit" is for Options only; in any case, we turn if OFF (we
    // must know what we are doing):
    tmsg->m_CheckLimit  = TWIME::CheckLimitE::DontCheck;

    // Finally, the Account:
    StrNCpy<false>(tmsg->m_Account, m_account.data());

    // For compatibility with "EConnector_OrdMgmt", we formally need to incre-
    // ment the TxSN, even though it is not used in TWIME at all:
    SeqNum txSN = (*m_txSN)++;

    //-----------------------------------------------------------------------//
    // Possibly Send It Out:                                                 //
    //-----------------------------------------------------------------------//
    utxx::time_val sendTS = !a_batch_send ? FlushOrders() : utxx::time_val();
    CHECK_ONLY(LogMsg(*tmsg);)

    //-----------------------------------------------------------------------//
    // Update the "ModReq1":                                                 //
    //-----------------------------------------------------------------------//
    a_mod_req1->m_status  = Req12::StatusT::New;
    a_mod_req1->m_ts_sent = sendTS;
    a_mod_req1->m_seqNum  = txSN;
  }

  //=========================================================================//
  // "CancelAllOrders":                                                      //
  //=========================================================================//
  void EConnector_TWIME_FORTS::CancelAllOrders
  (
    unsigned long  a_strat_hash48,
    SecDefD const* a_instr,
    FIX::SideT     a_side,
    char const*    a_segm_id
  )
  {
    // NB: Unless we are cancelling orders for a particular Strategy, TWIME
    // cannot do it for BOTH Futures and Options at once,  so in such cases
    // (when there is no SegmID), we will have to re-invoke this method 2ce:
    //
    if (utxx::unlikely(a_strat_hash48 == 0 && a_segm_id == nullptr))
    {
      CancelAllOrders (0, a_instr, a_side, "F");
      CancelAllOrders (0, a_instr, a_side, "O");
      return;
    }

    //-----------------------------------------------------------------------//
    // Generic Case: Fill in a "OrderMassCancelRequest" Msg:                 //
    //-----------------------------------------------------------------------//
    TWIME::OrderMassCancelRequest* tmsg =
      MkMsgHdr<TWIME::OrderMassCancelRequest>();

    // ClOrdID of the "MassCancel" Req itself: Because it is NOT recorded in
    // AOSes / Req12s, we can choose a random value here:
    tmsg->m_ClOrdID = MkTmpReqID(utxx::now_utc());

    // Filtering by Strategy Hash (reduced to an "int"). This filter is ortho-
    // gonal to all others:
    if (a_strat_hash48 != 0)
    {
      CHECK_ONLY
      (
        if (utxx::unlikely
           (a_instr   != nullptr || a_side != FIX::SideT::UNDEFINED ||
            a_segm_id != nullptr))
          throw utxx::badarg_error
                ("EConnector_TWIME_FORTS::CancelAllOrders: StratHash Filter "
                 "is incompatible with any others");
      )
      // If OK:
      tmsg->m_ClOrdLinkID  = int32_t(a_strat_hash48 & Mask32);

      // Other flds will be empty in this case:
      tmsg->m_SecurityID   = TWIME::EmptyInt32;
      tmsg->m_SecurityType = TWIME::SecurityTypeE::Future; // Does not matter!
      tmsg->m_Side         = TWIME::SideE::AllOrders;
    }
    else
    {
      // No filtering by StratHash: NB: The value to be stored here is 0, NOT
      // "EmptyInt32":
      tmsg->m_ClOrdLinkID  = 0;

      // Filtering by SecID. Again, don't forget to modify it:
      tmsg->m_SecurityID =
        (a_instr != nullptr)
        ? int32_t(a_instr->m_SecID)
        : TWIME::EmptyInt32;

      // Filtering by Security Type. XXX: It seems that "a_segm_id" is in this
      // case MANDATORY, because there is no default val (standing for Both Fu-
      // tures and Options):
      CHECK_ONLY
      (
        if (utxx::unlikely
           (a_segm_id == nullptr ||
           (a_segm_id[0] != 'F'  && a_segm_id[0] != 'O')))
          throw utxx::badarg_error
                ("EConnector_TWIME_FORTS::CancelAllOrders: Empty or Invalid "
                 "Segment");
      )
      tmsg->m_SecurityType =
        (a_segm_id[0] == 'F')
        ? TWIME::SecurityTypeE::Future
        : TWIME::SecurityTypeE::Option;

      // Side:
      tmsg->m_Side         =
        (a_side == FIX::SideT::Buy)
        ? TWIME::SideE::Buy
        :
        (a_side == FIX::SideT::Sell)
        ? TWIME::SideE::Sell
        : TWIME::SideE::AllOrders;
    }

    // NB: Filetering by Account is not implemented, so install a Catch-All
    // RegExp:
    StrNCpy<false>(tmsg->m_Account, m_account.data());

    // And filtering by Symbol RegExps is not implemented either: Use 0s:
    memset(tmsg->m_SecurityGroup, '\0', sizeof(tmsg->m_SecurityGroup));

    //-----------------------------------------------------------------------//
    // This msg is never buffered -- send it out immediately:                //
    //-----------------------------------------------------------------------//
    (void) FlushOrders();
    CHECK_ONLY(LogMsg(*tmsg);)
  }
} // End namespace MAQUETTE
