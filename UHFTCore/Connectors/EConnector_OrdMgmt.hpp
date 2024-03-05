// vim:ts=2:et
//===========================================================================//
//                    "Connectors/EConnector_OrdMgmt.hpp":                   //
//                 Templated Methods Used by Derived Classes:                //
//                       Client-Side API and State Mgmt                      //
//===========================================================================//
#pragma  once
#include "Connectors/EConnector_OrdMgmt_Impl.hpp"
#include "Basis/QtyConvs.hpp"

namespace MAQUETTE
{
  //=========================================================================//
  // "NewOrderGen":                                                          //
  //=========================================================================//
  template <QtyTypeT QT, typename QR, typename ProtoEng, typename SessData>
  inline AOS* EConnector_OrdMgmt::NewOrderGen
  (
    ProtoEng*          a_proto_eng,
    SessData*          a_sess,
    Strategy*          a_strategy,
    SecDefD const&     a_instr,
    FIX::OrderTypeT    a_ord_type,
    bool               a_is_buy,
    PriceT             a_px,
    bool               a_is_aggr,      // Intended to be Aggressive?
    QtyAny             a_qty,          // Full intended Qty
    utxx::time_val     a_ts_md_exch,
    utxx::time_val     a_ts_md_conn,
    utxx::time_val     a_ts_md_strat,
    bool               a_batch_send,   // To send several msgs at once
    FIX::TimeInForceT  a_time_in_force,
    int                a_expire_date,
    QtyAny             a_qty_show,     // Intended Qty Show
    QtyAny             a_qty_min,      // Intended Qty Min
    bool               a_peg_side,     // True: This side; False: Opposite side
    double             a_peg_offset    // Can be <=> 0.0
  )
  {
    //-----------------------------------------------------------------------//
    // Verify the Args:                                                      //
    //-----------------------------------------------------------------------//
    assert(a_proto_eng != nullptr && a_sess != nullptr);

    // Make a realistic BatchSend:
    a_batch_send &= ProtoEng::HasBatchSend;

    utxx::time_val createTS  = utxx::now_utc();

    // XXX: We do NOT check that the OMC "IsActive"; even if not, we will store
    // an Indication! But it must be Enabled:
    CHECK_ONLY
    (
      if (utxx::unlikely(!m_isEnabled))
        throw utxx::badarg_error
              ("EConnector_OrdMgmt::NewOrderGen: ", m_name,
               ": OMC Not Enabled");

      // NB: In some rare cases, Strategy may not be required, but we enforce
      // it being non-NULL in any case:
      if (utxx::unlikely(a_strategy == nullptr))
        throw utxx::badarg_error
              ("EConnector_OrdMgmt::NewOrderGen: ", m_name,
               ": Strategy must not be NULL");

      // For a Limit or Stop Order, Px must always be Finite;  for a Mkt Order,
      // it must be otherwise:
      if (utxx::unlikely
         (((a_ord_type == FIX::OrderTypeT::Limit  ||
            a_ord_type == FIX::OrderTypeT::Stop)  && !IsFinite(a_px)) ||
          ( a_ord_type == FIX::OrderTypeT::Market &&  IsFinite(a_px)) ))
        throw utxx::badarg_error
              ("EConnector_OrdMgmt::NewOrderGen: ", m_name,
               ": Inconsistency: OrderType=", char(a_ord_type), ", Px=", a_px);
    )
    //-----------------------------------------------------------------------//
    // Adjust the Px:                                                        //
    //-----------------------------------------------------------------------//
    // Round "a_px" to a multiple of PxStep (so the CallER is not obliged to do
    // it on its side):
    a_px = Round(a_px, a_instr.m_PxStep);

    //-----------------------------------------------------------------------//
    // Convert, Normalise and Verify the Qtys:                               //
    //-----------------------------------------------------------------------//
    // Convert the user-level Qtys into the OMC-specific ones. XXX:
    // (*) This is rather expensive!
    // (*) QtyA <-> QtyB conversions require a Px, for which only "a_px" is av-
    //     ailable at this point (if at all), so in those cases the result may
    //     be quite inaccurate, or an exception could be raised:
    Qty<QT,QR> qty     = a_qty     .ConvQty<QT,QR>(a_instr, a_px);
    Qty<QT,QR> qtyShow = a_qty_show.ConvQty<QT,QR>(a_instr, a_px);
    Qty<QT,QR> qtyMin  = a_qty_min .ConvQty<QT,QR>(a_instr, a_px);

    // Normalise them:
    qtyShow.MinWith(qty);
    qtyMin .MinWith(qty);

    assert(!IsNeg(qtyShow) && (IsPosInf(qty) || qtyShow <= qty));
    bool    isIceberg = (qtyShow < qty);

    // Now check them:
    CHECK_ONLY
    (
      if (utxx::unlikely
        (!IsPos(qty) || IsNeg(qtyShow) || IsNeg(qtyMin) || qtyShow > qty ||
         qtyMin > qty))
        throw utxx::badarg_error
              ("EConnector_OrdMgmt::NewOrderGen: ", m_name,
               ": Invalid Params: Qty=", QR(qty),   ", QtyShow=", QR(qtyShow),
               ", QtyMin=", QR(qtyMin));
    )
    // Risk Checks: An exception will be thrown if the check below fails:
    if (m_riskMgr != nullptr)
      // IsReal=true, OldPx=0 (not NaN):
      m_riskMgr->OnOrder<true>
        (this, a_instr, a_is_buy, QT, a_px, double(qty), PriceT(0.0), 0.0,
         a_ts_md_strat);

    //-----------------------------------------------------------------------//
    // Create the new "AOS" and "Req12":                                     //
    //-----------------------------------------------------------------------//
    // The counters used by the Application and Session Layers must be same:
    assert(m_TxSN  == a_sess->m_txSN && m_RxSN == a_sess->m_rxSN);

    DEBUG_ONLY(OrderID currReqN = *m_ReqN;)
    DEBUG_ONLY(OrderID currOrdN = *m_OrdN;)

    // Create a new AOS; this increments *m_OrdN:
    AOS* aos = NewAOS
    (
      a_strategy,      &a_instr,   a_is_buy,  a_ord_type,  isIceberg,
      a_time_in_force, a_expire_date
    );
    assert(aos != nullptr && *m_OrdN == currOrdN + 1);

    // Create the Req12 with already-recorded time stamps; AttachToAOS=true:
    Req12* req = NewReq12<QT, QR, true>
    (
      aos,           0,             Req12::KindT::New, a_px,
      a_is_aggr,     qty,           qtyShow,           qtyMin,
      a_peg_side,    a_peg_offset,  a_ts_md_exch,      a_ts_md_conn,
      a_ts_md_strat, createTS
    );
    assert(req != nullptr && req->m_id == currReqN     &&
           req->m_status  == Req12::StatusT::Indicated &&
           *m_ReqN == currReqN + 1);

    //-----------------------------------------------------------------------//
    // Try to send it out:                                                   //
    //-----------------------------------------------------------------------//
    // NB: IsNew=true, since "req" was just created above:
    //
    (void) TrySendIndications<ProtoEng, SessData>
      (a_proto_eng, a_sess, nullptr, req, nullptr, true, a_batch_send,
       createTS);

    // All Done:
    return aos;
  }

  //=========================================================================//
  // "CancelOrderGen":                                                       //
  //=========================================================================//
  template<QtyTypeT QT, typename QR, typename ProtoEng, typename SessData>
  inline bool EConnector_OrdMgmt::CancelOrderGen
  (
    ProtoEng*       a_proto_eng,
    SessData*       a_sess,
    AOS*            a_aos,          // Non-NULL
    utxx::time_val  a_ts_md_exch,
    utxx::time_val  a_ts_md_conn,
    utxx::time_val  a_ts_md_strat,
    bool            a_batch_send    // To send multiple msgs at once
  )
  {
    //-----------------------------------------------------------------------//
    // Checks:                                                               //
    //-----------------------------------------------------------------------//
    assert(a_proto_eng != nullptr && a_sess != nullptr  && a_aos != nullptr &&
           m_hasAtomicModify == ProtoEng::HasAtomicModify);

    // Make a realistic BatchSend:
    a_batch_send &= ProtoEng::HasBatchSend;

    // XXX: We do NOT check that the OMC "IsActive"; even if not, we will store
    // an Indication. But it must be Enabled:
    CHECK_ONLY
    (
      if (utxx::unlikely(!m_isEnabled))
        throw utxx::badarg_error
              ("EConnector_OrdMgmt::CancelOrderGen: ", m_name,
               ": OMC Not Enabled");
    )
    // NB: If the AOS is already Inactive or CxlPending,     do not do anything
    // (just return False), UNLESS the "OrigReq" is a "ModLegN", which may need
    // to be Cancelled even in that case. But we can only apply this cond below
    // when we determine the "OrigReq":

    // Static and Dynamic AtomicModify flags must match:
    CHECK_ONLY(constexpr bool IsTandem = !ProtoEng::HasAtomicModify;)
    assert(m_hasAtomicModify == !IsTandem);

    //-----------------------------------------------------------------------//
    // OrigReq and CxlReq:                                                   //
    //-----------------------------------------------------------------------//
    Req12* origReq = nullptr;
    Req12* clxReq  = nullptr;

    // The following flag tells whether "clxReq" is a new Indication created
    // right here, or we update an existing one:
    bool   isNew   = false;

    // The counters used by the Application and Session Layers must be same:
    assert(m_TxSN == a_sess->m_txSN && m_RxSN == a_sess->m_rxSN);
    DEBUG_ONLY(OrderID currReqN = *m_ReqN;)

    // New TimeStamps:
    utxx::time_val createTS = utxx::now_utc();

    //-----------------------------------------------------------------------//
    // First of all, check if there are any Indications already:             //
    //-----------------------------------------------------------------------//
    Req12* lastReq =  a_aos->m_lastReq;
    assert(lastReq != nullptr);

    if (lastReq->m_status == Req12::StatusT::Indicated)
    {
      //---------------------------------------------------------------------//
      // Yes, at least 1 Indication: Over-write existing Req(s):             //
      //---------------------------------------------------------------------//
      // NB: "isNew" remains False:
      //
      Req12* preLReq = lastReq->m_prev;

      if (preLReq != nullptr && preLReq->m_status == Req12::StatusT::Indicated)
      {
        //-------------------------------------------------------------------//
        // 2 Indications:                                                    //
        //-------------------------------------------------------------------//
        // Then it can only be a "ModLegC", and lastReq is "ModLegN". Thus, we
        // are  in the Tandem mode. The preceding Req,  if any, must NOT be an
        // Indication:
        assert(IsTandem                                 &&
               lastReq->m_kind == Req12::KindT::ModLegN &&
               preLReq->m_kind == Req12::KindT::ModLegC &&
              (preLReq->m_prev == nullptr ||
               preLReq->m_prev->m_status  != Req12::StatusT::Indicated));

        // In this case, the Indications will just be modified in-place:
        // "ModLegC" will become a full-fledged "Cancel" (below):
        clxReq  = preLReq;
        origReq = GetOrigReq(clxReq);

        // And "ModLegN" will be marked as "Failed" in-place, w/o affecting any
        // of its TimeStamps.    XXX: We will not invoke the Strategy Call-Back
        // ("OnOrderError")   in this case, because the Order will be Cancelled
        // anyway (and the corresp Call-Back invoked):
        //
        lastReq->m_status = Req12::StatusT::Failed;
      }
      else
      {
        //-------------------------------------------------------------------//
        // Exactly 1 Indication:                                             //
        //-------------------------------------------------------------------//
        // NB:
        // (*) It can only be a "New" (in any mode), "ModLegN" (with the corresp
        //     "ModLegC" already sent out, in Tandem mode), or "Modify"
        //     (in !Tandem mode);
        // (*) "Cancel" is impossible: the latter would result in the whole AOS
        //     marked as "CxlPending" (this "Cancel" was only Indicated,  ie it
        //     could nor have Failed yet, removing the "CxlPending" mark),  and
        //     we have already treated the "CxlPending" case above:
        //
        assert(lastReq->m_kind == Req12::KindT::New                   ||
              (lastReq->m_kind == Req12::KindT::ModLegN &&  IsTandem) ||
              (lastReq->m_kind == Req12::KindT::Modify  && !IsTandem));

        if (utxx::unlikely(lastReq->m_kind == Req12::KindT::New       ||
                           lastReq->m_kind == Req12::KindT::ModLegN))
        {
          // A "New" or "ModLegN" is cancelled in-place w/o sending out any-
          // thing: This is a SYNCHRONOUS Cancel NOT originating from an Ex-
          // change event: so there is no "clxReq" or related TimeStamps:
          //
          OrderCancelledImpl<QT, QR, ProtoEng, SessData>
            (a_proto_eng,      a_sess,  a_aos, nullptr, lastReq, false,
             utxx::time_val(), utxx::now_utc());

          // Nothing else to do in this case:
          return true;
        }
        else
        {
          // This is a "Modify". Similar to "ModLegC" above, replace it with a
          // "Cancel":
          assert(lastReq->m_kind == Req12::KindT::Modify);
          clxReq  = lastReq;
          origReq = GetOrigReq(clxReq);
        }
      }
      //---------------------------------------------------------------------//
      // So: Set the params on "CxlReq":                                     //
      //---------------------------------------------------------------------//
      // XXX: We have to override "const" fld properties:
      assert(clxReq != nullptr);

      const_cast<Req12::KindT&>  (clxReq->m_kind)        = Req12::KindT::Cancel;
      const_cast<PriceT&>        (clxReq->m_px)          = PriceT();     // NaN
      const_cast<QtyU&>          (clxReq->m_qty)         = QtyU();       // 0
      const_cast<QtyU&>          (clxReq->m_qtyShow)     = QtyU();       // 0
      const_cast<QtyU&>          (clxReq->m_qtyMin)      = QtyU();       // 0
      clxReq->m_leavesQty                                = QtyU();       // 0
      const_cast<utxx::time_val&>(clxReq->m_ts_md_exch)  = a_ts_md_exch;
      const_cast<utxx::time_val&>(clxReq->m_ts_md_conn)  = a_ts_md_conn;
      const_cast<utxx::time_val&>(clxReq->m_ts_md_strat) = a_ts_md_strat;
      const_cast<utxx::time_val&>(clxReq->m_ts_created)  = createTS;
    }
    else
    {
      //---------------------------------------------------------------------//
      // No Indications: Create a new CxlReq:                                //
      //---------------------------------------------------------------------//
      // Get the Original Req -- the Cancellation Target: IsModify=false:
      origReq = GetTargetReq12<QT,QR,false>(a_aos);

      if (utxx::unlikely(origReq == nullptr))
        // Could not find the Target (OrigReq) -- nothing to Cancel:
        return false;

      // NB: AttchToAOS=true, ReduceOnly=false (because it's a Clx!):
      clxReq = NewReq12<QT, QR, true>
      (
        a_aos,                 origReq->m_id,         Req12::KindT::Cancel,
        PriceT(),              false,                 Qty<QT,QR>::Invalid(),
        Qty<QT,QR>::Invalid(), Qty<QT,QR>::Invalid(), false,
        NaN<double>,           a_ts_md_exch,          a_ts_md_conn,
        a_ts_md_strat,         createTS
      );
      assert(clxReq  != nullptr && clxReq->m_id == currReqN  &&
             clxReq->m_status   == Req12::StatusT::Indicated &&
             *m_ReqN == currReqN + 1);

      // Yes, we have just created a new Indication:
      isNew = true;
    }
    //-----------------------------------------------------------------------//
    // Check the CxlReq and the Target(OrigReq):                             //
    //-----------------------------------------------------------------------//
    // If we got here, both Reqs must exist:
    //
    assert(clxReq  != nullptr && clxReq->m_aos  == a_aos &&
           clxReq->m_kind     == Req12::KindT::Cancel    &&
           clxReq->m_status   == Req12::StatusT::Indicated);

#   if !UNCHECKED_MODE
    // IsModify=false:
    CheckOrigReq<QT, QR, false, IsTandem>(a_aos, origReq);
#   endif

    // XXX: And only HERE we can finally decide whether Cancel should go on. If
    // not, it is NOT an error -- just return False. In general,  we should NOT
    // encounter a live "ModLegN" here, as there is a separate method ("Cancel-
    // ModLegN") for them:
    //
    if (utxx::unlikely
       (origReq->m_kind != Req12::KindT::ModLegN &&
       (a_aos->m_isInactive || a_aos->m_isCxlPending)))
      return false;

    //-----------------------------------------------------------------------//
    // Send the Cancel out:                                                  //
    //-----------------------------------------------------------------------//
    // It is an Indication yet, so re-use "TrySendIndications":
    //
    (void) TrySendIndications<ProtoEng, SessData>
      (a_proto_eng, a_sess, origReq, clxReq, nullptr, isNew, a_batch_send,
       createTS);

    //-----------------------------------------------------------------------//
    // Finally:                                                              //
    //-----------------------------------------------------------------------//
    // IMPORTANT: The whole order is marked as "PendingCancel" (with the corr-
    // esp CxlID):
    a_aos->m_isCxlPending = clxReq->m_id;

    // Done:
    return true;
  }

  //=========================================================================//
  // "CancelModLegN":                                                        //
  //=========================================================================//
  // This is a simplified version of "CancelOrderGen", specifically  indended
  // for Cancelling "stray" ("left-over") individual "ModLegN" Reqs. Formally
  // speaking, it does NOT Cancel the whole AOS (though in many cases the lat-
  // ter already be Inactive when this method is invoked):
  //
  template<QtyTypeT QT, typename QR, typename ProtoEng, typename SessData>
  inline void EConnector_OrdMgmt::CancelModLegN
  (
    ProtoEng*       a_proto_eng,
    SessData*       a_sess,
    Req12*          a_orig_req,    // Non-NULL, must be a ModLegN
    utxx::time_val  a_ts_exch,
    utxx::time_val  a_ts_recv
  )
  {
    //-----------------------------------------------------------------------//
    // Checks:                                                               //
    //-----------------------------------------------------------------------//
    assert(a_proto_eng != nullptr && a_sess != nullptr     &&
           a_orig_req  != nullptr &&
           a_orig_req->m_kind     == Req12::KindT::ModLegN &&
          !m_hasAtomicModify);

    // NB: Even in this case, we formally do NOT require the OMC  to be Active;
    // if not Active, Indications will be created and sent out OnTimer, but NOT
    // OnEvent, because they do not appear on the main AOS sequence...
    //-----------------------------------------------------------------------//
    // AOS and an "UnAttached" CxlReq:                                       //
    //-----------------------------------------------------------------------//
    AOS*   aos  = a_orig_req->m_aos;
    assert(aos != nullptr);

    // The counters used by the Application and Session Layers must be same:
    DEBUG_ONLY(OrderID currReqN = *m_ReqN;)

    // New TimeStamps:
    utxx::time_val createTS = utxx::now_utc();

    // Create the "clxReq", but:
    // (*) use AttachToAOS=false: "clxReq" will still point to the AOS, but will
    //     NOT apper as aos->m_lastReq.    This is to prevent logic errors  with
    //     already-existing Indications;
    // (*) use "ModLegC", not "Cancel", to prevent wrongly Cancelling  the whole
    //     AOS;
    // (*) ReduceOnly is False, as it is a Clx!
    Req12* clxReq =
      NewReq12<QT, QR, false>
      (
        aos,                   a_orig_req->m_id,      Req12::KindT::ModLegC,
        PriceT(),              false,                 Qty<QT,QR>::Invalid(),
        Qty<QT,QR>::Invalid(), Qty<QT,QR>::Invalid(), false,
        NaN<double>,           a_ts_exch,             a_ts_recv,
        createTS,              createTS
      );
    assert(clxReq  != nullptr && clxReq->m_id == currReqN          &&
           clxReq->m_status   == Req12::StatusT::Indicated         &&
           clxReq->m_kind     == Req12::KindT::ModLegC             &&
           clxReq->m_aos      == aos  &&  aos->m_lastReq != clxReq &&
           *m_ReqN == currReqN + 1);

    //-----------------------------------------------------------------------//
    // Send the Cancel out:                                                  //
    //-----------------------------------------------------------------------//
    // We can still re-use "TrySendIndications". NB:
    // (*) using BatchSend if supported by the ProtoEng (the CallER is then re-
    //     sponsible for the Flush);
    // (*) if not sent, "clxReq" will be on "m_allInds";
    // (*) IsNew=true as we have just created a fresh "clxReq" above:
    //
    (void) TrySendIndications<ProtoEng, SessData>
      (a_proto_eng,    a_sess, a_orig_req, clxReq, nullptr, true,
       ProtoEng::HasBatchSend, createTS);
  }

  //=========================================================================//
  // "ModifyOrderGen":                                                       //
  //=========================================================================//
  template<QtyTypeT QT, typename QR, typename ProtoEng, typename SessData>
  inline bool EConnector_OrdMgmt::ModifyOrderGen
  (
    ProtoEng*       a_proto_eng,
    SessData*       a_sess,
    AOS*            a_aos,            // Non-NULL
    PriceT          a_new_px,
    bool            a_is_aggr,        // Intended to be Aggressive?
    QtyAny          a_new_qty,        // Full Qty
    utxx::time_val  a_ts_md_exch,
    utxx::time_val  a_ts_md_conn,
    utxx::time_val  a_ts_md_strat,
    bool            a_batch_send,     // To send multiple msgs at once
    QtyAny          a_new_qty_show,   // ditto
    QtyAny          a_new_qty_min     // ditto
  )
  {
    //-----------------------------------------------------------------------//
    // Checks:                                                               //
    //-----------------------------------------------------------------------//
    assert(a_proto_eng != nullptr && a_sess != nullptr  && a_aos != nullptr &&
           m_hasAtomicModify == ProtoEng::HasAtomicModify);

    // Make a realistic BatchSend:
    a_batch_send &= ProtoEng::HasBatchSend;

    // XXX: We do NOT check that the OMC "IsActive"; even if not, we will store
    // an Indication! But it must be Enabled:
    CHECK_ONLY
    (
      if (utxx::unlikely(!m_isEnabled))
        throw utxx::badarg_error
              ("EConnector_OrdMgmt::ModifyOrderGen: ", m_name,
               ": OMC Not Enabled");
    )
    // If the AOS is already Inactive or CxlPending, do not do anything; this
    // is NOT an error -- just return False:
    if (utxx::unlikely(a_aos->m_isInactive || a_aos->m_isCxlPending))
      return false;

    // Static and Dynamic AtomicModify flags must match:
    constexpr bool IsTandem  =  !ProtoEng::HasAtomicModify;
    assert(m_hasAtomicModify == !IsTandem);

    //-----------------------------------------------------------------------//
    // Get the SecDef and convert Qtys into Native ones:                     //
    //-----------------------------------------------------------------------//
    SecDefD const& instr  = *(a_aos->m_instr);

    // XXX: Again, providing "a_new_px" in case we need a QtyA<->QtyB convers-
    // ion; the result may be inaccurate, or fail altogether:
    Qty<QT,QR> newQty     = a_new_qty     .ConvQty<QT,QR>(instr, a_new_px);
    Qty<QT,QR> newQtyShow = a_new_qty_show.ConvQty<QT,QR>(instr, a_new_px);
    Qty<QT,QR> newQtyMin  = a_new_qty_min .ConvQty<QT,QR>(instr, a_new_px);

    //-----------------------------------------------------------------------//
    // Req(s) to send and OrigReq:                                           //
    //-----------------------------------------------------------------------//
    // These are the Reqs to be be over-written or created, and then sent or
    // Indicated; "req1" always exists, and "req0" (preceding "req1") is for
    // the Tandem mode only (and even then it is not needed if we are Modify-
    // ing a "New" Req):
    Req12*         req1     = nullptr;
    Req12*         req0     = nullptr;
    Req12 const*   origReq  = nullptr;  // Modification Target
    bool           isNew    = false;    // req1 (and req0) are newly-created?

    // The counters used by the Application and Session Layers must be same:
    assert(m_TxSN == a_sess->m_txSN && m_RxSN == a_sess->m_rxSN);
    DEBUG_ONLY(OrderID currReqN  = *m_ReqN;)

    // New TimeStamps:
    utxx::time_val createTS = utxx::now_utc();

    //-----------------------------------------------------------------------//
    // Over-write existing Indications(s) or create new Req(s)?              //
    //-----------------------------------------------------------------------//
    // Do we have Indications (as the last 1 or 2 Reqs)? If yes, then we first
    // over-write them.   There is no need to search for the Modify Target, as
    // the Target is already specified in Indications:
    //
    Req12* lastReq  = a_aos->m_lastReq;
    assert(lastReq != nullptr);     // AOS always contains at least 1 Req

    if (lastReq->m_status == Req12::StatusT::Indicated)
    {
      //---------------------------------------------------------------------//
      // Yes, at least 1 Indication: Over-write existing Req(s):             //
      //---------------------------------------------------------------------//
      Req12* preLReq = lastReq->m_prev;

      if (preLReq != nullptr && preLReq->m_status == Req12::StatusT::Indicated)
      {
        //-------------------------------------------------------------------//
        // 2 Indications:                                                    //
        //-------------------------------------------------------------------//
        // Then "prevReq" can only be a "ModLegC", and lastReq  is "ModLegN".
        // Thus, we are  in the Tandem mode. The preceding Req, if any, must NOT
        // be an Indication:
        assert(IsTandem                                 &&
               lastReq->m_kind == Req12::KindT::ModLegN &&
               preLReq->m_kind == Req12::KindT::ModLegC &&
              (preLReq->m_prev == nullptr ||
               preLReq->m_prev->m_status  != Req12::StatusT::Indicated));

        // In this case, the Indications will just be modified in-place;  they
        // will remain a (ModLegC+ModLegN) pair. The Target is that of preLReq,
        // and it must exist:
        origReq = GetOrigReq(preLReq);
        assert(origReq != nullptr);

        // If OK: Over-write the (req0=prvLReq) -- TimeStamps only:
        req0 = preLReq;
        const_cast<utxx::time_val&>(req0->m_ts_md_exch)  = a_ts_md_exch;
        const_cast<utxx::time_val&>(req0->m_ts_md_conn)  = a_ts_md_conn;
        const_cast<utxx::time_val&>(req0->m_ts_md_strat) = a_ts_md_strat;
        const_cast<utxx::time_val&>(req0->m_ts_created)  = createTS;
      }
      else
      {
        //-------------------------------------------------------------------//
        // Exactly 1 Indication:                                             //
        //-------------------------------------------------------------------//
        // (*) It can be a "New", a "ModLegN" (the corresp "ModLegC" has already
        //     been sent), or "Modify" in the AtomicModify (!Tandem) mode.
        // (*) It CANNOT be a "Cancel", because an Indicated "Cancel" could not
        //     be Failed, and thus,  the AOS must be marked as "CxlPending"  in
        //     this case (that we have already excluded).
        // Again, the Indication can only be modified in-place in this case:
        //
        assert(lastReq->m_kind == Req12::KindT::New                   ||
              (lastReq->m_kind == Req12::KindT::ModLegN &&  IsTandem) ||
              (lastReq->m_kind == Req12::KindT::Modify  && !IsTandem));

        origReq = GetOrigReq(lastReq);

        // Obviously, "origReq" is NULL iff "lastReq" is "New" or "ModlegN":
        assert((origReq == nullptr) ==
              (lastReq->m_kind == Req12::KindT::New    ||
               lastReq->m_kind == Req12::KindT::ModLegN));
      }
      //---------------------------------------------------------------------//
      // In both cases, over-write the req1==lastReq:                        //
      //---------------------------------------------------------------------//
      // NB: "req0" can only be a "ModLegC", so it is never over-written:
      //
      req1 = lastReq;
      assert(req1 != nullptr);

      // First, if the OrigReq is non-NULL, verify the new params against it:
      if (utxx::likely(origReq != nullptr))
      {
        // Verify the OrigReq and adjust the params:
        if (utxx::unlikely
           (!CheckModifyParams
              (a_aos, *origReq, &a_new_px, &newQty, &newQtyShow, &newQtyMin)))
          return false;
      }
      // Update the "req1":
      // Once again, it is a "New", "Modify" or "ModLegN", so we over-write the
      // Px, Qty(s) and TimeStamps. The Req Kind remains the same, in particul-
      // ar a modified "New" is still a "New" with modified params!
      //
      assert(req1->m_kind == Req12::KindT::New                    ||
            (req1->m_kind == Req12::KindT::ModLegN  &&  IsTandem) ||
            (req1->m_kind == Req12::KindT::Modify   && !IsTandem));

      // NB: It is STILL possible that we get invalid NewPx and/or NewQty(s)
      // here, but only in case when OrigReq  was NULL (ie it is a New).  So
      // if new pxs or qtys are not valid, do not apply them -- keep the ones
      // already there. NB: A px may in general be <= 0 (ie for Rates):
      //
      if (utxx::likely(IsFinite(a_new_px)))
        const_cast<PriceT&>(req1->m_px)    = a_new_px;

      if (utxx::likely(IsPos(newQty)))
      {
        const_cast<QtyU&>(req1->m_qty)     = QtyU(newQty);
        req1->m_leavesQty                  = QtyU(newQty);  // YET UNFILLED!
      }
      if (utxx::likely(!IsNeg(newQtyShow)))
        const_cast<QtyU&>(req1->m_qtyShow) = QtyU(newQtyShow);

      if (utxx::likely(!IsNeg(newQtyMin)))
        const_cast<QtyU&>(req1->m_qtyMin)  = QtyU(newQtyMin);

      // Time Stamps:
      const_cast<utxx::time_val&>(req1->m_ts_md_exch)  = a_ts_md_exch;
      const_cast<utxx::time_val&>(req1->m_ts_md_conn)  = a_ts_md_conn;
      const_cast<utxx::time_val&>(req1->m_ts_md_strat) = a_ts_md_strat;
      const_cast<utxx::time_val&>(req1->m_ts_created)  = createTS;

      // This is NOT all -- the Indications may actually be sent out below as
      // "New" Reqs!
    }
    else
    {
      //---------------------------------------------------------------------//
      // No Indications: Create new Req(s):                                  //
      //---------------------------------------------------------------------//
      // First, get the Original Req -- the Modification Target: NB: "GetTarg-
      // etReq12" checks for Inactive, CxlPending or Part-Filled Unmodifyable
      // orders:  IsModify=true:
      //
      origReq = GetTargetReq12<QT,QR,true>(a_aos);

      if (utxx::unlikely(origReq == nullptr))
        // Could not find the Target (OrigReq) -- nothing to Modify, but this
        // is not an error:
        return false;

      // Generic Case: Got the Target.
      // Again, apply the "origReq" to verify / update the "new" params:
      if (utxx::unlikely
         (!CheckModifyParams
            (a_aos, *origReq, &a_new_px, &newQty, &newQtyShow, &newQtyMin)))
        return false;

      // Then we need to create new Req12(s) for the Modify. Depending on whe-
      // ther the corresp order(s) are actually sent on the wire (below), they
      // will become "New" (by default) or "Indication" orders:
      // XXX: Previously, we were creating Reqs AFTER the order has been sent
      // on the wire (if so),   which resulted is a slight latency advantage,
      // but was highly error-prone:
      //
      if constexpr (!IsTandem)
      {
        // Normal (Atomic) "Modify" @ currReqN. XXX: Pegging params are curren-
        // tly not modifyable, so just copied over;
        // AttachToAOS=true, ReduceOnly=false (as it is part of Modify):
        req1 =
          NewReq12<QT, QR, true>
          (
            a_aos,                 origReq->m_id,         Req12::KindT::Modify,
            a_new_px,              a_is_aggr,             newQty,
            newQtyShow,            newQtyMin,             origReq->m_pegSide,
            origReq->m_pegOffset,  a_ts_md_exch,          a_ts_md_conn,
           a_ts_md_strat,          createTS
          );
        assert(req1 != nullptr         && req1->m_id == currReqN &&
               req1->m_status  == Req12::StatusT::Indicated      &&
               *m_ReqN == currReqN + 1 && req0 == nullptr);
      }
      else
      {
        // Tandem: First, the "ModLegC" @ currReqN. This is similar to "Cancel",
        // so all Pxs and Qtys are Invalid;
        // again, AttachToAOS=true, ReduceOnly=false (as it is a Clx!):
        req0 =
          NewReq12<QT, QR, true>
          (
            a_aos,                 origReq->m_id,         Req12::KindT::ModLegC,
            PriceT(),              false,                 Qty<QT,QR>::Invalid(),
            Qty<QT,QR>::Invalid(), Qty<QT,QR>::Invalid(), false,
            NaN<double>,           a_ts_md_exch,          a_ts_md_conn,
            a_ts_md_strat,         createTS
          );
        assert(req0 != nullptr && req0->m_id == currReqN    &&
               req0->m_status  == Req12::StatusT::Indicated &&
               *m_ReqN == currReqN + 1);

        // Now the "ModLegN" @ (currReqN+1). NB: Here OrigID is 0, as "ModLegN"
        // is similar to a "New";
        // again, AttachToAOS=true, ReduceOnly=false (as it is part of Modify):
        req1 =
          NewReq12<QT, QR, true>
          (
            a_aos,                 0,                     Req12::KindT::ModLegN,
            a_new_px,              a_is_aggr,             newQty,
            newQtyShow,            newQtyMin,             origReq->m_pegSide,
            origReq->m_pegOffset,  a_ts_md_exch,          a_ts_md_conn,
            a_ts_md_strat,         createTS
          );
        assert(req1 == req0 + 1 && req1->m_id == currReqN + 1 &&
               req1->m_status   == Req12::StatusT::Indicated  &&
               *m_ReqN == currReqN + 2);
      }
      // NB: "isNew" applies to "req1" (iff "req0" is NULL), or to both "req1"
      // and "req0": they are either both new, or both not new:
      //
      isNew = true;
    }
    //-----------------------------------------------------------------------//
    // Check the Modification Req(s) and the OrigReq (if exists):            //
    //-----------------------------------------------------------------------//
    // "req1" is always available, and must contain the actual new order params
    // which will be used for wire sending:
#   ifndef NDEBUG
    Qty<QT,QR> qty1     = req1->GetQty    <QT,QR>();
    Qty<QT,QR> qtyShow1 = req1->GetQtyShow<QT,QR>();
    Qty<QT,QR> qtyMin1  = req1->GetQtyMin <QT,QR>();
#   endif
    assert(req1 != nullptr         && req1->m_aos   == a_aos       &&
          (req1->m_kind   == Req12::KindT::New                     ||
          (req1->m_kind   == Req12::KindT::ModLegN  &&  IsTandem)  ||
          (req1->m_kind   == Req12::KindT::Modify   && !IsTandem)) &&
           req1->m_aos    == a_aos && req1->m_px    == a_new_px    &&
           qty1 == newQty && qtyShow1 == newQtyShow && qtyMin1 == newQtyMin);

    // "req0" is non-NULL iff we are in the Tandem mode, and "req1" was not a
    // "New" (ie it is a "ModLegN"):
    assert(req0 == nullptr                                   ||
          (IsTandem && req1->m_kind == Req12::KindT::ModLegN &&
                       req0->m_kind == Req12::KindT::ModLegC &&
                       req0->m_aos  == a_aos));

    // "origReq" is always available unless:
    // "req1" (formerly "lastReq") is an Indicated "New" (which may happen in
    // both Tandem and !Tandem modes) or an Indicated "ModLegN" (Tandem only,
    // and w/o "req0"):
    //
    assert((origReq == nullptr) ==
           ((req1->m_kind  == Req12::KindT::New                 ||
            (req1->m_kind  == Req12::KindT::ModLegN && IsTandem &&
             req0   == nullptr))                                &&
            req1->m_status == Req12::StatusT::Indicated));

#   if !UNCHECKED_MODE
    // Check the "origReq" further (IsModify=true):
    CheckOrigReq<QT, QR, true, IsTandem>(a_aos, origReq);
#   endif

    //-----------------------------------------------------------------------//
    // Risk checks (with the intended Qty) first:                            //
    //-----------------------------------------------------------------------//
    // An exception will be thrown if the check fails:
    if (utxx::likely(m_riskMgr != nullptr))
      // IsReal=true:
      m_riskMgr->OnOrder<true>
        (this,   instr, a_aos->m_isBuy, QT,     a_new_px, double(newQty),
         origReq->m_px, double(origReq->GetQty<QT,QR>()), a_ts_md_strat);

    //-----------------------------------------------------------------------//
    // Send the Req(s) out:                                                  //
    //-----------------------------------------------------------------------//
    // They are Indications yet, so reuse "TrySendIndications", but beware of
    // the orger of the args:
    //
    (void) TrySendIndications<ProtoEng, SessData>
    (
      a_proto_eng,    a_sess,    origReq,
      ((req0 != nullptr) ? req0 : req1),
      ((req0 != nullptr) ? req1 : nullptr), isNew, a_batch_send, createTS
    );
    // All Done!
    return true;
  }

  //=========================================================================//
  // "CancelAllOrdersGen":                                                   //
  //=========================================================================//
  // Generates and sends an "OrderMassCancelRequest":
  //
# if defined(__GNUC__) && !defined(__clang__)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wunused-but-set-parameter"
# endif
  template<QtyTypeT QT, typename QR, typename ProtoEng, typename SessData>
  inline void EConnector_OrdMgmt::CancelAllOrdersGen
  (
                     ProtoEng*       a_proto_eng,
                     SessData*       a_sess,
                     unsigned long   a_strat_hash48,
                     SecDefD const*  a_instr,    // May be NULL
                     FIX::SideT      a_side,     // May be UNDEFINED
    [[maybe_unused]] char const*     a_segm_id   // Segment or Session ID; may
                                                 // be NULL or empty
  )
  {
    assert(a_proto_eng != nullptr && a_sess != nullptr);

    // First of all, the OMC must be Enabled:
    CHECK_ONLY
    (
      if (utxx::unlikely(!m_isEnabled))
        throw utxx::badarg_error
              ("EConnector_OrdMgmt:CancelAllOrdersGen: ", m_name,
               ": OMC Not Enabled");
    )
    // XXX: If the OMC is NOT Active, it is a serious problem because MassCancel
    // is typically required urgently, and may not result in Indications (if not
    // Emulated). Log an error but do not throw an exception.
    // NB: "IsActive" is tested on the ProtoEng,  not via the top-level virtual
    // method, for efficiency and for compatibility with "FlushOrderGen" below:
    //
    if (utxx::unlikely(!(a_proto_eng->IsActive())))
    {
      LOG_ERROR(1,
        "EConnector_OrdMgmt::CancelAllOrdersGen: ProtoEng is Not Active")
      return;
    }
    // If OK:
    //
    if constexpr (!ProtoEng::HasNativeMassCancel)
      // Invoke the Emulator. NB: It uses "CancelOrderGen" which checks the
      // Throttlers properly:
      //
      EConnector_OrdMgmt::EmulateMassCancelGen<QT, QR, ProtoEng, SessData>
        (a_proto_eng, a_sess, a_strat_hash48, a_instr, a_side);
    else
      // No Need for Emulation: Invoke the Underlying "ProtoEng":
      // NB: In that case, unlike "EmulateMassCancelGen" above, we will NOT
      // install Cancel "Req12"s in the subject AOSes.  And since this is a
      // rare one-off msg, we do not check the Throttlets here:
      //
      a_proto_eng->CancelAllOrdersImpl
        (a_sess, a_strat_hash48, a_instr, a_side, a_segm_id);
  }
# if defined(__GNUC__) && !defined(__clang__)
# pragma GCC diagnostic pop
# endif

  //=========================================================================//
  // "FlushOrdersGen":                                                       //
  //=========================================================================//
  template<typename ProtoEng, typename SessData>
  inline utxx::time_val EConnector_OrdMgmt::FlushOrdersGen
  (
    ProtoEng*       a_proto_eng,
    SessData*       a_sess
  )
  {
    // First of all, the OMC must be Enabled:
    CHECK_ONLY
    (
      if (utxx::unlikely(!m_isEnabled))
        throw utxx::badarg_error
              ("EConnector_OrdMgmt::FlushOrdersGen: ", m_name,
               ": OMC Not Enabled");
    )
    // Similar to "CancelAllOrdersGen" above, if the ProtoEng is NOT Actice, we
    // log a 1st-class error but do not throw an exception -- a subsequent call
    // to "FlushOrderGen" may succeed:
    //
    if (utxx::unlikely(!(a_proto_eng->IsActive())))
    {
      LOG_ERROR(1, "EConnector_OrdMgmt::FlushOrdersGen: ProtoEng is Not Active")
      return utxx::time_val();
    }
    // If OK:
    // Invoke the Protocol Layer (which, most likely, will in turn invoke the
    // Session Layer when it comes to the actual data sending over the network):
    assert(a_proto_eng != nullptr && a_sess != nullptr);
    utxx::time_val sendTime = a_proto_eng->FlushOrdersImpl(a_sess);

    // NB: If BatchSend is not suppoted by the underlying Protocol, the unflush-
    // ed buffer should be empty, and so is "sendTime":
    assert(ProtoEng::HasBatchSend || sendTime.empty());

    if (utxx::likely(!sendTime.empty()))
      // Propagate this "sendTime" to prev "Req12"s. NB: The last valid ReqN
      // is (*m_ReqN-1), because "*m_ReqN" is where the NEXT Req12 will be:
      BackPropagateSendTS(sendTime, (*m_ReqN)-1);

    // Notionally, there are no buffered orders anymore (the following counter
    // only affects the depth of "BackPropagateSendTS"):
    m_nBuffdReqs = 0;
    return sendTime;
  }

  //=========================================================================//
  // "OrderAcknowledged":                                                    //
  //=========================================================================//
  // XXX: For "OrderAcknowledged", there are no TimeStamp updates or Strategy
  // notifications, nor ExchIDs currently:
  //
  template<QtyTypeT QT, typename QR>
  inline void EConnector_OrdMgmt::OrderAcknowledged
    (OrderID a_id, OrderID a_aos_id, PriceT a_px, Qty<QT,QR> a_leaves_qty)
  {
    // The Req Kind can be ANY, search is Strict:
    Req12* req  =
      GetReq12<Req12::AnyKindT, true>
        (a_id, a_aos_id, a_px, a_leaves_qty, "OrderAcknowledged");
    assert(req != nullptr);

    // Set the Status to "Acked", unless it is already "Acked" or higher:
    if (utxx::likely(req->m_status < Req12::StatusT::Acked))
      req->m_status = Req12::StatusT::Acked;

    // XXX: Should we reset the ConsFails Counter in this case? -- Not yet,
    // because an Acked order can still be Rejected afterwards!
  }

  //=========================================================================//
  // "OrderConfirmedImpl":                                                   //
  //=========================================================================//
  // Invoked on ANY order confirmation -- New, ModifyReq or CancelReq. Returns
  // a pair (ConfirmedReq12, PrevStatus):
  //
  template<unsigned Mask, QtyTypeT QT, typename QR>
  inline  std::pair<Req12*, Req12::StatusT>
  EConnector_OrdMgmt::OrderConfirmedImpl
  (
    OrderID           a_id,         // Must be non-0
    OrderID           a_aos_id,     // Optional (0 if unknown)
    ExchOrdID const&  a_exch_id,
    ExchOrdID const&  a_mde_id,     // May or may not be available
    PriceT            a_px,         // May be empty
    Qty<QT, QR>       a_leaves_qty, // Again, optional
    utxx::time_val    a_ts_exch,
    utxx::time_val    a_ts_recv,
    char const*       a_where       // Non-NULL
  )
  {
    assert(a_where != nullptr);

    // Get the Req12 and AOS. They are Strict (we must get a non-NULL) UNLESS
    // the Req in question is "Cancel" (because auto-Cancels w/o a Req may oc-
    // cur; but for a "ModLegC", it is still Strict):
    //
    constexpr bool Strict = (Mask & unsigned(Req12::KindT::Cancel)) == 0;

    Req12* req = GetReq12<Mask, Strict, QT, QR>
      (a_id, a_aos_id, a_px, a_leaves_qty, a_where);
    assert(!Strict  || req != nullptr);

    if (utxx::unlikely(req == nullptr))
      // Nothing else to do: We cannot use "a_exch_id" or "a_mde_id" because
      // they are to be set AS A RESULT OF "Confirm"!
      return std::make_pair(nullptr, Req12::StatusT::UNDEFINED);

    // Generic Case:
    assert(req != nullptr);
    AOS*   aos =  req->m_aos;
    assert(aos != nullptr && (a_aos_id == 0 || a_aos_id == aos->m_id));

    // It must be a real order, not an Indication of course:
    Req12::StatusT prevStat = req->m_status;
    assert(prevStat != Req12::StatusT::Indicated);

    // The result to be returned:
    std::pair<Req12*, Req12::StatusT> res = std::make_pair(req, prevStat);

    // Apply or verify the ExchID: Force=false, as there could be (in theory)
    // multiple inconsistent confirmations -- will guard against them:
    ApplyExchIDs<false>(req, aos, a_exch_id, a_mde_id, a_where);

    // If we got a non-trivial "MDEntryID", register this Order with all Linked
    // MDCs:
    if (req->m_mdEntryID.GetOrderID() != 0)
      for (EConnector_MktData* mdc: m_linkedMDCs)
      {
        assert(mdc != nullptr);
        mdc->RegisterOrder(*req);
      }

    // Mark the Req as "Confirmed" (if it did not get any more advanced status
    // yet):
    if (prevStat < Req12::StatusT::Confirmed)
    {
      req->m_status        = Req12::StatusT::Confirmed;
      req->m_ts_conf_exch  = a_ts_exch;
      req->m_ts_conf_conn  = a_ts_recv;
    }
    // All Done!
    return res;
  }

  //=========================================================================//
  // "OrderConfirmedNew":                                                    //
  //=========================================================================//
  template<QtyTypeT QT, typename QR, typename ProtoEng, typename SessData>
  inline Req12 const& EConnector_OrdMgmt::OrderConfirmedNew
  (
    ProtoEng*         a_proto_eng,
    SessData*         a_sess,
    OrderID           a_id,         // Must be non-0
    OrderID           a_aos_id,     // Optional (0 if unknown)
    ExchOrdID const&  a_exch_id,
    ExchOrdID const&  a_mde_id,     // May or may not be available
    PriceT            a_px,         // May be empty
    Qty<QT, QR>       a_leaves_qty, // May be empty
    utxx::time_val    a_ts_exch,
    utxx::time_val    a_ts_recv
  )
  {
    RefreshThrottlers(a_ts_recv);
    assert(a_proto_eng != nullptr && a_sess != nullptr);

    // Invoke the Impl. The ReqKind affected must be "New" or "ModLegN". The
    // Impl returns the Req and its Prev Status:
    constexpr unsigned NewMask = unsigned(Req12::KindT::New)     |
                                 unsigned(Req12::KindT::ModLegN);
    auto res =
      OrderConfirmedImpl<NewMask, QT, QR>
      (
        a_id, a_aos_id, a_exch_id, a_mde_id, a_px, a_leaves_qty,
        a_ts_exch,      a_ts_recv, "OrderConfirmedNew"
      );
    // Get the NewReq:
    Req12* req  = res.first;
    assert(req != nullptr);
    Req12::StatusT prevStat = res.second;

    // Get the AOS:
    AOS*   aos  = req->m_aos;
    assert(aos != nullptr && (a_aos_id == 0 || a_aos_id == aos->m_id));

    // We can now send out the Indications (if any) which were waiting for this
    // "req" to be "Confirmed":
    SendIndicationsOnEvent<ProtoEng, SessData>
      (a_proto_eng, a_sess, aos, a_ts_recv);

    // A "New" order must be the 1st one in its AOS (but this is not true for a
    // "ModLegN" one, obviously):
    assert(req->m_kind != Req12::KindT::New || aos->m_frstReq == req);

    // If this Req was not Confirmed previously (this might happen, however un-
    // likely), invoke the following actions:
    //
    if (prevStat < Req12::StatusT::Confirmed)
    {
      // Notify the Strategy about this Confirm (unless this AOS is a left-over
      // from a previous invocation). Catch any exceptions (apart from ExitRun)
      // which could be propagated from the Strategy:
      //
      Strategy* strat = aos->m_strategy;
      if (utxx::likely(strat != nullptr))
        SAFE_STRAT_CB(strat, OnConfirm, (*req))
    }
    // All Done!
    return *req;
  }

  //=========================================================================//
  // "OrderReplaced":                                                        //
  //=========================================================================//
  // NB: For Atomic "Modify" ONLY; NOT for "ModLegC"+"ModLegN":
  //
  template<QtyTypeT QT, typename QR, typename ProtoEng, typename SessData>
  inline Req12 const& EConnector_OrdMgmt::OrderReplaced
  (
    ProtoEng*         a_proto_eng,
    SessData*         a_sess,
    OrderID           a_curr_id,      // ID of the Replacement Req
    OrderID           a_orig_id,      // ID of the Replaced    Req
    OrderID           a_aos_id,       // 1st ReqID for that AOS; 0 if unknown
    ExchOrdID const&  a_curr_exch_id, // Same for Exch-assigned IDs
    ExchOrdID const&  a_orig_exch_id, //
    ExchOrdID const&  a_curr_mde_id,  // For the MDC->OMC link
    PriceT            a_curr_px,      // Helper only; empty OK
    Qty<QT, QR>       a_leaves_qty,   // Helper only
    utxx::time_val    a_ts_exch,
    utxx::time_val    a_ts_recv
  )
  {
    RefreshThrottlers(a_ts_recv);

    // First of all, the Replacement (Curr) Req now becomes Confirmed. It can
    // only be a "Modify" (ie if a tandem Cancel-New is used to emulate "Modi-
    // fy", this method is NOT invoked):
    constexpr unsigned CurrMask = unsigned(Req12::KindT::Modify);
    auto curr =
      OrderConfirmedImpl<CurrMask, QT, QR>
      (
        a_curr_id,    a_aos_id,  a_curr_exch_id, a_curr_mde_id, a_curr_px,
        a_leaves_qty, a_ts_exch, a_ts_recv,      "OrderReplaced-Curr"
      );
    Req12* currReq  = curr.first;
    assert(currReq != nullptr);
    Req12::StatusT prevStat = curr.second;

    // Then find the OrigReq (the one which is now Replaced). NB: "a_orig_id"
    // may be 0 in some underlying protocols (eg TWIME), but then the correct
    // one could be available from the "currReq" (if present):
    if (a_orig_id == 0)
      a_orig_id = currReq->m_origID;      // May still be 0!

    // Similarly, for "origReq", the Kind can only be "New" or "Modify", Non-
    // Strict. If the OrigID is 0, perform search by OrigExchID.  No Px info
    // here:
    constexpr unsigned OrigMask = unsigned(Req12::KindT::New)  |
                                  unsigned(Req12::KindT::Modify);
    Req12* origReq =
      GetReq12<OrigMask, false, QT, QR>
        (a_orig_id, a_aos_id, PriceT(), Qty<QT,QR>::Invalid(),
         "OrderReplaced-Orig");

    if (utxx::unlikely(origReq == nullptr && m_useExchOrdIDsMap)) // Rare case!
      origReq = GetReq12ByExchID<OrigMask>(a_orig_exch_id);

    // Eventually, "origReq" must have been found:
    CHECK_ONLY
    (
      if (utxx::unlikely(origReq == nullptr))
        throw utxx::runtime_error
              ("EConnector_OrdMgmt::OrderReplaced: OrigID=",  a_orig_id,
               ", ExchID=", a_orig_exch_id.ToString(),   ": Not Found");
    )
    // Gereric Case: If we got here, both Reqs were found:
    assert(origReq != nullptr);

    // Both Reqs must be real, not Indications, of course. Furthermore, the
    // "origReq" should normally be "Confirmed" by now,  but we cannot make
    // formal assumptions on that:
    assert(currReq->m_status != Req12::StatusT::Indicated &&
           origReq->m_status != Req12::StatusT::Indicated);

    // Obviously, "currReq" must be a "Modify", whereas "origReq" could be "New"
    // or "Modify":
#   if !UNCHECKED_MODE
    CheckOrigID(*currReq, a_orig_id, "OrderReplaced");
#   endif

    // Obviously, both "Req"s (if available) must point to the same AOS:
    AOS*   aos  = GetSameAOS(*currReq, *origReq, "OrderReplaced");
    assert(aos != nullptr && (a_aos_id == 0 || a_aos_id == aos->m_id));

    // Send out the Indications (if any) waiting on "currReq" to be "Confirmed".
    // NB: The "origReq" (see below) CANNOT be a dependency for yet-unsent Indi-
    // cations, becuase in that case, "currReq" could not have been sent!
    //
    SendIndicationsOnEvent<ProtoEng, SessData>
      (a_proto_eng, a_sess, aos, a_ts_recv);

    // Mark "origReq" as "Replaced" -- but there is no Strategy Call-Back invo-
    // cation on "origReq". We also do NOT invoke the RiskMgr here -- order mo-
    // dification has already been accounted for, when the corresp Req was sub-
    // mitted:
    origReq->m_status      = Req12::StatusT::Replaced;
    origReq->m_ts_end_exch = a_ts_exch;
    origReq->m_ts_end_conn = a_ts_recv;

    // However, we may need to invoke Call-Backs on the "currReq" update. This
    // is similar to "OrderConfirmedNew":
    if (prevStat < Req12::StatusT::Confirmed)
    {
      Strategy* strat = aos->m_strategy;
      if (utxx::likely(strat != nullptr))
        SAFE_STRAT_CB( strat, OnConfirm, (*currReq) )
    }
    // All Done!
    return *currReq;
  }

  //=========================================================================//
  // "OrderCancelled":                                                       //
  //=========================================================================//
  template<QtyTypeT QT, typename QR, typename ProtoEng, typename SessData>
  inline AOS const& EConnector_OrdMgmt::OrderCancelled
  (
    ProtoEng*         a_proto_eng,
    SessData*         a_sess,
    OrderID           a_clx_id,
    OrderID           a_orig_id,
    OrderID           a_aos_id,          // Optional; for both Cxl and Orig Reqs
    ExchOrdID const&  a_exch_id,         // Orig
    ExchOrdID const&  a_mde_id,          // Orig
    PriceT            a_orig_px,         // May be empty (hint only)
    Qty<QT, QR>       a_orig_leaves_qty, // May be empty as well
    utxx::time_val    a_ts_exch,
    utxx::time_val    a_ts_recv
  )
  {
    RefreshThrottlers(a_ts_recv);
    assert(a_proto_eng != nullptr && a_sess != nullptr);

    // First, get the "clxReq". It should normally be non-NULL,  except when an
    // order was cancelled by a MassCancel or an automatic "CancelOnDisconnect":
    // The "clxReq" will be Confirmed:
    constexpr unsigned CxlMask =  unsigned(Req12::KindT::Cancel) |
                                  unsigned(Req12::KindT::ModLegC);
    auto clx =
      OrderConfirmedImpl<CxlMask, QT, QR>
      (
        a_clx_id,    a_aos_id, EmptyExchOrdID, EmptyExchOrdID,  PriceT(),
        Qty<QT,QR>::Invalid(), a_ts_exch,      a_ts_recv,  "OrderCancelled-Cxl"
      );
    Req12* clxReq = clx.first;  // May be NULL!

    // If non-NULL, "clxReq" must NOT be an Indication, of course:
    assert(clxReq == nullptr || clxReq->m_status != Req12::StatusT::Indicated);

    // NB: Normally (eg in FIX), "a_orig_id" is always available; but some pro-
    // tocols (eg TWIME) do not provide it, so we will need to find OrigReq by
    // other means. Use "clxReq" if available:
    //
    if (a_orig_id == 0 && clxReq != nullptr)
      a_orig_id = clxReq->m_origID;

    // The "origReq" can be "New", "Modify" or "ModLegN", but not "Cancel" or
    // "ModLegC" of course;  and again, search is Non-Strict. We typically do
    // not have Px for OrigReq:
    constexpr unsigned OrigMask = unsigned(Req12::KindT::New)    |
                                  unsigned(Req12::KindT::Modify) |
                                  unsigned(Req12::KindT::ModLegN);
    Req12* origReq =
      GetReq12<OrigMask, false, QT, QR>
        (a_orig_id, a_aos_id, a_orig_px, a_orig_leaves_qty,
         "OrderCancelled-Orig");

    // XXX: The following is the most complicated case (eg an unsolicited Can-
    // cel msg in a prococol like TWIME): Will have to do a look-up by ExchID:
    //
    if (utxx::unlikely(origReq == nullptr && m_useExchOrdIDsMap))
      origReq = GetReq12ByExchID<OrigMask>(a_exch_id);

    // Found Eventually?
    if (utxx::unlikely(origReq == nullptr))
      throw utxx::runtime_error
            ("EConnector_OrdMgmt::OrderCancelled: OrigID=",  a_orig_id,
             ", ExchID=", a_exch_id.ToString(), ": Not Found");

    // OK: "origReq" must not be an Indication. Furthermore, the "origReq"
    // should normally be "Confirmed" by now, but we cannot make any formal
    // assumptions on that:
    assert(origReq != nullptr &&
           origReq->m_status  != Req12::StatusT::Indicated);

    // Get the AOS:
    AOS*   aos  = origReq->m_aos;
    assert(aos != nullptr && (a_aos_id == 0 || a_aos_id == aos->m_id));

    // IMPORTANT: Verify the ExchangeID to avoid spurious Cancellations;  if
    // the check fails, return via an exception: Force=false (consistency chk
    // IS performed):
    ApplyExchIDs<false>(origReq, aos, a_exch_id, a_mde_id, "OrderCancelled");

    // If "clxReq" is really available:  Check if it is part of a "Cancel-New"
    // Tandem emulating "Modify". XXX:   It would be a BIG problem if "clxReq"
    // is unavailable (NULL) but it is still a Tandem: We then mark the AOS as
    // Inactive erroneously!
    //
    bool isTandem  = false;
    if (utxx::likely(clxReq != nullptr))
    {
      isTandem = (clxReq->m_kind == Req12::KindT::ModLegC);
      CHECK_ONLY
      (
        // Check that the Embedded and Protocol OrigIDs match:
        CheckOrigID(*clxReq, a_orig_id, "OrderCancelled");

        // Check for the following error condition -- if happens, it is VERY
        // SEVERE:
        if (utxx::unlikely(clxReq == origReq))
          throw utxx::logic_error
                ("EConnector_OrdMgmt::OrderCancelled: OrigID=", a_orig_id,
                 ", ExchID=", a_exch_id.ToString(), ", CxlID=", a_clx_id,
                 ": Got OrigReq==CxlReq");

        // And check that both "Req"s must in this case point to the same AOS
        // (already obtained above):
        (void) GetSameAOS(*clxReq, *origReq, "OrderCancelled");
      )
    }
    // NB: This Cancel may actually be part of "Cancel-New" Tandem in which case
    // the whole AOS is NOT made "Inactive".   Otherwise, mark it as such.  This
    // also marks all yet-unconfirmed Reqs as "WillFail".
    // IsTandem is only possible if Modifies are Non-Atomic:
    assert(!(isTandem && m_hasAtomicModify));

    // Now really process it as Cancelled:
    OrderCancelledImpl<QT, QR, ProtoEng, SessData>
      (a_proto_eng, a_sess, aos, clxReq, origReq, isTandem,
       a_ts_exch,   a_ts_recv);

    return *aos;
  }

  //=========================================================================//
  // "OrderCancelledImpl": Back-end of "OrderCancelled":                     //
  //=========================================================================//
  template<QtyTypeT QT, typename QR, typename ProtoEng, typename SessData>
  void EConnector_OrdMgmt::OrderCancelledImpl
  (
    ProtoEng*       a_proto_eng,
    SessData*       a_sess,
    AOS*            a_aos,
    Req12*          a_clx_req,
    Req12*          a_orig_req,
    bool            a_is_tandem,
    utxx::time_val  a_ts_exch,
    utxx::time_val  a_ts_recv
  )
  {
    assert(a_proto_eng != nullptr     && a_sess     != nullptr    &&
           a_aos       != nullptr     && a_orig_req != nullptr    &&
           a_orig_req->m_aos == a_aos &&
          (a_clx_req  == nullptr      || a_clx_req->m_aos == a_aos));

    // NB: In some rare cases, OrigReq may just be an Indication which is Can-
    // celled before it could be sent out -- this is OK.
    // If this Cancel is not part of a Tandem (ModLegC+ModLegN), it means that
    // the whole AOS becomes Inactive now and forever:
    //
    if (!a_is_tandem)
      MakeAOSInactive<QT, QR, ProtoEng, SessData>
        (a_proto_eng, a_sess, a_aos, a_clx_req, "OrderCancelledImpl",
         a_ts_exch,   a_ts_recv);

    //-----------------------------------------------------------------------//
    // Call-Backs:                                                           //
    //-----------------------------------------------------------------------//
    // Invoke the RiskMgr -- the corresp order will be removed from  potential
    // Risk Srcs:
    if (m_riskMgr != nullptr)
    {
      SecDefD const& instr = *(a_aos->m_instr);

      // Unwinding a cancelled order: IsReal=false, NewQty=0.   NB: the actual
      // amount of risk reduction is determined by the remaining unfilled qty,
      // not by the very original order qty, so OldQty = LeavesQty:
      // NB: For a Cancel, NewPx=0 (not NaN):
      //
      m_riskMgr->OnOrder<false>
        (this,      instr,   a_aos->m_isBuy,  m_qt,      PriceT(0.0),  0.0,
         a_orig_req->m_px, double(a_orig_req->GetLeavesQty<QT,QR>()),
         utxx::time_val());
    }
    // Mark "a_orig_req" as "Cancelled" or "Replaced" (in case this Cancel is
    // part of a Tandem) unless it already got a more advanced status yet (eg
    // already Cancelled or Filled), and invoke the Strategy Call-Back:
    //
    if (!a_is_tandem && a_orig_req->m_status < Req12::StatusT::Cancelled)
    {
      // Really Cancelled:
      a_orig_req->m_ts_end_exch = a_ts_exch;
      a_orig_req->m_ts_end_conn = a_ts_recv;
      a_orig_req->m_status      = Req12::StatusT::Cancelled;

      // Invoke the Strategy Call-Back (it may be missing if the AOS is a
      // left-over from a previous invocation). Again, catch possible exc-
      // eptions:
      Strategy* strat = a_aos->m_strategy;
      if (utxx::likely(strat != nullptr))
        SAFE_STRAT_CB( strat, OnCancel, (*a_aos, a_ts_exch, a_ts_recv) )
    }
    else
    if (a_is_tandem && a_orig_req->m_status < Req12::StatusT::Replaced)
    {
      // Cancelled but (will be) Replaced, in the Tandem mode:
      a_orig_req->m_ts_end_exch = a_ts_exch;
      a_orig_req->m_ts_end_conn = a_ts_recv;
      a_orig_req->m_status      = Req12::StatusT::Replaced;

      // XXX: However, no Startegy Call-Back is invoked in this case: It will
      // only be done when the Replacement order becomes active...
    }
  }

  //=========================================================================//
  // "OrderRejected":                                                        //
  //=========================================================================//
  // NB: This method is primarily for a New Order being rejected; however, it
  // works correctly in all other cases as well, by invoking  "OrderCancelOr-
  // ReplaceRejected":
  //
  template<QtyTypeT QT, typename QR, typename ProtoEng, typename SessData>
  inline Req12 const& EConnector_OrdMgmt::OrderRejected
  (
    ProtoEng*         a_proto_eng,
    SessData*         a_sess,
    SeqNum            a_sn,           // SeqNum  of the rejected order, mb  0
    OrderID           a_id,           // ClOrdID of the rejected order, non-0
    OrderID           a_aos_id,       // Optional (0 if unknown)
    FuzzyBool         a_non_existent, // Hint from Proto
    int               a_err_code,
    char const*       a_err_text,
    utxx::time_val    a_ts_exch,
    utxx::time_val    a_ts_recv
  )
  {
    RefreshThrottlers(a_ts_recv);

    assert(a_proto_eng != nullptr && a_sess != nullptr);
    //-----------------------------------------------------------------------//
    // Get the "Req12":                                                      //
    //-----------------------------------------------------------------------//
    // Pre-caution:
    if (utxx::unlikely(a_err_text == nullptr))
      a_err_text = "";

    // XXX: How should we look up the orig (failed) Req -- by "a_sn" or by
    // "a_id"? -- We will use both if available,   and verify the results;
    // use Non-Strict version of "GetReq12".  The Kind of failed Req could
    // in general be ANY:
    Req12* req =
      GetReq12<Req12::AnyKindT, false, QT, QR>
        (a_id, a_aos_id, PriceT(), Qty<QT,QR>::Invalid(), "OrderRejected");

    if (req == nullptr)
      req = GetReq12BySeqNum(a_sn, "OrderRejected");

    CHECK_ONLY
    (
      if (utxx::unlikely(req == nullptr))
        // Could not find it at all, which is an error condition:
        throw utxx::badarg_error
              ("EConnector_OrdMgmt::OrderRejected: ArgID=", a_id, " or ArgSN=",
               a_sn, ": Not found");
      else
      if (utxx::unlikely((a_id != 0 && req->m_id     != a_id) ||
                         (a_sn >  0 && req->m_seqNum != a_sn)))
        // Inconsistency in the Req12 found, cannot use it:
        throw utxx::runtime_error
              ("EConnector_OrdMgmt::OrderRejected: Inconsistency: ArgID=",
               a_id, ", ArgSN=",  a_sn, ", StoredID=", req->m_id,
               ", StoredSeqNum=", req->m_seqNum);
    )
    // NB: "req" must be real, not an Indication:
    assert( req != nullptr && req->m_status != Req12::StatusT::Indicated );

    //-----------------------------------------------------------------------//
    // If OK: Which "Req12" was it?                                          //
    //-----------------------------------------------------------------------//
    // NB: NORMALLY, the Req found should be a "New" or "ModLegN", because if a
    // "Cancel", "Modify" or "ModLegC" has failed, another method ("OrderCancel-
    // OrReplaceRejected") would typically be invoked;   BUT this is not always
    // the case (may depend on the underlying Protocol), so take care:
    //
    if (utxx::unlikely(req->m_kind == Req12::KindT::Cancel ||
                       req->m_kind == Req12::KindT::Modify ||
                       req->m_kind == Req12::KindT::ModLegC))
    {
      // This situation should be VERY RARE, so we directly invoke "OrderCan-
      // celOrReplaceRejected" (though the latter will have to do Req look-up
      // again):
      AOS*   aos  = req->m_aos;
      assert(aos != nullptr && (a_aos_id == 0 || a_aos_id == aos->m_id));

      OrderCancelOrReplaceRejected<QT, QR, ProtoEng, SessData>
      (
        a_proto_eng,
        a_sess,
        req->m_id,            // Same as "a_id"
        req->m_origID,        // From Req12 itself, not from the Protocol
        aos->m_id,            // Same 1st Req12 for ThisReq and OrigReq
        nullptr,              // ExchID not known here
        FuzzyBool::UNDEFINED, // Don't know it it was due to a Fill
        a_non_existent,       // Other params are directly translated:
        a_err_code,           //
        a_err_text,           //
        a_ts_exch,            //
        a_ts_recv             //
      );
      return *req;
    }

    //-----------------------------------------------------------------------//
    // Generic Case: a "New" or "ModLegN" order was rejected:                //
    //-----------------------------------------------------------------------//
    assert(req->m_kind == Req12::KindT::New   ||
           req->m_kind == Req12::KindT::ModLegN);

    // Get the AOS:
    AOS*   aos  = req->m_aos;
    assert(aos != nullptr);

    // Mark the Req as "Failed" -- it should NOT  be  "Cancelled", "Replaced",
    // "Failed" or "Filled" yet (but proceed in any case). This happens irres-
    // pective to the Protocol-Level "NonExistent" flag:
    CHECK_ONLY
    (
      if (utxx::unlikely(req->m_status >= Req12::StatusT::Cancelled))
        LOG_WARN(2,
          "EConnector_OrdMgmt::OrderRejected: AOSID={}, ReqID={}: Already {}?",
          aos->m_id, a_id, req->m_status.to_string())
    )
    req->m_status      = Req12::StatusT::Failed;
    req->m_ts_end_exch = a_ts_exch;
    req->m_ts_end_conn = a_ts_recv;

    // Update the AOS "NFails" counter:
    ++(aos->m_nFails);

    // Mark the whole order (AOS) as "Inactive". This also marks all yet-uncon-
    // firmed Reqs (after "req")  as "WillFail":
    MakeAOSInactive<QT, QR, ProtoEng, SessData>
      (a_proto_eng, a_sess, aos, req, "OrderRejected", a_ts_exch, a_ts_recv);

    //-----------------------------------------------------------------------//
    // Call-Backs:                                                           //
    //-----------------------------------------------------------------------//
    // Invoke the RiskMgr -- the corresp order will be removed from potential
    // Risk Srcs:
    if (m_riskMgr != nullptr)
    {
      SecDefD const& instr = *(aos->m_instr);

      // Unwinding a rejected order: IsReal=false; Rej(Old)Qty is the full  qty
      // of the NewOrder which has failed. XXX: Do NOT convert (req->m_qty) di-
      // rectly into double -- this would erase the QR type and possibly result
      // in an invalid conversion!
      double rejQtyD = double(req->GetQty<QT,QR>());

      // NB: For a Reject, NewPx=0 (not NaN):
      m_riskMgr->OnOrder<false>
        (this, instr, aos->m_isBuy, QT, PriceT(0.0), 0.0, req->m_px, rejQtyD,
         utxx::time_val());
    }
    // Log the Rejection as a warning:
    LOG_WARN(2,
      "EConnector_OrdMgmt::OrderRejected: AOSID={}, ReqID={}, Kind={}: "
      "REJECTED: ErrCode={}: {}",
      aos->m_id, a_id, req->m_kind.to_string(), a_err_code, a_err_text)

    // Invoke the Strategy "OnOrderError" Call-Back (unless this AOS is a
    // left-over from a previous invocation). Again, catch possible excep-
    // tions. NB: "ProbFilled" is obviously "false" in this case:
    //
    Strategy* strat = aos->m_strategy;
    if (utxx::likely(strat != nullptr))
      SAFE_STRAT_CB( strat, OnOrderError,
                     (*req,      a_err_code,  a_err_text,  false,
                      a_ts_exch, a_ts_recv) )
    // All Done:
    return *req;
  }

  //=========================================================================//
  // "OrderTraded":                                                          //
  //=========================================================================//
  // NB: This method can also be invoked when Trade event is received from MDC,
  // hence an MDC ptr (may be NULL):
  //
  template<QtyTypeT QT,       typename QR,
           QtyTypeT QTF,      typename QRF,
           typename ProtoEng, typename SessData>
  inline Trade const* EConnector_OrdMgmt::OrderTraded
  (
    ProtoEng*                 a_proto_eng,
    SessData*                 a_sess,
    EConnector_MktData*       a_from_mdc,         // NULL if not from MDC
    OrderID                   a_id,               // ID of the traded order
    OrderID                   a_aos_id,           // Optional
    Req12*                    a_req,              // NULL if not known yet
    FIX::SideT                a_our_side,         // May be UNDEFINED
    FIX::SideT                a_aggr_side,        // May be UNDEFINED
    ExchOrdID const&          a_exch_id,          // May be unavailable (empty)
    ExchOrdID const&          a_mde_id,           // May be unavailable (empty)
    ExchOrdID const&          a_exec_id,          // Normally available
    PriceT                    a_orig_px,          // OrigPx,  may be empty
    PriceT                    a_last_px,          // TradePx, Must be Finite
    Qty<QT, QR>               a_last_qty,         // Must be > 0
    Qty<QT ,QR>               a_leaves_qty,       // <  0 if unknown
    Qty<QT ,QR>               a_cum_qty,          // <= 0 if unknown
    Qty<QTF,QRF>              a_fee,
    int                       a_settl_date,       // As YYYYMMDD
    FuzzyBool                 a_is_filled,        // May be UNDEFINED
    utxx::time_val            a_ts_exch,
    utxx::time_val            a_ts_recv
  )
  {
    RefreshThrottlers(a_ts_recv);
    //-----------------------------------------------------------------------//
    // Top Checks:                                                           //
    //-----------------------------------------------------------------------//
    assert(a_proto_eng != nullptr && a_sess != nullptr);

    LOG_INFO(4,
      "EConnector_OrdMgmt::OrderTraded: FromMDC={}, ReqID={}, StoredReqID={}, "
      "AOSID={}, OurSide={}, AggrSide={}, ExchID={}, MDEntryID={}, ExecID={}, "
      "Price={}, LastPx={}, LastQty={}, LeavesQty={}, CumQty={}, SettlDate={},"
      " IsFilled={}",
      (a_from_mdc != nullptr),                        a_id,
      (a_req      != nullptr) ? a_req->m_id : 0,      a_aos_id,
       char(a_our_side),        char(a_aggr_side),    a_exch_id.ToString(),
       a_mde_id.GetOrderID(),   a_exec_id.ToString(), double(a_orig_px),
       double(a_last_px),       QR(a_last_qty),       QR(a_leaves_qty),
       QR(a_cum_qty),           a_settl_date,         int(a_is_filled))

    // XXX: We currently cannot proceed w/o LastQty or LastPx:
    if (utxx::unlikely(!(IsPos(a_last_qty) && IsFinite(a_last_px))))
    {
      LOG_WARN(1,
        "EConnector_OrdMgmt::OrderTraded: ReqID={}: Invalid: LastQty={}, "
        "LastPx={}: Skipping this Trade", (a_req != nullptr) ? a_req->m_id : 0,
        QR(a_last_qty), double(a_last_px))
      return nullptr;
    }
    //-----------------------------------------------------------------------//
    // Get/Verify the Req12:                                                 //
    //-----------------------------------------------------------------------//
    // If the Order has been Traded, then the corresp Req is at least Confirmed.
    // It can of course only be a "New" or "Modify", not "Cancel":
    //
    if (utxx::likely(a_req == nullptr))
    {
      // In searching for the Req, we should use the LeavesQty as it stood BEF-
      // ORE this Trade, not as a result of it:
      Qty<QT,QR> estPrevLeavesQty =
        utxx::likely(!IsNeg(a_leaves_qty))
        ? (a_leaves_qty + a_last_qty) : Qty<QT,QR>::Invalid();

      constexpr unsigned Mask = unsigned(Req12::KindT::New)    |
                                unsigned(Req12::KindT::Modify) |
                                unsigned(Req12::KindT::ModLegN);
      auto res =
        OrderConfirmedImpl<Mask, QT, QR>
        (
          a_id,   a_aos_id, a_exch_id, a_mde_id,  a_orig_px,
          estPrevLeavesQty, a_ts_exch, a_ts_recv, "OrderTraded"
        );
      a_req = res.first;
      assert(a_req != nullptr && a_req->m_id == a_id);
    }
    else
    {
      // Otherwise, check the consistency of "a_req" and "a_id"; and "a_req"
      // must not be a Cancel, ModLegC or an Indication, of course:
      CHECK_ONLY
      (
        if (utxx::unlikely
           (a_req->m_id     != a_id                     ||
            a_req->m_kind   == Req12::KindT::Cancel     ||
            a_req->m_kind   == Req12::KindT::ModLegC    ||
            a_req->m_status == Req12::StatusT::Indicated))
          throw utxx::badarg_error
                ("EConnector_OrdMgmt::OrderTraded: ReqID Inconsistency: "
                 "Req12ID=", a_req->m_id, ", ProtoID=",  a_id,  ", Kind=",
                 int(a_req->m_kind),      ", Status=",   int(a_req->m_status));
      )
      // If this Req is not marked as "Confirmed" yet, do it now   (though the
      // status will be over-written as "PartFilled" shortly downwards) -- for
      // extra safety:
      if (a_req->m_status < Req12::StatusT::Confirmed)
      {
        a_req->m_status        = Req12::StatusT::Confirmed;
        a_req->m_ts_conf_exch  = a_ts_exch;
        a_req->m_ts_conf_conn  = a_ts_recv;
      }
    }
    // At this point, we must have a valid Req12 with the Status being at least
    // "Confirmed":
    assert(a_req != nullptr && a_req->m_id == a_id);

    //-----------------------------------------------------------------------//
    // Verify that the Req has not already been Filled:                      //
    //-----------------------------------------------------------------------//
    // This may happen, for example, if we infer Fills from Cancel/Replace Fai-
    // lures:
    bool alreadyFilled = (a_req->m_status == Req12::StatusT::Filled);
    bool leavesZero    =  IsZero(a_req->m_leavesQty);
    if  (alreadyFilled || leavesZero)
    {
      CHECK_ONLY
      (
        if (utxx::unlikely(alreadyFilled != leavesZero))
          LOG_WARN(2,
            "EConnector_OrdMgmt::OrderTraded: ReqID={}: Status={} but Leaves"
            "Qty={}", int(a_req->m_status), QR(a_req->GetLeavesQty<QT,QR>()))
      )
      // TODO: Ideally, we may try to find the existing Trade obj, possibly up-
      // date it (eg insert the final TradePx which was not known when we  inf-
      // erred that Fill earlier), adjust the Risks, etc. For now, we just ret-
      // urn NULL -- the CallER must trat this gracefully:
      return nullptr;
    }
    //-----------------------------------------------------------------------//
    // Get the AOS, verify the ExecID, ExchID and the Filled status:         //
    //-----------------------------------------------------------------------//
    // NB: Do NOT update the CumQty in the AOS until the very last stage:
    AOS*   aos =  a_req->m_aos;
    assert(aos != nullptr && (a_aos_id == 0 || a_aos_id == aos->m_id));

    // IMPORTANT: Verify the ExchangeID to avoid spurious Trades: Force=false
    // (consistency check IS performed):
    ApplyExchIDs<false>(a_req, aos, a_exch_id, a_mde_id, "Traded");

    //-----------------------------------------------------------------------//
    // Verify the AOS, SecDef and SettlDate:                                 //
    //-----------------------------------------------------------------------//
    CHECK_ONLY
    (
      // XXX: If the AOS is already Inactive (Filled or Cancelled), or the Req
      // has been Replaced,  we produce a warning,  but otherwise process this
      // Trade as usual.  It may indeed happen  that we got  eg a Cancel event
      // first, and only then a Fill:
      if (utxx::unlikely
         (aos->m_isInactive                            ||
          a_req->m_status == Req12::StatusT::Cancelled ||
          a_req->m_status == Req12::StatusT::Replaced  ||
          a_req->m_status == Req12::StatusT::Failed))
        LOG_WARN(2,
          "EConnector_OrdMgmt::OrderTraded: AOSID={}, ReqID={}: AOS is already"
          " Inactive", aos->m_id, a_req->m_id)

      // NB: "a_our_side" flag, as reported by the underlying Protocol, if not
      // UNDEFINED, must be consistent with the AOS Side. If not, we log an err
      // and set it to the AOS value below:
      if (utxx::unlikely
         ((a_our_side == FIX::SideT::Buy  && !aos->m_isBuy) ||
          (a_our_side == FIX::SideT::Sell &&  aos->m_isBuy) ))
        LOG_ERROR(2,
          "EConnector_OrdMgmt::OrderTraded: AOSID={}, ReqID={}: AOSBuy={}, but"
          " the Protocol says Side={}",
          aos->m_id, a_req->m_id, aos->m_isBuy, char(a_our_side))
    )
    // Then in any case, for extra safety, set "a_our_side" from the AOS:
    a_our_side = (aos->m_isBuy) ? FIX::SideT::Buy : FIX::SideT::Sell;

    // The Instrument Traded:
    assert(aos->m_instr != nullptr);
    SecDefD const& instr = *(aos->m_instr);

    CHECK_ONLY
    (
      // Check "a_aggr_side": For our Passive orders, it should normally be op-
      // posite to "a_our_side"; for our Aggressive orders, the 2 Sides should
      // be the same. However, because the Req12 "m_isAggr" flag is only an in-
      // dication of our intentions, the following is for info only:
      //
      if (utxx::unlikely(m_debugLevel >= 2))
      {
        FIX::SideT shouldBeAggr =
          ((aos->m_isBuy && a_req->m_isAggr) ||
          !(aos->m_isBuy || a_req->m_isAggr))
          ? FIX::SideT::Buy
          : FIX::SideT::Sell;

        if (utxx::unlikely
           (a_aggr_side != FIX::SideT::UNDEFINED &&
            a_aggr_side != shouldBeAggr))
          LOG_INFO(2,
            "EConnector_OrdMgmt::OrderTraded: AOSID={}, ReqID={}: UnExpected "
            "AggrSide: Projected={}, Actual={}",
            aos->m_id, a_req->m_id, char(shouldBeAggr), char(a_aggr_side))
      }
      // Also verify that the SettlDate coming from the Trade itself  is  the
      // same as the one in SecDef. If not, it could be a serious error condi-
      // tion,  because mismatch of SettlDates can severely disrupt all  Risk-
      // Mgmt logic.
      // XXX: Still, in that case, we do not terminate the application immedi-
      // ately, but log the error and use the SettlDate from the SecDef (this
      // is just easier for technical reasons):
      if (utxx::unlikely
         (a_settl_date != 0 && instr.m_SettlDate != a_settl_date))
        LOG_WARN(2,
          "EConnector_OrdMgmt::OrderTraded: AOSID={}, ReqID={}: SettlDate "
          "MisMatch: FromProtocol={}, FromSecDef={}: Will use the latter...",
          aos->m_id, a_req->m_id, a_settl_date, instr.m_SettlDate)
    )
    //-----------------------------------------------------------------------//
    // Decide whether it is a Complete or PartFill:                          //
    //-----------------------------------------------------------------------//
    // XXX: Various data regarding that may be inconsistent,  so the algorithm
    // is a complicated one:
    // OurLeavesQty is the LeavesQty according to our calculations  (assuming
    // that LastQty arg is a valid one -- which might not be the case either):
    //
    Qty<QT,QR> ourLeavesQty = a_req->GetLeavesQty<QT,QR>() - a_last_qty;

    if (utxx::unlikely(IsNeg(ourLeavesQty)))
      ourLeavesQty = Qty<QT,QR>();   // Reset to 0

    // Count "votes" for CompleteFill (only those args which are not UNDEFINED)
    // and for PartFill. Flags coming from the Protocol get weight 2, calculat-
    // ed Flags get weigt 1:
    int votesCompFill =
       int(IsZero(ourLeavesQty))       +
      (int(IsZero(a_leaves_qty)) << 1) +
      (int(a_is_filled  == FuzzyBool::True)  << 1);

    int votesPartFill =
       int(IsPos(ourLeavesQty))        +
      (int(IsPos(a_leaves_qty))  << 1) +
      (int(a_is_filled  == FuzzyBool::False) << 1);

    // Analysis of all possible cases shows that ties are impossible here, so
    // we can make a final decision by voting:
    assert(votesCompFill != votesPartFill);
    bool isCompFill      = (votesCompFill > votesPartFill);

    // However, produce a warning if voting was not unanimous (ie BOTH counts
    // are non-0):
    CHECK_ONLY
    (
      if (utxx::unlikely(votesCompFill != 0 && votesPartFill != 0))
        LOG_WARN(1,
          "EConnector_OrdMgmt::OrderTraded: ReqID={}, ExchOrdOD={}, ExecID={}, "
          "LastQty={}: Conflicting Votes: ForCompFill={}, ForPartFill={}: Proto"
          "LeavesQty={}, ProtoIsFilled={}, OurLeavesQty={}: Decided IsCompFill="
          "{}",
          a_req->m_id,      a_exch_id.ToString(), a_exec_id.ToString(),
          QR(a_last_qty),   votesCompFill,        votesPartFill,
          QR(a_leaves_qty), int(a_is_filled),     QR(ourLeavesQty),  isCompFill)
    )
    // XXX: A very unpleasant situation may(?) potentially occur if we have dec-
    // ided by voting that it is a PartFill, but the computed LeavesQty appears
    // to be 0. In that case, we will still have to declare a CompFill, otherwi-
    // se we would not be able to find a consistent LastQty:
    //
    assert(IsZero(ourLeavesQty) ==
          (a_last_qty >= a_req->GetLeavesQty<QT,QR>()));

    if (utxx::unlikely(!isCompFill && IsZero(ourLeavesQty)))
    {
      CHECK_ONLY
      (
        LOG_WARN(1,
          "EConnector_OrdMgmt::OrderTraded: ReqID={}, ExchOrdID={}, ExecID={}: "
          "Inconsistency: IsCompFill was False but OurLeavesQty=0: Setting "
          "IsCompFill=True",
          a_req->m_id, a_exch_id.ToString(), a_exec_id.ToString())
      )
      isCompFill = true;
    }

    // So: Adjust the params based on the "isCompFill" flag, for consistency:
    if (isCompFill)
    {
      // CompFill: NB: "a_last_qty" may need to be adjusted. In that case, it
      // should be equal to the prev LeavesQty:
      Qty<QT,QR> ourLastQty = a_req->GetLeavesQty<QT,QR>();
      assert(IsPos(ourLastQty));

      CHECK_ONLY
      (
        if (utxx::unlikely(ourLastQty != a_last_qty))
          LOG_WARN(1,
            "EConnector_OrdMgmt::OrderTraded: ReqID={}, ExchOrdID={}, ExecID="
            "{}: IsCompFill=True, so ProtoLastQty={} --> OurLastQty={}",
            a_req->m_id,    a_exch_id.ToString(), a_exec_id.ToString(),
            QR(a_last_qty), QR(ourLastQty))
      )
      if (!IsSpecial0(ourLastQty))
        a_last_qty   = ourLastQty;    // XXX!!!

      a_leaves_qty = Qty<QT,QR>();    // 0
      ourLeavesQty = a_leaves_qty;    // 0
      a_is_filled  = FuzzyBool::True;
    }
    else
    {
      // PartFill: We trust "a_last_qty", which in this case must be consistent
      // (due to above checks) with the prev (a_req->m_leavesQty):
      assert  (IsPos(ourLeavesQty));
      a_leaves_qty = ourLeavesQty;    // > 0
      a_is_filled  = FuzzyBool::False;
    }
    //-----------------------------------------------------------------------//
    // Update the Req12 and AOS Qtys:                                        //
    //-----------------------------------------------------------------------//
    // NOW: Update Req12 and AOS Qtys:
    a_req->m_leavesQty    = QtyU(a_leaves_qty);
    aos  ->m_cumFilledQty = QtyU(aos->GetCumFilledQty<QT,QR>() + a_last_qty);

    // If it was a CompleteFill, mark the Req12 as Filled, and the whole AOS as
    // Inactive:
    if (isCompFill)
    {
      // COMPLETE FILL:
      a_req->m_status     = Req12::StatusT::Filled;
      a_req->m_probFilled = true;
      assert(IsZero(a_req->m_leavesQty));

      // NB: This will also marks all yet-unconfirmed Reqs as "WillFail", so NO
      // need to call "MakePendingReqsFailing" explicitly in this case:
      //
      MakeAOSInactive<QT, QR, ProtoEng, SessData>
        (a_proto_eng, a_sess,   aos, a_req, "Traded-Filled",
         a_ts_exch,   a_ts_recv);
    }
    else
    {
      // PART-FILL:
      a_req->m_status     = Req12::StatusT::PartFilled;
      a_req->m_probFilled = true; // No more prob-filled signals...
      assert(!IsZero(a_req->m_leavesQty));

      if (!m_hasPartFilledModify)
        // Manage unconfirmed Reqs: If Part-Filled Reqs are not Modifyable, any
        // such Reqs will now Fail: IsPartFill=true:
        //
        MakePendingReqsFailing<true, QT, QR, ProtoEng, SessData>
          (a_proto_eng, a_sess, aos, a_req, "Traded-PartFilled",
           a_ts_exch,   a_ts_recv);
    }
    // NB: TimeStamps have already been updated

    //-----------------------------------------------------------------------//
    // Estimate the Trading Fee (if not known yet from the Protocol):        //
    //-----------------------------------------------------------------------//
    if (IsInvalid(a_fee))
    {
      // Will try to calculate the Fee using SecDef info. To do that, first de-
      // termine whether the Trade was agrressive or passive from our side:
      //
      bool ourAggr =
        (a_aggr_side != FIX::SideT::UNDEFINED)
        ? ((a_aggr_side == FIX::SideT::Buy) == aos->m_isBuy)
        : // If AggrSide is not known for any reason, will use an approx method
          // based on our *intentions*:
          a_req->m_isAggr;

      double feeRate =
        ourAggr
        ? instr.m_aggrFeeRate
        : instr.m_passFeeRate;

      if (utxx::likely(IsFinite(feeRate)))
        // Fee is a proportion of the Traded Qty:
        a_fee = QtyConverter<QT,QTF>::template Convert<QR,QRF>
                (a_last_qty, instr, a_last_px);
    }
    // XXX: It may happen that "a_fee" is still undefined -- produce a warning:
    CHECK_ONLY
    (
      if (utxx::unlikely(IsInvalid(a_fee)))
        LOG_WARN(2,
          "EConnector_OrdMgmt::OrderTraded: ReqID={}, ExchOrdID={}, ExecID={}: "
          "UnDefined Fees",
          a_req->m_id, a_exch_id.ToString(), a_exec_id.ToString())
    )
    //-----------------------------------------------------------------------//
    // Create a new "Trade" obj and attach it to the AOS:                    //
    //-----------------------------------------------------------------------//
    // NB:
    // (*) This is done even if "ExecID" is empty -- ONE such Trade can be ins-
    //     talled;
    // (*) Because it is a generic Trade decriptor also used by the MDC, there
    //     is no "OurSide" fld in it -- but there is an "AggrSide";
    // (*) If "NewTrade" returns NULL, it was a repated Trade Event; still, we
    //     process if FURTHER, because it may carry some new attributes (eg the
    //     SureFill flag which was not set before):
    //
    Trade const* tr = NewTrade
    (
      a_from_mdc,  a_req,     a_exec_id, a_last_px, a_last_qty, a_fee,
      a_aggr_side, a_ts_exch, a_ts_recv
    );

    if (utxx::unlikely(tr == nullptr))
     // This ExecID was already installed, so no new Trade was created. This
     // includes the case when ExecID was empty, and only  one empty  ExecID
     // could actually be stored (on a Complete Fill). In  any case, nothing
     // else to do:
     return nullptr;

    // Otherwise, the following invariant must hold:
    assert(tr->m_ourReq == a_req);

    //-----------------------------------------------------------------------//
    // RsikMgr and Strategy Call-Back Invocation:                            //
    //-----------------------------------------------------------------------//
    // XXX: Extenal Call-Backs are invoked  only if a new Trade was registered:
    //
    if (utxx::likely(tr != nullptr))
    {
      if (m_riskMgr != nullptr)
        m_riskMgr->OnTrade(*tr);

      // Should we treat PartFill as yet another Confirm? -- Probably not, so
      // we only invoke one Call-Back:
      Strategy* strat = aos->m_strategy;
      if (utxx::likely(strat != nullptr))
        SAFE_STRAT_CB( strat, OnOurTrade, (*tr) )

      // Log this Trade:
      // (But only AFTER the Strategy Call-Back was invoked -- this is why we
      // were catching possible exceptions there):
      if (utxx::likely(m_tradesLogger != nullptr))
        LogTrade<QT, QR, QTF, QRF>(*tr);
    }
    // All Done ("tr" MAY be NULL at this point):
    return tr;
  }

  //=========================================================================//
  // "OrderCancelOrReplaceRejected":                                         //
  //=========================================================================//
  template<QtyTypeT QT, typename QR, typename ProtoEng, typename SessData>
  inline Req12 const& EConnector_OrdMgmt::OrderCancelOrReplaceRejected
  (
    ProtoEng*         a_proto_eng,
    SessData*         a_sess,
    OrderID           a_rej_id,       // Of the Cancel/Replace     Req
    OrderID           a_orig_id,      // Of the Original (Subject) Req
    OrderID           a_aos_id,       // Optional
    ExchOrdID const*  a_exch_id,      // May be NULL; ortherwise, of OrigReq
    FuzzyBool         a_is_filled,    // Hints from Proto
    FuzzyBool         a_non_existent, // Same
    int               a_err_code,
    char const*       a_err_text,
    utxx::time_val    a_ts_exch,
    utxx::time_val    a_ts_recv
  )
  {
    RefreshThrottlers(a_ts_recv);

    //-----------------------------------------------------------------------//
    // Checks:                                                               //
    //-----------------------------------------------------------------------//
    assert(a_proto_eng != nullptr && a_sess != nullptr);

    // Obviously, "a_is_filled" flag should imply "a_non_existent"; if there is
    // a direct inconsistency, we reset both flags to "UNDEFINED" with an error
    // msg:
    if (a_is_filled == FuzzyBool::True)
    {
      if (utxx::unlikely(a_non_existent == FuzzyBool::UNDEFINED))
        a_non_existent = FuzzyBool::True;
      CHECK_ONLY
      (
      else
      if (utxx::unlikely(a_non_existent == FuzzyBool::False))
      {
        LOG_ERROR(2,
          "EConnector_OrdMgmt::OrderCancelOrReplaceRejected: RejID={}, OrigID="
          "{}, ExchOrdID={}: Inconsistency: Filled=True, NonExistent=False",
          a_rej_id,  a_orig_id,
          (a_exch_id != nullptr) ? a_exch_id->ToString() : "")

        // We don't really know which conflicting flag to trust, so reset both
        // potential offenders to UNDEFINED:
        a_is_filled = a_non_existent = FuzzyBool::UNDEFINED;
      }
      )
    }
    //-----------------------------------------------------------------------//
    // Req12 and AOS:                                                        //
    //-----------------------------------------------------------------------//
    // Get the Reqs. NB: Unlike other methods, here "a_orig_id" may be totally
    // invalid (eg 0) (XXX?); "origReq" is thus obtained using a non-throwing
    // version of "GetReq12", and may be NULL:
    // The Kind of failed Req12 is of course "Cancel", "Modify" or "ModLegC",
    // search is Strict. Px is typically unavailable:
    //
    constexpr unsigned RejMask  = unsigned(Req12::KindT::Cancel) |
                                  unsigned(Req12::KindT::Modify) |
                                  unsigned(Req12::KindT::ModLegC);

    Req12* rejReq = GetReq12<RejMask,  true,  QT, QR>
      (a_rej_id,  a_aos_id,  PriceT(), Qty<QT,QR>::Invalid(),
       "OrderCancelOrReplaceRejected-Rej");
    assert(rejReq != nullptr);    // The search was Strict

    if (a_orig_id == 0)
      // This may happen, eg in TWIME: Never mind:
      a_orig_id = rejReq->m_origID;

    // The OrigReq could be "New", "Modify" or "ModLegN", but not "Cancel" or
    // "ModLegC", and here search is Non-Strict:
    constexpr unsigned OrigMask = unsigned(Req12::KindT::New)    |
                                  unsigned(Req12::KindT::Modify) |
                                  unsigned(Req12::KindT::ModLegN);
    Req12* origReq =
      GetReq12<OrigMask, false>
        (a_orig_id,  a_aos_id, PriceT(), Qty<QT,QR>::Invalid(),
         "OrderCancelOrReplaceRejected-Orig");
    // NB: "origReq" may still be NULL: the search was Non-Strict

    CheckOrigID(*rejReq, a_orig_id, "OrderCancelOrReplaceRejected");

    // Both "Req"s must point to the same AOS (non-NULL):
    AOS* aos =
      (utxx::likely(origReq != nullptr))
      ? GetSameAOS(*rejReq, *origReq, "OrderCancelOrReplaceRejected")
      : rejReq->m_aos;
    assert(aos != nullptr && (a_aos_id == 0 || a_aos_id == aos->m_id));

    // And both Reqs (if available) must be real, not Indications:
    assert(rejReq ->m_status  != Req12::StatusT::Indicated &&
          (origReq == nullptr ||
           origReq->m_status  != Req12::StatusT::Indicated));

    // Verify the ExchangeID to avoid spurious Rejects   (whether "a_exch_id"
    // is NULL/empty or not in this case, depends on the underlying protocol;
    // in most cases, we expect it to be non-NULL but empty): Force=false, so
    // the consistency check IS performed (but only for ExchID,  not for  the
    // MDEntryID -- XXX the latter is not provided to this method):
    //
    if (utxx::likely(a_exch_id != nullptr))
      ApplyExchIDs<false>
        (origReq, aos, *a_exch_id, EmptyExchOrdID,
         "OrderCancelOrReplaceRejected");

    // In any case, mark "rejReq" as "Failed" (XXX: we don't check its curr
    // status, it probably does not matter):
    rejReq->m_status       = Req12::StatusT::Failed;
    rejReq->m_ts_end_exch  = a_ts_exch;
    rejReq->m_ts_end_conn  = a_ts_recv;

    //-----------------------------------------------------------------------//
    // So: Should we interpret this as a Fill?                               //
    //-----------------------------------------------------------------------//
    // Do so if:
    // (*) "a_is_filled" and "a_non_existent" are NOT explicitly set to False;
    // (*) the dynamic mode is set which requires us to interpret CR Failures
    //     as Fills;
    // (*) XXX:  We do not require that "origReq" is Confirmed, as in Non-Pipe-
    //     Lined mode it would be anyway (otherwise we would not try to Cancel
    //     or Replace it at all), and in PipeLined mode  we may  theoretically
    //     receive a CR failure even before its Confirmation;   HOWEVER, we do
    //     require that "origReq" is at least non-NULL, ie CR failure is not a
    //     result of simply specifying a wrong OrigID;
    // (*) "origReq" has not already been Cancelled, Replaced, Filled, or had
    //     Failed;
    // (*) "origReq" has a finite Px,  because in inferring the Fill, we have
    //     no option but to assume that the Fill has occurred at that Px  (in
    //     fact, it may have occurred at that or a BETTER Px!).
    // NB: "InferFill" is an edge-like flag which is activated only once;  we
    //     then set the "m_probFilled" flag in the Req12, which prevents this
    //     event from being signalled again:
    //
    bool inferFill =
         a_is_filled    != FuzzyBool::False            &&
         a_non_existent != FuzzyBool::False            &&
         origReq  != nullptr                           &&
       !(origReq->m_probFilled)                        &&
         origReq->m_status < Req12::StatusT::Cancelled &&
         IsFinite(origReq->m_px);

    // Do not raise this flag again:
    if (utxx::likely(origReq != nullptr))
      origReq->m_probFilled |= inferFill;

    // The "inferFill" flag will be passed to the Strategy Call-Back; it is up
    // to the Strategy to decide what to do in this case...

    //-----------------------------------------------------------------------//
    // Still, handle the Failure itself:                                     //
    //-----------------------------------------------------------------------//
    // Update the AOS "NFails" counter (XXX: even if "inferFill" is set?):
    ++(aos->m_nFails);

    // If the underlying Protocol tells us that the order does not exist any-
    // more, and the AOS is not yet marked as Inactive,  we will mark  it as
    // such (the actual Fill etc event may arrive later -- for now, we don't
    // know why it is Inactive):
    // NB: "a_non_existent" does not mean that the order to cancel/modify has
    // not been found; it really means  that the order DID exist, but not any-
    // more (so we would not mark the AOS as inactive simply because  we were
    // trying to cancel a wrong Req12):
    //
    if (utxx::unlikely
       (a_non_existent == FuzzyBool::True && !(aos->m_isInactive)))
      // Mark the AOS as Inactive, and all unconfirmed Reqs as "WillFail":
      MakeAOSInactive<QT, QR, ProtoEng, SessData>
        (a_proto_eng, a_sess, aos, rejReq, "OrderCancelOrReplaceRejected",
         a_ts_exch,   a_ts_recv);

    if (!(aos->m_isInactive))
      // Manage unconfirmed Reqs explicitly: IsPartFill=false:
      MakePendingReqsFailing<false, QT, QR, ProtoEng, SessData>
        (a_proto_eng, a_sess, aos, rejReq, "OrderCancelOrReplaceRejected",
         a_ts_exch,   a_ts_recv);

    //-----------------------------------------------------------------------//
    // Which order has actually failed -- "Cancel"/"ModLegC" or "Modify"?    //
    //-----------------------------------------------------------------------//
    if (utxx::likely(rejReq->m_kind == Req12::KindT::Modify))
    {
      //---------------------------------------------------------------------//
      // "Modify" ("Replace") has failed:                                    //
      //---------------------------------------------------------------------//
      // In this case, we DO need to invoke the RiskMgr and "unwind" the poten-
      // tial effects of the "RejReq".
      // XXX: But if we don't have the "OrigReq", or there is an inconsistency
      // about it, then we don't do the unwinding --  this should be rare, and
      // in general should not have much adverse effects on Risk Mgmt:
      //
      if ((m_riskMgr != nullptr && origReq != nullptr &&
           rejReq->m_origID     == origReq->m_id))
      {
        SecDefD const& instr = *(aos->m_instr);

        // NB: Because "Modify" has failed, the new qty is the (now restored)
        // OrigQty, and the old qty was the rejected modified qty. XXX: Again,
        // do NOT perform conversions directly into "double"  by-passing  Qty,
        // this would erase the correct QR rep!
        //
        double origQtyD = double(origReq->GetQty<QT,QR>());
        double  rejQtyD = double(rejReq ->GetQty<QT,QR>());

        m_riskMgr->OnOrder<false>
          (this,  instr, aos->m_isBuy, QT, origReq->m_px, origQtyD,
           rejReq->m_px, rejQtyD,      utxx::time_val());
      }
    }
    else
    if (utxx::unlikely(rejReq->m_kind == Req12::KindT::Cancel ||
                       rejReq->m_kind == Req12::KindT::ModLegC))
    {
      //---------------------------------------------------------------------//
      // "Cancel" has failed:                                                //
      //---------------------------------------------------------------------//
      // In this case, reset the "CxlPending" flag. XXX: Should we scan other
      // Reqs to see if there was another Cancel req, so "CxlPending"  should
      // still be on? But this must not be the case -- we would not do it  if
      // there was already one pending -- so no need to scan.   There is also
      // no need to invoke the RiskMgr, because it was not invoked when Cancel
      // req was submitted, either:
      //
      if (aos->m_isCxlPending == rejReq->m_id) // Check this for extra safety!
      {
        // This could only have happened if it was a "Cancel", not "ModLegC":
        assert(rejReq->m_kind == Req12::KindT::Cancel);
        aos->m_isCxlPending = 0;
      }
      // XXX: A BIG question is: should we declare the Order to be Inactive if
      // "Cancel" has failed? Intuitively, it seems to be the case,  but there
      // are important counter-examples: Eg:
      // (*) NewOrder with ReqID1;
      //     success;
      // (*) Modify :      ReqID1->ReqID2
      //     failed, eg because NewPx was out of range or so; but the Order
      //     still exists as ReqID1;
      // (*) Cancel (as ReqID2, because we don't know yet that Modify failed):
      //     this will obviously fail, but the Order is NOT Inactive!
      // So DON'T mark it as Inactive until we get a confirmation of Cancel...
    }
    // Other Kinds should not occur -- but in any case, the failed req's Kind
    // will be logged below -- no formal assert here...
    //-----------------------------------------------------------------------//
    // Logging and Strategy Call-Back:                                       //
    //-----------------------------------------------------------------------//
    // Log the Rejection as a warning:
    LOG_WARN(2,
      "EConnector_OrdMgmt::OrderCancelOrReplaceRejected: AOSID={}, ReqID={}, "
      "Kind={}: REJECTED: ErrCode={}: {}{}{}",
      aos->m_id,  rejReq->m_id, rejReq->m_kind.to_string(), a_err_code,
      (a_err_text != nullptr) ? a_err_text : "",
      (a_non_existent == FuzzyBool::True)  ? ": Order NonExistent" : "",
      aos->m_isInactive                    ? ": Order Inactive"    : "")

    // UNLESS it was an InferredFill, notify the Strategy  -- this is an Error
    // event actually (again, beware of this AOS being a left-over from a prev-
    // ious invocation, and catch possible exceptions):
    Strategy* strat = aos->m_strategy;

    if (!inferFill && strat != nullptr)
      SAFE_STRAT_CB( strat, OnOrderError,
                    (*rejReq,   a_err_code, a_err_text, inferFill,
                     a_ts_exch, a_ts_recv) )
    // All Done!
    return *rejReq;
  }

  //=========================================================================//
  // "ReqRejectedBySession":                                                 //
  //=========================================================================//
  template<QtyTypeT QT, typename QR, typename ProtoEng, typename SessData>
  inline Req12 const& EConnector_OrdMgmt::ReqRejectedBySession
  (
    ProtoEng*         a_proto_eng,
    SessData*         a_sess,
    bool              a_by_sn,      // SeqNum or ClOrdID?
    long              a_id,         // Semantics according to the above flag
    OrderID           a_aos_id,     // Optional
    char const*       a_reject_reason,
    utxx::time_val    a_ts_exch,    // Or the best known approximation
    utxx::time_val    a_ts_recv
  )
  {
    RefreshThrottlers(a_ts_recv);

    assert(a_proto_eng != nullptr && a_sess != nullptr);
    //-----------------------------------------------------------------------//
    // Get the Reqs and AOS involved:                                        //
    //-----------------------------------------------------------------------//
    // Find the rejected Req (non-strict), It can be of ANY Kind:
    Req12* rejReq =
      a_by_sn
      ? GetReq12BySeqNum(SeqNum (a_id),       "ReqRejectedBySession")
      : GetReq12<Req12::AnyKindT, false,  QT, QR>
          (OrderID(a_id), a_aos_id, PriceT(), Qty<QT,QR>::Invalid(),
           "ReqRejectedBySession");

    CHECK_ONLY
    (
      if (utxx::unlikely(rejReq == nullptr))
        // Rejected Req not found. This may be a serious error condition because
        // it can lead to order state corruption, OR  it may simply mean that we
        // eg have received a ResendRequest for SeqNums which we have never sent
        // in first place. Still throw an exception in this case:
        throw utxx::badarg_error
          ("EConnector_OrdMgmt::ReqRejectedBySession: ",
          (a_by_sn ? "SeqNum=" : "ReqID="), a_id, ": Failed Req Not Found");
    )
    // Generic Case: Yes, found the failed Req:
    AOS*   aos = rejReq->m_aos;
    assert(aos != nullptr && (a_aos_id == 0 || a_aos_id == aos->m_id));

    // The failed Req must be real, not an Indication:
    assert(rejReq->m_status != Req12::StatusT::Indicated);

    // Mark the "rejReq" as "Failed" -- produce a warning if it already has an-
    // other "inactive" status (but still proceed):
    CHECK_ONLY
    (
      if (utxx::unlikely(rejReq->m_status >= Req12::StatusT::Cancelled))
        LOG_WARN(2,
          "EConnector_OrdMgmt::ReqRejectedBySession: AOSID={}, ReqID={}: "
          "Already {}?", aos->m_id, rejReq->m_id, rejReq->m_status.to_string())
    )
    rejReq->m_status      = Req12::StatusT::Failed;
    rejReq->m_ts_end_exch = a_ts_exch;
    rejReq->m_ts_end_conn = a_ts_recv;

    // Update the AOS "NFails" counter:
    ++(aos->m_nFails);

    // Similar to "OrderCancelOrReplaceRejected",  the problem is whether the
    // whole order has failed as well.  Because the error has occurred at the
    // Session level, we consider the whole order to be failed only if it was
    // a "New".
    // And we need to invoke the RiskMgr for "New" and "Modify" (not for "Can-
    // cel" because there is no  RiskMgr invocation at  the beginning of "Can-
    // cel"):
    SecDefD const& instr = *(aos->m_instr);

    //-----------------------------------------------------------------------//
    if (utxx::likely(rejReq->m_kind == Req12::KindT::Modify))
    //-----------------------------------------------------------------------//
    {
      // Get the OrigReq -- it MUST be present, and its Kind could be New or
      // Modify; Strict=true, Px is typically unavailable:
      constexpr unsigned OrigMask = unsigned(Req12::KindT::New)   |
                                    unsigned(Req12::KindT::Modify);
      Req12* origReq =
        GetReq12<OrigMask, true>
          (rejReq->m_origID,  a_aos_id, PriceT(), Qty<QT,QR>::Invalid(),
           "ReqRejectedBySession");
      assert(origReq != nullptr);   // Search was Strict

      // Unwinding a failed "Modify": IsReal=false:
      if (m_riskMgr != nullptr)
      {
        // Similar to "OrderCancelOrReplaceRejected", the new qty is the (now
        // restored) OrigQty, and the old qty was the rejected modified qty.
        // Again, conversions to double MUST go via Qty<QT,QR>!
        //
        double origQtyD = double(origReq->GetQty<QT,QR>());
        double  rejQtyD = double(rejReq ->GetQty<QT,QR>());

        m_riskMgr->OnOrder<false>
          (this,     instr,        aos->m_isBuy, QT,     origReq->m_px,
           origQtyD, rejReq->m_px, rejQtyD,      utxx::time_val());
      }
    }
    else
    //-----------------------------------------------------------------------//
    if (utxx::unlikely(rejReq->m_kind == Req12::KindT::New    ||
                       rejReq->m_kind == Req12::KindT::ModLegN))
    //-----------------------------------------------------------------------//
    {
      // There is no OrigReq in these cases:
      assert(rejReq->m_origID == 0);

      if (!(aos->m_isInactive))
        // When this AOS is marked as Inactive, all pending Reqs are marked as
        // "WillFail". XXX: If it was already Inactive for any reason, no need
        // to do anything:
        MakeAOSInactive<QT, QR, ProtoEng, SessData>
          (a_proto_eng, a_sess, aos, rejReq, "OrderCancelOrReplaceRejected",
           a_ts_exch,   a_ts_recv);

      if (utxx::likely(m_riskMgr != nullptr))
      {
        // Unwinding a failed "New":  IsReal=false, NewQty=0, OldQty=FailedQty.
        // Again, conversion to double is to go via Qty<QT,QR>!
        double failedQtyD = double(rejReq->GetQty<QT,QR>());

        // NB: For a SessionReject, NewPx=0 (not NaN):
        m_riskMgr->OnOrder<false>
          (this, instr, aos->m_isBuy,  QT, PriceT(0.0), 0.0, rejReq->m_px,
           failedQtyD,  utxx::time_val());
      }
    }
    else
    //-----------------------------------------------------------------------//
    // Otherwise, a "Cancel" or "ModLegC" was rejected by Session:           //
    //-----------------------------------------------------------------------//
    {
      assert(rejReq->m_kind == Req12::KindT::Cancel ||
             rejReq->m_kind == Req12::KindT::ModLegC);

      // Same action as in "OrderCancelOrReplaceRejected": Clear the possible
      // "CxlPending" flag of the AOS, but nothing else:
      //
      if (aos->m_isCxlPending == rejReq->m_id) // Check this for extra safety!
      {
        // This could only have happened if it was a "Cancel", not "ModLegC":
        assert(rejReq->m_kind == Req12::KindT::Cancel);
        aos->m_isCxlPending = 0;
      }
    }

    //-----------------------------------------------------------------------//
    // Need to manage the pending Reqs explicitly: IsPartFill=false:         //
    //-----------------------------------------------------------------------//
    if (!(aos->m_isInactive))
      MakePendingReqsFailing<false, QT, QR, ProtoEng, SessData>
        (a_proto_eng, a_sess, aos, rejReq, "ReqRejectedBySession",
         a_ts_exch,   a_ts_recv);

    //-----------------------------------------------------------------------//
    // Logging and Strategy Notification:                                    //
    //-----------------------------------------------------------------------//
    // Log the Rejection as a warning:
    LOG_WARN(2,
      "EConnector_OrdMgmt::ReqRejectedBySession: AOSID={}, ReqID={}, Kind={}"
      ": REJECTED: {}{}",
      aos->m_id, rejReq->m_id, rejReq->m_kind.to_string(),
      (a_reject_reason  != nullptr) ? a_reject_reason : "",
      aos->m_isInactive ? ": Order Inactive" : "")

    // Notify the Strategy -- this is an Error event; there is no ErrCode,   so
    // use 0;  beware of this AOS being a left-over from a previous invocation;
    // catch possible exceptions; "ProbFilled" is "false" here:
    //
    Strategy* strat = aos->m_strategy;
    if (utxx::likely(strat != nullptr))
      SAFE_STRAT_CB( strat, OnOrderError,
                    (*rejReq, 0, a_reject_reason, false, a_ts_exch, a_ts_recv) )
    // All Done!
    return *rejReq;
  }

  //=========================================================================//
  // Accessors:                                                              //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "GetPosition":                                                          //
  //-------------------------------------------------------------------------//
  inline double EConnector_OrdMgmt::GetPosition(SecDefD const* a_sd) const
  {
    auto pos = m_positions.find(a_sd);
    if (pos != m_positions.end())
      return pos->second;
    else
      return NaN<double>;
  }

  //-------------------------------------------------------------------------//
  // "GetBalance":                                                           //
  //-------------------------------------------------------------------------//
  inline double EConnector_OrdMgmt::GetBalance (SymKey a_ccy)  const
  {
    auto bal = m_balances.find(a_ccy);
    if (bal != m_balances.end())
      return bal->second;
    else
      return NaN<double>;
  }
}
// End namespace MAQUETTE
