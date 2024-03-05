// vim:ts=2:et
//===========================================================================//
//                  "TradingStrats/3Arb/TriArb2S/TriArb2S.h":                //
//            2-Sided Triangle Arbitrage (using MICEX and AlfaFIX2)          //
//===========================================================================//
#pragma once

//===========================================================================//
// Algorithm:                                                                //
//===========================================================================//
// (*) There are 3 Ccys: A=EUR, B=RUB, C=USD
// (*) Passively quote (at synthetic pxs): A/B and C/B on MICEX, on both sides,
//     for 2 SettlDates: TOD (T+0) and TOM (T+1); so up to 8 passive orders at
//     any given time;
// (*) on each passive fill (or aggressive fill, as a special case), fire up 2
//     covering orders (IoCs at some aggressive but limited px):
//     for A/B:  C/B (MICEX) and A/C (AlfaFIX2);
//     for C/B:  A/B (MICEX) and A/C (AlfaFIX2);
// (*) because   A/C on AlfaFIX2  is SPOT (T+2), its (T+0) or (T+1) price should
//     be corrected using OTC swaps:
//
#include "Basis/Maths.hpp"
#include "Basis/PxsQtys.h"
#include "Connectors/FAST/EConnector_FAST_MICEX.hpp"
#include "Connectors/FIX/EConnector_FIX.h"
#include "InfraStruct/Strategy.hpp"
#include <boost/property_tree/ini_parser.hpp>
#include <boost/container/static_vector.hpp>
#include <string>
#include <utility>
#include <vector>
#include <type_traits>

namespace MAQUETTE
{
  //=========================================================================//
  // "TriArb2S" Class:                                                       //
  //=========================================================================//
  class TriArb2S final: public Strategy
  {
  private:
    //=======================================================================//
    // The Connector and Qty Types:                                          //
    //=======================================================================//
    using EConnector_FAST_MICEX_FX =
          EConnector_FAST_MICEX
          <FAST::MICEX::ProtoVerT::Curr, MICEX::AssetClassT::FX>;

    // The following type is used by both OMCs and by AlfaFIX2 MDC. We will
    // also use it to represent all internal Qtys:
    constexpr static QtyTypeT QTO  = QtyTypeT::QtyA;
    using    QRO   = long;
    using    QtyO  = Qty<QTO,QRO>;

    static_assert(std::is_same_v<QtyO, EConnector_FIX_MICEX   ::QtyN>);
    static_assert(std::is_same_v<QtyO, EConnector_FIX_AlfaFIX2::QtyN>);

    // And the following type is used by MICEX MDC:
    constexpr static QtyTypeT QTM  = QtyTypeT::Lots;
    using    QRM   = long;
    using    QtyM  = Qty<QTM,QRM>;

    static_assert(std::is_same_v<QtyM, EConnector_FAST_MICEX_FX::QtyN>);

    // Converter: QtyM -> QTyO:
    using    ToContracts = QtyConverter<QTM, QTO>;

    //=======================================================================//
    // "TrInfo": Statistics of a Completed Triangle:                         //
    //=======================================================================//
    struct TrInfo
    {
      //---------------------------------------------------------------------//
      // Data Flds:                                                          //
      //---------------------------------------------------------------------//
      // Passive Side: AOS(BackPtr), Side, QuotedPx, TradedPx, TradedQty.
      // NB: Normally, QuotedPx and TradedPx must be the same, but they may
      // differ if eg we got aggressive execution instead of passive one:
      AOS const*  m_passAOS;
      PriceT      m_passQuotedPx;
      PriceT      m_passTradedPx;
      QtyO        m_passQty;
      QtyO        m_passQtyFilled;

      // Aggr Sides 1 and 2: Instrument, Side, SrcPx (used in calculation of
      // PassQuotedPx), TradedPx, TradedQty:
      AOS const*  m_aggrAOS      [2];
      PriceT      m_aggrSrcPx    [2];
      PriceT      m_aggrTradedPx [2];
      QtyO        m_aggrQty      [2];
      QtyO        m_aggrQtyFilled[2];

      // Effect of this Triangle: Remaining pos in A=EUR (should normally be 0),
      // B=RUB, C=USD, and the USD PnL.
      // Triangle is Complete when all 3 AOSes referenced by it, are Inactive:
      bool        m_isComplete;
      double      m_posA;
      double      m_posB;
      double      m_posC;
      double      m_PnLUSD;

      //---------------------------------------------------------------------//
      // Default Ctor:                                                       //
      //---------------------------------------------------------------------//
      TrInfo()
      : m_passAOS       (nullptr),
        m_passQuotedPx  (),        // NaN
        m_passTradedPx  (),        // NaN
        m_passQty       (),        // 0
        m_passQtyFilled (),        // 0
        m_aggrAOS       { nullptr, nullptr },
        m_aggrSrcPx     (),        // NaNs
        m_aggrTradedPx  (),        // NaNs
        m_aggrQty       { QtyO(),  QtyO(), },
        m_aggrQtyFilled { QtyO(),  QtyO(), },
        m_isComplete    (false),
        m_posA          (0.0),
        m_posB          (0.0),
        m_posC          (0.0),
        m_PnLUSD        (0.0)      // USD = C
      {}
    };

    //=======================================================================//
    // "OrderInfo": Order Descriptor installed as "AOS::UserData":           //
    //=======================================================================//
    struct OrderInfo
    {
      // All Orders are known from the beginning to be either Passive Quotes,
      // or Aggressive Covering Orders:
      bool    m_isAggr;

      // Passive Order Indices: T = TOD or TOM and I = AB or CB; if the Order
      // is actually aggressive, these indices refer to the passive order cov-
      // ered by this aggressive one:
      int     m_passT;
      int     m_passI;

      // Ptr to the "TrInfo" this Order belongs to:
      TrInfo* m_trInfo;

      // Default Ctor:
      OrderInfo()
      : m_isAggr(false),
        m_passT (0),
        m_passI (0),
        m_trInfo(nullptr)
      {}
    };

    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // Constants:                                                            //
    //-----------------------------------------------------------------------//
    // Ccy designations (for brievity):
    // A = EUR
    // B = RUB
    // C = USD
    Ccy const                   m_EUR;
    Ccy const                   m_RUB;
    Ccy const                   m_USD;

    // In the arrays below, there is a unified index notation:
    // T:  SettlDate (0=TOD,    1=TOM):
    constexpr static int        TOD = 0;
    constexpr static int        TOM = 1;

    // I:  PassiveInstr (0=AB,  1=CB) -- always on MICEX:
    constexpr static int        AB = 0;
    constexpr static int        CB = 1;

    // S:  Side         (0=Ask, 1=Bid): aka IsBid:
    constexpr static bool       Bid = true;
    constexpr static bool       Ask = false;

    // L:  Leg (1=MICEX, 0=AlfaFIX2):
    constexpr static bool       MICEX   = true;
    constexpr static bool       AlfaFIX2 = false;

    //-----------------------------------------------------------------------//
    // Infrastructure: Reactor, Logger, RiskMgr, Connectors:                 //
    //-----------------------------------------------------------------------//
    // XXX: Reactor and Connectors are contained INSIDE the "TriArb2S" class.
    // This is OK provided that we only have 1 Strategy  running at any time;
    // for a multi-Strategy platform, a generic Strategy Adaptor would  prob-
    // ably be required:
    SecDefsMgr*                 m_secDefsMgr;   // Allocated on ShM
    RiskMgr*                    m_riskMgr;      // Allocated on ShM

    EConnector_FAST_MICEX_FX    m_mdcMICEX;     // MDC MICEX  : FAST
    EConnector_FIX_AlfaFIX2     m_mdcAlfa;      // MDC AlfaFIX2: FIX
    // NB: At the moment, 2 OMC MICEX Connectors (with different FIX IDs) are
    // used, for load balancing:
    EConnector_FIX_MICEX        m_omcMICEX0;    // OMC MICEX  : FIX (2 conns)
    EConnector_FIX_MICEX        m_omcMICEX1;    // (cannot use an array...)
    EConnector_FIX_AlfaFIX2     m_omcAlfa;      // OMC AlfaFIX2: FIX

    // File into which the StatusLine is written (XXX: this is a temporary sol-
    // ution only -- there must be better ways than using a file):
    std::string                 m_statusFile;

    //-----------------------------------------------------------------------//
    // Instruments and OrderBooks:                                           //
    //-----------------------------------------------------------------------//
    // SettlDates:
    int const                   m_TOD;    // As YYYYMMDD
    int const                   m_TOM_ER; // ditto, for EUR/RUB
    int const                   m_TOM_UR; // ditto, for USD/RUB
    int const                   m_SPOT;   // ditto

    // Last time when quoting of TOD instrs is allowed. Also, this is considered
    // to be roll-over time for EUR positions (from TOD to TOM); although they
    // are NOT nominally converted into TOM, but, from this time on,
    // (*) any EUR/RUB_TOD instrs (passive or aggressive) are no longer used;
    // (*) aggressive USR/RUB_TOD is not used (no need, because it is only used
    //     for covering passive EUR/RUB_TOD);
    // (*) remaining EUR positions will be valued (in USD) using TOM-SPOT swap
    //     rates, rather than TOD-SPOT:
    // (*) typically, this time is  14:30 MSK = 11:30 UTC  (because EUR/RUB_TOD
    //     cut-off time at MICEX is 15:15 MSK):
    //
    utxx::time_val const        m_lastQuoteTimeTOD;
    mutable bool                m_isFinishedQuotesTOD;

    // On the other hand, PASSIVE USD/EUR_TOD may be received for slightly lon-
    // ger time, eg until 17:15 MSK. For completeness, also provide roll-over
    // time for EUR_TOD  (which may be equal to LastQuoteTimeTOD, or slightly
    // later):
    utxx::time_val const        m_rollOverTimeEUR;    // From EUR/RUB_TOD
    utxx::time_val const        m_rollOverTimeRUB;    // From USD/RUB_TOD

    // SecDefs for passive quotes on MICEX (indices  compatible  with [T,I] in
    // "m_passAOSes" below). Same instrs are used for MICEX aggressive leg:
    // [T: 0=TOD, 1=TOM] [I: 0=AB, 1=CB]:
    //                                       T  I
    SecDefD  const*  const      m_passInstrs[2][2];

    // Whether we use optimal routing for covering orders;
    // if not, "m_aggrInstrs" and "m_orderBooks[4..7]" are not used:
    bool                        m_useOptimalRouting;

    // Instruments similar to "m_passInstrs", but on AlfaFIX2; they can only be
    // used for Aggressive Orders (optimal routing of covering orders or elimi-
    // nation of Ccy tails):
    //                                       T  I
    SecDefD  const*  const      m_aggrInstrs[2][2];

    // AC=EUR/USD_SPOT: A purely aggressive instrument (AlfaFIX2 leg only):
    SecDefD  const*  const      m_AC;

    // OrderBooks for all Instrs. We need OrderBook data for both Passive and
    // Aggressive Instrs: Passive -- for adjusting the quotes; Aggressive  --
    // to compute synthetic Passive pxs:
    // [0..3]: same layout as for "m_passInstrs";
    // [4..7]: same layout as for "m_aggrInstrs";
    // [8]   : AC = EUR/USD_SPOT on AlfaFIX2 (purely-aggressive):
    OrderBook const*            m_orderBooks[9];

    // For Tails Adjustment, we need AssetRisks for RUR_TOM and RUB_TOM (will be
    // used for approx Ccy->USD conversions of *total* Tails):
    AssetRisks const*           m_risksEUR_TOM;
    AssetRisks const*           m_risksRUB_TOM;

    //-----------------------------------------------------------------------//
    // Quant and Related Params:                                             //
    //-----------------------------------------------------------------------//
    // Swap pxs for EUR/USD:  [0]: TOD->SPOT (T+0->T+2)  and
    //                        [1]: TOM->SPOT (T+1->T+2); need both Bid & Ask:
    //                                        T  IsBid
    double                      m_ACSwapPxs  [2][2];

    // Swap pxs for USD/RUB:  Only used for accurate P&L accounting:
    // TOD->TOM (T+0->T+1):                   IsBid
    double                      m_CBSwapPxs  [2];

    // Quoting Spreads for A/B and C/B -- XXX: currently same for Bid and Ask
    // sides,  and for TOD and TOM:
    double                      m_sprds      [2];

    // Qtys to be quoted for A/B and C/B; similarly, "ReverveVols" are Qtys with
    // some extra safety factor (normally > 1), to make sure we have enough liq-
    // uidity to cover our orders:
    QtyO                        m_qtys       [2];
    QtyO                        m_reserveQtys[2];

    // Params preventing unnecessary order modifications. "m_resist*" vals are
    // in PxSteps (same for all quoted instruments):
    // Depth is the min(CurrDepth, NewDepth):
    // (*) Depth <= 0:                always move      (ie if AbsDiff >= 1)
    // (*) 1 <= Depth < ResistDepth1: move iff AbsDiff >= ResistDiff1 >= 1
    // (*) ResistDepth1 <= Depth    : move iff AbsDiff >= ResistDiff2 >= 1
    //
    int                         m_resistDiff1;
    int                         m_resistDepth1;
    int                         m_resistDiff2;

    // "Discount Factor" applied to Qtys of a single L1 order on MICEX (which
    // may very well come from a Mkt Manipulator) in VWAP computations:
    double                      m_manipDiscount;

    // Maximum number of Rounds (Triangles) to be completed: 0 for +oo:
    int                         m_maxRounds;

    // Should we invoke "AdjustCcyTails" (normally yes, but it can be disabled
    // for safety reasons in "live testing" mode):
    // If so, we need a throttler to protect ourselves  from a potential flood
    // there: Use 30 sec, 10 buckets per sec:
    bool                        m_adjustCcyTails;

    constexpr static int        TailsThrottlerWindowSec        = 30;
    constexpr static int        TailsThrottlerBucketsPerSec    =  1;
    constexpr static int        TailsThrottlerPeriodSec        = 10;
    constexpr static int        TailsThrottlerMaxAdjsPerPeriod = 10;

    mutable utxx::basic_rate_throttler<TailsThrottlerWindowSec,
                                       TailsThrottlerBucketsPerSec>
                                m_tailsThrottler;

    // GraceTime allowed to received all Order Exec Reports if DelayedGraceful-
    // Stop was requested:
    constexpr static int        GracefulStopTimeSec            = 3;

    //-----------------------------------------------------------------------//
    // Infrastructure Statuses:                                              //
    //-----------------------------------------------------------------------//
    mutable bool                m_allConnsActive;
    mutable bool                m_isInDtor;     // To prevent ExitRun throwing
    mutable int                 m_roundsCount;  // How many Triangles completed
    mutable int                 m_signalsCount;

    //-----------------------------------------------------------------------//
    // Order Statuses:                                                       //
    //-----------------------------------------------------------------------//
    // Passive quotes on MICEX:
    // [T: 0=TOD, 1=TOM] [I: 0=A/B, 1=C/B] [IsBid: 0=Ask, 1=Bid]:
    //                                      T  I  IsBid
    mutable AOS const*          m_passAOSes[2][2][2];

    // Prev "Effective Base Pxs" for the 5 Instrs (corresp to the initial 5 "m_
    // orderBooks" above) used in computation of Synthetic Quoted Pxs. NB: they
    // are NOT necessarily L1 Best{Bid,Ask}Pxs: VWAP can be used;  the last dim
    // is 0=Ask, 1=Bid:                          IsBid
    mutable PriceT              m_prevBasePxs[5][2];

    // But we need L1 Best{Bid,Ask}Pxs as well to determine whether the quoting
    // constraints have changed (not reallyrequired for AC @ AlfaFIX2):
    //                                           IsBid
    mutable PriceT              m_prevL1Pxs  [5][2];

    // Theoretical (Synthetic) Pxs computed for all [T,I,S]: They may differ
    // from the actual quoting pxs, because the latters  are also   adjusted
    // against the curr target OrderBooks; memoised for easy re-quoting when
    // the src pxs stay unchanged but the target OrderBook moves:
    //                                      T  I  IsBid
    mutable PriceT              m_synthPxs [2][2][2];

    // Aggressive orders to cover passive fills in the above.  These orders are
    // typically "transient"  but the total number of  them  cannot  be exactly
    // known in advance -- eg a sequence of PartFills may generate a short-term
    // "avalanche" of Aggr Orders. For that reason, use a reasonably long static
    // vector of AOS ptrs (yet it would be empty most of the time):
    //
    using   AOSPtrsVec = boost::container::static_vector<AOS const*, 64>;
    mutable AOSPtrsVec          m_aggrAOSes;

    // Pre-allocated "TrInfo" vector used to accumulate Triange Stats. XXX: It
    // could potentially be large, so not a "static_vector" as yet:
    //
    mutable std::vector<TrInfo> m_trInfos;

    // Default Ctor is deleted:
    TriArb2S() = delete;

  public:
    //=======================================================================//
    // Non-Default Ctor, Dtor:                                               //
    //=======================================================================//
    TriArb2S
    (
      EPollReactor  *              a_reactor,
      spdlog::logger*              a_logger,
      boost::property_tree::ptree* a_params   // XXX: Mutable (via ptr)
    );

    ~TriArb2S() noexcept override;

    //=======================================================================//
    // "Run" and "*Stop*":                                                   //
    //=======================================================================//
    // "Run": Main Infinite Loop:
    void Run();

    // "GracefulStop":
    // Initiate graceful stop of all Connectors (will NOT  stop the Reactor im-
    // mediately -- the latter is done only when  all Connectors confirm termi-
    // nation):
    template<bool FromDtor = false>
    void GracefulStop();

    // "DelayedGracefulStop":
    // As above, but waits asymchronously for some reasonable time  (a few sec)
    // to receive final ExecReports from Connectors and update the RiskMgr pro-
    // perly:
    void DelayedGracefulStop
    (
      utxx::time_val            a_ts_exch,
      utxx::time_val            a_ts_recv,
      utxx::time_val            a_ts_strat
    );

    //=======================================================================//
    // Implementations of Virtual Call-Backs from the "Strategy" Class:      //
    //=======================================================================//
    void OnTradingEvent
    (
      EConnector const&         a_conn,
      bool                      a_on,
      SecID                     a_sec_id,
      utxx::time_val            a_ts_exch,
      utxx::time_val            a_ts_recv
    )
    override;

    void OnOrderBookUpdate
    (
      OrderBook const&          a_ob,
      bool                      a_is_error,
      OrderBook::UpdatedSidesT  a_sides,
      utxx::time_val            a_ts_exch,
      utxx::time_val            a_ts_recv,
      utxx::time_val            a_ts_strat
    )
    override;

    void OnTradeUpdate(Trade const& a_tr)  override;
    void OnOurTrade   (Trade const& a_tr)  override;
    void OnConfirm    (Req12 const& a_req) override;

    void OnCancel
    (
      AOS   const&              a_aos,
      utxx::time_val            a_ts_exch,
      utxx::time_val            a_ts_recv
    )
    override;

    void OnOrderError
    (
      Req12 const&              a_req,
      int                       a_err_code,
      char  const*              a_err_text,
      bool                      a_prob_filled,
      utxx::time_val            a_ts_exch,
      utxx::time_val            a_ts_recv
    )
    override;

    void OnSignal(signalfd_siginfo const& a_si)  override;

  private:
    //=======================================================================//
    // Internal Methods:                                                     //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "RemoveOrder": Common functionality of "OnCancel" and "OnError":      //
    //-----------------------------------------------------------------------//
    void RemoveOrder
    (
      AOS const&      a_aos,
      char const*     a_msg,
      utxx::time_val  a_ts_exch,
      utxx::time_val  a_ts_recv,
      utxx::time_val  a_ts_strat
    );

    //-----------------------------------------------------------------------//
    // "RemoveAggrOrder":                                                    //
    //-----------------------------------------------------------------------//
    void RemoveAggrOrder
    (
      AOS const&      a_aos,
      utxx::time_val  a_ts_exch,
      utxx::time_val  a_ts_recv,
      utxx::time_val  a_ts_strat
    );

    //-----------------------------------------------------------------------//
    // "Quote":                                                              //
    //-----------------------------------------------------------------------//
    // Template params indicate the Target:
    // T: TOD(0) or TOM(1);
    // I: A/B(0) or C/B(1);
    // IsBid:       Side(0 or 1);
    // IsMICEX:     SrcPx: MICEX(1) or AlfaFIX2(0):
    //
    template<int T, int I, bool IsBid, bool IsMICEX>
    bool Quote
    (
      PriceT            a_src_px,
      utxx::time_val    a_ts_md_exch,
      utxx::time_val    a_ts_md_conn,
      utxx::time_val    a_ts_md_strat
    );

    //-----------------------------------------------------------------------//
    // "GetLastQuote":                                                       //
    //-----------------------------------------------------------------------//
    // If "a_aos" is NULL or Inactive or PendingCancel, empty values (NaN and 0
    // resp) are returned:
    static void GetLastQuote(AOS const* a_aos, PriceT* a_px, QtyO* a_qty);

    //-----------------------------------------------------------------------//
    // "MkAggrPx":                                                           //
    //-----------------------------------------------------------------------//
    static PriceT MkAggrPx  (bool a_is_buy, OrderBook const& a_ob);

    //-----------------------------------------------------------------------//
    // "GetVWAP":                                                            //
    //-----------------------------------------------------------------------//
    // Used for computing base pxs (which are in turn used to compute synthetic
    // quoted pxs of "opposite" instrs) -- and in particular, to  mitigate  mkt
    // manipulation effects to some extent:
    //
    PriceT GetVWAP(int a_T, int a_I, bool a_IsBid, QtyO a_qty) const;

    //-----------------------------------------------------------------------//
    // "DoCoveringOrders":                                                   //
    //-----------------------------------------------------------------------//
    // (After a passive Quote received Hit or Lift):
    //
    void DoCoveringOrders
    (
      AOS const&      a_pass_aos,
      PriceT          a_pass_px,
      QtyO            a_pass_qty,
      utxx::time_val  a_ts_md_exch,
      utxx::time_val  a_ts_md_conn,
      utxx::time_val  a_ts_md_strat
    );

    //-----------------------------------------------------------------------//
    // "AdjustCcyTails":                                                     //
    //-----------------------------------------------------------------------//
    void AdjustCcyTails
    (
      utxx::time_val  a_ts_md_exch,
      utxx::time_val  a_ts_md_conn,
      utxx::time_val  a_ts_md_strat
    );

    //-----------------------------------------------------------------------//
    // "NewOrderSafe": Exception-Safe Wrapper:                               //
    //-----------------------------------------------------------------------//
    // OMC is a template param, to avoid virtual calls:
    //
    template<bool IsAggr, typename OMC>
    AOS const* NewOrderSafe
    (
      OMC&            a_omc,
      SecDefD const&  a_instr,
      bool            a_is_buy,
      PriceT          a_px,
      QtyO            a_qty,
      utxx::time_val  a_ts_md_exch,
      utxx::time_val  a_ts_md_conn,
      utxx::time_val  a_ts_md_strat,
      int             a_passT,            // For Aggr Orders only; can be -1
      int             a_passI,            // For Aggr Orders only; can be -1
      TrInfo*         a_tri               // For Aggr Orders only; can be NULL
    );

    //-----------------------------------------------------------------------//
    // "ModifyOrderSafe": Exception-Safe Wrapper:                            //
    //-----------------------------------------------------------------------//
    // OMC is a template param, to avoid virtual calls:
    //
    template<typename OMC>
    bool ModifyOrderSafe
    (
      OMC&            a_omc,
      AOS const*      a_aos,              // Non-NULL
      PriceT          a_px,
      QtyO            a_qty,
      utxx::time_val  a_ts_md_exch,
      utxx::time_val  a_ts_md_conn,
      utxx::time_val  a_ts_md_strat
    );

    //-----------------------------------------------------------------------//
    // "CancelOrderSafe": Exception-Safe Wrapper:                            //
    //-----------------------------------------------------------------------//
    // OMC is a template param, to avoid virtual calls:
    // If the order is already Inactive or PendingCancel, it's perfectly OK, so
    // return "true"; "false" is returned only in case of an exception caught:
    //
    template<bool Buffered, typename OMC>
    bool CancelOrderSafe
    (
      OMC&            a_omc,              // NB: Ref!
      AOS const*      a_aos,              // Non-NULL
      utxx::time_val  a_ts_md_exch,
      utxx::time_val  a_ts_md_conn,
      utxx::time_val  a_ts_md_strat
    );

    //-----------------------------------------------------------------------//
    // "MkReserveQty":                                                       //
    //-----------------------------------------------------------------------//
    // Calculate the "ReserveQty" which will be used at run-time to  determine
    // the AvgVolPx of instrument "I" (AB or CB) which will in turn be used to
    // calculate the synthetic quoting px of the other instrument  (CB or AB),
    // resp):
    template<int A>
    QtyO MkReserveQty(boost::property_tree::ptree const& a_params) const;

    //-----------------------------------------------------------------------//
    // "CheckAllConnectors":                                                 //
    //-----------------------------------------------------------------------//
    // Enables the "steady operating mode" when all Connectors and all Order-
    // Books become available:
    //
    void CheckAllConnectors(utxx::time_val a_now);

    //-----------------------------------------------------------------------//
    // "CheckTOD":                                                           //
    //-----------------------------------------------------------------------//
    // Should TOD instrs be disabled by now?
    //
    void CheckTOD(utxx::time_val a_now);

    //-----------------------------------------------------------------------//
    // "GetOMCMX":                                                           //
    //-----------------------------------------------------------------------//
    // By AOS: Selects the same MICEX OMC as in the given AOS (or throws an
    // exception). Used in @{Cancel,Modfy}OrderSafe":
    //
    EConnector_FIX_MICEX& GetOMCMX(AOS const* a_aos);

    // Randomly (using the lowest bit of the curr TimeStamp): used in "NewOrder-
    // Safe":
    EConnector_FIX_MICEX& GetOMCMX(utxx::time_val a_ts);

    //-----------------------------------------------------------------------//
    // "WriteStatusLine":                                                    //
    //-----------------------------------------------------------------------//
    void WriteStatusLine(char const* a_msg)     const;

    //-----------------------------------------------------------------------//
    // "RefreshThrottlers":                                                  //
    //-----------------------------------------------------------------------//
    void RefreshThrottlers(utxx::time_val a_ts) const;

    //-----------------------------------------------------------------------//
    // "IsStopping":                                                         //
    //-----------------------------------------------------------------------//
    bool IsStopping() const;
  };
} // End namespace MAQUETTE
