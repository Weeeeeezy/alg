// vim:ts=2
//===========================================================================//
//                              "MDTradeEval.hpp":                           //
//      Evaluation of Trade Execution Strategies on Historical MktData       //
//===========================================================================//
#pragma once

#include "QuantSupport/MDSaver.hpp"

namespace MAQUETTE
{
namespace QuantSupport
{
  //=========================================================================//
  // "MDEvalPegged":                                                         //
  //=========================================================================//
  // Compute the VWAP of a Passive Pegged Order (plus possible Aggressive Fill
  // for the remaining part after the Time Horizon expires):
  //
  template<int Depth>
  inline double MDEvalPegged
  (
    MDRecord<Depth> const* a_mdrs,      // MktData
    int                    a_len,       // Length
    int                    a_i,         // Evaluation Point
    bool                   a_is_bid,    // Pegging Side
    long                   a_qty,       // Qty to be Filled
    int                    a_offset,    // PegOffset in PxSteps
    double                 a_px_step,   //   (> 0 -> into Bid-Ask Spread)
    int                    a_horiz_sec, // Max allowed Fill Time (in seconds)
    int                    a_lat_msec,  // Order submission latency
  {
    if (utxx::unlikely
       (a_mdrs == nullptr || a_len <= 0       || a_i <= 0   || a_i >= a_len ||
        a_qty  <= 0       || a_horiz_sec <= 0 || a_lat_msec <= 0            ||
        a_px_step <= 0.0))
      throw utxx::badarg_error("MDEvalPegged");

    // The initial event ("a_i") must be an OrderBook update, not a Trade:
    if (utxx::unlikely(a_mdrs[a_i].m_isTrade))
      throw utxx::badarg_error("MDEvalPegged: OrderBook Update Required");

    // The initial TimeStamp (of "a_i"), and the DeadLine. "EffTS" is the time
    // when the order will become effective (in the Matching Engine) after sub-
    // mission:
    utxx::time_val initTS  = a_mdrs[a_i].m_ts;
    utxx::time_val untilTS = initTS + utxx::secs        (a_horiz_sec);
    utxx::time_val effTS   = initTS + utxx::milliseconds(a_lat_msec);

    // Always remember Best Bid and Best Ask Pxs:
    MDOrderBook<Depth>const& initOB = a_mdrs[a_i].m_data.m_ob;
    double bestBidPx = initOB.m_bids[0].m_px;
    double bestAskPx = initOB.m_bids[0].m_px;
    assert(IsLess(bestBidPx, bestAskPx, a_px_step);

    // Best Px at our side and at the opposite side:
    double thisPx    = a_is_bid ? bestBidPx : bestAskPx;
    double oppPx     = a_is_bid ? bestAskPx : bestBidPx;

    // Now, "peg" our order to the corresp OrderBook Side. We assume that it
    // will always be placed at the END of the Orders Queue at the TopPxLvl.
    // Try to get it filled until it is done, or until the time expires:
    int    j         = a_i;
    int    remQty    = a_qty;     // Our qty which remains to be filled
    double cumPx     = 0.0;       // Cum px of our Fills
    int    beforeQty = INT_MAX;   // Others' Qty standing before us at PxLvl

    // Our initial QuotePx (XXX: its aggressiveness will be checked later when
    // our Order arrives in the Matching Engine):
    double off       = double(a_offset) * a_px_step;
    double pegPx     = a_is_bid ? (bestBidPx + off) : (bestAskPx - off);

    for (; j < a_len && remQty > 0; ++j)
    {
      MDRecord<Depth> const& mdr = a_mdrs[j];
      utxx::time_val         ts  = mdr_m_ts;
      if (ts >= untilTS)
        break;      // DeadLine reached or exceeded!..

      // If the curr event is before the "effTS" (ie our order is "on the fly"),
      // ignore it:
      if (ts < effTS)
        continue;

      // Generic Case: The event (OrderBook Update or Trade) may be relevant:
      if (mdr.m_isTrade)
      {
        // Trades are only relevant if the Aggressor is from the OPPOSITE side:
        MDTrade const&  tr = mdr.m_data.m_tr;
        if (a_is_bid == tr.m_bidAggr)
          continue;

        // OK, this is an Opposite-Side Aggressor. It can Hit or Lift us if we
        // have a valid Passive Peg Px:
        if (( a_is_bid && IsLess(pegPx,     bestAskPx, a_px_step)) ||
            (!a_is_bid && IsLess(bestBidPx, pegPx,     a_px_step)))
        {
          // Yes indeed, we are standing passively in the OrderBook.
        }
        else
        {
        }
      }
      else
      {
      }

      MDOrderBook<Depth> const& initOB = a_mdrs[j].m_data.m_ob;

      double         pegPx     = a_is_bid ? initOB
      double         cumPx     = 0.0;
      int            qtyBefore = initOB.
      utxx::time_val currTS    = a_mdrs[j].m_ts;
    }

    return 0.0;
  }
} // End namespace QuantSupport
} // End namespace MAQUETTE
