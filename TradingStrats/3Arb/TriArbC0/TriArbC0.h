// vim:ts=2:et
//===========================================================================//
//                  "TradingStrats/3Arb/TriArbC0/TriArbC0.h":                //
//       Single-Exchange Triangle Arbitrage over a Fixed Set of Instrs       //
//===========================================================================//
#pragma once

//===========================================================================//
// Algorithm:                                                                //
//===========================================================================//
// (*) There are 3 Ccys: A (eg ETH), B (eg USDT), C (eg BTC).
// (*) Passively quote (at synthetic pxs): AB, CB and AC   on both Bid and Ask
//     sides at the specified Exchange; so up to 6 passive orders at any given
//     time;
// (*) on each passive fill (or aggressive fill, as a special case), fire up 2
//     covering orders (IoCs at some aggressive but limited pxs):
//     for AB:  CB and AC;
//     for CB:  AB and AC;
//     for AC:  AB and CB.
//
#include "Basis/Maths.hpp"
#include "Basis/PxsQtys.h"
#include "Connectors/H2WS/Binance/EConnector_H2WS_Binance_MDC.h"
#include "Connectors/H2WS/Binance/EConnector_H2WS_Binance_OMC.h"
#include "Connectors/H2WS/BitMEX/EConnector_WS_BitMEX_MDC.h"
#include "Connectors/H2WS/BitMEX/EConnector_H2WS_BitMEX_OMC.h"
#include "InfraStruct/Strategy.hpp"
#include "InfraStruct/PersistMgr.h"
#include <boost/property_tree/ini_parser.hpp>
#include <boost/container/static_vector.hpp>
#include <string>
#include <utility>
#include <vector>
#include <type_traits>

namespace MAQUETTE
{
  //=========================================================================//
  // Exchanges Suitable for TriArb:                                          //
  //=========================================================================//
  enum class ExchC0T: int
  {
    BinanceSpot = 0,
    BitMEX      = 1
  };

  //=========================================================================//
  // MDC and OMC Types for various Exchanges:                                //
  //=========================================================================//
  // NB: "QtyO" is the Original / OMC / OrderBook Qty Type used by the MDC and
  // OMC connectors (their Qty Types are assumed to be same); it may be differ-
  // ent from the normalised "QtyN" (see below) used by the Strategy:
  //
  template<ExchC0T X>
  struct   ConnsC0;

  //-------------------------------------------------------------------------//
  // "BinanceSpot":                                                          //
  //-------------------------------------------------------------------------//
  template<>
  struct   ConnsC0<ExchC0T::BinanceSpot>
  {
    // MDC and OMC:
    using MDC = EConnector_H2WS_Binance_MDC_Spt;
    using OMC = EConnector_H2WS_Binance_OMC_Spt;
    constexpr static char const* MDCSuffix = "-H2WS-Binance-MDC-Spt-";
    constexpr static char const* OMCSuffix = "-H2WS-Binance-OMC-Spt-";

    // Qty Type:
    using QRO                      = MDC::QR;
    using QtyO                     = MDC::QtyN;
    constexpr static QtyTypeT QTO  = MDC::QT;

    static_assert
      (QTO == OMC::QT                           &&
       std::is_same_v<QRO,  typename OMC::QR>   &&
       std::is_same_v<QtyO, typename OMC::QtyN> &&
       std::is_same_v<QtyO, Qty<QTO,QRO>>);
  };

  //-------------------------------------------------------------------------//
  // "BitMEX":                                                               //
  //-------------------------------------------------------------------------//
  template<>
  struct   ConnsC0<ExchC0T::BitMEX>
  {
    // MDC and OMC:
    using MDC = EConnector_WS_BitMEX_MDC;
    using OMC = EConnector_H2WS_BitMEX_OMC;
    constexpr static char const* MDCSuffix = "-H2WS-BitMEX-MDC-";
    constexpr static char const* OMCSuffix = "-H2WS-BitMEX-OMC-";

    // Qty Type:
    using QRO                      = MDC::QR;
    using QtyO                     = MDC::QtyN;
    constexpr static QtyTypeT QTO  = MDC::QT;

    static_assert
      (QTO == OMC::QT                           &&
       std::is_same_v<QRO,  typename OMC::QR>   &&
       std::is_same_v<QtyO, typename OMC::QtyN> &&
       std::is_same_v<QtyO, Qty<QTO,QRO>>);
  };

  //=========================================================================//
  // "TriArbC0" Class:                                                       //
  //=========================================================================//
  template<ExchC0T X>
  class    TriArbC0 final: public Strategy
  {
    //=======================================================================//
    // Types and Consts:                                                     //
    //=======================================================================//
    // MDC, OMC and their common (as checked by ConnsC0<X>) Qty Types:
    using MDC                      = typename ConnsC0<X>::MDC;
    using OMC                      = typename ConnsC0<X>::OMC;
    constexpr static QtyTypeT QTO  =          ConnsC0<X>::QTO;
    using QRO                      = typename ConnsC0<X>::QRO;
    using QtyO                     = typename ConnsC0<X>::QtyO;

    // The Qty Type used by the Strategy may be different: We need <QtyA,double>
    // in order to perform all arbitrage computations:
    constexpr static QtyTypeT QT   = QtyTypeT::QtyA;
    using                     QR   = double;
    using                     QtyN = Qty<QT,QR>;

    //=======================================================================//
    // "TrInfo": Statistics of a Current or a Completed Triangle:            //
    //=======================================================================//
    struct TrInfo
    {
      //---------------------------------------------------------------------//
      // Data Flds:                                                          //
      //---------------------------------------------------------------------//
      // PassiveLeg: Instr, PassiveQty, PassiveQuotePx, AOS, etc.
      // NB: Although most of these params can be inferred from the AOS, we
      // provide them separately for the case when there is no AOS yet:
      //
      unsigned       m_id;           // ID of this "TrInfo"
      int            m_passI;        // 0..2: 0=AB, 1=CB, 2=AC
      int            m_passS;        // Side: 0=Ask,1=Bid
      SecDefD const* m_passInstr;
      QtyN           m_passQty;      // Quoted Qty
      PriceT         m_passPx;       // Quoted Px (as Px(A/B))
      AOS     const* m_passAOS;

      // 3 AggrLegs for a given Passive one, indexed by "AggrJ":
      // 0:   Aggr Closing of the Lassove Leg;
      // 1,2: Aggr Legs    of the TriAngle:
      // (*) Again, Instrs, Qtys etc are provided independently of AOSes.
      // (*) In general, there could be multiple AOSes for each AggrLeg (becau-
      //     se for each part-fill of the Passive Leg, a separate AggrOrder is
      //     fired up). Here we store the Last AggrAOS ptr for each Leg, others
      //     can be obtained via the corresp ptrs in "AOSExt" within that AOS:
      int            m_aggrIs        [3];  // I=0..2 as above
      int            m_aggrSs        [3];  // S=0..1 as above
      SecDefD const* m_aggrInstrs    [3];
      QtyN           m_aggrQtys      [3];  // Calculated Qtys
      QtyN           m_aggrVols      [3];  // As Qtys, but with Reserve Factors
      PriceT         m_aggrSrcPxs    [3];  // Pxs used in PassivePx calc
      AOS     const* m_aggrLastAOSes [3];

      // For slippage analysis, we provide average FillPxs for Passive and Aggr
      // Legs.  Obviously, for the PassiveLeg, the FillPx must be no worse than
      // the QuotedPx:
      QtyN           m_passFilledQty;
      PriceT         m_passAvgFillPx;      // As Pxs(A/B)
      QtyN           m_aggrFilledQtys[3];
      PriceT         m_aggrAvgFillPxs[3];  // As Pxs(A/B)

      // Effect of this Triangle:
      // Remaining poss in A (should normally be 0), B, C (should again be ~0),
      // and the PnL expressed in RFC (whatever is used in the RiskMgr):
      double         m_posA;
      double         m_posB;
      double         m_posC;
      double         m_PnLRFC;

      //---------------------------------------------------------------------//
      // Methods:                                                            //
      //---------------------------------------------------------------------//
      // Default Ctor:
      TrInfo();

      // For debugging only: "a_j" is AggrClosing (0) or AggrLegNo (1|2):
      void CheckInvariants() const;
      void GetPassTrades  (         std::vector<Trade const*>* o_trades) const;
      void GetAggrTrades  (int a_j, std::vector<Trade const*>* o_trades) const;
    };

    //=======================================================================//
    // "AOSExt": Extra Info installed as "AOS::UserData":                    //
    //=======================================================================//
    struct AOSExt
    {
      //---------------------------------------------------------------------//
      // Data Flds:                                                          //
      //---------------------------------------------------------------------//
      // (Redundant) ptr to the AOS this "AOSExt" belongs to:
      AOS const*  m_thisAOS;

      // Ptr to the "TrInfo" (in ShM!) this AOS belongs to:
      TrInfo*     m_trInfo;

      // How to find this AOS in "TrInfo": AggrJ={-1|0|1|2}:
      //   -1: Passive    TriAngle Leg;
      //    0: Aggressive Closing of the Passive Leg;
      //  1,2: Aggressive TriAngle Legs:
      int         m_aggrJ;

      // Ptr used to construct chain of AggrAOSes; always NULL for Passive one:
      AOS const*  m_prevAggrAOS;

      //---------------------------------------------------------------------//
      // Methods:                                                            //
      //---------------------------------------------------------------------//
      // Default Ctor:
      AOSExt()
      : m_thisAOS    (nullptr),
        m_trInfo     (nullptr),
        m_aggrJ      (-1),
        m_prevAggrAOS(nullptr)
      {}

      // For debugging only (generates assert failures if any inconsistencies
      // are found):
      void CheckInvariants() const;
    };

  private:
    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // Constants, Ccys, InstrNames:                                          //
    //-----------------------------------------------------------------------//
    // Ccys: For example: A=ETH, B=USDT, C=BTC:
    constexpr static int            A   = 0;
    constexpr static int            B   = 0;
    constexpr static int            C   = 0;
    Ccy                      const  m_A;
    Ccy                      const  m_B;
    Ccy                      const  m_C;

    // I:  InstrIndices:
    constexpr static int            AB  = 0;
    constexpr static int            CB  = 1;
    constexpr static int            AC  = 2;

    // S:  Side (0=Ask, 1=Bid): aka IsBid:
    constexpr static bool           Bid = true;
    constexpr static bool           Ask = false;

    // InstrNames as a vector (0=AB, 1=CB, 2=AC):
    std::vector<std::string> const  m_instrNames;

    //-----------------------------------------------------------------------//
    // Infrastructure: SecDefsMgr, RiskMgr, Connectors:                      //
    //-----------------------------------------------------------------------//
    // XXX:
    // Reactor and Connectors are currently contained INSIDE the "TriArbC0"
    // class. This may lead to some inefficiencies if multiple instances of
    // this Strategy run on the same host:
    //
    SecDefsMgr*              const  m_secDefsMgr;     // Allocated on ShM
    RiskMgr*                 const  m_riskMgr;        // Allocated on ShM
    AssetRisks const*        const  m_assetRisks[3];  // AssetRisks for [A,B,C]

    MDC*                     const  m_mdc;            // Ptr is OWNED
    OMC*                     const  m_omc;            // Ptr us OWNED

    //-----------------------------------------------------------------------//
    // Instruments and OrderBooks:                                           //
    //-----------------------------------------------------------------------//
    // SecDefs and OrderBooks for instrs traded:   [I: 0=AB, 1=CB, 2=AC]:
    //                                              I
    SecDefD   const*         const  m_instrs       [3];
    OrderBook const*         const  m_orderBooks   [3];

    //-----------------------------------------------------------------------//
    // Quant and Related Params:                                             //
    //-----------------------------------------------------------------------//
    bool                     const  m_dryRun;      // Orders not really placed

    // Relative Quoting Mark-Ups (epsilon). XXX: currently same for Bid and Ask
    // sides. TODO: "epsilon" should depend on the current volatility. For this
    // reason, relative mark-ups are better than those expressed in PxSteps(?).
    // Typical mark-up is around 1 bp (0.0001); "I" is the passive instr idx:
    //                                              I
    double                   const  m_relMarkUps   [3];

    // Qtys to be passively quoted, for each "I":
    QtyN                     const  m_qtys         [3];

    // Qty Reserve Factors (normally > 1) applied to AggrQtys used in VWAP com-
    // putations, for all Instrs:
    double                   const  m_resrvFactors [3];

    // Params preventing unnecessary order modifications; "m_resist*" vals are
    // in PxSteps:
    // Depth is the min(CurrDepth, NewDepth):
    // (*) Depth <= 0:                move (if AbsDiff >= 1)
    // (*) 1 <= Depth < ResistDepth1: move iff AbsDiff >= ResistDiff1 >= 1
    // (*) ResistDepth1 <= Depth    : move iff AbsDiff >= ResistDiff2 >= 1
    //                                              I
    int                      const  m_resistDiffs1 [3];
    int                      const  m_resistDepths1[3];
    int                      const  m_resistDiffs2 [3];

    // Maximum number of Rounds (Triangles) to be completed: also limited by
    // the max allocated space for "TriInfo"s ("m_max3s"):
    int                      const  m_maxRounds;

    // GraceTime (in sec) allowed to received all OrderExec Reports if Delayed-
    // GracefulStop was requested:
    constexpr static int            GracefulStopTimeSec = 3;

    //-----------------------------------------------------------------------//
    // Infrastructure Statuses:                                              //
    //-----------------------------------------------------------------------//
    mutable bool                    m_allConnsActive;
    mutable bool                    m_isInDtor;     // To prevent ExitRun throw
    mutable int                     m_roundsCount;  // #Triangles completed
    mutable int                     m_signalsCount;

    //-----------------------------------------------------------------------//
    // TriAngles:                                                            //
    //-----------------------------------------------------------------------//
    // Pre-allocated "TrInfo" vector used to accumulate Triange Stats, in ShM,
    // using the PersistMgr:
    unsigned                  const m_confMax3s;    // Max #TriAngles from Conf
    PersistMgr<>                    m_pm;
    unsigned*                       m_Max3s;        // Max  #TriAngles in ShM
    unsigned*                       m_N3s;          // Curr #TriAngles in ShM
    TrInfo*                         m_All3s;        // All   TriAngles in ShM

    // Ptrs to currently-active Triangles (acltually located in "m_3s" in ShM):
    // "I" is the Passive instr idx, "S=IsBid" is side:
    //                                            I  S
    mutable TrInfo*                 m_curr3s     [3][2];

    //=======================================================================//
    // Non-Default Ctor, Dtor:                                               //
    //=======================================================================//
    // Default Ctor is deleted:
    TriArbC0() = delete;

  public:
    TriArbC0
    (
      EPollReactor  *              a_reactor,
      spdlog::logger*              a_logger,
      boost::property_tree::ptree* a_params   // XXX: Mutable via non-NULL ptr
    );

    ~TriArbC0() noexcept override;

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
      utxx::time_val            a_ts_conn,
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
      utxx::time_val            a_ts_conn
    )
    override;

    void OnOrderBookUpdate
    (
      OrderBook const&          a_ob,
      bool                      a_is_error,
      OrderBook::UpdatedSidesT  a_sides,
      utxx::time_val            a_ts_exch,
      utxx::time_val            a_ts_conn,
      utxx::time_val            a_ts_strat
    )
    override;

    // NB: "OnTradeUpdate" is currently not overridden -- NoOp default is used

    void OnOurTrade   (Trade const& a_tr)  override;
    void OnConfirm    (Req12 const& a_req) override;

    void OnCancel
    (
      AOS   const&              a_aos,
      utxx::time_val            a_ts_exch,
      utxx::time_val            a_ts_conn
    )
    override;

    void OnOrderError
    (
      Req12 const&              a_req,
      int                       a_err_code,
      char  const*              a_err_text,
      bool                      a_prob_filled,
      utxx::time_val            a_ts_exch,
      utxx::time_val            a_ts_conn
    )
    override;

    void OnSignal(signalfd_siginfo const& a_si)  override;

  private:
    //=======================================================================//
    // Internal Methods:                                                     //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "OrderCanceledOrFailed": Common impl of "OnCancel" and "OnError":     //
    //-----------------------------------------------------------------------//
    void OrderCanceledOrFailed
    (
      AOS const&      a_aos,
      char const*     a_msg,      // NULL iff Cancelled
      utxx::time_val  a_ts_exch,
      utxx::time_val  a_ts_conn,
      utxx::time_val  a_ts_strat
    );

    //-----------------------------------------------------------------------//
    // "NewTrInfo":                                                          //
    //-----------------------------------------------------------------------//
    // Allocates a new "TrInfo" from a pre-allocated contiguous array in ShM
    // ("m_All3s)", initialises it and returns a ptr to it.  May return NULL
    // if there is no more space available:
    //
    template<int I, int S>
    TrInfo* NewTrInfo
    (
      utxx::time_val  a_ts_exch,
      utxx::time_val  a_ts_conn,
      utxx::time_val  a_ts_strat
    );

    //-----------------------------------------------------------------------//
    // "MkSynthPx*":                                                         //
    //-----------------------------------------------------------------------//
    // NB: The Px returned takes into account reserves for the covering (aggr)
    // legs, but NOT mark-up or rounding to a multiple of a PxStep yet  (the
    // remaining adjustments are done by "Quote"). The src data are store in
    // the "TrInfo":
    // TODO: HERE we should use continuous price prediction of aggr legs' VWAPs
    // rather than actually-observed ones:
    //
    template<bool IsBid> double MkSynthPxAB(TrInfo* a_tri) const;
    template<bool IsBid> double MkSynthPxCB(TrInfo* a_tri) const;
    template<bool IsBid> double MkSynthPxAC(TrInfo* a_tri) const;

    //-----------------------------------------------------------------------//
    // "SolveLiquidityEqn":                                                  //
    //-----------------------------------------------------------------------//
    template<bool IsBid>
    QtyN SolveLiquidityEqn(OrderBook const* a_ob, QtyN a_b) const;

    //-----------------------------------------------------------------------//
    // "Quote":                                                              //
    //-----------------------------------------------------------------------//
    template<int I, bool IsBid>
    void Quote
    (
      utxx::time_val  a_ts_exch,
      utxx::time_val  a_ts_conn,
      utxx::time_val  a_ts_strat
    );

    //-----------------------------------------------------------------------//
    // "*CoveringOrders":                                                    //
    //-----------------------------------------------------------------------//
    // (After a passive Quote received Hit or Lift):
    //
    template<bool IsBidAB>
    void ABCoveringOrders
    (
      TrInfo*         a_tri,      // Non-NULL
      PriceT          a_px_ab,    // Normalised Px(A/B)
      QtyN            a_qty_ab,
      utxx::time_val  a_ts_exch,
      utxx::time_val  a_ts_conn,
      utxx::time_val  a_ts_strat
    );

    template<bool IsBidCB>
    void CBCoveringOrders
    (
      TrInfo*         a_tri,      // Non-NULL
      PriceT          a_px_cb,    // Normalised Px(C/B)
      QtyN            a_qty_cb,
      utxx::time_val  a_ts_exch,
      utxx::time_val  a_ts_conn,
      utxx::time_val  a_ts_strat
    );

    template<bool IsBidAC>
    void ACCoveringOrders
    (
      TrInfo*         a_tri,      // Non-NULL
      PriceT          a_px_ac,    // Normalised Px(A/C)
      QtyN            a_qty_ac,
      utxx::time_val  a_ts_exch,
      utxx::time_val  a_ts_conn,
      utxx::time_val  a_ts_strat
    );

    //-----------------------------------------------------------------------//
    // "NewOrderSafe": Exception-Safe Wrapper:                               //
    //-----------------------------------------------------------------------//
    // AggrJ: (-1): PassiveLeg; 0: AggrClose; 1,2: AggrLegs:
    //
    template<int I, bool IsBuy, int AggrJ>
    void NewOrderSafe
    (
      TrInfo*         a_tri,
      PriceT          a_px,    // Contract (Instriment) Px, NOT Px(A/B)
      QtyN            a_qty,
      utxx::time_val  a_ts_exch,
      utxx::time_val  a_ts_conn,
      utxx::time_val  a_ts_strat,
      char const*     a_comment
    );

    //-----------------------------------------------------------------------//
    // "ModifyOrderSafe": Exception-Safe Wrapper:                            //
    //-----------------------------------------------------------------------//
    // OMC is a template param, to avoid virtual calls:
    //
    bool ModifyOrderSafe
    (
      AOS const*      a_aos,   // Non-NULL (unless DryRun)
      PriceT          a_px,    // Contract (Instrument) Px, NOT Px(A/B)
      QtyN            a_qty,
      utxx::time_val  a_ts_exch,
      utxx::time_val  a_ts_conn,
      utxx::time_val  a_ts_strat
    );

    //-----------------------------------------------------------------------//
    // "CancelOrderSafe": Exception-Safe Wrapper:                            //
    //-----------------------------------------------------------------------//
    // OMC is a template param, to avoid virtual calls:
    // If the order is already Inactive or PendingCancel, it's perfectly OK, so
    // return "true"; "false" is returned only in case of an exception caught:
    //
    bool CancelOrderSafe
    (
      AOS const*      a_aos,   // Non-NULL (unless DryRun)
      utxx::time_val  a_ts_exch,
      utxx::time_val  a_ts_conn,
      utxx::time_val  a_ts_strat,
      char const*     a_comment
    );

    //-----------------------------------------------------------------------//
    // "CheckAllConnectors":                                                 //
    //-----------------------------------------------------------------------//
    // Enables the "steady operating mode" when all Connectors and all Order-
    // Books become available:
    void CheckAllConnectors();
  };

  //=========================================================================//
  // "TriArbC0" Instance Types:                                              //
  //=========================================================================//
  using TriArbC0_BinanceSpot = TriArbC0<ExchC0T::BinanceSpot>;
  using TriArbC0_BitMEX      = TriArbC0<ExchC0T::BitMEX>;

} // End namespace MAQUETTE
