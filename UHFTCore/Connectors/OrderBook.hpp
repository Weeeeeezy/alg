// vim:ts=2:et
//===========================================================================//
//                         "Connectors/OrderBook.hpp":                       //
//  Internal "Unlimited-Depth" Order Book Representation with Fast Updates:  //
//                             Templated Methods                             //
//===========================================================================//
#pragma once
#include "Basis/BaseTypes.hpp"
#include "Basis/Macros.h"
#include "Basis/SecDefs.h"
#include "Basis/QtyConvs.hpp"
#include "Connectors/OrderBook.h"
#include "Connectors/EConnector_MktData.h"
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cassert>

namespace MAQUETTE
{
  //=========================================================================//
  // "UpdatedSidesT" vals are ORable:                                        //
  //=========================================================================//
  inline   OrderBook::UpdatedSidesT operator|
          (OrderBook::UpdatedSidesT a_left,  OrderBook::UpdatedSidesT a_right)
  { return OrderBook::UpdatedSidesT  (int(a_left) | int(a_right)); }


  inline   OrderBook::UpdatedSidesT& operator|=
          (OrderBook::UpdatedSidesT& a_left, OrderBook::UpdatedSidesT a_right)
  {
    a_left = OrderBook::UpdatedSidesT(int(a_left) | int(a_right));
    return   a_left;
  }

  //=========================================================================//
  // "GetSideMap": Specialisations:                                          //
  //=========================================================================//
  template<>
  inline OrderBook::BidsMap const& OrderBook::GetSideMap<true> () const
    { return m_bidsMap; }

  template<>
  inline OrderBook::AsksMap const& OrderBook::GetSideMap<false>() const
    { return m_asksMap; }

  //=========================================================================//
  // "Update":                                                               //
  //=========================================================================//
  template
  <
    bool     IsBid,
    bool     InitMode,
    bool     WithOrdersLog,
    bool     SnapShotsOnly,
    bool     IsRelaxed,
    bool     IsFullAmt,
    bool     IsSparse,
    QtyTypeT QT,
    typename QR
  >
  inline OrderBook::UpdateEffectT OrderBook::Update
  (
    FIX::MDUpdateActionT a_action,    // Use UNDEFINED if not known
    PriceT               a_px,
    Qty<QT,QR>           a_qty,       // WithOrdersLog: Delta; other: TargQty
    SeqNum               a_rpt_seq,
    SeqNum               a_seq_num,
    OrderInfo*           a_order      // Order affected (iff WithOrdersLog)
  )
  {
    //-----------------------------------------------------------------------//
    // Verify the Args and the OrderBook:                                    //
    //-----------------------------------------------------------------------//
    assert((IsValidQtyRep<QT,QR>(m_qt, m_withFracQtys)));
    assert(IsFinite(a_px));
    assert(IsFullAmt == m_isFullAmt && IsSparse == m_isSparse);

    // If SnapShotsOnly is set, we do not gave InitMode or Orders:
    static_assert(!SnapShotsOnly || (!InitMode && !WithOrdersLog),
                  "SnapShotsOnly is incompatible with InitMode or "
                  "WithOrdersLog");

    // "a_order" must be set iff "WithOrdersLog" is set:
    CHECK_ONLY
    (
      if (utxx::unlikely((a_order != nullptr) != WithOrdersLog))
        throw utxx::badarg_error
              ("OrderBook::Update: Inconsistent WithOrdersLog mode");
    )
    // Check SeqNums and update them (no matter whether this OrderBook update
    // succeeds or not: the book is touched anyway):
    m_lastUpdatedBid = IsBid;
    CheckSeqNums<InitMode>(a_rpt_seq, a_seq_num, "Update");

    if constexpr (WithOrdersLog)
    {
      // If QtyDelta is 0 (it may sometimes happen), there is nothing more to
      // do. In particular, "NOrders" at the corresp px level is NOT affected
      // in that case:
      if (utxx::unlikely(IsZero(a_qty)))
        return UpdateEffectT::NONE;

      // Obviosuly, if "UpdateAction" is "New", "QtyDelta" can only be > 0; if
      // it is "Delete", then < 0; but the converse is not true (because "Upd-
      // ateAction" can also be "Change" or "UNDEFINED"):
      CHECK_ONLY
      (
        if (utxx::unlikely
           (IsNeg(a_qty) && a_action == FIX::MDUpdateActionT::New)  ||
           (IsPos(a_qty) && a_action == FIX::MDUpdateActionT::Delete))
          throw utxx::badarg_error
                ("OrderBook::Update(WithOrdersLog): Inconsistent args: Update"
                 "Action=", char(a_action), ", QtyDelta=", QR(a_qty));
      )
    }
    else
    {
      // Similar check for Aggregated OrderBooks:
      CHECK_ONLY
      (
        if (utxx::unlikely
           (IsNeg (a_qty) ||
           (IsZero(a_qty) != (a_action == FIX::MDUpdateActionT::Delete))))
          LOG_ERROR(1, "OrderBook::Update(Aggregated): Inconsistent args: "
            "UpdateAction=", char(a_action), ", QtyDelta=", QR(a_qty));
          return UpdateEffectT::NONE;
      )
    }
    // In addition, the Book *should* be consistent -- if it is not, we *still*
    // allow update in the hope that it will be corrected later, possibly after
    // after multiple updates are carried out...
    //-----------------------------------------------------------------------//
    // The reset depends on whether the OrderBooks is Dense or Sparse:       //
    //-----------------------------------------------------------------------//
    return
      (!IsSparse)
      ? UpdateDenseSide
        <IsBid, WithOrdersLog, SnapShotsOnly, IsRelaxed, IsFullAmt, QT, QR>
        (a_action, a_px, a_qty, a_order)

      : UpdateSparseSide
        <IsBid, WithOrdersLog, SnapShotsOnly, IsRelaxed, IsFullAmt, QT, QR>
        (a_action, a_px, a_qty, a_order);
  }

  //=========================================================================//
  // "UpdateDenseSide":                                                      //
  //=========================================================================//
  template
  <
    bool     IsBid,
    bool     WithOrdersLog,
    bool     SnapShotsOnly,
    bool     IsRelaxed,
    bool     IsFullAmt,
    QtyTypeT QT,
    typename QR
  >
  inline OrderBook::UpdateEffectT OrderBook::UpdateDenseSide
  (
    FIX::MDUpdateActionT a_action,    // Use UNDEFINED if not known
    PriceT               a_px,
    Qty<QT,QR>           a_qty,       // WithOrdersLog: Delta; other: TargQty
    OrderInfo*           a_order      // Order affected (iff WithOrdersLog)
  )
  {
    //----------------------------------------------------------------------//
    // Dense-Specific Checks:                                               //
    //----------------------------------------------------------------------//
    // (Generic checks are already performed by the CallER ("Update"))
    // Only invoked in Dense mode:
    assert(!m_isSparse);

    // Integrity conditions for Bids:
    assert((m_LBI == -1 && m_BBI == -1    && !IsFinite(m_BBPx)) ||
           (0 <= m_LBI  && m_LBI <= m_BBI &&  m_BBI < m_NL       &&
            IsFinite(m_BBPx)));

    // Integrity conditions for Asks:
    assert((m_BAI == -1 && m_HAI == -1    && !IsFinite(m_BAPx)) ||
           (0 <= m_BAI  && m_BAI <= m_HAI &&  m_HAI < m_NL       &&
            IsFinite(m_BAPx)));

    CHECK_ONLY
    (
      if (utxx::unlikely
         ((WithOrdersLog && m_ND != 0)) || (!WithOrdersLog && m_ND < 0))
        throw utxx::badarg_error
              ("OrderBook::UpdateDenseSide: Invalid MktDepth: ", m_ND);
    )
    //-----------------------------------------------------------------------//
    // Proceed with the Update:                                              //
    //-----------------------------------------------------------------------//
    UpdateEffectT res = UpdateEffectT::L2;  // Made more precise below

    // Refs to the side to be updated:
    Qty<QT,QR>  newQty;             // New  qty in slot "s" (TBD; initially 0)
    Qty<QT,QR>  prevQty;            // Prev qty in slot "s" (TBD; initially 0)
    int         s         = -1;     // Slot to be Updated   (TBD)
    int    &    bestIdx   = IsBid ? m_BBI  : m_BAI;
    int    &    wrstIdx   = IsBid ? m_LBI  : m_HAI;
    PriceT &    bestPx    = IsBid ? m_BBPx : m_BAPx;
    OBEntry*    side      = IsBid ? m_bids : m_asks;
    assert(side != nullptr);

    //-----------------------------------------------------------------------//
    // This Side is Empty yet?                                               //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely(bestIdx == -1))
    {
      // There was nothing there yet:
      assert(!IsFinite(bestPx) && wrstIdx == -1);

      newQty = VerifyQty<IsBid,QT,QR>(a_qty, a_px);
      assert(!IsNeg(newQty));
      // "prevQty" was initialised to 0, that's just OK

      // Normally, "newQty" should not be 0, of course:
      if (utxx::likely(IsPos(newQty)))
      {
        // Check that "a_px" is a multiple of "PxStep":
        (void) GetPxStepMultiple<IsRelaxed>(double(a_px));

        // If OK: Place the initial "a_px" in the middle of the "side"; it is
        // currently the BestPx (others are not available):
        // NB: "bestIdx" is created WITHOUT ANY RELATION to the other side of
        // the Book -- even if the latter is non-empty, we do NOT use its pxs
        // when determining "bestIdx". As a result, there is NO RELATION betw-
        // een "m_BBI" and "m_BAI"!
        bestIdx = m_NL / 2;
        wrstIdx = bestIdx;
        bestPx  = a_px;
        s       = bestIdx;
        res     = UpdateEffectT::L1Px;  // Since the Book was empty!
        // NB: Do not set "currDepth" yet -- it will be done later when all in-
        // dices are updated!
      }
      else
      {
        // There is absolutely nothing to do -- deleting a level in an empty
        // book! XXX: This only produces a warning, not an error:
        assert(a_action == FIX::MDUpdateActionT::Delete);
        LOG_WARN(2,
          "OrderBook::Update: {}, Side={}: Delete on an empty side?",
          m_instr->m_FullName.data(),      IsBid ? "Bid" : "Ask")
        return UpdateEffectT::NONE;
      }
    }
    else
    //-----------------------------------------------------------------------//
    // Generic Case: The Side was NOT Empty:                                 //
    //-----------------------------------------------------------------------//
    {
      assert(( IsBid && 0 <= wrstIdx && wrstIdx <= bestIdx && bestIdx < m_NL
            && IsFinite(bestPx))                                         ||
             (!IsBid && 0 <= bestIdx && bestIdx <= wrstIdx && wrstIdx < m_NL
            && IsFinite(bestPx)));
      //---------------------------------------------------------------------//
      // Find the slot "s" to be updated:                                    //
      //---------------------------------------------------------------------//
      // The BestPx and {Best,Wrst}Idx must now be valid:

      // Compute the offset (in PxSteps) of "a_px" relative to "bestIdx", and
      // the slot to be updated:
      s = bestIdx + GetPxStepMultiple<IsRelaxed>(a_px - bestPx);

      if (utxx::unlikely(s < 0 || s >= m_NL))
      {
        // Beyond all Px Levels -- no update. This is most likely an error. Ho-
        // wever, this MAY happen in some test environments which artificially
        // enlarge px ranges. But in a SnapShotsOnly mode, it is OK  -- simply
        // skip this entry, w/o even a warning:
        if (SnapShotsOnly)
          return UpdateEffectT::NONE;
        else
        {
          LOG_WARN(2,
            "OrderBook::Update(WithIncrsUpdates): {}, Side={}, Px={}, BestPx="
            "{}, Slot={}, BestSlot={}: Out of Range",
            m_instr->m_FullName.data(), IsBid ? "Bid" : "Ask", double(a_px),
            double(bestPx), s, bestIdx)

          // Still, in the Relaxed mode, return NONE, not an ERROR:
          return IsRelaxed ? UpdateEffectT::NONE : UpdateEffectT::ERROR;
        }
      }
      // If OK: Adjust the Qty at that level; PrevQty does not matter for Agg-
      // regated OrderBooks (set to 0):
      prevQty =
        WithOrdersLog
        ? side[s].GetAggrQty<QT,QR>()
        : Qty<QT,QR>();
      assert(!IsNeg(prevQty));

      newQty  = VerifyQty<IsBid,QT,QR>(prevQty + a_qty, a_px);
      assert(!IsNeg(newQty));

      //---------------------------------------------------------------------//
      // Better than L1?                                                     //
      //---------------------------------------------------------------------//
      // This includes the case when the side was empty, so the curr level was
      // just established:
      if (utxx::unlikely((IsBid && s > bestIdx) || (!IsBid && s < bestIdx)))
      {
        // This normally means that BestPx has been improved: New level crea-
        // ted. Thus, "prevQty" should be 0 in any case, and "newQty" should
        // be > 0; the former is our logical invariant whereas the latter de-
        // pends on the input, so:
        assert(IsZero(prevQty) && 0 <= s && s < m_NL);

        if (utxx::unlikely(IsZero(newQty)))
        {
          // Do NOT update "bestIdx" and "bestPx" in this case, and do NOT
          // carry out any updates at all: This is a Delete beyond L1. Still,
          // this only produces a warning:
          LOG_WARN(2,
            "OrderBook::Update: {}, Side={}: BestIdx={}, NewBestIdx={} but "
            "NewQty=0",
            m_instr->m_FullName.data(), IsBid ? "Bid" : "Ask", bestIdx, s)
          return UpdateEffectT::NONE;
        }

        // OK: This will be new BestPx, so L1Px has been changed:
        assert(IsPos(newQty));
        bestIdx = s;
        bestPx  = a_px;
        res     = UpdateEffectT::L1Px;
      }
      else
      //---------------------------------------------------------------------//
      // At L1?                                                              //
      //---------------------------------------------------------------------//
      if (utxx::likely(s == bestIdx))
      {
        // Then the "prevQty" at this level must NOT be 0 (in WithOrdersLog
        // mode; otherwise it is always 0):
        //
        assert(!WithOrdersLog || IsPos(prevQty));
        double pxStep = m_instr->m_PxStep;

        if (IsZero(newQty))
        {
          // The current BestPx level has been removed completely. This should
          // NOT happen in the Init Mode, but  the latter case  would not be a
          // grave error: we would then just remove a newly-installed BestPx):
          // Find the new Best level. In case if a new one does not exist,  ie
          // the "side" is now completely empty, the following will apply:
          //
          res        = UpdateEffectT::L1Px;  // The orig L1 level was removed
          bool found = false;

          if constexpr (IsBid)
            // Find the next BestBid: Traverse the "side" down-wards:
            for (int i = s-1; i >= 0; --i)
            {
              assert(!IsNeg(side[i].m_aggrQty));

              if (IsPos(side[i].m_aggrQty))
              {
                bestIdx  = i;
                bestPx  += double(i-s) * pxStep;          // Neg incr
                found    = true;
                break;
              }
            }
          else  // !IsBid
            // Find the next BestAsk: Traverse the "side" up-wards:
            for (int i = s+1; i < m_NL; ++i)
            {
              assert(!IsNeg(side[i].m_aggrQty));

              if (IsPos(side[i].m_aggrQty))
              {
                bestIdx  = i;
                bestPx  += double(i-s) * pxStep;          // Pos incr
                found    = true;
                break;
              }
            }
          // So:
          if (utxx::unlikely(!found))
          {
            // The "side" is completely empty again:
            bestIdx = -1;
            wrstIdx = -1;
            bestPx  = PriceT(); // NaN
          }
          else
            // There must still be an invariant relation between Best and Wrst
            // Indices:
            assert(( IsBid && bestIdx >= wrstIdx) ||
                   (!IsBid && bestIdx <= wrstIdx));
        }
        else
          // The Qty at L1 was reduced, but not removed altogether; Indices and
          // the CurrDepth are not affected:
          res = UpdateEffectT::L1Qty;
      }
      else
      {
        //-------------------------------------------------------------------//
        // Generic L2 update:                                                //
        //-------------------------------------------------------------------//
        assert(res == UpdateEffectT::L2 && bestIdx != -1 && wrstIdx != -1 &&
               IsFinite(bestPx));

        // Best Idx and Px are not affected in that case, but WrstIdx can be:
        if (utxx::unlikely((IsBid && s < wrstIdx) || (!IsBid && s > wrstIdx)))
        {
          wrstIdx = s;
          assert((IsBid && bestIdx >= wrstIdx)   ||
                (!IsBid && bestIdx <= wrstIdx));
        }
      }
    }
    //-----------------------------------------------------------------------//
    // Apply the Qty Update at "s":                                          //
    //-----------------------------------------------------------------------//
    assert(0 <= s && s < m_NL);
    OBEntry& obe  = side[s];  // Ref!

    // NB: IsSparse=false:
    UpdateOBE<IsBid, WithOrdersLog, IsRelaxed, false, QT, QR>
      (a_action, a_px, newQty, a_order, &obe, &res);

    //-----------------------------------------------------------------------//
    // Manage the Depth (only now, once all Qtys and Indices are updated):   //
    //-----------------------------------------------------------------------//
    if (IsZero(newQty))
    {
      assert(a_action == FIX::MDUpdateActionT::Delete);
      DecrementDepth<IsBid>();
    }
    else
    // NB: The condition when the Depth increases depends on WithOrdersLog (be-
    // cause of that mode is not in use, "prevQty" is always 0, so must use "a_
    // action" instead; on the other hand, if  WithOrders,  "a_action"  is  not
    // sufficient -- if a single Order was added, it does not imply yet a whole
    // new px level):
    if ((!WithOrdersLog && a_action == FIX::MDUpdateActionT::New)  ||
        ( WithOrdersLog && IsZero(prevQty)))
      IncrementDepth<IsBid, WithOrdersLog, QT, QR>();

    //-----------------------------------------------------------------------//
    // Final Checks:                                                         //
    //-----------------------------------------------------------------------//
    // The following invariant must always hold: Either the "side" is empty,
    // or it has valid BestPx, BestQty and BestNOrders:
    assert((bestIdx == -1 && !IsFinite(bestPx) && wrstIdx == -1)   ||
           (0 <= bestIdx  && bestIdx < m_NL    && IsFinite(bestPx) &&
            IsPos(side[bestIdx].m_aggrQty)     &&
            (( IsBid && bestIdx >= wrstIdx)                        ||
             (!IsBid && bestIdx <= wrstIdx))));
    assert(!WithOrdersLog || bestIdx == -1 || side[bestIdx].m_nOrders > 0);

    // All Done:
    return res;
  }

  //=========================================================================//
  // "UpdateOBE":                                                            //
  //=========================================================================//
# if defined(__GNUC__) && !defined(__clang__)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wunused-but-set-parameter"
# endif
  template
  <
    bool     IsBid,
    bool     WithOrdersLog,
    bool     IsRelaxed,
    bool     IsSparse,
    QtyTypeT QT,
    typename QR
  >
  inline void OrderBook::UpdateOBE
  (
    FIX::MDUpdateActionT a_action,
    PriceT               CHECK_ONLY(a_px),
    Qty<QT,QR>           a_new_qty,
    OrderInfo*           a_order,
    OBEntry*             a_obe,
    UpdateEffectT*       CHECK_ONLY(a_res)   // May be modified
  )
  {
    assert(WithOrdersLog == (a_order != nullptr) && a_obe != nullptr &&
           a_res != nullptr);

    //-----------------------------------------------------------------------//
    // Update the OrderInfo-related data (only if Orders are supported):     //
    //-----------------------------------------------------------------------//
    if constexpr (WithOrdersLog)
    {
      // Perform a "shallow" check (DeepCheck=false) of pre-conditions:
      assert
        ((CheckOBEntry<false,IsRelaxed,IsSparse,QT,QR>(a_obe, IsBid, a_px)));

      switch (a_action)
      {
      //---------------------------------------------------------------------//
      case FIX::MDUpdateActionT::New:
      //---------------------------------------------------------------------//
        // A new order is added to level "s", place it at the end of the curr
        // chain:
        if (a_obe->m_lastOrder == nullptr)
        {
          // Empty px level (as yet):
          assert(a_obe->m_nOrders == 0 && a_obe->m_frstOrder == nullptr);
          a_obe->m_frstOrder = a_order;
          a_order->m_prev    = nullptr;
        }
        else
        {
          // Non-empty px level: new "OrderInfo" is installed at the end:
          assert(a_obe->m_nOrders > 0 && a_obe->m_frstOrder != nullptr &&
                 a_obe->m_lastOrder->m_next == nullptr                 &&
                 a_obe->m_frstOrder->m_prev == nullptr);

          a_obe->m_lastOrder->m_next = a_order;
          a_order->m_prev = a_obe->m_lastOrder;
        }
        // In any case:
        a_obe  ->m_lastOrder = a_order;
        a_order->m_next      = nullptr;
        a_order->m_obe       = a_obe;
        ++(a_obe->m_nOrders);
        break;

      //---------------------------------------------------------------------//
      case FIX::MDUpdateActionT::Delete:
      //---------------------------------------------------------------------//
        // IMPORTANT: It may theoretically happen that the Exchange has sent us
        // a "Delete" msg for an order which does not relly exist nor belong to
        // this px level; so to be on a safe side, check the invariants:
        //
        CHECK_ONLY
        (
        if (utxx::unlikely
           (a_order->m_obe     != a_obe   || a_obe->m_nOrders   <= 0      ||
            a_obe->m_frstOrder == nullptr || a_obe->m_lastOrder == nullptr))
        {
          LOG_ERROR(2,
            "OrderBook::UpdateOBE(Delete): OrderInfo / OBEntry Inconsistency")
          *a_res = UpdateEffectT::ERROR;
        }
        else
        )
        { // Generic case: Proceed with Deletion:
          assert
            ((a_order == a_obe->m_frstOrder) == (a_order->m_prev == nullptr) &&
             (a_order == a_obe->m_lastOrder) == (a_order->m_next == nullptr));

          // By-pass "a_order" in the Fwd chain:
          if (a_order->m_prev != nullptr)
            a_order->m_prev->m_next = a_order->m_next;
          else
            a_obe->m_frstOrder      = a_order->m_next;

          // By-pass "a_order" in the Bwd chain:
          if (a_order->m_next != nullptr)
            a_order->m_next->m_prev = a_order->m_prev;
          else
            a_obe->m_lastOrder      = a_order->m_prev;

          // Reset "a_order" ptrs and decrement "NOrders":
          a_order->m_next = nullptr;
          a_order->m_prev = nullptr;
          --(a_obe->m_nOrders);
        }
        assert
          ((a_obe->m_lastOrder == nullptr         &&
            a_obe->m_frstOrder == nullptr         && a_obe->m_nOrders == 0) ||
           (a_obe->m_lastOrder->m_next == nullptr &&
            a_obe->m_frstOrder->m_prev == nullptr && a_obe->m_nOrders >  0));
        break;

      //---------------------------------------------------------------------//
      case FIX::MDUpdateActionT::Change:
      //---------------------------------------------------------------------//
        // NOrders and the OrderInfo ptrs are unchanged:
        break;

      default:
        // OrderAction is not maintained: Always put NOrders to 1 (unless the
        // Qty has become 0):
        a_obe->m_nOrders = utxx::unlikely(IsZero(a_obe->m_aggrQty)) ? 0 : 1;
      }
    }
    //-----------------------------------------------------------------------//
    // Now Update the Aggregated Qty (AFTER updating Orders):                //
    //-----------------------------------------------------------------------//
    a_obe->m_aggrQty = QtyU(a_new_qty);
    assert(!IsNeg(a_obe->m_aggrQty));

    if constexpr (WithOrdersLog)
    {
      CHECK_ONLY
      (
        // Obviously, AggrQty and NOrders must simultaneously be 0 or positive:
        assert(!IsNeg(a_obe->m_aggrQty) && a_obe->m_nOrders >= 0);

        bool isZeroQty    = IsZero(a_obe->m_aggrQty);
        bool isZeroOrders = (a_obe->m_nOrders == 0);

        if (utxx::unlikely(isZeroQty != isZeroOrders))
        {
          LOG_ERROR(2,
            "OrderBook::UpdateOBE: {}, Side={}, Px={} Got Qty={} but #Orders="
            "{}",  m_instr->m_FullName.data(),  IsBid ? "Bid" : "Ask",
            double(a_px),   QR(a_obe->GetAggrQty<QT,QR>()),  a_obe->m_nOrders)

          // XXX: So what to do in case of this inconsistency?
          if (isZeroQty)
          {
            // If Qty is 0 but NOrders is non-0,  and  there are indeed Orders
            // there, we can either remove all Orders or re-calculate the Qty;
            // it is just easier to do the former; this may also prevent  Bid-
            // Ask collisions in the futue; and  removing  doubtful  liquidity
            // from the OrderBook would not harm much, anyway:
            ResetOrders(a_obe->m_frstOrder);
            *a_obe = OBEntry();
          }
          else
            // Other way round: Qty != 0 but NOrders = 0. In that case, we will
            // have to reset the Qty, as there is no way of re-constructing the
            // Orders correctly:
            a_obe->m_aggrQty = QtyU();
        }
      )
      //----------------------------------------------------------------------//
      // Perform a "deep" check (DeepCheck=true) of post-conds:               //
      //----------------------------------------------------------------------//
      // But only do it in DebugMode!
      assert
        ((CheckOBEntry<true,IsRelaxed,IsSparse,QT,QR>(a_obe, IsBid, a_px)));
    }
  }
# if defined(__GCC__) && !defined(__clang__)
# pragma GCC diagnostic pop
# endif

  //=========================================================================//
  // "IncrementDepth":                                                       //
  //=========================================================================//
  template<bool IsBid, bool WithOrdersLog, QtyTypeT QT, typename QR>
  inline void OrderBook::IncrementDepth()
  {
    // Only invoked in Dense mode:
    assert(!m_isSparse);
    assert((IsValidQtyRep<QT,QR>(m_qt, m_withFracQtys)));

    // Get the Side and its Indices:
    int&     bestIdx   = IsBid ? m_BBI  : m_BAI;
    int&     wrstIdx   = IsBid ? m_LBI  : m_HAI;
    int&     currDepth = IsBid ? m_BD   : m_AD;
    OBEntry* side      = IsBid ? m_bids : m_asks;
    assert  (side != nullptr);

    // This method is invoked only on an non-empty Side, so:
    assert
      (( IsBid && (m_NL > bestIdx && bestIdx >= wrstIdx && wrstIdx >= 0))   ||
       (!IsBid && (0   <= bestIdx && bestIdx <= wrstIdx && wrstIdx <  m_NL)));

    // Before incrementing, "currDepth" should be valid:
    assert((m_ND == 0  || currDepth <= m_ND) && currDepth >= 0);

    // So actually increment the Depth (it may or may not be limited):
    ++currDepth;

    // If MktDepth is unlimited, or limited but not yet exceeded, there is no-
    // thing else to do:
    if (m_ND == 0 || currDepth <= m_ND)
      return;

    // Otherwise: MktDepth is limited, and has temporarily been exceeded. Will
    // have to delete 1 top-most level.   This is incompatible with an Orders-
    // based Book:
    assert(!WithOrdersLog && currDepth == m_ND + 1);

    OBEntry& wrst  = side[wrstIdx];
    // There should be some qty there -- but no Orders (not maintained):
    assert(IsPos(wrst.m_aggrQty) && wrst.m_nOrders == 0);

    // Delete it and decrement "currDepth" back:
    wrst.m_aggrQty = QtyU();
    if constexpr (IsBid) ++wrstIdx; else --wrstIdx;

    --currDepth;
    assert((m_ND == 0 || currDepth <= m_ND) && currDepth >= 0); // Valid again!

    // Adjust  the "wrstIdx": For Bids, move it up; for Asks, down:
    while(utxx::likely // NB: the cond below is not just "likely", it's a MUST!
         ((IsBid && wrstIdx <= bestIdx) || (!IsBid && wrstIdx >= bestIdx)))
    {
      OBEntry& en = side[wrstIdx];
      assert(!IsNeg(en.m_aggrQty) && en.m_nOrders == 0);

      if (IsPos(en.m_aggrQty))
        // Got a non-empty level, which will now be the "wrst":
        break;
      else
        { if constexpr (IsBid) ++wrstIdx; else --wrstIdx; }
    }
    // Can it happen that "wrstIdx" is now inconsistent with "bestIdx", ie the
    // "side" has become completely empty? -- No, it should not be, because ini-
    // tially, this method was invoked after an insertion,  so at least 1 valid
    // entry must remain:
    CHECK_ONLY
    (
      if (utxx::unlikely
         ((IsBid && wrstIdx > bestIdx) || (!IsBid && wrstIdx < bestIdx)))
      {
        // XXX: This is a serious logic error (essentially an assertion failure).
        // We make the side empty and throw an exception:
        PriceT& bestPx = IsBid ? m_BBPx : m_BAPx;
        bestIdx   = -1;
        wrstIdx   = -1;
        bestPx    = PriceT(); // NaN
        currDepth = 0;
        throw utxx::logic_error
              ("OrderBook::IncrementDepth: UnExpectedly-Empty Book");
      }
    )
  }

  //=========================================================================//
  // "DecrementDepth":                                                       //
  //=========================================================================//
  // Similar to "IncrementDepth" above:
  //
  template<bool IsBid>
  inline void OrderBook::DecrementDepth()
  {
    // Only invoked in Dense mode:
    assert(!m_isSparse);

    // Get the Side and its Depth:
    int& currDepth = IsBid ? m_BD  : m_AD;

    // Initially, "currDepth" (to be decremented) must be valid, and non-0:
    assert((m_ND == 0 || currDepth <= m_ND) && currDepth >= 0);

    CHECK_ONLY
    (
      if (utxx::unlikely(currDepth == 0))
        // XXX: This is a serious logic error -- no corrective action really:
        throw utxx::logic_error
              ("OrderBook::DecrementDepth: UnExpectedly Depth=0");
    )
    // If OK: Really decrement it:
    --currDepth;

    // If we have arrived at Depth==0, the side must be empty:
    CHECK_ONLY
    (
      int& bestIdx = IsBid ? m_BBI : m_BAI;

      if (utxx::unlikely(currDepth == 0 && bestIdx != -1))
      {
        // XXX: This is a serious logic error. Make the side empty and throw an
        // exception:
        int   & wrstIdx = IsBid ? m_LBI  : m_HAI;
        PriceT& bestPx  = IsBid ? m_BBPx : m_BAPx;
        bestIdx         = -1;
        wrstIdx         = -1;
        bestPx          = PriceT(); // NaN
        currDepth       = 0;
        throw utxx::logic_error
              ("OrderBook::DecrementDepth: Depth=0 but the Book was not Empty");
      }
    )
  }

  //=========================================================================//
  // "GetPxStepMultiple":                                                    //
  //=========================================================================//
  // Private method, so can be inlined:
  //
  template<bool IsRelaxed>
  inline int OrderBook::GetPxStepMultiple(double a_numer) const
  {
    // Only invoked in Dense mode:
    assert(!m_isSparse);

    double r  = a_numer / m_instr->m_PxStep;
    double rn = Round(r);

    // Unless we are in the "Relaxed" mode, "r" must be close to "rn" (within
    // "s_Tol"):
    CHECK_ONLY
    (
      if (utxx::unlikely
         (!IsFinite(rn)                                         ||
         (!IsRelaxed                                            &&
            ((rn != 0.0 && Abs(r / rn - 1.0) > PriceT::s_Tol)   ||
             (rn == 0.0 && Abs(r)            > PriceT::s_Tol) ))))
        // Propagate an exception:
        throw utxx::runtime_error
              ("OrderBook::GetPxStepMultiple: ", m_instr->m_FullName.data(),
               ": PxDelta=",  a_numer,  " is not a multiple of PxStep=",
               m_instr->m_PxStep, " (r = ", r, ", rn = ", rn,
               ", Abs(r / rn - 1.0) = ", Abs(r / rn - 1.0),
               ",  PriceT::s_Tol = ", PriceT::s_Tol, ")");
    )
    // If OK:
    return int(rn);
  }

  //=========================================================================//
  // "UpdateSparseSide":                                                     //
  //=========================================================================//
  // NB: The types of the Bid and Ask sides are different in this case!
  //
  template
  <
    bool     IsBid,
    bool     WithOrdersLog,
    bool     SnapShotsOnly,
    bool     IsRelaxed,
    bool     IsFullAmt,
    QtyTypeT QT,
    typename QR
  >
  inline OrderBook::UpdateEffectT OrderBook::UpdateSparseSide
  (
    FIX::MDUpdateActionT a_action,     // Use UNDEFINED if not known
    PriceT               a_px,
    Qty<QT,QR>           a_qty,        // WithOrdersLog: Delta; other: TargQty
    OrderInfo*           a_order       // Order affected (iff WithOrdersLog)
  )
  {
    // Only invoked in Sparse mode:
    assert(m_isSparse && IsFinite(a_px));

    // Ref to the Side to be updated:
    typename std::conditional_t<IsBid, BidsMap, AsksMap>& sideMap =
      const_cast<typename std::conditional_t<IsBid, BidsMap, AsksMap>&>
      (GetSideMap<IsBid>());

    UpdateEffectT res = UpdateEffectT::L2;  // Made more precise below
    Qty<QT,QR>    newQty;                   // TBD; initially 0

    //-----------------------------------------------------------------------//
    // Find or install the "OBEntry" for this Px:                            //
    //-----------------------------------------------------------------------//
    auto it =  sideMap.find(a_px);
    if  (it == sideMap.end())
    {
      // No such Px level, or the Side was empty altogether. Then the "newQty"
      // is the given "a_qty" (no matter whether WithOrdersLog is set or not):
      newQty = VerifyQty<IsBid,QT,QR>(a_qty, a_px);
      assert(!IsNeg(newQty));

      // Normally, "newQty" should be > 0, of course:
      if (utxx::likely(IsPos(newQty)))
      {
        // Install a new "OBEntry", empty as yet, but immediately install the
        // OrderBook ptr in it:
        auto   insRes = sideMap.try_emplace(a_px);
        assert(insRes.second);
        it =   insRes.first;

        assert(it->first == a_px);
        it->second.m_ob  =  this;

        // If the new level is L1, make "res" more specific:
        if (utxx::unlikely(it == sideMap.begin()))
          res = UpdateEffectT::L1Px;
      }
      else
      {
        // Nothing to do -- attempting to delete a non-existing level:
        LOG_WARN(4,
          "OrderBook::UpdateSparseSide: {}: {}: Non-existant Px={}",
          m_instr->m_FullName.data(), IsBid ? "Bid" : "Ask", double(a_px))
        return UpdateEffectT::NONE;
      }
    }
    else
    {
      // The Px level already exists, so it must have a valid OrderBook ptr:
      assert(it->second.m_ob == this);

      // In the FullOrdersLog mode, we need its prev qty, since "a_qty" is a
      // delta in that case:
      Qty<QT,QR> prevQty = it->second.template GetAggrQty<QT,QR>();
      newQty =
        VerifyQty<IsBid,QT,QR>(WithOrdersLog ? prevQty + a_qty : a_qty, a_px);

      // If the level found is L1, and its Qty is being updated, modify the
      // "res":
      if (it == sideMap.begin() && newQty != prevQty)
        res = UpdateEffectT::L1Qty;
    }
    //-----------------------------------------------------------------------//
    // Update this "OBEntry" (Orders if used, and AggrQty):                  //
    //-----------------------------------------------------------------------//
    assert(it != sideMap.end() && it->first == a_px && !IsNeg(newQty));
    OBEntry& obe = it->second;    // Ref!

    // NB: IsSparse=true:
    UpdateOBE<IsBid, WithOrdersLog, IsRelaxed, true, QT, QR>
      (a_action, a_px, newQty, a_order, &obe, &res);

    // If the resulting "AggrQty" is 0, it could only be a "Delete", in which
    // case remove the whole Px level:
    if (IsZero(obe.m_aggrQty))
    {
      assert(a_action == FIX::MDUpdateActionT::Delete);
      bool   remL1    =  (it == sideMap.begin());
      sideMap.erase(it);

      // If we have removed the L1, another level will become L1, so adjust the
      // "res" (unless "UpdateOBE" has set it to ERROR):
      if (remL1 && res != UpdateEffectT::ERROR)
        res = UpdateEffectT::L1Px;
    }
    //-----------------------------------------------------------------------//
    // Propagate the BestPx:                                                 //
    //-----------------------------------------------------------------------//
    // (For safety, we do this in ANY case, the overhead is negligible):
    if constexpr (IsBid)
      m_BBPx =
        utxx::unlikely(m_bidsMap.empty())
        ? PriceT()
        : m_bidsMap.begin()->first;
    else
      m_BAPx =
        utxx::unlikely(m_asksMap.empty())
        ? PriceT()
        : m_asksMap.begin()->first;

    assert((m_bidsMap.empty() || m_BBPx == m_bidsMap.begin()->first) &&
           (m_asksMap.empty() || m_BAPx == m_asksMap.begin()->first));
    // All Done!
    return res;
  }

  //=========================================================================//
  // "CheckOBEntry":                                                         //
  //=========================================================================//
  // For OrdersLog-based OrderBooks, checks that the aggregated Qty at a given
  // px level is consistent with the chain of "OrderInfo" data:
  // The template param is for efficiency only, but XXX in general, this method
  // is VERY EXPENSIVE (esp if "DeepCheck" is set), so  only invoked  in Debug
  // mode:
  //
  template
  <
    bool     DeepCheck,
    bool     IsRelaxed,
    bool     IsSparse,
    QtyTypeT QT,
    typename QR
  >
  inline bool OrderBook::CheckOBEntry
  (
    OBEntry*  a_obe,    // If non-NULL, "a_is_bid", "a_px" are for logging only
    bool      a_is_bid, // Otherwise, they are used to find the OBEntry first
    PriceT    a_px      //
  )
  const
  {
    assert(IsSparse == m_isSparse);
    assert((IsValidQtyRep<QT,QR>(m_qt, m_withFracQtys)));

    // If "a_obe" is not specified, find it first:
    if (utxx::unlikely(a_obe == nullptr))
    {
      if constexpr (IsSparse)
      {
        // Sparse mode:
        if (a_is_bid)
        {
          auto it = m_bidsMap.find(a_px);
          if (utxx::unlikely(it == m_bidsMap.end()))
            // No such OBEntry -- which is considered to be OK:
            return true;
          a_obe = &(it->second);
        }
        else
        {
          auto it = m_asksMap.find(a_px);
          if (utxx::unlikely(it == m_asksMap.end()))
            // Again, this is considered ti be OK:
            return true;
          a_obe = &(it->second);
        }
      }
      else
      {
        // Dense mode:
        // Get the Side and the Index of "a_px" there:
        OBEntry* side = a_is_bid ? m_bids : m_asks;
        assert(side != nullptr);

        int    bestIdx = a_is_bid ? m_BBI  : m_BAI;
        if (utxx::unlikely(bestIdx < 0))
          // This side of the OrerBook is empty, which is OK by itself:
          return true;

        // If we cannot get a PxStep multiple for "a_px", something is
        // definitely wrong:
        PriceT bestPx = a_is_bid ? m_BBPx : m_BAPx;
        assert(IsFinite(bestPx));
        int s = 0;
        try
          { s = bestIdx + GetPxStepMultiple<IsRelaxed>(a_px - bestPx); }
        catch (std::exception const&)
          { return false; }

        if (utxx::unlikely(s < 0 || s >= m_NL))
          return false;

        // OK, identified the Aggregated Entry:
        a_obe = side + s;
      }
    }
    assert(a_obe != nullptr && a_obe->m_ob == this);

    // The following invariants must always hold:
    // Either the Entry is empty by all criteria, or Non-Empty by all criteria:
    // Check the Entry: It must be Empty by all criteria, or Non-Empty by all
    // criteria:
    assert((a_obe->m_frstOrder == nullptr && a_obe->m_lastOrder == nullptr &&
            a_obe->m_nOrders   == 0       && IsZero(a_obe->m_aggrQty))     ||
           (a_obe->m_frstOrder != nullptr && a_obe->m_lastOrder != nullptr &&
            a_obe->m_nOrders   >  0       && IsPos (a_obe->m_aggrQty)) );

    // Furthermore, if there is only 1 Order in this Entry, the following inva-
    // riants must hold; same holds if there are no Orders at all   (both ptrs
    // are NULL then):
    assert((a_obe->m_nOrders   <= 1) ==
           (a_obe->m_frstOrder == a_obe->m_lastOrder));

    // "DeepCheck": Verify the Aggregared Qty:
    if constexpr (DeepCheck)
    {
      // Compute the total qty of all Orders there:
      Qty<QT,QR> oQty;  // Initially 0
      for (OrderInfo const* info = a_obe->m_frstOrder; info != nullptr;
           info = info->m_next)
        oQty += info->m_qty.GetQty<QT,QR>(m_qt, m_withFracQtys);

      // Aggregated qty -- must be same:
      Qty<QT,QR> aQty = a_obe->GetAggrQty<QT,QR>();

      // NB: In the fractional case, tolerance is applied here:
      CHECK_ONLY
      (
        if (utxx::unlikely(oQty != aQty))
        {
          LOG_ERROR(2,
            "OrderBook::CheckOBEntry: {}, Side={}, Px={}: Inconsistency: "
            "Aggr={}, Ords={}",
            m_instr->m_FullName.data(), a_is_bid ? "Bid" : "Ask",
            double(a_px), QR(aQty), QR(oQty))
          return false;
        }
      )
    }
    // If we got here: Everything is OK:
    return true;
  }

  //=========================================================================//
  // "CorrectBook":                                                          //
  //=========================================================================//
  template<bool WithOrdersLog,    QtyTypeT QT,  typename QR>
  inline OrderBook::UpdatedSidesT OrderBook::CorrectBook()
  {
    assert((IsValidQtyRep<QT,QR>(m_qt, m_withFracQtys)));
 
    UpdatedSidesT sides = UpdatedSidesT::NONE;

    //-----------------------------------------------------------------------//
    // First, the case of a Sparse OrderBook:                                //
    //-----------------------------------------------------------------------//
    if (m_isSparse)
    {
      // If either side is empty, nothing to do -- such a Book is consistent:
      if (utxx::unlikely(m_bidsMap.empty() || m_asksMap.empty()))
      {
        assert(IsConsistent());
        return sides;   // NONE as yet
      }
      // For both sides being non-empty, the following invariants must hold at
      // the beginning:
      assert(m_BBPx == m_bidsMap.begin()->first &&
             m_BAPx == m_asksMap.begin()->first);

      // Otherwise, traverse the not-most-recently updated side and eliminate
      // the entries colliding with the other (most-recently-updated) one:
      //
      if (m_lastUpdatedBid)
      {
        for (auto it = m_asksMap.begin(); it != m_asksMap.end(); )
          if (it->first <= m_BBPx)
          {
            auto next = std::next(it);
            m_asksMap.erase(it);
            it = next;
          }
          else
            break;  // No more collisions!

        // Update BestAskPx:
        m_BAPx =
          utxx::unlikely(m_asksMap.empty())
          ? PriceT()
          : m_asksMap.begin()->first;

        // NB: In this case, the Ask side is modified!
        sides |=  UpdatedSidesT::Ask;
      }
      else
      {
        for (auto it = m_bidsMap.begin(); it != m_bidsMap.end(); )
          if (it->first >= m_BAPx)
          {
            auto next = std::next(it);
            m_bidsMap.erase(it);
            it = next;
          }
          else
            break;  // No more collisions!

        // Update BestBidPx:
        m_BBPx =
          utxx::unlikely(m_bidsMap.empty())
          ? PriceT()
          : m_bidsMap.begin()->first;

        // NB: In this case, the Bid side is modified!
        sides |=  UpdatedSidesT::Bid;
      }
      // All Done:
      assert(IsConsistent()                                            &&
             (m_bidsMap.empty() || m_BBPx == m_bidsMap.begin()->first) &&
             (m_asksMap.empty() || m_BAPx == m_asksMap.begin()->first));

      return sides;
    }

    //-----------------------------------------------------------------------//
    // Dense OrderBook:                                                      //
    //-----------------------------------------------------------------------//
    // Check that both Sides are initialised:
    // If not, the Book should not be corrected at all -- it is actually consi-
    // dered to be consistent:
    if (utxx::unlikely(!IsFinite(m_BBPx) || !IsFinite(m_BAPx)))
    {
      assert((m_LBI == -1 && m_BBI == -1) || (m_BAI == -1 && m_HAI == -1));
      return sides; // NONE as yet
    }
    // Each Book side must exist and must be consistent by itself:
    assert(m_LBI != -1    && m_BBI != -1  && m_BAI != -1    && m_HAI != -1 &&
           m_LBI <= m_BBI && m_BBI < m_NL && m_BAI <= m_HAI && m_HAI < m_NL);

    // NB: "m_BBI" and "m_BAI" are UNRELATED to each other -- there is no or-
    // dering between them!
    // And, because the whole Book may be incosistent, we may have
    // m_BBPx >= m_BAPx -- this will need to be corrected:

    double pxStep = m_instr->m_PxStep;
    assert(pxStep > 0.0);

    if (m_lastUpdatedBid)
    {
      //---------------------------------------------------------------------//
      // The Bid side is considered to be OK, the Ask side is Wrong:         //
      //---------------------------------------------------------------------//
      // Will compare Ask pxs with the following adjusted BidPx (so that
      // BidPx==AskPx would also be detected and corrected):
      //
      PriceT threshPx = m_BBPx + pxStep / 2.0;
      bool   fixed    = false;

      // Traverse the Ask side upwards and correct all wrong entries:
      for (int i = m_BAI; i <= m_HAI; ++i)
      {
        Qty<QT,QR> qtyI = m_asks[i].GetAggrQty<QT,QR>();
        assert(!IsNeg(qtyI));

        if (IsZero(qtyI))
        {
          assert(i != m_BAI && i != m_HAI);
          continue;
        }
        PriceT pxI = m_BAPx + double(i - m_BAI) * pxStep; // Pos incr
        if (pxI < threshPx)
        {
          // This Ask level is to be cleaned. For consistency, if WithOrdersLog
          // is set,  we need to reset the Orders at this level as well:
          if constexpr (WithOrdersLog)
            ResetOrders(m_asks[i].m_frstOrder);
          m_asks[i] = OBEntry();
        }
        else
        {
          // The first correct Ask level will become the new BestAsk:
          // pxI > threshPx = BBPx + pxStep/2 > BBPx:
          m_BAPx = pxI;
          m_BAI  = i;
          fixed  = true;
          assert(m_BAI <= m_HAI && m_HAI < m_NL);
          break;
        }
      }
      if (utxx::unlikely(!fixed))
      {
        // The whole Ask side has been eliminated? This is extremely unlikely:
        m_BAPx = PriceT(); // NaN
        m_BAI  = -1;
        m_HAI  = -1;
      }
      // NB: In this case, the Ask side is modified!
      sides |=  UpdatedSidesT::Ask;
    }
    else
    {
      //---------------------------------------------------------------------//
      // The Ask side is considered to be OK, the Bid side is Wrong:         //
      //---------------------------------------------------------------------//
      // Will compare Bid pxs with the following adjusted AskPx (similar to
      // the case above):
      //
      PriceT threshPx = m_BAPx - pxStep / 2.0;
      bool   fixed    = false;

      // Traverse the Bid side downwards and correct all wrong entries:
      for (int i = m_BBI; i >= m_LBI; --i)
      {
        Qty<QT,QR>    qtyI = m_bids[i].GetAggrQty<QT,QR>();
        assert(!IsNeg(qtyI));

        if (IsZero(qtyI))
        {
          assert(i != m_BBI);
          continue;
        }
        PriceT pxI = m_BBPx + double(i - m_BBI) * pxStep; // Neg incr
        if (pxI > threshPx)
        {
          // This Bid level is to be cleaned. For consistency, if WithOrdersLog
          // is set,  we need to reset the Orders at this level as well:
          if constexpr (WithOrdersLog)
            ResetOrders(m_bids[i].m_frstOrder);
          m_bids[i] = OBEntry();
        }
        else
        {
          // The first correct Bid level will become the new BestBid:
          // pxI < threshPx = BAPx - pxStep/2 < BAPx:
          m_BBPx = pxI;
          m_BBI  = i;
          fixed  = true;
          assert(m_LBI <= m_BBI && m_BBI < m_NL);
          break;
        }
      }
      if (utxx::unlikely(!fixed))
      {
        // The whole Bid side has been eliminated? This is extremely unlikely:
        m_BBPx = PriceT(); // NaN
        m_BBI  = -1;
        m_LBI  = -1;
      }
      // NB: In this case, the Bid side is modified:
      sides |=  UpdatedSidesT::Bid;
    }
    //-----------------------------------------------------------------------//
    // FINALLY: the Book must now be Consistent:                             //
    //-----------------------------------------------------------------------//
    assert(IsConsistent());
    return sides;
  }

  //=========================================================================//
  // "Traverse":                                                             //
  //=========================================================================//
  // "a_depth":    if 0, considered to be unlimited;
  // Action:       (int a_level, PriceT a_px, OBEntry const& a_entry) -> bool;
  // Return value: continue ("true") or stop traversing ("false"):
  //
  template<bool IsBid, typename Action>
  inline void OrderBook::Traverse(int a_depth, Action const& a_action) const
  {
    assert(a_depth >= 0);
    if (a_depth == 0)
      a_depth = INT_MAX;

    typename std::conditional_t<IsBid, BidsMap, AsksMap> const& sideMap =
      GetSideMap<IsBid>();

    // If the Side is empty, return immediately:
    bool isEmpty =
      m_isSparse ? sideMap.empty() : ((IsBid ? m_BBI : m_BAI) < 0);

    if (utxx::unlikely(isEmpty))
      return;

    // Generic Case: NB: "d" is the curr depth, NOT counting empty levels;
    // "s" is the curr slot, possibly empty (Dense OrderBook):
    if (!m_isSparse)
    {
      //---------------------------------------------------------------------//
      // Dense OrderBook:                                                    //
      //---------------------------------------------------------------------//
      if (IsBid)
        assert(0 <= m_LBI && m_LBI <= m_BBI && m_BBI < m_NL);
      else
        assert(0 <= m_BAI && m_BAI <= m_HAI && m_HAI < m_NL);

      double pxStep = m_instr->m_PxStep;

      for (int d = 0,  s = IsBid ? m_BBI : m_BAI;
           d < a_depth && utxx::likely(s >= 0 && s < m_NL);
           IsBid ? --s : ++s)
      {
        if ((IsBid && s < m_LBI) || (!IsBid && s > m_HAI))
          break;   // The whole side has been traversed

        OBEntry const& obe = IsBid ? m_bids[s] : m_asks[s];
        assert(!IsNeg(obe.m_aggrQty));

        if (IsPos(obe.m_aggrQty))
        {
          // Yes, this is a real non-empty OrderBook level, so invoke the
          // "a_action" and increment "d". NB: For BOTH Bid and Ask sides,
          // the Px increases with "s":
          //
          PriceT px = (IsBid ? m_BBPx : m_BAPx) +
                      double(s - (IsBid ? m_BBI : m_BAI)) * pxStep;

          if (!a_action(d, px, obe))
            return;   // Exit requested from the call-back
          ++d;
        }
      }
    }
    else
    {
      //---------------------------------------------------------------------//
      // Sparse OrderBook:                                                   //
      //---------------------------------------------------------------------//
      int d = 0;
      for (auto it  = sideMap.begin();
                it != sideMap.end() && d < a_depth;  ++it, ++d)
      {
        OBEntry const& obe = it->second;
        assert (IsPos (obe.m_aggrQty));

        if (!a_action(d, it->first, obe))
          return;
      }
    }
    // All Done!
  }

  //=========================================================================//
  // "GetMidPx":                                                             //
  //=========================================================================//
  template<QtyTypeT QT, typename QR>
  inline PriceT OrderBook::GetMidPx(Qty<QT,QR> a_cum_vol) const
  {
    assert((IsValidQtyRep<QT,QR>(m_qt, m_withFracQtys)));
    constexpr bool ToPxAB = false;
    return
      // NB: Here the "Arg Qty Type" and the "Intrinsic Qty Type" are same, and
      // no Px conversion to Px(A/B) is performed:
      ArithmMidPx
        (GetVWAP1<QT, QR, true,  QT, QR, ToPxAB>(a_cum_vol),
         GetVWAP1<QT, QR, false, QT, QR, ToPxAB>(a_cum_vol));
  }

  //=========================================================================//
  // "GetVWAP":                                                              //
  //=========================================================================//
  template
  <
    QtyTypeT OBQT,
    typename OBQR,
    QtyTypeT ArgQT,
    typename ArgQR,
    bool     IsBid
  >
  inline void OrderBook::GetVWAP(ParamsVWAP<ArgQT>* a_params) const
  {
    // Checks:
    assert(a_params != nullptr   &&
          (IsValidQtyRep<OBQT,OBQR>(m_qt, m_withFracQtys)));
    CHECK_ONLY
    (
      if (utxx::unlikely(a_params->m_manipRedCoeff < 0.0 ||
                         a_params->m_manipRedCoeff > 1.0))
        throw utxx::badarg_error("OrderBook::GetVWAP: Invalid ManipRedCoeff");
    )
    // The implementation is parameterised by the "IsSparse" for efficiency:
    if (m_isFullAmt)
    {
      if (m_isSparse)
        GetVWAP_Impl<OBQT,OBQR,ArgQT,ArgQR,IsBid,true, true >(a_params);
      else
        GetVWAP_Impl<OBQT,OBQR,ArgQT,ArgQR,IsBid,true, false>(a_params);
    }
    else
    {
      if (m_isSparse)
        GetVWAP_Impl<OBQT,OBQR,ArgQT,ArgQR,IsBid,false,true >(a_params);
      else
        GetVWAP_Impl<OBQT,OBQR,ArgQT,ArgQR,IsBid,false,false>(a_params);
    }
  }

  //=========================================================================//
  // "GetVWAP_Impl":                                                         //
  //=========================================================================//
  template
  <
    QtyTypeT OBQT,
    typename OBQR,
    QtyTypeT ArgQT,
    typename ArgQR,
    bool     IsBid,
    bool     IsFullAmt,
    bool     IsSparse
  >
  inline void OrderBook::GetVWAP_Impl(ParamsVWAP<ArgQT>* a_params) const
  {
    assert(IsSparse == m_isSparse);
    assert((IsValidQtyRep<OBQT,OBQR>(m_qt, m_withFracQtys)));

    //-----------------------------------------------------------------------//
    // Get the Side:                                                         //
    //-----------------------------------------------------------------------//
    // For a Dense  OrderBook only:
    OBEntry const* side    = IsBid ? m_bids : m_asks;
    int            bestIdx = IsBid ? m_BBI  : m_BAI;

    // For both Dense and Sparse OrderBooks:
    PriceT         bestPx  = IsBid ? m_BBPx : m_BAPx;

    // For a Sparse OrderBook only:
    typename std::conditional_t<IsBid, BidsMap, AsksMap> sideMap;
    if constexpr (IsSparse)
      assert(bestPx == sideMap.begin()->first);

    // The results are initially set to NaN:
    for (int i = 0; i < ParamsVWAP<ArgQT>::MaxBands; ++i)
    {
      a_params->m_vwaps  [i] = PriceT();
      a_params->m_wrstPxs[i] = PriceT();
    }

    // Is this Side completely empty? Then return all NaNs:
    if (utxx::unlikely
       ((!IsSparse && bestIdx < 0) || (IsSparse && sideMap.empty())))
    {
      assert(!IsFinite(bestPx));
      return;
    }
    //-----------------------------------------------------------------------//
    // Generic Case: Traverse the Side:                                      //
    //-----------------------------------------------------------------------//
    // For a Dense  OrderBook:
    double pxStep = (!IsSparse) ? m_instr->m_PxStep : 0.0;
    int    s      = bestIdx;

    // For a Sparse OrderBook: use the "it" iterator:
    auto   it     = sideMap.begin();
    assert(!IsSparse || it != sideMap.end());

    // Qty  at the curr (initially L1) OrderBook level, CONVERTED into ArgQty.
    // XXX: If this requires QtyA <-> QtyB conversion, Px(A/B) is required;
    // such a conversion is NOT a very good idea in general, but if required,
    // we may use the CurrPxLevel to that end; so initially, it is "bestPx":
    //
    Qty<OBQT,OBQR> obQty   =
      ((!IsSparse) ? side[s] : sideMap.begin()->second)
      .template GetAggrQty<OBQT,OBQR>();

    PriceT           px  = bestPx;
    Qty<ArgQT,ArgQR> qty =
      QtyConverter<OBQT,ArgQT>::template Convert<OBQR,ArgQR>
      (obQty, *m_instr, px);

    assert(IsPos(qty));

    // The curr band to be computed, and its remaining size to be satisfied:
    int  n  = 0;
    Qty<ArgQT,ArgQR> remQty(ArgQR(a_params->m_bandSizes[n]));

    if  (!IsPos(remQty))
      // End-of-Bands (XXX: There is no explicit "nBands" param!):
      return;

    // If there are aggressive (mkt-style) orders already active, which are
    // going to eat up the liquidity from L1 until those orders are filled,
    // ADD them to the "remQty" to be satisfied at band n=0. (NB: In theory,
    // "m_exclMktOrdsSz" can even be negative -- if know there are limit or-
    // ders on the fly which will ADD liquidity to L1, say):
    //
    remQty += Qty<ArgQT,ArgQR>(ArgQR(a_params->m_exclMktOrdsSz));

    //-----------------------------------------------------------------------//
    // Iterate until all Bands are filled, or no more Px Levels available:   //
    //-----------------------------------------------------------------------//
    while (true)
    {
      // "qty" is not 0 initially, but may become 0 if we at an empty level:
      assert(!IsNeg(qty) && IsFinite(px));

      if (IsPos(qty))   // Same as qty != 0
      {
        // For a Dense OrderBook, the actual Px at level "s". NB: For BOTH Bid
        // and Ask sides, the Px increases with "s":
        assert(IsPos(remQty) && IsPos(qty));

        // "nOrds" is the number of orders at that level. (If the OrderBook is
        // not OrdersLog-based, "nOrds" would be 0, and would be ignored):
        int nOrds = ((!IsSparse) ? side[s] : it->second).m_nOrders;

        //-------------------------------------------------------------------//
        // Full-Amount (Non-Sweepable) Pxs?                                  //
        //-------------------------------------------------------------------//
        // XXX: The following is slightly inefficient -- using a run-time param
        // rather than a template param, just for simplicity (because this meth-
        // od is often called from the Strategies).
        // In this case,
        //
        if constexpr (IsFullAmt)
        {
          assert(n == 0);
          if (qty >= remQty)
          {
            // Yes, found a px level large enough to accommodate our band:
            a_params->m_wrstPxs[n] = px;
            a_params->m_vwaps  [n] = px;
            // All Done:
            return;
          }
          // Otherwise: Not enough liquidity at this px level, will fall throw
          // and go to the next one...
        }
        else
        //-------------------------------------------------------------------//
        // Generic Case:                                                     //
        //-------------------------------------------------------------------//
        {
          // Is this is a single order, likely to be placed by a manipulator?
          //  NB: this flag can be reset later if it turns out to be our own
          // order:
          bool manip = (nOrds == 1);

          // Subtract the size of our own order (if found at this level)?  Any
          // resulting negative qty (which may happen if we don't know our own
          // size presizely) is silently converted to 0.  NB: If OurPx is NaN,
          // we will never get the equality below:
          //
          if (utxx::unlikely(px == a_params->m_exclLimitOrdPx))
          {
            qty  -= Qty<ArgQT,ArgQR>(ArgQR(a_params->m_exclLimitOrdSz));
            if (IsNeg(qty))
              qty = Qty<ArgQT,ArgQR>();
            // But this is our order, not a manipulated one:
            manip = false;
          }
          bool atL1 =
            (!IsSparse && (s == bestIdx))     ||
             (IsSparse && it == sideMap.begin());

          if (utxx::unlikely(manip && (atL1 || !(a_params->m_manipOnlyL1))))
            // Reduce the available "qty" due to manipulation possibility:
            qty *= a_params->m_manipRedCoeff;

          // Adjust "remQty" (which remains to be satisfied in Band "n"):
          Qty<ArgQT,ArgQR> delta  = (remQty < qty) ? remQty : qty;
          assert(!IsNeg(delta));
          remQty    -= delta;
          qty       -= delta;
          // Qty remaining at this level after our demand was satisfied
          assert(!(IsNeg(remQty) || IsNeg(qty)));

          // Accumulate VWAP for band "n". BEWARE that "m_vwaps" is initialised
          // to NaN:
          double incr = double(delta) * double(px);

          if (utxx::unlikely(!IsFinite(a_params->m_vwaps[n])))
            a_params->m_vwaps[n]  = PriceT(incr);
          else
            a_params->m_vwaps[n] += incr;

          if (IsZero(remQty))
          {
            //---------------------------------------------------------------//
            // This Band has now been done:                                  //
            //---------------------------------------------------------------//
            a_params->m_vwaps  [n] /= double(a_params->m_bandSizes[n]);
            a_params->m_wrstPxs[n]  = px;

            // Verify the semi-monotonicity of Band Pxs (both VWAPs and Wrst Pxs
            // are in fact only Semi-Monotonic):
            CHECK_ONLY
            (
              if (utxx::unlikely
                 (n >= 1  &&
                 (( IsBid &&
                  (a_params->m_vwaps  [n] > a_params->m_vwaps  [n-1]   ||
                   a_params->m_wrstPxs[n] > a_params->m_wrstPxs[n-1])) ||
                  (!IsBid &&
                  (a_params->m_vwaps  [n] < a_params->m_vwaps  [n-1]   ||
                   a_params->m_wrstPxs[n] < a_params->m_wrstPxs[n-1])) )))
                throw utxx::logic_error
                  ("OrderBook::GetVWAP: Non-Semi-Monotonic Results");
            )
            // If OK:
            if (n == ParamsVWAP<ArgQT>::MaxBands - 1)
              // All Done:
              return;

            // Otherwise: Initialise the next band. XXX: For FullAmt pxs, we
            // currently allow only 1 band:
            ++n;
            if constexpr (IsFullAmt)
              throw utxx::badarg_error
                    ("OrderBook::GetVWAP: Multiple Bands not supported for "
                     "FullAmount");

            remQty = Qty<ArgQT,ArgQR>(a_params->m_bandSizes[n]);
            if (!IsPos(remQty))
              // Again, All Done:
              return;

            // NB: Any "qty" remaining at this level is to be applied immedia-
            // tely. Ie, if there is any remainin "qty", we do not move to the
            // next px level yet:
            if (IsPos(qty))
              continue;  // Continue the outer loop
          }
        }
        // End of the Generic Case (!FullAmt)
      }
      // End of qty != 0

      // So if we got here, in the Generic Case the "qty" at the curr level MUST
      // be 0 -- either it was 0 originally, or we have eaten it up  (NB: but in
      // the FullAmt / NonSweepable case, this may not be true):
      assert(IsFullAmt || IsZero(qty));

      //---------------------------------------------------------------------//
      // Move to the next px level:                                          //
      //---------------------------------------------------------------------//
      bool atEnd = false;
      if constexpr (!IsSparse)
      {
        IsBid ? --s : ++s;
        atEnd = (IsBid && s < m_LBI) || (!IsBid && s > m_HAI);
      }
      else
      {
        it    = ++it;
        atEnd = (it == sideMap.end());
      }

      if (utxx::unlikely(atEnd))
      {
        // Insufficient liquidity on that side, some Bands not filled, incl the
        // curr one -- reset it to NaN:
        a_params->m_vwaps  [n] = PriceT();
        a_params->m_wrstPxs[n] = PriceT();
        return;
      }
      // Otherwise: OK, get the new Qty at the new level, and go on. CONVERSION
      // occurs HERE.  Again, QtyA <-> QtyB conversions are also allowed (using
      // the CurrPxLevel), but they are undesirable:
      //
      obQty = ((!IsSparse) ? side[s] : it->second)
              .template GetAggrQty<OBQT,OBQR>();
      px    =
        (!IsSparse) ? (bestPx + double(s - bestIdx) * pxStep) : it->first;

      qty   = QtyConverter<OBQT,ArgQT>::template Convert<OBQR,ArgQR>
              (obQty, *m_instr, px);
      // And go over...
    }
    __builtin_unreachable();
  }

  //=========================================================================//
  // "CheckSeqNums":                                                         //
  //=========================================================================//
  // Private method, so can be inlined -- explicit instances not required:
  //
  template<bool InitMode>
  inline void OrderBook::CheckSeqNums
  (
    SeqNum      a_rpt_seq,
    SeqNum      a_seq_num,
    char const* a_where
  )
  const
  {
    assert(a_where != nullptr);

    // In any case, "InitMode" must be consistent with the "m_initModeOver"
    // flag. NB: This flag does NOT mean that the book is fully initialised
    // and ready to use:
    // (*) If there is no InitMode at all (eg we get only SnapShots),   then
    //     this flag is always OFF, because InitMode did not even start; yet
    //     the Book may be directly usable after each SnapShot;
    // (*) If we have both SnapShots and IncrUpdates, this flag becomes  ON
    //     when the 1st IncrUpdate arrives; yet the Book may not be usable
    //     until ALL buffered IncrUpdates are installed;
    // (*) In any case, this flag could be ON but the Book may be empty  or
    //     without liquidity on one side:
    //
    if (!InitMode && !m_initModeOver)
      // m_initModeOver and InitMode are both "false"; this is OK: leaving the
      // InitMode right now:
      m_initModeOver = true;
    else
    if (InitMode  &&  m_initModeOver)
      // This is NOT OK: We are already Initialised, and switching back to
      // InitMode:
      throw utxx::runtime_error
            ("OrderBook::", a_where, ": Cannot switch back to InitMode");

    // Now: If SeqNums are enabled, check them:
    if (m_withSeqNums)
    {
      // XXX: To prevent "stuck" errors, always advance both "LastUpdate*" co-
      // unters to the max values, even if we later throw an exception and do
      // not carry out this particular update:
      SeqNum oldSeqNum   = m_lastUpdateSeqNum;
      m_lastUpdateSeqNum = std::max<SeqNum>(oldSeqNum, a_seq_num);

      // SeqNums must be non-decreasing in both Init and Non-Init Modes (but
      // may not be strictly increasing, because multiple updates can corres-
      // pond to multiple MDEs within the same msg with the same SeqNum):
      CHECK_ONLY
      (
        if (utxx::unlikely(a_seq_num <= 0 || a_seq_num < oldSeqNum))
          throw utxx::badarg_error
                ("OrderBook::", a_where,  ": ", m_instr->m_FullName.data(),
                 ", InitMode=", InitMode, ": Invalid SeqNum=",   a_seq_num,
                 ", LastUpdateSeqNum=",   oldSeqNum);
      )
    }
    // Similarly, if RptSeqs are enabled, check them as well:
    if (m_withRptSeqs)
    {
      // Again, advance the "LastUpdate*" first:
      SeqNum oldRptSeq   = m_lastUpdateRptSeq;
      m_lastUpdateRptSeq = std::max<SeqNum>(oldRptSeq, a_rpt_seq);

      // RptSeqs (per-instr) are just non-decreasing in the InitMode, and either
      // increasing by exactly 1 (ie continuous!) in the Non-InitMode, or merely
      // increasing (if declared to be Non-Cont):
      CHECK_ONLY
      (
        if (utxx::unlikely
           (a_rpt_seq <= 0 || a_rpt_seq <  oldRptSeq         ||
           (!InitMode &&
             (( m_contRptSeqs && a_rpt_seq != oldRptSeq + 1) ||
              (!m_contRptSeqs && a_rpt_seq <= oldRptSeq)))   ))
          throw utxx::badarg_error
                ("OrderBook::", a_where,  ": ", m_instr->m_FullName.data(),
                 ", InitMode=", InitMode, ", Invalid RptSeq=",   a_rpt_seq,
                 ", LastUpdateRptSeq=",   oldRptSeq);
      )
    }
    // All Done!
  }

  //=========================================================================//
  // "VerifyQty": Make a Non-Negative Qty:                                   //
  //=========================================================================//
  // Private method, so can be inlined -- explicit instances not required:
  //
  template<bool IsBid, QtyTypeT QT, typename QR>
  inline Qty<QT,QR> OrderBook::VerifyQty(Qty<QT,QR> a_qty, PriceT a_px) const
  {
    assert((IsValidQtyRep<QT,QR>(m_qt, m_withFracQtys)));

    // Any resulting negative Qty is adjusted to 0, with a warning:
    if (utxx::unlikely(IsNeg(a_qty)))
    {
      LOG_WARN(2,
        "OrderBook::VerifyQty: {}, Side={}, Px={}: NewQty={}: Reset to 0",
        m_instr->m_FullName.data(),   IsBid ? "Bid" : "Ask",  double(a_px),
        QR(a_qty))
      a_qty = Qty<QT,QR>();    // Zero!
    }
    assert(!IsNeg(a_qty));
    return a_qty;
  }

  //=========================================================================//
  // Qty Accessors:                                                          //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "GetBestBidQty":                                                        //
  //-------------------------------------------------------------------------//
  template<QtyTypeT QT, typename QR>
  inline   Qty<QT,QR>   OrderBook::GetBestBidQty() const
  {
    assert((IsValidQtyRep<QT,QR>(m_qt, m_withFracQtys)));
    if (m_isSparse)
    {
      bool   isEmpty =  m_bidsMap.empty();
      assert(isEmpty || IsPos(m_bidsMap.begin()->second.m_aggrQty));
      return
        (utxx::unlikely(isEmpty))
        ? Qty<QT,QR>()
        : m_bidsMap.begin()->second.GetAggrQty<QT,QR>();
    }
    else
    {
      bool   isEmpty = (m_BBI < 0);
      assert(isEmpty || IsPos(m_bids[m_BBI].m_aggrQty));
      return
        utxx::unlikely(isEmpty)
        ? Qty<QT,QR>()
        : m_bids[m_BBI].GetAggrQty<QT,QR>();
    }
    __builtin_unreachable();
  }

  //-------------------------------------------------------------------------//
  // "GetBestAskQty":                                                        //
  //-------------------------------------------------------------------------//
  template<QtyTypeT QT, typename QR>
  inline   Qty<QT,QR>   OrderBook::GetBestAskQty() const
  {
    assert((IsValidQtyRep<QT,QR>(m_qt, m_withFracQtys)));
    if (m_isSparse)
    {
      bool   isEmpty =  m_asksMap.empty();
      assert(isEmpty || IsPos(m_asksMap.begin()->second.m_aggrQty));
      return
        (utxx::unlikely(isEmpty))
        ? Qty<QT,QR>()
        : m_asksMap.begin()->second.GetAggrQty<QT,QR>();
    }
    else
    {
      bool   isEmpty = (m_BAI < 0);
      assert(isEmpty || IsPos(m_asks[m_BAI].m_aggrQty));
      return
        utxx::unlikely(isEmpty)
        ? Qty<QT,QR>()
        : m_asks[m_BAI].GetAggrQty<QT,QR>();
    }
    __builtin_unreachable();
  }

  //=========================================================================//
  // "Print" (to the specified depth; 0 means infinity):                     //
  //=========================================================================//
  template<QtyTypeT QT, typename QR>
  void OrderBook::Print(int a_depth) const
  {
    // We have to restrict the depth to avoid buffer overflow:
    CHECK_ONLY
    (
      if (utxx::unlikely(a_depth <= 0 || a_depth > 50))
        throw utxx::badarg_error
              ("OrderBook::Print: Invalid Depth=", a_depth, ", allowed range: "
               "1..50");
    )
    // Buffer for output -- with up to 100 levels of Bids and Asks, this size
    // is certainly sufficient (approx  40 bytes per entry!):
    char  buff[4096];
    char* vbuff = buff;
    int   off   = 0;

    // Printing an OrderBookEntry: Level:Px:Qty :
    // XXX: This method is for debugging only, we do not care about efficiency,
    // so use of "sprintf" is OK:
    auto PrintEntry =
      [this, vbuff, a_depth, &off]
      (int a_level, PriceT a_px, OBEntry const& a_obe)->bool
      {
        off +=
          utxx::unlikely(m_withFracQtys)
          ? sprintf(vbuff + off, "%d:%.6lf:%lf",
                    a_level + 1, double(a_px),
                    double(a_obe.GetAggrQty<QT,QR>()))
          : sprintf(vbuff + off, "%d:%.6lf:%ld",
                    a_level + 1, double(a_px),
                    long(a_obe.GetAggrQty<QT,QR>()));

        if (a_level < a_depth - 1)
        {
          strcpy(vbuff + off, ", ");
          off += 2;
        }
        return true;
      };

    // Top Matter:
    off += sprintf(vbuff + off, "%s: SeqNum=%ld, RptSeq=%ld",
                   GetInstr().m_FullName.data(),
                   GetLastUpdateSeqNum(), GetLastUpdateRptSeq());
    // Bids:
    strcpy(vbuff + off, "\nBIDS: ");
    off += 7;
    Traverse<true> (a_depth, PrintEntry);

    // Asks:
    strcpy(vbuff + off, "\nASKS: ");
    off += 7;
    Traverse<false>(a_depth, PrintEntry);

    // Now use the logger to actually output the "buff":
    assert(size_t(off) < sizeof(buff));
    m_logger->info(buff);
  }

  //=========================================================================//
  // "GetXPx": Common Implementation of "GetVWAP1" and "GetDeepestPx":       //
  //=========================================================================//
  template
  <
    OrderBook::XPxMode       Mode,
    QtyTypeT OBQT,  typename OBQR,
    bool     IsBid,
    QtyTypeT ArgQT, typename ArgQR,
    bool     ToPxAB
  >
  PriceT OrderBook::GetXPx(Qty<ArgQT,ArgQR> a_cum_vol) const
  {
    // NB: CumVol==0 is not allowed here (because it will be used as a denom):
    CHECK_ONLY
    (
      if (utxx::unlikely(!IsPos(a_cum_vol)))
        throw utxx::badarg_error("OrderBook::GetVWAP1: CumVol <= 0");
    )
    // Get the CumPx. NB: We may potentially receive NaN here (if there is not
    // enough liquidity), in which case NaN is returned:
    //
    PriceT           cumPx;   // Initially 0; actually, it is a different Qty!
    Qty<ArgQT,ArgQR> remVol = a_cum_vol;

    auto scanner =
      [this, a_cum_vol, &remVol, &cumPx]
      (int,  PriceT a_px_level, OrderBook::OBEntry const& a_obe) -> bool
      {
        // Get the OrderBook-typed Qty at this PxLevel:
        Qty<OBQT, OBQR>  lq  =  a_obe.GetAggrQty<OBQT,OBQR>();

        // Convert it into the "Arg" QtyType. If this involves QtyA<->QtyB con-
        // version (which is undesirable, as can cause exception in case of ze-
        // ro Px), we use the CurrPx for that:
        Qty<ArgQT,ArgQR> lqa =
          QtyConverter<OBQT, ArgQT>::template Convert<OBQR, ArgQR>
            (lq, *m_instr, a_px_level);

        assert(IsPos(remVol) && IsPos(lqa));

        Qty<ArgQT,ArgQR> vol = Min(remVol, lqa);
        assert(IsPos(vol));

        // XXX: UnTyped "cumPx" update -- for the moment, using the OrderBook's
        // intrinsic InstrPx. NB: do NOT use "if constexpr()" here, to avoid  a
        // bogus compiler warning:
        //
        if (Mode == XPxMode::VWAP1)
          cumPx += double(a_px_level * (vol / a_cum_vol));
        else
          cumPx  = a_px_level;

        // Continue if there is a remaining vol yet. XXX: "remVol" qty is auto-
        // adjusted to 0, so the following should be safe even in the presence
        // of rounding errors:
        remVol -= lqa;
        assert(!IsNeg(remVol));
        return  IsPos(remVol);
      };

    // Run the "scanner" to the unlimited depth. NB: in general, pxs may be neg-
    // ative, so we can say nothing about the sign of "cumPx", only that it must
    // be Finite:
    this->Traverse<IsBid>(0, scanner);
    assert(!IsNeg(remVol) && IsFinite(cumPx));

    // Check if there still is an unreached CumVol, in that case return NaN to
    // invalidate the result:
    if (utxx::unlikely(IsPos(remVol) || !IsFinite(cumPx)))
      return PriceT();   // NaN

    // May need conversion into Px(A/B): TODO: Use different types for ContrPx
    // and Px(A/B)!
    if constexpr(ToPxAB)
      cumPx = m_instr->GetPxAB(cumPx);

    // Any non-finite values (incl infinities, due to erroneous conversion into
    // Px(A/B)) are converted into NaN:
    return utxx::likely(IsFinite(cumPx)) ? cumPx : PriceT();
  }

  //=========================================================================//
  // "GetVWAP1" and "GetDeepestPx":                                          //
  //=========================================================================//
  template
  <
    QtyTypeT OBQT,  typename OBQR,
    bool     IsBid,
    QtyTypeT ArgQT, typename ArgQR,
    bool     ToPxAB
  >
  inline PriceT OrderBook::GetVWAP1(Qty<ArgQT,ArgQR> a_cum_vol) const
  {
    return
      GetXPx<XPxMode::VWAP1,OBQT,OBQR,IsBid,ArgQT,ArgQR,ToPxAB>(a_cum_vol);
  }

  template
  <
    QtyTypeT OBQT,  typename OBQR,
    bool     IsBid,
    QtyTypeT ArgQT, typename ArgQR,
    bool     ToPxAB
  >
  inline PriceT OrderBook::GetDeepestPx(Qty<ArgQT,ArgQR> a_cum_vol) const
  {
    return
      GetXPx<XPxMode::DeepestPx,OBQT,OBQR,IsBid,ArgQT,ArgQR,ToPxAB>(a_cum_vol);
  }
} // End namespace MAQUETTE
