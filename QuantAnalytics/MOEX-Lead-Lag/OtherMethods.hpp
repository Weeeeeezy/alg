// vim:ts=2
//===========================================================================//
//                           "OtherMethods.hpp":                             //
//      Misc Lead-Lag Analysis Methods (NOT VALIDATED, NOT RECOMMENDED!)     //
//===========================================================================//
#pragma  once
#include "QuantAnalytics/MOEX-Lead-Lag/MLL.hpp"
#include <utility>

namespace MLL
{
  //=========================================================================//
  // "ClassicalCrossCorr":                                                   //
  //=========================================================================//
  template<int N>
  double ClassicalCrossCorr
  (
    MDEntryT const (&a_instr1)[N],
    MDEntryT const (&a_instr2)[N],
    int              a_idx_from,
    int              a_idx_to,
    int              a_l,     // Lead of "instr1" over "instr2"
    int              a_winsz  // Window Size for correlation estimator
  )
  {
    assert(0 <= a_idx_from && a_idx_from < a_idx_to && a_idx_to < N &&
           a_winsz > 0);

    // Traverse the time series by "a_winsz" intervals.   We will shift the
    // "instr1" series to the left by "a_l" (but may have a_l < 0 as well),
    // so adjust the range of indices accordingly:
    a_idx_from  = std::max<int>(a_idx_from, a_idx_from + a_l);
    a_idx_to    = std::min<int>(a_idx_to,   a_idx_to   + a_l);

    // Now, make a whole number of "a_winsz" intervals in the range:
    int nInts   = (a_idx_to   - a_idx_from) / a_winsz;
    a_idx_to    =  a_idx_from + nInts       * a_winsz;
    assert(0 <= a_idx_from && 0 <= a_idx_from - a_l &&
           a_idx_to < N    && a_idx_to  - a_l < N);

    double var1  = 0.0;
    double var2  = 0.0;
    double cross = 0.0;

    // Trends are per-window:
    double trd1 = (a_instr1[a_idx_to].m_px - a_instr1[a_idx_from].m_px) /
                  double(nInts);
    double trd2 = (a_instr2[a_idx_to].m_px - a_instr2[a_idx_from].m_px) /
                  double(nInts);

    for (int i = a_idx_from; i < a_idx_to; i += a_winsz)
    {
      // Centered returns (for "instr1", use the shifted window):
      assert(i + a_winsz - a_l < N);
      double r1 = (a_instr1[i + a_winsz].m_px       - a_instr1[i].m_px)
                  - trd1;
      double r2 = (a_instr2[i + a_winsz + a_l].m_px - a_instr2[i + a_l].m_px)
                  - trd2;
      var1  += r1 * r1;
      var2  += r2 * r2;
      cross += r1 * r2;
    }
    // All done:
    return cross / sqrt(var1 * var2);
  }

  //=========================================================================//
  // "CondProbs":                                                            //
  //=========================================================================//
  // Computing a Conditional Probabilities that if "Instr1" has a tick in some
  // direction, then after a lag of "a_l",  "Instr2" has its next tick in  the
  // same direction (separately for Up and Dn):
  //
  template<int N>
  std::pair<double, double> CondProbs
  (
    MDEntryT const (&a_instr1)[N],
    MDEntryT const (&a_instr2)[N],
    int              a_idx_from,
    int              a_idx_to,
    int              a_l
  )
  {
    // Go through the ticks of Instr1:
    int n1Up = 0, n1Dn = 0, n12Up = 0, n12Dn = 0;

    a_idx_from = a_instr1[a_idx_from].m_curr;
    a_idx_to   = std::min(a_idx_to, a_idx_to - a_l);

    assert(0 <= a_idx_from && a_idx_from < a_idx_to && a_idx_to + a_l < N);

    for (int i = a_idx_from; i < a_idx_to; ++i)
    {
      // If this is not an Instr1 tick time, continue:
      if (i != a_instr1[i].m_curr)
      {
        assert(i > a_instr1[i].m_curr);
        continue;
      }
      // Also skip the very 1st Instr1 tick (when we may not be able to get its
      // return):
      int pi = a_instr1[i].m_prev;
      assert(pi < i);
      if (pi < a_idx_from)
        continue;

      // OK, "i" is an Instr1 tick time:
      // The return of Instr1 in this tick (does not need to be centered):
      double r1  = a_instr1[i].m_px - a_instr1[pi].m_px;

      // NB: Normally, "rL" should not be 0 (otherwise there is no tick!), but
      // it may occasionally be 0 due to data imperfections:
      if (r1 == 0.0)
        continue;

      // Yes, got a valid Instr1 tick:
      ((r1 > 0.0) ? n1Up : n1Dn)++;

      // Get the next Instr2 tick (after applying the "a_l" lag):
      int j = i + a_l;
      if (j <  a_idx_from)
        continue;
      if (j >= a_idx_to)
        break;

      int cj = a_instr2[j].m_curr;
      assert(cj <= j);
      if (cj < a_idx_from)
        continue;

      int nj = a_instr2[j].m_next;
      assert(nj > j);
      if (nj > a_idx_to)
        break;

      // The return of that Instr2 tick:
      double r2 = a_instr2[nj].m_px - a_instr2[cj].m_px;

      // NB: If "r2" is 0 (unlikely but possible), it will not count:
      if (r2 > 0.0 && r1 > 0.0)
        ++n12Up;
      else
      if (r2 < 0.0 && r1 < 0.0)
        ++n12Dn;
    }

    // Conditional Probabilities: P{I2Up|I1Up}, P{I2Dn|I1Dn}:
    double CPUp  = double(n12Up) / double(n1Up);
    double CPDn  = double(n12Dn) / double(n1Dn);
    return std::make_pair(CPUp, CPDn);
  }
}
// End namespace MLL
