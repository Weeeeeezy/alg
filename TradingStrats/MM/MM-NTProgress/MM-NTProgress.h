// vim:ts=2:et
//===========================================================================//
//                "TradingStrats/MM-NTProgress/MM-NTProgress.h":             //
//              Market-Making (Liquidity Provision) on NTProgress            //
//===========================================================================//
#pragma once

#include "Basis/Maths.hpp"
#include "Connectors/FAST/EConnector_FAST_MICEX.hpp"
#include "Connectors/FIX/EConnector_FIX.h"
#include "InfraStruct/Strategy.hpp"
#include <boost/property_tree/ini_parser.hpp>
#include <boost/container/static_vector.hpp>
#include <string>
#include <utility>
#include <vector>

namespace MAQUETTE
{
  //=========================================================================//
  // "MM_NTProgress" Class:                                                  //
  //=========================================================================//
  class MM_NTProgress final: public Strategy
  {
  private:
    //=======================================================================//
    // Types:                                                                //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // Connector and Qty Types:                                              //
    //-----------------------------------------------------------------------//
    using EConnector_FAST_MICEX_FX =
          EConnector_FAST_MICEX
          <FAST::MICEX::ProtoVerT::Curr, MICEX::AssetClassT::FX>;

    // The following type is used by both OMCs.
    // We will also use it to represent all internal Qtys:
    constexpr static QtyTypeT QTO  = QtyTypeT::QtyA;
    using    QRO   = long;
    using    QtyO  = Qty<QTO,QRO>;

    static_assert(std::is_same_v<QtyO, EConnector_FIX_MICEX     ::QtyN>);
    static_assert(std::is_same_v<QtyO, EConnector_FIX_NTProgress::QtyN>);

    // And the following type is used by MICEX MDC  (XXX: NTProgress MDC is not
    // used at all -- our only ref is MICEX MktData, not NTProgress data):
    constexpr static QtyTypeT QTM  = QtyTypeT::Lots;
    using    QRM   = long;
    using    QtyM  = Qty<QTM,QRM>;

    static_assert(std::is_same_v<QtyM, EConnector_FAST_MICEX_FX::QtyN>);

    // Converter: QtyM -> QTyO:
    using    ToContracts = QtyConverter<QTM, QTO>;

    //-----------------------------------------------------------------------//
    // "OrderInfo": Order Descriptor installed as "AOS::UserData":           //
    //-----------------------------------------------------------------------//
    // (Not to be confused with "EConnector_MktData::OrderInfo"!):
    //
    struct OrderInfo
    {
      // Instrument Indices (to identify the the Instr by AOS more easily):
      int  m_T;         // TOD or TOM
      int  m_I;         // AB  or CB
      int  m_B;         // Band   [0 .. nBands-1]
      bool m_isPegged;  // Only meaningful for MICEX orders

      // Default Ctor:
      OrderInfo()
      : m_T       (0),
        m_I       (0),
        m_B       (0),
        m_isPegged(false)
      {}
    };

    //-----------------------------------------------------------------------//
    // "QStatusT": Status of a Quote Submission Attempt:                     //
    //-----------------------------------------------------------------------//
    enum class QStatusT
    {
      UNDEFINED  = 0,  // Before anything...
      NoQuotePx  = 1,  // Insufficient liquidity on MICEX
      MustWait   = 2,  // Could not re-quote immediately -- wait for Cancel
      PxUnchgd   = 3,  // Skipped because the Quote Px was unchanged
      Throttled  = 4,  // Skipped because of Throttling
      Failed     = 5,  // Submission attempt failed in Connector
      Done       = 6   // Really done ("NewOrder" or "Modify")
    };

    //-----------------------------------------------------------------------//
    // "DelayedGracefulStop":                                                //
    //-----------------------------------------------------------------------//
    class DelayedGracefulStop
    {
    private:
      MM_NTProgress*       m_outer;
      std::stringstream m_msg;

      void Out();
      template<typename T, typename... Args> void Out(T const&, Args&&...);

    public:
      template<typename... Args>
      DelayedGracefulStop
      (
        MM_NTProgress*     a_outer,
        utxx::time_val  a_ts_strat,
        Args&&... args
      );
    };

    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // Constants:                                                            //
    //-----------------------------------------------------------------------//
    // Ccy designations:
    Ccy const                     m_EUR;    // A
    Ccy const                     m_RUB;    // B
    Ccy const                     m_USD;    // C

    // In the arrays below, there is a unified index notation:
    // T:  SettlDate (0=TOD,    1=TOM):
    constexpr static int          TOD = 0;
    constexpr static int          TOM = 1;

    // I:  Instr (0=AB,  1=CB); also used is 2=AC, but that is for EUR risk
    // valuations only:
    constexpr static int          AB = 0;
    constexpr static int          CB = 1;
    constexpr static int          AC = 2;

    // S:  Side         (0=Ask, 1=Bid): aka IsBid:
    constexpr static bool         Bid = true;
    constexpr static bool         Ask = false;

    // Number of Quoting Bands for each Instrument and Side:
    constexpr static int          MaxBands = 10;

    //-----------------------------------------------------------------------//
    // Infrastructure: Reactor, Logger, RiskMgr, Connectors:                 //
    //-----------------------------------------------------------------------//
    // XXX: Reactor and Connectors are contained  INSIDE  the "MM_NTProgress"
    // class. This is OK provided that we only have 1 Strategy running at any
    // time; for a multi-Strategy platform, a generic Strategy Adaptor  would
    // probably be required:
    bool const                    m_dryRunMode;   // Dry-Run Mode
    SecDefsMgr*                   m_secDefsMgr;   // Allocated on ShM
    RiskMgr*                      m_riskMgr;      // Allocated on ShM

    EConnector_FAST_MICEX_FX      m_mdcMICEX;     // MDC MICEX      : FAST
    std::string                   m_omcNTPro_AccountPfx;
    EConnector_FIX_NTProgress     m_omcNTPro;     // OMC NTProgress : FIX
    std::string                   m_omcMICEX_AccountPfx;
    EConnector_FIX_MICEX          m_omcMICEX;     // OMC MICEX      : FIX

    //-----------------------------------------------------------------------//
    // Instruments and OrderBooks:                                           //
    //-----------------------------------------------------------------------//
    // SettlDates:
    int  const                    m_TOD;          // As YYYYMMDD
    int  const                    m_TOM_AB;       // ditto, for EUR/RUB
    int  const                    m_TOM_CB;       // ditto, for USD/RUB

    // Rough Fixed Exchange Rates for EUR/RUB (0) and USD/RUB (1), used to con-
    // vert Order Sizes to USD -- still requited even for Relaxed Mode  of the
    // RiskMgr:
    double           const        m_fixedRUB_USD; // NB: This way!
    double           const        m_fixedEUR_USD; // Normal way...

    // Last time when quoting of TOD instrs is allowed -- they are different for
    // EUR/RUB and USD/RUB:                          I
    utxx::time_val   const        m_lastQuoteTimeTOD[2];
    utxx::time_val   const        m_startTime;

    // Whether EUR/RUB and USD/RUB are quoted at all TOD (may not be quoted due
    // to the resp bank holidays); for completeness, similar flags for TOM:
    //                                               T  I  (incl AC!)
    mutable bool                  m_isInstrEnabled  [2][3];

    // SecDefs and OrderBooks for Quotes on NTProgress (indices compatible with
    // [T,I] in "m_aosesNTPro" below):
    // [T: 0=TOD, 1=TOM] [I: 0=AB, 1=CB] [S=IsBid: 0=Ask, 1=Bid]:
    //                                               T  I  S  B
    SecDefD   const* const        m_instrsNTPro     [2][2];
    mutable   AOS const*          m_aosesNTPro      [2][2][2][MaxBands];

    // Curr QuotePxs on NTProgress. NB: They are only updated by MktData events;
    // if a quote (the corresp NTProgress AOS above) is Filled/Cancelled/Failed,
    // the CurrPx is not reset, so we can re-quote at the same px:
    //                                               T  I  S  B
    mutable   PriceT              m_currQPxs        [2][2][2][MaxBands];

    // Prev MICEX Best Pxs (required for Pegging); TODO: Move this functionality
    // into the OrderBook itself:                    T  I  S
    mutable   PriceT              m_prevBestPxs     [2][2][2];

    // Instruments similar to above, but on MICEX, used to generate Base Pxs for
    // our Quotes; also include AC instrs used for RiskMgmt only:
    //                                               T  I
    SecDefD   const* const        m_instrsMICEX     [2][3];
    OrderBook const* const        m_obsMICEX        [2][3];

    // If we use Pegging Orders for position covering (on MICEX), we must memo-
    // ise their AOSes. There is only 1 Pegged AOS per Tenor/Instrument/Side:
    //                                               T  I  S
    mutable   AOS const*          m_aosesPegMICEX   [2][2][2];

    // All AOSes currently being Cancelled. We keep track of them until the
    // Cancel is confirmed, so that we can immediately NULLify  the corresp
    // main AOS, and issue a New Order:
    //
    using   AOSPtrsVec = boost::container::static_vector<AOS const*, 512>;
    mutable AOSPtrsVec            m_cpAOSes;        // CP = CancelPending

    // XXX: We currently do NOT record the "transient" AOSes for Covering Ords
    // on MICEX, but we need to memoise the amounts of the corresp orders,  so
    // that the orders which are already "on the fly"  are taken  into account
    // when computing the size of subsequent covering orders:
    // NB: "FlyingCovDeltas" are Signed ("+" for Buy, "-" for Sell):
    //                                              T  I
    mutable QtyO                  m_flyingCovDeltas[2][2];

    //-----------------------------------------------------------------------//
    // Quantitative and Other Params:                                        //
    //-----------------------------------------------------------------------//
    // TOD-TOM Swap rates (for accurate risks and P&L valuations as well as for
    // optimal covering):                      I  Bid/Ask
    double                        m_swapRates [3][2];

    // The actual number of QuoteBands (can be different for different Tenors
    // and Instrs: in 0..MaxBands):            T  I
    int                           m_nBands    [2][2];

    // Quoted Qtys are same for Bid/Ask, but may be different across Tenors or
    // Instrs:                                 T  I  Band
    QtyO                          m_qtys      [2][2][MaxBands];

    // Mark-Ups (spread added on both Sides) applied to our Quotes:
    //                                         T  I  Band
    double                        m_markUps   [2][2][MaxBands];

    // Exponent ("beta") to be used in the Skewing Function:
    // 0 < beta < 1: concave func;
    // 1 = beta    : linear  func;
    // 1 < beta    : convex  func:
    // NB: In general, there may be different vals of "beta" for different
    // Instrs:                                 I
    double const                  m_betas     [2];

    // Barriers which stop (among other measures)  "too aggressive position-
    // based quote px improvements" (on top of the best MICEX px of the corr
    // side):                                  I
    double const                  m_maxQImprs [2];

    // Ptrs to InstrRisks in the RiskMgr, used to get positional info and comp-
    // ute skewing of quotes:                  T  I
    InstrRisks const*             m_irsNTPro  [2][2];   // On NTProgress (Pass)
    InstrRisks const*             m_irsMICEX  [2][2];   // On MICEX      (Aggr)

    // Position Limits for each Tenor / Instrument to be quoted:
    //                                         T  I
    QtyO const                    m_posLimits [2][2];

    // When we cover an excessive position, should we do that for the whole pos
    // size, or for the excess only?
    bool const                    m_coverWholePos;

    // Apply QuotePx skewing on one side or both sides?
    bool const                    m_skewBothSides;

    // If a Band is not available on one side, remove it from the other side
    // as well?
    bool const                    m_useSymmetricBands;

    // Whether Pegging (as opposed to Market Orders) is used for position cove-
    // ring:                                   T  I
    bool const                    m_usePegging[2][2];

    // A-Priori Limits for Quote Pxs: Used to detect calculations based on spu-
    // rious mkt data:                         I  Min,Max
    PriceT                        m_qPxRanges [2][2];

    // "Resistances" of Quotes Pxs, separately for different Instrs  and Bands:
    // Do not move an existing quote if the new calculated new px differes from
    // the curr quote px by no more than the corresp Resistance value:
    //                                         I  Band
    double                        m_qPxResists[2][MaxBands];

    // How we deal with potential market manipulation:
    // "ReductionCoeff" applied in VWAP computations to sizes of lone orders at
    // a given px level (which may be submitted by a manipulator):
    double                        m_manipRedCoeff;

    // Whether the above reduction coeff is applied on L1 only, or at all Order-
    // Book levels:
    bool                          m_manipRedOnlyL1;

    // The min interval beween re-quoting each Tenor / Instr / Side    (after a
    // Fill, Cancel or Quote Failure), in msec; this param is required in order
    // to prevent "toxic" clients from abusing NTProgress:
    int const                     m_minInterQuote;

    // The max re-quoting interval (in msec, similar to above); used to prevent
    // "flcikering" of quoted pxs in case there is no MktData updates:
    int const                     m_maxInterQuote;

    // The actual last quote times:
    // NB: No further division by Side and Band here, because all Sides and
    // Bands are processed together:               T  I  S  Band
    mutable utxx::time_val        m_lastQuoteTimes[2][2][2][MaxBands];

    // TimerFD used for monitoring of "MaxInterQuote":
    int                           m_timerFD;

    // Maximum number of Rounds, ie Quotes to be filled: usually +oo, but may be
    // limited for testing/debugging purposes:
    int                           m_maxRounds;

    // Soft Limit for the max number of Quote Modification Reqs per  sec  on
    // NTProgress. If this limit is exceeded, we try to reduce the Reqs Rate by
    // widening the quoting spreads:
    double                        m_maxReqsPerSec;

    //-----------------------------------------------------------------------//
    // Infrastructure Statuses:                                              //
    //-----------------------------------------------------------------------//
    // StatusLineFile used to output the error msg to be displayed in the GUI:
    std::string                   m_statusLineFile;
    mutable bool                  m_allConnsActive;
    mutable bool                  m_isInDtor;    // To prevent ExitRun throwing
    mutable int                   m_roundsCount; // <= m_maxRounds
    mutable int                   m_signalsCount;

  public:
    // Default Ctor is deleted:
    MM_NTProgress() = delete;

    //=======================================================================//
    // Non-Default Ctor, Dtor:                                               //
    //=======================================================================//
    MM_NTProgress
    (
      EPollReactor*                a_reactor,
      spdlog::logger*              a_logger,
      boost::property_tree::ptree& a_params     // XXX: Mutable!
    );

    ~MM_NTProgress() noexcept override;

    //=======================================================================//
    // "Run" and "*Stop*":                                                   //
    //=======================================================================//
    // "Run": Main Infinite Loop:
    void Run();

    // "SemiGracefulStop":
    // Initiate graceful stop of all Connectors (will NOT  stop the Reactor im-
    // mediately -- the latter is done only when  all Connectors confirm termi-
    // nation):
    void SemiGracefulStop();

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

    void OnOurTrade(Trade const& a_tr) override;

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
      bool                      a_prob_failed,
      utxx::time_val            a_ts_exch,
      utxx::time_val            a_ts_recv
    )
    override;

    void OnSignal   (signalfd_siginfo const& a_si)  override;

    // NB: The following methods:
    //    "OnTradeUpdate", "OnConfirm"
    // are currently NOT overridden -- their default impls do nothing...

  private:
    //=======================================================================//
    // Internal Methods:                                                     //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "OnTimer": Additional Call-Back:                                      //
    //-----------------------------------------------------------------------//
    // Used to enforce MaxInterQuote limit, and so prevent "quotes flickering":
    void OnTimer();

    //-----------------------------------------------------------------------//
    // "CheckPassAOS":                                                       //
    //-----------------------------------------------------------------------//
    // (*) Verifies that the given AOS is indeed found  in the corresp Main Pas-
    //     sive Slot on NTPro, or a Pegged Slot on MICEX, or on the CxlPending
    //     list.
    // (*) Returns "true" iff that was the case.
    // (*) Otherwise, an inconsitency was detected, strategy termination initia-
    //     ted, and "false" is returned -- but no exceptions are thrown.
    // (*) Inactive AOSes are completely removed from their resp locations.
    // (*) The "o_{TISB}" outputs are set in any case with the corresp intrinsic
    //     params of the AOS (as they may be needed for eg Re-Quote);
    // (*) "o_was_cp" indicates that the AOS was found NOT in the Main or Pegged
    //     [TISB] Slot, but rather or the CxlPending List (in any case, it  must
    //     be in exactly 1 location):
    //
    bool CheckPassAOS
    (
      char const*     a_where,
      AOS const*      a_aos,
      OrderID         a_req_id,
      char const*     a_err_msg,  // NULL if no error reported
      int*            o_T,        // SettlDate : TOD or TOM
      int*            o_I,        // Instrument: AB  or CB
      int*            o_S,        // Side      : Ask or Bid
      int*            o_B,        // Band        [0 .. nBands-1]
      bool*           o_was_cp,   // AOS was found on CxlPending List
      utxx::time_val  a_ts_strat
    );

    //-----------------------------------------------------------------------//
    // "Get{Main,Pegged}AOS": Controlled access to m_aoses{NTPro,PegMICEX}:  //
    //-----------------------------------------------------------------------//
    AOS const*& GetMainAOS  (int a_T, int a_I, int a_S, int a_B) const;
    AOS const*& GetPeggedAOS(int a_T, int a_I, int a_S)          const;

    //-----------------------------------------------------------------------//
    // "DoQuotes", "Do1Quote":                                               //
    //-----------------------------------------------------------------------//
    // "DoQuotes" Generates or updates quotes for 1 instrument on NTProgress,
    // both Sides, all Bands:
    void DoQuotes
    (
      int             a_T,                        // TOD or TOM
      int             a_I,                        // AB  or CB
      utxx::time_val  a_ts_exch,
      utxx::time_val  a_ts_recv,
      utxx::time_val  a_ts_strat
    );

    // "Do1Quote": As "DoQuotes", but for 1 Side and Band:
    void Do1Quote
    (
      int             a_T,                        // As above
      int             a_I,                        // Ditto
      int             a_S,                        // Side (Ask or Bid)
      int             a_B,                        // Band
      utxx::time_val  a_ts_exch,
      utxx::time_val  a_ts_recv,
      utxx::time_val  a_ts_strat
    );

    //-----------------------------------------------------------------------//
    // "MkQuotePxs":                                                         //
    //-----------------------------------------------------------------------//
    // Returns "false" iff a logic error was encountered which warrants termin-
    // ation of the Market-Making process:
    //
    bool MkQuotePxs
    (
      int             a_T,
      int             a_I,
      utxx::time_val  a_ts_strat,
      PriceT          (&a_new_pxs)[2][MaxBands]   // NB: Ref to array!
    );

    //-----------------------------------------------------------------------//
    // "ApplyQuotePxTransforms":                                             //
    //-----------------------------------------------------------------------//
    // Contains the actual quantitative logic of quoting. Used by "MkQuotePxs":
    //
    PriceT ApplyQuotePxTransforms
    (
      int             a_T,
      int             a_I,
      bool            a_is_bid,
      int             a_B,
      PriceT          a_px,           // Intermediate Px to be adjusted
      PriceT          a_this_l1,      // BestPx of the This     Side
      PriceT          a_opp_l1,       // BestPx of the Opposite Side
      QtyO            a_curr_pos,
      QtyO            a_max_pos
    )
    const;

    //-----------------------------------------------------------------------//
    // "SubmitQuotes", "Submit1Quote":                                       //
    //-----------------------------------------------------------------------//
    // Used by "DoQuotes", "Do1Quote" resp:
    void SubmitQuotes
    (
      int             a_T,
      int             a_I,
      PriceT const    (&a_new_pxs)[2][MaxBands],  // NB: Const ref to array!
      utxx::time_val  a_ts_exch,
      utxx::time_val  a_ts_recv,
      utxx::time_val  a_ts_strat
    );

    // "Submit1Quote" returns the QStatusT:
    QStatusT Submit1Quote
    (
      int             a_T,
      int             a_I,
      int             a_S,
      int             a_B,
      PriceT          a_new_px,
      utxx::time_val  a_ts_exch,
      utxx::time_val  a_ts_recv,
      utxx::time_val  a_ts_strat
    );

    //-----------------------------------------------------------------------//
    // "MayBeDoCoveringOrder":                                               //
    //-----------------------------------------------------------------------//
    // Check the position after a Passive Trade, and possibly issue a Covering
    // (Aggressive) Order:
    void MayBeDoCoveringOrder
    (
      int             a_T,
      int             a_I,
      utxx::time_val  a_ts_exch,
      utxx::time_val  a_ts_recv,
      utxx::time_val  a_ts_strat
    );

    //-----------------------------------------------------------------------//
    // "NewPassOrderSafe": Exception-Safe Wrapper:                           //
    //-----------------------------------------------------------------------//
    // Returns the newly-created AOS ptr, or NULL in case of error:
    //
    template<bool Buffered>
    AOS const* NewPassOrderSafe
    (
      SecDefD const&  a_instr,
      bool            a_is_buy,
      PriceT          a_px,
      QtyO            a_qty,
      utxx::time_val  a_ts_exch,
      utxx::time_val  a_ts_recv,
      utxx::time_val  a_ts_strat,
      int             a_T,
      int             a_I,
      int             a_B
    );

    //-----------------------------------------------------------------------//
    // "ModifyPassOrderSafe": Exception-Safe Wrapper:                        //
    //-----------------------------------------------------------------------//
    // Returns "true" iff the modification req was successfully submitted, and
    // "false" in case of any errors (which typically case DelayedStop):
    //
    template<bool Buffered>
    bool ModifyPassOrderSafe
    (
      AOS const*      a_aos,        // Non-NULL
      PriceT          a_px,
      QtyO            a_qty,
      utxx::time_val  a_ts_exch,
      utxx::time_val  a_ts_recv,
      utxx::time_val  a_ts_strat
    );

    //-----------------------------------------------------------------------//
    // "CancelPassOrderSafe": Exception-Safe Wrapper:                        //
    //-----------------------------------------------------------------------//
    // Returns "true" iff the Cancellation req has actually been submitted;
    // "false" if the order is already Inactive or CxlPending, or any error
    // has occurred (which typically triggers DelayedStop):
    //
    template<bool Buffered>
    bool CancelPassOrderSafe
    (
      AOS const*      a_aos,        // Non-NULL
      utxx::time_val  a_ts_exch,
      utxx::time_val  a_ts_recv,
      utxx::time_val  a_ts_strat
    );

    // As above, but for a Main AOS Slot (an NTPro Quote): no need to return a
    // "bool":
    template<bool Buffered>
    void CancelQuoteSafe
    (
      int             a_T,
      int             a_I,
      int             a_S,
      int             a_B,
      utxx::time_val  a_ts_exch,
      utxx::time_val  a_ts_recv,
      utxx::time_val  a_ts_strat
    );

    // As above, but for a Pegged MICEX Order:
    template<bool Buffered>
    void CancelPeggedSafe
    (
      int             a_T,
      int             a_I,
      int             a_S,
      utxx::time_val  a_ts_exch,
      utxx::time_val  a_ts_recv,
      utxx::time_val  a_ts_strat
    );

    //-----------------------------------------------------------------------//
    // "CheckAllConnectors":                                                 //
    //-----------------------------------------------------------------------//
    // Enables the "steady operating mode" when all Connectors and all Order-
    // Books become available:
    //
    void CheckAllConnectors(utxx::time_val a_ts_strat);

    //-----------------------------------------------------------------------//
    // "RegisterConverters":                                                 //
    //-----------------------------------------------------------------------//
    // Returns "true" iff successful, "false" iff failed (and stop has been in-
    // itiated):
    bool RegisterConverters(utxx::time_val a_ts_strat);

    //-----------------------------------------------------------------------//
    // "CheckTOD", "EvalStopConds":                                          //
    //-----------------------------------------------------------------------//
    // Should TOD instrs be disabled by now? -- Sets the corresp flags if so:
    //
    void CheckTOD(utxx::time_val a_now);

    // "EvalStopConds": Returns "true" iff we must NOT proceed further (invoked
    // from "OnOrderBookUpdate" and "OnTimer"):
    //
    bool EvalStopConds
    (
      utxx::time_val  a_ts_exch,
      utxx::time_val  a_ts_recv,
      utxx::time_val  a_ts_strat
    );

    //-----------------------------------------------------------------------//
    // "CancelAllOrders":                                                    //
    //-----------------------------------------------------------------------//
    // Cancelling all active Quotes and possibly Pegged Orders:
    //
    template<bool WithPegged>
    void CancelAllOrders
    (
      utxx::time_val  a_ts_exch,
      utxx::time_val  a_ts_recv,
      utxx::time_val  a_ts_strat
    );

    //-----------------------------------------------------------------------//
    // "LogQuote":                                                           //
    //-----------------------------------------------------------------------//
    // Does not actually perform logging -- only outputs the "a_buff" content:
    //
    char* LogQuote
    (
      int       a_T,
      int       a_I,
      int       a_S,
      int       a_B,
      QStatusT  a_stat,
      char*     a_buff
    )
    const;

    //-----------------------------------------------------------------------//
    // "IsStopping" (so no further Quotes etc):                              //
    //-----------------------------------------------------------------------//
    bool IsStopping() const
      { return (!m_delayedStopInited.empty() || m_nowStopInited); }

    //-----------------------------------------------------------------------//
    // "GetSNPos":                                                           //
    //-----------------------------------------------------------------------//
    // For a given Tenor and Instriment (ie Ccy Pair), produces a "semi-netted"
    // position across  the NTProgress (Passive) and MICEX (Aggressive) sides;
    // but full per-Ccy netting is NOT done in this case:
    //
    QtyO GetSNPos(int a_T, int a_I, utxx::time_val a_ts_strat);
  };
} // End namespace MAQUETTE
