// vim:ts=2
//===========================================================================//
//                                   "HY.hpp":                               //
//           Hayashi-Yoshida Cross-Correlation Estimator (Lagged)            //
//===========================================================================//
#pragma  once
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <utility>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <cmath>

namespace MLL
{
  //=========================================================================//
  // "HayashiYoshidaCrossCorr":                                              //
  //=========================================================================//
  template<int N>
  double HayashiYoshidaCrossCorr
  (
    MDEntryT const (&a_instr1)[N],
    StatsT   const&  a_stats1,
    MDEntryT const (&a_instr2)[N],
    StatsT   const&  a_stats2,
    int              a_idx_from,
    int              a_idx_to,
    int              a_l      // Lead of "instr1" over "instr2"
  )
  {
    assert(0 <= a_idx_from && a_idx_from < a_idx_to && a_idx_to < N);

    // Select the Leader and the Follower:
    MDEntryT const (&leader)  [N] = (a_l >= 0) ? a_instr1 : a_instr2;
    MDEntryT const (&follower)[N] = (a_l >= 0) ? a_instr2 : a_instr1;

    // Average ReturnRates for the Leader and the Follower:
    double   retL  = (a_l >= 0) ? a_stats1.m_retRate : a_stats2.m_retRate;
    double   retF  = (a_l >= 0) ? a_stats2.m_retRate : a_stats1.m_retRate;

    // NB: Unlike ReturnRates, QuadraticVars will appear in completely symmet-
    // ric way, so it does not matter which one belongs to Leader / Follower...

    // Then the "a_l" param will always be >= 0 (ie it's a real lead of the
    // Leader = lag of the Follower):
    a_l = abs(a_l);

    // The cross-covariance to be estimated:
    double cross = 0.0;

    // Go over all px intervals of the "leader":  [fromL, toL), where "fromL"
    // and "toL" are tick times:
    // Get the initial point; if it is not the left-most one for the "leader",
    // move to the beginning of that interval:
    //
    int    fromL = leader[a_idx_from].m_curr;
    assert(0 <= fromL && fromL < N && !leader[fromL].IsEmpty());
    // NB: We can get fromL < a_idx_from, but that's OK: "fromL" is still valid,
    // so the corresp "follower" interval might not be (see below)!

    while (true)
    {
      // Construct "toL" (the right end):
      int toL = leader[fromL].m_next;
      if (toL > a_idx_to)
        break;
      assert(fromL < toL);

      // Got L=[fromL, toL); the return of the "leader" over this interval is:
      assert(leader[toL].m_px > 0.0 && leader[fromL].m_px > 0.0);

      double rL = (leader[toL].m_px  - leader[fromL].m_px) -
                   retL * double(toL - fromL);

      // Now shift L=[fromL, toL) FORTH by "a_l" and find all  overlapping tick
      // intervals F for the "follower";
      // in other words, find all intervals F of the "follower", such that, if
      // shifted BACK in time by "a_l", would overlap with "L":
      int fromLS = fromL + a_l;
      int toLS   = toL   + a_l;

      assert(fromLS < toLS);
      if (toLS > a_idx_to)
        break;   // XXX: Though there may still be some valid overlaps in this
                 // case, they are disregarded...
      assert(!leader[toL].IsEmpty());

      // So the appropriate (real, not shifted) intervals F=[fromF, toF) for
      // the Follower will be: ...
      int fromF  = follower[fromLS].m_curr;
      assert(fromF <= fromLS);

      // But if we got "fromF" too low, it might be that the Follower did not
      // start yet, then skip this Leader tick as well:
      assert(0 <= fromF);
      if (fromF < a_idx_from)
      {
        fromL = toL;
        continue;
      }
      assert(!follower[fromF].IsEmpty());

      while (true)
      {
        // ...Construct "toF" (the right end):
        int toF = follower[fromF].m_next;
        if (toF > a_idx_to)
          break;
        assert(!follower[toF].IsEmpty());

        // We iterate while [fromF,  toF) has a non-empty intersection with
        //                  [fromLS, toLS):
        // By construction, the following should now hold:
        assert(fromLS < toF);

        // But stop when [fromF, toF) has completely moved out on the right:
        // No more overlaps possible in that case:
        if (toLS <= fromF)
          break;

        // So: Got an overlapping interval [fromF, toF) for [fromLS, toLS):
        assert(fromF < toLS && toF > fromLS);

        // The return of the "follower" over this interval:
        assert(follower[fromF].m_px > 0.0 && follower[toF].m_px > 0.0);

        double rF = (follower[toF].m_px - follower[fromF].m_px) -
                     retF *  double(toF - fromF);
        cross    += rL * rF;

        // Next iteration over the "follower":
        fromF = toF;
      }
      // Next Iteration over the "leader":
      fromL = toL;
    }
    // All Done:
    return cross / sqrt(a_stats1.m_var * a_stats2.m_var);
  }

  //=========================================================================//
  // "GetStatsHY": Some Descriptive Statistics for use with the HY Method:   //
  //=========================================================================//
  template<int N>
  StatsT GetStatsHY
  (
    MDEntryT const (&a_instr)[N],
    int              a_idx_from,
    int              a_idx_to
  )
  {
    assert(0 <= a_idx_from && a_idx_from < a_idx_to && a_idx_to < N);

    // NB: In accordance with "HayashiYoshidaCrossCorr", we adjust "from" to
    // the beginning of that interval, but leave "to" unchanged:
    a_idx_from = a_instr[a_idx_from].m_curr;

    double retRate = (a_instr[a_idx_to].m_px - a_instr[a_idx_from].m_px) /
                     double(a_idx_to - a_idx_from);

    int    nInts   = 0;
    double var     = 0.0;

    int from = a_idx_from;
    while (true)
    {
      if (from >= a_idx_to)
        break;
      int to = a_instr[from].m_next;
      if (to   >  a_idx_to)
        break;

      // Interval counter:
      ++nInts;

      // Centered return on this interval:
      double ret = (a_instr[to].m_px - a_instr[from].m_px) -
                   retRate * double(to - from);

      // Variance contribution:
      var += ret * ret;

      // Next iteration:
      from = to;
    }
    // The result:
    return { retRate, double(a_idx_to - a_idx_from) / double(nInts), var };
  }
}
// End namespace MLL
