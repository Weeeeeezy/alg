// vim:ts=2:et
//===========================================================================//
//                             "Predictors2.cpp":                            //
//===========================================================================//
#include "Predictors.h"
#include "Basis/Maths.hpp"
#include <utxx/compiler_hints.hpp>
#include <utxx/error.hpp>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <algorithm>

using namespace std;

namespace MAQUETTE
{
namespace Predictors
{
  //=========================================================================//
  // "IntervalData" Default Ctor:                                            //
  //=========================================================================//
  inline IntervalData::IntervalData()
  : m_minFutDelta(0.0),
    m_maxFutDelta(0.0),
    m_avgFutDelta(0.0)
  {
    static_assert(sizeof(IntervalData) % sizeof(double) == 0,
                 "UnExpected Size of IntervalData");
  }

  //=========================================================================//
  // "IntervalisePMF":                                                       //
  //=========================================================================//
  void IntervalisePMF
  (
    InstrT                          a_targInstr,
    RegrSideT                       a_regrSide,
    int                             a_intervalMS,  // For intervalisation
    double                          a_trendResist, // For robust trend detection
    double                          a_log0,        // Replacement for Log(0)
    std::vector<RawTickPMF> const&  a_src,
    std::vector<IntervalData>*      a_intervals
  )
  {
    //-----------------------------------------------------------------------//
    // Initialisation:                                                       //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely
       (a_intervalMS <= 0 || a_trendResist <= 0.0 || a_intervals == nullptr))
      throw utxx::badarg_error("Intervalise: Invalid arg(s)");

    // The number of currently-allocated (but as yet empty) Intervals:
    long M = long(a_src.size() / 1000);
    a_intervals->resize(size_t(M));

    // The number of currently-filled Rows of "a_mtx" and "a_rhs". NB: The init
    // val must be (-1), not 0:
    long m          = -1;

    // The notional index of the curr time Interval (used to detect the contin-
    // uity):
    long tPrev      = -1;
    long intervalNS = long(a_intervalMS) * 1'000'000L;

    // Where we are in the current Interval:
    utxx::time_val  intervalStart;  // Initially empty
    double          intervalFrac = 0.0;

    // The curr Fut and Spot Pxs (on the requested Side), cross-Interval:
    double currFutPx    = NaN<double>;
    double currSpotPx   = NaN<double>;

    // Latest Fut and Spot Qtys -- also cross-Interval:
    long   currFutQtys [2][10];
    long   currSpotQtys[2][10];

    for (int s = 0; s <  2; ++s)
    for (int l = 0; l < 10; ++l)
    {
      currFutQtys [s][l] = 0;
      currSpotQtys[s][l] = 0;
    }

    //-----------------------------------------------------------------------//
    // Traverse the Src RawTicks:                                            //
    //-----------------------------------------------------------------------//
    for (long i = 0; i < long(a_src.size()); ++i)
    {
      //---------------------------------------------------------------------//
      // Curr Src and Targ:                                                  //
      //---------------------------------------------------------------------//
      // The curr src RawTick:
      RawTickPMF const& rtI = a_src[size_t(i)];

      // Calculate its time Interval index (the base does not matter -- we only
      // need to check the  Intervals continuity):
      long tCurr = rtI.m_ts.nanoseconds() / intervalNS;

      // Make sure the Intervals are monotonic:
      if (utxx::unlikely(tCurr < tPrev))
        throw utxx::badarg_error
              ("Intervalise: Non-Monotonic Time Stamp @ SrcI=", i);

      //---------------------------------------------------------------------//
      // Starting a new Interval? First complete the prev one:               //
      //---------------------------------------------------------------------//
      // (3 possible cases: At the very beginning, after a gap, or just a nor-
      // mal sequential new Interval):
      //
      if (utxx::unlikely(tPrev < tCurr && m >= 0 && intervalFrac < 1.0))
      {
        IntervalData& targ = (a_intervals->data())[m];

        // The remaining part of the Interval:
        double fracDelta = 1.0 - intervalFrac;

        // Avg Pxs need to be updated:
        targ.m_avgFutDelta  += fracDelta * currFutPx;
        targ.m_avgSpotDelta += fracDelta * currSpotPx;

        // Also update Avg Qtys, and compute the Logs:
        for (int s = 0; s < 2; ++s)
        {
          for (int l = 0; l < 10; ++l)
          {
            targ.m_cumFutQtys [s][l] += fracDelta * double(currFutQtys [s][l]);
            targ.m_cumSpotQtys[s][l] += fracDelta * double(currSpotQtys[s][l]);

            targ.m_cumFutLogs [s][l] =  Log(targ.m_cumFutQtys [s][l]);
            targ.m_cumSpotLogs[s][l] =  Log(targ.m_cumSpotQtys[s][l]);
          }

          // Also compute Logs of Traded Qtys, BUT here 0 qtys are quite likely:
          targ.m_tradeFutLogs [s]    =
            (targ.m_tradeFutQtys [s] > 0)
            ? Log(double(targ.m_tradeFutQtys [s])) : a_log0;

          targ.m_tradeSpotLogs[s]    =
            (targ.m_tradeSpotQtys[s] > 0)
            ? Log(double(targ.m_tradeSpotQtys[s])) : a_log0;
        }
      }

      //---------------------------------------------------------------------//
      // Gap Mgmt:                                                           //
      //---------------------------------------------------------------------//
      if (utxx::unlikely(tPrev != -1 && tCurr != tPrev && tCurr != tPrev + 1))
      {
        // This means we got a gap. Will reset the running data:
        assert(tCurr - tPrev >= 2);

        // Curr{Fut,Spot}Pxs normally extend across Intervals, so in this case,
        // we must invalidate them:
        currFutPx  = NaN<double>;
        currSpotPx = NaN<double>;

        // Same for cross-Interval Qtys:
        for (int s = 0; s <  2; ++s)
        for (int l = 0; l < 10; ++l)
        {
          currFutQtys [s][l] = 0;
          currSpotQtys[s][l] = 0;
        }
      }

      //---------------------------------------------------------------------//
      // Proceed to the next target?                                         //
      //---------------------------------------------------------------------//
      if (tPrev < tCurr)
      {
        // (This also happens at the very beginning when m==-1, tPrev==0):
        ++m;
        tPrev = tCurr;

        // If the new target row to be filled is beyond the allocated targ spa-
        // ce, extend the latter:
        if (utxx::unlikely(m >= M))
        {
          M *= 2;
          a_intervals->resize(size_t(M));
        }

        // The following running vars are reset at the beginning of each Inter-
        // val:
        intervalFrac  = 0.0;
        intervalStart = utxx::time_val(utxx::msecs(tCurr * a_intervalMS));

        // Per-Interval
        for (int s = 0; s <  2; ++s)
        for (int l = 0; l < 10; ++l)
        {
          currFutQtys [s][l] = 0;
          currSpotQtys[s][l] = 0;
        }
      }
      else
        assert(tPrev == tCurr);

      //---------------------------------------------------------------------//
      // Generic Update:                                                     //
      //---------------------------------------------------------------------//
      assert(m >= 0);
      IntervalData& targ = (a_intervals->data())[m];

      // Check the Target's TimeStamp: It is either empty yet (if we just adv-
      // anced to the new Target), or must be equal to the curr one:
      assert(targ.m_start.empty() || targ.m_start == intervalStart);

      // So it would not harm if we re-assign the TimeStamp:
      targ.m_start = intervalStart;

      // Interval fraction corresponding to this "rtI":
      double fracI =
        double((rtI.m_ts - intervalStart).nanoseconds()) / double(intervalNS);

      // The following invars are extremely important: The Fractions must be
      // monotonic within [0.0, 1.0], even if we allow for rounding errors:
      if (utxx::unlikely
         (fracI < 0.0 || fracI > 1.0 || fracI > intervalFrac))
        throw utxx::logic_error
              ("Intervalise: Invalid IntervalFractions @ SrcI=", i, ": Prev=",
               intervalFrac, ", Curr=", fracI);

      // If OK:
      double fracDelta = fracI - intervalFrac;
      assert(fracDelta >= 0.0);
      intervalFrac = fracI;

      //---------------------------------------------------------------------//
      // Futures:                                                            //
      //---------------------------------------------------------------------//
      if (rtI.m_instr == InstrT::Fut)
      {
        if (utxx::likely(!rtI.m_isTrade))
        {
          // Update the Futures Pxs. NB: At the moment, they are real Pxs,   NOT
          // Deltas yet! Initialisation of "IntervalData" guarantees correctness
          // of the following ops:
          currFutPx     =  rtI.m_pxs[int(a_regrSide)][0];
          assert(currFutPx > 0.0);

          targ.m_minFutDelta =  min<double>(targ.m_minFutDelta, currFutPx);
          targ.m_maxFutDelta =  max<double>(targ.m_maxFutDelta, currFutPx);
          targ.m_avgFutDelta += fracDelta * currFutPx;

          // Update the Futures Cumulative Qtys:
          for (int s = 0; s <  2; ++s)
          for (int l = 0; l < 10; ++l)
          {
            long q = rtI.m_qtys[s][l];
            assert(q > 0);
            currFutQtys[s][l] = q;
            // Cumulative Qtys must be strictly monotonic, of course:
            assert(l == 0 || currFutQtys[s][l-1] < q);
            targ.m_cumFutQtys[s][l] += fracDelta * double(q);
          }

          // TODO: Trend Detection
        }
        else
          // Update the Futures Trade Qtys. Here we simply sum them up:
          for (int s = 0; s <  2; ++s)
            targ.m_tradeFutQtys[s] += double(rtI.m_qtys[s][0]);
      }
      else
      //---------------------------------------------------------------------//
      // Spot:                                                               //
      //---------------------------------------------------------------------//
      {
        if (utxx::likely(!rtI.m_isTrade))
        {
          // Similar to Futures, update the Spot Pxs:
          currSpotPx   = rtI.m_pxs[int(a_regrSide)][0];
          assert(currSpotPx > 0.0);

          targ.m_minSpotDelta = min<double>(targ.m_minSpotDelta, currSpotPx);
          targ.m_maxSpotDelta = max<double>(targ.m_maxSpotDelta, currSpotPx);
          targ.m_avgSpotDelta += fracDelta * currSpotPx;

          // Update the Spot Cumulative Qtys:
          for (int s = 0; s <  2; ++s)
          for (int l = 0; l < 10; ++l)
          {
            long q = rtI.m_qtys[s][l];
            assert(q > 0);
            currSpotQtys[s][l] = q;
            // Again, Cumulative Qtys must be strictly monotonic:
            assert(l == 0 || currSpotQtys[s][l-1] < q);
            targ.m_cumSpotQtys[s][l] += fracDelta * double(q);
          }

          // TODO: Trend Detection
        }
        else
          // Sum up the Spot Trade Qtys (within this Interval):
          for (int s = 0; s <  2; ++s)
            targ.m_tradeSpotQtys[s] += double(rtI.m_qtys[s][0]);
      }

      //---------------------------------------------------------------------//
      // For the next Src Iteration:                                         //
      //---------------------------------------------------------------------//
      tPrev = tCurr;
    }

    //-----------------------------------------------------------------------//
    // 2nd Pass: Compute the Deltas and Logs:                                //
    //-----------------------------------------------------------------------//
  }

} // End namespace Predictors
} // End namespace MAQUETTE
