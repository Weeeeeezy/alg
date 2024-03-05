// vim:ts=2:et
//===========================================================================//
//                              "Predictors1.cpp":                           //
//===========================================================================//
#include "Predictors.h"
#include "Basis/Maths.hpp"
#include "Basis/BaseTypes.hpp"
#include <utxx/compiler_hints.hpp>
#include <utxx/error.hpp>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <mkl.h>

using namespace std;

namespace
{
  using namespace MAQUETTE;

  //=========================================================================//
  // "LogTr": Non-Linear Log-Like Transform:                                 //
  //=========================================================================//
  inline double LogTr(double  a_x, double a_log0)
  {
    double res  = utxx::likely(a_x > 0.0) ? Log(a_x) : a_log0;
    return res;
  }
}

namespace MAQUETTE
{
namespace Predictors
{
  //=========================================================================//
  // "MkRawTicksPMF":                                                        //
  //=========================================================================//
  void MkRawTicksPMF
  (
    char const*         a_file_name,
    char const*         a_fut,
    char const*         a_spot,
    vector<RawTickPMF>* a_res
  )
  {
    //-----------------------------------------------------------------------//
    // Setup:                                                                //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely
       (a_file_name == nullptr || *a_file_name == '\0' ||
        a_fut       == nullptr || *a_fut       == '\0' ||
        a_spot      == nullptr || *a_spot      == '\0' || a_res == nullptr))
      throw utxx::badarg_error("MkRawTicksPMF: Invalid arg(s)");

    // Open the file:
    FILE* f = fopen(a_file_name, "r");
    if (utxx::unlikely(f == nullptr))
      throw utxx::badarg_error("Cannot open data file: ", a_file_name);

    // Input Buffer:
    char line[1024];  // Should be enough

    // The previous Time Stamp (initially empty):
    utxx::time_val  lastTS;

    // Lengths of Instr Names:
    int  futLen  = strlen(a_fut);
    int  spotLen = strlen(a_spot);

    //-----------------------------------------------------------------------//
    // The Reading Loop:                                                     //
    //-----------------------------------------------------------------------//
    for (int l = 0; ; ++l)
    {
      //---------------------------------------------------------------------//
      // Get the line (it will be 0-terminated):                             //
      //---------------------------------------------------------------------//
      if (fgets(line, int(sizeof(line)), f) == nullptr)
        // We assume this is an EOF, not an error:
        break;

      // To be filled in (initially, zeroed-out):
      RawTickPMF rt;

      //---------------------------------------------------------------------//
      // Get the TimeStamp:                                                  //
      //---------------------------------------------------------------------//
      int YYYY = -1, MM = -1, DD = -1,
          hh   = -1, mm = -1, ss = -1, sss = -1;

      if (utxx::unlikely
         (sscanf(line, "[%04d-%02d-%02d %02d:%02d:%02d.%03d] [MktData] [info] ",
                 &YYYY, &MM, &DD, &hh, &mm, &ss, &sss) != 7   || YYYY < 2016 ||
          // XXX: Leap seconds are not allowed; DaysInMonth are not checked:
          MM <= 0 || DD <= 0 || hh <  0 || mm <  0 || ss <  0 || sss  < 0    ||
          MM > 12 || DD > 31 || hh > 23 || mm > 59 || ss > 59 || sss  > 999))
        continue;

      // Ignore the entries made during unreliable time intervals (give or take
      // up to 2 mins):
      if (utxx::unlikely
         ( hh <  10              || (hh == 10 && mm <   2) ||
          (hh == 13 && mm >= 58) || (hh == 14 && mm <   6) ||
          (hh == 18 && mm >= 43) || (hh == 19 && mm <  15) ))
        continue;

      // Construct the TimeStamp in local time (MSK):
      rt.m_ts = utxx::time_val::local_time
                (YYYY, MM, DD, hh, mm, ss, sss * 1000);

      // Check the TimeStamp monotonicity; if equal TimeStamps are found (within
      // the 1 msec precision), separate them by 1 usec:
      long currMS = rt.m_ts.milliseconds();
      long lastMS = lastTS .milliseconds();

      if (utxx::unlikely(currMS < lastMS))
        throw utxx::badarg_error
              ("MkRawTicksPMF: ", a_file_name, ", Line ", l, ": Non-Monotonic "
               "TimeStamp: ",  currMS,  " < ", lastMS);
      if (utxx::unlikely(currMS == lastMS))
        rt.m_ts = lastTS + utxx::time_val(utxx::usecs(1));

      assert(lastTS < rt.m_ts);
      lastTS = rt.m_ts;

      //---------------------------------------------------------------------//
      // Trade or OrderBook Upadte?                                          //
      //---------------------------------------------------------------------//
      // (Any other actions, eg "OurTrade", are ignored):
      bool isOrderBook = (strncmp(line + 43, "OrderBook ", 10) == 0);
      bool isTrade     = (strncmp(line + 43, "Trade ",     6)  == 0);

      if (utxx::unlikely(!(isOrderBook || isTrade)))
        continue;
      // Thus, exactly one of "isOrderBook", "isTrade" flags is set:
      assert(isOrderBook == !isTrade);

      // Get the Symbol. Again, any symbols other than the specified Futures
      // and Spot ones, are ignored:
      int  symPos = isTrade ? 49 : 53;
      bool isFut  = (strncmp(line + symPos, a_fut,  futLen ) == 0);
      bool isSpot = (strncmp(line + symPos, a_spot, spotLen) == 0);

      if (utxx::unlikely(!(isFut || isSpot)))
        continue;
      // Thus, exactly one of "isFut", "isSpot" flags is set:
      assert(isFut == !isSpot);
      rt.m_instr   =   isFut ? InstrT::Fut : InstrT::Spot1;  // XXX: For now

      //---------------------------------------------------------------------//
      // OrderBook update?                                                   //
      //---------------------------------------------------------------------//
      if (utxx::likely(isOrderBook))
      {
        rt.m_isTrade = false;

        // Initially, the L1 update times will be set to the curr TimeStamp; if
        // the L1 pxs are later found to be unchanged compared to a prev tick,
        // the prev TimeStamps will be propagated:
        for (int s = 0; s < 3; ++s)
        {
          rt.m_since   [s] = rt.m_ts;
          rt.m_validFor[s] = 1;
        }

        //-------------------------------------------------------------------//
        // Get the Pxs and Cumulative Qtys (on both Bid and Ask sides):      //
        //-------------------------------------------------------------------//
        // Find the Bids and Asks MktData:
        char const* bids = line + symPos + (isFut ? futLen : spotLen);
        if (utxx::unlikely(strncmp(bids, " Bids: ", 7) != 0))
          continue;

        char const* asks = strstr (bids, "  Asks: ");
        if (utxx::unlikely(asks == nullptr))
          continue;

        for (int isBid = 1; isBid >= 0; --isBid)
        {
          char const* curr = isBid ? (bids + 7) : (asks + 8);
          // "curr" now points to the L1 Ask or Bid Px

          int l = 0;
          for (; l < 10; ++l)
          {
            // At each level, there is a pair <Px> <QtyLots>. Get the Px:
            char*  pxEnd  = nullptr;
            double px     = strtod(curr, &pxEnd);
            if (utxx::unlikely
               (!(px > 0.0) || pxEnd   == nullptr || *pxEnd  != ' '))
              break;

            // FIXME: For the Spot, Pxs are currently multiplied by 1000:
            if (!isFut)
              px *= 1000.0;

            // If found: Advance to the QtyLots:
            char* qtyEnd  = nullptr;
            long  qtyLots = strtol(pxEnd + 1, &qtyEnd, 10);

            if (utxx::unlikely
               (qtyLots <= 0 || qtyEnd == nullptr || *qtyEnd != ' '))
              break;

            rt.m_pxs [isBid][l] = px;
            rt.m_qtys[isBid][l] = qtyLots;

            // Advance the ptr beyond the ' ', so that we will jump over the Px
            // in the next iteration:
            curr = qtyEnd + 1;
          }

          // Error at any level above?
          if (utxx::unlikely(l != 10))
            continue;
        }

        //-------------------------------------------------------------------//
        // Skip this RawTick if all Pxs and Qtys are unchanged:              //
        //-------------------------------------------------------------------//
        // (This can happen if eg the OrderBook has changed at a level above
        // 10):
        // Find the prev tick of the same Kind and Instrument:
        //
        bool skip = false;
        for (auto cit = a_res->crbegin(); cit != a_res->crend(); ++cit)
        {
          RawTickPMF const& prev = *cit;
          if (!prev.m_isTrade && prev.m_instr == rt.m_instr)
          {
            // Yes, found the prev OrderBook RawTick. Is it different from "rt"
            // in terms of pxs and/or qtys:
            bool unchanged = true;
            for (int isBid = 0; isBid <  2 && unchanged; ++isBid)
            for (int l     = 0; l     < 10 && unchanged; ++l)
              if (prev.m_qtys[isBid][l]    == rt.m_qtys[isBid][l] ||
                  Abs(prev.m_pxs[isBid][l] - rt.m_pxs [isBid][l])
                  > PriceT::s_tol)
                unchanged  = false;

            // Skip "rt" if it was really found to be unchanged:
            skip &= unchanged;

            if (!skip)
            {
              // Install the L1 Px Deltas and Update Times:
              rt.m_pxDeltas[0] = rt.m_pxs[0][0] - prev.m_pxs[0][0];
              rt.m_pxDeltas[1] = rt.m_pxs[1][0] - prev.m_pxs[1][0];
              rt.m_pxDeltas[2] = 0.5 * (rt.m_pxDeltas[0] + rt.m_pxDeltas[1]);

              for (int s = 0; s < 3; ++s)
                if (Abs(rt.m_pxDeltas[s]) <= PriceT::s_tol)
                {
                  // Round near-0 Deltas to the exact 0:
                  rt.m_pxDeltas[s] = 0.0;
                  // Also, if the curr Delta is 0, propagate the prev TimeStamp:
                  rt.m_since   [s] = prev.m_since   [s];
                  rt.m_validFor[s] = prev.m_validFor[s] + 1;
                }
            }
            // Don't look any further into depth -- we already know whether this
            // "rt" is to be skipped or not:
            break;
          }
        }
        if (utxx::unlikely(skip))
          continue; // This "rt" is NOT to be installed
      }
      else
      //---------------------------------------------------------------------//
      // Trade?                                                              //
      //---------------------------------------------------------------------//
      {
        // Yes, it is a Trade:
        rt.m_isTrade = true;

        // Get the Trade Px:
        char*  pxEnd    = nullptr;
        double tradePx  = strtod
                          (line + symPos + (isFut ? futLen : spotLen), &pxEnd);
        if (utxx::unlikely
           (!(tradePx > 0.0 && pxEnd != nullptr && *pxEnd == ' ')))
          continue;

        // FIXME: Again, an awkward transform for Spot Trade Pxs:
        if (!isFut)
          tradePx *= 1000.0;

        // Get the Trade Qty:
        char*  qtyEnd   = nullptr;
        long   tradeQty = strtol(pxEnd + 1, &qtyEnd, 10);
        if (utxx::unlikely
           (tradeQty <= 0 || qtyEnd == nullptr || *qtyEnd != ' '))
          continue;

        // Get the AggressorSide:
        char const* aggr = qtyEnd + 1;
        bool bidAggr     = (strncmp(aggr, "BidAggr ",  8) == 0);
        bool askAggr     = (strncmp(aggr, "SellAggr ", 9) == 0);

        // At least one of {Bid,Ask}Aggr must be set:
        if (utxx::unlikely(!(bidAggr || askAggr)))
          continue;
        assert(bidAggr == !askAggr);

        // Install the Trade data into the "RawTick":
        rt.m_pxs [bidAggr][0] = tradePx;
        rt.m_qtys[bidAggr][0] = tradeQty;
      }

      //---------------------------------------------------------------------//
      // Finally, install the "RawTick" constructed:                         //
      //---------------------------------------------------------------------//
      a_res->push_back(rt);

    } // End of the Reading Loop
  }

  //=========================================================================//
  // "MkRegrPMF":                                                            //
  //=========================================================================//
  // Layout of "a_mtx" row:
  // [ (NF * DepthF) groups of K=9 entries:
  //   0: FutAskPx (relative to Last Fut MidPx)
  //   1: Log(|FutAskPx|)
  //   2: FutAsksQty
  //   3: FutAsksQtyLog
  //   4: FutBidPx (relative to Last Fut MidPx)
  //   5: Log(|FutBidPx|)
  //   6: FutBidsQty
  //   7: FutBidsQtyLog
  //   8: Log(FutBidAskSprd)
  //
  //   (NS * DepthS) groups of K=9 entries:
  //   0: SpotAskPx (relative to Last Spot MidPx)
  //   1: Log(|SpotAskPx|)
  //   2: SpotAsksQty
  //   3: SpotAsksQtyLog
  //   4: SpotBidPx (relative to Last Spot MidPx)
  //   5: Log(|SpotBidPx|)
  //   6: SpotBidsQty
  //   7: SpotBidsQtyLog
  //   8: Log(FutBidAskSprd)
  //
  //   (1.0) for the constant term
  // ]:
  //
  pair<int, int> MkRegrPMF
  (
    InstrT                    a_targInstr,
    RegrSideT                 a_regrSide,
    int                       a_NF,
    int                       a_depthF,   // For the OrderBook ImBalances
    int                       a_NS,
    int                       a_depthS,
    int                       a_nearMS,   // Nearest  temporal boundary
    int                       a_farMS,    // Furthest temporal boundary
    double                    a_log0,     // Substitute for Log(<=0)
    vector<RawTickPMF> const& a_src,
    vector<double>*           a_mtx,      // Size will be auto-adjusted
    vector<double>*           a_rhs       // Same
  )
  {
    //-----------------------------------------------------------------------//
    // Initialisation:                                                       //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely
       (a_mtx == nullptr || a_NF     <  0 || a_NS     <  0 || a_NF+a_NS == 0 ||
        a_rhs == nullptr || a_depthF <= 0 || a_depthF > 10 || a_depthS  <= 0 ||
        a_depthS > 10    || a_nearMS <  0 || a_farMS  <= a_nearMS))
      throw utxx::badarg_error("MkRegrPMF: Invalid arg(s)");

    // The Regression Matrix (and the RHS vector) have N=(NF+NS+4) Cols. Thus
    // reserve an estimated space for both ("T" below):
    a_mtx->clear();
    a_rhs->clear();

    long const K = 9;
    long const N = K * (a_NF * a_depthF + a_NS * a_depthS) + 1;

    // The number of currently-allocated (but as yet empty) Rows in "a_mtx" and
    // "a_rhs". The matrix and vector will have row-major layout:
    long T = long(a_src.size() / 10);

    if (utxx::unlikely(T == 0))
      throw utxx::badarg_error("MkRegrPMF: Src data empty or too small");

    a_mtx->resize(size_t(N * T));
    a_rhs->resize(size_t(T));

    // The number of currently-filled Rows of "a_mtx" and "a_rhs":
    long t = 0;

    utxx::time_val  lastTS;     // Gap Detection

    //-----------------------------------------------------------------------//
    // Traverse the Src RawTicks:                                            //
    //-----------------------------------------------------------------------//
    for (long  i = 0; i < long(a_src.size()); ++i)
    {
      //---------------------------------------------------------------------//
      // Curr Src and Targ:                                                  //
      //---------------------------------------------------------------------//
      // If the curr target row to be filled is beyond the allocated targ space,
      // extend the latter:
      if (utxx::unlikely(t >= T))
      {
        T *= 2;
        a_mtx->resize(size_t(N * T));
        a_rhs->resize(size_t(T));
      }
      assert(t < T);

      // So the curr targs are:
      double* M = a_mtx->data() + (N * t);
      double* B = a_rhs->data() +  t;

      // The curr src RawTick:
      RawTickPMF const& rtI = a_src[size_t(i)];

      // Trade? Then nothing else to do here (but Trades are used in BackTrack-
      // ing):
      if (utxx::unlikely(rtI.m_isTrade))
        continue;

      //---------------------------------------------------------------------//
      // Otherwise: An OrderBook Update:                                     //
      //---------------------------------------------------------------------//
      // If it is not the Target being modelled, skip this update:
      if (rtI.m_instr != a_targInstr)
        continue;

      // Also skip any zero Target ticks:
      double  deltaI = rtI.m_pxDeltas[int(a_regrSide)];
      if (utxx::likely(deltaI == 0.0))
        continue;

      //---------------------------------------------------------------------//
      // BackTracking: Traverse the prev Srcs:                               //
      //---------------------------------------------------------------------//
      // Go back from the "curr" RawTick, within the specified temporal limits,
      // to get the NF and NS Deltas of Fut and Spot (resp) pxs:
      // Temporal limits for the "prev" ticks:
      //
      utxx::time_val  earliest =  rtI.m_ts - utxx::msecs(a_farMS );
      utxx::time_val  latest   =  rtI.m_ts - utxx::msecs(a_nearMS);
      assert(earliest < latest && latest <= rtI.m_ts);

      // Have we also filled the OrderBook ImBalances and MktPressures? They
      // are already  filled in if the corresp EMAs are used:
      int    gotNF     = 0;
      int    gotNS     = 0;
      double futMidPxLast  = NaN<double>;   // Set on first Fut  tick
      double spotMidPxLast = NaN<double>;   // Set on first Spot tick

      for (long j = i-1; j >= 0; --j)
      {
        //-------------------------------------------------------------------//
        // Checks:                                                           //
        //-------------------------------------------------------------------//
        // BackTracking done?
        if (utxx::unlikely(gotNF == a_NF && gotNS == a_NS))
          break;

        // Otherwise: Get the "historical" tick "rtJ", moving backwards:
        RawTickPMF const& rtJ = a_src[size_t(j)];

        // If "rtJ" is too close to "rtI", skip it:
        if (utxx::unlikely(rtJ.m_ts > latest))
          continue;

        // If "rtJ" is earlier than the limit, cannot go further. This can hap-
        // pen eg if there was a gap in MktData:
        if (utxx::unlikely(rtJ.m_ts < earliest))
          break;

        //-------------------------------------------------------------------//
        // Otherwise: Got a valid Prev Tick:                                 //
        //-------------------------------------------------------------------//
        if (rtJ.m_instr == InstrT::Fut)
        {
          // Futures Trade: Skip it for the moment:
          if (utxx::unlikely(rtJ.m_isTrade))
            continue;

          //-----------------------------------------------------------------//
          // Otherwise: Futures Tick:                                        //
          //-----------------------------------------------------------------//
          // If the Futures OrderBook Qtys (cumulative, to the specified depth)
          // were not recorded yet, install them now:
          //
          if (gotNF < a_NF)
          {
            if (utxx::unlikely(gotNF == 0))
              futMidPxLast = 0.5 * (rtJ.m_pxs[0][0] + rtJ.m_pxs[1][0]);
            assert(Finite(futMidPxLast));

            int off    = K * a_depthF * gotNF;
            for (int l = 0; l < a_depthF; ++l)
            {
              double askPx   = rtJ.m_pxs[0][l] - futMidPxLast;
              double asksQty = double(rtJ.m_qtys[0][l]);
              double bidPx   = rtJ.m_pxs[1][l] - futMidPxLast;
              double bidsQty = double(rtJ.m_qtys[1][l]);
              assert(asksQty > 0 && bidsQty > 0 && askPx > bidPx);

              // Now compute the Avg Bid and Ask Pxs for this level:
              M[off++] = askPx;
              M[off++] = LogTr(Abs(askPx),    a_log0);
              M[off++] = asksQty / 1000.0;    // For uniformity
              M[off++] = LogTr(asksQty,       a_log0);
              M[off++] = bidPx;
              M[off++] = LogTr(Abs(bidPx),    a_log0);
              M[off++] = bidsQty / 1000.0;    // For uniformity
              M[off++] = LogTr(bidsQty,       a_log0);
              M[off++] = LogTr(askPx - bidPx, a_log0);
            }
            ++gotNF;
          }
        }
        else
        {
          // Spot Trade: Skip it for the moment:
          if (utxx::unlikely(rtJ.m_isTrade))
            continue;

          //-----------------------------------------------------------------//
          // Otherwise: Spot Tick:                                           //
          //-----------------------------------------------------------------//
          // If the Spot OrderBook Qtys (cumulative, to the specified depth)
          // were not recorded yet, install them now:
          if (gotNS < a_NS)
          {
            if (utxx::unlikely(gotNS == 0))
              spotMidPxLast = 0.5 * (rtJ.m_pxs[0][0] + rtJ.m_pxs[1][0]);
            assert(Finite(spotMidPxLast));

            int off    = K * (a_depthF * a_NF + a_depthS * gotNS);
            for (int l = 0; l < a_depthS; ++l)
            {
              double askPx   = rtJ.m_pxs[0][l] - spotMidPxLast;
              double asksQty = double(rtJ.m_qtys[0][l]);
              double bidPx   = rtJ.m_pxs[1][l] - spotMidPxLast;
              double bidsQty = double(rtJ.m_qtys[1][l]);
              assert(asksQty > 0 && bidsQty > 0 && askPx > bidPx);

              M[off++] = askPx;
              M[off++] = LogTr(Abs(askPx),    a_log0);
              M[off++] = asksQty / 1000.0;    // For uniformity
              M[off++] = LogTr(asksQty,       a_log0);
              M[off++] = bidPx;
              M[off++] = LogTr(Abs(bidPx),    a_log0);
              M[off++] = bidsQty / 1000.0;    // For uniformity
              M[off++] = LogTr(bidsQty,       a_log0);
              M[off++] = LogTr(askPx - bidPx, a_log0);
            }
            ++gotNS;
          }
        }
      }
      //---------------------------------------------------------------------//
      // BackTracking Done; Finally:                                         //
      //---------------------------------------------------------------------//
      if (utxx::likely(gotNF == a_NF && gotNS == a_NS))
      {
        // For the const term:
        M[N-1] = 1.0;

        // This row has been filled. Install the RHS:
        assert(a_targInstr == rtI.m_instr);
        *B = deltaI;

        // Can now go to the next Target:
        ++t;
      }
      // Otherwise, we advance the Src but stay put in the Target!
    }
    // Src loop done

    //-----------------------------------------------------------------------//
    // Post-Processing:                                                      //
    //-----------------------------------------------------------------------//
    assert(t <= T);
    T = t;
    a_mtx->resize(size_t(N * T));
    a_rhs->resize(size_t(T));

    return make_pair(T, N);
  }

  //=========================================================================//
  // "SolveRegrLSM":                                                         //
  //=========================================================================//
  // Upon completion, the solution is in the (duly resized) "a_rhs"; "a_mtx" is
  // over-written with intermediate data:
  //
  void SolveRegrLSM
  (
    vector<double>* a_mtx,
    vector<double>* a_rhs
  )
  {
    //-----------------------------------------------------------------------//
    // Get the problem geometry:                                             //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely
       (a_mtx == nullptr || a_rhs == nullptr || a_mtx->empty() ||
        a_rhs->empty()))
      throw utxx::badarg_error("SolveRegrLSM: Empty/NULL arg(s)");

    // The number of Rows is the length of the RHS (large):
    long nRows = long(a_rhs->size());

    // Then compte the number of Rows:
    if (utxx::unlikely(long(a_mtx->size()) % nRows) != 0)
      throw utxx::badarg_error
            ("SolveRegrLSM: Incompatible geometry of Mtx and RHS");

    long nCols = long(a_mtx->size()) / nRows;
    assert(1 <= nCols);

    if (utxx::unlikely(nRows <= nCols))
      throw utxx::badarg_error("SolveRegrLSM: Too little data");

    //-----------------------------------------------------------------------//
    // Invoke the Intel MKL Driver:                                          //
    //-----------------------------------------------------------------------//
    int res = LAPACKE_dgels
    (
      LAPACK_ROW_MAJOR, 'N', nRows,  nCols, 1,
      a_mtx->data(),  nCols, a_rhs->data(), 1
    );

    if (utxx::unlikely(res != 0))
      throw utxx::runtime_error("SolveRegrLSM: RC=", res);

    // XXX: Resize the RHS to contain the solution only:
    a_rhs->resize(nCols);
  }

  //=========================================================================//
  // "OutputRegr":                                                           //
  //=========================================================================//
  void OutputRegr
  (
    vector<double> const& a_mtx,
    vector<double> const& a_rhs,
    char const*           a_fileName
  )
  {
    // Check the dimensions:
    long nAll  = long(a_mtx.size());
    long nRows = long(a_rhs.size());

    if (utxx::unlikely
       (nAll == 0 || nRows == 0 || nAll % nRows != 0))
      throw utxx::badarg_error("OutputRegr: Incompatible geometries");
    long nCols = nAll / nRows;

    // Open the file:
    if (utxx::unlikely(a_fileName == nullptr || *a_fileName == '\0'))
      throw utxx::badarg_error("OutputRegr: No output file");

    FILE* f = fopen(a_fileName, "w");
    if (utxx::unlikely(f == nullptr))
      throw utxx::badarg_error
            ("OutputRegr: Cannot open the opuput file: ", a_fileName);

    // Proceed with Output:
    double const* M = a_mtx.data(); // In a Row-Major Format
    double const* B = a_rhs.data();

    for (int r = 0; r < nRows; ++r)
    {
      for (int c = 0; c < nCols; ++c)
        fprintf(f, "%f ", *(M++));
      fprintf(f, "%f\n",  *(B++));
    }
    // All Done:
    fclose(f);
  }

  //=========================================================================//
  // "EvalRegr":                                                             //
  //=========================================================================//
  // Returns the rate of successful directional matches:
  //
  double EvalRegr
  (
    vector<double> const& a_mtx,
    vector<double> const& a_rhs,
    vector<double> const& a_coeffs,
    vector<double>*       a_buff,
    RegrSideT             a_regrSide
  )
  {
    //-----------------------------------------------------------------------//
    // Check the dimensions:                                                 //
    //-----------------------------------------------------------------------//
    long nRows = long(a_rhs   .size());
    long nCols = long(a_coeffs.size());

    if (utxx::unlikely
       (nRows == 0 || nCols == 0 || a_buff == nullptr ||
        long(a_mtx.size())  != nRows * nCols))
      throw utxx::badarg_error("EvalRegr1: Incompatible geometries");

    // Buff must be of the same size as RHS:
    a_buff->resize(nRows);

    //-----------------------------------------------------------------------//
    // Perform evaluation (results -> Buff):                                 //
    //-----------------------------------------------------------------------//
    cblas_dgemv
    (
      CblasRowMajor, CblasNoTrans,   nRows, nCols,    1.0,
      a_mtx.data(),  nCols,          a_coeffs.data(), 1,
      0.0,           a_buff->data(), 1
    );

    //-----------------------------------------------------------------------//
    // The Rate of Successful Predictions:                                   //
    //-----------------------------------------------------------------------//
    double const* Preds = a_buff->data();
    double const* Acts  = a_rhs  .data();

    int p = 0;
    for (long i = 0; i < nRows; ++i)
    {
      double pred = Preds[i];
      double act  = Acts [i];

      switch (a_regrSide)
      {
      case RegrSideT::Bid:
        // Relaxed error counting:
        // Prediction error occurs when the actual Bid goes down (ie there was
        // Hit) but we did not predict it; in all other cases, it's OK:
        if (act >= 0.0 || (act < 0.0 && pred < 0.0))
          ++p;
        break;
      case RegrSideT::Ask:
        // Relaxed error counting:
        // Similarly, prediction error occurs when the actual Ask goes up  (ie
        // there was a Lift) but we did not predict it; in all other cases, OK:
        if (act <= 0.0 || (act > 0.0 && pred > 0.0))
          ++p;
        break;
      default:
        // Generic Case: Require exact directional match:
        if (pred * act > 0.0)
          ++p;
      }
    }
    return double(p) / double(nRows);
  }

  //=========================================================================//
  // "OutputSolutionPMF":                                                    //
  //=========================================================================//
  void OutputSolutionPMF
  (
    int                   a_NF,
    int                   a_depthF,
    int                   a_NS,
    int                   a_depthS,
    vector<double> const& a_res,
    char const*           a_outFile
  )
  {
    // XXX: The following consts must be same as in "MkRegrPMF":
    int const K = 9;
    int const N = K * (a_NF * a_depthF + a_NS * a_depthS) + 1;

    if (utxx::unlikely(long(a_res.size()) != N))
      throw utxx::badarg_error
            ("OutputSolutionPMF: Invalid Solution Sise: ", a_res.size(),
             ", Expected: ", N);

    // If OK: Open the Output File:
    if (utxx::unlikely(a_outFile == nullptr || *a_outFile == '\0'))
      throw utxx::badarg_error("OutputSolutionPMF: Empty Output FileName");

    FILE* f = fopen(a_outFile, "w");
    if (utxx::unlikely(f == nullptr))
      throw utxx::badarg_error
            ("OutputSolutionPMF: Cannot open Output File: ", a_outFile);

    // If OK: Proceed with Output:
    int off = 0;
    for (int FS = 0; FS < 2; ++FS)
    {
      int  N           = (FS == 0) ? a_NF      : a_NS;
      int  L           = (FS == 0) ? a_depthF  : a_depthS;
      char const* what = (FS == 0) ? "Futures" : "Spot";

      for (int i = 0; i < N; ++i)
      {
        fprintf(f, "# %s: HistDepth=%d:\n", what, i);
        for (int l = 0; l < L; ++l)
        {
          for (int k = 0; k < K; ++k)
          {
            fprintf(f, "%f%c", a_res[off++],
                    utxx::unlikely(k == K-1) ? '\n' : '\t');
          }
        }
      }
    }
    // Finally, the Free Term:
    fputs  ("# FreeTerm:\n", f);
    fprintf(f, "%f\n", a_res[off++]);

    // All Done:
    assert(off == N);
    fclose(f);
  }
} // End namespace Predictors
} // End namespace MAQUETTE
