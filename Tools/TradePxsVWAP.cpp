// vim:ts=2:et
//===========================================================================//
//                          "Tools/TradePxsVWAP.cpp":                        //
//             Comparison of Actual Trade Pxs with Computed VWAPs            //
//===========================================================================//
#include "QuantSupport/MDSaver.hpp"

#include <iostream>
#include <cstring>

using namespace MAQUETTE;
using namespace MAQUETTE::QuantSupport;
using namespace std;

//===========================================================================//
// "main":                                                                   //
//===========================================================================//
int main(int argc, char* argv[])
{
  // XXX: OrderBook Depth is fixed (compatible with "MkHistDataBin_HotSpotFX"):
  constexpr   int LDepth = DefaultLDepth;
  using MDR = MDAggrRecord<LDepth>;

  // Params:
  if (argc != 2)
  {
    cerr << "PARAMETER: MktDataBinFile" << endl;
    return 1;
  }
  char const* fileName = argv[1];

  TraverseAggrMD<LDepth>
  (
    string(fileName),

    [](MDR const* a_mdrs, long DEBUG_ONLY(a_len), long a_i) -> bool
    {
      // The Curr "MDAggrRecord":
      assert(a_mdrs != nullptr && 0 <= a_i && a_i < a_len);
      MDR const& mdi = a_mdrs[a_i];

      // If this record does not contain an Aggression, skip it and continue:
      if (utxx::likely(!mdi.m_aggr.IsValid()))
        return true;

      // OK, we encountered an Aggression:
      utxx::time_val ts = mdi.m_ts;
      double aggrQty    = mdi.m_aggr.m_totQty;
      bool   bidAggr    = mdi.m_aggr.m_bidAggr;
      double avgPx      = mdi.m_aggr.m_avgPx;
      assert(aggrQty > 0.0 && avgPx > 0.0);

      // NB: "mdi" is the earliest OrderBook state which is ALREADY affected by
      // this Aggression. So get back to the latest OrderBook state BEFORE this
      // Aggression, compute the VWAP and "predict" the AvgTradePx:
      //
      for (long j = a_i-1; j >= 0; --j)
      {
        MDR const& mdj  =  a_mdrs[j];
        assert(mdj.m_ts <= ts);

        if (mdj.m_ts < ts)
        {
          // Yes, this is the lastest OrderBook state BEFORE the Trade. Get the
          // VWAP on the Passive side (to compare it with the AvgTradePx):
          MDAggrOrderBookEntry const (&obSide)[LDepth] =
            bidAggr ? mdj.m_ob.m_asks : mdj.m_ob.m_bids;

          // Compute the VWAP  for "aggrQty" (in Strict mode -- there should be
          // enough liquidity for the whole Aggression)!
          double vwap = VWAP<true>(obSide, aggrQty);
          double sprd = mdj.m_ob.m_asks[0].m_px - mdj.m_ob.m_bids[0].m_px;
          assert(sprd > 0.0);

          // Output (incl the Bid-Ask Spread):
          cout << ts.milliseconds()    << ' ' << (bidAggr ? 'B' : 'A') << ' '
               << avgPx << ' ' << vwap << ' ' << (avgPx - vwap)        << ' '
               << sprd  << ' ' << aggrQty     << endl;
          break;
        }
      }
      // Go ahead:
      return true;
    }
  );
  // All Done:
  return 0;
}
