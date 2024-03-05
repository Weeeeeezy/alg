// vim:ts=2:et
//===========================================================================//
//                      "Connectors/EConnector_OrdMgmt.h":                   //
//       Abstract common base for Order Management Embedded Connectors       //
//===========================================================================//
#pragma once

#include "Basis/BaseTypes.hpp"
#include "Basis/TimeValUtils.hpp"
#include "Basis/OrdMgmtTypes.hpp"
#include "Connectors/EConnector.h"
#include <utxx/rate_throttler.hpp>
#include <boost/container/static_vector.hpp>
#include <unordered_map>

namespace MAQUETTE
{
  //=========================================================================//
  // "EConnector_OrdMgmt" Class:                                             //
  //=========================================================================//
  // NB: Virtual inheritance from "EConnector":
  //
  class EConnector_OrdMgmt: virtual public EConnector
  {
  private:
    //=======================================================================//
    // OMC-MDCs Links:                                                       //
    //=======================================================================//
    // NB:
    // (*) This OMC may register (link) 1 or more MDCs which will be used for
    //     receiving information about our own Trades  (ie Trades coming from
    //     Orders placed via this OMC), in the hope that such info would become
    //     available earlier than over the OMC itself;
    // (*) The registered MDCs are stored on the "m_linkedMDCs" list;
    // (*) When an Order is placed and confirmed via this OMC, the OMC tells
    //     all registered MDCs to watch the corresp ExchID/MDEntryID,  by invo-
    //     king RegisterOrder() on those MDCs;
    // (*) When an MDC detects a Trade related to a registered ExecID, it invo-
    //     kes OrderTraded() on this MDC; this OMC takes care  to process  each
    //     Trade (identified by the ExecID) exactly once;
    // (*) Because MDCs may need to invoke OrderTraded() (a protected method)
    //     on the OMC, "OMCTradeProcessor" must be declared a "friend":
    //
    template<typename OMC>
    friend struct OMCTradeProcessor;

  public:
    //=======================================================================//
    // Names of Persistent Objs:                                             //
    //=======================================================================//
    constexpr static char const* AOSesON()  { return "AOSes";   }
    constexpr static char const* Req12sON() { return "Reqs12s"; }
    constexpr static char const* TradesON() { return "Trades";  } // OurTrades!

    constexpr static char const* RxSN_ON()  { return "RxSN";    }
    constexpr static char const* TxSN_ON()  { return "TxSN";    }
    constexpr static char const* OrdN_ON()  { return "OrdN";    }
    constexpr static char const* ReqN_ON()  { return "ReqN";    }
    constexpr static char const* TrdN_ON()  { return "TrdN";    }

    //=======================================================================//
    // Types:                                                                //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "PipeLineModeT": PipeLining Mode:                                     //
    //-----------------------------------------------------------------------//
    enum class PipeLineModeT: int
    {
      // (*) "Wait0" is a Fully-PipeLined mode: The OMC can send a "Modify",
      //     "ModLegC"+"ModLegN" or "Cancel" req not waiting for the OrigReq
      //     being Confirmed. NB: This mode is incompatible with the  "Send-
      //     ExchOrdIDs" flag because ExchOrdIDs become available only after
      //     Confirmations:
      Wait0 = 0,

      // (*) "Wait1": The OMC must wait for the OrigReq to be Confirmed before
      //     sending a "Modify", "ModLegC"+"ModLegN" or "Cancel" req. This mo-
      //     or may not be used together with the "SendExchOrdIDs" flag:
      Wait1 = 1,

      // (*) "Wait2": This case is only applicable to OMCs with Non-Atomic Mo-
      //     difies, ie where "ModLegC"+"ModLegN" need to be sent.   It means
      //     that before sending a new "ModLegC"+"ModLegN" pair, the OMC must
      //     wait not only for the Confirmation of the OrigReq (which is "New"
      //     or most frequently the previous "ModLegN"), but also for the prev
      //     "ModLegC" (if any). This is to avoid the risk of having two "*N"
      //     reqs active at the same time (and so both could get Filled):
      Wait2 = 2
    };

  protected:
    //=======================================================================//
    // Internal Flds:                                                        //
    //=======================================================================//
    // The OMC may be disabled insome cases of virtual inheritance:
    bool          const    m_isEnabled;

    // Direct ptrs to, and sizes of, "AOS"es, "Req12"s and "Trade"s (in ShM):
    unsigned               m_maxAOSes;
    AOS*                   m_AOSes;
    unsigned               m_maxReq12s;
    Req12*                 m_Req12s;
    unsigned               m_maxTrades;
    Trade*                 m_Trades;

    // XXX: An auxiliary HashTable mapping ExcOrdIDs to confirmed Req12s, loca-
    // ted in conventional process memory. It is currently for numeric ExchIDs
    // only. Only used if "m_useExchOrdIDsMap" flag (below) is set:
    mutable std::unordered_map<OrderID, Req12*>
                           m_reqsByExchID;

    // Properties (set by the derived classes). XXX: They could be moved into
    // template params of the methods which use them, but this would probably
    // be as unnecessary complication, as they are rarely used, in fact:
    //
    PipeLineModeT const    m_pipeLineMode;
    bool const             m_sendExchOrdIDs;
    bool const             m_useExchOrdIDsMap;
    bool const             m_hasAtomicModify;
    bool const             m_hasPartFilledModify;
    bool const             m_hasExecIDs;
    bool const             m_hasMktOrders;
    bool const             m_cancelsNotThrottled;

    // Direct ptrs to Counters (in ShM):
    SeqNum*                m_RxSN; // Expected next   SeqNum to Rx
    SeqNum*                m_TxSN; // Expected next   SeqNum to Tx
    OrderID*               m_OrdN; // Next OrderNumber (Reqs nest)
    OrderID*               m_ReqN; // Next individual ReqNumber
    OrderID*               m_TrdN; // Next individual OurTradeNum

    // The number of buffered but not yet sent Reqs (each with its Req12):
    mutable int            m_nBuffdReqs;

    // MDCs which may be used to receive alternative (hopefully early!) notifi-
    // cations of Trades made via this OMC. XXX: In some cases, there might be
    // quite a few of such Associated MDCs  (eg on FORTS -- multiple instances
    // of P2CGate MDCs):
    constexpr static unsigned MaxLinkedMDCs = 32;
    boost::container::static_vector<EConnector_MktData*, MaxLinkedMDCs>
                                              m_linkedMDCs;

    // Request Rate Throttler. Template Params:
    // (*) ThrottlSecs  : throttling window temporal length
    // (*) BucketsPerSec: throttling window granularity
    // XXX: The total window size is (ThrottlSecs * BucketsPerSec), which should
    // not be too large for efficiency reasons. The following settings provide a
    // compromise:
    constexpr static int ThrottlSecs    = 60;   //  1 minute
    constexpr static int BucketsPerSec  = 100;  // 10 msec buckets
    int                                       m_throttlingPeriodSec;
    int                                       m_maxReqsPerPeriod;

    mutable utxx::basic_rate_throttler<ThrottlSecs, BucketsPerSec>
                                              m_reqRateThrottler;

    // Throttling may result in reqs being Indicated but not sent out. We run a
    // periodic PusherTask  which sends the Indications  out when timing gets
    // right. The period is set to half of Bucket length, ie 5 msec currently:
    //
    constexpr static unsigned PusherTaskPeriodMSec = 500 / BucketsPerSec;
    static_assert(PusherTaskPeriodMSec > 0, "PusherTaskPeriod too short");

    // Indications across all AOSes in a FIFO order; this vector is used by the
    // PusherTask, and TimerFD used for PusherTask invocation.  XXX: there is a
    // static limit on the total number of unsent Indications:
    //
    constexpr static unsigned MaxInds   = 65536;
    mutable boost::container::static_vector<Req12*, MaxInds>*
                                              m_allInds;
    mutable int                               m_pusherFD;

    // A separate logger for all Trades:
    std::shared_ptr<spdlog::logger>           m_tradesLoggerShP;
    spdlog::logger*                           m_tradesLogger;

    // Balances per Instrument: FIXME: STD map, inefficient, but called rarely;
    // at least, we need to implement a custom allocator for it:
    //
    mutable std::map<SymKey, double>          m_balances;
    mutable std::map<SecDefD const*, double>  m_positions;

    //=======================================================================//
    // Ctors, Dtor and Properties:                                           //
    //=======================================================================//
    // No Default Ctor:
    EConnector_OrdMgmt() = delete;

    //-----------------------------------------------------------------------//
    // Non-Default Ctor:                                                     //
    //-----------------------------------------------------------------------//
    // NB:
    // (*) "SecDef"s would typically come from the corresp Mkt Data Connector;
    // (*) "MaxReqsPerSec" is passed as a separate arg, not in "a_params", be-
    //     cause it typically originates from a static Config  of the derived
    //     class, NOT from a dynamic ".ini" Config;
    // (*) No params are provided for "EConnector",  as it is a Virtual Base;
    //     all params below are for "EConnector_OrdMgmt" specifically:
    //
    EConnector_OrdMgmt
    (
      bool                                a_is_enabled,
      boost::property_tree::ptree const&  a_params,
      int                                 a_throttling_period_sec,
      int                                 a_max_reqs_per_period,
      PipeLineModeT                       a_pipeline_mode,
      bool                                a_send_exch_ord_ids,
      bool                                a_use_exch_ord_ids_map,
      bool                                a_has_atomic_modify,
      bool                                a_has_part_filled_modify,
      bool                                a_has_exec_ids,
      bool                                a_has_mkt_orders
    );

  public:
    //-----------------------------------------------------------------------//
    // Virtual Dtor and Properties:                                          //
    //-----------------------------------------------------------------------//
    virtual ~EConnector_OrdMgmt() noexcept override;

    // NB: "Start", "Stop" and "ReStart" methods remain abstract!

    // Whether we can issue a "ModifyOrder" before a previous "NewOrder" or
    // "ModifyOrder"  was confirmed:
    PipeLineModeT GetPipeLineMode() const { return m_pipeLineMode; }

    // Whether it is allowed to modify a Partially-Filled Order:
    BT_VIRTUAL
    bool HasPartFilledModify()      const { return m_hasPartFilledModify;  }

    // Whether Market Orders are supported:
    bool HasMktOrders()             const { return m_hasMktOrders; }

    // Link an MDC to this OMC, so that Orders made via this OMC will be regis-
    // tered in the MDC as well, and if an Order update is first encountered in
    // that MDC, it will be processed anyway (see above):
    //
    void LinkMDC(EConnector_MktData* a_mdc);

    //=======================================================================//
    // Starting / Stopping / Subscription Mgmt:                              //
    //=======================================================================//
    // NB: "Start", "Stop", "IsActive", "UnSubscribe" are all inherited from
    //     "EConnector".
    // But virtual "Subscribe" is specifically for OrdMgmt:
    //
    virtual void Subscribe(Strategy* a_strat) override;

    //-----------------------------------------------------------------------//
    // Other Properties:                                                     //
    //-----------------------------------------------------------------------//
    BT_VIRTUAL double GetPosition(SecDefD const* a_sd) const;
    BT_VIRTUAL double GetBalance (SymKey a_ccy)        const;

    //=======================================================================//
    // Virtual Order Entry Methods:                                          //
    //=======================================================================//
    // XXX: In those virtual methods, all Qtys are passed as "QtyAny" discrimi-
    // nated unions:
    //-----------------------------------------------------------------------//
    // "NewOrder":                                                           //
    //-----------------------------------------------------------------------//
    // NB: If the Px is specified  (normally for all Orders except Mkt ones),
    // the OMC is allowed to perform some rounding of that Px; in particular,
    // the CallER is NOT obliged to round "a_px" to the PxStep (though it may
    // still be a good idea).  The actual order Px will be stored in the corr
    // Req12:
    //
    virtual AOS const* NewOrder
    (
      // The originating Stratergy:
      Strategy*           a_strategy,

      // Main order params:
      SecDefD const&      a_instr,
      FIX::OrderTypeT     a_ord_type,
      bool                a_is_buy,
      PriceT              a_px,
      bool                a_is_aggr,
      QtyAny              a_qty,

      // Temporal params of the triggering MktData event:
      utxx::time_val      a_ts_md_exch    = utxx::time_val(),
      utxx::time_val      a_ts_md_conn    = utxx::time_val(),
      utxx::time_val      a_ts_md_strat   = utxx::time_val(),

      // Optional order params:
      // NB: TimeInForceT::UNDEFINED must automatically resolve to a Connector-
      // specific default:
      bool                a_batch_send    = false,    // Recommendation Only!
      FIX::TimeInForceT   a_time_in_force = FIX::TimeInForceT::UNDEFINED,
      int                 a_expire_date   = 0,        // Or YYYYMMDD
      QtyAny              a_qty_show      = QtyAny::PosInf(),
      QtyAny              a_qty_min       = QtyAny::Zero  (),
                                                      //   reduces the position
      // This applies to FIX::OrderTypeT::Pegged only:
      bool                a_peg_side      = true,     // True: this, False: opp
      double              a_peg_offset    = NaN<double>  // Signed
    )
    = 0;

    //-----------------------------------------------------------------------//
    // "CancelOrder":                                                        //
    //-----------------------------------------------------------------------//
    // Returns  "true" if the Cancellation Request was successfully submitted
    // and "false" if the order was not Cancellable (eg already Filled or Pen-
    // ding Cancel). Iff "true" was returned, the Cancel Req12 is formed, and
    // is available as the last one of this AOS:
    //
    virtual bool CancelOrder
    (
      AOS const*          a_aos,                      // Non-NULL
      utxx::time_val      a_ts_md_exch    = utxx::time_val(),
      utxx::time_val      a_ts_md_conn    = utxx::time_val(),
      utxx::time_val      a_ts_md_strat   = utxx::time_val(),
      bool                a_batch_send    = false     // Recommendation!
    )
    = 0;

    //-----------------------------------------------------------------------//
    // "ModifyOrder":                                                        //
    //-----------------------------------------------------------------------//
    // NB:
    // (*) "new_px" should be set to NaN if unchanged; the OMC MAY do some ro-
    //     unding of the requested NewPx, so the actual one is NOT guaranteed
    //     to be same as requested;
    // (*) "new_qty_show" should be set to Invalid for no-change,  and to any
    //     value which is >= "new_qty" (eg PosInf) to show the full "new_qty",
    //     which is the default; using no-change (Invalid) may result   in an
    //     error if "new_qty" is less than the original "qty";
    // (*) "new_qty_min"  should be set to Invalid for no-change, and to 0 for
    //     no-minimum, which is the default; again, using no-change  (Invalid)
    //     may result in an error if "new_qty" is less than the original "qty";
    // (*) IMPORTANT: Currently, only the Px, Qty, ShowQty and MinQty can be
    //     modified;  the Side cannot be (although some Exchanges may allow for
    //     that); FIXME: Peg Side and Peg Offset are not modifyable either yet!
    // Returns "true" if the modification request was successfully submitted,
    // or "false" if the Order was not Modifyable (eg already Filled, or Part-
    // Filled and the Exchange does not allow such orders to be Modified,  or
    // Pending Cancel).  Iff "true" was returned, the Modify Req12 is formed,
    // and is available as the last one of this AOS:
    //
    virtual bool ModifyOrder
    (
      AOS const*          a_aos,                      // Non-NULL
      PriceT              a_new_px,
      bool                a_is_aggr,
      QtyAny              a_new_qty,
      utxx::time_val      a_ts_md_exch    = utxx::time_val(),
      utxx::time_val      a_ts_md_conn    = utxx::time_val(),
      utxx::time_val      a_ts_md_strat   = utxx::time_val(),
      bool                a_batch_send    = false,    // Recommendation!
      QtyAny              a_new_qty_show  = QtyAny::PosInf(),
      QtyAny              a_new_qty_min   = QtyAny::Zero  ()
    )
    = 0;

    //-----------------------------------------------------------------------//
    // "CancelAllOrders":                                                    //
    //-----------------------------------------------------------------------//
    // Will issue cancellation requests for all AOSes created by the given
    // Strategy (or all Strategies, if the "strat" otr is NULL).   Returns
    // "true" iff such requests were successfully created and submitted.
    // XXX: We currently assume that this is method is typically invoked for
    // "operational" reasons, not in response to some MktData update,  hence
    // there are no "ts_md_{exch,conn,strat}" args:
    //
    virtual void CancelAllOrders
    (
      unsigned long       a_strat_hash48 = 0,                     // All  Strats
      SecDefD const*      a_instr        = nullptr,               // All  Instrs
      FIX::SideT          a_side         = FIX::SideT::UNDEFINED, // Both Sides
      char const*         a_segm_id      = nullptr                // All  Segms
    )
    = 0;

    //-----------------------------------------------------------------------//
    // "FlushOrders":                                                        //
    //-----------------------------------------------------------------------//
    // If a sequence of previous "{New|Cancel|Modify}Order" methods were invok-
    // ed with "a_batch_send" set, the orders MIGHT NOT put on the wire immedi-
    // ately -- they are potentially (but not necessarily!) concatenated  in a
    // buffer and then sent in 1 chunk when "FlushOrders" is invoked. (This may
    // the processing overhead on both the Client and the Server sides). HOWEV-
    // ER, "a_batch_send" is a recommendation only: eg if not supported by the
    // underlying protocol, this flag is ignored (w/ or w/o warning).  In such
    // cases, or when there are no buffered Orders, "FlushOrders" is a NoOp:
    //
    virtual utxx::time_val FlushOrders() = 0;

    //-----------------------------------------------------------------------//
    // "GetCurrReqsRate":                                                    //
    //-----------------------------------------------------------------------//
    // Returns the curr Reqs rate (per sec, may be fractional) as measured by
    // the Throttler:
    //
    double GetCurrReqsRate() const { return m_reqRateThrottler.curr_rate(); }

    //=======================================================================//
    // Order State Management API:                                           //
    //=======================================================================//
    // The following Protocol-independent methods are to be invoked by Protocol
    // Handlers (implemented as derived classes) to update the Order State. In
    // turn, the Order State Manager notifies the originating Strategies:
    // NB:
    // (*) All TimeStamps are assumed to be UTC;
    // (*) Most of the methods below are parameterized by QT and QR for Qtys:
    //
  protected:
    //-----------------------------------------------------------------------//
    // "OrderAcknowledged":                                                  //
    //-----------------------------------------------------------------------//
    // XXX: For "OrderAcknowledged", there are no TimeStamp updates or Strategy
    // notifications, nor ExchIDs currently; Px is a hint only (may be empty):
    //
    template<QtyTypeT QT, typename QR>
    void OrderAcknowledged
    (
      OrderID           a_id,         // ReqID    (non-0)
      OrderID           a_aos_id,     // Optional (0 if unknown)
      PriceT            a_px,         // May be empty
      Qty<QT, QR>       a_leaves_qty  // May be empty as well
    );

    //-----------------------------------------------------------------------//
    // "OrderConfirmedNew":                                                  //
    //-----------------------------------------------------------------------//
    // (For New orders); Returns the Confirmed New "Req12":
    // NB: It requires "ProtoEng" and "SessData" because it may need to send out
    // the Indicated Reqs:
    //
    template<QtyTypeT QT, typename QR, typename ProtoEng, typename SessData>
    Req12 const& OrderConfirmedNew
    (
      ProtoEng*         a_proto_eng,  // Non-NULL
      SessData*         a_sess,       // Non-NULL
      OrderID           a_id,         // ReqID    (non-0)
      OrderID           a_aos_id,     // Optional (0 if unknown)
      ExchOrdID const&  a_exch_id,
      ExchOrdID const&  a_mde_id,     // May or may not be available
      PriceT            a_px,         // Hint only (may be empty)
      Qty<QT, QR>       a_leaves_qty, // Again, hint only (may be empty or <= 0)
      utxx::time_val    a_ts_exch,
      utxx::time_val    a_ts_recv
    );

  private:
    //-----------------------------------------------------------------------//
    // "OrderConfirmedImpl": Internal Implementation:                        //
    //-----------------------------------------------------------------------//
    template<unsigned Mask,  QtyTypeT QT,   typename QR>
    std::pair<Req12*, Req12::StatusT> OrderConfirmedImpl
    (
      OrderID           a_id,         // ReqID    (non-0)
      OrderID           a_aos_id,     // Optional (0 if unknown)
      ExchOrdID const&  a_exch_id,
      ExchOrdID const&  a_mde_id,     // May or may not be available
      PriceT            a_px,         // Hint only (may be empty)
      Qty<QT, QR>       a_leaves_qty, // Again, hint only
      utxx::time_val    a_ts_exch,
      utxx::time_val    a_ts_recv,
      char const*       a_where       // Non-NULL
    );

  protected:
    //-----------------------------------------------------------------------//
    // "OrderReplaced":                                                      //
    //-----------------------------------------------------------------------//
    // (*) "a_curr_id" is the order ID AFTER  it has been replaced;
    // (*) "a_orig_id" is the order ID BEFORE it has been replaced;
    // Again, "ProtoEng" and "SessData" are for sending out Indicated Reqs;
    // Returns the Post-Replacement "Req12":
    //
    template<QtyTypeT QT, typename QR, typename ProtoEng, typename SessData>
    Req12 const& OrderReplaced
    (
      ProtoEng*         a_proto_eng,    // Non-NULL
      SessData*         a_sess,         // Non-NULL
      OrderID           a_curr_id,      // ID of the Replacement Req
      OrderID           a_orig_id,      // ID of the Replaced    Req
      OrderID           a_aos_id,       // Optional (0 if unknown)
      ExchOrdID const&  a_curr_exch_id, // Same for Exch-assigned IDs
      ExchOrdID const&  a_orig_exch_id, //
      ExchOrdID const&  a_mde_id,       // May or may not be available
      PriceT            a_curr_px,      // Hint only (may be empty)
      Qty<QT, QR>       a_leaves_qty,   // Hint only (may be empty or <= 0)
      utxx::time_val    a_ts_exch,
      utxx::time_val    a_ts_recv
    );

    //-----------------------------------------------------------------------//
    // "OrderCancelled":                                                     //
    //-----------------------------------------------------------------------//
    // This method is invoked on a direct cancellation, or if an order was can-
    // celled because if was part-filled and could not be modified:
    // (*) "a_clx_id"  is the ID of the cancellation (or failed modify) req;
    // (*) "a_orig_id" is the ID of the order cancelled;
    // (*) "a_exch_id" and "a_mde_id" also apply to the order cancelled -- the
    //     CxlReq itself does not have them (typically):
    // Returns the AOS of the Cancelled Order:
    //
    template<QtyTypeT QT, typename QR, typename ProtoEng, typename SessData>
    AOS const& OrderCancelled
    (
      ProtoEng*         a_proto_eng,        // Non-NULL
      SessData*         a_sess,             // Non-NULL
      OrderID           a_clx_id,
      OrderID           a_orig_id,
      OrderID           a_aos_id,           // Optional (0 if unknown)
      ExchOrdID const&  a_exch_id,
      ExchOrdID const&  a_mde_id,           // May or may not be available
      PriceT            a_orig_px,          // Hint only (may be empty)
      Qty<QT, QR>       a_orig_leaves_qty,  // Orig LeavesQty (hint only)
      utxx::time_val    a_ts_exch,
      utxx::time_val    a_ts_recv
    );

  private:
    //-----------------------------------------------------------------------//
    // "OrderCancelledImpl": Internal Back-End of "OrderCancelled":          //
    //-----------------------------------------------------------------------//
    template<QtyTypeT QT, typename QR, typename ProtoEng, typename SessData>
    void OrderCancelledImpl
    (
      ProtoEng*         a_proto_eng,        // Non-NULL
      SessData*         a_sess,             // Non-NULL
      AOS*              a_aos,              // Non-NULL
      Req12*            a_clx_req,          // May be NULL (eg if auto-Cxl)
      Req12*            a_orig_req,         // Non-NULL
      bool              a_is_tandem,
      utxx::time_val    a_ts_exch,
      utxx::time_val    a_ts_recv
    );

  protected:
    //-----------------------------------------------------------------------//
    // "OrderRejected":                                                      //
    //-----------------------------------------------------------------------//
    // Invoked when:
    // (*) "a_id" is the ID of the order Rejected / Restated / Expired / other-
    //     wise failed;
    // (*) there was no explicit cancellation (by another order):
    // (*) ExchID would most likely be empty if a new order was rejected, but
    //     could be non-empty in case of expiration etc, so it is still used:
    // Returns the failed "Req12":
    //
    template<QtyTypeT QT, typename QR, typename ProtoEng, typename SessData>
    Req12 const& OrderRejected
    (
      ProtoEng*         a_proto_eng,    // Non-NULL
      SessData*         a_sess,         // Non-NULL
      SeqNum            a_sn,           // SeqNum  of the rejected order, mb 0
      OrderID           a_id,           // ClOrdID of the rejected order
      OrderID           a_aos_id,       // Optional (0 if unknown)
      FuzzyBool         a_non_existent, // Hint from Proto (may be UNDEFINED)
      int               a_err_code,
      char const*       a_err_text,
      utxx::time_val    a_ts_exch,
      utxx::time_val    a_ts_recv
    );

    //-----------------------------------------------------------------------//
    // "OrderTraded":                                                        //
    //-----------------------------------------------------------------------//
    // Returns a ptr to the new Trade obj, or NULL if this evt was skipped  for
    // any reason. May also be invoked on an evt received from an MDC, hence an
    // MDC ptr here:
    //
    template<QtyTypeT QT,       typename QR,
             QtyTypeT QTF,      typename QRF,
             typename ProtoEng, typename SessData>
    Trade const* OrderTraded
    (
      ProtoEng*                 a_proto_eng,  // Non-NULL
      SessData*                 a_sess,       // Non-NULL
      EConnector_MktData*       a_mdc,        // NULL if not from MDC
      OrderID                   a_id,         // ID of the failed Req
      OrderID                   a_aos_id,     // Optional (0 if unknown)
      Req12*                    a_req,        // NULL if not known yet
      FIX::SideT                a_our_side,   // May be UNDEFINED
      FIX::SideT                a_aggr_side,  // May be UNDEFINED
      ExchOrdID const&          a_exch_id,    // May be unavailable
      ExchOrdID const&          a_mde_id,     // May be unavailable
      ExchOrdID const&          a_exec_id,    // Normally available
      PriceT                    a_orig_px,    // Original Px (mb empty)
      PriceT                    a_last_px,    // Trade    Px: Must be Finite
      Qty<QT, QR>               a_last_qty,   // Must be > 0
      Qty<QT, QR>               a_leaves_qty, // <  0 if unknown
      Qty<QT, QR>               a_cum_qty,    // <= 0 if unknown
      Qty<QTF,QRF>              a_fee,        // Commission/Fee, mb Invalid
      int                       a_settl_date, // As YYYYMMDD
      FuzzyBool                 a_is_filled,  // Complete/Partial Fill?
      utxx::time_val            a_ts_exch,
      utxx::time_val            a_ts_recv
    );

    //-----------------------------------------------------------------------//
    // "OrderCancelOrReplaceRejected":                                       //
    //-----------------------------------------------------------------------//
    // XXX: Should this function be split into two: "OrderCancelRejected" and
    // "OrderReplaceRejected", similar to "OrderCancelled" and "OrderReplaced"
    // above? -- At the moment, we don't do that, because the Protocol  Layer
    // may not always know (prior to calling the State Manager) which req has
    // actually failed:
    // NB: There is no "MDEntryID" arg here (as yet);
    // Returns the failed "Req12":
    //
    template<QtyTypeT QT, typename QR, typename ProtoEng, typename SessData>
    Req12 const& OrderCancelOrReplaceRejected
    (
      ProtoEng*                 a_proto_eng,    // Non-NULL
      SessData*                 a_sess,         // Non-NULL
      OrderID                   a_rej_id,       // Of the Cancel/Replace    Req
      OrderID                   a_orig_id,      // Of the Original (Target) Req
      OrderID                   a_aos_id,       // Optional (0 if unknown)
      ExchOrdID const*          a_exch_id,      // May be NULL
      FuzzyBool                 a_filled,       // May be UNDEFINED
      FuzzyBool                 a_non_existent, // Ditto
      int                       a_err_code,
      char const*               a_err_text,
      utxx::time_val            a_ts_exch,
      utxx::time_val            a_ts_recv
    );

    //-----------------------------------------------------------------------//
    // "ReqRejectedBySession":                                               //
    //-----------------------------------------------------------------------//
    // Invoked when we get a Session-Level Req rejection. The Req may be ident-
    // ified either by SeqNum or by ClOrdID (distinguished by the 1st arg). In
    // this case, Order State Mgmt is simplicied compared to eg "OrderRejected"
    // or "OrderCancelOrReplaceRejected", because we know for sure that the Req
    // did not even reach the Matching Engine:
    // Returns the failed Req12:
    //
    template<QtyTypeT QT, typename QR, typename ProtoEng, typename SessData>
    Req12 const& ReqRejectedBySession
    (
      ProtoEng*         a_proto_eng,  // Non-NULL
      SessData*         a_sess,       // Non-NULL
      bool              a_by_sn,      // Arg2 is SeqNum (otherwise, it's ReqID)
      long              a_id,         // So SeqNum or ReqID
      OrderID           a_aos_id,     // Optional (0 if unknown)
      char const*       a_reject_reason,
      utxx::time_val    a_ts_exch,    // Or the best known approximation
      utxx::time_val    a_ts_recv
    );

    //=======================================================================//
    // Misc:                                                                 //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "RefreshThrottlers":                                                  //
    //-----------------------------------------------------------------------//
    // To be invoked as frequently as reasonably possible (but still with diff-
    // erent increasing "a_ts" time-stamps), to improve the accuracy and effic-
    // iency of Orders Rate Throttling. Currently, there is only 1 Throttle to
    // refresh:
    void RefreshThrottlers(utxx::time_val a_ts) const;

    //=======================================================================//
    // Generic Order Entry Methods Impls:                                    //
    //=======================================================================//
    // XXX:
    // (*) Parameterised by "QT" and "QR", "ProtoEng", "SessData" types;
    // (*) The advantage of using those template params at the Method Level as
    //     opposed to the Class Level, is that "EConnector_OrdMgmt"  itself is
    //     then non-templated, and can provide a Virtual Sender API  (which is
    //     better avoided for efficiency, but still useful sometimes);
    // (*) However, the disadvantage is increased complexity of this and der-
    //     ived classes; the latter are responsible  for providing consistent
    //     (same) "ProtoEng" and "SessData" types and args to all of the "*Gen"
    //     methods below:
    //-----------------------------------------------------------------------//
    // "NewOrderGen":                                                        //
    //-----------------------------------------------------------------------//
    template<QtyTypeT QT, typename QR, typename ProtoEng, typename SessData>
    AOS* NewOrderGen
    (
      ProtoEng*           a_proto_eng,
      SessData*           a_sess,
      Strategy*           a_strategy,
      SecDefD const&      a_instr,
      FIX::OrderTypeT     a_ord_type,
      bool                a_is_buy,
      PriceT              a_px,
      bool                a_is_aggr,
      QtyAny              a_qty,
      utxx::time_val      a_ts_md_exch,
      utxx::time_val      a_ts_md_conn,
      utxx::time_val      a_ts_md_strat,
      bool                a_batch_send,
      FIX::TimeInForceT   a_time_in_force,
      int                 a_expire_date,
      QtyAny              a_qty_show,
      QtyAny              a_qty_min,
      bool                a_peg_side,
      double              a_peg_offset
    );

    //-----------------------------------------------------------------------//
    // "CancelOrderGen":                                                     //
    //-----------------------------------------------------------------------//
    template<QtyTypeT QT, typename QR, typename ProtoEng, typename SessData>
    bool CancelOrderGen
    (
      ProtoEng*           a_proto_eng,
      SessData*           a_sess,
      AOS*                a_aos,      // Non-NULL
      utxx::time_val      a_ts_md_exch,
      utxx::time_val      a_ts_md_conn,
      utxx::time_val      a_ts_md_strat,
      bool                a_batch_send
    );

  private:
    //-----------------------------------------------------------------------//
    // "CancelModLegN":                                                      //
    //-----------------------------------------------------------------------//
    // Special version of "CancelOrderGen", for internal use only: indended for
    // Cancelling "stray" ("left-over") "ModLegN" Reqs:
    //
    template<QtyTypeT QT, typename QR, typename ProtoEng, typename SessData>
    void CancelModLegN
    (
      ProtoEng*       a_proto_eng,
      SessData*       a_sess,
      Req12*          a_orig_req,     // Non-NULL, must be a ModLegN
      utxx::time_val  a_ts_exch,
      utxx::time_val  a_ts_recv
    );

  protected:
    //-----------------------------------------------------------------------//
    // "ModifyOrderGen":                                                     //
    //-----------------------------------------------------------------------//
    template<QtyTypeT QT, typename QR, typename ProtoEng, typename SessData>
    bool ModifyOrderGen
    (
      ProtoEng*           a_proto_eng,
      SessData*           a_sess,
      AOS*                a_aos,      // Non-NULL
      PriceT              a_new_px,
      bool                a_is_aggr,
      QtyAny              a_new_qty,
      utxx::time_val      a_ts_md_exch,
      utxx::time_val      a_ts_md_conn,
      utxx::time_val      a_ts_md_strat,
      bool                a_batch_send,
      QtyAny              a_new_qty_show,
      QtyAny              a_new_qty_min
    );

    //-----------------------------------------------------------------------//
    // "CancelAllOrdersGen":                                                 //
    //-----------------------------------------------------------------------//
    template<QtyTypeT QT, typename QR, typename ProtoEng, typename SessData>
    void CancelAllOrdersGen
    (
      ProtoEng*           a_proto_eng,
      SessData*           a_sess,
      unsigned long       a_strat_hash48,
      SecDefD const*      a_instr,
      FIX::SideT          a_side,
      char const*         a_segm_id
    );

    //-----------------------------------------------------------------------//
    // "FlushOrdersGen":                                                     //
    //-----------------------------------------------------------------------//
    template<typename ProtoEng, typename SessData>
    utxx::time_val FlushOrdersGen
    (
      ProtoEng*           a_proto_eng,
      SessData*           a_sess
    );

    //=======================================================================//
    // Persistent Object Mgmt:                                               //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "UpdateReqsRate":                                                     //
    //-----------------------------------------------------------------------//
    // Actually updates the Msg Counts in Throttlers after the msg(s) have been
    // sent:
    template<int NMsgs>
    void UpdateReqsRate(utxx::time_val a_ts) const;

    //-----------------------------------------------------------------------//
    // "IsReadyToSend":                                                      //
    //-----------------------------------------------------------------------//
    // Whether the given Req is ready to be sent on the wire:
    //
    enum class ReadyT: int
    {
      OK              = 0, // Yes, Req is ready to be sent on the wire
      Throttled       = 1, // Not ready because of throttling, OR the Protocol
                           //   engine not ready (eg re-connecting)
      OrigStatusBlock = 2  // Throttling OK, but OrigReq's status disallows...
    };

    template<typename ProtoEng>
    ReadyT IsReadyToSend
    (
      ProtoEng*           a_proto_eng, // Non-NULL
      Req12 const*        a_req,       // Non-NULL
      Req12 const*        a_orig_req,  // May be NULL
      utxx::time_val      a_ts         // Non-empty
    )
    const;

    //-----------------------------------------------------------------------//
    // "NewAOS":                                                             //
    //-----------------------------------------------------------------------//
    // Allocate and initialise a new "AOS" object; Same args as for "AOS" Ctor,
    // except for OrdID which is automatically determined    (same as the slot
    // index from which this "AOS"  is allocated):
    //
    AOS* NewAOS
    (
      Strategy*           a_strategy,
      SecDefD const*      a_instr,
      bool                a_is_buy,
      FIX::OrderTypeT     a_order_type,
      bool                a_is_iceberg,
      FIX::TimeInForceT   a_time_in_force,
      int                 a_expire_date
    );

    //-----------------------------------------------------------------------//
    // "NewReq12":                                                           //
    //-----------------------------------------------------------------------//
    // Allocate and initialise a new "Req12" object; Same args as for "Req12"
    // Ctor, except for ReqID which is automatically determined  (same as the
    // slot index from which this "Req12" is allocated):
    //
    template<QtyTypeT QT, typename QR, bool AttachToAOS>
    Req12* NewReq12
    (
      AOS*                a_aos,
      OrderID             a_orig_id,    // 0 for New, > 0 otherwise
      Req12::KindT        a_req_kind,
      PriceT              a_px,
      bool                a_is_aggr,
      Qty<QT,QR>          a_qty,
      Qty<QT,QR>          a_qty_show,
      Qty<QT,QR>          a_qty_min,
      bool                a_peg_side,
      double              a_peg_offset,
      utxx::time_val      a_ts_md_exch,
      utxx::time_val      a_ts_md_conn,
      utxx::time_val      a_ts_md_strat,
      utxx::time_val      a_ts_created
    );

    //-----------------------------------------------------------------------//
    // "NewTrade" (for our own Trade):                                       //
    //-----------------------------------------------------------------------//
    // NB: Returns NULL if no new Trade was created (for some obscure reason):
    //
    template<QtyTypeT QT, typename QR, QtyTypeT QTF, typename QRF>
    Trade const* NewTrade
    (
      EConnector_MktData* a_mdc,         // NULL if not from MDC
      Req12     const*    a_req,         // Always  non-NULL
      ExchOrdID const&    a_exec_id,     // May be empty
      PriceT              a_px,          // Always Finite
      Qty<QT, QR>         a_qty,         // Always > 0
      Qty<QTF,QRF>        a_fee,
      FIX::SideT          a_aggressor,   // May be UNDEFINIED
      utxx::time_val      a_ts_exch,
      utxx::time_val      a_ts_recv
    );

    //-----------------------------------------------------------------------//
    // "BackPropagateSendTS":                                                //
    //-----------------------------------------------------------------------//
    // NB: Because some OrdMgmt Protocols (eg FIX) allow for buffering and then
    // batch sending of Reqs, this function may be invoked  before  the corresp
    // Req has actually been sent out; "a_ts_sent" is empty in that case.  When
    // it is actually sent out,  "a_ts_sent" may apply to  several  previously-
    // buffered "Req12"s. Thus, if "SendTS" is available, it may need to be pro-
    // pagated backwards as well:
    //
    void BackPropagateSendTS(utxx::time_val a_ts_send, OrderID a_from_id);

    //=======================================================================//
    // Utils:                                                                //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "GetReq12":                                                           //
    //-----------------------------------------------------------------------//
    // (*) In the "Strict" mode, this method throws an exception if the reques-
    //     ted Req could not be found, and returns a non-NULL ptr otherwise;
    // (*) but there is also a non-Strict version which returns  NULL  in  the
    //     "not found" case;
    // (*) a_id == 0 is not allowed (0 is reserved as a special value); ZeroID
    //     will always result in NULL (!Strict) or exception (Strict);
    // (*) "Mask" is a bit-wise mask of admissible "Req12::Kind"s; the function
    //     will only look for matching "Req12"s;
    // (*) "Px" may be NaN; otherwise, the Req12 found must match that Px:
    //
    template<unsigned Mask, bool Strict, QtyTypeT QT, typename QR>
    Req12* GetReq12
    (
      OrderID     a_id,            // Always non-0
      OrderID     a_aos_id,        // Optional  (0 if unknown)
      PriceT      a_px,            // May be empty (NaN)
      Qty<QT,QR>  a_leaves_qty,    // May be emprt as well
      char const* a_where          // Non-NULL
    )
    const;

    //-----------------------------------------------------------------------//
    // "GetReq12ByExchID":                                                   //
    //-----------------------------------------------------------------------//
    // NON-Throwing (Non-Strict), returns NULL if a suitable Req12 not found.
    // XXX: Relatively inefficient, so it should only be used as a last resort
    // when the main "GetReq12" method yields nothing. Px is optional:
    //
    template<unsigned Mask>
    Req12* GetReq12ByExchID(ExchOrdID const& a_exch_id) const;

  private:
    //-----------------------------------------------------------------------//
    // "GetReq12BySeqNum":                                                   //
    //-----------------------------------------------------------------------//
    // NB: This is currently a NON-Throwing function (in case of the SeqNum is
    // not found -- NULL is returned). Very inefficient,    so only applies to
    // failed orders:
    //
    Req12* GetReq12BySeqNum(SeqNum a_sn, char const* a_where) const;

    //-----------------------------------------------------------------------//
    // "GetTargetReq12":                                                     //
    //-----------------------------------------------------------------------//
    // Searches for a Req12 which will be the target of a Cancel or Modify Req:
    //
    template<QtyTypeT QT, typename QR, bool IsModify>
    Req12* GetTargetReq12(AOS const* a_aos) const;   // AOS must be non-NULL

    //-----------------------------------------------------------------------//
    // "ApplyExchIDs":                                                       //
    //-----------------------------------------------------------------------//
    // Store the ExchID and MDEntryID (as assigned by the Exchange) in the Req12
    // and AOS, or verify the consistency of already-installed IDs (unless Force
    // is set,  in which case new ID is stored without verification). This check
    // is essential to prevent bogus Cancellations or Fills from being recorded.
    // An exception is thrown if the check fails:
    //
    template<bool Force>
    void ApplyExchIDs
    (
      Req12*           a_req,       // Non-NULL
      AOS*             a_aos,       // Non-NULL
      ExchOrdID const& a_exch_id,
      ExchOrdID const& a_mde_id,
      char const*      a_where
    );

    //-----------------------------------------------------------------------//
    // "MakeAOSInactive":                                                    //
    //-----------------------------------------------------------------------//
    // Mark the whole order (AOS) as "Inactive" -- it sould NOT be Inactive yet.
    // Also invokes "MakePendingReqsFailing" (see below):
    //
    template<QtyTypeT QT,       typename QR,
             typename ProtoEng, typename SessData>
    void MakeAOSInactive
    (
      ProtoEng*       a_proto_eng,  // Non-NULL
      SessData*       a_sess,       // Non-NULL
      AOS*            a_aos,        // Non-NULL
      Req12 const*    a_req,        // CxlReq or FilledReq, may be NULL
      char  const*    a_where,      // Non-NULL
      utxx::time_val  a_ts_exch,
      utxx::time_val  a_ts_recv
    );

    //-----------------------------------------------------------------------//
    // "MakePendingReqsFailing":                                             //
    //-----------------------------------------------------------------------//
    // Mark the Reqs coming after "a_req" as those which "WillFail" if "a_req"
    // itself has failed or (in some cases) was Part-Filled; in a Tandem mode,
    // *tries* to Cancel subsequent "ModLegN" reqs:
    //
    template<bool IsPartFill,   QtyTypeT QT,   typename QR,
             typename ProtoEng, typename SessData>
    void MakePendingReqsFailing
    (
      ProtoEng*       a_proto_eng,  // Non-NULL
      SessData*       a_sess,       // Non-NULL
      AOS*            a_aos,        // Non-NULL
      Req12 const*    a_req,        // CxlReq or TradedReq, may be NULL
      char  const*    a_where,      // Non-NULL
      utxx::time_val  a_ts_exch,
      utxx::time_val  a_ts_recv
    );

    //-----------------------------------------------------------------------//
    // "*SendIndications*":                                                  //
    //-----------------------------------------------------------------------//
    // "SendIndicationsOnEvent":
    // Send out the Indications waiting on a Req (from a given AOS) to be
    // "Confirmed" (this also includes previously-Throttled Indications):
    //
    template<typename ProtoEng, typename SessData>
    void SendIndicationsOnEvent
    (
      ProtoEng*      a_proto_eng,  // Non-NULL
      SessData*      a_sess,       // Non-NULL
      AOS*           a_aos,        // Non-NULL
      utxx::time_val a_ts          // Non-empty
    );

  protected:
    // "SendIndicationsOnTimer":
    // The derived classes need to create a TimerFD  with this method attached
    // to it (it is ProtoEng- and SessData-specific). To be invoked by Derived
    // classed which implement ProtoEng and SessData:
    //
    template<typename ProtoEng, typename SessData>
    void SendIndicationsOnTimer
    (
      ProtoEng*       a_proto_eng,  // Non-NULL
      SessData*       a_sess        // Non-NULL
    );

    template<typename ProtoEng, typename SessData>
    void CreatePusherTask
    (
      ProtoEng*       a_proto_eng, // Non-NULL
      SessData*       a_sess       // Non-NULL
    );

  private:
    // "TrySendIndications":
    // Returns True iff there was no Throttling encountered (even though Indic-
    // ation(s) could not be sent for other reasons):
    //
    template<typename ProtoEng, typename SessData>
    bool TrySendIndications
    (
      ProtoEng*       a_proto_eng, // Non-NULL
      SessData*       a_sess,      // Non-NULL
      Req12 const*    a_orig_req,  // May be NULL
      Req12*          a_ind_req,   // Non-NULL
      Req12*          a_next_ind,  // May be NULL
      bool            a_is_new,    // Indication(s) have just been created?
      bool            a_batch_send,
      utxx::time_val  a_ts         // Non-empty
    );

    // "MemoiseIncation":
    // If send was unsuccessful, keep the Indication on the "m_allInds" list
    // for a future re-sending (by Timer or Event).  If "a_is_new" is false,
    // do not store the Indication -- check that it is already there:
    //
    void MemoiseIndication(Req12* a_ind_req, bool a_is_new) const;

    //-----------------------------------------------------------------------//
    // "CheckOrigID":                                                        //
    //-----------------------------------------------------------------------//
    // NB: Because OrigReqID is recorded in Req12 when a new Cancel or Modify
    // req is created, and later reported back on various events via the Pro-
    // tocol layer, we compare the recorded and protocol-supplied OrigReqIDs;
    // XXX: there is currently no exception on a mismatch -- only error logged:
    //
    void CheckOrigID
    (
      Req12 const& a_req,
      OrderID      a_orig_id,
      char const*  a_where
    );

    //-----------------------------------------------------------------------//
    // "CheckOrigReq":                                                       //
    //-----------------------------------------------------------------------//
    // While "CheckOrigID" is invoked on Rx of a Response, "CheckOrigReq" is
    // invoked on sending Cancel or Modify. Throws an exception if the check
    // fails:
    template<QtyTypeT QT, typename QR, bool IsModify, bool IsTandem>
    void CheckOrigReq
    (
      AOS   const* a_aos,
      Req12 const* a_orig_req
    )
    const;

    //-----------------------------------------------------------------------//
    // "CheckModifyParams":                                                  //
    //-----------------------------------------------------------------------//
    // Verify and possibly adjust the New (Modification) params against the
    // "OrigReq". Returns True iff the check succeeds:
    //
    template<QtyTypeT QT, typename QR>
    bool CheckModifyParams
    (
      AOS   const* a_aos,          // Non-NULL
      Req12 const& a_orig_req,
      PriceT*      a_new_px,       // Non-NULL
      Qty<QT,QR>*  a_new_qty,      // Non-NULL
      Qty<QT,QR>*  a_new_qty_show, // Non-NULL
      Qty<QT,QR>*  a_new_qty_min   // Non-NULL
    )
    const;

    //-----------------------------------------------------------------------//
    // "GetOrigReq":                                                         //
    //-----------------------------------------------------------------------//
    // NB: Returns NULL if there is no OrigReq (eg for a "New"):
    //
    Req12* GetOrigReq(Req12 const* a_req) const;

    //-----------------------------------------------------------------------//
    // "GetSameAOS":                                                         //
    //-----------------------------------------------------------------------//
    AOS* GetSameAOS
    (
      Req12 const& a_req_curr,
      Req12 const& a_req_orig,
      char  const* a_where
    )
    const;

    //-----------------------------------------------------------------------//
    // "LogTrade":                                                           //
    //-----------------------------------------------------------------------//
    template<QtyTypeT QT, typename QR, QtyTypeT QTF, typename QRF>
    void LogTrade(Trade const& a_tr) const;

  protected:
    //-----------------------------------------------------------------------//
    // "EmulateMassCancelGen":                                               //
    //-----------------------------------------------------------------------//
    // If "StratName", "Instr" or "Side" are empty, they are not used for fil-
    // tering-out of AOSes to cancel:
    // Returns "true" iff at least one cancellation request was successfully
    // submitted:
    //
    template<QtyTypeT QT, typename QR, typename ProtoEng, typename SessData>
    void EmulateMassCancelGen
    (
      ProtoEng*        a_proto_eng,                           // Non-NULL
      SessData*        a_sess,                                // Non-NULL
      unsigned long    a_strat_hash48 = 0,                    // All Strats
      SecDefD  const*  a_instr        = nullptr,              // All Instrs
      FIX::SideT       a_side         = FIX::SideT::UNDEFINED // Both Sides
    );

    //-----------------------------------------------------------------------//
    // PusherTask TimerFD Mgmt:                                              //
    //-----------------------------------------------------------------------//
    // Used by derived classes:
    void RemovePusherTimer();

    void PusherTimerErrHandler
         (int a_err_code, uint32_t a_events, char const* a_msg);
  };
} // End namespace MAQUETTE
