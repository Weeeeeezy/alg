// vim:ts=2:et
//===========================================================================//
//                  "TradingStrats/3Arb/TriArbC0/Orders.hpp":                //
//                            Order Entry Methods                            //
//===========================================================================//
#pragma  once
#include "TradingStrats/3Arb/TriArbC0/TriArbC0.h"
#include <ctime>

namespace MAQUETTE
{
  //=========================================================================//
  // "NewTrInfo":                                                            //
  //=========================================================================//
  template<ExchC0T X>
  template<int I,  int S>
  inline typename  TriArbC0<X>::TrInfo* TriArbC0<X>::NewTrInfo
  (
    utxx::time_val a_ts_exch,
    utxx::time_val a_ts_conn,
    utxx::time_val a_ts_strat
  )
  {
    // (I,S) are the PassiveLeg indices:
    static_assert(0 <= I && I <= 2 && 0 <= S && S <= 1);

    // Do we still have space in ShM?
    assert(*m_N3s <= *m_Max3s);
    if (utxx::unlikely(*m_N3s == *m_Max3s))
    {
      // No more space for TriAngles, will have to stop the Strategy because
      // cannot proceed with quoting:
      LOG_CRIT(1, "Quote: ShM Full: N={}, STOPPING...", *m_N3s)
      m_logger->flush();
      DelayedGracefulStop(a_ts_exch, a_ts_conn, a_ts_strat);
      return nullptr;
    }
    // If OK: take a new "TrInfo":
    TrInfo* tri = m_All3s + (*m_N3s);
    ++(*m_N3s);
    m_curr3s[I][S]   = tri;

    // Initialize the Passive Leg flds of "TrInfo", but the PassPx, AOS etc
    // are unset yet:
    tri->m_id        = *m_N3s;          // 1-based; 0 means empty
    tri->m_passI     = I;
    tri->m_passS     = S;
    tri->m_passInstr = m_instrs[I];
    tri->m_passQty   = m_qtys  [I];
    assert(tri->m_passAOS == nullptr);  // As yet

    // Initialize the AggrLegs (to the extent possible at this point), XXX:
    // In the following, we preserve the invariant I0 < I1, NOT  the cyclic
    // permutation property of AB, CB, AC:
    // CORRECTNESS OF THE FOLLOWING IS CRITICALLY IMPORTANT:
    // I   I0  I1
    // AB  CB  AC
    // CB  AB  AC
    // AC  AB  CB
    constexpr int I1 = (I == AB) ? CB : AB;
    constexpr int I2 = (I == AC) ? CB : AC;

    // NB: S0  is   always the same as S, because:
    // I = AB: I0 = CB, on the same side as AB;
    // I = CB: I0 = AB, on the same side as CB;
    // I = AC: I0 = AB, on the same side as AC:
    constexpr int S1 = S;

    // S1: here the situation is different (no symmetry!):
    // I = AB: I1 = AC, on the same side as AB;
    // I = CB: I1 = AC, on the OPP  side to CB;
    // I = AC: I1 = CB, on the OPP  side to AC:
    constexpr int S2 = (I == AB) ? S : (1-S);

    // Thus:
    tri->m_aggrIs    [0] = I;
    tri->m_aggrIs    [1] = I1;
    tri->m_aggrIs    [2] = I2;

    tri->m_aggrSs    [0] = 1-S; // NB: AggrClose side is OPPOSITE to Passive!
    tri->m_aggrSs    [1] = S1;
    tri->m_aggrSs    [2] = S2;

    tri->m_aggrInstrs[0] = m_instrs[I];
    tri->m_aggrInstrs[1] = m_instrs[I1];
    tri->m_aggrInstrs[2] = m_instrs[I2];
    return tri;
  }

  //=========================================================================//
  // "Quote":                                                                //
  //=========================================================================//
  // "I" is the index of the Passively-Quoted Instr, "IsBid" is Quote Side:
  //
  template<ExchC0T X>
  template<int I,  bool IsBid>
  void TriArbC0<X>::Quote
  (
    utxx::time_val  a_ts_exch,
    utxx::time_val  a_ts_conn,
    utxx::time_val  a_ts_strat
  )
  {
    static_assert(I == AB || I == CB || I == AC, "Quote: Invalid InstrIdx");

    //-----------------------------------------------------------------------//
    // Get the TriAngle (if exists; otherwise the ptr is NULL):              //
    //-----------------------------------------------------------------------//
    // NB: "TrInfo"s are arranged by Passive side. For Quoting (unlike reaction
    // on delayed Fills), we always use a Current "TrInfo":
    //
    constexpr int    S      = int(IsBid);
    TrInfo*          tri    = m_curr3s    [I][S];  // May be NULL
    SecDefD   const* instr  = m_instrs    [I];
    OrderBook const* ob     = m_orderBooks[I];
    assert(instr != nullptr && ob != nullptr);

    // Get the AOS (if available):
    AOS const* aos = utxx::likely(tri != nullptr) ? tri->m_passAOS : nullptr;

    // Obviously, the following invariants must hold:
    assert(tri == nullptr                                  ||
          (tri->m_passI     == I     && tri->m_passS == S  &&
           tri->m_passInstr == instr                       &&
          (aos == nullptr                                  ||
          (aos->m_isBuy     == IsBid && aos->m_instr == instr))));

    // Check the AOS status (if available):
    if (utxx::likely(aos != nullptr))
    {
      // If the corresp AOS is already Inactive for any reason, set the AOS and
      // "TrInfo" to to NULL to force construction of new ones.   NB: There may
      // still be updates to that "TrInfo", but it is no longer the current one.
      // XXX: A similar check is performed in "OnOurTrade"; two checks are pro-
      // vided for extra safety:
      if (aos->m_isInactive)
      {
        tri            = nullptr;
        aos            = nullptr;
        m_curr3s[I][S] = nullptr;
      }
      else
      // If the AOS is Pending Cancel,  don't do anything until the Cancel is
      // confirmed  (because, in some rare cases, it might NOT be canceled at
      // the at the end):
      if (utxx::unlikely(aos->m_isCxlPending != 0))
        return;
    }
    //-----------------------------------------------------------------------//
    // If there is no current "TrInfo", HERE we allocate a new one:          //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely(tri == nullptr))
    {
      assert(aos == nullptr); // Then there is no AOS either

      // The following also stores the "TrInfo" ptr in "m_curr3s":
      tri = NewTrInfo<I,S>(a_ts_exch, a_ts_conn, a_ts_strat);

      if (utxx::unlikely(tri == nullptr))
        return;               // We are already stopping...
    }
    assert(tri != nullptr && tri == m_curr3s[I][S]);

    //-----------------------------------------------------------------------//
    // Calculate the Quote Px:                                               //
    //-----------------------------------------------------------------------//
    // Synthetic px of the quoted instr (yet w/o mark-up or other adjustments).
    // The src pxs and qtys are stored in "TrInfo":
    //
    double synthPx =
      (I == AB)
      ? MkSynthPxAB<IsBid>(tri) :
      (I == CB)
      ? MkSynthPxCB<IsBid>(tri)
      : MkSynthPxAC<IsBid>(tri);

    // Apply the Mark-Up and Rounding:
    double mkUpCf = 1.0  + m_relMarkUps[I];
    assert(mkUpCf > 0.0);

    double pxStep = instr->m_PxStep;
    assert(pxStep > 0.0);

    // NB: "synthPx" is of Px(A/B) format, but for the following adjustments we
    // must convert it into the Px(Contract) format. XXX: As yet, these formats
    // are not distinguishable via the type system (unlike Qtys):
    PriceT px =
      IsBid
      ? RoundDown(instr->GetContractPx(PriceT(synthPx / mkUpCf)), pxStep)
      : RoundUp  (instr->GetContractPx(PriceT(synthPx * mkUpCf)), pxStep);

    //-----------------------------------------------------------------------//
    // Check the Pxs:                                                        //
    //-----------------------------------------------------------------------//
    // Cannot proceed if the calculated "px" is NaN (this might happen if there
    // is not enough liquidity for covering orders). In that case, any existing
    // quote is also cancelled for safety:
    //
    if (utxx::unlikely(!IsFinite(px)))
    {
      if (aos != nullptr)
        (void) CancelOrderSafe
               (aos, a_ts_exch,  a_ts_conn, a_ts_strat, "No Valid QuotePx");
      // Nothing more to do in this case:
      return;
    }
    // Otherwise: Generic Case: "px" is valid:
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
    //
    PriceT bestBidPx = ob->GetBestBidPx();
    PriceT bestAskPx = ob->GetBestAskPx();

    if (utxx::unlikely(bestBidPx < px && px < bestAskPx))
    {
      // Yes, "px" is strictly inside the Bid-Ask spread: Move it towards the
      // best level of the curr side:
      px = IsBid ? (bestBidPx + pxStep) : (bestAskPx - pxStep);

      // The adjusted "px" is STILL strictly within the Bid-Ask spread, because
      // in this case, the spread is at least 2 "pxStep"s:
      assert(bestBidPx < px && px < bestAskPx);
    }
    //-----------------------------------------------------------------------//
    // Al this point, "px" is FINAL:                                         //
    //-----------------------------------------------------------------------//
    // NB: tri->m_aggrSrcPxs[0] may be different (was not adjusted):
    // Save the old px, we will need it below:
    //
    PriceT  oldPx = instr->GetContractPx(tri->m_passPx); // Must be Contracts
    PriceT   pxAB = instr->GetPxAB(px);
    tri->m_passPx = pxAB;                                // Must be Px(A/B)

    //-----------------------------------------------------------------------//
    // Modify an Existing Active Quote?                                      //
    //-----------------------------------------------------------------------//
    bool modify   = (aos != nullptr);
    if (utxx::likely(modify))
    {
      // Then we have already checked that the AOS is indeed fully-active:
      assert(!(aos->m_isInactive || aos->m_isCxlPending != 0));

      // Because of that, "oldPx" must be valid:
      assert(IsFinite(oldPx));

      // Then decide whether we want to do this modification at all. XXX: for
      // now, we do NOT modify an existing quote iff both the CurrPx  and the
      // NewPx are located sufficiently deeply inside the OrderBook,  and the
      // magnitude of move is sufficienty small -- in that case, such a  move
      // would be irrelevant indeed. In all other cases, we DO the move:
      //
      // Has the quoted Px changed? (The Qty is always constant):
      int diff = int(Round(Abs(px - oldPx) / pxStep));
      if (diff == 0)
        // Then no modification at all is required:
        return;

      // Otherwise: Compute the "Distances into the Depth of the OrderBook" for
      // the curr and new pxs, in PxSteps.  NB: Unlike "diff", "depth"s  can be
      // of ANY sign:
      int currDepth =
        int(Round((IsBid ? (bestBidPx - oldPx) : (oldPx - bestAskPx))
                  / pxStep));
      int newDepth  =
        int(Round((IsBid ? (bestBidPx - px)    : (px    - bestAskPx))
                  / pxStep));
      int minDepth  = std::min<int>(currDepth, newDepth);

      if (utxx::likely
         ((1 <= minDepth && minDepth < m_resistDepths1[I] &&
           diff     <  m_resistDiffs1 [I])            ||
          (minDepth >= m_resistDepths1[I] && diff < m_resistDiffs2[I])))
        // Then no modification:
        return;

      // In all other cases, issue a modification req with the OMC.
      // NB: Use "px" here, NOT "pxAB":
      bool ok = ModifyOrderSafe
                (aos, px, m_qtys[I], a_ts_exch,  a_ts_conn, a_ts_strat);
      if (utxx::unlikely(!ok))
        return; // All error handling has already been done
    }
    else
    //-----------------------------------------------------------------------//
    // A New Quote?                                                          //
    //-----------------------------------------------------------------------//
    {
      // NB: Again, use "px", NOT "pxAB". The order is Passive (J=-1):
      NewOrderSafe<I, IsBid, -1>
        (tri, px, m_qtys[I], a_ts_exch,  a_ts_conn, a_ts_strat,
         "New Passive Quote");

      aos = tri->m_passAOS;
      if (utxx::unlikely(aos == nullptr))
        return;  // All handling has already been done. Possibly DryRun!
    }
    //-----------------------------------------------------------------------//
    // Log the Quote:                                                        //
    //-----------------------------------------------------------------------//
    assert(aos != nullptr);

    // In the Debug mode, verify the "TrInfo" -- it must be fully-consistent at
    // this point:
    tri->CheckInvariants();

    // The Req which was just submitted:
    Req12 const* req = aos->m_lastReq;
    assert(req != nullptr);

    // A fill becomes more likely if the quote falls onto, or in between,
    // BestBidPx .. BestAskPx:
    long lat_usec   = utxx::time_val::now_diff_usec(a_ts_conn);
    bool fillLikely = (bestBidPx <= px && px <= bestAskPx);

    LOG_INFO(2,
      "Passive Quote ({}): Symbol={}, AOSID={}, ReqID={}, {}: Px={}, BestPxs="
      "{}..{}, Qty={}, SrcPx1={}, SrcPx2={}, Latency={} usec{}",
      modify ? "Modify" : "New", aos->m_id,  req->m_id, instr->m_Symbol.data(),
      IsBid  ? "Bid"    : "Ask", double(px), double(bestBidPx),
      double(bestAskPx),  QR(m_qtys[I]),     double(tri->m_aggrSrcPxs[1]),
      double(tri->m_aggrSrcPxs[2]), lat_usec,   fillLikely ? ",!!!" : "")

    // All Done. No flush here -- this output is quite frequent...
  }

  //=========================================================================//
  // "ABCoveringOrders":                                                     //
  //=========================================================================//
  template<ExchC0T X>
  template<bool IsBidAB>
  void  TriArbC0<X>::ABCoveringOrders
  (
    TrInfo*         a_tri,      // Non-NULL
    PriceT          a_px_ab,    // Already normalised to Px(A/B)
    QtyN            a_qty_ab,
    utxx::time_val  a_ts_exch,
    utxx::time_val  a_ts_conn,
    utxx::time_val  a_ts_strat
  )
  {
    assert(IsPos(a_px_ab) && a_tri != nullptr && a_tri->m_passI == AB &&
           bool(a_tri->m_passS) == IsBidAB);

    // Re-compute the Qtys of Aggr Orders (CB and AC) and estimate the TriAngle
    // PnL (in units of B):
    // CB and AC sides for AggrOrders:
    constexpr bool  BuyAB   = !IsBidAB; // For aggressive closing of AB pos
    constexpr bool  BuyCB   = !IsBidAB;
    constexpr bool  BuyAC   = !IsBidAB;
    // CB and AC sides for VWAP:
    constexpr bool  IsBidCB = !BuyCB;
    constexpr bool  IsBidAC = !BuyAC;

    assert(BuyAB == a_tri->m_aggrSs[AB] && BuyCB == a_tri->m_aggrSs[CB] &&
           BuyAC == a_tri->m_aggrSs[AC]);

    QtyN      qAC = a_qty_ab;
    PriceT vwapAC = m_orderBooks[AC]->GetVWAP1<QTO,QRO,IsBidAC,QT,QR,true>(qAC);

    QtyN      qCB = a_qty_ab * double(vwapAC);
    PriceT vwapCB = m_orderBooks[CB]->GetVWAP1<QTO,QRO,IsBidCB,QT,QR,true>(qCB);

    // Projected PnL (in B) if we close the TriAngle, up to "a_qty_ab" factor;
    // we expect pnlB1 > 0:
    double  pnlB1 = double(vwapCB) * double(vwapAC) - double(a_px_ab);

    // Similar projected PnL (in B) if we simply close the AB pos aggressively
    // (so again SAME side: IsBidAB, thus theoretically, pnlB1 = 0):
    double  pnlB0 =
      double(m_orderBooks[AB]->GetVWAP1<QTO,QRO,IsBidAB,QT,QR,true>(a_qty_ab)) -
      double(a_px_ab);

    // NB: The sign factor of both PnLs depebds on the direction:
    if constexpr(!IsBidAB)
    {
      pnlB1 = - pnlB1;
      pnlB0 = - pnlB0;
    }
    // So what is the best way? XXX: It is possible that both PnLs are negative,
    // but we still select the "best" one:
    // TODO:
    // (*) Use predicted rather than actual VWAPs;
    // (*) Proper accounting for Trading Fees here:
    //
    if (pnlB1 > pnlB0)
    {
      // Complete the TriAngle: J=1 --> CB, J=2 --> AC:
      assert(a_tri->m_aggrInstrs[1] == m_instrs[CB] &&
             a_tri->m_aggrInstrs[2] == m_instrs[AC]);

      // Use ContractPxs here (they will be made "more aggressive" by "NewOrder-
      // Safe":
      NewOrderSafe<CB, BuyCB, 1>
        (a_tri, PriceT(), qCB, a_ts_exch, a_ts_conn, a_ts_strat,
         "Covering PassLegAB with AggrLegCB");
      NewOrderSafe<AC, BuyAC, 2>
        (a_tri, PriceT(), qAC, a_ts_exch, a_ts_conn, a_ts_strat,
         "Covering PassLegAB with AggrLegAC");
    }
    else
    {
      // Just close the AB pos aggressively (indicated by J==0):
      assert(a_tri->m_passInstr     == m_instrs[AB] &&
             a_tri->m_aggrInstrs[0] == m_instrs[AB]);

      NewOrderSafe<AB, BuyAB, 0>
        (a_tri, PriceT(), a_qty_ab, a_ts_exch, a_ts_conn, a_ts_strat,
         "Closing LegAB");
    }
  }

  //=========================================================================//
  // "CBCoveringOrders":                                                     //
  //=========================================================================//
  template<ExchC0T X>
  template<bool IsBidCB>
  void  TriArbC0<X>::CBCoveringOrders
  (
    TrInfo*         a_tri,
    PriceT          a_px_cb,    // Already normalised to Px(C/B)
    QtyN            a_qty_cb,
    utxx::time_val  a_ts_exch,
    utxx::time_val  a_ts_conn,
    utxx::time_val  a_ts_strat
  )
  {
    assert(IsPos(a_qty_cb) && a_tri != nullptr && a_tri->m_passI == CB &&
           bool(a_tri->m_passS) == IsBidCB);

    // Re-compute the Qtys of Aggr Orders (AB and AC) and estimate the TriAngle
    // PnL (in units of B):
    // AB and AC sides for AggrOrders:
    constexpr bool  BuyAB   = !IsBidCB;
    constexpr bool  BuyCB   = !IsBidCB; // For aggressive closing of CB pos
    constexpr bool  BuyAC   =  IsBidCB;
    // CB and AC sides for VWAP:
    constexpr bool  IsBidAB = !BuyAB;
    constexpr bool  IsBidAC = !BuyAC;

    assert(BuyAB == a_tri->m_aggrSs[AB] && BuyCB == a_tri->m_aggrSs[CB] &&
           BuyAC == a_tri->m_aggrSs[AC]);

    QtyN      qAC = SolveLiquidityEqn<IsBidAC>(m_orderBooks[AC], a_qty_cb);
    QtyN      qAB = qAC;
    PriceT vwapAB = m_orderBooks[AB]->GetVWAP1<QTO,QRO,IsBidAB,QT,QR,true>(qAB);

    // Projected PnL (in B) if we close the TriAngle, up to "a_qty_cb" factor;
    // we expect pnlB1 > 0:
    double  pnlB1 = double(qAB) * double(vwapAB) / double(a_qty_cb)
                  - double(a_px_cb);

    // Similar projected PnL (in B) if we simply close the AB pos aggressively
    // (so again SAME side: IsBidCB, thus theoretically, pnlB1 = 0):
    double  pnlB0 =
      double(m_orderBooks[CB]->GetVWAP1<QTO,QRO,IsBidCB,QT,QR,true>(a_qty_cb)) -
      double(a_px_cb);

    if constexpr(!IsBidCB)
    {
      pnlB1 = - pnlB1;
      pnlB0 = - pnlB0;
    }
    // Again, select the best way: Fire up Aggressive Legs or Close the CB pos:
    if (pnlB1 > pnlB0)
    {
      // Complete the TriAngle: J=1 --> AB, J=2 --> AC:
      assert(a_tri->m_aggrInstrs[1] == m_instrs[AB] &&
             a_tri->m_aggrInstrs[2] == m_instrs[AC]);

      // Use ContractPxs here (they will be made "more aggressive" by "NewOrder-
      // Safe":
      NewOrderSafe<AB, BuyAB, 1>
        (a_tri, PriceT(), qAB, a_ts_exch, a_ts_conn, a_ts_strat,
         "Covering PassLegCB with AggrLegAB");
      NewOrderSafe<AC, BuyAC, 2>
        (a_tri, PriceT(), qAC, a_ts_exch, a_ts_conn, a_ts_strat,
         "Covering PassLegCB with AggrLegAC");
    }
    else
    {
      // Just close the CB pos aggressively (indicated by J==0):
      assert(a_tri->m_passInstr     == m_instrs[CB] &&
             a_tri->m_aggrInstrs[0] == m_instrs[CB]);

      NewOrderSafe<CB, BuyCB, 0>
        (a_tri, PriceT(), a_qty_cb, a_ts_exch, a_ts_conn, a_ts_strat,
         "Closing LegCB");
    }
  }

  //=========================================================================//
  // "ACCoveringOrders":                                                     //
  //=========================================================================//
  template<ExchC0T X>
  template<bool IsBidAC>
  void  TriArbC0<X>::ACCoveringOrders
  (
    TrInfo*         a_tri,
    PriceT          a_px_ac,    // Already normalised to Px(A/C)
    QtyN            a_qty_ac,
    utxx::time_val  a_ts_exch,
    utxx::time_val  a_ts_conn,
    utxx::time_val  a_ts_strat
  )
  {
    assert(IsPos(a_px_ac) && a_tri != nullptr  && a_tri->m_passI == AC &&
           bool(a_tri->m_passS) == IsBidAC);

    // Re-compute the Qtys of Aggr Orders (AB and CB) and estimate the TriAngle
    // PnL (in units of B):
    // AB and CB sides for AggrOrders:
    constexpr bool  BuyAB   = !IsBidAC;
    constexpr bool  BuyCB   =  IsBidAC;
    constexpr bool  BuyAC   = !IsBidAC; // For aggressive closing of AC pos
    // AB and CB sides for VWAP:
    constexpr bool  IsBidAB = !BuyAB;
    constexpr bool  IsBidCB = !BuyCB;

    assert(BuyAB == a_tri->m_aggrSs[AB] && BuyCB == a_tri->m_aggrSs[CB] &&
           BuyAC == a_tri->m_aggrSs[AC]);

    QtyN      qAB = a_qty_ac;
    PriceT vwapAB = m_orderBooks[AB]->GetVWAP1<QTO,QRO,IsBidAB,QT,QR,true>(qAB);

    QtyN      qCB = QtyN(double(a_qty_ac) * double(a_px_ac));
    PriceT vwapCB = m_orderBooks[CB]->GetVWAP1<QTO,QRO,IsBidCB,QT,QR,true>(qCB);

    // Projected PnL (in B) if we close the TriAngle, up to "a_qty_ac" factor;
    // we expect pnlB1 > 0:
    double  pnlB1 = double(vwapAB) - double(a_px_ac) * double(vwapCB);

    // Similar projected PnL (in B) if we simply close the AB pos aggressively
    // (so again SAME side: IsBidCB, thus theoretically, pnlB1 = 0):
    double  pnlB0 =
      double(m_orderBooks[AC]->GetVWAP1<QTO,QRO,IsBidCB,QT,QR,true>(a_qty_ac)) -
      double(a_px_ac);

    if constexpr(!IsBidAC)
    {
      pnlB1 = - pnlB1;
      pnlB0 = - pnlB0;
    }

    // Again, select the best way: Fire up Aggressive Legs or Close the CB pos:
    if (pnlB1 > pnlB0)
    {
      // Complete the TriAngle: J=1 --> AB, J=2 --> CB:
      assert(a_tri->m_aggrInstrs[1] == m_instrs[AB] &&
             a_tri->m_aggrInstrs[2] == m_instrs[AC]);

      // Use ContractPxs here (they will be made "more aggressive" by "NewOrder-
      // Safe":
      NewOrderSafe<AB, BuyAB, 1>
        (a_tri, PriceT(), qAB, a_ts_exch, a_ts_conn, a_ts_strat,
         "Covering PassLegAC with AggrLegAB");
      NewOrderSafe<CB, BuyCB, 2>
        (a_tri, PriceT(), qCB, a_ts_exch, a_ts_conn, a_ts_strat,
         "Covering PassLegAC with AggrLegCB");
    }
    else
    {
      // Just close the AC pos aggressively (indicated by J==0):
      assert(a_tri->m_passInstr     == m_instrs[AC] &&
             a_tri->m_aggrInstrs[0] == m_instrs[AC]);

      NewOrderSafe<AC, BuyAC, 0>
        (a_tri, PriceT(), a_qty_ac, a_ts_exch, a_ts_conn, a_ts_strat,
         "Closing LegAC");
    }
  }

  //=========================================================================//
  // "NewOrderSafe": Exception-Safe Wrapper:                                 //
  //=========================================================================//
  // AggrJ: Passive: -1, AggrClose: 0; AggrTriCover: 1..2
  template<ExchC0T X>
  template<int I, bool IsBuy, int AggrJ>
  void TriArbC0<X>::NewOrderSafe
  (
    TrInfo*         a_tri,
    PriceT          a_px, // ContractPx, NOT Px(A/B)
    QtyN            a_qty,
    utxx::time_val  a_ts_exch,
    utxx::time_val  a_ts_conn,
    utxx::time_val  a_ts_strat,
    char const*     a_comment
  )
  {
    //-----------------------------------------------------------------------//
    // Checks:                                                               //
    //-----------------------------------------------------------------------//
    // NB: J==0 corresponds to aggressive closing of a position in PassInstr:
    static_assert(0 <= I && I <= 2 && -1 <= AggrJ && AggrJ <= 2);
    assert(a_tri != nullptr);

    // AggrFlag and Instr are inferrable:
    constexpr bool IsAggr = (AggrJ >= 0);

    assert(a_tri->m_aggrInstrs[0] == a_tri->m_passInstr &&
           a_tri->m_aggrIs    [0] == a_tri->m_passI);

    SecDefD const* instr  =
      (AggrJ < 0)
      ? a_tri->m_passInstr
      : a_tri->m_aggrInstrs[AggrJ];
    assert(instr != nullptr && instr == m_instrs[I]);

    // Furthermore, for a Passive Order, the following invariant must hold:
    // PassQty is never altered:
    assert(IsAggr || a_qty == a_tri->m_passQty);

    // For Aggr Orders, Px is not specified:
    assert(IsAggr == !IsFinite(a_px));

    DEBUG_ONLY
    (
      int i = IsAggr ? a_tri->m_aggrIs[AggrJ] : a_tri->m_passI;
      int s = IsAggr ? a_tri->m_aggrSs[AggrJ] : a_tri->m_passS;
    )
    assert(i==I && IsBuy == bool(s));

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
        LOG_ERROR(1, "NewOrder: RiskMgr has entered SafeMode, STOPPING...")
        DelayedGracefulStop(a_ts_exch, a_ts_conn, a_ts_strat);
      }
      return;
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
      return;

    //-----------------------------------------------------------------------//
    // Aggressive Orders:                                                    //
    //-----------------------------------------------------------------------//
    if constexpr (IsAggr)
    {
      // Compute the deepest aggressive price over the OrderBook. NB: Use the
      // OPPOSITE OrderBook side here (!IsBuy) -- we aggress against it:
      PriceT deepPxAB =
        m_orderBooks[I]->GetDeepestPx<QTO,QRO,!IsBuy,QT,QR,true>(a_qty);

      // Compare it with a possibly-existing quote at that side. If there is a
      // possibility of a collision, cancel the quote first.  XXX: This method
      // DOES NOT FULLY GUARANTEE that there will be no collision, because the
      // actual aggressive fill px is unknown a priori,  and we do not want to
      // be too conservative and cancel passive quotes in all cases:
      //
      TrInfo* passTrI = m_curr3s[I][int(!IsBuy)];

      if (utxx::likely(passTrI != nullptr && passTrI->m_passAOS != nullptr))
      {
        // XXX: Is it possible at al that    passTrI->m_passAOS == nullptr ?
        // Check whether a collision is likely: XXX: Again, the inequalities
        // correspond to aggressive pxs:
        bool coll =
          ( IsBuy && deepPxAB >= passTrI->m_passPx) ||
          (!IsBuy && deepPxAB <= passTrI->m_passPx);

        if (coll)
          // Collision is likely: Cancel the Passive Quote first:
          (void) CancelOrderSafe
            (passTrI->m_passAOS, a_ts_exch, a_ts_conn, a_ts_strat,
             "Potential Collision");
      }
      // Calculate the AggressivePx with 2.5% excess (XXX: This is hopefuly a
      // reasonably-balanced value). NB: This is a Contract (Instrument) Px!
      //
      a_px = instr->GetContractPx(deepPxAB) * (IsBuy ? 1.025 : 0.975);
    }
    //-----------------------------------------------------------------------//
    // Actually issue a new Order (Passive or Aggressive):                   //
    //-----------------------------------------------------------------------//
    // Capture and log possible "NewOrder" exceptions, then terminate the
    // Strategy. NB: BatchSend=false:
    AOS const* aos  = nullptr;
    assert  (m_omc != nullptr);
    try
    {
      aos =
        utxx::likely(!m_dryRun)
        ? m_omc->NewOrder
          (
            this,        *instr,     FIX::OrderTypeT::Limit,
            IsBuy,       a_px,       IsAggr,          QtyAny(a_qty),
            a_ts_exch,   a_ts_conn,  a_ts_strat,      false,
            FIX::TimeInForceT::Day,  0
          )
        : nullptr;
    }
    catch (EPollReactor::ExitRun const&)
    {
      // This exception is always propagated:
      throw;
    }
    catch (std::exception const& exc)
    {
      LOG_ERROR(1,
        "NewOrderSafe: {} Order FAILED: Symbol={}, {}, Px={}, Qty={}: {}"
        ", STOPPING...",
        IsAggr ? "Aggressive" : "Passive",    instr->m_Symbol.data(),
        IsBuy  ? "Bid" : "Ask", double(a_px), QR(a_qty),  exc.what())
      m_logger->flush();

      // NewOrder failed -- initiate stopping:
      DelayedGracefulStop(a_ts_exch, a_ts_conn, a_ts_strat);
      return;
    }
    //-----------------------------------------------------------------------//
    // Memoise the AOS in "TrInfo", and other way round:                     //
    //-----------------------------------------------------------------------//
    // If we got here, NewOrder was successful (but beware of DryRun!):
    assert(m_dryRun || aos != nullptr);

    // Where this AOS is to be installed?
    AOS const*& old =
      (AggrJ  < 0)
      ? a_tri->m_passAOS
      : a_tri->m_aggrLastAOSes[AggrJ];

    // NB: The PassiveAOS is installed only once in a given "TrInfo":
    assert(IsAggr || old == nullptr);

    // First do the inverse (AOS -> AOSExt -> TrInfo):
    AOSExt& ext       = aos->m_userData.Get<AOSExt>();
    ext.m_thisAOS     = aos;
    ext.m_trInfo      = a_tri;
    ext.m_aggrJ       = AggrJ;
    ext.m_prevAggrAOS = old;
    ext.CheckInvariants();  // Debug only

    // Now the fwd map (TrInfo -> AOS):
    old = aos;

    // Finally, Log it:
    LOG_INFO(2,
      "NewOrderSafe: {}, Symbol={}, {}, Px={}, Qty={}: {}",
      IsAggr ? "Aggressive" : "Passive",    instr->m_Symbol.data(),
      IsBuy  ? "Bid" : "Ask", double(a_px), QR(a_qty), a_comment)
  }

  //=========================================================================//
  // "ModifyOrderSafe": Exception-Safe Wrapper:                              //
  //=========================================================================//
  template<ExchC0T X>
  inline bool TriArbC0<X>::ModifyOrderSafe
  (
    AOS const*      a_aos,      // Non-NULL
    PriceT          a_px,       // ContractPx, NOT Px(A/B)
    QtyN            a_qty,
    utxx::time_val  a_ts_exch,
    utxx::time_val  a_ts_conn,
    utxx::time_val  a_ts_strat
  )
  {
    // Since this method is invoked, AOS does exist, so it must NOT be a DryRun
    // mode:
    assert(!m_dryRun && a_aos != nullptr && IsFinite(a_px) && IsPos(a_qty) &&
           !(a_aos->m_isInactive)        && a_aos->m_isCxlPending == 0);

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
        LOG_ERROR(1, "ModifyOrder: RiskMgr has entered SafeMode, STOPPING...")
        DelayedGracefulStop(a_ts_exch, a_ts_conn, a_ts_strat);
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
        m_omc->ModifyOrder
        (
          a_aos,  a_px, false,     QtyAny(a_qty),
          a_ts_exch,    a_ts_conn, a_ts_strat,   false
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
        LOG_WARN(2,
          "ModifyOrderSafe: Returned False: Symbol={}, {}: Px={}, "
          "Qty={}, AOSID={}: Cancelling this Order...",
          instr.m_Symbol.data(), a_aos->m_isBuy ? "Bid" : "Ask",
          double(a_px),          QR(a_qty),       a_aos->m_id)

        (void) CancelOrderSafe
          (a_aos, a_ts_exch, a_ts_conn, a_ts_strat, "Could Not Modify");

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
      // ModifyOrder failed -- log the error and initiate stopping:
      LOG_ERROR(1,
        "ModifyOrderSafe: FAILED: Symbol={}, {}: Px={}, Qty={}, "
        "AOSID={}: {}, STOPPING...",
        instr.m_Symbol.data(), a_aos->m_isBuy ? "Bid" : "Ask",  double(a_px),
        QR(a_qty),             a_aos->m_id,     exc.what())
      m_logger->flush();

      DelayedGracefulStop(a_ts_exch, a_ts_conn, a_ts_strat);
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
  template<ExchC0T X>
  inline bool TriArbC0<X>::CancelOrderSafe
  (
    AOS const*      a_aos,        // Non-NULL unless DryRun
    utxx::time_val  a_ts_exch,
    utxx::time_val  a_ts_conn,
    utxx::time_val  a_ts_strat,
    char const*     a_comment
  )
  {
    // Since this method is invoked on a real AOS, we cannot be in DryRun mode:
    assert(!m_dryRun && a_aos != nullptr && m_omc != nullptr &&
           a_comment != nullptr);

    SecDefD const& instr = *(a_aos->m_instr);

    if (utxx::unlikely(a_aos->m_isInactive || a_aos->m_isCxlPending != 0))
      return true;  // Nothing to do

    // NB: Cancellations are always allowed; BatchSend=false (normally not appl-
    // icable to Crypto-OMCs anyway):
    try
    {
      DEBUG_ONLY(bool ok =)
        m_omc->CancelOrder(a_aos, a_ts_exch, a_ts_conn, a_ts_strat, false);

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
      LOG_ERROR(1,
        "CancelOrderSafe: CancelOrder FAILED: Symbol={}, {}: AOSID={}: {}"
        ", STOPPING...", instr.m_Symbol.data(), a_aos->m_isBuy ? "Bid" : "Ask",
        a_aos->m_id, exc.what())
      m_logger->flush();

      // CancelOrder failed -- initiate stopping:
      DelayedGracefulStop(a_ts_exch, a_ts_conn, a_ts_strat);
      return false;
    }
    // NB: The AOS ptr is NOT reset to NULL yet --  will only do that when
    // the cancellation is confirmed (before that, "PendingCancel" flag is
    // set).
    // Log the Cancel:
    LOG_INFO(2,
      "CancelOrderSafe: CANCELLING: Symbol={}, {}, AOSID={}: {}",
      instr.m_Symbol.data(), a_aos->m_isBuy ? "Bid" : "Ask", a_aos->m_id,
      a_comment)

    return true;
  }

  //=========================================================================//
  // "OrderCanceledOrFailed":                                                //
  //=========================================================================//
  template<ExchC0T X>
  void TriArbC0<X>::OrderCanceledOrFailed
  (
    AOS const&      a_aos,
    char const*     a_msg,        // NULL iff Canceled
    utxx::time_val  a_ts_exch,
    utxx::time_val  a_ts_conn,
    utxx::time_val  a_ts_strat
  )
  {
    // This method cannot be invoked in the DryRun mode. And if invoked, the
    // Order must really be Inactive:
    assert(!m_dryRun && a_aos.m_isInactive);

    // Is it a Passive Order which was Cancelled (or has Failed)? -- This is the
    // most likely case, because  Aggressive  orders cannot  normally be cancel-
    // led. If there is no explicit "a_msg", then it's a Cancel:
    //
    bool       isCancel = (a_msg == nullptr);
    AOSExt const&   ext = a_aos.m_userData.Get<AOSExt>();
    bool       isAggr   = (ext.m_aggrJ >= 0);
    TrInfo*         tri = ext.m_trInfo;
    assert(tri != nullptr);
    char const*   which = nullptr;

    if (utxx::unlikely(isAggr))
    {
      // An AggrOrder failure is a VERY STRANGE event. It cannot be a Cancel:
      assert(!isCancel);
      which = "AGGRESSIVE";
    }
    else
    {
      // It was a Passive Order -- find and remove its "TrInfo" from "m_curr3s":
      which = "PASSIVE";
      int I = tri->m_passI;
      int S = tri->m_passS;
      assert(0 <= I && I <= 2 && 0 <= S && S <= 1 && bool(S) == a_aos.m_isBuy);

      if (m_curr3s[I][S] == tri)
        m_curr3s[I][S] = nullptr;
    }

    // Log the Cancellation/Failure (and if logged, flush the logger):
    LOG_INFO(2,
      "{} ORDER {}: AOSID={}, Symbol={}, {}{}{}{}",
      which,    isCancel ? "CANCELLED" : "FAILED",     a_aos.m_id,
      a_aos.m_instr->m_Symbol.data(),  a_aos.m_isBuy ? "Bid" : "Ask",
     (a_msg != nullptr)  ? a_msg              : "",
      a_aos.m_isInactive ? ": Order Inactive" : "",
     isAggr              ? ", STOPPING..."    : "")

    // Since an AggrOrder failure may result in an unbalanced position, initiate
    // a ShutDown in that case:
    if (utxx::unlikely(isAggr))
      DelayedGracefulStop(a_ts_exch, a_ts_conn, a_ts_strat);

    m_logger->flush();
  }
} // End namespace MAQUETTE
