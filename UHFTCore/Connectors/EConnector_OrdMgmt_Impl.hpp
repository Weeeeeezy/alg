// vim:ts=2:et
//===========================================================================//
//                    "Connectors/EConnector_OrdMgmt_Impl.hpp":              //
//                      Templated and Inlined Internal Utils                 //
//===========================================================================//
#pragma  once

#include "Basis/Maths.hpp"
#include "Connectors/EConnector_OrdMgmt.h"
#include "Connectors/EConnector_MktData.h"
#include "InfraStruct/RiskMgr.h"
#include <utxx/compiler_hints.hpp>
#include <utxx/error.hpp>
#include <utxx/convert.hpp>
#include <utility>
#include <algorithm>
#include <cstdlib>

namespace MAQUETTE
{
  //=========================================================================//
  // "NewAOS":                                                               //
  //=========================================================================//
  // Allocate and initialise a new "AOS" object; Same args as for "AOS" Ctor,
  // except for OrdID which is automatically determined    (same as the slot
  // index from which this "AOS"  is allocated). Inlined for efficiency:
  //
  inline AOS* EConnector_OrdMgmt::NewAOS
  (
    Strategy*         a_strategy,
    SecDefD const*    a_instr,
    bool              a_is_buy,
    FIX::OrderTypeT   a_order_type,
    bool              a_is_iceberg,
    FIX::TimeInForceT a_time_in_force,
    int               a_expire_date
  )
  {
    assert(a_strategy != nullptr && a_instr != nullptr);

    // Do we still have room for allocating a new "AOS"? NB: "m_OrdN" is the idx
    // not taken yet, so:
    assert(*m_OrdN <= m_maxAOSes);
    CHECK_ONLY
    (
      if (utxx::unlikely(*m_OrdN == m_maxAOSes))
        throw utxx::runtime_error
              ("EConnector_OrdMgmt::NewAOS: ", m_name, ": No more space: "
               "AOSID=", *m_OrdN);
    )
    // Because slots are allocated sequentially, the next slot must be empty
    // yet:
    OrderID ordID = (*m_OrdN)++;   // NB: Post-Increment!
    assert (ordID >= 1);
    AOS*    aos   = m_AOSes + ordID;
    assert (aos->m_id == 0 && aos->m_instr == nullptr && ordID >= 1);

    // Run the "AOS" Ctor on "aos":
    new (aos) AOS
    (
      m_qt,    m_withFracQtys, a_strategy,   ordID,        a_instr,
      this,    a_is_buy,       a_order_type, a_is_iceberg, a_time_in_force,
      a_expire_date
    );
    return aos;
  }

  //=========================================================================//
  // "NewReq12":                                                             //
  //=========================================================================//
  // Allocate and initialise a new "Req12" object; Same args as for "Req12"
  // Ctor, except for ReqID which is automatically determined  (same as the
  // slot index from which this "Req12" is allocated):
  //
  template<QtyTypeT QT, typename QR, bool AttachToAOS = true>
  inline Req12* EConnector_OrdMgmt::NewReq12
  (
    AOS*            a_aos,
    OrderID         a_orig_id,    // 0 for New, > 0 otherwise
    Req12::KindT    a_req_kind,
    PriceT          a_px,
    bool            a_is_aggr,
    Qty<QT,QR>      a_qty,
    Qty<QT,QR>      a_qty_show,
    Qty<QT,QR>      a_qty_min,
    bool            a_peg_side,
    double          a_peg_offset,
    utxx::time_val  a_ts_md_exch,
    utxx::time_val  a_ts_md_conn,
    utxx::time_val  a_ts_md_strat,
    utxx::time_val  a_ts_created
  )
  {
    // Do we still have room for allocating a new "Req12"? NB: "m_reqN" is the
    // idx not taken yet, so:
    assert(*m_ReqN <= m_maxReq12s);
    CHECK_ONLY
    (
      if (utxx::unlikely(*m_ReqN == m_maxReq12s))
        throw utxx::runtime_error
              ("EConnector_OrdMgmt::NewReq12: ", m_name,
               ": No more space: ReqID=",       *m_ReqN);
    )
    OrderID reqID = (*m_ReqN)++;   // NB: Post-Increment!
    assert (1 <= reqID && reqID < *m_ReqN);
    Req12*  req   = m_Req12s + reqID;
    // Because slots are allocated sequentially, the next slot must be empty
    // yet:
    assert (req->m_id == 0 && req->m_aos == nullptr);

    // Run the "Req12" Ctor on "req":
    new (req)   Req12
    (
      a_aos,       AttachToAOS,  reqID,        a_orig_id,    a_req_kind,
      a_px,        a_is_aggr,    a_qty,        a_qty_show,   a_qty_min,
      a_peg_side,  a_peg_offset, a_ts_md_exch, a_ts_md_conn, a_ts_md_strat,
      a_ts_created
    );
    assert(req->m_id    == reqID  && req->m_aos == a_aos &&
          (!AttachToAOS || a_aos->m_lastReq     == req));

    // All Done:
    return req;
  }

  //=========================================================================//
  // "NewTrade":                                                             //
  //=========================================================================//
  // NB: This is OUR OWN Trade. It would normally come from this  OMC ("Order-
  // Traded" method), or possibly from and MDC -- in any case, we register  it
  // here, and the OMC ptr is "this" OMC. Returns "true" iff the Trade was not
  // previously recorded, ie it is indeed a NEW one, and needs to be fully pro-
  // cessed:
  template<QtyTypeT QT, typename QR, QtyTypeT QTF, typename QRF>
  inline Trade const*   EConnector_OrdMgmt::NewTrade
  (
    EConnector_MktData* a_mdc,       // May be NULL
    Req12     const*    a_req,
    ExchOrdID const&    a_exec_id,   // May be empty
    PriceT              a_px,
    Qty<QT, QR>         a_qty,
    Qty<QTF,QRF>        a_fee,
    FIX::SideT          a_aggressor,
    utxx::time_val      a_ts_exch,
    utxx::time_val      a_ts_recv
  )
  {
    //-----------------------------------------------------------------------//
    // Curr Order Status:                                                    //
    //-----------------------------------------------------------------------//
    assert(a_req != nullptr);
    AOS*   aos = a_req->m_aos;
    assert(aos   != nullptr);

    // We must have a valid Px and Qty:
    assert(IsFinite(a_px) && IsPos(a_qty));

    //-----------------------------------------------------------------------//
    // Check that the ExecID is unique (ie the Trade not recorded yet):      //
    //-----------------------------------------------------------------------//
    // XXX: However, ExecID may be empty in 2 cases:
    // (*)  The Trade is inferred from MDC data, which we only do if it is a
    //      CompleteFill (othrtwise TradeQty is unknown); in this case, only
    //      1 Trade with empty ExecID is possible, so we check ExecID for
    //      uniqueness in a normal way;
    // (*)  Or the ExecIDs are not provided by the underlying Protocol at all,
    //      which is very unusual but possible. In that case, skip the check:
    CHECK_ONLY
    (
      if (m_hasExecIDs)
        for (Trade const* tr = aos->m_lastTrd; tr != nullptr; tr = tr->m_prev)
          if (utxx::unlikely(tr->m_execID == a_exec_id))
          {
            LOG_INFO(3,
              "EConnector_OrdMgmt::NewTrade: ReqID={}, ExecID={}, FromMDC={}: "
              "Already recorded",
              a_req->m_id, a_exec_id.GetOrderID(), (a_mdc != nullptr))
            return nullptr;
          }
    )
    //-----------------------------------------------------------------------//
    // If we got here, this Trade was NOT recorded yet -- proceed further:   //
    //-----------------------------------------------------------------------//
    // Do we still have room for allocating a new "Trade"? NB: "m_TrdN" is the
    // idx not taken yet, so:
    assert(*m_TrdN <= m_maxTrades);
    CHECK_ONLY
    (
      if (utxx::unlikely(*m_TrdN == m_maxTrades - 1))
        throw utxx::runtime_error
              ("EConnector_OrdMgmt::NewTrade: ", m_name,
               ": No more space: TrdN=",        *m_TrdN);
    )
    // Get the curr "Trade" slot, and Post-Increment the idx:
    OrderID id = (*m_TrdN)++;
    Trade*  tr = m_Trades + id;

    SecDefD const*  instr = aos->m_instr;
    assert(instr != nullptr);

    // Run the "Trade" Ctor on "tr" (OMC = this). Because it is our own Trade,
    // we put AccountID=0, and include OurSide:
    //
    FIX::SideT ourSide = aos->m_isBuy ? FIX::SideT::Buy : FIX::SideT::Sell;
    new (tr) Trade
    (
      id,   a_mdc, instr,   a_req,  0, a_exec_id, a_px, a_qty, a_fee,
      a_aggressor, ourSide, a_ts_exch, a_ts_recv
    );

    // Attach it to the top of AOS-rooted list:
    Trade const* curr = aos->m_lastTrd;
    if (curr != nullptr)
      curr->m_next    = tr;
    assert(tr->m_next == nullptr);

    tr->m_prev        = curr;
    aos->m_lastTrd    = tr;

    // OK, a new Trade was successfully created and attached:
    return tr;
  }

  //=========================================================================//
  // "GetReq12":                                                             //
  //=========================================================================//
  // (*) In the "Strict" mode, this method throws an exception if the reques-
  //     ted Req could not be found, and returns a non-NULL ptr otherwise;
  // (*) but there is also a non-Strict version which returns  NULL  in  the
  //     "not found" case:
  //
  template<unsigned Mask, bool Strict, QtyTypeT QT, typename QR>
  inline Req12* EConnector_OrdMgmt::GetReq12
  (
    OrderID     a_id,                     // Normally non-0
    OrderID     a_aos_id,                 // May be 0
    PriceT      a_px,                     // May be empty
    Qty<QT,QR>  CHECK_ONLY(a_leaves_qty), // May be empty
    char const* a_where
  )
  const
  {
    //-----------------------------------------------------------------------//
    // Checks:                                                               //
    //-----------------------------------------------------------------------//
    // Obviously, if we are searching for a "Cancel" or "ModLegC" Req ONLY, the
    // Px should NOT be given:
    constexpr unsigned MaskC  = unsigned(Req12::KindT::Cancel) |
                                unsigned(Req12::KindT::ModLegC);
    constexpr bool     IsAnyC = (Mask & MaskC) != 0;
    assert(!(IsFinite(a_px) &&  IsAnyC));

    Req12* req = nullptr;   // Will-be-result

    //-----------------------------------------------------------------------//
    // Generic case: Get "req" by "a_id":                                    //
    //-----------------------------------------------------------------------//
    // Obviously, a_id==0 is excluded:
    //
    if (utxx::likely(a_id > 0))
    {
      // Then the valid range for currently-available ReqIDs is
      // 1 .. (*m_ReqN)-1,
      // ie    *m_reqN  is the nearest FRESH slot. Thus:
      CHECK_ONLY
      (
        if (utxx::unlikely
           (*m_ReqN <= 0 || *m_ReqN > m_maxReq12s || a_id >= *m_ReqN))
          throw utxx::logic_error
                ("EConnector_OrdMgmt::GetReq12(", a_where, "): Inconsistency: "
                 "ReqID=",   a_id, ", ReqN=",    *m_ReqN,  ", MaxReq12s=",
                 m_maxReq12s);
      )
      // OK, "a_id" is within the valid range:
      req = m_Req12s + a_id;

      // Check the Req12 for validity:
      CHECK_ONLY
      (
        if (utxx::unlikely(req->m_id != a_id || req->m_aos == nullptr))
          throw utxx::logic_error
                ("EConnector_OrdMgmt::GetReq12(", a_where, "): Invalid Req12 "
                 "@ ID=", a_id, ": StoredID=", req->m_id);

        // If, in addition, "a_aos_id" is valid, it must match the real AOSID:
        if (utxx::unlikely(a_aos_id != 0 && req->m_aos->m_id != a_aos_id))
          throw utxx::logic_error
                ("EConnector_OrdMgmt::GetReq12(", a_where, "): ReqID=", a_id,
                 " and AOSID=", a_aos_id, " are Inconsistent: StoredAOSID=",
                 req->m_aos->m_id);
      )
    }
    else
    //-----------------------------------------------------------------------//
    // Special Case: "a_id" is unavailable; try to use "a_aos_id":           //
    //-----------------------------------------------------------------------//
    // But we can only do this if we are looking for a "Cancel" Req, which is
    // most likely unique for its AOS.  We use "a_aos_id" to find the corresp
    // AOS, and then find the latest "Cancel" Req.  XXX: This is still error-
    // prone in some marginal situations.
    // NB: Notionally, this case is activated by EITHER  "Cancel" or "ModLegC"
    // (because that's the mask which would be used by the Caller), but we are
    // only looking for a "Cancel" Req here because, unlike "ModLegC", it must
    // be unique. So do not even enter this search if "ModLegC" Reqs are allo-
    // wed, ie if Modify is non-Atomic:
    // XXX: Here we need "m_hasAtomicModify" fld, as there is no ProtoEng!
    //
    if (IsAnyC && m_hasAtomicModify && 1 <= a_aos_id && a_aos_id < *m_OrdN)
    {
      assert(a_aos_id <  m_maxAOSes);
      AOS* aos = m_AOSes + a_aos_id;

      // Check the AOS for validity:
      CHECK_ONLY
      (
        if (utxx::unlikely(aos->m_id != a_aos_id))
          throw utxx::logic_error
                ("EConnector_OrdMgmt::GetReq12(", a_where, "): Invalid AOS @ "
                 "ID=", a_aos_id,  ": StoredID=", aos->m_id);
      )
      // Because we are really looking for a "Cancel" Req, the AOS should norm-
      // ally be in the "CxlPending" state -- if not, do not waste time search-
      // ing for that Req12:
      //
      if (utxx::likely(aos->m_isCxlPending))
        for (req = aos->m_lastReq; req != nullptr; req = req->m_prev)
          // IMPORTANT: We are looking for a Req for which we received a respo-
          // nce from the Exchange, so it must NOT be an Indication:
          if (req->m_kind   == Req12::KindT::Cancel    &&
              req->m_status != Req12::StatusT::Indicated)
            break;
    }
    //-----------------------------------------------------------------------//
    // Have we got a non-NULL result? If so, check Kind, Px and LeavesQty:   //
    //-----------------------------------------------------------------------//
#   if !UNCHECKED_MODE
    if (utxx::likely(req != nullptr))
    {
      // First of all, the following invariants must hold regarding the Kind
      // and the Qtys in any case -- whether the Req is the one we sought or
      // not:
      Qty<QT,QR> reqQty       = req->GetQty      <QT,QR>();
      Qty<QT,QR> reqLeavesQty = req->GetLeavesQty<QT,QR>();

      bool       isAnyC   = (req->m_kind   == Req12::KindT::Cancel    ||
                             req->m_kind   == Req12::KindT::ModLegC);
      bool       noTrades = (req->m_status <  Req12::StatusT::PartFilled);
      bool       isFilled = (req->m_status == Req12::StatusT::Filled);

      if (utxx::unlikely
         (( isAnyC       && (!IsSpecial0(reqQty)     ||
                             !IsSpecial0(reqLeavesQty)))              ||
          (!isAnyC       &&
            (!IsPos(reqQty) || IsNeg(reqLeavesQty)   || reqQty < reqLeavesQty ||
            (noTrades    && reqLeavesQty != reqQty)  ||
            (isFilled    && !IsZero(reqLeavesQty)))) ))
        throw utxx::logic_error
              ("EConnector_OrdMgmt::GetReq12(", a_where,   "), ID=", req->m_id,
               ": Inconsistency: Kind=",        int(req->m_kind), ", Qty=",
               QR(req->GetQty<QT,QR>()),        ", LeavesQty=",
               QR(req->GetLeavesQty<QT,QR>()),  ", Status=",
               int(req->m_status));

      // Now check for other search constraints: Kind and Px must match:
      bool kindMatch = ((unsigned(req->m_kind) & Mask) != 0);
      bool pxMatch   = !IsFinite(a_px) || a_px == req->m_px;

      if (utxx::unlikely(!(kindMatch && pxMatch)))
      {
        LOG_WARN(1,
          "EConnector_OrdMgmt::GetReq12({}): ID={}: MisMatch: Mask={}, ReqKind="
          "{}, ProtoPx={}, ReqPx={}: Rejecting this Req12",
          a_where,      req->m_id,   Mask,  unsigned(req->m_kind),
          double(a_px), double(req->m_px))

        req = nullptr;  // Not good, so not found!
      }

      // XXX: For LeavesQty, we use a relaxed check: "a_leaves_qty" coming from
      // the proto layer may sometimes be inaccurate(???), so we only produce a
      // warning, not an error, in case of a mismatch -- it does NOT invalidate
      // the "req":
      Qty<QT,QR> leavesQty = a_leaves_qty;
      bool       qtyMatch  =
        IsInvalid(leavesQty) || (leavesQty == req->GetLeavesQty<QT,QR>());

      if (utxx::unlikely(!qtyMatch))
        LOG_WARN(1,
          "EConnector_OrdMgmt::GetReq12({}): ID={}: MisMatch: ProtoLeavesQty="
          "{}, ReqLeavesQty={}: Still accepting this Req12",
          a_where, req->m_id, QR(a_leaves_qty), QR(req->GetLeavesQty<QT,QR>()))
    }
#   endif // !UNCHECKED_MODE
    //-----------------------------------------------------------------------//
    // At the end, "NULL" is tolerated in non-Strict mode only:              //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely(Strict && (req == nullptr)))
      throw utxx::runtime_error
            ("EConnector_OrdMgmt::GetReq12(", a_where, "): ID=", a_id, ", AOS"
             "ID=", a_aos_id,  ", Mask=",  Mask, ", Px=",  double(a_px),
             ": Not Found or Params MisMatch");
    return req;
  }

  //=========================================================================//
  // "GetReq12ByExchID":                                                     //
  //=========================================================================//
  // NB: Unlike "GetReq12" above, this method is always Non-Strict: Return NULL
  // if no suitable Req12 was found:
  //
  template<unsigned Mask>
  Req12* EConnector_OrdMgmt::GetReq12ByExchID(ExchOrdID const& a_exch_id) const
  {
    // (*) ExchIDs Map must be supported. Then, only numeric "ExchOrdID"s are
    //     currently supported (XXX: for efficiency; should we change that?);
    // (*) This method is not applicable if UnChangedOrdIDs flag is set, as in
    //     that case, we cannot determine a unique Req12 with a given ExchID:
    CHECK_ONLY
    (
      if (utxx::unlikely
         (!m_useExchOrdIDsMap || a_exch_id.IsEmpty() || !a_exch_id.IsNum()))
        return nullptr;
    )
    OrderID oid  = a_exch_id.GetOrderID();
    assert (oid != 0);
    auto it = m_reqsByExchID.find(oid);
    if (utxx::unlikely(it == m_reqsByExchID.end()))
      return nullptr;

    // If found:
    Req12* req  = it->second;
    assert(req != nullptr && req->m_exchOrdID == a_exch_id);

    if (utxx::unlikely((unsigned(req->m_kind) & Mask) == 0))
    {
      LOG_WARN(2,
        "EConnector_OrdMgmt::GetReq12ByExchID: ExchID={}: MisMatch: Mask={}, "
        "ReqKind={}", oid, Mask, unsigned(req->m_kind))
      req = nullptr;
    }
    // XXX: Return what we got (even NULL):
    return req;
  }

  //=========================================================================//
  // "GetTargetReq12":                                                       //
  //=========================================================================//
  // Returns a ptr to "Req12" which can serve as the subject of a subsequent
  // "Cancel" or "Modify" request wrt this order, or NULL if no such "Req12"
  // exists  (ie if the order is already Inactive or PendingCancel):
  //
  template<QtyTypeT QT, typename QR, bool IsModify>
  inline Req12* EConnector_OrdMgmt::GetTargetReq12(AOS const* a_aos) const
  {
    //-----------------------------------------------------------------------//
    // Check the Args:                                                       //
    //-----------------------------------------------------------------------//
    CHECK_ONLY
    (
      if (utxx::unlikely(a_aos == nullptr))
        // Cannot proceed:
        throw utxx::badarg_error
              ("EConnector_OrdMgmt::GetTargetReq12(",
               IsModify ? "Modify" : "Cancel", "): AOS=NULL");
    )
    // So got the AOS -- check it. Maybe it is too late to Cancel or Modify this
    // order? It also includes the case when it is Part-Filled, and is unmodify-
    // able as as result (but still cancellable, of course)!
    bool okPartFilled =
      !IsModify || a_aos->m_lastTrd == nullptr || m_hasPartFilledModify;

    if (utxx::unlikely
       (a_aos->m_isInactive  || a_aos->m_isCxlPending != 0 || !okPartFilled))
    {
      // XXX: This is NOT an error. It simply means that the Target Req12 does
      // not exist,  because no further Cancellation/Modification  is possible
      // at this stage:
      LOG_WARN(2,
         "EConnector_OrdMgmt::GetTargetReq12: AOSID={}: Cannot be {}: {}",
          a_aos->m_id, IsModify       ? "Modified"   : "Cancelled",
          a_aos->m_isInactive         ? "Inactive"   :
         (a_aos->m_isCxlPending != 0) ? "CxlPending" : "Part-Filled")
      return nullptr;
    }

    //-----------------------------------------------------------------------//
    // Generic Case: Search for the Target Req:                              //
    //-----------------------------------------------------------------------//
    // It will be the LATEST admissible one. TODO: Need a formal guarantee that
    // multiple "Modify" Reqs cannot be issued against one Target Req. Normally,
    // this should not be the case since each new "Modify" Req  will become the
    // latest target on its own -- but we need more strict checks perhaps:
    //
    Req12* req  = a_aos->m_lastReq;
    for (; req != nullptr; req = req->m_prev)
    {
      // IMPORTANT:
      // (*) The Status of a "Req12" which is a valid target for Cancel or Modi-
      //     fy op is "New", "Acked" or "Confirmed",  and it must NOT be Failed
      //     or Failing. The latter could happen if eg a chain of "Modify"s was
      //     entered, and the order was then Part-Filled -- then unconfirmed Mo-
      //     difys will fail (but may not have been reported as Failed yet).
      // (*) The Target MAY have a  "New" or "Acked" (not yet "Confirmed") stat-
      //     us even if PipeLining is not allowed (and/or ExchOrdID needs to be
      //     quoted) -- it only means that the Cancel/Modify Req will become an
      //     Indication for a while. The latter also happens in case of Throttl-
      //     ing. This is hanfled by the CallER, not here.
      // (*) However, thr Target MUST NOT be an "Indication" iteself, because
      //     chaining up Indications is not allowed:  More recent Indications
      //     over-write earlier ones. Ie, the Target is always a Req which has
      //     already been sent on the wire.
      // (*) Obviously, the Target cannot be a "Cancel", though in theory we may
      //     still encounter a "Cancel" Req (if it has failed -- in that case,
      //     the AOS would NOT be CxlPending):
      //     would be CxlPending, and we already checked that it is not the ca-
      //     se). But we need to specifically ignore "ModLegC" ones:
      //
      if (utxx::unlikely
         (req->m_status == Req12::StatusT::Failed ||
          req->m_willFail                         ||
          req->m_kind   == Req12::KindT::Cancel   ||
          req->m_kind   == Req12::KindT::ModLegC))
        continue;

      // Furthermore:
      // If we have encountered a Req with a status from "Cancelled" on (eg
      // "Replaced"), then there is no valid target. Same if we encountered
      // a Cancel Req (w/o WillFail status -- already excluded above).  But
      // the only valid status in that case could be "Replaced"; all others
      // imply that the AOS is Inactive:
      //
      if (utxx::unlikely
         (req->m_status >= Req12::StatusT::Cancelled ||
          req->m_kind   == Req12::KindT::Cancel))
      {
        CHECK_ONLY
        (
          if (utxx::unlikely
             (req->m_status != Req12::StatusT::Replaced ||
              req->m_kind   == Req12::KindT::Cancel))
            LOG_ERROR(2,
              "EConnector_OrdMgmt::GetTargetReq12(",
              IsModify ? "Modify" : "Cancel", "): Encountered ReqID={} with "
              "Kind={}, Status={}",
              req->m_id, int(req->m_kind), int(req->m_status))
        )
        // Fall through to raising an exception:
        break;
      }
      // In all other cases: Found a valid Target:
      return req;
    }
    // If we got here (very unlikely): Could not find a suitable Req although
    // the AOS is active. This is an error:
    throw utxx::runtime_error
          ("EConnector_OrdMgmt::GetTargetReq12(",
          (IsModify ? "Modify" : "Cancel"), "): AOSID=", a_aos->m_id,
           ": Not Found");
    __builtin_unreachable();
  }

  //=========================================================================//
  // "CheckOrigID":                                                          //
  //=========================================================================//
  // NB: Because OrigReqID is recorded in Req12 when a new Cancel or Modify
  // req is created, and later reported back on various events via the Pro-
  // tocol layer, we compare the recorded and protocol-supplied OrigReqIDs;
  // XXX: there is currently no exception on a mismatch -- only error logged.
  // Does nothing in the UNCHECKED_MODE. Inlined for efficiency:
  //
  inline void EConnector_OrdMgmt::CheckOrigID
  (
    Req12 const& CHECK_ONLY(a_req),
    OrderID      CHECK_ONLY(a_orig_id),
    char const*  CHECK_ONLY(a_where)
  )
  {
    CHECK_ONLY
    (
      if (utxx::unlikely(a_req.m_origID != a_orig_id))
        LOG_ERROR(2,
          "EConnector_OrdMgmt::CheckOrigID({}): AOSID={}: Inconsistency: "
          "RecordedOrigID={}, ProtocolOrigID={}",
          a_where, a_req.m_aos->m_id, a_req.m_origID, a_orig_id)
    )
  }

  //=========================================================================//
  // "GetOrigReq":                                                           //
  //=========================================================================//
  // Very trivial, inlined for efficiency:
  //
  inline Req12* EConnector_OrdMgmt::GetOrigReq(Req12 const* a_req) const
  {
    assert (a_req != nullptr);
    OrderID origID = a_req->m_origID;

    if (utxx::unlikely(origID == 0))
    {
      assert(a_req->m_kind == Req12::KindT::New ||
             a_req->m_kind == Req12::KindT::ModLegN);
      return nullptr;
    }
    // Otherwise, "origID" must be valid:
    assert(origID  < a_req->m_id);
    Req12* origReq = m_Req12s + origID;
    assert(origReq->m_id == origID);
    return origReq;
  }

  //=========================================================================//
  // "CheckModifyParams":                                                    //
  //=========================================================================//
  // Returns the success flag:
  //
  template<QtyTypeT QT, typename QR>
  inline bool EConnector_OrdMgmt::CheckModifyParams
  (
    AOS   const* a_aos,
    Req12 const& a_orig_req,
    PriceT*      a_new_px,
    Qty<QT,QR>*  a_new_qty,
    Qty<QT,QR>*  a_new_qty_show,
    Qty<QT,QR>*  a_new_qty_min
  )
  const
  {
    //-----------------------------------------------------------------------//
    // Verify the OrigReq itself:                                            //
    //-----------------------------------------------------------------------//
    assert(a_aos         != nullptr && a_new_px       != nullptr &&
           a_new_qty     != nullptr && a_new_qty_show != nullptr &&
           a_new_qty_min != nullptr);

    assert(a_orig_req.m_aos         == a_aos                     &&
           a_orig_req.m_kind        != Req12::KindT::Cancel      &&
           a_orig_req.m_kind        != Req12::KindT::ModLegC     &&
           a_orig_req.m_status      <  Req12::StatusT::Cancelled &&
          !a_orig_req.m_willFail);

    // Orig Pxs and Qtys -- may be needed if the corresp New ones are not given:
    PriceT     origPx      = a_orig_req.m_px;
    Qty<QT,QR> origQty     = a_orig_req.GetQty    <QT,QR>();
    Qty<QT,QR> origQtyShow = a_orig_req.GetQtyShow<QT,QR>();
    Qty<QT,QR> origQtyMin  = a_orig_req.GetQtyMin <QT,QR>();

    // We currently do not support modification of non-Limit orders:
    CHECK_ONLY
    (
      OrderID         origID   = a_orig_req.m_id;
      FIX::OrderTypeT origType = a_aos->m_orderType;

      if (utxx::unlikely((origType != FIX::OrderTypeT::Limit) &&
                         (origType != FIX::OrderTypeT::Stop)))
        throw utxx::badarg_error
              ("EConnector_OrdMgmt::CheckModifyParams: ", m_name, ": AOSID=",
               a_aos->m_id, ": Only Limit and Stop Orders can currently be "
               "modified");

      if (utxx::unlikely(!(IsFinite(origPx) && IsPos(origQty))))
        // This must not happen, because OrigReq was a valid Limit order:
        throw utxx::runtime_error
              ("EConnector_OrdMgmt::CheckModifyParams: ", m_name, ": AOSID=",
               a_aos->m_id, ", OrigID=", origID, ": Invalid Param(s): OrigPx=",
               double(origPx),        ", OrigQty=", QR(origQty));
    )
    //-----------------------------------------------------------------------//
    // Old and Modified Params: Shall and Can we really Modify?              //
    //-----------------------------------------------------------------------//
    // Either the Modified Px or the Modified Qty may be invalid  (in that case
    // they are not amended), but NOT both, as otherwise there is nothing to do:
    CHECK_ONLY
    (
      if (utxx::unlikely(!(IsFinite(*a_new_px) || IsPos(*a_new_qty))))
        throw utxx::badarg_error
              ("EConnector_OrdMgmt::CheckModifyParams: ", m_name,
               ": Both NewPx & NewQty are Invalid");
    )
    // Those modifyable params which are not specified explicitly will retain
    // their original values -- we STILL submit them explicitly  rather  than
    // omit them -- this may be required by the unferlying protocol:
    //
    if (utxx::unlikely(!IsFinite(*a_new_px)))
      *a_new_px = origPx;

    if (!IsPos(*a_new_qty)) // Use the orig value: NB: 0 is not valid either!
      *a_new_qty = origQty;

    // NB: NewQtyShow ==  0 is perfectly valid (it means an absolutely invisible
    // Iceberg, if supported by the Exchange)!
    if (IsNeg(*a_new_qty_show))  // Use the orig value
      *a_new_qty_show = origQtyShow;

    // NB: NewQtyMin  ==  0 is also valid (which means no minimum):
    if (IsNeg(*a_new_qty_min))   // Use the orig value
      *a_new_qty_min  = origQtyMin;

    // Normalise the NewQtys:
    a_new_qty_show->MinWith(*a_new_qty);
    a_new_qty_min ->MinWith(*a_new_qty);

    // Now both "price" and "qty"s must be valid and consistent:
    CHECK_ONLY
    (
      if (utxx::unlikely
         (!IsFinite(*a_new_px)          || !IsPos(*a_new_qty) ||
           IsNeg(*a_new_qty_show)       ||  IsNeg(*a_new_qty_min) ||
           *a_new_qty_show > *a_new_qty || *a_new_qty_min > *a_new_qty))
        throw utxx::runtime_error
              ("EConnector_OrdMgmt::CheckModifyParams: ", m_name,
               ": Invalid Param(s): NewPx=",           *a_new_px,
               ", NewQty=",         QR(*a_new_qty), ", NewQtyShow=",
               QR(*a_new_qty_show), ", NewQtyMin=",    QR(*a_new_qty_min));
    )
    // XXX: If Nothing really is being changed, whould we return "false"  or
    // throw an exception? -- For the moment, do the former -- so the Caller
    // does NOT need to check whether Px and/or Qty is Modify will change:
    if (utxx::unlikely
       (*a_new_px       == origPx      && *a_new_qty     == origQty   &&
        *a_new_qty_show == origQtyShow && *a_new_qty_min == origQtyMin))
    {
      LOG_INFO(2,
        "EConnector_OrdMgmt::CheckModifyParams: AOSID={}: Skipped: All Pxs and "
        "Qtys are unchanged",  a_aos->m_id)
      return false;
    }
    // All Done:
    return true;
  }

  //=========================================================================//
  // "CheckOrigReq" (on sending Cancel or Modify, see "IsModify" flag):      //
  //=========================================================================//
  template<QtyTypeT QT, typename QR, bool IsModify, bool IsTandem>
  inline void EConnector_OrdMgmt::CheckOrigReq
  (
    AOS   const* a_aos,
    Req12 const* a_orig_req
  )
  const
  {
    assert(a_aos != nullptr);

    // First of all, OrigReq may be NULL only for IsModify=true (if we have in
    // fact modified an Indicated "New"):
    if (utxx::unlikely(a_orig_req == nullptr))
    {
      assert(IsModify);
      return;
    }
    // Generic Case: Got a non-NULL OrigReq:
    assert(a_orig_req->m_aos    == a_aos                                &&
          (a_orig_req->m_kind   == Req12::KindT::New                    ||
          (a_orig_req->m_kind   == Req12::KindT::ModLegN &&  IsTandem)  ||
          (a_orig_req->m_kind   == Req12::KindT::Modify  && !IsTandem)) &&
           a_orig_req->m_status <= Req12::StatusT::PartFilled           &&
          !a_orig_req->m_willFail);

    // Also, We currently do not support Modification orCancellation of Mkt,
    // IoC or FoK ords  (because they are presumed to be Filled/Failed inst-
    // antaneously):
    FIX::OrderTypeT   origType = a_aos->m_orderType;
    FIX::TimeInForceT origTiF  = a_aos->m_timeInForce;
    PriceT            origPx   = a_orig_req->m_px;
    OrderID           origID   = a_orig_req->m_id;

    if (utxx::unlikely
       (origType == FIX::OrderTypeT::Market          ||
        origTiF  == FIX::TimeInForceT::ImmedOrCancel ||
        origTiF  == FIX::TimeInForceT::FillOrKill))
      throw utxx::runtime_error
            ("EConnector_OrdMgmt::CheckOrigReq: ", m_name,
             ": Market, IoC, FoK orders CANNOT currently be cancelled");

    if (utxx::unlikely
       (origID   == 0                                            ||
       (origType == FIX::OrderTypeT::Limit && !IsFinite(origPx)) ||
        !IsPos(a_orig_req->m_qty)))
      // This MUST NOT happen:
      throw utxx::runtime_error
            ("EConnector_OrdMgmt::CheckOrigReq: ", m_name,
             ": Invalid Orig Params: AOSID=",      a_aos->m_id,
             "OrigReqID=", origID,   ", OrigPx=",  double(origPx),
             ", OrigQty=", double(a_orig_req->GetQty<QT,QR>()));
  }

  //=========================================================================//
  // "GetSameAOS":                                                           //
  //=========================================================================//
  // Inlined for efficiency:
  //
  inline AOS* EConnector_OrdMgmt::GetSameAOS
  (
    Req12 const&            a_req_curr,
    Req12 const& CHECK_ONLY(a_req_orig),
    char  const* CHECK_ONLY(a_where)
  )
  const
  {
    // Both AOS ptrs must be non-NULL, and must be same:
    AOS*   currAOS  = a_req_curr.m_aos;
    CHECK_ONLY(AOS*   origAOS  = a_req_orig.m_aos;)
    assert(currAOS != nullptr && origAOS != nullptr);

    CHECK_ONLY
    (
      if (utxx::unlikely(currAOS != origAOS))
        throw utxx::runtime_error
              ("EConnector_OrdMgmt::GetSameAOS(", a_where,  "): ",  m_name,
               ": Inconsistency: CurrReqID=",
               a_req_curr.m_id, " -> CurrAOSID=", currAOS->m_id, ", OrigReqID=",
               a_req_orig.m_id, " -> OrigAOSID=", origAOS->m_id);
    )
    // If OK: Return the AOS ptr:
    return currAOS;
  }

  //=========================================================================//
  // "RefreshThrottlers":                                                    //
  //=========================================================================//
  // To be invoked as frequently as reasonably possible, to improved the accu-
  // racy and efficiency of Orders Rate Throttling.  Currently, there is only
  // 1 Throttle to refresh. Inlined for efficiency:
  //
  inline void EConnector_OrdMgmt::RefreshThrottlers(utxx::time_val a_ts) const
  {
    if (utxx::likely(!a_ts.empty()))
      m_reqRateThrottler.refresh(a_ts);
  }

  //=========================================================================//
  // "UpdateReqsRate":                                                       //
  //=========================================================================//
  template<int NMsgs>
  inline void EConnector_OrdMgmt::UpdateReqsRate(utxx::time_val a_ts) const
  {
    static_assert(NMsgs == 1 || NMsgs == 2);
    assert(!a_ts.empty());
    // Increment the counter once or twice:
    m_reqRateThrottler.add  (a_ts);
    if constexpr (NMsgs == 2)
      m_reqRateThrottler.add(a_ts);
  }

  //=========================================================================//
  // "IsReadyToSend":                                                        //
  //=========================================================================//
  // Returns True iff there the pre-conditions for sending out a Req which ref-
  // ers to the given "OrigReq" (if any) are satisfied,  as well as Throttling
  // pre-conds. Inlined for efficiency:
  //
  template<typename ProtoEng>
  inline EConnector_OrdMgmt::ReadyT EConnector_OrdMgmt::IsReadyToSend
  (
    ProtoEng*      a_proto_eng,   // Non-NULL
    Req12 const*   a_req,         // Non-NULL
    Req12 const*   a_orig_req,    // May be NULL
    utxx::time_val a_ts           // Non-empty
  )
  const
  {
    assert(a_proto_eng != nullptr && a_req != nullptr && !a_ts.empty());

    //-----------------------------------------------------------------------//
    // Check Throttling first:                                               //
    //-----------------------------------------------------------------------//
    // Refresh the Throttler first, so it may clear up from a previously-busy
    // state:
    m_reqRateThrottler.refresh(a_ts);

    // Check the Throttling, unless it is a Cancel which may be exempt:
    bool throttlOK = true;
    long currReqs  = 0;
    bool noLimit   = (m_maxReqsPerPeriod <= 0);
    bool isExempt  =
      (a_req->m_kind == Req12::KindT::Cancel   ||
       a_req->m_kind == Req12::KindT::ModLegC) &&
      (ProtoEng::HasFreeCancel || m_cancelsNotThrottled);

    if (!(noLimit || isExempt))
    {
      // Really check the Throttle:
      currReqs  = m_reqRateThrottler.running_sum();
      throttlOK = currReqs < m_maxReqsPerPeriod;
    }
    if (utxx::unlikely(!throttlOK))
      LOG_WARN(2, "EConnector_OrdMgmt::CheckReqsRate: Throttled! {} Reqs!",
               currReqs)

    // If Throttling fails, no further checks -- fail:
    if (!throttlOK)
      return ReadyT::Throttled;

    //-----------------------------------------------------------------------//
    // Check the ProtoEngine:                                                //
    //-----------------------------------------------------------------------//
    // Eg, the underlying ProtoEngine may be TCP-based, and re-connecting... If
    // ProtoEngine is not Active    (which is typically equivalent to the whole
    // OMC being not Active), this cond is is also classified as "Throttling":
    //
    if (utxx::unlikely(!(a_proto_eng->IsActive())))
      return ReadyT::Throttled;

    //-----------------------------------------------------------------------//
    // Throttling is OK:                                                     //
    //-----------------------------------------------------------------------//
    bool ok = false;

    if (a_orig_req == nullptr)
    {
      //---------------------------------------------------------------------//
      // No "OrigReq": The curr Req is then a "New" or "ModLegN":            //
      //---------------------------------------------------------------------//
      assert(a_req->m_kind == Req12::KindT::New    ||
             a_req->m_kind == Req12::KindT::ModLegN);

      // If it is a "New", it really has no restrictions to go out:
      if (a_req->m_kind == Req12::KindT::New)
        return ReadyT::OK;

      // However, for a "ModLegN", the situation is different:
      // (*) It cannot be sent before the prior "ModLegC" (if the latter is an
      //     Indication yet);
      // (*) Furthermore, in PipeLineMode::Wait2, it cannot even be sent before
      //     its prior "ModLegC" is Confirmed, that is, the latter's target is
      //     actually Cancelled:
      assert(a_req->m_kind   == Req12::KindT::ModLegN);
      Req12 const*    prev   =  a_req->m_prev;
      assert(prev != nullptr && prev ->m_kind == Req12::KindT::ModLegC);

      ok = (m_pipeLineMode == PipeLineModeT ::Wait2      &&
            prev->m_status == Req12::StatusT::Confirmed) ||
           (m_pipeLineMode != PipeLineModeT ::Wait2      &&
            prev->m_status >= Req12::StatusT::New);
    }
    else
    {
      //---------------------------------------------------------------------//
      // Otherwise: Need to check the "OrigReq" as well:                     //
      //---------------------------------------------------------------------//
      // Return OK if the following conds are satisfied:
      // (1) OrigReq is Confirmed or Part-Filled   (we then assume that it has
      //     a valid ExchOrdID which may be required, if supported at all);
      //
      ok = a_orig_req->m_status  == Req12::StatusT::Confirmed ||
           a_orig_req->m_status  == Req12::StatusT::PartFilled;

      // (2) In the Fully-PipeLined Mode (Wait0), "confOK" is set more liberal-
      //     ly: it is True unless "SendExchOrdIDs" is set,  and the ExchOrdID
      //     is not available:
      //
      if (!ok && m_pipeLineMode  == PipeLineModeT::Wait0)
        ok |= !(m_sendExchOrdIDs && a_orig_req->m_exchOrdID.IsEmpty());
    }
    return (ok ? ReadyT::OK : ReadyT::OrigStatusBlock);
  }

  //=========================================================================//
  // "EmulateMassCancelGen":                                                 //
  //=========================================================================//
  // If "StratName", "Instr" or "Side" are empty, they are not used for filter-
  // ing-out the AOSes to Cancel:
  // Returns "true" iff at least one cancellation request was successfully sub-
  // mitted:
  //
  template<QtyTypeT QT, typename QR, typename ProtoEng, typename SessData>
  inline void EConnector_OrdMgmt::EmulateMassCancelGen
  (
    ProtoEng*       a_proto_eng,
    SessData*       a_sess,
    unsigned long   a_strat_hash48,
    SecDefD  const* a_instr,
    FIX::SideT      a_side
  )
  {
    assert(a_proto_eng != nullptr && a_sess != nullptr);
    //-----------------------------------------------------------------------//
    // Iterate over all AOSes:                                               //
    //-----------------------------------------------------------------------//
    utxx::time_val now  = utxx::now_utc();
    bool           done = false;  // At least 1 AOS cancelled?

    // NB: To do cancellations with the lowest latency, we start from the top-
    // most currently available OrderID, and move down to 1 (as 0 is not used):
    //
    for (OrderID id = (*m_OrdN)-1; id >= 1; --id)
    {
      AOS* aos = m_AOSes + id;

      if (utxx::unlikely(aos->m_id != id))
      {
        // XXX: ID mismatch can occur if eg the AOSes persistent storage was
        // reset but the Counters one preserved (XXX: it normally  DOES need
        // to be preserved, otherwise we could get wrong ReqIDs and SeqNums);
        // so just skip such AOSes. In case of AOSes file reset, the ID would
        // be 0:
        CHECK_ONLY
        (
          if (utxx::likely(aos->m_id != 0))
            LOG_WARN(2,
              "EConnector_OrdMgmt::EmulateMassCancelGen: AOSID mismatch: Got "
              "{}, should be {}", aos->m_id, id)
        )
        continue;
      }
      // Also skip this order if it is already Inactive or Pending Cancel (we
      // expect that the majority of AOSes would actually be "disused"):
      //
      if (utxx::likely(aos->m_isInactive || aos->m_isCxlPending != 0))
        continue;

      // Otherwise: Apply the Filters:
      if (utxx::likely
         ( // Side:
           ((a_side  == FIX::SideT::UNDEFINED)                 ||
            (a_side  == FIX::SideT::Buy  &&   aos->m_isBuy)    ||
            (a_side  == FIX::SideT::Sell && !(aos->m_isBuy)))  &&
           // SecID:
           ((a_instr == nullptr)                               ||
            (a_instr->m_SecID == aos->m_instr->m_SecID))       &&
           // StratHash:
           (a_strat_hash48    == aos->m_stratHash48)
         ))
      {
        // Yes, proceed with Cancellation. We do NOT use the Delayed Send opt-
        // ion here in any case, because the number of Orders to Cancel may be
        // so large that Delayed Send would case buffer overflow:
        //
        (void) CancelOrderGen<QT, QR, ProtoEng, SessData>
          (a_proto_eng, a_sess, aos, now, now, now, false);
        done = true;
      }
    }
    //---------------------------------------------------------------------//
    // Result:                                                             //
    //---------------------------------------------------------------------//
    if (utxx::unlikely(!done && m_debugLevel >= 2))
    {
      // Produce hex representation of "a_strat_hash48":
      char  buff[13];
      char* hex48 = buff;
      utxx::itoa16_right<unsigned long, 12>(hex48, a_strat_hash48);
      buff[12] = '\0';

      LOG_WARN(2,
        "EConnector_OrdMgmt::EmulateMassCancelGen: StratHash48={}, Instr={}, "
        "Side={}: No applicable AOSes",
        hex48,    (a_instr != nullptr) ? a_instr->m_FullName.data() : "NULL",
        int(a_side))
    }
  }

  //=========================================================================//
  // "MakeAOSInactive":                                                      //
  //=========================================================================//
  // Mark the whole order (AOS) as "Inactive" -- it sould NOT be Inactive yet:
  //
  template<QtyTypeT QT,       typename QR,
           typename ProtoEng, typename SessData>
  inline void EConnector_OrdMgmt::MakeAOSInactive
  (
    ProtoEng*       a_proto_eng,  // Non-NULL
    SessData*       a_sess,       // Non-NULL
    AOS*            a_aos,        // Non-NULL
    Req12 const*    a_req,        // CxlReq or FilledReq, may be NULL
    char  const*    a_where,      // Non-NULL
    utxx::time_val  a_ts_exch,
    utxx::time_val  a_ts_recv
  )
  {
    assert(a_proto_eng != nullptr && a_sess != nullptr && a_aos != nullptr &&
          (a_req       == nullptr || a_req->m_aos == a_aos)                &&
           a_where != nullptr);

    // Already Inactive?
    CHECK_ONLY
    (
      if (utxx::unlikely(a_aos->m_isInactive))
        LOG_WARN(2,
          "EConnector_OrdMgmt::MakeAOSInactive({}): AOSID={}: Is already "
          "Inactive", a_where, a_aos->m_id)
    )
    // In any case, set the flags:
    a_aos->m_isInactive   = true;
    a_aos->m_isCxlPending = 0;

    // All Reqs which are still unconfirmed, will be marked as "WillFail" (XXX:
    // special treatment for "ModLegC"/"ModLegN" ones); IsPartFill=false, beca-
    // use the AOS would NOT be Inactive in case of a PartFill:
    //
    MakePendingReqsFailing<false,  QT, QR, ProtoEng, SessData>
      (a_proto_eng, a_sess, a_aos, a_req,  a_where,  a_ts_exch, a_ts_recv);
    // All Done!
  }

  //=========================================================================//
  // "MakePendingReqsFailing":                                               //
  //=========================================================================//
  // After "a_req" has failed  (or was Part-Filled and became non-Modifyable),
  // all subsequent unconfirmed Reqs which transiently target this one, are to
  // be marked as "WillFail":
  //
  template<bool IsPartFill,   QtyTypeT QT,   typename QR,
           typename ProtoEng, typename SessData>
  inline void EConnector_OrdMgmt::MakePendingReqsFailing
  (
    ProtoEng*       a_proto_eng,
    SessData*       a_sess,
    AOS*            a_aos,        // Non-NULL
    Req12 const*    a_req,        // CxlReq or TradedReq, may be NULL
    char  const*    a_where,
    utxx::time_val  a_ts_exch,
    utxx::time_val  a_ts_recv
  )
  {
    assert(a_proto_eng != nullptr && a_sess       != nullptr &&
           a_aos != nullptr       && a_where      != nullptr &&
          (a_req == nullptr       || a_req->m_aos == a_aos));

    // Also, if we got here due to a PartFill, PartFilled Reqs must be non-mod-
    // ifyable -- otherwise this method should not be invoked at all:
    assert(!(IsPartFill && m_hasPartFilledModify));

    //-----------------------------------------------------------------------//
    // Traverse the Subsequent Reqs:                                         //
    //-----------------------------------------------------------------------//
    // If "a_req" is non-NULL, we traverse Reqs from the next one forward.
    // Otherwise, traverse the Reqs from the last one backwards:
    //
    bool isFwd    = (a_req != nullptr);
    Req12* wfr    = isFwd  ?  a_req->m_next : a_aos->m_lastReq;
    bool   didCxl = false;    // Any ModLegN Cancellations in this process?

    while (wfr != nullptr)
    {
      //---------------------------------------------------------------------//
      // Checks:                                                             //
      //---------------------------------------------------------------------//
      // If moving bwd in time, stop if an already-Inactive Req is encountered?
      // XXX: Strictly speaking, it is not proved (because the Next/Prev Req12
      // relationship  may not be same as the ReqID one), but we use this heur-
      // istics anyway:
      if (!isFwd && wfr->m_status >= Req12::StatusT::Cancelled)
        break;

      // Then, if it is an Indication (has not been sent), mark it as "Failed",
      // not "WillFail", in place. XXX: However, we do NOT invoke any Strategy
      // Call-Backs ("OnOrderError") in this case, since Indication Failure is
      // a second-order event after a Cancel or Trade:
      //
      if (wfr->m_status == Req12::StatusT::Indicated)
      {
        wfr->m_status = Req12::StatusT::Failed;
        continue;
      }
      // Then:
      // (1) There should be no "New" Reqs here: a "New" Req can only be the 1st
      //     (oldest), so it cannnot come after "a_req";
      // (2) All subsequent Reqs (after "a_req") should be yet-UnConfirmed. In-
      //     deed, when the next Req becomes confirmed, "a_req" becomes Cancel-
      //     led or Replaced. But in fact, "a_req" is either Failed or PartFil-
      //     led (ie Confirmed and still alive); thus, the status of "wfr" can
      //     only be "New" or "Acked":
      CHECK_ONLY
      (
        if (utxx::unlikely
           ((wfr->m_kind   == Req12::KindT::New         || // (1)
             wfr->m_status >= Req12::StatusT::Confirmed))) // (2)
          LOG_WARN(2,
            "EConnector_OrdMgmt::MakePendingReqsFailing({}): AOSID={}: "
            "UnExpected: ReqID={}, Kind={}, Status={}",
            a_where,  a_aos->m_id, wfr->m_id, wfr->m_kind.to_string(),
            wfr->m_status.to_string())
      )
      //---------------------------------------------------------------------//
      // A live "ModLegN" Found?                                             //
      //---------------------------------------------------------------------//
      if (wfr->m_kind == Req12::KindT::ModLegN)
      {
        // Obviously, this can only happen in the Non-Atomic-Modify mode:
        assert(!ProtoEng::HasAtomicModify);

        // Then this Req will NOT Fail by itself -- we need to Cancel it expli-
        // citly rather than kust mark it as "WillFail",   in order to prevent
        // extraneous Trades on the already-Cancelled (or Traded) AOS.  Obvios-
        // ly, we can only submit a CxlReq: there is no guarantee that it will
        // take effect before the "ModLegN" is Traded:
        //
        LOG_INFO(4,
          "EConnector_OrdMgmt::MakePendingReqsFailing({}): AOSID={}, ReqID={}: "
          "Cancelling a ModLegN", a_where, a_aos->m_id, wfr->m_id)

        CancelModLegN<QT, QR, ProtoEng, SessData>
          (a_proto_eng, a_sess, wfr, a_ts_exch, a_ts_recv);
        didCxl = true;
      }
      //---------------------------------------------------------------------//
      // Mark those Reqs which WillFail:                                     //
      //---------------------------------------------------------------------//
      // (1) ALL subsequent Reqs after a failed one (!IsPartFill) will fail,
      //     obviously;
      // (2) If it was a PartFill, then the subsequnt Reqs will fail if they
      //     are unmodifyable -- unless it is the 1st Cancel;
      // (3) Also, "ModLegN" Reqs will never fail (XXX: even if the orig order
      //     has been Filled -- then we need to Cancel "ModLegN" explicitly):
      //
      bool noFail =
           wfr->m_kind == Req12::KindT::ModLegN     ||
           (IsPartFill                              &&
             (m_hasPartFilledModify                 ||
             (wfr->m_kind   == Req12::KindT::Cancel &&
              wfr->m_origID == a_req->m_id))
           );
      // XXX: Don't use the condition (wfr == a_req->m_next), it may not ref-
      // lect the Target(wfr)==a_req  relation correctly!

      if (!noFail)
      {
        wfr->m_willFail = true;   // Yes, it will fail!

        // Also, possibly reset the "CxlPending" flag: If "wfr" was  a  "Cancel"
        // req, and we now know it WillFail, then we clear the "CxlPending" flag
        // on the AOS:
        //
        if (wfr->m_kind         == Req12::KindT::Cancel &&
            a_aos->m_isCxlPending == wfr->m_id)  // Check this for extra safety!
          a_aos->m_isCxlPending = 0;
      }
      //---------------------------------------------------------------------//
      // Go fwd or bwd:                                                      //
      //---------------------------------------------------------------------//
      wfr = isFwd ? wfr->m_next : wfr->m_prev;
    }
    //-----------------------------------------------------------------------//
    // Finally:                                                              //
    //-----------------------------------------------------------------------//
    // If we did any ModLegN Cancellations, may need to Flush the orders. NB:
    // for some instantiation reasons,  we cannot use ProtoEng::HasBatchSend
    // here:
    if (didCxl && ProtoEng::HasBatchSend)
      FlushOrdersGen<ProtoEng, SessData>(a_proto_eng, a_sess);

    // All Done!
  }

  //=========================================================================//
  // "SendIndicationsOnEvent":                                               //
  //=========================================================================//
  // Send out 1 or more Indicative (as yet) Reqs which follow the given "a_req",
  // when the status of "a_req" has changed:
  //
  template<typename ProtoEng, typename SessData>
  inline void EConnector_OrdMgmt::SendIndicationsOnEvent
  (
    ProtoEng*       a_proto_eng,  // Non-NULL
    SessData*       a_sess,       // Non-NULL
    AOS*            a_aos,        // Non-NULL
    utxx::time_val  a_ts          // Non-empty
  )
  {
    //-----------------------------------------------------------------------//
    // Find the Indications for this AOS:                                    //
    //-----------------------------------------------------------------------//
    // The ProtoEng and Session must be valid:
    assert(a_proto_eng != nullptr && a_sess     != nullptr  &&
           a_aos       != nullptr && !a_ts.empty());

    // Also verify the SeqNums:
    assert(m_TxSN == a_sess->m_txSN && m_RxSN == a_sess->m_rxSN);

    // By construction, UnSent Indication(s) may only be the latest ones: 1 if
    // AtomicModify is in use, or possibly 2 otherwise:
    assert(m_hasAtomicModify == ProtoEng::HasAtomicModify);
    DEBUG_ONLY(constexpr bool IsTandem  = !ProtoEng::HasAtomicModify;)

    // So where is our starting point (the earliest Indication)? Normally, it
    // would be the last (most recent) Req in the AOS:
    Req12* indReq  = a_aos->m_lastReq;
    assert(indReq != nullptr);

    if (indReq->m_status != Req12::StatusT::Indicated)
      return;   // No Indications at all!

    Req12* prev  = indReq->m_prev;
    Req12* next  = nullptr;
    if (prev != nullptr && prev->m_status == Req12::StatusT::Indicated)
    {
      // This can only be a (ModLegC, ModLegN) pair in the Tandem mode:
      assert(IsTandem && prev->m_kind == Req12::KindT::ModLegC &&
             indReq->m_kind == Req12::KindT::ModLegN);
      next   = indReq;
      indReq = prev;
      // There must be NO further Indications:
      assert(indReq->m_prev == nullptr ||
             indReq->m_prev->m_status  != Req12::StatusT::Indicated);
    }
    //-----------------------------------------------------------------------//
    // Actually send out 1 or 2 Indications if possible:                     //
    //-----------------------------------------------------------------------//
    // (*) IsNew=false;
    // (*) OrigReq is not known, so pass NULL
    // (*) BatchSend is set according to the ProtoEng capabilities:
    // (*) Catch exceptions here -- if "TrySendIndications" fails (eg due to
    //     some temporary conds), they can be safely re-sent later by  Timer
    //     or another Event:
    try
    {
      (void) TrySendIndications<ProtoEng, SessData>
             (a_proto_eng, a_sess, nullptr, indReq, next, false,
              ProtoEng::HasBatchSend,     a_ts);

      // NB: Even if the Indication(s) have been sent successfully, they are NOT
      // removed from "m_allInds" here --  it is more efficient to do all remov-
      // als in one go in "SendIndicationsOnTimer".

      // Flush the orders if BatchSend was effective:
      //
      if constexpr (ProtoEng::HasBatchSend)
        FlushOrdersGen<ProtoEng, SessData>(a_proto_eng, a_sess);
    }
    catch (std::exception const& exn)
    {
      LOG_WARN(2,
        "EConnector_OrdMgmt::SendIndicationsOnEvent: Exception on Send: {}",
        exn.what())
    }
  }

  //=========================================================================//
  // "SendIndicationsOnTimer":                                               //
  //=========================================================================//
  // Send out all previously-Indicated Reqs which are now eligible for sending.
  // Also, this methid performs clean-up of "m_allInds" of the Reqs are no lon-
  // ger Indications (eg either have been send, or Failed in-place):
  //
  template<typename ProtoEng, typename SessData>
  inline void EConnector_OrdMgmt::SendIndicationsOnTimer
  (
    ProtoEng*       a_proto_eng,  // Non-NULL
    SessData*       a_sess        // Non-NULL
  )
  {
    assert(m_allInds != nullptr); // Since the OMC is enabled!
    if (m_allInds->empty() || !IsActive())
      return;                     // Nothing to do (or can do nothing)

    //-----------------------------------------------------------------------//
    // Generic Case: 1st Pass: Go through all Indications:                   //
    //-----------------------------------------------------------------------//
    utxx::time_val  ts = utxx::now_utc();

    Req12** allInds = &((*m_allInds)[0]);
    int       nInds = int(m_allInds->size());

    for (int i = 0; i < nInds; ++i)
    {
      Req12* ind  = allInds[i];
      assert(ind != nullptr);

      // First of all, "ind" might not really be an Indication (if it was suc-
      // cessfully sent out by "SendIndicationsOnEvent",  or if it has Failed
      // in-place):
      if (ind->m_status != Req12::StatusT::Indicated)
        continue;

      // Generic Case: It is an Indication. Will try to send 1 or 2 Inds. Check
      // if it is actually a pair of (ModLegC + ModLegN), or a single Ind.  NB:
      // a stand-alone ModLegC is also possible here (but NOT in "SendIndicati-
      // onsOnEvent") -- if it cancels a stray "ModLegN":
      //
      Req12* next = nullptr;   // By default, a single Ind

      if (ind->m_kind == Req12::KindT::ModLegC && i < nInds-1)
      {
        // We may or may not have a (ModLegC + ModLegN) pair here:
        next = allInds[i+1];
        assert(next != nullptr);

        // Guard against false pairs. Valid (ModLegC + ModLegN) pairs have suc-
        // cessive ReqIDs:
        if (next->m_kind   != Req12::KindT::ModLegN     ||
            next->m_status != Req12::StatusT::Indicated ||
            next->m_id     != ind->m_id + 1)
          next = nullptr;  // Not a valid successor -- still a single "ModLegC"
      }
      // Now, try to send 1 or 2 Indications (depending on next != NULL):
      // (*) IsNew = false
      // (*) OrigReq is not known, so pass NULL
      // (*) BatchSend is set to true if supported by ProtoEng:
      // XXX: This is a bit inefficient, since the Throttler will be refreshed
      //     every time with the same "ts"...
      // (*) Catch exceptions here -- we do not want to propagate them from
      //     the Timer-driven task:
      try
      {
        bool throttlOK =
          TrySendIndications<ProtoEng, SessData>
            (a_proto_eng, a_sess, nullptr, ind, next, false,
             ProtoEng::HasBatchSend,   ts);

        // If we encountered throttling (or not Active ProtoEngine), no point
        // in going further:
        if (!throttlOK)
          break;
      }
      catch (std::exception const& exn)
      {
        // Any sending exceptions are likely due to the OMC not being fully
        // Active (though we check this cond at the beginning). So log them
        // and break, but no drastic actions:
        LOG_WARN(2,
          "EConnector_OrdMgmt::SendIndicationsOnTimer: Exception on Send: {}",
          exn.what())
        break;
      }
      // NB: If 2 Indications were consumed (as opposed to 1), increment the
      // counter by 1 more:
      if (next != nullptr)
        ++i;
    }
    // If BatchSend mode was used, Flush the reqs now:
    if constexpr (ProtoEng::HasBatchSend)
      FlushOrdersGen<ProtoEng, SessData>(a_proto_eng, a_sess);

    //-----------------------------------------------------------------------//
    // 2nd Pass: Expunge the Sent Reqs:                                      //
    //-----------------------------------------------------------------------//
    // Compress "AllInds" removing non-Indications:
    //
    int to = 0;
    for (int i = 0; i < nInds; ++i)
      if (allInds[i]->m_status == Req12::StatusT::Indicated)
        allInds[to++] = allInds[i];

    // At the end, "to" is the number of remaining Indications:
    m_allInds->resize(size_t(to));

    // All Done!
  }

  //=========================================================================//
  // "CreatePusherTask":                                                     //
  //=========================================================================//
  // A Helper Method to be invoked by Derived Classes (where ProtoEng and Sess-
  // Data are available):
  //
  template<typename ProtoEng, typename SessData>
  inline void EConnector_OrdMgmt::CreatePusherTask
  (
    ProtoEng* a_proto_eng,
    SessData* a_sess
  )
  {
    assert(a_proto_eng != nullptr && a_sess != nullptr && m_allInds != nullptr);

    // To be on a safe side, remove the existing PusherFD and clear AllInds:
    RemovePusherTimer();
    assert(m_pusherFD < 0 && m_allInds->empty());

    // Activate the periodic PusherTask:
    // TimerHandler:
    IO::FDInfo::TimerHandler timerH
    (
      [this, a_proto_eng, a_sess](int DEBUG_ONLY(a_fd))->void
      {
        assert(a_fd == m_pusherFD);
        // The PusherTask body:
        this->SendIndicationsOnTimer<ProtoEng, SessData>(a_proto_eng, a_sess);
      }
    );

    // ErrorHandler:
    IO::FDInfo::ErrHandler   errH
    (
      [this](int DEBUG_ONLY(a_fd), int a_err_code, uint32_t a_events,
             char const* a_msg) -> void
      {
        assert(a_fd == m_pusherFD);
        this->PusherTimerErrHandler(a_err_code, a_events, a_msg);
      }
    );

    // Create the TimerFD and add it to the Reactor:
    m_pusherFD = EConnector::m_reactor->AddTimer
    (
      "PusherTaskTimer",
      PusherTaskPeriodMSec, // Initial offset
      PusherTaskPeriodMSec, // Period: same
      timerH,
      errH
    );
    LOG_INFO(2,
      "PusherTaskTimer Created, FD={}, Period={} msec",
      m_pusherFD, PusherTaskPeriodMSec)
  }

  //=========================================================================//
  // "TrySendIndications":                                                   //
  //=========================================================================//
  // Returns True iff there was no Throttling encountered (even though Indicat-
  // ion(s) could not be sent for other reasons):
  //
  template<typename ProtoEng, typename SessData>
  inline bool   EConnector_OrdMgmt::TrySendIndications
  (
    ProtoEng*       a_proto_eng,    // Non-NULL
    SessData*       a_sess,         // Non-NULL
    Req12 const*    a_orig_req,     // May be NULL
    Req12*          a_ind_req,      // Non-NULL
    Req12*          a_next_ind,     // May be NULL
    bool            a_is_new,       // Indication(s) are newly-created?
    bool            a_batch_send,
    utxx::time_val  a_ts            // Non-empty
  )
  {
    //-----------------------------------------------------------------------//
    // Checks:                                                               //
    //-----------------------------------------------------------------------//
    // If these are Indication(s) which we are just trying to send, they status
    // must indeed by "Indicated":
    //
    assert(a_ind_req  != nullptr                               &&
           a_ind_req ->m_status  == Req12::StatusT::Indicated  &&
          (a_next_ind == nullptr ||
           a_next_ind->m_status  == Req12::StatusT::Indicated) &&
          !a_ts.empty());

    // The "a_batch_send" param must be supported by the "ProtoEng". It is not
    // an error if we have to switch it off:
    a_batch_send &= ProtoEng::HasBatchSend;

    //-----------------------------------------------------------------------//
    // Obtain/Check the "OrigReq":                                           //
    //-----------------------------------------------------------------------//
    // (But "OrigReq" MUST be NULL for "New"/"ModLegN"):
    //
    if (a_orig_req == nullptr                    &&
        a_ind_req->m_kind != Req12::KindT::New   &&
        a_ind_req->m_kind != Req12::KindT::ModLegN)
    {
      a_orig_req = GetOrigReq(a_ind_req);
      CHECK_ONLY
      (
        if (utxx::unlikely(a_orig_req == nullptr))
          // XXX: This is a serious error condition:
          throw utxx::logic_error
                ("EConnector_OrdMgmt::TrySendIndications: ReqID=",
                 a_ind_req->m_id,  ": OrigReq not found");
      )
    }
    // At this point, the following invariant must hold in any case:
    assert((a_orig_req == nullptr) ==
           (a_ind_req->m_kind   == Req12::KindT::New    ||
            a_ind_req->m_kind   == Req12::KindT::ModLegN));

    //-----------------------------------------------------------------------//
    // Check Throttling:                                                     //
    //-----------------------------------------------------------------------//
    // We will try to send at least 1 req. Obviously, if we cannot send the 1st
    // one for any reason, we cannot send the 2nd one (ModLegN):
    //
    ReadyT ready1 =
      IsReadyToSend<ProtoEng>(a_proto_eng, a_ind_req, a_orig_req, a_ts);

    if (ready1 != ReadyT::OK)
    {
      // Cannot send the Indication(s) right now. If this is our first attempt,
      // memoise them in "allInds"; otherwise, they are already there:
      //
      MemoiseIndication(a_ind_req, a_is_new);

      if (a_next_ind != nullptr)
        MemoiseIndication(a_next_ind, a_is_new);

      // Return True iff the reason for non-sending is NOT throttling:
      return (ready1 != ReadyT::Throttled);
    }
    ReadyT  ready2    = ReadyT::OK; // Calculated later, OK by default
    int     nReqsSent = 1;          // It will be 1 or 2

    //-----------------------------------------------------------------------//
    // OK, can send at least 1 Indication now!                               //
    //-----------------------------------------------------------------------//
    DEBUG_ONLY(SeqNum oldTxSN = *m_TxSN;)

    switch (a_ind_req->m_kind)
    {
      //---------------------------------------------------------------------//
      case Req12::KindT::New:
      case Req12::KindT::ModLegN:   // A stand-alone unsent "ModLegN"
      //---------------------------------------------------------------------//
      {
        assert(a_orig_req == nullptr  &&  a_next_ind == nullptr);
        a_proto_eng->NewOrderImpl(a_sess, a_ind_req, a_batch_send);

        // We have sent 1 req ("New" or "ModLegN"):
        assert(a_ind_req->m_status == Req12::StatusT::New &&
               a_ind_req->m_seqNum == oldTxSN   && *m_TxSN == oldTxSN + 1);

        UpdateReqsRate<1>(a_ts);
        break;
      }
      //---------------------------------------------------------------------//
      case Req12::KindT::Cancel:
      //---------------------------------------------------------------------//
      {
        assert(a_orig_req != nullptr && a_next_ind == nullptr);
        a_proto_eng->CancelOrderImpl
          (a_sess, a_ind_req, a_orig_req, a_batch_send);

        // We have sent 1 req ("Cancel"):
        assert(a_ind_req->m_status == Req12::StatusT::New &&
               a_ind_req->m_seqNum == oldTxSN &&  *m_TxSN == oldTxSN + 1);

        // NB: If "Cancel"s are free, do NOT update the Throttler:
        if constexpr (!ProtoEng::HasFreeCancel)
          UpdateReqsRate<1>(a_ts);
        break;
      }
      //---------------------------------------------------------------------//
      case Req12::KindT::Modify:
      //---------------------------------------------------------------------//
      {
        assert(a_orig_req != nullptr && a_next_ind == nullptr);
        a_proto_eng->ModifyOrderImpl
          (a_sess, nullptr, a_ind_req, a_orig_req, a_batch_send);

        // We have sent 1 req (an atomic "Modify"):
        assert(ProtoEng::HasAtomicModify                  &&
               a_ind_req->m_status == Req12::StatusT::New &&
               a_ind_req->m_seqNum == oldTxSN &&  *m_TxSN == oldTxSN + 1);

        UpdateReqsRate<1>(a_ts);
        break;
      }
      //---------------------------------------------------------------------//
      case Req12::KindT::ModLegC:
      //---------------------------------------------------------------------//
      {
        // Found an Indicated "ModLegC" -- then it must be followed by an Indi-
        // cated "ModLegN":
        assert(!ProtoEng::HasAtomicModify                     &&
               a_orig_req != nullptr && a_next_ind != nullptr &&
               a_next_ind->m_kind == Req12::KindT::ModLegN);

        // "ModLegN" does not have an "OrigReq", but may be affected by Thrott-
        // ling, and it cannot be sent before the correp "ModLegC" is sent or
        // Confirmed:
        ready2 =
          IsReadyToSend<ProtoEng>(a_proto_eng, a_next_ind, nullptr, a_ts);

        if (ready2 == ReadyT::OK)
        {
          // Yes, it is OK to send BOTH "ModLegC" and "ModLegN":
          // Invoke "ModifyOrderImpl" once, it will send BOTH msgs, but the par-
          // ams come from the 2nd Req12 (ModLegN).    Again, use BatchSend iff
          // supported:
          // NB: ReqIDs of the (ModLegC, ModLegN) are:
          //  (a_ind_req->m_id, next->m_id),
          // so the base one (to be passed to "ModifyOrderImpl") is:
          //  a_ind_req->m_id;
          //
          a_proto_eng->ModifyOrderImpl
            (a_sess, a_ind_req, a_next_ind, a_orig_req, a_batch_send);

          // We have sent 2 reqs ("ModLegC" and "ModLegN") AT ONCE, so TxSN is
          // incremented by 2 in this case:
          assert(a_ind_req ->m_status == Req12::StatusT::New &&
                 a_ind_req ->m_seqNum == oldTxSN             &&
                 a_next_ind->m_status == Req12::StatusT::New &&
                 a_next_ind->m_seqNum == oldTxSN + 1         &&
                *m_TxSN               == oldTxSN + 2);

          // Still, as the 1st req is "ModLegC", it may be "free" for the
          // Throttler:
          UpdateReqsRate<ProtoEng::HasFreeCancel ? 1 : 2>(a_ts);

          // But "nReqsSent" will be 2 in this case irrespective to whether
          // "ModLegC" is "free" or not:
          nReqsSent = 2;
        }
        else
        {
          // "ModLegN" cannot be sent, so only send a "ModLegC", similar to the
          // "Cancel"  case:
          a_proto_eng->CancelOrderImpl
            (a_sess, a_ind_req, a_orig_req, a_batch_send);

          assert(a_ind_req->m_status  == Req12::StatusT::New &&
                 a_ind_req->m_seqNum  == oldTxSN && *m_TxSN  == oldTxSN + 1);

          // Again, this "ModLegC" may be "free" for the Throttler:
          if constexpr (!ProtoEng::HasFreeCancel)
            UpdateReqsRate<1>(a_ts);

          // And if it is a newly-created Indication, "ModLegN" is memoised:
          MemoiseIndication(a_next_ind, a_is_new);
        }
        break;
      }
      //---------------------------------------------------------------------//
      default:
      //---------------------------------------------------------------------//
        // Encountered something inapplicable, log an error and return:
        CHECK_ONLY
        (
          LOG_ERROR(2,
            "EConnector_OrdMgmt::TrySendIndications: Spurious Indication: "
            "OrigID={}, IndID={}, IndKind={}",
            ((a_orig_req != nullptr) ? a_orig_req->m_id : 0), a_ind_req->m_id,
            int(a_ind_req->m_kind), int(a_ind_req->m_status))
        )
        return true;  // Still, no trottling encountered!
    }
    //-----------------------------------------------------------------------//
    // If we got here: Sent!                                                 //
    //-----------------------------------------------------------------------//
    // NB: If it was a BatchSend, the CallER,  not this method,  is responsible
    // for Flushing the wire orders (eventually).
    // But WE are responsible for stats: Increment or reset NBuffReqs depending
    // on whether it was a BatchSend:
    //
    if (a_batch_send)
      m_nBuffdReqs += nReqsSent;
    else
      m_nBuffdReqs = 0;

    // Return the flag indicating whether there was NO throttling:
    assert(ready1 != ReadyT::Throttled);
    return ready2 != ReadyT::Throttled;
  }

  //=========================================================================//
  // "MemoiseIndication":                                                    //
  //=========================================================================//
  // Places an Indication on the "m_allInds" queue:
  //
  inline void EConnector_OrdMgmt::MemoiseIndication
    (Req12* a_ind_req, bool a_is_new)
  const
  {
    assert(a_ind_req != nullptr && m_allInds != nullptr);

    if (a_is_new)
    {
      // "IndReq" was just created -- it must NOT be in "m_allInds" yet:
      assert(std::find(m_allInds->cbegin(), m_allInds->cend(), a_ind_req) ==
                       m_allInds->cend());

      // Check for "m_allInds" overflow (it must not happen anyway, just for
      // extra safety):
      CHECK_ONLY
      (
        if (utxx::unlikely(m_allInds->size() >= MaxInds))
          throw utxx::runtime_error
                ("EConnector_OrdMgmt::TrySendIndicators: AllInds OverFlow");
      )
      // Store it!
      m_allInds->push_back(a_ind_req);
    }
    else
    {
      // Conversely, if "IsNew" flag is NOT set, then "IndReq" is an existing
      // Indication, and it *should* be on the "m_allInds" list.  However, un-
      // like ShM-based Req12s and AOSes, is cleared on restarts -- so we can
      // NOT strictly assert that "IndReq" must be there. Just produce a warn-
      // ing if it is not:
      //
      bool notIn =
           (std::find(m_allInds->cbegin(), m_allInds->cend(), a_ind_req) ==
                      m_allInds->cend());
      if (utxx::unlikely(notIn))
      {
        LOG_WARN(2,
          "EConnector_OrdMgmt::MemoiseIndication: IndID={} is NotNew but is "
          "not on the AllInds list -- a restart effect?",  a_ind_req->m_id)
        // Then store it as well:
        m_allInds->push_back(a_ind_req);
      }
    }
    // All Done!
  }

  //=========================================================================//
  // "ApplyExchIDs":                                                         //
  //=========================================================================//
  // Store the ExchOrdID and the MDEntryID (if non-NULL) (both assigned by the
  // Exchange) in the given Req12 and AOS,  or verify those IDs consistency if
  // they are already there. An exception is thrown if the check fails:
  //
  template<bool Force>
  inline void EConnector_OrdMgmt::ApplyExchIDs
  (
    Req12*           a_req,
    AOS*             CHECK_ONLY(a_aos),
    ExchOrdID const& a_exch_id,
    ExchOrdID const& a_mde_id,
    char const*      CHECK_ONLY(a_where)
  )
  {
    assert(a_req != nullptr && a_aos != nullptr && a_req->m_aos == a_aos);

    // We have 2 srcs and 2 targs for each src. XXX: Hopefully, the following
    // loops will be unrolled by the optimiser:
    for (int s = 0; s < 2; ++s)
    {
      // Select the SrcID:
      ExchOrdID const& src = (s == 0) ? a_exch_id : a_mde_id;

      // Do not perform any checks or over-writing if the SrcID is empty:
      if (utxx::unlikely(src.IsEmpty()))
        continue;

      // Yes, install "src" in Req if the target is empty or if "Force" is
      // set. Ie, in the latter case, there are NO consistency checks!
      //
      ExchOrdID& targ = a_req->m_exchOrdID; // Ref!

      if (Force || targ.IsEmpty())
        targ = src;
      CHECK_ONLY
      (
      else
      if (utxx::unlikely(targ != src))
        // Consistency check was performed, and has failed: Already-installed
        // ID is not the same as the new one. This is an error cond:
        throw utxx::runtime_error
          ("EConnector_OrdMgmt::ApplyExchID(",  a_where, "): ",   m_name,
           ": ReqID=",  a_req->m_id,  ": Inconsistent ",
           ((s == 0) ? "ExchOrdIDs" : "MDEntryIDs"), ": Old=",
           targ.ToString(),           ", New=", src.ToString());
      )
    }
    // If the checks are OK: Update the { ExchID => Req12 } map (only for nume-
    // ric ExchIDs -- if this map is maintained at all).  HOWEVER, this is inc-
    // ompatible with UnChangedOrdIDs flag,  as in that case, more than 1 ReqID
    // can correspond to a given (unchanged) ExchOrdID:
    //
    if (m_useExchOrdIDsMap)
    {
      CHECK_ONLY
      (
        if (utxx::unlikely(a_exch_id.IsEmpty() || !a_exch_id.IsNum()))
        {
          if ((a_req->m_kind & Req12::KindT::Cancel) > 0) {
            // this is a cancel, it's ok to not have an ExchOrdID
            return;
          }

          throw utxx::runtime_error
            ("EConnector_OrdMgmt::ApplyExchID(",  a_where, "): ", m_name,
             ": ReqID=", a_req->m_id, ", AOSID=", a_aos->m_id,
             " : Invalid ExchOrdID=", a_exch_id.ToString());
        }
      )
      // Otherwise:
      OrderID oid  = a_exch_id.GetOrderID();
      assert (oid != 0);
      auto it =  m_reqsByExchID.find(oid);

      if  (it == m_reqsByExchID.end())
        // Yes, memoise the (oid => a_req):
        (void)   m_reqsByExchID.insert(std::make_pair(oid, a_req));
      else
      {
        // If the pair (oid => a_req) is already there, then "oid" should point
        // to the same Req12 (by ptr comparison),  because it was inserted on
        // the 1st Confirm. If not, do NOT over-write the Req12 -- produce an
        // error msg but continue;
        CHECK_ONLY
        (
          Req12* storedReq  = it->second;
          assert(storedReq != nullptr);

          if (utxx::unlikely(it->second != a_req))
            LOG_ERROR(2,
              "EConnector_OrdMgmt::ApplyExchID({}): Inconsistency for ExchID="
              "{}: OldReqID={}, NewReqID={}",
              a_where,   oid, storedReq->m_id, a_req->m_id)
        )
      }
    }
  }

  //=========================================================================//
  // "LogTrade":                                                             //
  //=========================================================================//
  template<QtyTypeT QT, typename QR, QtyTypeT QTF, typename QRF>
  inline void EConnector_OrdMgmt::LogTrade(Trade const& a_tr) const
  {
    Req12 const* req = a_tr.m_ourReq;
    assert(req != nullptr);
    AOS   const* aos = req->m_aos;
    assert(aos != nullptr);

    //-----------------------------------------------------------------------//
    // For Time Stamps, we extract only the times -- and use CurrDateStr():  //
    //-----------------------------------------------------------------------//
    utxx::time_val currDate = GetCurrDate();
    utxx::time_val exchTime = a_tr.m_exchTS - currDate;
    utxx::time_val recvTime = a_tr.m_recvTS - currDate;
    utxx::time_val endDay(86399, 999999); // XXX: No leap seconds!

    // Make sure that the time stamps are intra-day:
    CHECK_ONLY
    (
      utxx::time_val zero;

      if (utxx::unlikely(exchTime < zero))
      {
        LOG_WARN(2, "EConnector_OrdMgmt::LogTrade: ExchTime < CurrDate")
        exchTime   = zero;
      }
      if (utxx::unlikely(recvTime < zero))
      {
        LOG_WARN(2, "EConnector_OrdMgmt::LogTrade: RecvTime < CurrDate")
        recvTime   = zero;
      }
      if (utxx::unlikely(exchTime > endDay))
      {
        LOG_WARN(2, "EConnector_OrdMgmt::LogTrade: ExchTime > EndDay")
        exchTime   = endDay;
      }
      if (utxx::unlikely(recvTime > endDay))
      {
        LOG_WARN(2, "EConnector_OrdMgmt::LogTrade: RecvTime > EndDay")
        recvTime   = endDay;
      }
    )
    auto     exchHMS  = utxx::time_val::to_hms(exchTime.sec());
    auto     recvHMS  = utxx::time_val::to_hms(recvTime.sec());

    unsigned exchHour = get<0>(exchHMS);
    unsigned exchMin  = get<1>(exchHMS);
    unsigned exchSec  = get<2>(exchHMS);
    long     exchUSec = exchTime.usec();

    unsigned recvHour = get<0>(recvHMS);
    unsigned recvMin  = get<1>(recvHMS);
    unsigned recvSec  = get<2>(recvHMS);
    long     recvUSec = recvTime.usec();

    // XXX: Cannot use a format-based log functon here:
    // (*) it would probably be inefficient (dynamic parsing of format str);
    // (*) and apparently, it does not work anyway (assert failure);
    // so:
    char  buff[512];
    char* curr = buff;

    //-----------------------------------------------------------------------//
    // Date, ExchTS, RecvTS:                                                 //
    //-----------------------------------------------------------------------//
    // CurrDate = YYYYMMDD:
    curr      = stpcpy(curr, GetCurrDateStr());
    *(curr++) = ',';
    // ExchTS = hhmmss.ssssss:
    (void) utxx::itoa_right<unsigned,2,char>(curr, exchHour, '0');
    curr += 2;
    (void) utxx::itoa_right<unsigned,2,char>(curr, exchMin,  '0');
    curr += 2;
    (void) utxx::itoa_right<unsigned,2,char>(curr, exchSec,  '0');
    curr += 2;
    *(curr++) = '.';
    (void) utxx::itoa_right<long,    6,char>(curr, exchUSec, '0');
    curr += 6;
    *(curr++) = ',';
    // RecvTS = hhmmss.ssssss:
    (void) utxx::itoa_right<unsigned,2,char>(curr, recvHour, '0');
    curr += 2;
    (void) utxx::itoa_right<unsigned,2,char>(curr, recvMin,  '0');
    curr += 2;
    (void) utxx::itoa_right<unsigned,2,char>(curr, recvSec,  '0');
    curr += 2;
    *(curr++) = '.';
    (void) utxx::itoa_right<long,    6,char>(curr, recvUSec, '0');
    curr += 2;
    *(curr++) = ',';

    //-----------------------------------------------------------------------//
    // ExchID, ExecID:                                                       //
    //-----------------------------------------------------------------------//
    // ExecID as string:
    curr      = stpcpy(curr, a_tr.m_execID.ToString());
    *(curr++) = ',';

    //-----------------------------------------------------------------------//
    // Order Props:                                                          //
    //-----------------------------------------------------------------------//
    // Full Instrument Name:
    SecDefD const& instr =  *(aos->m_instr);
    assert(&instr == a_tr.m_instr);

    curr      = stpcpy(curr, instr.m_FullName.data());
    *(curr++) = ',';
    // Buy/Sell Flag:
    *(curr++) = aos->m_isBuy ? 'B' : 'S';
    *(curr++) = ',';
    // Last Qty -- up to the maximum width (20 chars) -- but actually does not
    // use more positions than necessary:
    QR trQty  = QR(a_tr.GetQty<QT,QR>());
    assert(std::is_floating_point_v<QR> == a_tr.m_withFracQtys);
    if constexpr(std::is_floating_point_v<QR>)
      curr += utxx::ftoa_left(double(trQty), curr, 20, 8);
    else
      curr  = utxx::itoa_left<long,20,char> (curr, long(trQty));
    *(curr++) = ',';
    // Px: XXX: Is f15.5 always sufficient?
    curr     += utxx::ftoa_left(double(a_tr.m_px), curr, 15, 5);
    *(curr++) = ',';
    // SettlDate = YYYYMMDD:
    (void) utxx::itoa_right<int, 8, char>(curr, instr.m_SettlDate,  '0');
    curr += 8;
    *(curr++) = ',';
    // Trading Fee: Assumed to be always fractional:
    QRF trFee = QRF(a_tr.GetFee<QTF,QRF>());
    if constexpr(std::is_floating_point_v<QRF>)
      curr += utxx::ftoa_left(double(trFee), curr, 15, 5);
    else
      curr  = utxx::itoa_left<long,20,char> (curr, long(trFee));
    *(curr++) = ',';
    // Where the Trade event came from:
    // (O): normal OMC ExecReport;
    // (M): MDC    FullOrdersLog;
    *(curr++) = (a_tr.m_mdc != nullptr) ? 'M' : 'O';
    *(curr++) = ',';

    //-----------------------------------------------------------------------//
    // And finally, the StratergyName (if available):                        //
    //-----------------------------------------------------------------------//
    if (utxx::likely(aos->m_strategy != nullptr))
      curr = stpcpy(curr, aos->m_strategy->GetName().data());

    // The output should be '\0'-terminated and fit in the "buff":
    assert(*curr == '\0' && (curr - buff) < int(sizeof(buff)));

    //-----------------------------------------------------------------------//
    // Now actually log it:                                                  //
    //-----------------------------------------------------------------------//
    assert(m_tradesLogger != nullptr);
    m_tradesLogger->info(buff);
    m_tradesLogger->flush();
  }
} // End namespace MAQUETTE
