// vim:ts=2:et
//===========================================================================//
//                   "TradingStrats/3Arb/TriArb2S/Orders.hpp":               //
//                             Order Entry Methods                           //
//===========================================================================//
#pragma  once
#include "TradingStrats/3Arb/TriArb2S/TriArb2S.h"
#include <ctime>

namespace MAQUETTE
{
  //=========================================================================//
  // "RemoveOrder": Common functionality of "OnCancel" and "OnError":        //
  //=========================================================================//
  void TriArb2S::RemoveOrder
  (
    AOS const&      a_aos,
    char const*     a_msg,
    utxx::time_val  a_ts_exch,
    utxx::time_val  a_ts_recv,
    utxx::time_val  a_ts_strat
  )
  {
    // Is it a Passive Order which was Cancelled (or has Failed)? -- This is the
    // most likely case, because  Aggressive  orders cannot  normally be cancel-
    // led. If there is no explicit "a_msg", then it's a Cancel:
    bool    isCancel = (a_msg == nullptr);

    // Also note that a failure may not be "fatal", eg it could be a Modify Req
    // which has failed -- but the Order may still be alive (though it's unlike-
    // ly); tell this by "IsInactive" flag;  but Cancel implies "Inactive",  of
    // course:
    assert(!isCancel || a_aos.m_isInactive);

    OrderInfo const& ori = a_aos.m_userData.Get<OrderInfo>();
    char const*    which = nullptr;

    if (ori.m_isAggr)
    {
      // It was an Aggressive Order -- find and remove its AOS:
      RemoveAggrOrder(a_aos, a_ts_exch, a_ts_recv, a_ts_strat);
      which = "AGGRESSIVE";
    }
    else
    {
      // It was a Passive Order -- find and remove its AOS. XXX: For speed, we
      // iterate directly over a "flattened" "m_passAOSes" array:
      //
      AOS const** passBegin = &(m_passAOSes[0][0][0]);
      AOS const** passEnd   = &(m_passAOSes[1][1][1]) + 1;
      AOS const** passIt    = std::find(passBegin, passEnd, &a_aos);

      if (utxx::likely(passIt != passEnd && a_aos.m_isInactive))
        // Remove an Inactive Passive Order:
        *passIt = nullptr;
      which = "PASSIVE";
    }

    // Log the Cancellation/Failure (and if logged, flush the logger):
    LOG_INFO(2,
      "{} ORDER {}: AOSID={}, Symbol={}, {}{}{}",
      which,    isCancel ? "CANCELLED" : "FAILED",       a_aos.m_id,
      a_aos.m_instr->m_Symbol.data(), a_aos.m_isBuy ? "Bid" : "Ask",
     (a_msg != nullptr)  ? a_msg              : "",
      a_aos.m_isInactive ? ": Order Inactive" : "")

    m_logger->flush();
  }

  //=========================================================================//
  // "RemoveAggrOrder":                                                      //
  //=========================================================================//
  void TriArb2S::RemoveAggrOrder
  (
    AOS const&      a_aos,
    utxx::time_val  a_ts_exch,
    utxx::time_val  a_ts_recv,
    utxx::time_val  a_ts_strat
  )
  {
    // This should really be an Aggressive Order:
    assert(a_aos.m_userData.Get<OrderInfo>().m_isAggr);

    // It must be found in the list of active Aggr Orders:
    auto aggrIt = std::find(m_aggrAOSes.begin(), m_aggrAOSes.end(), &a_aos);

    if (utxx::unlikely(aggrIt == m_aggrAOSes.end()))
    {
      // XXX: This is unlikely, and is not really normal:
      LOG_WARN(2, "Aggr Order ID={} not found on the list!", a_aos.m_id)
      // No flush...
      return;
    }
    // Otherwise: Generic Case:
    // Found an Aggressive Order which was Cancelled, Filled, or has Failed:
    // Expunge the corresp AOS Ptr if the order is confirmed to be Inactive
    // (as opposed to a "soft fail"):
    //
    if (utxx::likely(a_aos.m_isInactive))
        m_aggrAOSes.erase(aggrIt);

    // IMPORTANT:
    // Analyse the situation after an Aggressive Order was removed (most likely,
    // it has been filled):
    // If there are no aggressive orders currently active, we assume that all
    // traiangles are "complete" for this moment,  and we can analyse the
    // left-over Ccy positions:
    // XXX: In case of any errors (eg in Positions calculation), this method can
    // give rise to an infinite sequence of aggressive orders, so for extra saf-
    // ety, it can be disabled explicitly (by re-setting "m_adjustCcyTails"):
    //
    if (m_adjustCcyTails && m_aggrAOSes.empty())
      AdjustCcyTails(a_ts_exch, a_ts_recv, a_ts_strat);
  }

  //=========================================================================//
  // "Quote":                                                                //
  //=========================================================================//
  // Template params:
  // T: TOD(0) or TOM(1);
  // I: AB (0) or CB (1);
  // IsBid:       Side
  // IsMICEX:     SrcPx: MICEX (1) or AlfaFIX2 (0)
  // Args:
  // "a_src_px":  Either SrcPx1 (if MICEX)  or SrcPx2 (if AlfaFIX2)  which has
  //              just been updated, or NaN (if only a constraint has changed):
  // Returns "true" iff any quote has actually been placed or modified as a res-
  // ult of this invocation:
  //
  template<int T, int I, bool IsBid, bool IsMICEX>
  bool TriArb2S::Quote
  (
    PriceT            a_src_px,      // May be NaN
    utxx::time_val    a_ts_md_exch,
    utxx::time_val    a_ts_md_conn,
    utxx::time_val    a_ts_md_strat
  )
  {
    //-----------------------------------------------------------------------//
    // Should we quote it at all?                                            //
    //-----------------------------------------------------------------------//
    static_assert((T == TOD || T == TOM) && (I == AB || I == CB),
                  "TriArb2S::Quote: Invalid T and/or I");

    // TOD instrs are quoted only before "m_lastQuoteTimeTOD". After that time,
    // any existing TOD quotes would be cancelled (see CheckTOD()):
    if (T == TOD && m_isFinishedQuotesTOD)
      return false;

    //-----------------------------------------------------------------------//
    // Get the Instrument, Pxs, and AOS (if already exists):                 //
    //-----------------------------------------------------------------------//
    AOS const*&      aos       = m_passAOSes   [T][I][IsBid];   // NB: Ref!!!
    SecDefD   const& instr     = *(m_passInstrs[T][I]);         // Instr Quoted
    OrderBook const& ob        = *(m_orderBooks[2*T + I]);      // Target OB
    PriceT&          px        = m_synthPxs    [T][I][IsBid];   // NB: Ref!!!
    PriceT           bestBidPx = ob.GetBestBidPx();
    PriceT           bestAskPx = ob.GetBestAskPx();

    // An existing order must of course be on the same Side as the new one. Al-
    // so, the explicitly-provided Instrument and that given via the OrderBook,
    // must of course be the same:
    if (utxx::unlikely
       (&instr != &(ob.GetInstr())              ||
       (aos != nullptr && aos->m_isBuy != IsBid)))
    {
      LOG_ERROR(1,
        "TriArb2S::Quote: Inconsistency: Explicit SecDef={}, OrderBook SecDef="
        "{}, OrigSide={}, NewSide={}",
        instr.m_Symbol.data(),       ob.GetInstr().m_Symbol.data(),
        aos->m_isBuy ? "Bid": "Ask", IsBid ? "Bid" : "Ask")

      // This is a very serious error condition:
      m_logger->flush();
      DelayedGracefulStop(a_ts_md_exch, a_ts_md_conn, a_ts_md_strat);
      return false;
    }

    // Then: "a_src_px" is either "srcPx1" (if IsMICEX is set) or "srcPx2". Ie,
    // this method is invoked when one of the SrcPxs has changed; the other one
    // will be NaN, which means "unchanged":
    //
    PriceT srcPx1 =  IsMICEX ? a_src_px : PriceT();
    PriceT srcPx2 = !IsMICEX ? a_src_px : PriceT();

    // If the AOS is already Inactive for any reason, set it to NULL and force
    // re-computation of all pxs:
    if (utxx::likely(aos != nullptr))
    {
      if (aos->m_isInactive)
        aos = nullptr;
      else
      if (aos->m_isCxlPending != 0)
        // XXX: In the latter case, don't do anything until the cancellation is
        // confirmed (because, in some rare cases, it might not be canceled  at
        // the end):
        return false;
    }

    // Otherwise, proceed to quoting. But to do that, we must have liquidity on
    // both sides of the Target OrderBook. XXX: If we do not, it's not a criti-
    // cal error by itself, but we cannot quote in that case:
    //
    if (utxx::unlikely(!(IsFinite(bestBidPx) && IsFinite(bestAskPx))))
      return false;

    //-----------------------------------------------------------------------//
    // If one of "srcPx{1,2}" is Finite, re-calculate Synthetic Px:          //
    //-----------------------------------------------------------------------//
    // This means that this method has been involved on a src px update, not on
    // constraints (Target OrderBook) update:
    //
    if (IsFinite(a_src_px) || !IsFinite(px) || aos == nullptr)
    {
      //---------------------------------------------------------------------//
      // Src1: MICEX:                                                        //
      //---------------------------------------------------------------------//
      // The MICEX instrument to be quoted ("I") is always "opposite" to the
      // MICEX base px src ("src1"):
      constexpr int    src1 = I ^ 1;

      // Get the Src1 Px (unless it is already known): SAME side as indicated
      // by "IsBid":
      // (*) we will get passive fill on "IsBid";
      // (*) will need to enter an OPPOSITE aggr order (so "!IsBid");
      // (*) but it will aggress into opposite side, so "IsBid" again for Px"
      //
      if (utxx::unlikely(!IsFinite(srcPx1)))
        srcPx1 = GetVWAP(T, src1, IsBid, m_reserveQtys[src1]);

      //---------------------------------------------------------------------//
      // Now AC (Src2,  from AlfaFIX2):                                       //
      //---------------------------------------------------------------------//
      // (*) for quoting AB (I==0): Same side  as indicated by "IsBid".
      // (*) for quoting CB (I==1): Opposite side:
      constexpr bool   IsBid2 = IsBid ^ bool(I);
      if (!IsFinite(srcPx2))
      {
        // But for for AC (unlike Src1), we always use the corresp BestPx, be-
        // cause AC is alwaready quoted in bands, and those bands are  typica-
        // lly sufficiently wide (total vol in best band much larger than the
        // quoted qty):
        OrderBook const& ob2    = *(m_orderBooks[8]);
        srcPx2  = IsBid2 ? ob2.GetBestBidPx() : ob2.GetBestAskPx();
      }

      // In any case, "srcPx2" (originally from SPOT) is to be adjusted  by Swap
      // Pxs to get the TOD or TOM Px; swap px depends on both T (SettlDate) and
      // side: use the opposite side to that of AC:
      constexpr bool   IsBidSwap = !IsBid2;
      srcPx2 -= m_ACSwapPxs[T][IsBidSwap];

      // PxStep for the quoted Instrument:
      double pxStep =  instr.m_PxStep;
      assert(pxStep >  0.0);

      //---------------------------------------------------------------------//
      // Verify the SrcPxs (VWAP) and the corresp BestPxs for abrupt change: //
      //---------------------------------------------------------------------//
      // TODO!

      //---------------------------------------------------------------------//
      // Re-calculate the price to be quoted:                                //
      //---------------------------------------------------------------------//
      // For AB, it's a product; for CB, ratio:
      double px12 = (I == AB)
                    ? double(srcPx1) * double(srcPx2)
                    : double(srcPx1) / double(srcPx2);
      px =
        IsBid
        ? RoundDown(PriceT(px12 - m_sprds[I]), pxStep)
        : RoundUp  (PriceT(px12 + m_sprds[I]), pxStep);
    }
    // Otherwise, Synthetic px remains the same, but the constrains may have
    // changed ("DoQuote" below will take care of them)...

    //-----------------------------------------------------------------------//
    // Check the Pxs:                                                        //
    //-----------------------------------------------------------------------//
    // Cannot proceed if any of the following pxs are NaN. In that case, any
    // existing order is cancelled for safety:
    if (utxx::unlikely
       (!(IsFinite(px) && IsFinite(bestBidPx) && IsFinite(bestAskPx)) ))
    {
      if (aos != nullptr)
      {
        // This must be a MICEX OMC:
        (void) CancelOrderSafe<false>     // Not buffered
               (GetOMCMX(aos), aos,
                a_ts_md_exch,  a_ts_md_conn, a_ts_md_strat);
      }
      // Nothing more to do in this case. Return "false" because we might only
      // have done a cancellation:
      return false;
    }

    // Otherwise: Generic Case: "px", "bestBidPx" and "bestAskPx" are all valid
    // (though "srcPx[12]" might not be,  if only constraints have been modifi-
    // ed):
    //-----------------------------------------------------------------------//
    // May need to apply constraints on "px" based on curr OrderBook:        //
    //-----------------------------------------------------------------------//
    // (*) if "px" appears to be aggressive, it is left unchanged -- the actual
    //     exec px will  be determined  by the best OrderBook px  on the target
    //     side;
    // (*) if, other way round, "px" is deeply passive, it is also left unchan-
    //     ged -- it cannot be improved without sacrificing the PnL;
    // (*) but if it falls into the curr Bid-Ask spread, we push it next to the
    //     curr Side's best px level  -- the fill probability will be the  same
    //     but the expected PnL will be improved:
    // (*) TODO: volatility-based adjustments or continuous px prediction?
    //
    double pxStep = instr.m_PxStep;
    if (utxx::unlikely(bestBidPx < px && px < bestAskPx))
    {
      // Yes, "px" is strictly inside the Bid-Ask spread: Move it towards the
      // best level of the curr side:
      px  = IsBid ? (bestBidPx + pxStep) : (bestAskPx - pxStep);

      // The adjusted "px" is STILL strictly within the Bid-Ask spread, because
      // in this case, the spread is at least 2 "psStep"s:
      assert(bestBidPx < px && px < bestAskPx);
    }

    //-----------------------------------------------------------------------//
    // Modify an Existing Active Quote?                                      //
    //-----------------------------------------------------------------------//
    bool modify   = (aos != nullptr);
    if (utxx::likely(modify))
    {
      // Then we have already checked that the AOS is indeed fully-active:
      assert(!(aos->m_isInactive || aos->m_isCxlPending != 0));

      // Then decide whether we want to do this modification at all. XXX: for
      // now, we do NOT modify an existing quote iff both the CurrPx  and the
      // NewPx are located sufficiently deeply inside the OrderBook,  and the
      // magnitude of move is sufficienty small -- in that case, such a  move
      // would be irrelevant indeed. In all other cases, we DO the move:
      //
      PriceT currPx;   // NaN
      QtyO   currQty;  // 0
      GetLastQuote(aos, &currPx, &currQty);

      // Since this AOS exists, it must contain a valid current quote -- when
      // it becomes Inactive, it is removed:
      assert(IsFinite(currPx) && IsPos(currQty));

      // Has the quoted Px changed? (The Qty is always constant):
      int diff = int(Round(Abs(px - currPx) / pxStep));
      if (diff == 0)
        // Then no modification at all is required:
        return false;

      // Otherwise: Compute the "Distances into the Depth of the OrderBook" for
      // the curr and new pxs, in PxSteps.  NB: Unlike "diff", "depth"s  can be
      // of ANY sign:
      int currDepth =
        int(Round((IsBid ? (bestBidPx - currPx) : (currPx - bestAskPx))
                  / pxStep));
      int newDepth  =
        int(Round((IsBid ? (bestBidPx - px)     : (px     - bestAskPx))
                  / pxStep));
      int minDepth  = std::min<int>(currDepth, newDepth);

      if (utxx::likely
         ((1 <= minDepth && minDepth < m_resistDepth1 &&
           diff     <  m_resistDiff1)                 ||
          (minDepth >= m_resistDepth1 && diff < m_resistDiff2)))
        // Then no modification:
        return false;

      // In all other cases, issue a modification req. The OMC must be MICEX:
      bool ok = ModifyOrderSafe
                (GetOMCMX(aos), aos, px, m_qtys[I],
                 a_ts_md_exch,  a_ts_md_conn, a_ts_md_strat);
      if (utxx::unlikely(!ok))
        return false;   // All error handling has already been done

      // Modify the "TrInfo" -- install the new Quoted and Src Pxs:
      TrInfo* tri = aos->m_userData.Get<OrderInfo>().m_trInfo;

      if (utxx::likely(tri != nullptr))
      {
        // NB: "tri" may be NULL if this AOS is a left-over from a prev run.
        // Otherwise:
        assert (tri->m_passAOS == aos);

        tri->m_passQuotedPx   = px;
        tri->m_passQty        = m_qtys[I];
        if (IsFinite(srcPx1))
          tri->m_aggrSrcPx[0] = srcPx1;
        if (IsFinite(srcPx2))
          tri->m_aggrSrcPx[1] = srcPx2;
      }
    }
    else
    //-----------------------------------------------------------------------//
    // A New Quote?                                                          //
    //-----------------------------------------------------------------------//
    {
      // IsAggr=false; T, I, TrInfo are set explicitly below:
      // VirtSettldate will be taken from "instr":
      // The MICEX OMC Connector to be used, is selected randomly:
      aos = NewOrderSafe<false>
            (GetOMCMX(a_ts_md_strat),    instr,  IsBid,  px,  m_qtys[I],
             a_ts_md_exch, a_ts_md_conn, a_ts_md_strat,  -1, -1, nullptr);

      if (utxx::unlikely(aos == nullptr))
        return false;   // All handling has already been done

      // Allocate (from a pre-allocated vector) a new "TrInfo" struct  and att-
      // ach it to the AOS. In this case, SrcPxs must be finite   (as we would
      // have re-computed them if "aos" was NULL):
      assert(IsFinite(srcPx1) && IsFinite(srcPx2));

      m_trInfos.emplace_back();
      TrInfo& tri            = m_trInfos.back();
      tri.m_passAOS          = aos;
      tri.m_passQuotedPx     = px;
      tri.m_passQty          = m_qtys[I];
      tri.m_aggrSrcPx    [0] = srcPx1;
      tri.m_aggrSrcPx    [1] = srcPx2;

      OrderInfo ori;
      ori.m_isAggr           = false;
      ori.m_passT            = T;
      ori.m_passI            = I;
      ori.m_trInfo           = &tri;

      aos->m_userData.Set(ori);
    }
    // NB: We did not do anything if "a_aos" already exists, but is in the
    // "PendingCancel" state, because that state can (theoretically)    be
    // reversed. Instead, we will wait until the cancellation is confirmed,
    // and then re-quote. This condition is very unlikely anyway.

    //-----------------------------------------------------------------------//
    // Log the Quote:                                                        //
    //-----------------------------------------------------------------------//
    // The Req which was just submitted:
    Req12 const* req = aos->m_lastReq;
    assert(req != nullptr);

    // A fill becomes more likely if the quote falls onto, or in between,
    // BestBidPx .. BestAskPx:
    long lat_usec   = utxx::time_val::now_diff_usec(a_ts_md_conn);
    bool fillLikely = (bestBidPx <= px && px <= bestAskPx);

    LOG_INFO(2,
      "Passive Quote ({}): AOSID={}, ReqID={}, Symbol={}, {}: Px={}, Best"
      "Pxs={}..{}, Qty={}, SrcPx1={}, SrcPx2={}, Latency={} usec{}",
      modify ? "Modify" : "New", aos->m_id,  req->m_id, instr.m_Symbol.data(),
      IsBid  ? "Bid"    : "Ask", double(px), double(bestBidPx),
      double(bestAskPx),     QRO(m_qtys[I]),  double(srcPx1),  double(srcPx2),
      lat_usec, fillLikely  ? ",!!!" : "")
      // No flush here -- this output is very frequent

    return true;
  }

  //=========================================================================//
  // "DoCoveringOrders":                                                     //
  //=========================================================================//
  void TriArb2S::DoCoveringOrders
  (
    AOS const&      a_pass_aos,
    PriceT          a_pass_px,
    QtyO            a_pass_qty,
    utxx::time_val  a_ts_md_exch,
    utxx::time_val  a_ts_md_conn,
    utxx::time_val  a_ts_md_strat
  )
  {
    //-----------------------------------------------------------------------//
    // Check the Passive Order which needs to be covered:                    //
    //-----------------------------------------------------------------------//
    OrderInfo const& ori = a_pass_aos.m_userData.Get<OrderInfo>();
    int      T   = ori.m_passT;
    int      I   = ori.m_passI;
    TrInfo*  tri = ori.m_trInfo;
    bool passBid = a_pass_aos.m_isBuy;

    if (utxx::unlikely
       ((T != TOD && T != TOM)   || (I != AB && I != CB) || tri == nullptr ||
        double(a_pass_px) <= 0.0 || !IsPos(a_pass_qty)))
      throw utxx::badarg_error
            ("TriArb2S::DoCoveringOrders: Invalid: T=", T, ", I=", I,
             ", TrInfo=", (tri == nullptr) ? "NULL" : "non-NULL",  ", PassPx=",
             a_pass_px,   ", PassQty=",   QRO(a_pass_qty));

    if (utxx::unlikely(T == TOD && m_isFinishedQuotesTOD))
      // XXX: OK, cannot do TOD -- will do TOM instead!
      T = TOM;

    //-----------------------------------------------------------------------//
    // Evaluate approx prospective PnL (if we close the triangle):           //
    //-----------------------------------------------------------------------//
    // Which side should we take with this Instr?
    // (G==0) MICEX   leg is always opposite to PassSide;
    // (G==1) AlfaFIX2 leg is opposite to PassSide for I==0 (AB),
    //                and is same     as PassSide for I==1 (CB):
    // So the following will do:
    //
    bool aggrBids[2]
    {
      ! passBid,                  // G==0: MICEX    Leg
      !(passBid ^ I)              // G==1: AlfaFIX2 Leg
    };

    // By default, use MICEX for Aggr Order @ G==0, and always use AlfaFIX2 for
    // Aggr Order @ G==1. However, if Optimal Routing of Aggr Orders is allow-
    // ed, G==0 may also result in an ALfaECN order:
    //
    // Instruments which can be used (G==0):
    SecDefD   const* aggrInstr0M = m_passInstrs[T][I^1];

    // The corresp OrderBooks (G==0):
    OrderBook const* aggrOB0M    = m_orderBooks[2*T + (I^1)];

    // Qty of Aggr Order for G==0. XXX:  It depends on the Exec Px  which in
    // turn may depend on the Qty (via VWAP)! However, the dependency of Qty
    // on  the ExecPx is rather weak (eg for MICEX execution,  it is rounded
    // to the LotSize anyway), so for the moment, we do NOT solve for a fix-
    // ed point -- we just use MICEX pxs and compute a single Qty:
    //
    PriceT refPx0 =
      passBid
      ? aggrOB0M->GetBestBidPx()
      : aggrOB0M->GetBestAskPx();

    // XXX: Beware of unavailable BestPxs!
    QtyO aggrQty0 =
      IsFinite(refPx0)
      ? QtyO(std::max<long>
        (long(Round(double(a_pass_qty) * double(a_pass_px) / double(refPx0))),
         1L))
      : QtyO();

    // NB: "refPx0" is the L1 px on the target OrderBook side (ie we will hit
    // or lift that side)  -- but it is NOT the likely exec px on that  side;
    // for the latter, we will have to use the VWAP. Still, to compute the Qty,
    // it should be OK.

    // Now the likely Exec Px(s):
    // (*) For MICEX  we use VWAP of "aggrQty0" with 50% contingency (XXX: this
    //     is currently non-configurable, and may differ from how the ReserveQs
    //     are calculated);
    // (*) for AlfaFIX2 we assume that the L1 band is always wide enough;
    // NB: In both cases, the pxs are taken on the side INTO which we will
    //     aggress, so OPPOSITE to aggrBids[0], so actually on "passBid":
    //
    PriceT likelyExecPx0M =
      IsPos(aggrQty0)
      ? GetVWAP(T, I^1, passBid, (aggrQty0 * 3L) / 2L)
      : PriceT();

    // For G==1, we currently always use AC on AlfaFIX2; for likely Exec Px,
    // again take the L1 of the side INTO which we will aggress:
    SecDefD   const* aggrInstr1    = m_AC;
    OrderBook const* aggrOB1       = m_orderBooks[8];
    PriceT           likelyExecPx1 =
      aggrBids[1]
      ? aggrOB1->GetBestAskPx()
      : aggrOB1->GetBestBidPx();
      // Can be NaN as well...

    // Predicted PnL (NB: can be NaN if any of the LikeleyExecPxs was missing!):
    double passCash   =
      passBid     ? (- double(a_pass_px))      : double(a_pass_px);

    double aggrCash0M =
      aggrBids[0] ? (- double(likelyExecPx0M)) : double(likelyExecPx0M);

    double predPnLM   =
      (I == AB)
      ? (passCash + aggrCash0M * double(likelyExecPx1))
      : (passCash + aggrCash0M / double(likelyExecPx1));

    double predPnLA   = NaN<double>;   // (This is for logging only)

    // So initially, assume routing to MICEX (G==0):
    bool             useMICEX0     = true;
    SecDefD   const* aggrInstr0    = aggrInstr0M;
    OrderBook const* aggrOB0       = aggrOB0M;
    PriceT           likelyExecPx0 = likelyExecPx0M;  // Can be NaN
    double           predPnL       = predPnLM;        // Can be NaN

    // If Optimal Routing is used, compute prospectve PnL conditional on cover-
    // ing on MICEX and AlfaFIX2 (for G==0), and select the best route:
    // XXX: However, the best route for G==0 is NOT selected if "m_useOptimal-
    // Routing" is off, or if the best AlfaFIX2 px on the corresp side is worse
    // than the best MICEX px (even though  it could still  be better than the
    // VWAP MICEX px!), because in that case, we can "hit badly" the AlfaFIX2:
    // XXX: Ideally we should do this using Continuous Price Prediction  -- to
    // predict the Exec Pxs of the 2 aggressive legs by the time when they get
    // filled. This is not implemented yet:  we calculate  the prospective PnL
    // based on the curr pxs:
    // XXX: "aggrOB0A" can be NULL if the corresp instrument is de-configured
    // for some reason in AlfaCN:
    if (m_useOptimalRouting)
    {
      OrderBook const* aggrOB0A = m_orderBooks[2*T + (I^1) + 4];

      PriceT likelyExecPx0A      =
        (aggrOB0A != nullptr)
        ? (aggrBids[0]
          ? aggrOB0A->GetBestAskPx()
          : aggrOB0A->GetBestBidPx())
        : PriceT();
        // NB: "likelyExecPx0A" can be NaN even if "aggrOB0A" is available (but
        // contains no liquidity...)

      bool canUse0A =
        (aggrOB0A != nullptr)
        ? (// Can use "0A" route if we Buy aggressively and the "A" px is lower
           // than the best "M" px:
          ( aggrBids[0] && likelyExecPx0A < refPx0) ||
           // ... or if we Sell aggressively and the "A" px is higher than the
           // best "M" px:
          (!aggrBids[0] && likelyExecPx0A > refPx0))
        : false;

      // But always use AlfaFIX2 covering if MICEX covering is unavailable at
      // all:
      if (utxx::unlikely(!(IsFinite(predPnLM) && IsPos(aggrQty0))))
        canUse0A = true;

      if (canUse0A)
      {
        SecDefD const* aggrInstr0A = m_aggrInstrs[T][I^1];
        assert(aggrInstr0A != nullptr);
        // Because OrderBook does exist and features all pxs

        // Once again, here L1 Pxs are used, because the quotes are banded, and
        // are supposed to be sufficiently wide:

        // Calculate the prospective PnL (per 1 triangle) in both cases (using
        // the notional RUB "cash" to be released after triangle completion);
        // NB: "likelyExecPx1" is a scaling factor which is always > 0  :
        double aggrCash0A =
          aggrBids[0] ? (- double(likelyExecPx0A)) : double(likelyExecPx0A);

        predPnLA =
          (I == AB)
          ? (passCash + aggrCash0A * double(likelyExecPx1))
          : (passCash + aggrCash0A / double(likelyExecPx1));

        // NB: If "likelyExecPx0A" is for any reason unavailable, then the fol-
        // lowing cond will be false, so we will continute to use the MICEX
        // route;
        // but if MICEX pxs are unavailable, we switch to AlfaFIX2:
        if (predPnLA > predPnLM || !IsFinite(predPnLM))
        {
          // Re-calculate "refPx0" and "aggrQty0":
          refPx0   = likelyExecPx0;
          aggrQty0 =
            IsFinite(refPx0)
            ? QtyO(std::max<long>
              (long(Round(double(a_pass_qty) * double(a_pass_px) /
                          double(refPx0))),
               1L))
            : QtyO();

          aggrInstr0    = aggrInstr0A;
          aggrOB0       = aggrOB0A;
          likelyExecPx0 = likelyExecPx0A;
          predPnL       = predPnLA;
          useMICEX0     = false;
        }
      }
      // XXX: In rare cases when "likelyExecPx*" is unavailable, we throw a cri-
      // tical error, because this means that the corresp liquidity has evapora-
      // ted:
      if (utxx::unlikely
         (!IsFinite(likelyExecPx0) || !IsFinite(likelyExecPx1) ||
          !IsFinite(predPnL)       || !IsPos(aggrQty0)))
      {
        LOG_CRIT(1,
          "TriArb2S::DoCoveringOrders: Liquidity Evaporated: {}: {}; {}: {}; "
          "PredPnL={}, AggrQty0={}",
          aggrInstr0->m_Symbol.data(), double(likelyExecPx0),
          aggrInstr1->m_Symbol.data(), double(likelyExecPx1),
          predPnL,                     QRO(aggrQty0))
        m_logger->flush();

        DelayedGracefulStop(a_ts_md_exch, a_ts_md_conn, a_ts_md_strat);
        return;
      }
    }
    //-----------------------------------------------------------------------//
    // But is PredPnL going to be positive at all?                           //
    //-----------------------------------------------------------------------//
    // If not, we better NOT complete the triangle at all,   but simply close
    // (again with optimal routing) the passive position obtained.  XXX: More
    // sophisticated logic may be required here, incl Continuous Price Predic-
    // tion:
    if (predPnL < 0.0)
    {
      // NB: VWAP is to be taken on the side into which we will aggress (so same
      // as "passBid"):
      QtyO             aggrQty       = a_pass_qty;
      bool             aggrBid       = !passBid;
      SecDefD   const* instrM        = m_passInstrs[T][I];
      OrderBook const* obM           = m_orderBooks[2*T + I];
      PriceT           likelyExecPxM = GetVWAP(T, I, passBid, aggrQty);
      assert(instrM == a_pass_aos.m_instr);   // Same as passive one!

      SecDefD   const* instrA        = m_aggrInstrs[T][I];
      OrderBook const* obA           = m_orderBooks[2*T + I + 4];
      assert(m_useOptimalRouting == (instrA != nullptr && obA != nullptr));

      PriceT           likelyExecPxA =
        m_useOptimalRouting
        ? (aggrBid ? obA->GetBestAskPx() : obA->GetBestBidPx())
        : PriceT();

      // If the Aggr Order  is Buy, select the lower px; otherwise, higher px:
      // NB: (^ (!aggrBid)) is same as (^ passBid):
      // NB: "instrA" may be unavailble at all (deliberately de-configured), in
      // which case always use MICEX:
      bool useMICEX  =
        (!m_useOptimalRouting) || ((likelyExecPxM <= likelyExecPxA) ^ passBid);

      // After all, the selected "aggrInstr" and "aggrOB" must be available:
      SecDefD   const* aggrInstr  =  useMICEX ? instrM : instrA;
      OrderBook const* aggrOB     =  useMICEX ? obM    : obA;
      assert(aggrInstr != nullptr && aggrOB != nullptr &&
            &(aggrOB->GetInstr()) == aggrInstr);

      PriceT aggrPx = MkAggrPx(aggrBid, *aggrOB);

      // IsAggr=true;
      AOS const* aggrAOS =
        useMICEX
        ? NewOrderSafe<true>
          (GetOMCMX(a_ts_md_strat),    *aggrInstr,   aggrBid, aggrPx, aggrQty,
           a_ts_md_exch, a_ts_md_conn, a_ts_md_strat,   T, I, tri)

        : NewOrderSafe<true>
          (m_omcAlfa,    *aggrInstr,   aggrBid, aggrPx, aggrQty,
           a_ts_md_exch, a_ts_md_conn, a_ts_md_strat,   T, I, tri);

      if (aggrAOS != nullptr)
        LOG_INFO(2,
          "Unwinding Quote via {}, AggrAOSID={}, PassAOSID={}, AggrSymbol={}, "
          "{}: Qty={}, LikelyCoverPxs would be: MICEX={}, Alfa={}",
          useMICEX ? "MICEX" : "AlfaFIX2",  aggrAOS->m_id,    a_pass_aos.m_id,
          aggrInstr->m_Symbol.data(),       aggrBid ? "Bid" : "Ask",
          QRO(aggrQty),                     double(likelyExecPxM),
          double(likelyExecPxA))

      // All Done!
      m_logger->flush();
      return;
    }

    //-----------------------------------------------------------------------//
    // Otherwise: Generate 2 Aggressive Covering Orders:                     //
    //-----------------------------------------------------------------------//
    // NB: For AC, we currently never use MICEX:
    SecDefD   const* aggrInstrs   [2] { aggrInstr0,    aggrInstr1    };
    OrderBook const* aggrOBs      [2] { aggrOB0,       aggrOB1       };
    PriceT           likelyExecPxs[2] { likelyExecPx0, likelyExecPx1 };
    bool             useMICEX     [2] { useMICEX0,     false         };

    for (int G = 0; G < 2; ++G)
    {
      //---------------------------------------------------------------------//
      // Curr Aggressive Instrument:                                         //
      //---------------------------------------------------------------------//
      PriceT aggrPx      = MkAggrPx(aggrBids[G], *(aggrOBs[G]));

      // NB: "tri" may be NULL, but very unlikely so:
      bool       cancelObstr = false;
      AOS const* obstrAOS    = nullptr;
      PriceT     obstrPx;      // NaN
      QtyO       aggrQty;      // 0

      //---------------------------------------------------------------------//
      // Aggressive Qty: This is Non-Trivial:                                //
      //---------------------------------------------------------------------//
      if (G == 0)
      {
        //-------------------------------------------------------------------//
        // Passive=AB or CB, Covering=CB or AB, resp:                        //
        //-------------------------------------------------------------------//
        aggrQty = aggrQty0;

        // Do we have a passive order standing in the way of the aggr one?  --
        // If so, to avoid crossing with our own order (which would have nega-
        // tive P&L impact), cancel the passive order first.
        // The obstructing order would be on the side opposite to "aggrBid" ie
        // in this case, on the "passBid" side; the instr is opposite to "I":
        //
        obstrAOS      = m_passAOSes[T][I^1][passBid];
        QtyO obstrQty;  // 0;  Not used
        GetLastQuote(obstrAOS, &obstrPx, &obstrQty);

        // Even if "obstrPx" exists (as is most likely the case), we need to
        // compare it with the likely price level we are going to hit  -- if
        // the latter is deeper, no obstruction will occur. Otherwise,  need
        // to cancel the passive order first:
        if (IsFinite(obstrPx) &&
           (( aggrBids[G] && likelyExecPxs[G] >= obstrPx) ||
            (!aggrBids[G] && likelyExecPxs[G] <= obstrPx)) )
        {
          // This must be a MICEX OMC:
          (void) CancelOrderSafe<true>       // Buffered!
                (GetOMCMX(obstrAOS), obstrAOS,
                 a_ts_md_exch,   a_ts_md_conn,  a_ts_md_strat);
          cancelObstr = true;
        }
      }
      else
      {
        //-------------------------------------------------------------------//
        // Passive=AB or CB, Covering=AC (G==1):                             //
        //-------------------------------------------------------------------//
        // NB: In this case,  because we do NOT quote AC, there could not be
        // any passive orders standing in the way  -- so don't need "likely-
        // ExecPx":
        assert(G == 1);

        // Calculate Qty(AC):
        double dqAC =
          double(a_pass_qty) * double(a_pass_px) / double(likelyExecPx0);

        if (I == AB)
          dqAC /= double(likelyExecPx1);  // NB: This is not trivial, but true!

        // And now round it up:
        aggrQty = QtyO(std::max<long>(long(Round(dqAC)), 1L));
      }
      // So "aggrQty" must be available now:
      assert(IsPos(aggrQty));

      //---------------------------------------------------------------------//
      // Now actually place the Aggressive Order:                            //
      //---------------------------------------------------------------------//
      // NB:
      // (*) For G==0 (MICEX), the default VirtSettlDate (same as normal Settl-
      //     Date) would so;
      // (*) For G==1 (AlfaFIX2), need to install explicit VirtSettlDate by pro-
      //     pagating it from the corresp Passive Quote:
      // IsAggr=true:
      AOS const* aggrAOS =
        useMICEX[G]
        ? NewOrderSafe<true>
          // Select the MICEX OMC randomly:
          (GetOMCMX(a_ts_md_strat), *(aggrInstrs[G]), aggrBids[G],
           aggrPx,                  aggrQty,
           a_ts_md_exch,            a_ts_md_conn,     a_ts_md_strat, T, I, tri)
        : NewOrderSafe<true>
          (m_omcAlfa,               *(aggrInstrs[G]), aggrBids[G],
           aggrPx,                  aggrQty,
           a_ts_md_exch,            a_ts_md_conn,     a_ts_md_strat, T, I, tri);

      if (utxx::unlikely(aggrAOS == nullptr))
        // NB: error condition would already be handled... Still try next "G"
        // if any:
        continue;

      // Update the "TrInfo" and install a ptr to it in "aggrAOS" so we will
      // know the triangle to which the corresp aggr fill will belong to:
      if (utxx::likely(tri != nullptr))
      {
        tri->m_aggrAOS[G] = aggrAOS;
        tri->m_aggrQty[G] = aggrQty;
      }

      //---------------------------------------------------------------------//
      // Log the order made:                                                 //
      //---------------------------------------------------------------------//
      // NB: The AOSID logged is that of the Passive Order filled, for ease
      // of tracing the order dependencies:
      //
      if (utxx::unlikely(cancelObstr))
      {
        assert(obstrAOS != nullptr && IsFinite(obstrPx));
        LOG_INFO(3,
          "Cancellig Quote (X-Prevention): ObstrAOSID={}, Symbol={}, {}: "
          "ObstrPx={}, NewLikelyPx={}",
          obstrAOS->m_id,     obstrAOS->m_instr->m_Symbol.data(),
          obstrAOS->m_isBuy ? "Bid" : "Ask",     double(obstrPx),
          double(likelyExecPxs[G]))
      }
      LOG_INFO(3,
        "Aggressive Covering Order on {}: AggrAOSID={}, PassAOSID={}, "
        "AggrSymbol={}, {}: Qty={}, LikelyAggrExecPxs={}/{}, PredPnL={}",
        useMICEX[G] ? "MICEX" : "AlfaFIX2", aggrAOS->m_id,  a_pass_aos.m_id,
        aggrInstrs[G]->m_Symbol.data(),     aggrBids [G] ? "Bid" : "Ask",
        QRO(aggrQty),                       double(likelyExecPx0),
        double(likelyExecPx1),              predPnL)
    }
    // End of "G" loop. All done.
  }

  //=========================================================================//
  // "AdjustCcyTails":                                                       //
  //=========================================================================//
  void TriArb2S::AdjustCcyTails
  (
    utxx::time_val  a_ts_md_exch,
    utxx::time_val  a_ts_md_conn,
    utxx::time_val  a_ts_md_strat
  )
  {
    //-----------------------------------------------------------------------//
    // BEWARE!!!                                                             //
    //-----------------------------------------------------------------------//
    // This method is potentially dangerous -- it is the ONLY point in the Stra-
    // tegy where Aggressive Orders are generated by a mathematical signal rath-
    // er than as covers for Passive Orders. Thus, there is a potential risk of
    // "uncontrolled "flood" of Aggreessive Orders here. Thus, this method needs
    // to be protected by a separate throttler:
    //
    if (utxx::unlikely
       (m_tailsThrottler.running_sum() >
        TailsThrottlerMaxAdjsPerPeriod))
    {
      LOG_WARN(2,
        "AdjustCcyTails: Too Frequent Invocations per period ({} sec): {}; "
        "MaxAllowed={}",
        TailsThrottlerPeriodSec, m_tailsThrottler.running_sum(),
        TailsThrottlerMaxAdjsPerPeriod)
      return;
    }
    // If OK: Update the Throttler:
    m_tailsThrottler.add(a_ts_md_strat);

    //-----------------------------------------------------------------------//
    // Traverse all Ccy Positions:                                           //
    //-----------------------------------------------------------------------//
    // FIXME: Here we always use TOM for covering orders, because it is always
    // available -- but this can create extra swap risks!
    //
    auto const& ars = m_riskMgr->GetAllAssetRisks(0);

    for (auto const& arp: ars)
    {
      AssetRisks const& ar = arp.second;
      //---------------------------------------------------------------------//
      // Analyse the Ccy Position:                                           //
      //---------------------------------------------------------------------//
      // NB: USD positions are Risk-Free, so ignore them:
      if (ar.m_isRFC)
        continue;

      // XXX: IMPORTANT: Do NOT use "ar.GetRFC()" to decide whether the corresp
      // risky Ccy "tail" needs to be adjusted, because this value (though it's
      // correct for PnL accounting) may imply  a  hugely-skewed Ccy/USD  exch
      // rate (due to the effect of multiple  Ccy strips with different Settl-
      // Dates and opposite positions), and could then give rise to an infini-
      // te loop of tail reductions (because none of them  would  reduce  "ar.
      // GetRFC()" below the threshold required). Use ToRFC(ar.GetPos()) inste-
      // ad, computed using the corresp TOM rate (always available):
      // NB: In "ToRFC", FullReCalc = false:
      //
      double tailUSD                = 0.0;
      constexpr double TailLimitUSD = 2000.0;

      // Select the Instrument to be used for Tail Adjustment. XXX: For the mo-
      // ment, we will just do an aggressive adjustment order on AlfaFIX2:
      SecDefD   const* adjInstr     = nullptr;
      QtyO             adjQty;      // 0
      bool             adjBuy       = false;
      OrderBook const* adjOB        = nullptr;

      if (strcmp(ar.m_asset.data(), m_EUR.data()) == 0)
      {
        // Use AC:
        assert(m_risksEUR_TOM != nullptr);
        (void) m_risksEUR_TOM->ToRFC<false>
          (ar.GetTrdPos(), a_ts_md_strat, &tailUSD);

        if (Abs(tailUSD) < TailLimitUSD)
          continue;

        adjInstr = m_AC;     // Same for TOD and TOM -- it's actually SPOT!
        adjQty   = QtyO(std::max<long>(long(Round(Abs(ar.GetTrdPos()))), 1000));
        adjBuy   = (ar.GetTrdPos() < 0);
        adjOB    = m_orderBooks[8];
        // NB: Buy EUR/USD (ie buy EUR, sell USD) if USD pos is SHORT
      }
      else
      if (strcmp(ar.m_asset.data(), m_RUB.data()) == 0)
      {
        // Use CB:
        assert(m_risksRUB_TOM != nullptr);
        (void) m_risksRUB_TOM->ToRFC<false>
          (ar.GetTrdPos(), a_ts_md_strat, &tailUSD);

        if (Abs(tailUSD) < TailLimitUSD)
          continue;

        // Normally, use an Aggressive (AlfaFIX2) instrument for Tail Adjust,
        // but if it is not available, use the corresp MICEX instr:
        adjInstr =
          m_useOptimalRouting
          ? m_aggrInstrs[TOM][CB]
          : m_passInstrs[TOM][CB];

        adjOB    =
          m_useOptimalRouting
          ? m_orderBooks[2 * TOM + CB + 4]
          : m_orderBooks[2 * TOM + CB];

        adjQty   = QtyO(std::max<long>(long(Round(Abs(tailUSD))), 1000));
        adjBuy   = (ar.GetTrdPos() > 0);
        // NB: Buy USD/RUB (ie buy USD, sell RUB) if RUB pos is LONG
      }
      else
        // This must never happen:
        throw utxx::logic_error
              ("TriArb2S::AdjustCcyTails: UnExpected Ccy=", ar.m_asset.data());

      // NB: "adjOB" and "adjInstr" should be non-NULL, but check that once
      // again:
      assert(adjInstr != nullptr && adjOB  != nullptr &&
           &(adjOB->GetInstr()) == adjInstr );

      //---------------------------------------------------------------------//
      // Do an Aggressive Adjustment Order:                                  //
      //---------------------------------------------------------------------//
      // Calculate the Aggressive Px:
      assert(IsPos(adjQty)  && adjInstr != nullptr);
      PriceT adjPx  = MkAggrPx(adjBuy, *adjOB);

      // IsAggr=true, no Passive-related info here:
      // (*) Use AlfaFIX2 Connector if OptimalRouting is set, MICEX otherwise
      //     (which would typically result in higher spread  costs of an aggr
      //     order);
      // (*) but EUR covering is ALWAYS on AlfaECN:
      AOS const* adjAOS =
        (strcmp(ar.m_asset.data(), m_EUR.data()) == 0 || m_useOptimalRouting)
        ? NewOrderSafe<true>
          (m_omcAlfa,      *adjInstr,     adjBuy,  adjPx, adjQty,
           a_ts_md_exch,   a_ts_md_conn,  a_ts_md_strat,  -1, -1, nullptr)
        : NewOrderSafe<true>
          (m_omcMICEX0,    *adjInstr,     adjBuy,  adjPx, adjQty,
           a_ts_md_exch,   a_ts_md_conn,  a_ts_md_strat,  -1, -1, nullptr);

      // Log the order made:
      // NB: The AOSID logged is that of the Passive Order filled, for ease
      // of tracing the order dependencies:
      if (adjAOS != nullptr)
        LOG_INFO(1,
          "Tails Adjustment Order: Ccy={}: Pos={}, AOSID={}, Symbol={}, {}: "
          "Qty={}", ar.m_asset.data(), ar.GetTrdPos(),         adjAOS->m_id,
          adjInstr->m_Symbol.data(),   adjBuy ? "Bid" : "Ask", QRO(adjQty))
    }
    // End of "AssetTotalRisks" loop
    // All Done!
  }

  //=========================================================================//
  // "MkAggrPx":                                                             //
  //=========================================================================//
  // FIXME: Must use Exchange-provided Px Limits rather than  our guesses here,
  // otherwise the computed px could be too passive, or too aggressive (outside
  // the limits):
  //
  PriceT TriArb2S::MkAggrPx(bool a_is_buy, OrderBook const& a_ob)
  {
    double pxStep = a_ob.GetInstr().m_PxStep;
    return
      a_is_buy
      ? RoundUp  (1.01 * a_ob.GetBestAskPx(), pxStep)   // + 1%
      : RoundDown(0.99 * a_ob.GetBestBidPx(), pxStep);  // - 1%
  }

  //=========================================================================//
  // "NewOrderSafe": Exception-Safe Wrapper:                                 //
  //=========================================================================//
  // NB:
  // (*) OMC is a template param, to avoid virtual calls;
  // (*) "a_T" and "a_I", if valid, are the Tenor and Instr of a Passive Order;
  //     if this order is not passive (IsAggr=true), then they correspond to a
  //     passive order being covered; they are NOT available for Tails-Adj  or-
  //     ders:
  //
  template<bool IsAggr, typename OMC>
  AOS const* TriArb2S::NewOrderSafe
  (
    OMC&            a_omc,
    SecDefD const&  a_instr,
    bool            a_is_buy,
    PriceT          a_px,
    QtyO            a_qty,
    utxx::time_val  a_ts_md_exch,
    utxx::time_val  a_ts_md_conn,
    utxx::time_val  a_ts_md_strat,
    int             a_passT,            // For Aggr Orders only
    int             a_passI,            // For Aggr Orders only
    TrInfo*         a_tri               // For Aggr Orders only
  )
  {
    //-----------------------------------------------------------------------//
    // Safe Mode?                                                            //
    //-----------------------------------------------------------------------//
    // Then do not even try to issue a new order -- it will be rejected by the
    // RiskMgr anyway.  Initiate graceful shut-down instead:
    //
    if (utxx::unlikely(m_riskMgr->GetMode() == RMModeT::Safe))
    {
      if (m_delayedStopInited.empty() && !m_nowStopInited)
      {
        WriteStatusLine("NewOrder: RiskMgr has entered SafeMode");
        DelayedGracefulStop(a_ts_md_exch, a_ts_md_conn, a_ts_md_strat);
      }
      return nullptr;
    }

    //-----------------------------------------------------------------------//
    // Stopping?                                                             //
    //-----------------------------------------------------------------------//
    // If so, Passive Orders are not allowed (we should have already issued
    // Cancellation reqs for all Passive Quotes), but Aggressive Orders may
    // in theory happen. For Passive Orders, no exception occurs (so not to
    // complicate the termination procedure) -- we simply ignore them:
    //
    if (utxx::unlikely(!(IsAggr || m_delayedStopInited.empty())))
      return nullptr;

    //-----------------------------------------------------------------------//
    // Actually issue a new Order:                                           //
    //-----------------------------------------------------------------------//
    // Capture and log possible "NewOrder" exceptions, then terminate the
    // Strategy. NB: BatchSend=false:
    AOS const* res = nullptr;
    try
    {
      res = a_omc.NewOrder
      (
        this,          a_instr,      FIX::OrderTypeT::Limit,
        a_is_buy,      a_px,         IsAggr,          QtyAny(a_qty),
        a_ts_md_exch,  a_ts_md_conn, a_ts_md_strat,   false,
        FIX::TimeInForceT::Day,      0
      );
    }
    catch (EPollReactor::ExitRun const&)
    {
      // This exception is always propagated:
      throw;
    }
    catch (std::exception const& exc)
    {
      LOG_ERROR(1,
        "TriArb2S::NewOrderSafe: {} Order FAILED: Symbol={}, {}, Px={}, Qty="
        "{}: {}", IsAggr ? "Aggressive" : "Passive", a_instr.m_Symbol.data(),
        a_is_buy ? "Bid" : "Ask",   double(a_px),    QRO(a_qty),  exc.what())
      m_logger->flush();

      // NewOrder failed -- initiate stopping:
      DelayedGracefulStop(a_ts_md_exch, a_ts_md_conn, a_ts_md_strat);
      return nullptr;
    }

    //-----------------------------------------------------------------------//
    // Special Mgmt for Aggr Orders:                                         //
    //-----------------------------------------------------------------------//
    // (Passive Orders are post-managed by the Caller):
    assert(res != nullptr);
    if (IsAggr)
    {
      // XXX: "a_pass[TI]" may actually be invalid:
      if (a_passT != TOD && a_passT != TOM)
        a_passT = -1;
      if (a_passI != AB  && a_passI != CB)
        a_passI = -1;

      // Create new OrderInfo and mark this Order as Aggessive. This is CRITIC-
      // ALLY IMPORTANT, because otherwise, after Fill, the Order will be cons-
      // idered Passive, and an attempt will be made to cover it, thus creating
      // inifinite recusrion of Aggressive Orders eating up the spread!!!
      //
      OrderInfo ori;
      ori.m_isAggr = true;    // IMPORTANT!!!
      ori.m_passT  = a_passT;
      ori.m_passI  = a_passI;
      ori.m_trInfo = a_tri;   // May be NULL

      res->m_userData.Set(ori);

      // Save this "aggrAOS" on the transient vector (XXX: it is long enough so
      // it will never overflow):
      m_aggrAOSes.push_back(res);
    }
    return res;
  }

  //=========================================================================//
  // "ModifyOrderSafe": Exception-Safe Wrapper:                              //
  //=========================================================================//
  // OMC is a template param, to avoid virtual calls:
  //
  template<typename OMC>
  inline bool TriArb2S::ModifyOrderSafe
  (
    OMC&            a_omc,
    AOS const*      a_aos,      // Non-NULL
    PriceT          a_px,
    QtyO            a_qty,
    utxx::time_val  a_ts_md_exch,
    utxx::time_val  a_ts_md_conn,
    utxx::time_val  a_ts_md_strat
  )
  {
    assert(a_aos != nullptr     && IsFinite(a_px)    &&     IsPos(a_qty)  &&
         !(a_aos->m_isInactive) && a_aos->m_isCxlPending == 0);

    //-----------------------------------------------------------------------//
    // Safe Mode or Stopping?                                                //
    //-----------------------------------------------------------------------//
    // Again, check for Safe Mode. If this is the case, do not even try to do a
    // modification -- it will be rejected by the RiskMgr anyway. Initiate gra-
    // ceful shut-down instead:
    //
    if (utxx::unlikely(m_riskMgr->GetMode() == RMModeT::Safe))
    {
      if (m_delayedStopInited.empty())
      {
        WriteStatusLine("ModifyOrder: RiskMgr has entered SafeMode");
        DelayedGracefulStop(a_ts_md_exch, a_ts_md_conn, a_ts_md_strat);
      }
      return false;
    }

    // Modifications are not allowed (silently ignored) in  the Stopping  Mode
    // (because we should have issued Cancellation Reqs for all Passive Quotes
    // before entering that mode anyway):
    //
    if (utxx::unlikely(!m_delayedStopInited.empty()))
      return false;

    //-----------------------------------------------------------------------//
    // Generic Case:                                                         //
    //-----------------------------------------------------------------------//
    SecDefD const& instr = *(a_aos->m_instr);
    try
    {
      // NB: IsAggr=false, BatchSend=false:
      bool ok =
        a_omc.ModifyOrder
        (
          a_aos,  a_px, false,        QtyAny(a_qty),
          a_ts_md_exch, a_ts_md_conn, a_ts_md_strat, false
        );

      // "ModifyOrder" SHOULD NOT return "false" in this case, because pre-conds
      // were satisfied -- but in practice, it can sometimes happen that the or-
      // der could not be modified for some reason. Most likely,  we got the Px
      // unchanged despite trying to avoid such situations.  This may happen if
      // a previous Modify Req has failed (eg the new intended px was   out-of-
      // range due to OrderBook issues) but we used the intended px as  a  base
      // for further modifications. To be on a safe side, cancel the curr order
      // in that case:
      if (utxx::unlikely(!ok))
      {
        (void) CancelOrderSafe<false>  // Non-Buffered
               (a_omc, a_aos, a_ts_md_exch, a_ts_md_conn, a_ts_md_strat);

        LOG_WARN(2,
          "TriArb2S::Quote: ModifyOrderSafe: Returned False: Symbol={}, {}: "
          "Px={}, Qty={}, AOSID={}: Cancelling this Order...",
          instr.m_Symbol.data(), a_aos->m_isBuy ? "Bid" : "Ask",
          double(a_px),          QRO(a_qty),       a_aos->m_id)

        m_logger->flush();
        return false;
      }
    }
    catch (EPollReactor::ExitRun const&)
    {
      // This exception is always propagated:
      throw;
    }
    catch (std::exception const& exc)
    {
      LOG_ERROR(1,
        "TriArb2S::Quote: ModifyOrderSafe: FAILED: Symbol={}, {}: Px={}, Qty="
        "{}, AOSID={}: {}",
        instr.m_Symbol.data(), a_aos->m_isBuy ? "Bid" : "Ask",  double(a_px),
        QRO(a_qty),            a_aos->m_id,     exc.what())
      m_logger->flush();

      // ModifyOrder failed -- initiate stopping:
      DelayedGracefulStop(a_ts_md_exch, a_ts_md_conn, a_ts_md_strat);
      return false;
    }

    // If OK:
    return true;
  }

  //=========================================================================//
  // "CancelOrderSafe": Exception-Safe Wrapper:                              //
  //=========================================================================//
  // OMC is a template param, to avoid virtual calls:
  // If the order is already Inactive or PendingCancel, it's perfectly OK, so
  // return "true"; "false" is returned only in case of an exception caught:
  //
  template<bool Buffered, typename OMC>
  inline bool TriArb2S::CancelOrderSafe
  (
    OMC&            a_omc,        // NB: Ref!
    AOS const*      a_aos,        // Non-NULL
    utxx::time_val  a_ts_md_exch,
    utxx::time_val  a_ts_md_conn,
    utxx::time_val  a_ts_md_strat
  )
  {
    assert(a_aos != nullptr);
    if (utxx::unlikely(a_aos->m_isInactive || a_aos->m_isCxlPending != 0))
      return true;

    // NB: Cancellations are always allowed (even if "m_delayedStopInited"  is
    // set, or the RiskMgr has entered Safe Mode). So always the Generic Case:
    // BatchSend=Buffered:
    try
    {
      DEBUG_ONLY(bool ok =)
      a_omc.CancelOrder
        (a_aos, a_ts_md_exch, a_ts_md_conn, a_ts_md_strat, Buffered);

      // "CancelOrder" cannot return "false" in this case, because pre-conds
      // were satisfied:
      assert(ok);
    }
    catch (EPollReactor::ExitRun const&)
    {
      // This exception is always propagated -- even for "Cancel":
      throw;
    }
    catch (std::exception const& exc)
    {
      SecDefD const& instr = *(a_aos->m_instr);
      LOG_ERROR(1,
        "TriArb2S::CancelOrderSafe: CancelOrder FAILED: Symbol={}, {}: AOSID="
        "{}: {}",    instr.m_Symbol.data(),    a_aos->m_isBuy ? "Bid" : "Ask",
        a_aos->m_id, exc.what())
      m_logger->flush();

      // CancelOrder failed -- initiate stopping:
      DelayedGracefulStop(a_ts_md_exch, a_ts_md_conn, a_ts_md_strat);
      return false;
    }
    // NB: The AOS ptr is NOT reset to NULL yet --  will only do that when
    // the cancellation is confirmed (before that, "PendingCancel" flag is
    // set):
    return true;
  }

  //========================================================================//
  // "GetLastQuote":                                                        //
  //========================================================================//
  // Fill in the info on an existing active quote (if any) for a given AOS:
  //
  inline void TriArb2S::GetLastQuote
    (AOS const* a_aos,  PriceT* a_px,  QtyO* a_qty)
  {
    assert(a_px != nullptr && a_qty != nullptr);

    // NB: "a_aos" may be NULL or invalid, in which case empty values are retu-
    // rned. Otherwise, consider the generic case:
    if (utxx::likely
       (a_aos != nullptr     &&
      !(a_aos->m_isInactive) && a_aos->m_isCxlPending == 0))
    {
      // Get the last Req (XXX: This could lead to trouble if it later gets re-
      // jected, but that is quite unlikely). NB: "lastReq" must NOT be a Canc-
      // el because the AOS itself is not "CxlPending", but we better check it
      // explicitly anyway:
      Req12 const* lastReq = a_aos->m_lastReq;
      assert(lastReq != nullptr);
      if (utxx::likely
         (lastReq->m_kind != Req12::KindT::Cancel &&
          IsFinite(lastReq->m_px) && IsPos(lastReq->m_qty)))
      {
        *a_px  = lastReq->m_px;
        *a_qty = lastReq->GetQty<QTO,QRO>();
        return;
      }
    }
    // If we fell through to this point: The result is empty:
    *a_px  = PriceT();
    *a_qty = QtyO();
  }

  //=========================================================================//
  // "GetVWAP":                                                              //
  //=========================================================================//
  PriceT TriArb2S::GetVWAP
  (
    int  a_T,
    int  a_I,
    bool a_IsBid,
    QtyO a_qty
  )
  const
  {
    // NB: VWAPs are ONLY computed on MICEX (where passive quoting occurs). So
    // we can always use Qty<QTM,QRM> for MktData here!
    //
    assert((a_T == TOD || a_T == TOM) && (a_I == AB || a_I == CB) &&
           IsPos(a_qty));

    // Get the OrderBook for "I":
    OrderBook const* ob  = m_orderBooks[2*a_T + a_I];
    assert(ob != nullptr);

    SecDefD   const* instr = &(ob->GetInstr());
    assert(instr == m_passInstrs[a_T][a_I]);

    // Do we have our own quote for [T,I,IsBid]? If so, its size is to be dis-
    // regarded in VWAP calculations:
    PriceT     ourPx;   // NaN
    QtyO       ourQty;  // 0
    AOS const* ourAOS = m_passAOSes[a_T][a_I][a_IsBid];
    assert(ourAOS == nullptr  || ourAOS->m_instr == m_passInstrs[a_T][a_I]);
    GetLastQuote(ourAOS, &ourPx, &ourQty);

    // VWAP params and results will be stored here. We specify all BandSizes
    // in QTO (Contracts) units, suitable for OMCs:
    OrderBook::ParamsVWAP<QTO> params;

    params.m_bandSizes[0]   = a_qty;
    params.m_exclLimitOrdPx = ourPx;
    params.m_exclLimitOrdSz = ourQty;
    params.m_manipRedCoeff  = m_manipDiscount;
    params.m_manipOnlyL1    = true;
    // XXX: For now; because single orders at other levels are typically placed
    // by retail clients, not manipulators...

    // Add info on currently-active Aggressive Orders targeting the same side
    // of this OrderBook:
    assert(params.m_exclMktOrdsSz == 0);
    for (AOS const* aos: m_aggrAOSes)
    {
      assert(aos != nullptr);
      if (aos->m_instr == instr && aos->m_isBuy == a_IsBid)
      {
        // Get the size of the most recent Req in this AOS:
        Req12* req   = aos->m_lastReq;
        assert(req  != nullptr);
        QtyO aggrQty = req->GetQty<QTO,QRO>();
        // Because  the Order is Aggressive, "aggrQty" must be > 0: it is never
        // a Cancel req:
        assert(IsPos(aggrQty));
        params.m_exclMktOrdsSz += aggrQty;
      }
    }
    // Now compute and return the VWAP for the "Reserve Qty" for "I". In doing
    // so, apply "reduction coeff" (it could even be 0) to *single* orders  at
    // each px level -- these could be orders placed by manipulators:
    if (a_IsBid)
      ob->GetVWAP<QTM, QRM, QTO, QRO, true> (&params);  // IsBid=true
    else
      ob->GetVWAP<QTM, QRM, QTO, QRO, false>(&params);  // IsBid=false

    return params.m_vwaps[0];
  }
} // End namespace MAQUETTE
