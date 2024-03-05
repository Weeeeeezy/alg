// vim:ts=2:et
//===========================================================================//
//                           "Connectors/OrderBook.h":                       //
//  Internal "Unlimited-Depth" Order Book Representation with Fast Updates   //
//===========================================================================//
#pragma once

#include "Basis/BaseTypes.hpp"
#include "Basis/SecDefs.h"
#include "Basis/PxsQtys.h"
#include "Protocols/FIX/Msgs.h"        // For some enums
#include "InfraStruct/StaticLimits.h"
#include <utxx/compiler_hints.hpp>
#include <utxx/enum.hpp>
#include <utxx/error.hpp>
#include <spdlog/spdlog.h>
#include <boost/core/noncopyable.hpp>
#include <boost/container/static_vector.hpp>
#include <boost/container/map.hpp>     // To use Boost Pool allocators (below)
#include <boost/pool/pool_alloc.hpp>
#include <utility>
#include <cstring>
#include <type_traits>

namespace MAQUETTE
{
  //=========================================================================//
  // "OrderBookBase" Class:                                                  //
  //=========================================================================//
  // This is the most simplistic OrderBook which just provides L1 Pxs. Used eg
  // in STP:
  //
  class EConnector_MktData;

  class OrderBookBase
  {
  protected:
    friend class EConnector_MktData;

    //-----------------------------------------------------------------------//
    // Data Flds:                                                            //
    //-----------------------------------------------------------------------//
    EConnector_MktData const* m_mdc;      // MDC this Book belongs to
    SecDefD            const* m_instr;    // Instr of this Book (NOT owned)
    mutable PriceT            m_BBPx;     // BestBidPx
    mutable PriceT            m_BAPx;     // BestAskPx

  public:
    //-----------------------------------------------------------------------//
    // Default / Non-Default Ctor:                                           //
    //-----------------------------------------------------------------------//
    OrderBookBase
    (
      EConnector_MktData const* a_mdc   = nullptr,
      SecDefD            const* a_instr = nullptr
    )
    : m_mdc  (a_mdc),
      m_instr(a_instr),
      m_BBPx (),              // NaN
      m_BAPx ()               // ditto
    {}

    //-----------------------------------------------------------------------//
    // Dtor:                                                                 //
    //-----------------------------------------------------------------------//
    ~OrderBookBase() noexcept
    {
      // Just for safety, clear all flds:
      m_instr = nullptr;
      m_mdc   = nullptr;
      m_BBPx  = PriceT();
      m_BAPx  = PriceT();
    }

    //-----------------------------------------------------------------------//
    // Accessors:                                                            //
    //-----------------------------------------------------------------------//
    // MDC:
    EConnector_MktData const* GetMDCOpt() const   { return m_mdc; } // Mb NULL

    EConnector_MktData const& GetMDC()    const
    {
      if (utxx::unlikely(m_mdc   == nullptr))
        throw utxx::runtime_error("OrderBookBase::GetMDC: NULL");
      return *m_mdc;
    }

    // Instrument:
    SecDefD const& GetInstr()             const
    {
      if (utxx::unlikely(m_instr == nullptr))
        throw utxx::runtime_error("OrderBookBase::GetInstr: NULL");
      return *m_instr;
    }

    // L1 Pxs:
    PriceT GetBestBidPx() const { return m_BBPx;   }
    PriceT GetBestAskPx() const { return m_BAPx;   }

    // XXX: Setters are in general a very bad idea, but we use them here. They
    // Return "true" iff the corresp px was successfully set (without invalida-
    // ting the other side of the Book). In the Relaxed mode, Bid-Ask  collisi-
    // ons up to some Tolerance are allowed:
    //
    template<bool IsRelaxed>
    bool   SetBestBidPx(PriceT a_px) const;

    template<bool IsRelaxed>
    bool   SetBestAskPx(PriceT a_px) const;

    //  "IsConsistent": Normal valid state of the Book:
    bool IsConsistent()              const
    {
      // NV: If either Bid or Ask side (or both) does not exist at all, this is
      // STILL a Consistent Book:
      return
        (utxx::unlikely(!(IsFinite(m_BBPx) && IsFinite(m_BAPx))))
        ? true
        : (m_BBPx < m_BAPx);     // Generic Case
    }
  };

  //=========================================================================//
  // "OrderBook" Class:                                                      //
  //=========================================================================//
  struct Req12;
  class  Strategy;

  class OrderBook: public OrderBookBase, public boost::noncopyable
  {
  public:
    //=======================================================================//
    // Data Types:                                                           //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "UpdateEffectT":                                                      //
    //-----------------------------------------------------------------------//
    // NB: Each subsequent level or "UpdateEffectT" includes all previous level
    // (more events); this is why "NONE" is the lowest level:
    //
    enum class UpdateEffectT: int
    {
      NONE     = 0,  // No updates at all (nothing affected):          WEAKEST
      L2       = 1,  // L2 Px and/or Qty affected, but not L1             |
      L1Qty    = 2,  // L1 Qty affected, but not L1 Px                    V
      L1Px     = 3,  // L1 Px (and this Qty at that Px) affected       STRONG
      ERROR    = 4   // Always Reported!                              STRONGEST
    };

    //-----------------------------------------------------------------------//
    // "UpdatedSidesT":                                                      //
    //-----------------------------------------------------------------------//
    enum class UpdatedSidesT: int
    {
      NONE     = 0,
      Bid      = 1,
      Ask      = 2,
      Both     = 3
    };

    static bool IsBidUpdated(UpdatedSidesT a_upd)
      { return ((int(a_upd) & int(UpdatedSidesT::Bid)) != 0); }

    static bool IsAskUpdated(UpdatedSidesT a_upd)
      { return ((int(a_upd) & int(UpdatedSidesT::Ask)) != 0); }

    //-----------------------------------------------------------------------//
    // "SubscrInfo":                                                         //
    //-----------------------------------------------------------------------//
    // Descriptor for a Strategy subscribed to receive updates from this Order-
    // Book (as well as Trades) on the corresp Symbol. "MinCBLevel" is to igno-
    // re "weak" effects (see above). XXX: Here we do not discriminate updates
    // by OrderBook side; either side would do:

    struct SubscrInfo
    {
      // Data Flds:
      Strategy*       m_strategy;             // StrategyPtr NOT owned
      UpdateEffectT   m_minCBLevel;           // Filter for CB

      // Default Ctor:
      SubscrInfo()
      : m_strategy    (nullptr),
        m_minCBLevel  (UpdateEffectT::NONE)   // Placeholder only
      {}

      // Non-Default Ctor:
      SubscrInfo
      (
        Strategy*     a_strategy,
        UpdateEffectT a_min_cb_level
      )
      : m_strategy    (a_strategy),
        m_minCBLevel  (a_min_cb_level)
      {}

      // Dtor is not reqlly required...
    };

    //-----------------------------------------------------------------------//
    // "OrderInfo":                                                          //
    //-----------------------------------------------------------------------//
    // Only for OrdersLog-based OrderBooks. The actual mgmt of "OrderInfo"s is
    // within the "EConnector_MktData"; here we only manage ptrs from "OBEntry"s
    // to the chains or "OrderInfo"s:
    //
    struct OBEntry;
    struct OrderInfo
    {
      // Data Flds:
      OrderBook const*  m_ob;     // "Outer" ptr
      OrderID           m_id;     // Eg a numerical MDEntryID
      bool              m_isBid;  // Side recorded -- may be required for Change
      PriceT            m_px;     // Price
      QtyU              m_qty;    // Qty (>= 0; 0 for deleted Orders)
      Req12 const*      m_req12;  // If it was placed by our Strategy; or NULL
      OrderInfo*        m_next;   // Next (later-coming)   at the same Px Level
      OrderInfo*        m_prev;   // Prev (earlier-coming) at the same Px Level
      OBEntry*          m_obe;    // For safety of "Delete"

      // Default Ctor:
      OrderInfo()
      : m_ob      (nullptr),
        m_id      (0),
        m_isBid   (false),
        m_px      (),             // NaN
        m_qty     (),             // 0
        m_req12   (nullptr),
        m_next    (nullptr),
        m_prev    (nullptr),
        m_obe     (nullptr)
      {}

      // Emptyness Test:
      bool IsEmpty() const
      {
        assert((m_ob == nullptr) == (m_id == 0));
        return (m_ob == nullptr);
      }

      // Dynamically-Type-Checked Qty Accessor:
      template<QtyTypeT QT, typename QR>
      Qty<QT,QR> GetQty() const
      {
        return utxx::unlikely(IsEmpty())
               ? Qty<QT,QR>()
               : m_qty.GetQty<QT,QR>(m_ob->m_qt, m_ob->m_withFracQtys);
      }
    };

    //-----------------------------------------------------------------------//
    // "OBEntry": Aggregated OrderBook Entry:                                //
    //-----------------------------------------------------------------------//
    // NB: There is no Px here -- the Px is impicit (defined by the slot occup-
    // ied by this "OBEntry"):
    //
    struct OBEntry
    {
      // Data Flds:
      OrderBook const*  m_ob;        // "Outer" ptr
      QtyU              m_aggrQty;   // Aggregated Qty   at this PxLvl
      int               m_nOrders;   // Number of Orders at this PxLvl (if any)
      OrderInfo*        m_frstOrder; // NULL if not avail (MBP); otherwise MBO
      OrderInfo*        m_lastOrder; // ditto

      // Default Ctor: 
      OBEntry()
      : m_ob       (nullptr),        // Set afterwards
        m_aggrQty  (),               // 0
        m_nOrders  (0),
        m_frstOrder(nullptr),
        m_lastOrder(nullptr)
      {}

      // Dynamically-Type-Checked AggrQty Accessor:
      template<QtyTypeT QT, typename QR>
      Qty<QT,QR> GetAggrQty() const
      {
        assert(m_ob != nullptr);
        return m_aggrQty.GetQty<QT,QR>(m_ob->m_qt, m_ob->m_withFracQtys);
      }
    };

  private:
    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    friend class EConnector_MktData;

    //-----------------------------------------------------------------------//
    // Constants (checked against template method params):                   //
    //-----------------------------------------------------------------------//
    // WithSeqNums and WithRptSeqs are flags indicating whether SeqNums (global)
    // and RptSeqs (per-instr), resp, are used; XXX they should actually be tem-
    // plate params, but this would complicate all APIs using the "OrderBook" --
    // so we use a run-time params instead (the overhead is absolutely minimal):
    //
    bool     const            m_isFullAmt;
    bool     const            m_isSparse;   // Use Maps, not Arrays!
    QtyTypeT const            m_qt;
    bool     const            m_withFracQtys;
    bool     const            m_withSeqNums;
    bool     const            m_withRptSeqs;
    bool     const            m_contRptSeqs;

    //-----------------------------------------------------------------------//
    // Dense (Equi-Spaced) OrderBook Rep:                                    //
    //-----------------------------------------------------------------------//
    // NB:
    // (*) Unlike "SeqNumBuffer", here we use 0, not (-1), for Invalid SeqNums
    //     (all valid values start from 1, anyway);
    // (*) "m_lastUpdateRptSeq" is the last "continuous" (per-this-instrument,
    //     monotonic, no-gaps) SeqNum of the attempted or successful update of
    //     this Book; may be <= 0 if WithSeqNums is "false";
    // (*) "m_lastUpdateSeqNum" is the last "global" (all-through, for ALL in-
    //     struments, monotonic but with gaps when applied to a particular in-
    //     strument) SeqNum; may be <= 0 is WithSeqNums is "false";
    // (*) In both "m_bids" and "m_asks" below, Px Levels come in the ASCENDING
    //     order wrt array indices; initial top of the book roughly corresponds
    //     to the mid-idx of each array:
    // NB: In general, we do not need separate "m_bids" and "m_asks" arrays; a
    // single array indexed by discrete pxs would do (and save 50% of space).
    // The only reason why the OrderBook is not implemented this way is poten-
    // tial Bid-Ask Collisions:
    //
    int     const             m_NL;   // Size of "m_bids" and "m_asks"  (#lvls)
    int     const             m_ND;   // Max MktDepth (0 = +oo for OrdersLog)
    mutable int               m_LBI;  // Current Lowest Bid Idx (in "m_bids")
    mutable int               m_BBI;  // Current Best   Bid Idx (in "m_bids")
                                      // NB: "m_BBPx" corresponds to "m_BBI"
    mutable int               m_BD;   // Curr MktDepth of Bids (or +oo)
    OBEntry*                  m_bids; // (Qty,#Orders) at all Bid Px Levels
                                      //   (both 0s at empty lvls)
    mutable int               m_BAI;  // Current Best    Ask Idx (in "m_asks")
                                      // NB: "m_BAPx" corresponds to "m_BAI"
    mutable int               m_HAI;  // Current Highest Ask Idx (in "m_asks")
    mutable int               m_AD;   // Curr Mktdepth if Asks (or +oo)
    OBEntry*                  m_asks; // (Qty,#Orders) at all Ask Px Lvls
                                      //   (both 0s at empty lvls)
    //-----------------------------------------------------------------------//
    // Sparse (Map-Based) OrderBook Rep:                                     //
    //-----------------------------------------------------------------------//
    using FastPoolMapAlloc =
          boost::fast_pool_allocator<std::pair<PriceT const, OBEntry>>;
    using BidsMap          =   // Descending Pxs
          boost::container::map
          <PriceT, OBEntry, std::greater<PriceT>, FastPoolMapAlloc>;
    using AsksMap          =   // Ascending Pxs
          boost::container::map
          <PriceT, OBEntry, std::less<PriceT>,    FastPoolMapAlloc>;

    mutable BidsMap           m_bidsMap;
    mutable AsksMap           m_asksMap;

    // "GetSideMap": Helper accessor for use in templated methods:
    template<bool IsBid>
    typename std::conditional_t<IsBid, BidsMap, AsksMap> const&
    GetSideMap()  const;

    //-----------------------------------------------------------------------//
    // Multi-Book Rep (for CME):                                             //
    //-----------------------------------------------------------------------//
//  OrderBook* const          m_multiDepthOB;   // Ptr to Dense OB
//  OrderBook* const          m_impliedOB;      // ditto
//  OrderBook* const          m_consolidtOB;    // ditto
//  OrderBook* const          m_l3OB;           // Dencs with OrdersLog

    //-----------------------------------------------------------------------//
    // Orders:                                                               //
    //-----------------------------------------------------------------------//
    // Array for collecting info on all individual Orders (required for correct
    // updating of OrderBooks when the Action is "Change" or "Delete"). Each in-
    // dex is an OrderID (in throughout enumeration within the TradingSession);
    // vals are Qtys of those Orders. May or may not be used  (in the latter ca-
    // se, the following flds remain 0).
    // XXX: It is currently assumed that all OrderIDs are numerical, and contig-
    // uous; if not, a HashTable will need to be used instead of an array:
    long                      m_nOrders;
    OrderBook::OrderInfo*     m_orders;

    //-----------------------------------------------------------------------//
    // Initialisation and Sequencing:                                        //
    //-----------------------------------------------------------------------//
    // XXX: At the moment, any "LastUpdate" data stored in the OrderBook are
    // Sequential ("SeqNum"s), NOT temporal ("utxx::time_val").   The latter
    // are managed separately, because there may be multiple TimeStamps asso-
    // ciated with a given OrderBook state (eg Exchange, Recv, Strategy etc):
    //
    mutable bool              m_initModeOver;     // Eg SnapShots are  done
    bool                      m_isInitialised;    // Full dynamic init done
    mutable SeqNum            m_lastUpdateRptSeq; // See above
    mutable SeqNum            m_lastUpdateSeqNum;
    mutable bool              m_lastUpdatedBid;

    //-----------------------------------------------------------------------//
    // Subscription Data:                                                    //
    //-----------------------------------------------------------------------//
    // Strategies subscribed to receive updates from this OrderBook (and/or
    // Trades with the same Symbol):
    using SubscrInfosVec =
          boost::container::static_vector<SubscrInfo, Limits::MaxStrats>;
    mutable SubscrInfosVec    m_subscrs;

    //-----------------------------------------------------------------------//
    // Logging (from MDC):                                                   //
    //-----------------------------------------------------------------------//
    spdlog::logger*    const  m_logger;           // Ptr NOT owned: From MDC
    int                const  m_debugLevel;       // From MDC

  public:
    //=======================================================================//
    // Default and Copy Ctor:                                                //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // Default Ctor: Constructs an invalid obj:                              //
    //-----------------------------------------------------------------------//
    OrderBook();

    //=======================================================================//
    // Non-Default Ctor, Dtor & Co:                                          //
    //=======================================================================//
    // Creates an empty OrderBook with Bids and Asks of the specified size. The
    // recommended size depends on how long the OrderBook is supposed to be in
    // use continuously without resetting it -- eg it could be up to 5% px mov-
    // ement per day, 12% per week, and so on. Because the total number of act-
    // ive "OrderBook"s would typically be small,   a sufficiently large number
    // of levels can be used:
    // NB: MDSubscrID (coming from the underlying protocol)  is  typically NOT
    // known at the point of OrderBook construction, or not used at all, so we
    // provide a separate method to set it later:
    OrderBook
    (
      EConnector_MktData const* a_mdc,
      SecDefD  const*           a_instr,
      bool                      a_is_full_amt,
      bool                      a_is_sparse,
      QtyTypeT                  a_qt,
      bool                      a_with_frac_qtys,
      bool                      a_with_seq_nums,   // From the enclosing
      bool                      a_with_rpt_seqs,   //   "EConnector_MktData"
      bool                      a_cont_rpt_seqs,   // ...
      int                       a_total_levels,    // Physical  rep
      int                       a_max_depth,       // Eg 1..50, or 0 for +oo
      long                      a_max_orders       // maximum number of orders
    );

    //-----------------------------------------------------------------------//
    // Get QtyTypeT of this OrderBook:                                       //
    //-----------------------------------------------------------------------//
    QtyTypeT GetQT() const { return m_qt; }

    //-----------------------------------------------------------------------//
    // Dtor:                                                                 //
    //-----------------------------------------------------------------------//
    ~OrderBook() noexcept;

    //-----------------------------------------------------------------------//
    // "Clear":                                                              //
    //-----------------------------------------------------------------------//
    // Removes all Qtys from all Bid and Ask px levels, but otherwise, the Book
    // structure remains intact. XXX: The following pragmas are to suppress mis-
    // placed GCC warnings:
    //
#   if defined(__GNUC__) && !defined(__clang__)
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wunused-parameter"
#   endif

    template<bool InitMode>
    UpdateEffectT Clear(SeqNum a_rpt_seq, SeqNum a_seq_num);

#   if defined(__GNUC__) && !defined(__clang__)
#   pragma GCC diagnostic pop
#   endif
    //-----------------------------------------------------------------------//
    // "Invalidate":                                                         //
    //-----------------------------------------------------------------------//
    // This is a strong form of "Clear" which is ALWAYS considered to be "Init-
    // Mode":
    // It resets the LastUpdate SeqNum and TimeStamps. Still, the allocated Si-
    // des are preserved, and a subsequent "Update" would be OK,  and will  re-
    // initialise the SeqNum. It does NOT reset the Protocol-level Subscr Info!
    //
    void Invalidate();

    //-----------------------------------------------------------------------//
    // Setters from "OrderBookBase" are NOT valid for "OrderBook" itself:    //
    //-----------------------------------------------------------------------//
    // XXX: Yes, we know, this violates the IS-A relationship:
    //
    template<bool IsRelaxed>
    bool SetBestBidPx(PriceT) const
         { throw utxx::logic_error("OrderBook::SetBestBidPx: Deleted"); }

    template<bool IsRelaxed>
    bool SetBestAskPx(PriceT) const
         { throw utxx::logic_error("OrderBook::SetBestAskPx: Deleted"); }

    //=======================================================================//
    // OrdserBook Updates:                                                   //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "Update" (Top-Level):                                                 //
    //-----------------------------------------------------------------------//
    // Add or Remove a new Qty Block (Order) at the specified Bid or Ask Px.
    // NB:
    // (*) The precise semantics of "SeqNum"s is protocol-dependent, and is on-
    //     ly known to the external caller; here we do some basic checks only:
    //     -- In the InitMode, SeqNums do not need to be monotonic (may all be
    //        same, or even randomly interleaved);
    //     -- otherwise, they must be strictly monotonic but  not  necessarily
    //        continuous;
    // (*) If individual Orders are maintained by the Caller ("WithOrdersLog"
    //     is set), "Update" is to be invoked on each single Order Insertion /
    //     Deletion / Modification, as given by the "a_action" param; otherwi-
    //     se, "a_action" should be UNDEFINED, and "Update"  would notionally
    //     maintain 1 order per px level;
    // (*) If "WithOrdersLog" is set, then "Qty" is assumed to be a Delta;
    //     otherwise, it is just a target Qty;
    // (*) If "SnapShotsOnly"  is set, there are no IncrUpdates in that case;
    // (*) If "IsRelaxed"      is set, we do not signal an ERROR if the Px is
    //     beyond the available slots (produce NONE instead), or if it cannot
    //     be rounded exactly to a PxStep multiplier (use the nearest integer
    //     instead):
    //
    template
    <
      bool     IsBid,
      bool     InitMode,
      bool     WithOrdersLog,
      bool     SnapShotsOnly,
      bool     IsRelaxed,
      bool     IsFullAmt,
      bool     IsSparse,   // TODO: 3-way Rep
      QtyTypeT QT,
      typename QR
    >
    UpdateEffectT Update
    (
      FIX::MDUpdateActionT a_action,    // Use UNDEFINED if not known
      PriceT               a_px,
      Qty<QT,QR>           a_qty,       // WithOrdersLog: Delta; other: TargQty
      SeqNum               a_rpt_seq,
      SeqNum               a_seq_num,
      OrderInfo*           a_order      // Order affected (iff WithOrdersLog)
    );

  private:
    //-----------------------------------------------------------------------//
    // "UpdateDenseSide":  Impl of "Update" for Dense (Array) Rep:           //
    //-----------------------------------------------------------------------//
    template
    <
      bool     IsBid,
      bool     WithOrdersLog,
      bool     SnapShotsOnly,
      bool     IsRelaxed,
      bool     IsFullAmt,
      QtyTypeT QT,
      typename QR
    >
    UpdateEffectT UpdateDenseSide
    (
      FIX::MDUpdateActionT a_action,    // Use UNDEFINED if not known
      PriceT               a_px,
      Qty<QT,QR>           a_qty,       // Delta or RealQty
      OrderInfo*           a_order      // Order affected (iff WithOrdersLog)
    );

    //-----------------------------------------------------------------------//
    // "UpdateSparseSide": Impl of "Update" for Sparse (Map) Rep:            //
    //-----------------------------------------------------------------------//
    // This is parameterised bu the Side Map type:
    template
    <
      bool     IsBid,
      bool     WithOrdersLog,
      bool     SnapShotsOnly,
      bool     IsRelaxed,
      bool     IsFullAmt,
      QtyTypeT QT,
      typename QR
    >
    UpdateEffectT UpdateSparseSide
    (
      FIX::MDUpdateActionT a_action,    // Use UNDEFINED if not known
      PriceT               a_px,
      Qty<QT,QR>           a_qty,       // Delta or RealQty
      OrderInfo*           a_order      // Order affected (iff WithOrdersLog)
    );

    //-----------------------------------------------------------------------//
    // "UpdateOBE": Common part of "UpdateDenseSide" and "UpdateSparseSide": //
    //-----------------------------------------------------------------------//
    template
    <
      bool     IsBid,
      bool     WithOrdersLog,
      bool     IsRelaxed,
      bool     IsSparse,
      QtyTypeT QT,
      typename QR
    >
    void UpdateOBE
    (
      FIX::MDUpdateActionT a_action,
      PriceT               a_px,
      Qty<QT,QR>           a_new_qty,
      OrderInfo*           a_order,
      OBEntry*             a_obe,
      UpdateEffectT*       a_res        // May be modified
    );

  public:
    //=======================================================================//
    // Accessors:                                                            //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // Top of the Book:                                                      //
    //-----------------------------------------------------------------------//
    template<QtyTypeT QT, typename QR>
    Qty<QT,QR> GetBestBidQty()     const;

    int        GetBestBidNOrders() const;
    OBEntry    GetBestBidEntry()   const;

    template<QtyTypeT QT, typename QR>
    Qty<QT,QR> GetBestAskQty()     const;

    int        GetBestAskNOrders() const;
    OBEntry    GetBestAskEntry()   const;

    //-----------------------------------------------------------------------//
    // Meta-Data:                                                            //
    //-----------------------------------------------------------------------//
    bool                  WithFracQtys() const { return  m_withFracQtys; }

    //-----------------------------------------------------------------------//
    // Subscription Info:                                                    //
    //-----------------------------------------------------------------------//
    SubscrInfosVec const& GetSubscrs()   const { return m_subscrs;   }

    //-----------------------------------------------------------------------//
    // "SeqNum" management:                                                  //
    //-----------------------------------------------------------------------//
    SeqNum GetLastUpdateRptSeq()         const { return m_lastUpdateRptSeq; }
    SeqNum GetLastUpdateSeqNum()         const { return m_lastUpdateSeqNum; }

    //=======================================================================//
    // Consistency Checking and Restoration:                                 //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "{Is,Set}Initialised", "IsReady":                                     //
    //-----------------------------------------------------------------------//
    // "IsReady" means that the OrderBook is fully ready for normal use:
    // (*) "m_isInitialised" flag must be set (it set by the EXTERNAL caller
    //     which knows when the dynamic initialisation procedure is done);
    // (*) the Book must have liquidity on both sides:
    // For "IsInitialised", only the former condition is sufficient:
    //
    bool IsInitialised() const
      { return m_isInitialised; }

    void SetInitialised()
      { m_isInitialised = true; }

    bool IsReady()       const;

    //-----------------------------------------------------------------------//
    // "CorrectBook":                                                        //
    //-----------------------------------------------------------------------//
    // For Inconsistent Books, throw away qtys at the oldest non-updated side
    // until Bid-Ask spread is restored.  It has no effect if invoked on cons-
    // istent book. Returns the side(s) affected by correction:
    //
    template<bool WithOrdersLog, QtyTypeT QT, typename QR>
    UpdatedSidesT CorrectBook();

    //-----------------------------------------------------------------------//
    // "ResetOrders":                                                        //
    //-----------------------------------------------------------------------//
    // Helper Func used by "CorrectBook": When a PxLevel is removed as stale,
    // any Orders belonging to that level must be cleared as well for consis-
    // tency:
    static void ResetOrders(OrderInfo* a_first_order);

    //=======================================================================//
    // Traversing the OrderBook:                                             //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "Traverse" (Generic):                                                 //
    //-----------------------------------------------------------------------//
    // "a_depth":    if 0, considered to be unlimited;
    // Action:       (int a_level, PriceT a_px, OBEntry const& a_entry) -> bool;
    // Return value: continue ("true") or stop traversing ("false"):
    //
    template<bool IsBid, typename Action>
    void Traverse(int a_depth, Action const& a_action) const;

    //-----------------------------------------------------------------------//
    // "GetMidPx":                                                           //
    //-----------------------------------------------------------------------//
    // Get the MidPx:
    //
    template<QtyTypeT QT, typename QR>
    PriceT GetMidPx(Qty<QT,QR> a_cum_vol = Qty<QT,QR>()) const;

    //-----------------------------------------------------------------------//
    // "Print" (to the specified depth or max depth available):              //
    //-----------------------------------------------------------------------//
    // XXX: This method is for debugging only -- it's HUGELY INEFFICIENT. Again,
    // Depth=0 means infinite depth:
    //
    template<QtyTypeT QT, typename QR>
    void Print(int a_depth) const;

    //-----------------------------------------------------------------------//
    // "ParamsVWAP":                                                         //
    //-----------------------------------------------------------------------//
    // For use with "GetVWAP" (see below). NB:
    // (*) All Qtys are represented as "long"s in order to simplify their use at
    //     the application level;
    // (*) but the semantics of them is still given by the "QT" param, because
    //     we do not want to mix up eg Lots and QtyA;
    // (*) For FullAmt (non-sweepable) OrderBooks,   "ParamsVWAP" must contain
    //     exactly 1 band:
    //
    template<QtyTypeT QT>
    struct ParamsVWAP
    {
      constexpr static int  MaxBands = 10;

      // Sequential Band Sizes (NOT cumulative!), from L1  into the OrderBook
      // Depth, for which the VWAPxs should be obtained:
      Qty<QT,long>  m_bandSizes[MaxBands];

      // Total size of known Market-like Orders which are active at the moment
      // of VWAP calculation. Those orders will eat up the OrderBook liquidity
      // from L1, so the corresp size is to be excluded from VWAP calculations:
      Qty<QT,long>  m_exclMktOrdsSz;

      // Known Limit Orders (pxs and sizes) which also need to be excluded from
      // the VWAP calculation. (These are possibly our own orders which need to
      // be "cut out"). XXX: For the moment, we allow for only 1 such order  --
      // ie no layering. If not used, set Px=NaN and Sz=0:
      PriceT        m_exclLimitOrdPx;
      Qty<QT,long>  m_exclLimitOrdSz;

      // Treatment of "possibly manipulative" orders: If, during the OrderBook
      // traversal (provided that the OrderBook in composed of individual Ord-
      // ers), we encounter a Px Level comprised of just 1 Order (either at L1
      // or at any level, depending on the param given below), this could be a
      // "manipulative" order, and its size is countered towards   VWAP with a
      // "reduction coeff" [0..1]. If particular, if the Reduction coeff is  0,
      // the corresp qty is not counted at all; if 1, it is counted in full as
      // normal:
      double  m_manipRedCoeff;
      bool    m_manipOnlyL1;

      // OUTPUT: Monotinic WorstPxs and VWAP values for each Band are placed
      // here. In particular, if the required band size(s) could be obtained
      // at any px, the corresp result is NaN:
      PriceT  m_wrstPxs[MaxBands];
      PriceT  m_vwaps  [MaxBands];

      // Default Ctor:
      ParamsVWAP()
      : m_bandSizes     {},
        m_exclMktOrdsSz (),                // 0
        m_exclLimitOrdPx(),                // NaN
        m_exclLimitOrdSz(),                // 0
        m_manipRedCoeff (1.0),             // Single orders are normal ones!
        m_manipOnlyL1   (false),
        m_wrstPxs       {},
        m_vwaps         {}
      {
        for (int i = 0;  i < MaxBands;  ++i)
        {
          m_bandSizes[i] = Qty<QT,long>(); // 0
          m_wrstPxs  [i] = PriceT();
          m_vwaps    [i] = PriceT();
        }
      }
    };

    //-----------------------------------------------------------------------//
    // "GetVWAP": The top-level VWAP calculating function:                   //
    //-----------------------------------------------------------------------//
    template
    <
      QtyTypeT  OBQT, typename  OBQR,
      QtyTypeT ArgQT, typename ArgQR,
      bool     IsBid
    >
    void GetVWAP(ParamsVWAP<ArgQT>* a_params) const;

    //-----------------------------------------------------------------------//
    // "GetVWAP1": A simple 1-band VWAP:                                     //
    //-----------------------------------------------------------------------//
    // Also assumes same Qty Type for the OrderBook and for the result. NB: The
    // resulting VWAP is expressed in the same units at the Instr Pxs   in this
    // OrderBook unless "ToPxAB" us set, in which case the result is in Px(A/B):
    //
    template
    <
      QtyTypeT  OBQT, typename  OBQR,
      bool     IsBid,
      QtyTypeT ArgQT, typename ArgQR, // May be inferred from the arg
      bool     ToPxAB = false
    >
    PriceT GetVWAP1(Qty<ArgQT,ArgQR> a_cum_vol) const;

    //-----------------------------------------------------------------------//
    // "GetDeepestPx":                                                       //
    //-----------------------------------------------------------------------//
    // Similar to "GetVWAP1", but returns the deepest (furthest from the Top of
    // the Book) Px for a given CumVol:
    template
    <
      QtyTypeT  OBQT, typename  OBQR,
      bool     IsBid,
      QtyTypeT ArgQT, typename ArgQR, // May be inferred from the arg
      bool     ToPxAB = false
    >
    PriceT GetDeepestPx(Qty<ArgQT,ArgQR> a_cum_vol) const;

  private:
    //-----------------------------------------------------------------------//
    // Internal templated implementation of "GetVWAP":                       //
    //-----------------------------------------------------------------------//
    template
    <
      QtyTypeT OBQT,
      typename OBQR,
      QtyTypeT ArgQT,
      typename ArgQR,
      bool     IsBid,
      bool     IsFullAmt,
      bool     IsSparse
    >
    void GetVWAP_Impl(ParamsVWAP<ArgQT>* a_params) const;

    //-----------------------------------------------------------------------//
    // Internal implementation of "GetVWAP1" and "GetDeepestPx":             //
    //-----------------------------------------------------------------------//
    enum class XPxMode { VWAP1, DeepestPx };
    template
    <
      XPxMode   Mode,
      QtyTypeT  OBQT, typename  OBQR,
      bool     IsBid,
      QtyTypeT ArgQT, typename ArgQR, // May be inferred from the arg
      bool     ToPxAB = false
    >
    PriceT GetXPx(Qty<ArgQT,ArgQR> a_cum_vol) const;

  public:
    //=======================================================================//
    // Subscription Services:                                                //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "AddSubscr":                                                          //
    //-----------------------------------------------------------------------//
    // Attach a Strategy to receive Update events from this OrderBook (and pos-
    // sibly Trade events from the corresp Symbol as well).
    // XXX: Currently, if a finite (non-0) MktDepth is used, we assume that the
    // Max Depth must correspond to the very 1st subscription (otherwise there
    // may be problems with amending subscriptions via the Underlying Protocol);
    // so if a new subscriber to this OrderBook requests a larger-than-existing
    // MktDepth, an exception is thrown:
    //
    void AddSubscr
    (
      Strategy*     a_strat,
      UpdateEffectT a_min_cb_level
    );

    //-----------------------------------------------------------------------//
    // "RemoveSubscr":                                                       //
    //-----------------------------------------------------------------------//
    // Returnes "true" if the corresp subscription was indeed found, and has
    // been removed:
    //
    bool RemoveSubscr(Strategy const* a_strat);

    //-----------------------------------------------------------------------//
    // "RemoveAllSubscrs":                                                   //
    //-----------------------------------------------------------------------//
    // Remove ALL subscribed Strategies -- eg if the corresp Connector is to be
    // shut down.
    // Returns "true" if any Strategy was really removed from the subscr list,
    // and "false" if the list was already empty:
    //
    bool RemoveAllSubscrs();

    //-----------------------------------------------------------------------//
    // "GetOrderInfo" (by OrderID):                                          //
    //-----------------------------------------------------------------------//
    // Normally, returns a non-NULL ptr to the OrderInfo slot (which may be em-
    // pty yet). Returns NULL if an error occurs -- no exceptions:
    //
    OrderBook::OrderInfo const* GetOrderInfo(OrderID a_order_id) const;

  private:
    //-----------------------------------------------------------------------//
    // "GetPxStepMultiple":                                                  //
    //-----------------------------------------------------------------------//
    // Up to an acceptable error bound (unless "IsRelaxed" is set), "a_numer"
    // must be a multiple of "PxStep". If yes, return that multiple. If not,
    // throw an exception:
    //
    template<bool IsRelaxed>
    int GetPxStepMultiple(double a_numer) const;

    //-----------------------------------------------------------------------//
    // "CheckSeqNums":                                                       //
    //-----------------------------------------------------------------------//
    // NB: This check is either not called at all, or we require strictly mono-
    // tonic SeqNums:
    // Returns "false" if we simply disallow the operation (but there are no
    // inconsistencies),  or throws an exception on a real invariant violation:
    //
    template<bool InitMode>
    void CheckSeqNums
    (
      SeqNum      a_rpt_seq,
      SeqNum      a_seq_num,
      char const* a_where
    )
    const;

    //-----------------------------------------------------------------------//
    // "VerifyQty": Make a Non-Negative Qty:                                 //
    //-----------------------------------------------------------------------//
    template
    <
      bool     IsBid,
      QtyTypeT QT,
      typename QR
    >
    Qty<QT,QR> VerifyQty(Qty<QT,QR> a_qty, PriceT a_px) const;

    //-----------------------------------------------------------------------//
    // "CheckOBEntry":                                                       //
    //-----------------------------------------------------------------------//
    // Checks consistency of Aggregated and OrdersLog-based Qtys (if used). XXX:
    // This check is VERY expensive, so it should be used for debugging purp-
    // oses only! The template param is for optimisation only; it must be same
    // as "m_withFracQtys";
    // if "a_obe" is NULL, then "a_is_bid" and "a_px" arew used to find the re-
    // quired Entry; otherwise, the latter 2 params are for logging only:
    //
    template
    <
      bool     DeepCheck,
      bool     IsRelaxed,
      bool     IsSparse,
      QtyTypeT QT,
      typename QR
    >
    bool CheckOBEntry(OBEntry* a_obe, bool a_is_bid, PriceT a_px) const;

    //-----------------------------------------------------------------------//
    // Depth Mgmt:                                                           //
    //-----------------------------------------------------------------------//
    template
    <
      bool     IsBid,
      bool     WithOrdersLog,
      QtyTypeT QT,
      typename QR
    >
    void IncrementDepth();

    template<bool IsBid>
    void DecrementDepth();
  };
} // End namespace MAQUETTE
