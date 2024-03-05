// vim:ts=2:et
//===========================================================================//
//                       "Tools/OptExecProfilePost.cpp":                     //
//            A-Posteriori Optimisation of Order Execution Profile           //
//===========================================================================//
#include "Basis/Maths.hpp"
#include "QuantSupport/MDSaver.hpp"
#include <utxx/compiler_hints.hpp>
#include <iostream>
#include <utility>
#include <algorithm>
// XXX: CLang support for OpenMP is still problematic:
#ifndef __clang__
#include <omp.h>
#endif

using namespace MAQUETTE;
using namespace MAQUETTE::QuantSupport;
using namespace std;
using OSB     = MDOrdersBook<DefaultODepth>;

namespace
{
  //=========================================================================//
  // Utils:                                                                  //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "Usage":                                                                //
  //-------------------------------------------------------------------------//
  inline void Usage(char const* msg)
  {
    if (msg != nullptr)
      cerr << msg << endl;

    cerr << "PARAMETERS:\n"
            "\t[-h]     (Help)\n"
            "\t[-T MaxExecTime_Seconds] (default:       600)\n"
            "\t[-N Notional]            (default: 2'000'000)\n"
            "\t[-E ExecChunks]          (default:        20)\n"
            "\t[-b BetaFrom]            (default:      0.05)\n"
            "\t[-B BetaTo]              (default:      20.0)\n"
            "\t[-K BetaIntervals]       (default:        40)\n"
            "\t[-j MaxOMPThreads]       (default:         8)\n"
            "\t[-n] (No impact modeling, default:     false)\n"
            "\tOrdersLogBinFile"
         << endl;
  }

  //=========================================================================//
  // "MkSnapShotInds":                                                       //
  //=========================================================================//
  // Fills in "a_inds", returns the following flag:
  //
  enum class IndsResT: int
  {
    OK   = 0,
    Skip = 1,
    Done = 2
  };

  IndsResT MkSnapShotInds
  (
    OSB const* a_recs,      // Non-NULL, of "a_n_recs"   length
    long       a_n_recs,
    long       a_i,         // Curr idx
    long*      a_inds,      // Non-NULL, of "a_n_chunks" length
    int        a_n_chunks,
    int        a_Tsec       // Exec Windows in seconds
  )
  {
    assert(a_recs != nullptr && a_n_recs   > 0 && 0 <= a_i && a_i < a_n_recs &&
           a_inds != nullptr && a_n_chunks > 0 && a_Tsec > 0);

    // Frame the curr exec window (typically, a linear search is OK):
    // [i..j] => [t0..tN]:
    // XXX: At the moment, we assume that there is only 1 Symbol in "recs",
    // so for the sake of efficiency, do not perform any filtering wrt it:
    //
    utxx::time_val t0 = a_recs[a_i].m_ts;
    utxx::time_val tN = t0 + utxx::secs(a_Tsec);

    long j   = a_i+1;
    for (; j < a_n_recs && a_recs[j].m_ts < tN; ++j);

    // If we have reached the end of "a_recs" before reaching "tN", we are done:
    if (utxx::unlikely(j == a_n_recs))
      return IndsResT::Done;

    // Generic Case: MktData Chunk is [a_i..j] inclusive:
    long   len  = j-a_i+1;
    assert(len >= 2);

    // If there are too few MktData (say less ticks than ExecChunks), we skip
    // this point:
    if (utxx::unlikely(len < a_n_chunks))
      return IndsResT::Skip;

    //-----------------------------------------------------------------------//
    // Generic Case: Fill in the "a_inds":                                   //
    //-----------------------------------------------------------------------//
    long l = 0;
    for (int c = 0; c < a_n_chunks; ++c)
    {
      if (utxx::unlikely(l >= len))
        // Cannot construct the "a_inds" -- too few points perhaps, so got to
        // the end of data: Cannot do anything:
        return IndsResT::Skip;

      a_inds[c]  =  a_i + l;
      assert(a_i <= a_inds[c]  &&  a_inds[c] <= j  &&
            (c == 0 || a_inds[c] > a_inds[c-1]));

      // Now advance "l" to the time corresp to the next SnapShot point (XXX:
      // linearly in indices, not in time  -- but over short intervals, that
      // should be almost the same):
      //
      long lNext = long(Ceil(double(len)  *
                             double(c+1)) / double(a_n_chunks));
      assert(l <= lNext && lNext <= len);

      // "l" must be strictly increasing -- during the next iteration, we will
      // check its bounds:
      l = utxx::likely(lNext > l) ? lNext : (l+1);
    }
    // All Done:
    return IndsResT::OK;
  }

  //=========================================================================//
  // "EvalExecProfile":                                                      //
  //=========================================================================//
  // Creates and evaluates the ExecProfile for the given params. Returns a pair
  // [VWAP(Buy), VWAP(Sell)]:
  // NB:
  // (*) In the Propagate mode,  "a_books" is just an array of SnapShots, of
  //     length "a_n_chunks", and "a_inds" are NULL;
  // (*) In the Non-Propagate mode, "a_inds" are an array of "a_n_chunks" len
  //     containing indices in "a_books":
  //
  pair<double, double> EvalExecProfile
  (
    int             a_n_chunks,
    double const*   a_chunk_sizes,
    OSB    const*   a_books,
    long   const*   a_inds  // May be NULL
  )
  {
    assert(a_n_chunks > 0 && a_chunk_sizes != nullptr && a_books != nullptr);

    bool   propagate    = (a_inds == nullptr);
    double totalBuyPx   = 0.0;
    double totalBuyQty  = 0.0;
    double totalSellPx  = 0.0;
    double totalSellQty = 0.0;

    //-----------------------------------------------------------------------//
    // Simulate execution of all Order Chunks:                               //
    //-----------------------------------------------------------------------//
    for (int c = 0; c < a_n_chunks; ++c)
    {
      //---------------------------------------------------------------------//
      // Simulate Buy and Sell orders of "chunkSz" at "c":                   //
      //---------------------------------------------------------------------//
      double chunkSz    = a_chunk_sizes[c];
      double remQtyBuy  = chunkSz;
      double remQtySell = chunkSz;

      OSB const& curr =
        (a_inds == nullptr)
        ? a_books[c]          // Propagation Mode:   "a_books" is a buffer
        : a_books[a_inds[c]]; // No-Propagation: Use "a_books" at given offsets

      // NB: "b" is OUR aggressive side: b==1 is Buy, b==0 is Sell:
      for (int b = 0; b < 2; ++b)
      {
        // Ref: The passive side (against which we aggress): opposite to OURS:
        MDOrdersBookEntry const (&pass)[DefaultODepth] =
          b ? curr.m_asks : curr.m_bids;

        // Ref: OUR remaining qty to fill:
        double& remQty   = b ? remQtyBuy   : remQtySell;
        assert (remQty > 0.0);

        // Refs to Accumulators:
        double& totalQty = b ? totalBuyQty : totalSellQty;
        double& totalPx  = b ? totalBuyPx  : totalSellPx;

        // Traverse the "pass" side and match the Orders there:
        for (int d = 0; d < DefaultODepth && remQty > 0.0; ++d)
        {
          MDOrdersBookEntry const& entry = pass[d];
          assert(entry.m_px      >  0.0  && entry.m_qty >= 0.0 &&
                 entry.m_orderID != 0);

          if (entry.m_qty == 0.0)
            // This Order has already been taken by a previous Chunk:
            continue;

          // Otherwise: We got a Match:
          double matchQty = Min(remQty, entry.m_qty);
          remQty      -= matchQty;
          totalQty    += matchQty;
          totalPx     += matchQty * entry.m_px;

          // XXX: In the Propagate mode, remove "matchQty" from this OrderID in
          // all subsequent SnapShots ("poor man's impact simulation"):
          if (propagate)
          {
            const_cast<MDOrdersBookEntry&>(entry).m_qty -= matchQty;

            for (long m = c+1; m < a_n_chunks; ++m)
            {
              MDOrdersBookEntry (&subs)[DefaultODepth] =
                const_cast<MDOrdersBookEntry (&)[DefaultODepth]>
                (b ? a_books[m].m_asks : a_books[m].m_bids);

              for (int e = 0; e < DefaultODepth; ++e)
                if (subs[e].m_orderID == entry.m_orderID)
                  subs[e].m_qty = Max(subs[e].m_qty - matchQty, 0.0);
            }
          }
        }
        // XXX: If we have exhaused the whole Depth and could not fill our ord-
        // er, return an error:
        if (utxx::unlikely(remQty != 0.0))
          return make_pair(NaN<double>, NaN<double>);
      }
    }
    //-----------------------------------------------------------------------//
    // The Result:                                                           //
    //-----------------------------------------------------------------------//
    // NB: Due to rounding errors, the actual Qtys filled may be different from
    // the Notional:
    assert(totalBuyPx  > 0.0 && totalBuyQty  > 0.0 &&
           totalSellPx > 0.0 && totalSellQty > 0.0);
    return make_pair(totalBuyPx / totalBuyQty, totalSellPx / totalSellQty);
  }
}

//===========================================================================//
// "main":                                                                   //
//===========================================================================//
int main(int argc, char* argv[])
{
  try
  {
    //-----------------------------------------------------------------------//
    // Get the CmdL Params:                                                  //
    //-----------------------------------------------------------------------//
    int         Tsec          = 600;          // MaxExecTime, in seconds
    double      Notional      = 2'000'000.0;  // Flipping a 1m position
    int         ExecChunks    = 20;
    double      LogBetaFrom   = Log(0.05);
    double      LogBetaTo     = Log(20.0);
    int         KBetaInts     = 40;
    bool        Propagate     = true;
    int         MaxOMPThreads = 8;

    while (true)
    {
      int c = getopt(argc, argv, "N:T:E:b:B:K:j:n");
      if (c < 0)
        break;

      switch (c)
      {
      case 'T':
        Tsec          = atoi(optarg);
        break;
      case 'N':
        Notional      = atof(optarg);
        break;
      case 'E':
        ExecChunks    = atoi(optarg);
        break;
      case 'b':
        LogBetaFrom   = Log(atof(optarg));
        break;
      case 'B':
        LogBetaTo     = Log(atof(optarg));
        break;
      case 'K':
        KBetaInts     = atoi(optarg);
        break;
      case 'j':
        // XXX: It has no real effect if OpenMP is not enabled
        MaxOMPThreads = atoi(optarg);
        break;
      case 'n':
        Propagate     = false;
        break;
      default:
        Usage("ERROR: Invalid option");
        return 1;
      }
    }
    if (optind != argc-1)
    {
      Usage("ERROR: OrdersLogBinFile is missing");
      return 1;
    }
    char const* osbFile = argv[argc-1];

    // Check the params:
    if (utxx::unlikely
       (Tsec       <= 0 || Notional <= 0.0 || *osbFile == '\0' ||
        ExecChunks <= 0 || !(LogBetaFrom   <= LogBetaTo)       ||
        KBetaInts  <= 0 || MaxOMPThreads   <= 0))
    {
      Usage("ERROR: Invalid param(s)");
      return 1;
    }

    //-----------------------------------------------------------------------//
    // Betas and Chunk Sizes:                                                //
    //-----------------------------------------------------------------------//
    // Construct the range of "beta" vals:
    double betas[KBetaInts + 1];
    double lbStep = (LogBetaTo - LogBetaFrom) / double(KBetaInts);

    // Chunks Sizes for all "beta"s:
    double chunkSizes[KBetaInts + 1][ExecChunks];

    // Fill them in:
    for (int k = 0; k <= KBetaInts; ++k)
    {
      double beta      = Exp(LogBetaFrom + double(k)  * lbStep);
      betas[k]         = beta;

      double prevFracB = 0.0;
      for (int c = 0; c < ExecChunks; ++c)
      {
        double fracB     = Pow(double(c+1) / double(ExecChunks), beta);
        chunkSizes[k][c] = Notional * (fracB - prevFracB);
        assert(chunkSizes[k][c] > 0.0);
        prevFracB        = fracB;
      }
    }
    // Mutable Buffers of OrdersBook chunks, for per-beta processing (which may
    // be destructive of MktData) -- filled in later, only in Propagate Mode:
    OSB mdBuffs[KBetaInts + 1][ExecChunks];

    // In Non-Propagate Mode, we use an array of indices in the original (immu-
    // table) MktData for VWAP evaluation:
    long inds[ExecChunks];

    //-----------------------------------------------------------------------//
    // MMap the OrdersLogFile:                                               //
    //-----------------------------------------------------------------------//
    // RO mode (IsRW=false):
    IO::MMapedFile<OSB, false> mmf(osbFile);

    // Get access to the "MDOrdersBook" records:
    long       nTicks = mmf.GetNRecs();
    OSB const* recs   = mmf.GetPtr();
    assert    (recs  != nullptr);

#   ifndef __clang__
    // Restrict the number of threads to the maximum specified:
    if (omp_get_max_threads() > MaxOMPThreads)
      omp_set_num_threads(MaxOMPThreads);
#   endif

    //-----------------------------------------------------------------------//
    // Traverse the File:                                                    //
    //-----------------------------------------------------------------------//
    // For each OrdersBook tick, determine the optimal "beta" exec shape:
    //
    for (long i = 0; i < nTicks; ++i)
    {
      //---------------------------------------------------------------------//
      // Prepare the MktData:                                                //
      //---------------------------------------------------------------------//
      // Prepare the array of SnapShot indices to be used in  VWAP evaluation.
      // We need it in both Propagate and Non-Propagate modes;  in the former
      // case, it is used to fill in "mdBuffs", and in the latter case, it is
      // passed to "EvalExecProfile" explicitly. If, for any reason, we cannot
      // construct the indices, skip this point:
      //
      IndsResT res = MkSnapShotInds(recs, nTicks, i, inds, ExecChunks, Tsec);

      if (utxx::unlikely(res == IndsResT::Skip))
        continue;
      if (utxx::unlikely(res == IndsResT::Done))
        break;
      assert(res == IndsResT::OK);  // Got the "inds"!

      // XXX: In the "Propagate" mode, for each "beta", VWAP evaluation is dest-
      // ructive of MktData on the corresp side, so copy the MktData chunks into
      // the per-beta vectors. It is a rather expensive operation:
      if (Propagate)
        for (int k = 0; k <= KBetaInts; ++k)
        {
          OSB* mdb = &(mdBuffs[k][0]);

          // Do not copy the whole MktData chunk into "mdb" -- we only need the
          // SnapShots which will be used for VWAP evaluation, as marked by
          // "inds":
          for (int c = 0; c < ExecChunks; ++c)
          {
            long  ind = inds[c];
            assert(0 <= ind && ind < nTicks);
            mdb[c] = recs[ind];
          }
        }
      //---------------------------------------------------------------------//
      // Optimise the VWAPs over the range of "beta"s:                       //
      //---------------------------------------------------------------------//
      // Buy : Minimise VWAP;
      // Sell: Maximise VWAP:
      double optBuyVWAP  = + Inf<double>;
      double optSellVWAP = - Inf<double>;
      double optBuyBeta  =   NaN<double>;
      double optSellBeta =   NaN<double>;

#     ifndef __clang__
#     pragma omp parallel for
#     endif
      for (int k = 0; k <= KBetaInts; ++k)
      {
        double beta     = betas[k];
        auto   vwaps    =
          EvalExecProfile
          (
            ExecChunks,
            &(chunkSizes[k][0]),
            // In the Propagate mode, use the corresp "mdBuff":
            Propagate  ? &(mdBuffs[k][0]) : recs,
            // In Non-Propagate mode, use "inds" instead:
            Propagate  ? nullptr                    : inds
          );
        double buyVWAP  = vwaps.first;
        double sellVWAP = vwaps.second;

        // Got some results. NB: The following conds will only work if the resp
        // result is valid (not NaN):
#       ifndef __clang__
#       pragma omp critical
#       endif
        {
          if (buyVWAP  < optBuyVWAP)
          {
            optBuyVWAP = buyVWAP;
            optBuyBeta = beta;
          }
          if (sellVWAP > optSellVWAP)
          {
            optSellVWAP = sellVWAP;
            optSellBeta = beta;
          }
        }
      }
      // For comparison, compute the VWAPs of immediate aggressive execution
      // for the full Notional (IsStrict=true):
      double aggrBuyVWAP  = VWAP<true>(recs[i].m_asks, Notional);
      double aggrSellVWAP = VWAP<true>(recs[i].m_bids, Notional);
      double bestAskPx    = recs[i].m_asks [0].m_px;
      double bestBidPx    = recs[i].m_bids [0].m_px;

      // Optimisation at "i" has been done: Output the results:
      cout << i     << ' ' << recs[i].m_ts.milliseconds()
           << " B " << bestAskPx   << ' ' << aggrBuyVWAP  << ' ' << optBuyVWAP
           << ' '   << optBuyBeta  << ' '
           << " S " << bestBidPx   << ' ' << aggrSellVWAP << ' ' << optSellVWAP
           << ' '   << optSellBeta << ' '
           << endl;
      cerr << '\r' << (100.0 * double(i) / double(nTicks)) << " %";
    }
    // All Done:
    return 0;
  }
  catch (exception const& exn)
  {
    cerr << "EXCEPTION: " << exn.what() << endl;
    return 1;
  }
  __builtin_unreachable();
}
