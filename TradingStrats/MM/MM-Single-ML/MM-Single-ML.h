// vim:ts=2:et
//===========================================================================//
//            "TradingStrats/MM-Single-ML/MM-Single-ML.h":                 //
//       Pair Trading on MICEX and FORTS (and on Some Other Exchanges)       //
//===========================================================================//
#pragma once

#include "Basis/Maths.hpp"
#include "InfraStruct/Strategy.hpp"
#include <utxx/config.h>
#include <utxx/enumv.hpp>
#include <boost/container/static_vector.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <string>
#include <utility>
#include <map>
#include "Connectors/H2WS/BitFinex/EConnector_WS_BitFinex_MDC.h"
#include "Connectors/H2WS/BitFinex/EConnector_WS_BitFinex_OMC.h"
#include "Connectors/H2WS/Binance/EConnector_H2WS_Binance_MDC.h"
#include "Connectors/H2WS/Binance/EConnector_H2WS_Binance_OMC.h"

namespace MAQUETTE
{
  //=========================================================================//
  // "MM_Single_ML" Class:                                                  //
  //=========================================================================//
  template<class MDC, class OMC>
  class MM_Single_ML final : public Strategy {
  private:
    constexpr static QtyTypeT QT = OMC::QT;
    using QR                     = typename OMC::QR;
    using QtyN                   = Qty<QT, QR>;
    //=======================================================================//
    // Consts:                                                               //
    //=======================================================================//
    // S:  Side (0=Ask, 1=Bid): aka IsBid:
    constexpr static bool Ask = false;
    constexpr static bool Bid = true;

    // Max orders per side
    constexpr static int MaxOrdersPerSide = 10;

    //=======================================================================//
    // "OrderInfo" Struct:                                                   //
    //=======================================================================//
    struct OrderInfo {
      //---------------------------------------------------------------------//
      // Data Flds: //
      //---------------------------------------------------------------------//
      OrderID m_refAOSID;  // For Aggr only -- the corr PassAOSID
      PriceT  m_expPx;
      QR      m_Slip;
      short   m_iPairID;

      //---------------------------------------------------------------------//
      // Ctors: //
      //---------------------------------------------------------------------//
      // Default Ctor:
      //
      OrderInfo() { memset(this, '\0', sizeof(OrderInfo)); }

      OrderInfo(OrderID a_ref_aosid, short a_ipair_id, PriceT a_exp_px) :
          m_refAOSID(a_ref_aosid), m_expPx(a_exp_px), m_Slip(0.0),
          m_iPairID(a_ipair_id) {
        static_assert(sizeof(OrderInfo) <= sizeof(UserData),
                      "OrderInfo is too large");
      }
    };

    //=======================================================================//
    // "TargAdjModeT":                                                       //
    //=======================================================================//
    // Specifies how the Passive Quotes are adjusted to the Target OrderBook.
    // Other adjustments, eg Skewing  based on our position or px prediction,
    // are specified separately:
    //
    UTXX_ENUMV(
        TargAdjModeT,
        (int, -1),
        // No Adjustment: Ie if the resulting price will aggress the Target OB,
        // it's OK. Most suitable for low-liquidity Targets:
        (None, 0)

        // Our Quoted Px is not more narrow than the corresp L1 Px of the Target
        // OB. So if a "too optimistic" Quote Px was computed, it is always adj-
        // usted to the Target's L1, or even slightly beyond  (depending  on how
        // much liquidity is at  L1 -- if not much, we can make a better px from
        // our own perspective w/o sacrificing the Fill Probability):
        (ToL1Px, 1)

        // Our Quoted Px may be made slightly more narrow (by 1 Px Step) than
        // the corresp Target L1 Px, to improve the Fill Probability:
        (ImprFillRates, 2));

    //=======================================================================//
    // "IPair": Pair of Instruments:                                         //
    //=======================================================================//
    struct IPair
    {
      //=====================================================================//
      // Data Flds:                                                          //
      //=====================================================================//
      MM_Single_ML*   m_outer;
      spdlog::logger* m_logger;
      int             m_debugLevel;

      int m_classPredicted;

      EConnector_MktData* m_MDC;
      EConnector_OrdMgmt* m_OMC;
      // "Instr":
      SecDefD const* m_Instr;

      // OrderBook for "Instr":
      OrderBook const* m_OB;

      // Quoted Qty of "Instr":
      QR m_Qty;

      // Volume levels for determining prices
      QR     m_volTargs[MaxOrdersPerSide] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

      // m_Qty allocation per price level (closer -> farther from midPrice)
      double m_qtyFracs[MaxOrdersPerSide] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

      int m_numOrdersPerSide;

      // AOSes: 0 = Ask, 1 = Bid:
      mutable AOS const* m_AOSes[2][MaxOrdersPerSide];

      // Position wrt the Passive Instr. It is also available from the RiskMgr,
      // but it is more convenient to keep it here as well:
      mutable QR m_Pos;

      // Soft pos limit (both sides):
      QR m_posSoftLimit;

      // Last Fill time (used to prevent immediate re-quoting, which is
      // shown by experience to cause Passive Slippage):
      mutable utxx::time_val m_lastFillTS;

      // Minimum delay (in milliseconds) for a new quote after a Fill:
      int const m_reQuoteDelayMSec;

      //---------------------------------------------------------------------//
      // Quant Analytics:                                                    //
      //---------------------------------------------------------------------//
      // Coeffs used to calculate the Passive Quote Px Adjustments:
      // [0]: Mark-Up
      // [1]: Coeff at "m_pos" (to adjust for the curr pos):
      double m_adjCoeffs[2];

      // The actual (adjusted) Quote Pxs for the Passive Instr:
      mutable PriceT m_quotePxs[2][MaxOrdersPerSide];

      // Quote Price Moving Resistance Coeff (TODO: make it dependent on the
      // curr tick vol as well):
      double m_resistCoeff;

      // Minimum spread threshold in bps:
      double m_bpsSpreadThresh;

      // Override for pxStep
      double m_pxStep;

      // Override for minSize
      double m_minSize;

      int  m_predCounter;
      bool m_ignorePrediction;

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
      // "a_outer"  is a "MM_Single_ML" obj which this "IPair" belongs to:
      IPair(MM_Single_ML*                a_outer,
            boost::property_tree::ptree& a_params  // XXX: Mutable!
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
      void DoQuotes(utxx::time_val a_ts_exch,
                    utxx::time_val a_ts_recv,
                    utxx::time_val a_ts_strat);

      //---------------------------------------------------------------------//
      // "CancelQuotes":                                                     //
      //---------------------------------------------------------------------//
      // Cancel any active quotes on the Passive Side of this IPair:
      //
      void CancelQuotes(utxx::time_val a_ts_exch,
                        utxx::time_val a_ts_recv,
                        utxx::time_val a_ts_strat);

      //---------------------------------------------------------------------//
      // Exception-Safe Order Mgmt:                                          //
      //---------------------------------------------------------------------//
      //---------------------------------------------------------------------//
      // "NewOrderSafe":                                                     //
      //---------------------------------------------------------------------//
      // For both Passive and Aggressive orders. See the impl for the semantics
      // of "a_px{123}" and "a_pass_slip":
      //
      template <bool Buffered>
      AOS const* NewOrderSafe
      (
        bool           a_is_bid,
        OrderID        a_ref_aosid,
        PriceT         a_px1,
        PriceT         a_px2,
        QR             a_slip,
        QR             a_qty,
        utxx::time_val a_ts_exch,
        utxx::time_val a_ts_recv,
        utxx::time_val a_ts_strat
      );

      //---------------------------------------------------------------------//
      // "CancelOrderSafe":                                                  //
      //---------------------------------------------------------------------//
      // Applicable to Passive and Non-Deep Aggressive Orders:
      //
      template <bool Buffered>
      void
      CancelOrderSafe(AOS const*     a_aos,       // May be NULL...
                      utxx::time_val a_ts_exch,
                      utxx::time_val a_ts_recv,
                      utxx::time_val a_ts_strat);

      //---------------------------------------------------------------------//
      // "ModifyQuoteSafe":                                                  //
      //---------------------------------------------------------------------//
      // Applicable to Passive Quotes only (Non-Deep Aggr Orders  are handled by
      // "EvalStopLoss" below). Returns "true" iff the modification was actually
      // carried out (Buffered or otherwise):
      //
      template <bool Buffered>
      bool ModifyQuoteSafe(
          bool           a_is_bid,
          int            a_idx,
          PriceT         a_px1,  // Always set -- this is a Passive order!
          PriceT         a_px2,  // As above, but UnAdjusted
          QR             a_qty,
          utxx::time_val a_ts_exch,
          utxx::time_val a_ts_recv,
          utxx::time_val a_ts_strat);

      //---------------------------------------------------------------------//
      // Misc Methods:                                                       //
      //---------------------------------------------------------------------//
      template <bool StopAtOurs>
      QR GetOthersVolsUpTo(bool a_is_bid, PriceT a_upto_px) const;

      template <bool ReturnPrevious>
      void GetPxsAtVols(bool    a_is_bid,
                        PriceT* a_prices) const;

      void DistributeQtys(QR const      a_targ_qty,
                          PriceT const* a_prices,
                          QR const      a_min_size,
                          QR*           a_qtys) const;
    };

    //=======================================================================//
    // "DelayedGracefulStop":                                                //
    //=======================================================================//
    class DelayedGracefulStop {
    private:
      MM_Single_ML*        m_outer;
      std::stringstream m_msg;

      void Out();
      template <typename T, typename... Args>
      void Out(T const&, Args&&...);

    public:
      template <typename... Args>
      DelayedGracefulStop(MM_Single_ML*     a_outer,
                          utxx::time_val a_ts_strat,
                          Args&&... args);
    };

    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // Infrastructure: Reactor, Loggers, RiskMgr, Connectors:                //
    //-----------------------------------------------------------------------//
    // XXX: Reactor and Connectors are contained INSIDE the "MM_Single_ML"
    // class. This is OK provided that we only have 1 Strategy  running at any
    // time; for a multi-Strategy platform, a generic Strategy Adaptor would
    // prob- ably be required:
    bool const  m_dryRun;  // Do not place Orders!
    SecDefsMgr* m_secDefsMgr;
    RiskMgr*    m_riskMgr;  // Allocated on ShM

    std::shared_ptr<spdlog::logger> m_mktDataLoggerShP;
    spdlog::logger*                 m_mktDataLogger;  // May be NULL
    int const                       m_mktDataDepth;
    mutable char                    m_mktDataBuff[8192];

    // NB: Some of these Connectors can remain NULL if not used:
    // MDCs:
    MDC* m_mdc;
    // OMCs:
    OMC* m_omc;

    //-----------------------------------------------------------------------//
    // Infrastructure Params and Statuses:                                   //
    //-----------------------------------------------------------------------//
    // StatusLineFile used to output the error msg to be displayed in the GUI:
    std::string   m_statusLineFile;

    // Maximum number of Rounds, ie Quotes to be filled: usually +oo, but may be
    // limited for testing/debugging purposes:
    int           m_maxRounds;
    mutable int   m_roundsCount;  // <= m_maxRounds
    mutable bool  m_allConnsActive;
    mutable int   m_signalsCount;

    //-----------------------------------------------------------------------//
    // "IPair"s:                                                             //
    //-----------------------------------------------------------------------//
    // At most "MaxPairs" pairs can be traded simultaneously:
    constexpr static int MaxPairs = 8;
    using IPairsVec = boost::container::static_vector<IPair, MaxPairs>;
    mutable IPairsVec m_iPairs;

    // XXX: Fixed Asset Valuation Rates (Asset->USD conversion rates): Only used
    // at start-up time, so not performance-critical:
    std::map<SymKey, double> m_fixedRates;

    int m_nIgnored;

  public:
    // Default Ctor is deleted:
    MM_Single_ML() = delete;

    //=======================================================================//
    // Non-Default Ctor, Dtor:                                               //
    //=======================================================================//
    MM_Single_ML(EPollReactor*                a_reactor,
              spdlog::logger*              a_logger,
              boost::property_tree::ptree& a_params  // XXX: Mutable!
    );

    ~MM_Single_ML() override;

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
    void OnNewClassPrediction
    (
      int             a_class_number,
      utxx::time_val  a_ts_exch,
      utxx::time_val  a_ts_recv,
      utxx::time_val  a_ts_handl,
      std::map<std::string, std::pair<unsigned, float*>> const*
                      a_features = nullptr
    )
    override;

    void OnTradingEvent(EConnector const& a_conn,
                        bool              a_on,
                        SecID             a_sec_id,
                        utxx::time_val    a_ts_exch,
                        utxx::time_val    a_ts_recv) override;

    void OnOrderBookUpdate(OrderBook const&         a_ob,
                           bool                     a_is_error,
                           OrderBook::UpdatedSidesT a_sides,
                           utxx::time_val           a_ts_exch,
                           utxx::time_val           a_ts_recv,
                           utxx::time_val           a_ts_strat) override;

    void OnTradeUpdate(Trade const& a_tr) override;
    void OnOurTrade   (Trade const& a_tr) override;

    void OnCancel(AOS const&     a_aos,
                  utxx::time_val a_ts_exch,
                  utxx::time_val a_ts_recv) override;

    void OnOrderError(Req12 const&   a_req,
                      int            a_err_code,
                      char const*    a_err_text,
                      bool           a_prob_filled,
                      utxx::time_val a_ts_exch,
                      utxx::time_val a_ts_recv) override;

    void OnSignal(signalfd_siginfo const& a_si) override;

  private:
    //=======================================================================//
    // Internal Methods:                                                     //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "Get{MDC,OMC,OrderDetails}":                                          //
    //-----------------------------------------------------------------------//
    // Returns a ptr to MDC or OMC specified by its Params.ini Secton Name:
    //
    EConnector_MktData* GetMDC() const;
    EConnector_OrdMgmt* GetOMC() const;

    std::pair<OrderInfo*, IPair*> GetOrderDetails(AOS const*     a_aos,
                                                  utxx::time_val a_ts_strat,
                                                  char const*    a_where);

    //-----------------------------------------------------------------------//
    // "CheckAllConnectors":                                                 //
    //-----------------------------------------------------------------------//
    void CheckAllConnectors(utxx::time_val a_ts_strat);

    //-----------------------------------------------------------------------//
    // "EvalStopConds":                                                      //
    //-----------------------------------------------------------------------//
    // Returns "true" iff the time is high to stop now:
    //
    bool EvalStopConds(utxx::time_val a_ts_exch,
                       utxx::time_val a_ts_recv,
                       utxx::time_val a_ts_strat);

    //-----------------------------------------------------------------------//
    // "CancelAllQuotes":                                                    //
    //-----------------------------------------------------------------------//
    void CancelAllQuotes(
        EConnector_MktData const* a_mdc,  // If NULL, then for all MDCs
        utxx::time_val            a_ts_exch,
        utxx::time_val            a_ts_recv,
        utxx::time_val            a_ts_strat);

    //-----------------------------------------------------------------------//
    // "IsStopping" (so no further Quotes etc):                              //
    //-----------------------------------------------------------------------//
    bool IsStopping() const {
      return (!m_delayedStopInited.empty() || m_nowStopInited);
    }

    //-----------------------------------------------------------------------//
    // MktData Logging:                                                      //
    //-----------------------------------------------------------------------//
    void LogOrderBook(OrderBook const& a_obi, long a_lat_ns) const;
    void LogTrade(Trade const& a_tr) const;

    void LogOurTrade(char const*      a_symbol,
                     bool             a_is_bid,
                     PriceT           a_px,
                     QR               a_qty_lots,
                     ExchOrdID const& a_exec_id) const;
  };
}  // End namespace MAQUETTE
