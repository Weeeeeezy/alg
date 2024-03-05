// vim:ts=2:et
//===========================================================================//
//                            "MDReader_HotSpotFX.h":                        //
//             Reader for Historical (CSV) MktData for HotSpotFX             //
//===========================================================================//
#pragma once
#include "QuantSupport/MDReader.hpp"
#include "QuantSupport/HistoGram.hpp"
#include <map>

namespace MAQUETTE
{
namespace QuantSupport
{
  //=========================================================================//
  // "MDReader_HotSpotFX" Class:                                             //
  //=========================================================================//
  class MDReader_HotSpotFX final: public MDReader<MDReader_HotSpotFX>
  {
  public:
    //=======================================================================//
    // Client Call-Back Types:                                               //
    //=======================================================================//
    // "OnOBUpdateCBT":
    // Call-Back to be invoked on OrderBook Updates: Args:
    //  (OrderBook, UpdateID, TimeStamp):
    //
    using OnOBUpdateCBT =
      std::function<void(OrderBook const&, long, utxx::time_val)>;

    // "OnAggressionCBT":
    // Call-Back to be invoked on Aggressions. Args:
    //   (Aggression, UpdateID, TimeStamp)
    // where:
    //  TimeStamp is the earlist evidence of this Aggression;
    //  UpdateID  is the earlist SUBSEQUENT OrderBook update (which may already
    //    be affected by this Aggression).
    // Note that first all "OnOBUpdate" Call-Backs are invoked,  and  then  all
    // "OnAggression" ones: XXX: It is currently NOT possible to invoke them in
    // temporal sync with each other:
    //
    using OnAggressionCBT =
      std::function<void(MDAggression const&, long, utxx::time_val)>;

  private:
    //=======================================================================//
    // Internal Types:                                                       //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "OrderUpdate":                                                        //
    //-----------------------------------------------------------------------//
    // Coming from "OrderBooks":
    // For Cancellations (which may also be results of Complete Fills) and In-
    // Place Modifications (which are always Partial Fills):
    //
    struct Match;

    struct OrderUpdate
    {
      // Data Flds:
      utxx::time_val  m_updTS;
      OrderID         m_orderID;
      bool            m_isBid;      // Passive Side
      double          m_px;         // Passive Order Px
      double          m_wasQty;     // Qty before update
      double          m_nowQty;     // Qty after  update: 0 if deleted
      Match*          m_match;      // Corresp Match

      // Default Ctor:
      OrderUpdate()
      : m_updTS  (),
        m_orderID(0),
        m_isBid  (false),
        m_px     (0.0),
        m_wasQty (0.0),
        m_nowQty (0.0),
        m_match  (nullptr)          // Not yet known!
      {}

      // Properties:
      bool IsValid()  const
      {
        return !m_updTS.empty() && m_orderID != 0   && m_px > 0.0 &&
               m_wasQty > 0.0   && m_nowQty  >= 0.0 && m_wasQty > m_nowQty;
      }

      bool IsDelete() const
        { return m_wasQty > 0.0 && m_nowQty == 0.0; }
    };

    //-----------------------------------------------------------------------//
    // "Match":                                                              //
    //-----------------------------------------------------------------------//
    // Coming from "Trades":
    // An individual Trade (ie a Match between exactly 2 orders) which we will
    // try to map to OrderBook Updates, ie Order Cancels or Modifies:
    //
    struct Match
    {
      // Data Flds:
      utxx::time_val  m_trTS;       // TimeStamp from Trades Data
      double          m_px;         // TradePx
      double          m_qty;        // TradeQty
      bool            m_bidAggr;    // Buyer was the Aggressor?
      OrderUpdate*    m_upd;        // Corresp OrderUpdate

      // Default Ctor:
      Match()
      : m_trTS    (),
        m_px      (0.0),
        m_qty     (0.0),
        m_bidAggr (false),
        m_upd     (nullptr)         // Not yet known!
      {}

      // Properties:
      bool IsValid() const
        { return !m_trTS.empty() && m_px > 0.0 && m_qty > 0.0; }

      // Is this Match a complete Fill? -- We only know if the corresp Order-
      // Update is known:
      bool IsFill()  const
      {
        if (m_upd != nullptr)
        {
          // XXX: We do not check Px here:
          assert(m_upd->IsValid() && m_bidAggr == !(m_upd->m_isBid));
          return m_upd->IsDelete();
        }
        return false;   // XXX: We don't really know...
      }
    };

    //-----------------------------------------------------------------------//
    // "Aggression":                                                         //
    //-----------------------------------------------------------------------//
    // A MULTI-TRADE: A collection of Matches resulting from a single Aggressi-
    // ve Order. All of them are categorized by the same AggrSide  and a close
    // range of OrderBook TimeStamps   (ideally, the range of those TimeStamps
    // would contain just a single millisecond instant):
    //
    struct Aggression: public MDAggression
    {
      // Data Flds:
      // "m_totQty", "m_avgPx" and "m_BidAggr" are inherited from the parent:
      utxx::time_val            m_trTSs [2];  // Match TSs: [From, To]
      std::vector<Match*>       m_matches;    // All MatchPtrs from this Aggr
      utxx::time_val            m_updTSs[2];  // Corresp OrderBook Update TSs

      // Default Ctor:
      Aggression    ()
      : MDAggression(),
        m_matches   (),
        m_updTSs    ()
      {}

      // XXX: The following is NOT a full validity check:
      bool IsValid() const
      {
        return MDAggression::IsValid()     &&  m_avgPx > 0.0        &&
               !(m_trTSs [0].empty()       ||  m_trTSs [1].empty()) &&
               m_trTSs   [0] <= m_trTSs[1] && !m_matches  .empty()  &&
               !(m_updTSs[0].empty()       ||  m_updTSs[1].empty()) &&
               m_updTSs  [0] <= m_updTSs[1];
      }
    };

    //-----------------------------------------------------------------------//
    // "AggrPeriod":                                                         //
    //-----------------------------------------------------------------------//
    // Multiple Aggressions with overlapping time ranges ("obTSs") are further
    // clustered into "AggrPeriod"s. The latter are non-overlapping,   and are
    // arranged in a monotonically-ascending sequence:
    //
    struct AggrPeriod
    {
      // Data Flds:
      utxx::time_val            m_range[2]; // [FromTS, ToTS]
      std::vector<Match*>       m_matches;  // All Matches from all Aggrs there
      std::vector<OrderUpdate>  m_updates;

      // Default Ctor:
      AggrPeriod ()
      : m_range  (),
        m_matches(),
        m_updates()
      {}

      // Non-Default Ctor:
      AggrPeriod
      (
        utxx::time_val              a_from,
        utxx::time_val              a_to,
        std::vector<Match*> const&  a_matches   // Copied into "m_matches"
      )
      : m_range  { a_from, a_to },
        m_matches( a_matches ),
        m_updates()                             // No Updates yet
      {
        assert(!(a_from.empty() || a_to.empty() || a_matches.empty()) &&
               a_from < a_to);
      }
    };

  private:
    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    SecDefD const*  m_instr1;      // Curr Instr: Ptr NOT owned
    OrderBook*      m_ob1;         // Alias for quick access
    int     const   m_clustMSec;   // Separation (msec) for Matches clustering
    mutable char    m_lineBuff[256];

    // Matches and Aggressions for a whole Trading Week (24/5):
    mutable std::vector<Match>              m_wkMatches;
    mutable std::vector<Aggression>         m_wkAggrs;
    mutable std::vector<AggrPeriod>         m_wkPeriods;

    // For each OrderBook Update Time, map that Time  to  the lowest UpdateID
    // which happens at that time (NOT other way round: we will need this map
    // to install Aggressions causing those Updates).  Actually, we only need
    // TimeStamps from "m_wkPeriods" to occur in this map (as only those Time-
    // Stamps could possibly correspond to Aggression moments);  BaseUpdID is
    // used to construct  global (not-per-week) UpdateIDs  which are actually
    // passed to the Call-Backs:
    //
    mutable std::map<utxx::time_val, long>  m_wkUpdates;
    mutable long                            m_baseUpdID;  // Base for curr Wk

    // Histogram used to determine the optimal separation limit for clustering
    // Matches into Aggressions:
    mutable HistoGram<20>                   m_sepHist;

    // Histogram of temporal discrepancies between Matches and the corresp Ord-
    // erBook Updates:
    mutable HistoGram<20>                   m_discrHist;

    constexpr static double Eps = 1e-7;

    // Default Ctor is deleted as usual:
    MDReader_HotSpotFX() = delete;

  public:
    //=======================================================================//
    // Attributes (used by "MDReader"):                                      //
    //=======================================================================//
    // NB: Mark it as "-Hist"!
    // XXX:
    // (*) Here we assume that FractQtys may occur!
    // (*) DynInitMode is used to detect erroneous interleaving of SnapShots
    //     and FOL updates:
    //
    constexpr static char const* MDRNameHist      = "MDReader-HotSpotFX-Hist";
    constexpr static bool        IsFullAmt        = false;
    constexpr static bool        ChangeIsPartFill = true;
    constexpr static bool        NewEncdAsChange  = false;
    constexpr static bool        ChangeEncdAsNew  = false;
    constexpr static bool        IsSparse         = false;
    constexpr static bool        WithIncrUpdates  = true;
    constexpr static bool        WithTrades       = false;
    constexpr static bool        WithFracQtys     = true;
    constexpr static bool        WithDynInit      = true;
    constexpr static bool        StaticInitMode   = false; // Only appl to UDP
    constexpr static bool        WithRelaxedOBs   = false;
    constexpr static QtyTypeT    QT               = QtyTypeT::QtyA;
    using                        QR               = double;

    // Because DataFiles for HotSpotFX are separate for each 1-minute interval,
    // and contain SnapShot initialization, we do not need too many Orders; so
    // 500k is probbaly more than enough?
    constexpr static long DefaultMaxOrders    = 500'000;

    // However, we do need sufficiently many OrderBook Levels (as there may be
    // very deep orders placed):
    constexpr static int  DefaultMaxOBLevels  = 50'000;

    //=======================================================================//
    // Non-Default Ctor, Dtor:                                               //
    //=======================================================================//
    MDReader_HotSpotFX
    (
      std::vector<std::string>    const& a_instr_names,
      std::vector<FileTS>         const& a_data_files,
      boost::property_tree::ptree const& a_params
    );

    // Dtor is Trivial:
    ~MDReader_HotSpotFX() noexcept override {}

    //=======================================================================//
    // "Run":                                                                //
    //=======================================================================//
    // Read the Data Files, install Trades and OrderBook Updates, and invoke
    // the Call-Backs.    This method performs synchronisation of Trades and
    // OrderBook Updates:
    //
    void Run(OnOBUpdateCBT const& a_ob_cb, OnAggressionCBT const& a_aggr_cb);

    // The following "Run" method only processes OrderBook Updates, so no syn-
    // cronisation with Trades is performed, and Trades are not used at all:
    //
    void Run(OnOBUpdateCBT const& a_ob_cb);

  private:
    // This class is final, so the following method must finally be implemented:
    void EnsureAbstract()  const override {}

    //=======================================================================//
    // Data Reading Methods:                                                 //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "RunWk":                                                              //
    //-----------------------------------------------------------------------//
    // Process Trades and OrderBook Updates for 1 week (24/5) independently of
    // any others; AggrCB is NULL if we do not process Trades/Aggressions (and
    // do not syncronise them with OrderBook Updates):
    //
    void RunWk
    (
      std::vector<FileTS> const& a_curr_wk,
      OnOBUpdateCBT       const& a_ob_cb,
      OnAggressionCBT     const* a_aggr_cb  // May be NULL
    );

    //-----------------------------------------------------------------------//
    // "ReadWkTrades":                                                       //
    //-----------------------------------------------------------------------//
    // Reads the curr "Minute" intra-day ".trd" files, fills in the "m_wkAggrs"
    // vector.  XXX: Trades (Matches) are currently clustered into Aggressions
    // simply by direction and temporal separation:
    //
    void ReadWkTrades(std::vector<FileTS> const& a_base_files) const;

    //-----------------------------------------------------------------------//
    // "ReadWkOrderBookUpdates":                                             //
    //-----------------------------------------------------------------------//
    // Reads the curr "Minute" intra-day ".tks" files, integrates the previous-
    // ly-constructed "m_wkAggrs", and invokes the specified Call-Back. Returns
    // the number of Updates performed (ie weekly UpdateIDs generated):
    //
    long ReadWkOrderBookUpdates
    (
      std::vector<FileTS> const& a_base_files,
      OnOBUpdateCBT       const& a_ob_cb
    );

    //-----------------------------------------------------------------------//
    // "ReadTS":                                                             //
    //-----------------------------------------------------------------------//
    // Reads a TimeStamp from "m_lineBuff" and returns it if valid; "hour" and
    // "min" are known in advance, so they are only checked; returns empty  TS
    // if any error was encountered;  also returns the comma pos  after the TS
    // fld:
    std::pair<utxx::time_val, int>
    ReadTS(utxx::time_val a_min_ts, int a_hour, int a_min) const;

    //-----------------------------------------------------------------------//
    // "InvokeCBs":                                                          //
    //-----------------------------------------------------------------------//
    // Returns the number of Call-Back invocations performed:
    //
    int InvokeCBs
    (
      utxx::time_val        a_ts,
      OnOBUpdateCBT const&  a_ob_cb
    )
    const;

    //-----------------------------------------------------------------------//
    // "MapMatchesToUpdates":                                                //
    //-----------------------------------------------------------------------//
    // Tries to associate Matches (Trades) with OrderBook Updates which came
    // around the same times:
    //
    void MapMatchesToUpdates() const;

    //=======================================================================//
    // Utils:                                                                //
    //=======================================================================//
    // "FindAggression":
    // Find an Aggression to which the given Match may belong (returns NULL if
    // not found):
    //
    Aggression* FindAggression(Match const& a_mt) const;

    // "MkPeriods":
    // Merges Aggressions into non-overlapping Periods with monotonically-inc-
    // reasing Time Ranges:
    //
    void MkPeriods() const;

    // "AddMatches": Append "src" to "targ" and sort "targ" in the ascending
    // order of TimeStamps:
    static void AddMatches
    (
      std::vector<Match*> const& a_src,
      std::vector<Match*>*       a_targ
    );

  public:
    //=======================================================================//
    // Static Helper Methods:                                                //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "MkFullInstrName":                                                    //
    //-----------------------------------------------------------------------//
    // For "MDReader" Subscr Mgmt:
    //
    static std::string MkFullInstrName(std::string const& a_symbol)
      { return a_symbol + "-HotSpotFX--SPOT"; }
  };
} // End namespace QuantSupport
} // End namespace MAQUETTE
