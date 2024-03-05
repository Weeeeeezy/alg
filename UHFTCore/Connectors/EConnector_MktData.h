// vim:ts=2:et
//===========================================================================//
//                     "Connectors/EConnector_MktData.h":                    //
//         Abstract Common Base for Market Data Embedded Connectors          //
//===========================================================================//
#pragma once

#include "Basis/BaseTypes.hpp"
#include "Basis/OrdMgmtTypes.hpp"
#include "Basis/TimeValUtils.hpp"
#include "Connectors/EConnector.h"
#include "Connectors/OrderBook.h"
#include "QuantSupport/HistoGram.hpp"
#include <utxx/error.hpp>
#include <spdlog/spdlog.h>
#include <boost/container/static_vector.hpp>
#include <string>
#include <utility>

namespace MAQUETTE
{
  //=========================================================================//
  // "EConnector_MktData" Class:                                             //
  //=========================================================================//
  // NB: Virtual inheritance from "EConnector":
  //
  class EConnector_MktData: virtual public EConnector
  {
  public:
    //=======================================================================//
    // Names of Persistent Objs:                                             //
    //=======================================================================//
    constexpr static char const* SocketStatsON () { return "SocketStats";  }
    constexpr static char const* ProcStatsON   () { return "ProcStats";    }
    constexpr static char const* UpdtStatsON   () { return "UpdtStats";    }
    constexpr static char const* StratStatsON  () { return "StratStats";   }
    constexpr static char const* OverAllStatsON() { return "OverAllStats"; }

    // Each of the above objs is "LatencyStats":
    using LatencyStats = QuantSupport::HistoGram<20>;

  protected:
    //=======================================================================//
    // "OBUpdateInfo" Struct:                                                //
    //=======================================================================//
    // In particular, contains timing statistics on the processing stages of an
    // OrderBook update:
    //
    struct OBUpdateInfo
    {
      //---------------------------------------------------------------------//
      // Data Flds:                                                          //
      //---------------------------------------------------------------------//
      OrderBook*                  m_ob;
      OrderBook::UpdateEffectT    m_effect;
      OrderBook::UpdatedSidesT    m_sides;

      utxx::time_val              m_exchTS;
      utxx::time_val              m_recvTS;   // When received by hardware
      utxx::time_val              m_handlTS;  // When "RecvHandler" was invoked
      utxx::time_val              m_procTS;   // When Processor     was invoked
      utxx::time_val              m_updtTS;   // When OB was actually updated

      //---------------------------------------------------------------------//
      // Ctors:                                                              //
      //---------------------------------------------------------------------//
      // Default Ctor:
      OBUpdateInfo();

      // Non-Default Ctor:
      OBUpdateInfo
      (
        OrderBook*                a_ob,
        OrderBook::UpdateEffectT  a_effect,
        OrderBook::UpdatedSidesT  a_sides,
        utxx::time_val            a_ts_exch,
        utxx::time_val            a_ts_recv,
        utxx::time_val            a_ts_handl,
        utxx::time_val            a_ts_proc,
        utxx::time_val            a_ts_updt
      );
      // Dtor is trivial, and is auto-generated...
    };

    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    // The MDC may be disabled in some cases of virtual inheritance:
    bool                const     m_isEnabled;

    // If this MDC by itself is NOT the Primary one, the following ptr is non-
    // NULL (points to the actual Primary which owns Orders and OrderBooks):
    EConnector_MktData* const     m_primaryMDC;

    // "OrderBook" objs for all configured Symbols (with Strategy Subscr).  NB:
    // do NOT use std::vector here, because we export ptrs to OrderBooks creat-
    // ed, and the latter may be re-allocated when std::vector enlarges!
    // NB: OrderBooks are relatively large, so we allocate a vector of them se-
    // parately from the MDC obj:
    using OrderBooksVec =
          boost::container::static_vector<OrderBook, Limits::MaxInstrs>;
    OrderBooksVec*                m_orderBooks;

    // Max OrderBooks Logical Depth to be maintained (0 for +oo):
    int                           m_mktDepth;

    // Max Number of Levels in the Physical OrderBook representation -- should
    // be large enough to accommodate px movements for the whole duration of
    // continuous trading, even under high volatility:
    int                           m_maxOrderBookLevels;

    // Whether pxs are non-sweepable (full-amount): Propagated to OrderBooks:
    bool const                    m_isFullAmt;

    // Whether this MDC uses Dense (Array-based) or Sparse (Map-based) Order-
    // Books:
    bool const                    m_isSparse;

    // Whether SeqNums and/or RptSeqs are used in OrderBooks (see there for the
    // details). Also, if enabled, RptSeqs may be continuous (ie always increm-
    // ented by 1) or not:
    bool const                    m_withSeqNums;
    bool const                    m_withRptSeqs;
    bool const                    m_contRptSeqs;

    // Should this MDC receive the Trades data via an explicit subscription (if
    // available)?
    bool const                    m_withExplTrades;

    // Should this MDC receive the Trades data inferred  from the FullOrdersLog
    // (if available)?
    bool const                    m_withOLTrades;

    // NB:
    // (*) It may be that both "WithExplTrades" and "WithOLTrades" flags are NOT
    //     set, in which case no Trades data are processed at all, and "OnTrade-
    //     Update" Strategy call-Backs are NOT invoked;
    // (*) It is also OK to have both such flags SET, in which case:
    //     -- For our own Trades (which are reported to the resp OMCs), both re-
    //        ports are made, and the OMC prioritises them;
    //     -- In all other cases (incl the inetern MDC processing and  "OnTrade-
    //        Update" invocations), only Explicit Trades are processed.

    // Maximum number of orders in each order book
    long const                    m_maxOrders;

    // At each "IncrementalRefresh" processed, OrderBook update results will be
    // accumulated here:
    constexpr static int MaxUpdates = 128;
    using   OBUpdateInfosVec =
            boost::container::static_vector<OBUpdateInfo, MaxUpdates>;
    mutable OBUpdateInfosVec      m_updated;

    // Whether we use DynInitMode at all, and whether it is currently on:
    bool    const                 m_useDynInit;
    mutable bool                  m_dynInitMode;

    // The following flags for correct notifications of subscribers in UDP mode:
    mutable bool                  m_wasLastFragm;
    mutable bool                  m_delayedNotify;

    // If Protocol-Level MktData Subscription IDs are required in order  to id-
    // entify incoming updates, we maintain them here.
    // NB: If Trades are subscribed for together with OrderBook Updates, then
    // "m_trdSubscrIDs" remains empty:
    using ProtoSubscrIDsVec =
          boost::container::static_vector<OrderID,   Limits::MaxInstrs>;
    ProtoSubscrIDsVec*            m_obSubscrIDs;
    ProtoSubscrIDsVec*            m_trdSubscrIDs;

    // Ptrs to HistoGrams collecting latency statistics of MktData progressing
    // times (in nsec) through this Connector. Actual objs are located  in ShM
    // or on heap (for Hist Connectors):
    LatencyStats*                 m_socketStats;  // From HWRecv   to Socket
    LatencyStats*                 m_procStats;    // From Socket   to Process
    LatencyStats*                 m_updtStats;    // From Process  to OBUpdate
    LatencyStats*                 m_stratStats;   // From OBUpdate to Strategy
    LatencyStats*                 m_overAllStats; // From HWRecv   to Strategy
                                                  // (Over-All)
    //=======================================================================//
    // Ctors, Dtor and Properties:                                           //
    //=======================================================================//
    // No Default Ctor:
    EConnector_MktData() = delete;

    //-----------------------------------------------------------------------//
    // Non-Default Ctor:                                                     //
    //-----------------------------------------------------------------------//
    // NB:
    // (*) If "a_is_enabled" is False, this Ctor does nothing and returns imme-
    //     diately (this is a normal behaviour in some cases of multiple inher-
    //     itance);
    // (*) If “a_primary_mdc” is non-NULL, then THIS Connector is NOT Primary;
    //     it will NOT allocate Orders and OrderBooks by itself - rather, it
    //     will alias and use the objs owned by the Primary:
    // (*) No params are provided for "EConnector", as it is a Virtual Base;
    //     all params below are for "EConnector_MktData" specifically:
    //
    EConnector_MktData
    (
      bool                                a_is_enabled,
      EConnector_MktData*                 a_primary_mdc,  // NULL iff Primary
      boost::property_tree::ptree const&  a_params,
      bool                                a_is_full_amt,
      bool                                a_is_sparse,
      bool                                a_with_seq_nums,
      bool                                a_with_rpt_seqs,
      bool                                a_cont_rpt_seqs,
      int                                 a_mkt_depth,    // 0 -> +oo
      bool                                a_with_expl_trades,
      bool                                a_with_ol_trades,
      bool                                a_use_dyn_init
    );

  public:
    //-----------------------------------------------------------------------//
    // Virtual Dtor:                                                         //
    //-----------------------------------------------------------------------//
    virtual ~EConnector_MktData() noexcept override;

    //-----------------------------------------------------------------------//
    // Properties:                                                           //
    //-----------------------------------------------------------------------//
    // Get the list of all OrderBooks configured:
    OrderBooksVec const& GetOrderBooks() const { return *m_orderBooks; }

  protected:
    // Trades and OrdersLog support:
    bool HasTrades   () const
      { return (m_withExplTrades || m_withOLTrades);  }

    bool HasOrdersLog() const
    {
      return (m_maxOrders > 0);
    }

    // Is this MDC a Primary one?
    bool IsPrimaryMDC() const
      { return (m_primaryMDC == nullptr || m_primaryMDC == this); }

    //-----------------------------------------------------------------------//
    // Starting / Stopping:                                                  //
    //-----------------------------------------------------------------------//
    // NB: "Start" and "Stop" methods remain abstract!

  public:
    //=======================================================================//
    // Accessors:                                                            //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "GetOrderBook": For external use:                                     //
    //-----------------------------------------------------------------------//
    // XXX: To select this method, "this" ptr must be qualified as "const"!
    //
    OrderBook const* GetOrderBookOpt(SecDefD const& a_instr ) const;
    BT_VIRTUAL
    OrderBook const& GetOrderBook   (SecDefD const& a_instr ) const;
    OrderBook const& GetOrderBook   (SecID          a_sec_id) const;

    // XXX: The following enum class needs to be made public, because otherwise
    // it cannot be used in template params of "friend" class methods (in CLang
    // only; GCC is OK!):
    enum class FindOrderBookBy: int
    {
      UNDEFINED   = 0,
      SecID       = 1,
      Symbol      = 2,
      AltSymbol   = 3,
      SubscrReqID = 4
    };

  protected:
    //-----------------------------------------------------------------------//
    // "FindOrderBook": For internal use: non-const, returning a ptr:        //
    //-----------------------------------------------------------------------//
    // Will return NULL (no exceptions are thrown) if the required OrderBook was
    // not found:
    // By SecDefD:
    OrderBook* FindOrderBook(SecDefD const& a_instr );

    // By SecID:
    OrderBook* FindOrderBook(SecID          a_sec_id);

    // By Symbol (UseAltSymbol = false) or AltSymbol (UseAltSymbol = true):
    template<bool UseAltSymbol>
    OrderBook* FindOrderBook(char const*    a_symbol);

    //-----------------------------------------------------------------------//
    // Generic Version:                                                      //
    //-----------------------------------------------------------------------//
    // It also includes searching by Protocol-level SubscrReqID which is not
    // available above:
    //
    template<FindOrderBookBy FOBBy, typename Msg>
    OrderBook* FindOrderBook
    (
      Msg const&                    a_msg,
      typename Msg::MDEntry const&  a_mde,
      char const*                   a_where
    );

  public:
    //=======================================================================//
    // Subscription Services:                                                //
    //=======================================================================//
    // NB: "Start", "Stop", "IsActive" and "Subscribe" are all inherited from
    //     "EConnector".
    //-----------------------------------------------------------------------//
    // "SubscribeMktData":                                                   //
    //-----------------------------------------------------------------------//
    // Subscribing a given Strategy to BOTH MktData Events and general Trading
    // Status Events (forthe latter, "EConnector::Subscribe" can be used,  but
    // it is unnecessary for MDCs, as it is included into this method):
    // (*) "min_cb_level" is about OrderBook update NOTIFICATIONS invoked  via
    //     the corresp Strategy Call-Back ("on_ob_update_cb");
    // (*) "instr" can be obtained using the "GetInstr" method;
    // (*) thses methods are pure virtual because we don't know if any  "Proto-
    //     colAction" is required (so cannot call  the templated impls   given
    //     above);
    // This method is made "virtual"  because  it may occasionally need  to be
    // overridden by derived classes:
    //
    virtual void SubscribeMktData
    (
      Strategy*                 a_strat,
      SecDefD const&            a_instr,
      OrderBook::UpdateEffectT  a_min_cb_level,
      bool                      a_reg_instr_risks    // Interact with RiskMgr?
    );

    // NB: "Subscribe" from the base class  ("EConnector")  is NOT overridden,
    // and remains usable,  but is not required if "SubscribeMktData" is used!

    //-----------------------------------------------------------------------//
    // "UnSubscribe*": Overridden with MDC specifics:                        //
    //-----------------------------------------------------------------------//
    virtual void UnSubscribe   (Strategy const* a_strat) override;
    virtual void UnSubscribeAll()                        override;

    //-----------------------------------------------------------------------//
    // "RegisterOrder":                                                      //
    //-----------------------------------------------------------------------//
    // Registers the given Req12 (and thus the AOS, OMC and Strategy)  to recv
    // Call-Backs on any events related to the MDEntryID (an alternative OrdID)
    // recorded in the Req12. This provides a lnk between the MDC and the OMC,
    // and potentially allowes the Strategy to receive  Order-related  events
    // earlier via MDC:
    //
    void RegisterOrder(Req12 const& a_req);

  protected:
    //=======================================================================//
    // Processing Trades and OrderBook Updates:                              //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "ProcessEndOfDataChunk":                                              //
    //-----------------------------------------------------------------------//
    template
    <
      bool     IsMultiCast,
      bool     WithIncrUpdates,
      bool     WithOrdersLog,
      bool     WithRelaxedOBs,
      QtyTypeT QT,
      typename QR
    >
    void ProcessEndOfDataChunk() const;

    //-----------------------------------------------------------------------//
    // "ProcessTrade":                                                       //
    //-----------------------------------------------------------------------//
    // NB: "a_order" may be NULL; it is non-NULL only if this Trade is associa-
    // ted with an Order in a FullOrdersLog:
    //
    template
    <
      bool     WithOrdersLog,
      QtyTypeT QT,
      typename QR,
      typename OMC,
      typename Msg
    >
    void ProcessTrade
    (
      Msg                   const& a_msg,
      typename Msg::MDEntry const& a_mde,
      OrderBook::OrderInfo  const* a_order,           // May be NULL
      OrderBook             const& a_ob,              // For Notifications
      // If the following vals are explicitly given, they override those extr-
      // acted from the Msg and MDE:
      Qty<QT,QR>                   a_trade_sz,        // <= 0      if omitted
      FIX::SideT                   a_trade_aggr_side, // UNDEFINED if omitted
      // TimeStamp(s): XXX: "a_ts_handl" is currently NOT used here:
      utxx::time_val               a_ts_recv
    );

    //-----------------------------------------------------------------------//
    // "UpdateOrderBooks":                                                   //
    //-----------------------------------------------------------------------//
    // Performs all required updates to OrderBooks based on the Msg  (which may
    // be eg a FAST or FIX ShapShot or IncrementalRefresh Msg);
    // Returns the Boolean success flag (mostly useful for SnapShots-based Init
    // Mode, see the impl for further details):
    template
    <
      bool            IsSnapShot,
      bool            IsMultiCast,
      bool            WithIncrUpdates,
      bool            WithOrdersLog,
      bool            WithRelaxedOBs,
      bool            ChangeIsPartFill,
      bool            NewEncdAsChange,
      bool            ChangeEncdAsNew,
      bool            IsFullAmt,
      bool            IsSparse,
      FindOrderBookBy FOBBy,
      QtyTypeT        QT,
      typename        QR,
      typename        OMC,
      typename        Msg
    >
    bool UpdateOrderBooks
    (
      SeqNum            a_seq_num,   // NB: May not be available from Msg itself
      Msg const&        a_msg,
      bool              a_dyn_init_mode,
      utxx::time_val    a_ts_recv,
      utxx::time_val    a_ts_handl,
      OrderBook const** a_orderbook_out = nullptr,  // pointer to the orderbook
      OrderBook*        a_known_orderbook = nullptr // pointer to OB if known
    );

    //-----------------------------------------------------------------------//
    // "ResetOrderBooksAndOrders":                                           //
    //-----------------------------------------------------------------------//
    // For safety, should typically be invoked in the "Start" and/ot "Stop" se-
    // quences of Derived Classes. May also be required eg if a failed SnapShot
    // initialization has left the MDC in an inconsistent state, and we need to
    // clean-up the state to start over. For Orders-based MDCs,  Orders as well
    // as OrderBooks are reset:
    //
    void ResetOrderBooksAndOrders();

    //=======================================================================//
    // Utils:                                                                //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "ApplyOrderUpdate":                                                   //
    //-----------------------------------------------------------------------//
    // NB: Although this method is only used  in the "WithOrdersLog" mode,  we
    // may STILL have "WithIncrUpdates=false" in some weired cases! "NewIsChan-
    // ge" flag is used when the CallER does not know whether the Action is  a
    // "New" or "Change" (the MDC can tell the difference by whether the Order-
    // ID already exists).
    // Returns a quadruple (UpdateEffect, UpdatedSides, DeltaQty, OrderInfo*):
    //
    template
    <
      bool     IsSnapShot,
      bool     WithIncrUpdates,
      bool     StaticInitMode,
      bool     ChangeIsPartFill,
      bool     NewEncdAsChange,
      bool     ChangeEncdAsNew,
      bool     WithRelaxedOBs,
      bool     IsFullAmt,
      bool     IsSparse,
      QtyTypeT QT,
      typename QR
    >
    std::tuple
    <OrderBook::UpdateEffectT,  OrderBook::UpdatedSidesT,
     Qty<QT,QR>,                OrderBook::OrderInfo const*>

    ApplyOrderUpdate
    (
      OrderID                   a_order_id,
      FIX::MDUpdateActionT      a_action,
      OrderBook*                a_ob,          // Non-NULL
      FIX::MDEntryTypeT         a_new_side,
      PriceT                    a_new_px,
      Qty<QT,QR>                a_new_qty,     // 0 for Delete
      SeqNum                    a_rpt_seq,
      SeqNum                    a_seq_num
    )
    const;

    //-----------------------------------------------------------------------//
    // "AddToUpdatedList":                                                   //
    //-----------------------------------------------------------------------//
    // After "a_ob" has been updated, it is memoised on the updated list along
    // with the Update Effect and all the time stamps, so that the Subscribers
    // can be notified in due time:
    //
    bool AddToUpdatedList
    (
      OrderBook*                a_ob,
      OrderBook::UpdateEffectT  a_upd,
      OrderBook::UpdatedSidesT  a_sides,
      utxx::time_val            a_ts_exch,
      utxx::time_val            a_ts_recv,
      utxx::time_val            a_ts_handl,
      utxx::time_val            a_ts_proc
    )
    const;

    //-----------------------------------------------------------------------//
    // "NotifyOrderBookUpdates":                                             //
    //-----------------------------------------------------------------------//
    template
    <
      bool     IsMultiCast,
      bool     WithIncrUpdates,
      bool     WithOrdersLog,
      bool     WithRelaxedOBs,
      QtyTypeT QT,
      typename QR
    >
    void NotifyOrderBookUpdates
    (
      SeqNum                    a_seq_num,
      char const*               a_where
    )
    const;

  private:
    //-----------------------------------------------------------------------//
    // "VerifyOrderBookUpdates": Post-Processing in "UpdateOrderBooks":      //
    //-----------------------------------------------------------------------//
    // Checks for Bid-Ask collisions.
    // Returns "true" if everything was OK, "false" if collisions were detected
    // (and automatically corrected):
    //
    template
    <
      bool     IsMultiCast,
      bool     WithIncrUpdates,
      bool     WithOrdersLog,
      bool     WithRelaxedOBs,
      QtyTypeT QT,
      typename QR
    >
    bool VerifyOrderBookUpdates
    (
      OBUpdateInfo*             a_obui,
      SeqNum                    a_seq_num,
      char const*               a_where
    )
    const;

    //-----------------------------------------------------------------------//
    // "CreateOrderBook":                                                    //
    //-----------------------------------------------------------------------//
    // Creates and installs am OrderBook and the corresp ProtoSubscrInfo, both
    // empty yet:
    void CreateOrderBook(SecDefD const& a_instr);

    //-----------------------------------------------------------------------//
    // "UpdateLatencyStats" (jsut before Notifications are invoked):         //
    //-----------------------------------------------------------------------//
    void UpdateLatencyStats
    (
      OBUpdateInfo const&       a_obui,
      utxx::time_val            a_ts_strat
    )
    const;
  };
} // End namespace MAQUETTE
