// vm:ts=2:et
//===========================================================================//
//                               "Predictors.h":                             //
//        Data Formats and Processing Funcs used in Predictors               //
//===========================================================================//
#pragma once
#include "Basis/Maths.hpp"
#include <utxx/time_val.hpp>
#include <utxx/config.h>
#include <utxx/enumx.hpp>
#include <utility>

namespace MAQUETTE
{
namespace Predictors
{
  //=========================================================================//
  // Enums:                                                                  //
  //=========================================================================//
  // "RegrSideT": Which Side(s) are used in Regression:
  //
  UTXX_ENUMX(RegrSideT, int, -1,
    (Ask, 0)    // Ask Only
    (Bid, 1)    // Bid Only
    (Mid, 2)    // Mid-Point
  );

  // "InstrT": Instrument:
  //
  UTXX_ENUMX(InstrT,    int, -1,
    (Spot0, 0)  // TOD Spot
    (Spot1, 1)  // TOM Spot
    (Fut,   2)  // Futures
  );

  //=========================================================================//
  // "RawTickPMF":                                                           //
  //=========================================================================//
  // (From the MktData recorded by the "Pairs-MF" strategy):
  //
  struct RawTickPMF
  {
    //-----------------------------------------------------------------------//
    // Data Flds:                                                            //
    //-----------------------------------------------------------------------//
    // Time Stamp and Trade Flag (normally, it is OrderBook(s) SnapShot):
    utxx::time_val  m_ts;
    InstrT          m_instr;            // Futures  or Spot ?
    bool            m_isTrade;          // Trade or OrderBook update?

    // OrderBook Qtys in Contracts (Lots). Or if it is a Trade, the Aggressor
    // Side Size is given by [Aggr][0], and all others are 0s:
    long            m_qtys    [2][10];  // 0: Ask; 1: Bid

    // The corresp Pxs: for an OrderBook update, they are most likely contigu-
    // ous. For a Trade, [Aggr][0] is the Trade Px, all others are NaN:
    double          m_pxs     [2][10];  // Same

    // L1 Px Deltas for Ask(0), Bid(1) and Mid(2), compared to the prev RawTick
    // of the same Kind and Instrument:
    double          m_pxDeltas[3];

    // When the curr L1 Ask, Bid and Mid pxs were set?
    utxx::time_val  m_since   [3];

    // For how many "RawTick"s (incl the curr one) the curr L1 Ask, Bid and Mid
    // pxs were valid?
    int             m_validFor[3];

    //-----------------------------------------------------------------------//
    // Default Ctor:
    //-----------------------------------------------------------------------//
    RawTickPMF()
    : m_ts(),
      m_instr  (InstrT::UNDEFINED),
      m_isTrade(false)
    {
      for (int s = 0; s <  2; ++s)
      for (int l = 0; l < 10; ++l)
      {
        m_qtys[s][l]  = 0;
        m_pxs [s][l]  = NaN<double>;
      }
      for (int s = 0; s <  3; ++s)
      {
        m_pxDeltas[s] = 0.0;
        m_since   [s] = utxx::time_val();
        m_validFor[s] = 0;
      }
    }
  };

  //=========================================================================//
  // "MkRawTicksPMF":                                                        //
  //=========================================================================//
  // Reads an MQT MktData file (XXX: currently for MOEX only), fills in a vector
  // of "RawTick"s. If the Res vector is already non-empty, it is NOT cleared;
  // the new data are appended to it.
  // 1st Instr is notionally a "Futures" (although it can in fact be any  MOEX
  // Instr), 2nd Instr is notionally a "Spot":
  //
  void MkRawTicksPMF
  (
    char const*               a_file_name,
    char const*               a_fut,
    char const*               a_spot,
    std::vector<RawTickPMF>*  a_res
  );

  //=========================================================================//
  // "MkRegrPMF":                                                            //
  //=========================================================================//
  // Constructing the Regression Matrix and the RHS, later used  in  Regression
  // Analysis, from the Raw Ticks, to predict the Px movements of the specified
  // Target and Side. The params and the matrix layout is specified in the impl-
  // ementation part.
  // Returns (NRows, NCols) of the resulting Matrix (Row-Major Layout):
  //
  std::pair<int,int> MkRegrPMF
  (
    InstrT                          a_targInstr,
    RegrSideT                       a_regrSide,
    int                             a_NF,       // For Prev Ticks
    int                             a_depthF,   // For the OrderBook ImBalances
    int                             a_NS,       // Similar for Spot
    int                             a_depthS,
    int                             a_nearMS,   // Nearest  temporal boundary
    int                             a_farMS,    // Furthest temporal boundary
    double                          a_log0,     // Substitute for Log(<=0)
    std::vector<RawTickPMF> const&  a_src,
    std::vector<double>*            a_mtx,      // Size will be auto-adjusted
    std::vector<double>*            a_rhs       // Same
  );

  //=========================================================================//
  // "OutputRegr":                                                           //
  //=========================================================================//
  // Outouts the Regression Matrix and the RHS Vector into the specified file.
  // This function should be agnostic as to how the data were constructed:
  //
  void OutputRegr
  (
    std::vector<double> const& a_mtx,
    std::vector<double> const& a_rhs,
    char const*                a_fileName   // Must be non-NULL, non-empty
  );

  //=========================================================================//
  // "SolveRegrLSM":                                                         //
  //=========================================================================//
  // Solving the Regression Model by the Least Squares Method. On successful
  // completion, "a_rhs" contains the solution. Throws an exception in a num-
  // erical error (eg a singularity) was encountered:
  //
  void SolveRegrLSM
  (
    std::vector<double>* a_mtx,
    std::vector<double>* a_rhs
  );

  //=========================================================================//
  // "EvalRegr":                                                             //
  //=========================================================================//
  // Returns the rate of successful directional matches. If RegrSide is speci-
  // died to be "Bid" or "Ask", then the Relaxed mode is used while evaluating
  // predictior errors (thus producing a higher success rate compared to the
  // normal mode):
  //
  double EvalRegr
  (
    std::vector<double> const&  a_mtx,
    std::vector<double> const&  a_rhs,
    std::vector<double> const&  a_coeffs,
    std::vector<double>*        a_buff,      // Auto-resized
    RegrSideT                   a_regrSide = RegrSideT::UNDEFINED
  );

  //=========================================================================//
  // "OutputSolutionPMF":                                                    //
  //=========================================================================//
  // XXX: The args must be compatible with those of "MkRegrPMF":
  //
  void OutputSolutionPMF
  (
    int                         a_NF,
    int                         a_depthF,
    int                         a_NS,
    int                         a_depthS,
    std::vector<double> const&  a_res,
    char const*                 a_outFile
  );

  //=========================================================================//
  // "IntervalData":                                                         //
  //=========================================================================//
  struct IntervalData
  {
    //-----------------------------------------------------------------------//
    // Data Flds:                                                            //
    //-----------------------------------------------------------------------//
    utxx::time_val  m_start;

    // Futures Pxs: XXX: "Open" and "Close" are not much useful because they
    // refer to (quite arbitrary, in fact) interval boundaries. The Pxs  are
    // for one Side (specified in the construction func),  and are expressed
    // as DELTAs to the prev Interval -- ie abs pxs are NOT used:
    double          m_minFutDelta;
    double          m_maxFutDelta;
    double          m_avgFutDelta;   // Average over Time

    // Spot Pxs: Similar:
    double          m_minSpotDelta;
    double          m_maxSpotDelta;
    double          m_avgSpotDelta;

    // Futures and Spot Cumulative OrderBook Qtys, by the Depth (1..10), avera-
    // ged over the Interval. NB: We also provide Logs of those qtys to facili-
    // tate multiplicative transforms: 0=Ask, 1=Bid:
    double          m_cumFutQtys   [2][10];
    double          m_cumFutLogs   [2][10];
    double          m_cumSpotQtys  [2][10];
    double          m_cumSpotLogs  [2][10];

    // Market Pressure during this Interval: Again, using BOTH absolute Trade
    // Qtys and their Logs: 0=AskAggr, 1=BidAggr:
    double          m_tradeFutQtys [2];
    double          m_tradeFutLogs [2];
    double          m_tradeSpotQtys[2];
    double          m_tradeSpotLogs[2];

    // The current "trend" as determinrd by a sequence of Ask Maxes and Bid
    // Mins; >0: Up, 0: UnDetermined, <0: Down. If this qty is non-0, it is the
    // distance of the curr px from the prev Minimum (for Up-trend) or Maximum
    // (for Down-trend):
    double          m_trendFut;
    double          m_trendSpot;

    //-----------------------------------------------------------------------//
    // Default Ctor:                                                         //
    //-----------------------------------------------------------------------//
    IntervalData();
  };

  //=========================================================================//
  // "IntervalisePMF":                                                       //
  //=========================================================================//
  // Constructs INTERVALISED data set suitable for linear regression or classi-
  // fication analysis, or in various AI learning methods (SVMs,  DNNs,  etc),
  // from a vector of "RawTickPMF" data.
  // RHS vals:
  // +1 if the MaxPx went over the EvalThres within the next EvalMS, but MinPx
  //    did NOT go below (-EvalThresh) within that time;
  // -1 if other way round;
  //  0 in all other cases (both or neither pxs crossed the EvalThresh):
  //
  void IntervalisePMF
  (
    InstrT                          a_targInstr,
    RegrSideT                       a_regrSide,
    int                             a_intervalMS,  // For intervalisation
    double                          a_trendResist, // For robust trend detection
    double                          a_log0,        // Replacement for Log(0)
    std::vector<RawTickPMF> const&  a_src,
    std::vector<IntervalData>*      a_mtx,
    std::vector<double>*            a_rhs
  );
} // End namespace Predictors
} // End namespace MAQUETTE
