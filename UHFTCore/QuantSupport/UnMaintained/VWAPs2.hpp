// vim:ts=2:et
//===========================================================================//
//                                "VWAPs.hpp":                               //
//        Computation of VWAPs for Multiple Volume Bands in One Pass         //
//===========================================================================//
#pragma once
#include "Basis/BaseTypes.hpp"

namespace MQEUETTE_A
{
namespace QuantSupport
{
  //=========================================================================//
  // "ParamsVWAP" Struct:                                                    //
  //=========================================================================//
  // Contains the args and return values of the "GetVWAPs" function (see below).
  // ven by the following type. NB: in this struct, all Qtys are just "long"s
  // in order to simplify its use at the application level:
  // NB: For FullAmt (non-sweepable) OrderBooks,   "ParamsVWAP" must contain
  // exactly 1 band:
  //
  template<unsigned MaxBandsTP = 10>
  struct ParamsVWAP
  {
    //-----------------------------------------------------------------------//
    // Data Flds:                                                            //
    //-----------------------------------------------------------------------//
    constexpr static unsigned MaxBands = MaxBandsTP;

    // Band Sizes from L1 on (Cumulative or Sequential -- the interpretation  is
    // left to "GetVWAP"!), for which the VWAPs should be obtained. Expressed in
    // the original Qty Units, NOT in Lots, rounded to an  integer  (even if the
    // corresp OrderBook uses Fractional Qtys):
    long    m_bandSizes[MaxBands];

    // Total size of known Market-like Orders which are active at the moment
    // of VWAP calculation. Those orders will eat up the OrderBook liquidity
    // from L1, so the corresp size is to be excluded from VWAP calculations:
    long    m_exclMktOrdsSz;

    // Known Limit Orders (pxs and sizes) which also need to be excluded from
    // the VWAP calculation. (These are possibly our own orders which need to
    // be "cut out"). XXX: For the moment, we allow for only 1 such order  --
    // ie no layering. If not used, set Px=NaN and Sz=0.    Not applicable to
    // FullAmt mode:
    PriceT  m_exclLimitOrdPx;
    long    m_exclLimitOrdSz;

    // Treatment of "possibly manipulative" orders: If, during the OrderBook
    // traversal (provided that the OrderBook in composed of individual Ord-
    // ers), we encounter a Px Level comprised of just 1 Order (either at L1
    // or at any level, depending on the param given below), this could be a
    // "manipulative" order, and its size is countered towards   VWAP with a
    // "reduction coeff" [0..1]. If particular, if the Reduction coeff is  0,
    // the corresp qty is not counted at all; if 1, it is counted in full as
    // normal. Again, bot applicable to FullAmt mode:
    double  m_manipRedCoeff;
    bool    m_manipOnlyL1;

    // OUTPUT: Monotinic WorstPxs and VWAP values for each Band are placed
    // here. In particular, if the required band size(s) could be obtained
    // at any px, the corresp result is NaN:
    PriceT  m_wrstPxs[MaxBands];
    PriceT  m_vwaps  [MaxBands];

    //-----------------------------------------------------------------------//
    // Default Ctor:                                                         //
    //-----------------------------------------------------------------------//
    ParamsVWAP()
    : m_bandSizes     {},
      m_exclMktOrdsSz (0),
      m_exclLimitOrdPx(),
      m_exclLimitOrdSz(0),
      m_manipRedCoeff (1.0),          // Count single orders as normal ones!
      m_manipOnlyL1   (false),
      m_wrstPxs       {},
      m_vwaps         {}
    {
      for (unsigned i = 0; i < MaxBands; ++i)
      {
        m_bandSizes[i] = 0;
        m_wrstPxs  [i] = PriceT();   // NaN
        m_vwaps    [i] = PriceT();   //
      }
    }
  };

  //=========================================================================//
  // "AdjustLiquidity":                                                      //
  //=========================================================================//
  // Liquidity size adjustment due to possible presence of a market manipulator
  // at a given px level. Returns the new (reduced) available liquidity size:
  //
  template<bool IsL1, unsigned MaxBands, typename LevelInfo>
  inline void AdjustLiquidity
  (
    ParamsVWAP<MaxBands> const& a_params,
    LevelInfo*                  a_curr_level
  )
  {
    // NB: The pre-condition (m_qty > 0) is not strictly necessary, but this is
    // our convention -- the Caller is responsible for that:
    assert(a_curr_level != nullptr && a_curr_level->m_qty > 0);

    // Manipulation is suspected if there is exactly 1 order at the curr level;
    // if the number of orders is unknown, it would be <= 0, so not applicable:
    bool manip    = (a_curr_level->m_nOrders == 1);

    // Furthermore, if (provided that the "manip" flag is set)  it is actually
    // OUR OWN order at the curr level, then there is no manipulation, so reset
    // the flag in that case:
    bool ourLevel = a_curr_level.m_px == a_params.m_exclLimitOrdPx &&
                    a_params.m_exclLimitOrdSz > 0;
    manip &= (!ourLevel);

    // Furthermore, the "manip" flag (if set) may apply to any level, or to L1
    // only:
    manip &= (IsL1 || !a_params.m_manipOnlyL1);

    // If there is still risk of manipulation, reduce the qty available at the
    // curr level according to "a_params" settings:
    if (utxx::unlikely(manip))
      a_curr_level->m_qty =
        long(Round(double(a_curr_level->m_qty) * a_params.m_manipRedCoeff));

    // Furthermore, exclude our own orders (if happen to occur at this level)
    // from the available liquidity:
    if (utxx::unlikely(ourLevel))
      a_curr_level->m_qty =
        std::max<long>(a_curr_level->m_qty - a_params.m_exclLimitOrdSz, 0);

    // All Done:
    assert(a_curr_level->m_qty >= 0);
  }

  //=========================================================================//
  // "GetVWAP":                                                              //
  //=========================================================================//
  // One-Pass computation of VWAPs for Multiple Cumulative Volume Bands. Iter-
  // ation over the the corresp side of the OrderBook is performed using  the
  // following iterators:
  //
  // LevelInfo:     { m_px (any Finite), m_qty > 0, m_nOrders }
  // Initer:  () -> LevelInfo
  // Stepper: () -> LevelInfo
  //
  // (*) For an empty level (which is possible for Equi-Distant  OrderBooks),
  //     LevelInfo returned by Initer/Stepper must have m_qty==0 but a valid
  //     m_px;
  // (*) When there are no more OrderBook liquidity levels, LevelInfo returned
  //     must contain m_qty < 0 and m_px being NaN;
  // (*) If "m_nOrders" is not known (not a FullOrdersLog-based OrderBook), it
  //     must be set to any value <= 0, so it would be ignored (it is only used
  //     for detection of possible market-manipulating orders).
  // IMPORTANT:
  // (*) Bands specified in "a_params" may be Sequential or Cumulative;
  // (*) Liquidity available (as provided by "LevelInfo" objs) may be Sweepable
  //     (which is semantically similar to Sequential) or FullAmt (a la Cumula-
  //     tive).
  // "GetVWAP" Returns "true" if all requested Bands  have been successfully
  //     computed, or "false" if there was insufficient liquidity to do that:
  //
  template
  <
    bool     CumBands,      // This is about the Demand for Liquidity
    bool     IsFullAmt,     // This is about the Supply of  Liquidity
    bool     IsBid,         // Which Side -- for verification only
    typename LevelInfo,
    typename Initer,
    typename Stepper,
    unsigned MaxBands = 10
  >
  inline bool GetVWAPs
  (
    ParamsVWAP<MaxBands>* a_params,
    Initer  const&        a_init,
    Stepper const&        a_next
  )
  {
    //-----------------------------------------------------------------------//
    // Initialisation:                                                       //
    //-----------------------------------------------------------------------//
    // The very degenerate case:
    if (MaxBands == 0)
      return true;
    assert(a_params != nullptr);

    if (utxx::unlikely(a_params->m_manipRedCoeff < 0.0 ||
                       a_params->m_manipRedCoeff > 1.0))
      throw utxx::badarg_error("GetVWAP: Invalid ManipRedCoeff");

    // The results are initially set to NaN (not 0!):
    for (unsigned i = 0; i < MaxBands; ++i)
    {
      a_params->m_vwaps  [i] = PriceT();
      a_params->m_wrstPxs[i] = PriceT();
    }

    // Get to the initial level (normally L1)  of the side  we will traverse --
    // this is done  by the Initer:
    LevelInfo currLevel = a_init();

    // The curr band to be computed, and its total and remaining size to be sa-
    // tisfied. IMPORTANT: For computational purposes, in the FullAmt mode, the
    // "bandQty" is ALWAYS a cumulative one, whereas in the Sweepable mode, it
    // is a sequential one. For the 0th Band, they are the same:
    unsigned nb         = 0;
    long     bandQty    = a_params->m_bandSizes[0];

    // Nothing to do if we immediately know that the 0th Band is empty:
    if (utxx::unlikely(bandQty <= 0))
      return true;     // Done immediately!

    // XXX: Dirty Trick: If there are aggressive (Mkt-style) orders already on
    // the fly, which are going to eat up the liquidity from L1 up,    we will
    // increase all our demand:
    // (*) For Sequential Bands, increase the 0th Band only -- all others will
    //     be shifted accordingly;
    // (*) For Cumulative Bands, inctease all of them:
    // XXX: At the moment, non-positive increments are not considered (though a
    //     negative one can actually be interpreted as expected extra liquidity
    //     supply -- but we currently don't do that):
    //
    if (a_params->m_exclMktOrdsSz > 0)
      bandQty += a_params->m_exclMktOrdsSz;

    // How muvh remains to be filled in this Band:
    long   remBandQty = bandQty;
    assert(remBandQty > 0);

    // So now we know that there is some "remBandQty" to be satisfied. If we
    // immediately know that the "currLevel" (L1) is invalid,   ie the whole
    // OrderBook side is empty, stop now and return an empty result.  For L1,
    // this would happen if any of the "currLevel" params are invalid.   XXX:
    // actually, m_qty==0 should never appear at L1, but even if it does, the
    // result is the same:
    if (utxx::unlikely(currLevel.m_qty <= 0 || !IsFinite(currLevel.m_px)))
      return false;    // We are UnDone immediately!

    // If we can proceed, apply counter-manipulation measures at L1  (which may
    // reset the qty available at L1 to 0, but this is NOT the same as L1 being
    // unavailable from the beginning -- in the former case, we can still  move
    // to higher levels). But this does NOT apply to FullAmt mode:
    if (!IsFullAmt)
      AdjustLiquidity<true>(a_params, &currLevel);

    //-----------------------------------------------------------------------//
    // Loop until all Bands are filled, or no more liquidity is available:   //
    //-----------------------------------------------------------------------//
    while (true)
    {
      // At this point, the following invariants always hold:
      assert(remBandQty      >  0 && bandQty >0  &&
             currLevel.m_qty >= 0 && IsFinite(currLevel.m_px));

      //---------------------------------------------------------------------//
      // Full-Amount (Non-Sweepable) Pxs?                                    //
      //---------------------------------------------------------------------//
      if (IsFullAmt)
      {
        // In the FullAmt mode, qty==0 is not allowed: It must not occur at any
        // "currLevel" in the 1st place, and there is no reduction due to susp-
        // ected manipulation:
        assert(currLevel.m_qty > 0);

        // Can "remBandQty" be satisfied now?
        if (currLevel.m_qty >= remBandQty)
        {
          // Yes, found a Px Level large enough to accommodate our band:
          a_params->m_wrstPxs[nb] = currLevel.m_px;
          a_params->m_vwaps  [nb] = currLevel.m_px;

          // This  Band has now been done. Setting "remBandQty" to 0 will cause
          // moving to the next Band (if exists) below:
          remBandQty = 0;

          // But the "currLevel" stays unchanged: If it is large enough, the
          // next (cumulative) Band can still be satisfied at this px!
        }
        else
        {
          // Otherwise: Not enough liquidity at this px level, so reset the
          // available qty to 0 -- this will trigger moving to the next level
          // (if exists) below:
          assert(remBandQty > 0):
          currLevel.m_qty   = 0;
        }
      }
      else
      //---------------------------------------------------------------------//
      // Generic Case (Sweepable): Skip this "currLevel" if empty:           //
      //---------------------------------------------------------------------//
      if (currLevel.m_qty > 0)
      {
        //-------------------------------------------------------------------//
        // Apply "currLevel" to the curr Band ("nb"):                        //
        //-------------------------------------------------------------------//
        long   delta = std::min<long>(currLevel.m_qty, remBandQty);
        assert(delta > 0);

        // "remBandQty" will be satisfied by "delta" @ "currLevel":
        remBandQty      -= delta;
        currLevel.m_qty -= delta;
        assert(currLevel.m_qty >= 0 && remBandQty >= 0);

        // Accumulate VWAP for band "nb". BEWARE that "m_vwaps" were initialised
        // to NaNs:
        double incr = double(delta) * double(currLevel.m_px);

        if (utxx::unlikely(!IsFinite(a_params->m_vwaps[nb])))
          a_params->m_vwaps[nb]  = PriceT(incr);
        else
          a_params->m_vwaps[nb] += incr;

        // This Band has now been done?
        if (remBandQty == 0)
        {
          // Compute the VWAP and the WrstPx for "nb". NB: Use "bandQty",
          // rather than (a_params->m_bandSizes[nb]),  because the former
          // is always sequential, not cumulative:
          a_params->m_vwaps  [nb] /= double(bandQty);
          a_params->m_wrstPxs[nb]  = currLevel.m_px;
        }
        // NB: Moving to the next band below...
      }
      // End of the Sweepable Case (!FullAmt) with m_qty > 0

      //---------------------------------------------------------------------//
      // Curr Band ("nb") has been filled?                                   //
      //---------------------------------------------------------------------//
      assert(remBandQty >= 0);
      if (remBandQty == 0)
      {
        // Yes, done. Verify the semi-monotonicity of Band Pxs (NB: both VWAPs
        // and Wrst Pxs are in fact only SEMI-Monotonic):
        if (utxx::unlikely
           (nb >= 1   &&
            !(( IsBid &&
             (a_params->m_vwaps  [nb] <= a_params->m_vwaps  [nb-1]   ||
              a_params->m_wrstPxs[nb] <= a_params->m_wrstPxs[nb-1])) ||
             (!IsBid &&
             (a_params->m_vwaps  [nb] >= a_params->m_vwaps  [nb-1]   ||
              a_params->m_wrstPxs[nb] >= a_params->m_wrstPxs[nb-1])))))
          throw utxx::logic_error
                ("GetVWAP: IsBid=", IsBid,
                 ": Non-Semi-Monotonic Results @ Band=", nb);

        // Go to the next Band:
        ++nb;

        // Stop now if there is no next Band, actually:
        assert(nb <= MaxBands);
        if (utxx::unlikely(nb == MaxBands || a_params->m_bandSizes[nb] <= 0))
          return true;     // Done!

        // The "remBandQty" to be filled at new "nb" -- if none, then it was
        // the last Band as well:
        assert(nb < MaxBands);

        // Calculate the "bandQty":
        bandQty    =
          ( IsFullAmt && !CumBands)
          ? // Force "bandsQty"to be Cumulative (for FullAmt   Liquidity):
            (bandQty + a_params->m_bandSizes[nb])
          :
          (!IsFullAmt &&  CumBands)
          ? // Force "bandQty" to be Sequential (for Sweepable Liquidity):
            (a_params->m_bandSizes[nb] - a_params->m_bandSizes[nb-1])
          : // In any other cases, use the readily-available value:
            (a_params->m_bandSizes[nb]);

        // In any case, "bandQty" must be positive at this point:
        if (utxx::unlikely(bandQty <= 0))
          throw utxx::badarg_error
                ("GetVWAP: Non-Monotonic Band found? CumBands=", CumBands,
                 ", IsBid=", IsBid, ", Band=", nb, ", Size=",
                 a_params->m_bandSizes[nb],    ", PrevSize=",
                 a_params->m_bandSizes[nb-1]);

        // If the Bands are Cumulative and there are liquidity-eating orders
        // which are already on-the-fly,

        // The remaining size of this Band to be filled: Initially equal to the
        // calculated "bandQty":
        remBandQty =   bandQty;
      }

      //---------------------------------------------------------------------//
      // Move to the next Px Level?                                          //
      //---------------------------------------------------------------------//
      assert(remBandQty > 0);

      // If there is no more liquidity at the "currLevel", we will try to move
      // to the next non-empty level:
      assert(currLevel.m_qty >= 0);

      if (currLevel.m_qty == 0)
      {
        currLevel = a_next();

        // But it might be that the new "currLevel" does not actually exist, ie
        // all available liquidity has been exhausted:
        if (utxx::unlikely(currLevel.m_qty < 0 || !IsFinite(currLevel.m_px)))
        {
          // Then the curr Band ("nb"), if already touched, must be invalidated
          // (reset to NaN):
          a_params->m_vwaps  [nb] = PriceT();
          a_params->m_wrstPxs[nb] = PriceT();
          // We are UnDone:
          return false;
        }

        // Also, we must NOT get Qty==0 in the FullAmt mode -- there must be NO
        // empty levels in that case:
        if (IsFullAmt)
        {
          if (utxx::unlikely(currLevel.m_qty == 0))
            throw utxx::runtime_error
                  ("GetVWAP(FullAmt): IsBid=", IsBid, ", Band=", nb, ": Qty=0 "
                   "encountered");
        }
        else
        // In the Sweepable mode, adjust the available liquidity  for  possible
        // manipulations and our own orders (which can reduce the available qty
        // to 0): IsL1 = false now:
        if (currLevel.m_qty > 0)
          AdjustLiquidity<false>(a_params, &currLevel);
      }
      // In the Sweepable case, we may still get (currLevel.m_qty==0) at this
      // point, which will result in a new iteration...
    }
    // End of "while" loop. This point is not reachable:
    __builtin_unreachable();
  }
} // End namespace QuantSupport
} // End namespace MAQUETTE
