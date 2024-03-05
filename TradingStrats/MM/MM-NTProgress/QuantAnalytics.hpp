// vim:ts=2:et
//===========================================================================//
//             "TradingStrats/MM-NTProgress/QuantAnalytics.hpp":             //
//                           Quote Price Management                          //
//===========================================================================//
#pragma  once
#include "MM-NTProgress.h"
#include <utility>

namespace MAQUETTE
{
  //=========================================================================//
  // "MkQuotePxs":                                                           //
  //=========================================================================//
  inline bool MM_NTProgress::MkQuotePxs
  (
    int            a_T,
    int            a_I,
    utxx::time_val a_ts_strat,
    PriceT        (&a_new_pxs)[2][MaxBands]  // NB: Ref to Array!
  )
  {
    //-----------------------------------------------------------------------//
    // Checks:                                                               //
    //-----------------------------------------------------------------------//
    int         nBands  = m_nBands[a_T][a_I];
    assert(0 <= nBands && nBands <= MaxBands);

    static_assert(MaxBands <= OrderBook::ParamsVWAP<QTO>::MaxBands,
                 "MaxBands too large -- not supported by OrderBook::GetVWAP");
    assert((a_T == TOD || a_T == TOM) && (a_I == AB || a_I == CB));

    // XXX: Currently, THE ONLY Src of QuotePxs is MICEX. This allows us to use
    // Qty<QTM,QRM> on MktData here!
    OrderBook const* ob    = m_obsMICEX[a_T][a_I];
    assert(ob != nullptr);

    // Check if we have exceeded the SoftLimit for the Quotes Rate:
    double currReqsRate    = m_omcNTPro.GetCurrReqsRate();
    bool   isOverSoftLimit = (currReqsRate > m_maxReqsPerSec);

    LOG_INFO(3,
      "MM-NTProgress::MkQuotePxs: CurrReqsRate={}/sec, SoftLimit={}/sec",
      currReqsRate, m_maxReqsPerSec)

    // NB: Don't use "instr" from the OrderBook -- that's the Src -- use the
    // Targ one instead:
    SecDefD const*  instr  = m_instrsNTPro[a_T][a_I];
    assert(instr != nullptr);
    double pxStep = instr->m_PxStep;

    // Since we got to the QuotePxs Computation point, the following conds must
    // hold:
    QtyO currPos  = GetSNPos    (a_T, a_I, a_ts_strat);
    QtyO maxPos   = m_posLimits [a_T][a_I];
    assert(maxPos >= 0);

    //-----------------------------------------------------------------------//
    // For both Sides:                                                       //
    //-----------------------------------------------------------------------//
    PriceT bestBidPx = ob->GetBestBidPx();
    PriceT bestAskPx = ob->GetBestAskPx();

    for (int S = 0; S < 2; ++S)
    {
      bool isBid = S;
      OrderBook::ParamsVWAP<QTO> params;  // All VWAPs are initially NaN

      //---------------------------------------------------------------------//
      // Fill in the "ParamsVWAP" and invoke "GetVWAP":                      //
      //---------------------------------------------------------------------//
      // In general, do NOT quote this Side if the limit is exceeded -- in that
      // case, the VWAPs will simply remain NaN. But we must STILL quote it if
      // Symmetric Bands are requested: otherwise, all quotes would be stopped!
      //
      if (utxx::likely
         (m_useSymmetricBands        ||
         (isBid && currPos < maxPos) || (!isBid && currPos > -maxPos)))
      {
        for (int B = 0; B < nBands; ++B)
          params.m_bandSizes[B] = m_qtys[a_T][a_I][B];

        params.m_manipRedCoeff = m_manipRedCoeff;
        params.m_manipOnlyL1   = m_manipRedOnlyL1;

        // Actually compute the VWAPs. Obviously, in any case, they are NOT
        // tighter than the corresp Best Pxs;
        if (isBid)
          ob->GetVWAP<QTM, QRM, QTO, QRO, Bid>(&params);
        else
          ob->GetVWAP<QTM, QRM, QTO, QRO, Ask>(&params);

        // Check the VWAP for simple monotonicity (NB: the following error is
        // NOT signalled if Bands0 VWAP is NaN -- that is checksed separately
        // below):
        if (utxx::unlikely
          (( isBid && params.m_vwaps[0] > bestBidPx) ||
           (!isBid && params.m_vwaps[0] < bestAskPx)))
        {
          // This would be a logic error in band computation, so better stop
          // now:
          LOG_CRIT(1,
            "MM-NTProgress::MkQuotePxs: VWAP Non-Monotonicity: T={}, I={}, "
            "IsBid={}: BestPx={}, Band0Px={}",     a_T, a_I, isBid,
            double(isBid ? bestBidPx : bestAskPx), double(params.m_vwaps[0]))

          return false;
        }
      }
      else
      {
        // Our position is too large -- not quoting this Side:
        LOG_INFO(3,
          "MM-NTProgress::MkQuotePxs: Pos Limit Exceeded: T={}, I={}, IsBid="
          "{}: Pos={}, Limit={}: Not Quoting...",
          a_T, a_I, isBid, QRO(currPos), QRO(maxPos))
        continue;
      }

      //---------------------------------------------------------------------//
      // Fill in the Bands:                                                  //
      //---------------------------------------------------------------------//
      for (int B = 0; B < nBands; ++B)
      {
        //-------------------------------------------------------------------//
        // Take the Src VWAP:                                                //
        //-------------------------------------------------------------------//
        // The initial "NewPx" (before all Transforms) is the computed VWAP. It
        // is normally expected to be Finite:
        PriceT newPx(params.m_vwaps[B]);

        if (utxx::unlikely(!IsFinite(newPx)))
        {
          LOG_WARN(2,
            "MM-NTProgress::MkQuotePxs: T={}, I={}, IsBid={}, Band={}: No "
            "Liquidity in the Src(?), skipping...", a_T, a_I, S, B)
        }
        else
        //-------------------------------------------------------------------//
        // If the VWAP is Finite: Apply the Transforms:                      //
        //-------------------------------------------------------------------//
        newPx =
          ApplyQuotePxTransforms
          (
            a_T,    a_I, isBid, B,  newPx,
            isBid ? bestBidPx : bestAskPx,
            isBid ? bestAskPx : bestBidPx,
            currPos, maxPos
          );
        // NB: "newPx" may still be NaN at this point

        //-------------------------------------------------------------------//
        // Check that the transformed "newPx" is still reasonable:           //
        //-------------------------------------------------------------------//
        if (utxx::unlikely
           (newPx < m_qPxRanges[a_I][0] || newPx > m_qPxRanges[a_I][1]))
        {
          // VWAP is outside of the safety range, remove it altogether:
          LOG_WARN(2,
            "MM-NTProgress::MkQuotePxs: T={}, I={}, IsBid={}, Band={}: "
            "SrcVWAP={} is outside of the valid range: {} .. {}, "
            "skipping...", a_T, a_I, S, B, double(newPx),
            double(m_qPxRanges[a_I][0]),   double(m_qPxRanges[a_I][1]))

          newPx = PriceT();  // NaN
        }

        //-------------------------------------------------------------------//
        // If the computed NewPx is NaN, reset ALL SUBSEQUENT BANDS as well: //
        //-------------------------------------------------------------------//
        if (utxx::unlikely(!IsFinite(newPx)))
        {
          // Cancel the subsequent Bands:
          for (int b2 = B; b2 < nBands; ++b2)
            a_new_pxs[S][b2]  = PriceT();  // Actually, it was already NaN!

          LOG_WARN(2,
            "MM-NTProgress::MkQuotePxs: {}_{}, SettlDate={}, {}: No NewQu"
            "otePxs in Band={}, will Cancel this and subsequent Bands...",
            instr->m_Symbol.data(), (a_T == TOD) ? "TOD" : "TOM",
            instr->m_SettlDate,      isBid       ? "Bid" : "Ask",  B)

          // Stop traversing the Bands:
          break;
        }

        //-------------------------------------------------------------------//
        // If OK: Memoise the "newPx":                                       //
        //-------------------------------------------------------------------//
        a_new_pxs[S][B] = newPx;
        assert(IsFinite(a_new_pxs[S][B]));

        //-------------------------------------------------------------------//
        // Should we stay put?                                               //
        //-------------------------------------------------------------------//
        // This is done if:
        // (*) the px change is within the resistance limit, AND    this is not
        //     Band0 (the latter is required to preserve the invariant that our
        //     pxs are never tighter than those @ MICEX);
        // (*) we are over the Soft Reqs Rate Limit, and the px modification is
        //     towards the top of the book -- then we can keep the curr px  w/o
        //     the risk of making a loss if it is hit or lifted:
        //
        PriceT currPx = m_currQPxs[a_T][a_I][S][B];

        bool  stay1  = (B != 0) &&
                       (Abs(currPx - a_new_pxs[S][B]) < m_qPxResists[a_I][B]);
        bool  stay2  = isOverSoftLimit &&
                       (( isBid && a_new_pxs[S][B] > currPx) ||
                        (!isBid && a_new_pxs[S][B] < currPx));
        if (stay1 || stay2)
        {
          // In particular, this means that the CurrPx is Finite as well:
          assert(IsFinite(currPx));
          a_new_pxs[S][B] = currPx;
        }

        //-------------------------------------------------------------------//
        // Check the Semi-Monotonicity of Band Pxs:                          //
        //-------------------------------------------------------------------//
        // NB: The Pxs returned by OrderBook::GetVWAP(), are guaranteed to be
        // Semi-Monotonic by construction. However, this property may  by now
        // be broken, eg because the prev band, or this band, was required to
        // stay put (use the CurrPx). So if that happens, we must move it away:
        if (B >= 1)
        {
          if ( isBid && a_new_pxs[S][B] > a_new_pxs[S][B-1])
            a_new_pxs[S][B] = a_new_pxs[S][B-1] - pxStep;
          else
          if (!isBid && a_new_pxs[S][B] < a_new_pxs[S][B-1])
            a_new_pxs[S][B] = a_new_pxs[S][B-1] + pxStep;
        }
      } // End of "B" loop
    }   // End of "S" loop

    //-----------------------------------------------------------------------//
    // XXX: Optionally, make the Bands "Symmetric":                          //
    //-----------------------------------------------------------------------//
    // Ie, if a Band is missing on one Side, remove it from the other Side as
    // well:
    if (utxx::likely(m_useSymmetricBands))
      for (int B = 0; B < nBands; ++B)
        if (!(IsFinite(a_new_pxs[Ask][B]) && IsFinite(a_new_pxs[Bid][B])))
        {
          a_new_pxs[Ask][B] = PriceT(); // NaN
          a_new_pxs[Bid][B] = PriceT(); //
        }

    //-----------------------------------------------------------------------//
    // Check for Bid-Ask Collisions in New QuotePxs:                         //
    //-----------------------------------------------------------------------//
    // NB: Collisions can theoretically occur (because it is proved otherwise),
    // so prevent them explicitly:
    //
    if (utxx::likely
       (IsFinite(a_new_pxs[Ask][0]) && IsFinite(a_new_pxs[Bid][0])))
    {
      // Number of PxSteps in the Bid-Ask Spread:
      int ns = int(Round((a_new_pxs[Ask][0]  - a_new_pxs[Bid][0]) / pxStep));

      if (utxx::unlikely(ns <= 0))
      {
        // Yes, there is a collision. XXX: We currently adopt the simplest me-
        // thod of resolving it -- move the pxs out on both sides, symmetrica-
        // lly, by (ns/2) px steps:
        ns       = abs(ns) + 1;
        int nBid = ns / 2;
        int nAsk = ns - nBid;
        assert(nBid >= 0 && nAsk >= 0 && nBid + nAsk >= 1);

        for (int B = 0; B < nBands; ++B)
        {
          if (utxx::likely(IsFinite(a_new_pxs[Bid][B])))
            a_new_pxs[Bid][B] -= double(nBid) * pxStep;

          if (utxx::likely(IsFinite(a_new_pxs[Ask][B])))
            a_new_pxs[Ask][B] += double(nAsk) * pxStep;
        }
      }
    }
    //-----------------------------------------------------------------------//
    // Final Independent Check:                                              //
    //-----------------------------------------------------------------------//
    // Band0 pxs must NOT be worse than the corresp MICEX pxs, otherwise we can
    // be arbitraged upon!  And all subsequent Bands must be semi-monotonic wrt
    // each other, and STRICTLY monotonic wrt the Best Opposite Src Px:
    bool bidsOK = true, asksOK = true;

    for (int B = 0; bidsOK && asksOK && (B < nBands); ++B)
    {
      PriceT bidB = a_new_pxs[Bid][B];
      PriceT askB = a_new_pxs[Ask][B];

      bidsOK =
        !IsFinite(bidB)  ||
        (utxx::likely(B > 0)
          ? (bidB <= a_new_pxs[Bid][B-1])
          : (bidB <  bestAskPx));

      asksOK =
        !IsFinite(askB)  ||
        (utxx::likely(B > 0)
          ? (askB >= a_new_pxs[Ask][B-1])
          : (askB >  bestBidPx));

      if (utxx::unlikely(!(bidsOK && asksOK)))
      {
        LOG_ERROR(2,
          "MM-NTProgress::MkQuotePxs: Invalid Pxs Generated: Symbol={}: "
          "BidBand[{}]={}, BidBand[{}]={}, AskBand[{}]={}, AskBand[{}]={}{}{}",
          instr->m_Symbol.data(),
          B,   double(a_new_pxs[Bid][B]),
          B-1, double(B > 0 ? a_new_pxs[Bid][B-1] : bestAskPx),
          B-1, double(B > 0 ? a_new_pxs[Ask][B-1] : bestBidPx),
          B,   double(a_new_pxs[Ask][B]),
          (!bidsOK ? ": Clearing Bid Quotes" : ""),
          (!asksOK ? ": Clearing Ask Quotes" : ""))
        break;
      }
    }
    // If the pxs on the corresp Side are wrong, clean the up completely --
    // this will result in Quotes being Cancelled on that Side, but XXX: this
    // is NOT a terminal error (unlike the incorrect VWAP above):
    //
    if (utxx::unlikely(!bidsOK))
      for (int B = 0;  B < nBands; ++B)
        a_new_pxs[Bid][B] = PriceT();  // NaN

    if (utxx::unlikely(!asksOK))
      for (int B = 0;  B < nBands; ++B)
        a_new_pxs[Ask][B] = PriceT();  // NaN

    // All Done:
    return true;
  }

  //=========================================================================//
  // "ApplyQuotePxTransforms":                                               //
  //=========================================================================//
  // XXX: The Essential Quantitative Analytics of Quoting comes HERE:
  // XXX: It is rather unsophisticated at the moment...
  //
  inline PriceT MM_NTProgress::ApplyQuotePxTransforms
  (
    int     a_T,
    int     a_I,
    bool    a_is_bid,
    int     a_B,
    PriceT  a_px,           // Intermediate Px to be adjusted
    PriceT  a_this_l1,
    PriceT  a_opp_l1,       // BestPx of the Opposite Side
    QtyO    a_curr_pos,
    QtyO    a_max_pos       // >= 0
  )
  const
  {
    //-----------------------------------------------------------------------//
    // First, apply the Mark-Up:                                             //
    //-----------------------------------------------------------------------//
    // The following invariants  must be guaranteed by the Caller:
    assert((a_T == TOD || a_T == TOM) && (a_I == AB  ||   a_I == CB) &&
            0 <= a_B                  &&  a_B <  m_nBands[a_T][a_I]  &&
            a_max_pos >= 0            &&  IsFinite(a_px));

    PriceT  origPx = a_px;
    double  dist   = 0.0;
    double  shft   = 0.0;

    double markUp = m_markUps[a_T][a_I][a_B];
    // Thus:
    if (a_is_bid)
      a_px -= markUp;
    else
      a_px += markUp;

    //-----------------------------------------------------------------------//
    // Compute the limit to which we can skew our price:                     //
    //-----------------------------------------------------------------------//
    // (This is done even if no skewing is actually performed). NB: The follow-
    // ing check also catches the situation when L1 MICEX Px(s) are NaN:
    //
    if (utxx::unlikely
       (( a_is_bid && !(a_this_l1 < a_opp_l1)) ||
        (!a_is_bid && !(a_this_l1 > a_opp_l1)) ))
    {
      LOG_ERROR(2,
        "MM-NTProgress::ApplyQuotePxTransforms: L1 SrcPxs Error: T={}, I={}, "
        "IsBid={}, ThisSideL1={}, OppSideL1={}",
        a_T, a_I, a_is_bid, double(a_this_l1), double(a_opp_l1))

      return PriceT();    // This quote will be removed!
    }

    // If OK:
    PriceT limitPx =
      a_is_bid
      ? std::min<PriceT>(a_this_l1 + m_maxQImprs[a_I], a_opp_l1)
      : std::max<PriceT>(a_this_l1 - m_maxQImprs[a_I], a_opp_l1);
    assert(IsFinite(limitPx));

    //-----------------------------------------------------------------------//
    // Calculate the maximum shift distance:                                 //
    //-----------------------------------------------------------------------//
    // NB: Depending on the setup, it can be applied on One Side or on Both Si-
    // des. If it is one side only, then we will lower our Ask for long pos and
    // raise our Bid for short pos:
    //
    if (utxx::likely
       (a_curr_pos != 0                                 &&
       (m_skewBothSides || (a_is_bid && a_curr_pos < 0) ||
                          (!a_is_bid && a_curr_pos > 0))))
    {
      // First of all, at this moment, we must have a positive distance to the
      // opposite side of MICEX pxs:
      assert((a_is_bid && a_px < a_opp_l1) || (!a_is_bid && a_px > a_opp_l1));

      // XXX: But it does NOT necessarily mean that we have a positive distance
      // to the "limitPx", so:
      dist = std::max(a_is_bid ? (limitPx - a_px) : (a_px - limitPx), 0.0);
      assert(dist >= 0.0);

      //---------------------------------------------------------------------//
      // Now calculate and apply the Non-Linear Abs Shift:                   //
      //---------------------------------------------------------------------//
      QtyO   absPos = Abs(a_curr_pos);
      assert(absPos > 0 && a_curr_pos != 0);

      shft =
        (utxx::unlikely(absPos >= a_max_pos)) // Incl a_max_pos == 0
        ? dist
        : // The generic case occurs HERE:
        dist * Pow(double(absPos) / double(a_max_pos), m_betas[a_I]);

      assert(0.0 <= shft && shft <= dist);

      // Apply the Shift. XXX: Be careful here: the direction of Px Translation
      // depends on Pos (which is != 0), NOT on the Side:
      if (a_curr_pos < 0)
        a_px += shft;   // Short Pos: Raise Bid and/or Ask
      else
        a_px -= shft;   // Long  Pos: Lower Bid and/or Ask
    }
    //-----------------------------------------------------------------------//
    // Post-Processing: Round the Px to a multiple of PxStep:                //
    //-----------------------------------------------------------------------//
    // This is done in order to:
    // (*) avoid crossing of Bid and Ask quotes;
    // (*) prevent P&L degradation when the PassPx is Hit/Lifted;
    // In any case, do NOT allow the px to touch the opposite side of the target
    // OrderBook:
    assert(m_instrsNTPro[a_T][a_I] != nullptr);
    double pxStep = m_instrsNTPro[a_T][a_I]->m_PxStep;

    if (a_is_bid)
    {
      // Bid: Move it Down, and below LimitPx:
      a_px.RoundDown(pxStep);
      while (a_px >= limitPx)
        a_px -= pxStep;
    }
    else
    {
      // Ask: Move it Up,   and above LimitPx:
      a_px.RoundUp  (pxStep);
      while (a_px <= limitPx)
        a_px += pxStep;
    }

    // Optionally, log the transform applied:
    LOG_INFO(3,
      "MM-NTProgress::ApplyQuotePxTransforms: T={}, I={}, Symbol={}, IsBid="
      "{}, Band={}, OrigPx={}, MarkUp={}, Pos={}, PosLimit={}, LimitPx={}, "
      "Dist={}, Shift={} --> {}",
      a_T, a_I, m_instrsNTPro[a_T][a_I]->m_Symbol.data(), a_is_bid,   a_B,
      double(origPx),  markUp,  QRO(a_curr_pos), QRO(a_max_pos),
      double(limitPx), dist,    shft,            double(a_px))

    //-----------------------------------------------------------------------//
    // All Done:                                                             //
    //-----------------------------------------------------------------------//
    assert(IsFinite(a_px));
    return a_px;
  }

  //=========================================================================//
  // "LogQuote":                                                             //
  //=========================================================================//
  inline char* MM_NTProgress::LogQuote
  (
    int       a_T,
    int       a_I,
    int       a_S,
    int       a_B,
    QStatusT  a_stat,
    char*     a_buff
  )
  const
  {
    // NB: The buffer length is NOT checked here!
    assert(a_buff != nullptr);
    char*  curr    = a_buff;

    // Band Num:
    *curr = '[';
    ++curr;
    (void)utxx::itoa(a_B, curr);
    curr  = stpcpy(curr, "]: ");

    if (a_stat == QStatusT::Done)
    {
      // Quoted Qty:
      (void)utxx::itoa(QRO(m_qtys[a_T][a_I][a_B]), curr);
      curr  = stpcpy(curr, " @ ");

      // XXX: In this case, use up to 5 dec points. Assume that we always have
      // sufficient buff space:
      curr += utxx::ftoa_left(double(m_currQPxs[a_T][a_I][a_S][a_B]),
                              curr, 20, 5);
      curr  = stpcpy(curr, ": ");

      // AOS ID:
      AOS const* aos = GetMainAOS(a_T, a_I, a_S, a_B);
      assert(aos != nullptr);
      (void)utxx::itoa(aos->m_id, curr);
      *curr=':';
      ++curr;

      // ReqID:
      Req12* req = aos->m_lastReq;
      assert(req != nullptr);
      (void)utxx::itoa(req->m_id, curr);
    }
    else
    {
      // Not Done: Display the reason why not:
      char const* reason =
        (a_stat == QStatusT::MustWait)
        ? "wait"      :
        (a_stat == QStatusT::NoQuotePx)
        ? "no-liq"    :
        (a_stat == QStatusT::PxUnchgd)
        ? "unchd"     :
        (a_stat == QStatusT::Throttled)
        ? "throttled" :
        (a_stat == QStatusT::Failed)
        ? "FAILED"
        : "???";
      curr = stpcpy(curr, reason);
    }
    return curr;
  }

  //=========================================================================//
  // "GetSNPos":                                                             //
  //=========================================================================//
  // NB: This INCLUDES the "currently-flying" MktOrders on MICEX (because we
  // know they will be filled anyway):
  //
  inline MM_NTProgress::QtyO MM_NTProgress::GetSNPos
  (
    int             a_T,
    int             a_I,
    utxx::time_val  a_ts_strat
  )
  {
    assert((a_T == TOD || a_T == TOM) && (a_I == TOD || a_I == TOM));

    // A safety check:
    InstrRisks const* irNTPro = m_irsNTPro[a_T][a_I];
    InstrRisks const* irMICEX = m_irsMICEX[a_T][a_I];

    if (utxx::unlikely(irNTPro == nullptr || irMICEX == nullptr))
    {
      DelayedGracefulStop hmm
        (this, a_ts_strat, "MM-NTProgress::GetSNPos: T=", a_T, ", I=", a_I,
         ": InstrRisks not available? NTPro=",       (irNTPro  != nullptr),
         ", MICEX=", (irMICEX != nullptr));
      // Return the safest value -- to prevent any mishaps while we are closing
      // done:
      return QtyO();  // 0
    }

    // If OK: Return the semi-netted position (netted across MICEX and NTPro,
    // but w/o full ccy netting), and include the currently-flying orders:
    //
    QtyO res = QtyO(irNTPro->m_posA + irMICEX->m_posA)
             + m_flyingCovDeltas[a_T][a_I];
    return res;
  }
} // End namespace MAQUETTE
