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
  template<int MaxBandsTP = 10>
  struct ParamsVWAP
  {
    //-----------------------------------------------------------------------//
    // Data Flds:                                                            //
    //-----------------------------------------------------------------------//
    constexpr static int MaxBands = MaxBandsTP;

    // Sequential Band Sizes (NOT cumulative!), from L1  into the OrderBook
    // Depth, for which the VWAPxs should be obtained. Expressed in the ori-
    // ginal Qty Units, NOT in Lots:
    long    m_bandSizes[MaxBands];

    // Total size of known Market-like Orders which are active at the moment
    // of VWAP calculation. Those orders will eat up the OrderBook liquidity
    // from L1, so the corresp size is to be excluded from VWAP calculations:
    long    m_exclMktOrdsSz;

    // Known Limit Orders (pxs and sizes) which also need to be excluded from
    // the VWAP calculation. (These are possibly our own orders which need to
    // be "cut out"). XXX: For the moment, we allow for only 1 such order  --
    // ie no layering. If not used, set Px=NaN and Sz=0:
    PriceT  m_exclLimitOrdPx;
    long    m_exclLimitOrdSz;

    // Treatment of "possibly manipulative" orders: If, during the OrderBook
    // traversal (provided that the OrderBook in composed of individual Ord-
    // ers), we encounter a Px Level comprised of just 1 Order (either at L1
    // or at any level, depending on the param given below), this could be a
    // "manipulative" order, and its size is countered towards   VWAP with a
    // "reduction coeff" [0..1]. If particular, if the Reduction coeff is  0,
    // the corresp qty is not counted at all; if 1, it is counted in full as
    // normal:
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
      for (int i = 0;  i < MaxBands;  ++i)
      {
        m_bandSizes[i] = 0;
        m_wrstPxs  [i] = PriceT();   // NaN
        m_vwaps    [i] = PriceT();   //
      }
    }
  };

  //=========================================================================//
  // "GetVWAP":                                                              //
  //=========================================================================//
  // One-Pass Computation of VWAPs for Multiple Cumulative Volume Bands. Iter-
  // ation over the the corresp side of the OrderBook is performed using  the
  // following iterators:
  // LevelInfo:     { m_px, m_qty, m_nOrders }
  // Initer:  () -> LevelInfo
  // Stepper: () -> LevelInfo

  template
  <
    bool     IsFullAmt,
    bool     WithFracQtys,
    typename LevelInfo,
    typename Initer,
    typename Stepper
  >
  void GetVWAPs
  (
    ParamsVWAP*    a_params,
    Initer  const& a_init,
    Stepper const& a_next
  )
  {
    //-----------------------------------------------------------------------//
    // Initialisation:                                                       //
    //-----------------------------------------------------------------------//
    assert(a_params != nullptr);

    if (utxx::unlikely(a_params->m_manipRedCoeff < 0.0 ||
                       a_params->m_manipRedCoeff > 1.0))
      throw utxx::badarg_error("OrderBook::GetVWAP: Invalid ManipRedCoeff");

    // The results are initially set to NaN:
    for (int i = 0; i < ParamsVWAP::MaxBands; ++i)
    {
      a_params->m_vwaps  [i] = PriceT();
      a_params->m_wrstPxs[i] = PriceT();
    }

    LevelInfo currLevel = a_init();

    // The curr band to be computed, and its remaining size to be satisfied:
    int  n      = 0;
    QtyU remQty = QtyU::FromL<FQ>(a_params->m_bandSizes[n]);

    // If there are aggressive (mkt-style) orders already active, which are
    // going to eat up the liquidity from L1 until those orders are filled,
    // ADD them to the "remQty" to be satisfied at band n=0. (NB: In theory,
    // "m_exclMktOrdsSz" can even be negative -- if know there are limit or-
    // ders on the fly which will ADD liquidity to L1, say):
    //
    remQty = Add<FQ>(remQty, QtyU::FromL<FQ>(a_params->m_exclMktOrdsSz));

    //-----------------------------------------------------------------------//
    // Iterate until all Bands are filled, or no more Px Levels available:   //
    //-----------------------------------------------------------------------//
    while (true)
    {
      if  (remQty.IsNonPos<FQ>())
        // End-of-Bands (XXX: There is no explicit "NBands" param!):
        break;

      // Get the Qty at the "currLevel". If the OrderBook representation is
      // Equi-Distant,  then some levels can be empty   (containing 0 qty):
      QtyU   qty = QtyU::FromD<FQ>(currLevel.second);
      assert(qty.IsNonNeg<FQ>());

      if (qty.IsNonZero<FQ>())
      {
        // If we got here, the following invariant holds:
        assert(remQty.IsPos<FQ>() && qty.IsPos<FQ>());

        // Actual Px at this level:
        PriceT px(currLevel.m_qty);

        // "nOrds" is the number of orders at that level. (If the OrderBook is
        // not OrdersLog-based, "nOrds" would be 0, and would be ignored):
        int  nOrds = currLevel.m_nOrders;

        //-------------------------------------------------------------------//
        // Full-Amount (Non-Sweepable) Pxs?                                  //
        //-------------------------------------------------------------------//
        // XXX: The following is slightly inefficient -- using a run-time param
        // rather than a template param, just for simplicity (because this meth-
        // od is often called from the Strategies).
        // In this case,
        //
        if (m_isFullAmt)
        {
          assert(n == 0);
          if (qty.IsGE<FQ>(remQty))
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
            qty = Sub<FQ>(qty, QtyU::FromL<FQ>(a_params->m_exclLimitOrdSz));
            if (qty.IsNeg<FQ>())
              qty = QtyU();
            // But this is our order, not a manipulated one:
            manip = false;
          }

          if (utxx::unlikely
             (manip && (s == bestIdx  ||  !(a_params->m_manipOnlyL1))))
            // Reduce the available "qty" due to manipulation possibility:
            qty = QtyU::FromD<FQ>(qty.ToD<FQ>() * a_params->m_manipRedCoeff);

          // Adjust "remQty" (which remains to be satisfied in Band "n"):
          QtyU delta = remQty.IsLT<FQ>(qty) ? remQty : qty;
          assert(delta.IsNonNeg<FQ>());
          remQty     = Sub<FQ>(remQty, delta);
          qty        = Sub<FQ>(qty,    delta);
          // Qty remaining at this level after our demand was satisfied
          assert(remQty.IsNonNeg<FQ>() && qty.IsNonNeg<FQ>());

          // Accumulate VWAP for band "n". BEWARE that "m_vwaps" is initialised
          // to NaN:
          double incr = delta.ToD<FQ>() * double(px);

          if (utxx::unlikely(!IsFinite(a_params->m_vwaps[n])))
            a_params->m_vwaps[n]  = PriceT(incr);
          else
            a_params->m_vwaps[n] += incr;

          if (remQty.IsZero<FQ>())
          {
            //---------------------------------------------------------------//
            // This Band has now been done:                                  //
            //---------------------------------------------------------------//
            a_params->m_vwaps  [n] /= double(a_params->m_bandSizes[n]);
            a_params->m_wrstPxs[n]  = px;

            // Verify the semi-monotonicity of Band Pxs (both VWAPs and Wrst Pxs
            // are in fact only Semi-Monotonic):
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

            // If OK:
            if (n == ParamsVWAP::MaxBands - 1)
              // All Done:
              return;

            // Otherwise: Initialise the next band:
            ++n;
            remQty = QtyU::FromL<FQ>(a_params->m_bandSizes[n]);
            if  (remQty.IsNonPos<FQ>())
              // Again, All Done:
              return;

            // NB: Any "qty" remaining at this level is to be applied immedia-
            // tely. Ie, if there is any remainin "qty", we do not move to the
            // next px level yet:
            if (qty.IsPos<FQ>())
              continue;
          }
        }
        // End of the Generic Case (!FullAmt)
      }
      // End of qty != 0

      // So if we got here, in the Generic Case the "qty" at the curr level MUST
      // be 0 -- either it was 0 originally, or we have eaten it up  (NB: but in
      // the FullAmt / NonSweepable case, this may not be true):
      assert(m_isFullAmt || qty.IsZero<FQ>());

      //---------------------------------------------------------------------//
      // Move to the next px level:                                          //
      //---------------------------------------------------------------------//
      IsBid ? --s : ++s;

      if (utxx::unlikely((IsBid && s < m_LBI) || (!IsBid && s > m_HAI)))
      {
        // Insufficient liquidity on that side, some Bands not filled, incl the
        // curr one -- reset it to NaN:
        a_params->m_vwaps  [n] = PriceT();
        a_params->m_wrstPxs[n] = PriceT();
        return;
      }

      // Otherwise: OK, get the new Qty at the new level, and go on:
      qty = side[s].m_aggrQty;
    }
  }

} // End namespace QuantSupport
} // End namespace MAQUETTE
