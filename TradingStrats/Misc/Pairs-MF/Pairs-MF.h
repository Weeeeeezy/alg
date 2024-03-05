// vim:ts=2:et
//===========================================================================//
//                  "TradingStrats/Misc/Pairs-MF/Pairs-MF.h":                //
//       Pair Trading on MICEX and FORTS (and on Some Other Exchanges)       //
//===========================================================================//
#pragma once

#include "Basis/Maths.hpp"
#include "Connectors/FAST/EConnector_FAST_MICEX.hpp"
#include "Connectors/FAST/EConnector_FAST_FORTS.h"
#include "Connectors/FIX/EConnector_FIX.h"
#include "Connectors/TWIME/EConnector_TWIME_FORTS.h"
#include "Connectors/ITCH/EConnector_ITCH_HotSpotFX.h"
#include "InfraStruct/Strategy.hpp"
#include <utxx/config.h>
#include <utxx/enumv.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/container/static_vector.hpp>
#include <string>
#include <utility>
#include <map>

namespace MAQUETTE
{
  //=========================================================================//
  // "Pairs_MF" Class:                                                       //
  //=========================================================================//
  class Pairs_MF final: public Strategy
  {
  private:
    //=======================================================================//
    // Consts:                                                               //
    //=======================================================================//
    // S:  Side (0=Ask, 1=Bid): aka IsBid:
    constexpr static bool     Ask = false;
    constexpr static bool     Bid = true;

    // XXX: FracQtys are NOT used throughout this Strategy!
    constexpr static QtyTypeT QTU = QtyTypeT::UNDEFINED;
    using                     QR  = long;

    //=======================================================================//
    // "OrderInfo" Struct:                                                   //
    //=======================================================================//
    struct OrderInfo
    {
      //---------------------------------------------------------------------//
      // Data Flds:                                                          //
      //---------------------------------------------------------------------//
      // (NB: The strange order of flds is for compactness -- this struct must
      // fit in 64 bytes, it is stored as UserData in AOS):
      //
      OrderID   m_refAOSID;       // For Aggr only -- the corr PassAOSID
      PriceT    m_expPassPx;      // Exp Fill Px if Passive (NaN otherwise)
      PriceT    m_expAggrPxLast;  // Exp Fill Px for the Covering Order
      PriceT    m_expAggrPxNew;   // as above, but may not be acted upon
      double    m_passSlip;
      short     m_aggrLotsNew;    // Aggr Lots used in m_expAggrPxNew
      short     m_iPairID;
      bool      m_isAggr;         // Placed here for compactness

      //---------------------------------------------------------------------//
      // Ctors:                                                              //
      //---------------------------------------------------------------------//
      // Default Ctor:
      //
      OrderInfo() { memset(this, '\0', sizeof(OrderInfo)); }

      OrderInfo
      (
        bool            a_is_aggr,
        OrderID         a_ref_aosid,
        short           a_ipair_id,
        PriceT          a_exp_pass_px,
        PriceT          a_exp_aggr_px_last,
        PriceT          a_exp_aggr_px_new,
        short           a_aggr_lots_new
      )
      : m_refAOSID      (a_ref_aosid),
        m_expPassPx     (a_exp_pass_px),
        m_expAggrPxLast (a_exp_aggr_px_last),
        m_expAggrPxNew  (a_exp_aggr_px_new),
        m_passSlip      (0.0),
        m_aggrLotsNew   (a_aggr_lots_new),
        m_iPairID       (a_ipair_id),
        m_isAggr        (a_is_aggr)
      {
        static_assert
          (sizeof(OrderInfo) <= sizeof(UserData), "OrderInfo is too large");

        // XXX: Also, normally, (m_isAggr == (m_refAOSID != 0)), but this does
        // NOT apply to the case of making aggressive adjustment to  a  skewed
        // hedge (in this case, there is no direct PassAOS to refer to), so we
        // cannot assert that cond...
      }
    };

    //=======================================================================//
    // "PassModeT":                                                          //
    //=======================================================================//
    // Quoting Mode for Passive Orders:
    // (*) Normally, we place passive quotes continously (upon each update of
    //     the VWAP on the Aggr Side);
    // (*) However, there is a special CAESAR mode which monitors "wrong" quo-
    //     tes placed by others on the PassSide, and executes our special ord-
    //     ers against them:
    //
    UTXX_ENUMV(
    PassModeT, (int, 0),
      (Normal,       1)
      (CAESAR,       2)
    );

    //=======================================================================//
    // "TargAdjModeT":                                                       //
    //=======================================================================//
    // Specifies how the Passive Quotes are adjusted to the Target OrderBook.
    // Other adjustments, eg Skewing  based on our position or px prediction,
    // are specified separately:
    //
    UTXX_ENUMV(
    TargAdjModeT, (int, -1),
      // No Adjustment: Ie if the resulting price will aggress the Target OB,
      // it's OK. Most suitable for low-liquidity Targets:
      (None,             0)

      // Our Quoted Px is not more narrow than the corresp L1 Px of the Target
      // OB. So if a "too optimistic" Quote Px was computed, it is always adj-
      // usted to the Target's L1, or even slightly beyond  (depending  on how
      // much liquidity is at  L1 -- if not much, we can make a better px from
      // our own perspective w/o sacrificing the Fill Probability):
      (ToL1Px,           1)

      // Our Quoted Px may be made slightly more narrow (by 1 Px Step) than the
      // corresp Target L1 Px, to improve the Fill Probability:
      (ImprFillRates,    2)
    );

    //=======================================================================//
    // "AggrModeT":                                                          //
    //=======================================================================//
    // Execution Mode for Aggressive Orders (therefore, they are not always re-
    // ally aggressive -- that only happens for AggrModeT::DeepAggr!):
    //
    UTXX_ENUMV(
    AggrModeT, (int, 0),
      (DeepAggr,     1)   // Making really aggressive orders
      (Pegged,       2)   // Peg the order to the opp side for passive exec
      (FixedPass,    3)   // Use a fixed (normally passive) px
    );

    //=======================================================================//
    // "IPair": Pair of Instruments:                                         //
    //=======================================================================//
    struct IPair
    {
      //=====================================================================//
      // Data Flds:                                                          //
      //=====================================================================//
      Pairs_MF*               m_outer;
      spdlog::logger*         m_logger;
      int                     m_debugLevel;
      utxx::time_val          m_enabledUntil; // If empty, then disabled at all!

      //---------------------------------------------------------------------//
      // Passive Leg:                                                        //
      //---------------------------------------------------------------------//
      // MDC and OMC for "PassInstr":
      // The most common situation is when MDC is FAST_MICEX_FX or FAST_MICEX_
      // EQ, and OMC is FIX_MICEX (same type for FX and EQ; only configs would
      // be different in that case):
      //
      EConnector_MktData*     m_passMDC;
      EConnector_OrdMgmt*     m_passOMC;

      // "PassInstr": Passively-Quoted Instr, normally on MICEX (FX or EQ), but
      // could be on FORTS as well (if the IPair consists of 2 Futures):
      SecDefD const*          m_passInstr;

      // OrderBook for "PassInstr":
      OrderBook const*        m_passOB;

      // Quoted Qty of "PassInstr":
      long                    m_passQty;

      // Passive Quoting Mode:
      PassModeT               m_passMode;

      // AOSes for Passively-Quoted Instrs: As above, 0 = Ask, 1 = Bid:
      mutable AOS const*      m_passAOSes[2];

      // Position wrt the Passive Instr. It is also available from the RiskMgr,
      // but it is more convenient to keep it here as well:
      mutable long            m_passPos;

      // Soft pos limit (both sides):
      long                    m_passPosSoftLimit;

      // Last Passive Fill time (used to prevent immediate re-quoting, which is
      // shown by experience to cause Passive Slippage):
      mutable utxx::time_val  m_passLastFillTS;

      // Minimum delay (in milliseconds) for a new quote after a Fill:
      int const               m_reQuoteDelayMSec;

      //---------------------------------------------------------------------//
      // Spread Base:                                                        //
      //---------------------------------------------------------------------//
      // It can be given by:
      // (*) a filtered (LPF) mid-point-spread; used if no Swap Instr is given;
      // (*) or by some Swap Instrument:
      //
      // Coeff for calculating EMA(PassPx-AggrPx): FIXME: There are better fil-
      // ters than EMA (eg JMA, Kalman Filter):
      // [0]: coeff @ LastTick
      // [1]: coeff @ PrevEMA (1s complement) :
      double                  m_sprdEMACoeffs [2];

      // The EMA itself (computed using mid-point PassivePx, and the mid-point
      // AggrPx adjusted by AggrQtyFact):
      mutable double          m_sprdEMA;

      // Swap Instrument and its Connector and OrderBook:
      EConnector_MktData*     m_swapMDC;
      SecDefD   const*        m_swapInstr;
      OrderBook const*        m_swapOB;

      // Qty Factor for Swaps (wrt the Passive Leg; normally just 1):
      double                  m_swapQtyFact;

      //---------------------------------------------------------------------//
      // Aggressive Leg:                                                     //
      //---------------------------------------------------------------------//
      // MDC and OMC for "AggrInstr":
      // The most common situation is when MDC is FAST_FORTS and OMC is TWIME_
      // FORTS (same for Fut and Opt):
      EConnector_MktData*     m_aggrMDC;
      EConnector_OrdMgmt*     m_aggrOMC;

      // "AggrInstr": Aggressively-Traded Instr (normally on FORTS, though it
      // may be on MICEX if the IPair consists of 2 Spots, eg TOD and TOM):
      SecDefD const*          m_aggrInstr;

      // OrderBook for "AggrInstr":
      OrderBook const*        m_aggrOB;

      // When a Passive Order is Hit or Lifted, we issue a Covering Aggr  Order
      // -- but the latter's Qty may involve a Scale Factor wrt the  Filled Qty
      // of the PassInstr:  Eg for 1 USD/RUB Spot contract (not Lot!), 0.001 Si
      // Futures contracts; ie for 1 USD/RUB Lot (1000 contracts), 1 Si Futures:
      double                  m_aggrQtyFact;

      // A "reserve factor" used in VWAP computations:
      double                  m_aggrQtyReserve;

      // We can then compute the nominal size of the Aggr Order which corresps
      // to the full "m_passQty" above, incl the Reserve. It is an  INDICATIVE
      // qty only:
      long                    m_aggrNomQty;

      // How we execute the Aggressive Leg orders -- actually, out of the 3 pos-
      // sible modes, only 1 is really aggressive!
      AggrModeT               m_aggrMode;

      // Position wrt the Aggr Instr. Normally it is a negated PassPos, unless
      // the corresp aggr covering orders are not yet filled (still flying):
      mutable long            m_aggrPos;

      //---------------------------------------------------------------------//
      // Quant Analytics:                                                    //
      //---------------------------------------------------------------------//
      // Coeffs used to calculate the Passive Quote Px Adjustments:
      // [0]: Mark-Up
      // [1]: Coeff at "m_passPos" (to adjust for the curr pos):
      double                  m_adjCoeffs     [2];

      // The actual (adjusted) Quote Pxs for the Passive Instr:
      mutable PriceT          m_quotePxs      [2];

      // Expected Aggressive (Cover) Pxs -- also stored in AOS UserData; here
      // only stored to be used with IntendPxs (see above) when the Order can
      // NOT be sent immediately:
      mutable PriceT          m_expCoverPxs   [2];

      // How (if at all) we adjust our Quote Pxs to the Target OrderBook:
      TargAdjModeT            m_targAdjMode;

      // Quote Price Moving Resistance Coeff (TODO: make it dependent on the
      // curr tick vol as well):
      double                  m_resistCoeff;

      // Extra Mark-Up to be applied to Non-Deep Aggr Orders (NOT used in Pass
      // Pxs computation, but applied once a PassFill was achieved):
      double                  m_extraMarkUp;

      // Stop-Loss (in PxPts per 1 Unit) for Non-Deep Aggressive Orders  (if
      // breached, the order becomes Fully-Aggressive):
      double                  m_aggrStopLoss;

      // Tick Counters:
      // The number of "periods" corresp to "m_sprdEMACoeffs[0]":
      int                     m_nSprdPeriods;
      // The number of actual "ticks" since the beginning (when we started tra-
      // cking the EMA):
      mutable int             m_currSprdTicks;

      // "Dead Zone" for passive quoting (where rivals are likley lurking): If
      // our prospective quote falls into that zone, we cancel it):
      long                    m_dzLotsFrom;
      long                    m_dzLotsTo;
      long                    m_dzAggrQty;

      // The method of computing the PassiveQtys: either Normal, or FlipFlop
      // (for position alternation):
      bool                    m_useFlipFlop;

      //---------------------------------------------------------------------//
      // Large vector -- better be placed at the end:                        //
      //---------------------------------------------------------------------//
      // Vector of Aggressive AOSes -- there could be more than 2 of them, bec-
      // ause we could re-quote the PassInstr even before the corresp  AggrOrd
      // is filled. Also, those orders may be Non-Deep (so not really  Aggr!):
      //
      using   AOSPtrsVec =    boost::container::static_vector<AOS const*, 256>;
      mutable AOSPtrsVec      m_aggrAOSes;
      utxx::time_val          m_lastAggrPosAdj;

      //=====================================================================//
      // Methods:                                                            //
      //=====================================================================//
      //---------------------------------------------------------------------//
      // Ctors, Dtor:                                                        //
      //---------------------------------------------------------------------//
      // Default Ctor:
      IPair();

      // Non-Default Ctor:
      // "a_params" is the corresp Section of the over-all Config:
      // "a_outer"  is a "Pairs_MF" obj which this "IPair" belongs to:
      IPair
      (
        Pairs_MF*                    a_outer,
        boost::property_tree::ptree& a_params // XXX: Mutable!
      );

      // Copy Ctor is auto-generated: All ptrs in "IPair" are not owned, so it
      // is OK to do shallow copy.
      // The  Dtor is also auto-generated: no need to de-allocate any memory...

      // But Equality needs to be defined explicitly:
      bool operator==(IPair const&) const;

      //---------------------------------------------------------------------//
      // "DoQuotes":                                                         //
      //---------------------------------------------------------------------//
      // Calculates the Pxs (both Bid and Ask) for the Passive Instr, and poss-
      // ibly inserts or modifies the corresp Quotes:
      //
      void DoQuotes
      (
        utxx::time_val    a_ts_exch,
        utxx::time_val    a_ts_recv,
        utxx::time_val    a_ts_strat
      );

      //---------------------------------------------------------------------//
      // "CancelQuotes":                                                     //
      //---------------------------------------------------------------------//
      // Cancel any active quotes on the Passive Side of this IPair:
      //
      void CancelQuotes
      (
        utxx::time_val    a_ts_exch,
        utxx::time_val    a_ts_recv,
        utxx::time_val    a_ts_strat
      );

      //---------------------------------------------------------------------//
      // "DoCoveringOrder":                                                  //
      //---------------------------------------------------------------------//
      void DoCoveringOrder
      (
        AOS const*        a_pass_aos,
        long              a_pass_qty,
        utxx::time_val    a_ts_exch,
        utxx::time_val    a_ts_comm,
        utxx::time_val    a_ts_strat
      );

      //---------------------------------------------------------------------//
      // Exception-Safe Order Mgmt:                                          //
      //---------------------------------------------------------------------//
      //---------------------------------------------------------------------//
      // "NewOrderSafe":                                                     //
      //---------------------------------------------------------------------//
      // For both Passive and Aggressive orders. See the impl for the semantics
      // of "a_px{123}" and "a_pass_slip":
      //
      template<bool IsAggr, bool Buffered>
      AOS const* NewOrderSafe
      (
        bool              a_is_bid,
        OrderID           a_ref_aosid,
        PriceT            a_px1,
        PriceT            a_px2,
        PriceT            a_px3,
        double            a_pass_slip,
        short             a_aggr_lots,
        long              a_qty,
        utxx::time_val    a_ts_exch,
        utxx::time_val    a_ts_recv,
        utxx::time_val    a_ts_strat
      );

      //---------------------------------------------------------------------//
      // "CancelOrderSafe":                                                  //
      //---------------------------------------------------------------------//
      // Applicable to Passive and Non-Deep Aggressive Orders:
      //
      template<bool Buffered>
      void CancelOrderSafe
      (
        AOS const*        a_aos,        // Non-NULL
        utxx::time_val    a_ts_exch,
        utxx::time_val    a_ts_recv,
        utxx::time_val    a_ts_strat
      );

      //---------------------------------------------------------------------//
      // "ModifyQuoteSafe":                                                  //
      //---------------------------------------------------------------------//
      // Applicable to Passive Quotes only (Non-Deep Aggr Orders  are handled by
      // "EvalStopLoss" below). Returns "true" iff the modification was actually
      // carried out (Buffered or otherwise):
      //
      template<bool Buffered>
      bool ModifyQuoteSafe
      (
        bool              a_is_bid,
        PriceT            a_pass_px1, // Always set -- this is a Passive order!
        PriceT            a_pass_px2, // As above, but UnAdjusted
        PriceT            a_aggr_px,  // Expected Cover Px
        short             a_aggr_lots,
        long              a_pass_qty,
        utxx::time_val    a_ts_exch,
        utxx::time_val    a_ts_recv,
        utxx::time_val    a_ts_strat
      );

      //---------------------------------------------------------------------//
      // "ModifyPeggedOrder":                                                //
      //---------------------------------------------------------------------//
      // Similar to "ModifyQuoteSafe" above, but for Aggressive (actually Peg-
      // ged) Orders. Always Buffered:
      //
      void ModifyPeggedOrder
      (
        AOS const*        a_aos,      // Non-NULL
        OrderBook const&  a_ob,
        utxx::time_val    a_ts_exch,
        utxx::time_val    a_ts_recv,
        utxx::time_val    a_ts_strat
      );

      //---------------------------------------------------------------------//
      // "EvalStopLoss":                                                     //
      //---------------------------------------------------------------------//
      // Evaluates stop-loss conds on a Non-Deep Aggressive Order, and makes it
      // Deeply-Aggressive if the negative threshoild is exceeded.
      // Again, returns "true" iff the Order modification   (into a Deeply-Aggr
      // one) has indeed been carried out:
      //
      bool EvalStopLoss
      (
        OrderBook const&  a_ob,
        AOS const*        a_aos,
        utxx::time_val    a_ts_exch,
        utxx::time_val    a_ts_recv,
        utxx::time_val    a_ts_strat
      );

      //---------------------------------------------------------------------//
      // Misc Methods:                                                       //
      //---------------------------------------------------------------------//
      PriceT MkDeepAggrPx    (bool a_is_bid)                    const;
      bool   UpdateSprdEMA   ()                                 const;

      template<bool StopAtOurs>
      long  GetOthersLotsUpTo(bool  a_is_bid, PriceT a_upto_px) const;

      void  RemoveInactiveAggrOrder(AOS const* a_aos, char const* a_where);
    };

    //=======================================================================//
    // "DelayedGracefulStop":                                                //
    //=======================================================================//
    class DelayedGracefulStop
    {
    private:
      Pairs_MF*           m_outer;
      std::stringstream   m_msg;

      void Out();
      template<typename T, typename... Args> void Out(T const&, Args&&...);

    public:
      template<typename... Args>
      DelayedGracefulStop
      (
        Pairs_MF*         a_outer,
        utxx::time_val    a_ts_strat,
        Args&&... args
      );
    };

    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // Infrastructure: Reactor, Loggers, RiskMgr, Connectors:                //
    //-----------------------------------------------------------------------//
    // XXX: Reactor and Connectors are contained INSIDE the "Pairs_MF" class.
    // This is OK provided that we only have 1 Strategy  running at any time;
    // for a multi-Strategy platform, a generic Strategy Adaptor would  prob-
    // ably be required:
    bool const                      m_dryRun;           // Do not place Orders!
    SecDefsMgr*                     m_secDefsMgr;
    RiskMgr*                        m_riskMgr;          // Allocated on ShM

    std::shared_ptr<spdlog::logger> m_mktDataLoggerShP;
    spdlog::logger*                 m_mktDataLogger;    // May be NULL
    int const                       m_mktDataDepth;
    mutable char                    m_mktDataBuff[8192];

    // NB: Some of these Connectors can remain NULL if not used:
    // MDCs:
    EConnector_FAST_MICEX_EQ*       m_mdcMICEX_EQ;
    EConnector_FAST_MICEX_FX*       m_mdcMICEX_FX;
    EConnector_FAST_FORTS_FOL*      m_mdcFORTS;         // Same for Fut&Opt
    EConnector_FIX_AlfaFIX2*        m_mdcAlfa;
    EConnector_FIX_NTProgress*      m_mdcNTPro;
    EConnector_ITCH_HotSpotFX_Gen*  m_mdcHSFX_Gen;
    EConnector_ITCH_HotSpotFX_Link* m_mdcCCMA;          // CCM Alpha over HSFX
    // OMCs:
    EConnector_FIX_MICEX*           m_omcMICEX_EQ;
    EConnector_FIX_MICEX*           m_omcMICEX_FX;
    EConnector_TWIME_FORTS*         m_omcFORTS;         // Same for Fut&Opt
    EConnector_FIX_AlfaFIX2*        m_omcAlfa;
    EConnector_FIX_NTProgress*      m_omcNTPro;
    EConnector_FIX_HotSpotFX_Gen*   m_omcHSFX_Gen;
    EConnector_FIX_HotSpotFX_Link*  m_omcCCMA;          // CCM Alpha over HSFX

    //-----------------------------------------------------------------------//
    // Infrastructure Params and Statuses:                                   //
    //-----------------------------------------------------------------------//
    // StatusLineFile used to output the error msg to be displayed in the GUI:
    std::string                     m_statusLineFile;

    // Maximum number of Rounds, ie Quotes to be filled: usually +oo, but may be
    // limited for testing/debugging purposes:
    int                             m_maxRounds;
    mutable int                     m_roundsCount;  // <= m_maxRounds
    mutable bool                    m_allConnsActive;
    mutable int                     m_signalsCount;

    // Schedule of FORTS trading (whereas MICEX is continuous): 3 continuous
    // sessions (with 2 clearing gaps in between):
    constexpr static int            NSess = 3;
    //                                             Sess  From,To
    utxx::time_val                  m_tradingSched[NSess][2];

    //-----------------------------------------------------------------------//
    // "IPair"s:                                                             //
    //-----------------------------------------------------------------------//
    // At most "MaxPairs" pairs can be traded simultaneously:
    constexpr static int            MaxPairs = 8;
    using   IPairsVec = boost::container::static_vector<IPair, MaxPairs>;
    mutable IPairsVec               m_iPairs;

    // XXX: Fixed Asset Valuation Rates (Asset->USD conversion rates): Only used
    // at start-up time, so not performance-critical:
    std::map<SymKey, double>        m_fixedRates;

  public:
    // Default Ctor is deleted:
    Pairs_MF() = delete;

    //=======================================================================//
    // Non-Default Ctor, Dtor:                                               //
    //=======================================================================//
    Pairs_MF
    (
      EPollReactor  *               a_reactor,
      spdlog::logger*               a_logger,
      boost::property_tree::ptree&  a_params   // XXX: Mutable!
    );

    ~Pairs_MF() noexcept override;

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

    void OnTradeUpdate(Trade const& a_tr) override;
    void OnOurTrade   (Trade const& a_tr) override;

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

    void OnSignal   (signalfd_siginfo const& a_si)  override;

  private:
    //=======================================================================//
    // Internal Methods:                                                     //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "Get{MDC,OMC,OrderDetails}":                                          //
    //-----------------------------------------------------------------------//
    // Returns a ptr to MDC or OMC specified by its Params.ini Secton Name:
    //
    EConnector_MktData*   GetMDC(std::string   const& a_conn_section) const;
    EConnector_OrdMgmt*   GetOMC(std::string   const& a_conn_section) const;

    std::pair<OrderInfo*, IPair*> GetOrderDetails
    (
      AOS const*      a_aos,
      utxx::time_val  a_ts_strat,
      char const*     a_where
    );

    //-----------------------------------------------------------------------//
    // "CheckAllConnectors":                                                 //
    //-----------------------------------------------------------------------//
    void CheckAllConnectors(utxx::time_val a_ts_strat);

    //-----------------------------------------------------------------------//
    // "EvalStopConds":                                                      //
    //-----------------------------------------------------------------------//
    // Returns "true" iff the time is high to stop now:
    //
    bool EvalStopConds
    (
      utxx::time_val  a_ts_exch,
      utxx::time_val  a_ts_recv,
      utxx::time_val  a_ts_strat
    );

    //-----------------------------------------------------------------------//
    // "CancelAllQuotes":                                                    //
    //-----------------------------------------------------------------------//
    void CancelAllQuotes
    (
      EConnector_MktData const* a_mdc,      // If NULL, then for all MDCs
      utxx::time_val            a_ts_exch,
      utxx::time_val            a_ts_recv,
      utxx::time_val            a_ts_strat
    );

    //-----------------------------------------------------------------------//
    // "IsStopping" (so no further Quotes etc):                              //
    //-----------------------------------------------------------------------//
    bool IsStopping() const
      { return (!m_delayedStopInited.empty() || m_nowStopInited); }

    //-----------------------------------------------------------------------//
    // MktData Logging:                                                      //
    //-----------------------------------------------------------------------//
    void LogOrderBook   (OrderBook const& a_obi, long a_lat_ns) const;
    void LogTrade       (Trade     const& a_tr)  const;

    void LogOurTrade
    (
      char const*       a_symbol,
      bool              a_is_bid,
      PriceT            a_px,
      long              a_qty_lots,
      ExchOrdID const&  a_exec_id
    )
    const;
  };
} // End namespace MAQUETTE
