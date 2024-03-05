// vim:ts=2:et:sw=2
//===========================================================================//
//                             "OrderBook.h":                                //
//                Low-Level Order Book Types and Functions                   //
//===========================================================================//
// NB: This is a "low-level" implementation of a single Order Book. It provides
// reasonably efficient update mechanisms which are close to the Connector mes-
// saging.  In particular, in general it does NOT guarantee the monotonicity of
// Bid and Ask prices.
// Use "OrderBooksShM" for high-level view of the whole collection or Order Bks
// including subscription / unsubscription mechanisms, shared memory and notif-
// ications:
//
#ifndef MAQUETTE_STRATEGYADAPTOR_ORDERBOOK_H
#define MAQUETTE_STRATEGYADAPTOR_ORDERBOOK_H

#include "Common/Maths.h"
#include "StrategyAdaptor/EventTypes.h"
#include "Connectors/OrderBookPressureCounter.h"
#include <utxx/running_stat.hpp>
#include <utxx/time_val.hpp>
#include <boost/pool/pool_alloc.hpp>
#include <utility>
#include <string>
#include <stdexcept>
#include <cassert>
#include <sys/time.h>

namespace MAQUETTE
{
  //=========================================================================//
  // "OrderBook" Struct:                                                     //
  //=========================================================================//
  // Each OrderBook entry is a pair <Price, Qty>, plus some extra info.
  // This strucure is suitable for Shared Memory allocations:
  //
  class  OrderBook
  {
  public:
    //-----------------------------------------------------------------------//
    // Externally-Visible Data Types and Flds:                               //
    //-----------------------------------------------------------------------//
    // XXX: Here Qty is "long" and SeqNum is defined in "Events.h" as "unsigned
    // long"; the reason for that is that Qties may be subtracted, whereas Seq-
    // Nums are not subject to any arithmetic operations (on the Strategy side
    // at least):
    //
    static int const MaxDepth = 50;  // Absolute maximum for all OBs

    //-----------------------------------------------------------------------//
    // "Entry":                                                              //
    //-----------------------------------------------------------------------//
    struct    Entry
    {
      double  m_price;
      long    m_qty;

      // Default Ctor:
      Entry()
      : m_price(Arbalete::NaN<double>),
        m_qty  (0)
      {}

      // XXX: Equality is not automatically derived???
      bool operator==(Entry const& right) const
        { return m_price == right.m_price && m_qty == right.m_qty; }

      bool operator!=(Entry const& right) const
        { return m_price != right.m_price || m_qty != right.m_qty; }
    };

    //-----------------------------------------------------------------------//
    // For OrderID-based API:                                                //
    //-----------------------------------------------------------------------//
    // Same with 3 flds (including the Side). No equality is required:
    struct    En3
    {
      bool    m_isBid;
      double  m_price;
      long    m_qty;
    };

    // A map OrderID => Entry: Individual orders by order ID. May or may not
    // be used. If used, then the "NewOrder" / "ModifyOrder" / "DeleteOrder"
    // API functions are used:
    typedef
      typename boost::fast_pool_allocator<std::pair<Events::OrderID,  En3>>
      IDEnAlloc;

    typedef
      std::map<Events::OrderID, En3, std::less<Events::OrderID>, IDEnAlloc>
      IDMap;

    //-----------------------------------------------------------------------//
    // Order book update action:                                             //
    //-----------------------------------------------------------------------//
    enum class Action
    {
      New    = 0,       // Create a  new price level, shifting others
      Change = 1,       // Amend  an existing price level (no shifts)
      Delete = 2        // Delete an existing price level
    };

    static std::string ToString(Action);

  private:
    //-----------------------------------------------------------------------//
    // Types of the Stats (incl Vols) Estimators:                            //
    //-----------------------------------------------------------------------//
    typedef utxx::basic_moving_average<double, 0, true> RunningStats;
    // Max allowed window size = 64 sec, granularity = 32 slots/sec:
    typedef OrderBookPressureCounter  <64, 32>          PressureCounter;

    //-----------------------------------------------------------------------//
    // Internals:                                                            //
    //-----------------------------------------------------------------------//
    // NB:
    // (*) "m_lastUpdate" is the "SeqNum" of the last update when this book was
    //     was actually changed or attempted to be changed (even if the update
    //     attempt has actually failed, and the book was left unchanged);
    // (*) "m_upToDateAs" is the "SeqNum" of the last state which this book ad-
    //     equately represents; it is NOT changed if an update attempt fails,
    //     but it may be changed by an external call
    // That is:
    // (*) if the book was changed at N1 and then there were no further updates
    //     trough N2, then m_lastUpdate = N1 (set by the "Update" method on the
    //     book), and m_upToDateAs = N1 at that moment,  but then increased  to
    //     N2 by some external process;
    // (*) so under normal conditions:
    //     m_lastUpdate <= m_upToDateAs;
    //
    // (*) if the update at N1 has failed, then m_lastUpdate = N1 still, but
    //     the previous "m_upToDateAs" value is NOT increased;
    // (*) so we get (for an out-of-sync book):
    //     m_lastUpdate >  m_upToDateAs,
    //     and this can be used to detect update failure and re-sync the book:
    //
    Events::SecID              m_secID;
    Events::SymKey             m_symbol;
    int                        m_depth;          // Depth of this OrderBook
    utxx::time_val             m_eventTS;        // TimeStamp of last Exch Event
    int                        m_currBids;       // Curr number of Bids
    Entry                      m_bids[MaxDepth]; // Bids in the Desc price order
    int                        m_currAsks;       // Curr number of Asks
    Entry                      m_asks[MaxDepth]; // Asks in the Asc  price order
    Events::SeqNum             m_lastUpdate;     // See above
    Events::SeqNum             m_upToDateAs;     // ...
    utxx::time_val             m_recvTime;       // Corrersp to "m_lastUpdate"
    bool                       m_lastUpdateWasBid;
    int                        m_debug;          // Debug level
    std::unique_ptr<IDEnAlloc> m_idAlloc;        // For the OrderID-based API
    std::unique_ptr<IDMap>     m_idMap;          // ditto
    RunningStats               m_bidStats;       // Vols etc for Best Bid
    RunningStats               m_askStats;       // Vols etc for Best Ask
    PressureCounter            m_pressureCounter;// Trend pressure counter
    double                     m_volCoeff;
    Entry                      m_prevBestBid;    // Used to update RunningStats
    Entry                      m_prevBestAsk;    //

    // Deleted / Hidden:
    //
    OrderBook()                             = delete;
    OrderBook(OrderBook const&)             = delete;
    OrderBook& operator= (OrderBook const&) = delete;
    bool       operator==(OrderBook const&) const;
    bool       operator!=(OrderBook const&) const;

  public:
    //-----------------------------------------------------------------------//
    // Ctors, Dtor:                                                          //
    //-----------------------------------------------------------------------//
    // Non-Default Ctor:
    // NB: Upon construction, the "m_initialised" flag is NOT set:
    //
    OrderBook
    (
      Events::SecID         sec_id,
      Events::SymKey const& symbol,
      int                   depth,
      bool                  with_ids,
      int                   vol_window,
      int                   press_win_sec,
      int                   debug,
      double                min_price_incr = 0.0
    );

    // Move Ctor:
    OrderBook(OrderBook&&);

    // Dtor:
    ~OrderBook();

    //-----------------------------------------------------------------------//
    // OrderID-Based API: Requires "with_ids" to be set in Ctor:             //
    //-----------------------------------------------------------------------//
    // These functions return the Level at which the order was Installed /
    // Modified / Deleted, or (-1) if the operation has failed:
    //
    int NewOrder
    (
      Events::OrderID       order_id,
      bool                  is_bid,
      double                price,
      long                  qty,
      utxx::time_val        recv_time,
      utxx::time_val        event_ts,
      Events::SeqNum        seq_num         // Use 0 if unknown
    );

    int DeleteOrder
    (
      Events::OrderID       order_id,
      utxx::time_val        recv_time,
      utxx::time_val        event_ts,
      Events::SeqNum        seq_num,        // Use 0 if unknown
      En3*                  what_deleted = nullptr
    );

    int ModifyOrder
    (
      Events::OrderID       order_id,
      long                  new_qty,
      utxx::time_val        recv_time,
      utxx::time_val        event_ts,
      Events::SeqNum        seq_num         // Use 0 if unknown
    );

    std::pair<bool, int> UpsertOrder
    (
      Events::OrderID       order_id,
      bool                  is_bid,
      double                price,
      long                  qty,
      utxx::time_val        recv_time,
      utxx::time_val        event_ts,
      Events::SeqNum        seq_num         // Use 0 if unknown
    );

    //-----------------------------------------------------------------------//
    // "Update":                                                             //
    //-----------------------------------------------------------------------//
    // (1) If the Level and Action are not specified: They will be determined
    //     dynamically:
    // (*) the level will be found according to the "price";
    // (*) the action: if the qty is 0 and the exact price level exists, then
    //     Delete; otherwise New or Change (depending on the level existence).
    // If this request cannot be satisfied (e.g. price beyond all levels, or
    //     qty=0 but exact price not found), no exception is thrown but a war-
    //     ning is logged.
    // Returns the Level at which the OrderBook was updated, or (-1) of the
    // "Update" has failed:
    //
    int Update
    (
      bool                  is_bid,
      double                price,          // Must be >  0
      long                  qty,            // Must be >= 0; 0 implies "Delete"
      bool                  qyu_is_delta,   // For use with OrderID API
      utxx::time_val        recv_time,
      utxx::time_val        event_ts,
      Events::SeqNum        seq_num         // Use 0 if unknown
    );

    // (2) If the Level (1 .. Depth) and Action are specified:
    //     Returns "true" iff the operation has succeeded:
    bool Update
    (
      bool                  is_bid,
      int                   level,
      Action                action,
      double                price,          // Must be >= 0; 0 OK for  "Delete"
      long                  qty,            // Must be >= 0; 0 implies "Delete"
      utxx::time_val        recv_time,
      utxx::time_val        event_ts,
      Events::SeqNum        seq_num         // Use 0 if unknown
    );

    //-----------------------------------------------------------------------//
    // "Clear":                                                              //
    //-----------------------------------------------------------------------//
    // This is a normal market event. Always succeeds:
    //
    void Clear
    (
      utxx::time_val        recv_time,
      utxx::time_val        event_ts,
      Events::SeqNum        seq_num         // Use 0 if unknown
    );

    //-----------------------------------------------------------------------//
    // "Invalidate":                                                         //
    //-----------------------------------------------------------------------//
    // The Book is cleared AND marked as not-up-to-date:
    //
    void Invalidate(Events::SeqNum = 0);

    //-----------------------------------------------------------------------//
    // "SeqNum"s Management:                                                 //
    //-----------------------------------------------------------------------//
    Events::SeqNum GetLastUpdate() const { return m_lastUpdate; }
    Events::SeqNum GetUpToDateAs() const { return m_upToDateAs; }

    // (*) "m_lastUpdate" is set by "Update" / "Delete" / "Clear" methods, so
    //     there is no setter for it;
    // (*) "m_upToDateAs" can only be set  by an external caller  which KNOWS
    //     that there were no further relevant updates to this book up to and
    //     including the given SeqNum, hence the following method:
    //
    void SetUpToDateAs(Events::SeqNum seq_num) { m_upToDateAs = seq_num; }

    bool IsUpToDate() const
    {
      return m_lastUpdate > 0 && m_upToDateAs > 0 &&
             m_lastUpdate     <= m_upToDateAs;
    }

    //-----------------------------------------------------------------------//
    // Accessors:                                                            //
    //-----------------------------------------------------------------------//
    // "GetTimeStamp":
    // Returns the time stamp of the (presumably latest) event recorded in the
    // OrderBook:
    //
    utxx::time_val TimeStamp() const   { return m_eventTS;  }

    // "RecvTime":
    utxx::time_val RecvTime()  const   { return m_recvTime; }

    // "GetBids":
    // Returns curr number of Bids, and a ptr to them. Both read-only and read-
    // write viewes are provided:
    //
    std::pair<Entry const*, int> GetBids() const        // RO
      { return std::make_pair(m_bids, m_currBids); }

    std::pair<Entry*, int> GetBids()                    // RW
      { return std::make_pair(m_bids, m_currBids); }

    // "GetAsks":
    // Similar to "GetBids":
    //
    std::pair<Entry const*, int> GetAsks() const        // RO
      { return std::make_pair(m_asks, m_currAsks); }

    std::pair<Entry*, int> GetAsks()
      { return std::make_pair(m_asks, m_currAsks); }    // RW

    // "GetMktDepth":
    int                   GetMktDepth()  const { return m_depth;  }
    Events::SecID         GetSecID()     const { return m_secID;  }
    Events::SymKey const& GetSymbol()    const { return m_symbol; }
    // "LastUpdateWasBid":
    bool LastUpdateWasBid() const { return m_lastUpdateWasBid; }

    //-----------------------------------------------------------------------//
    // Integrity Verification:                                               //
    //-----------------------------------------------------------------------//
    // Result: OK or not (errors / warnings are logged):
    //
    bool Verify(Events::SeqNum seq_num, bool detailed);

    //-----------------------------------------------------------------------//
    // Misc Utils:                                                           //
    //-----------------------------------------------------------------------//
    void Print() const;

    //-----------------------------------------------------------------------//
    // Updating the Stats:                                                   //
    //-----------------------------------------------------------------------//
    // NB: this is never done by default when the OrderBook is updated, because
    // it may contain inconsistent data (eg Bid / Ask collission) which will be
    // cleared later.   Instead, an external call to "UpdateStats" is required,
    // when the caller knows it is OK to do so. This is why  best{Bid,Ask}  are
    // passed as params, NOT taken from the OrderBook itself  --  the  external
    // caller needs to resolve collisions first:
    //
    void UpdateStats
    (
      utxx::time_val        a_now,
      Entry          const& a_best_bid,
      Entry          const& a_best_ask
    );

    //-----------------------------------------------------------------------//
    // Extracting the Statistics:                                            //
    //-----------------------------------------------------------------------//
    // The caller should NOT use the values returned by Get{Bid,Ask}Vol()
    // unless Is{Bid,Ask}VolReady() is true:
    //
    bool IsBidVolReady() const
      { return m_bidStats.size() == m_bidStats.capacity(); }

    bool IsAskVolReady() const
      { return m_askStats.size() == m_askStats.capacity(); }

    // NB: The following methods return UNBIASED range-based estimators for the
    // vols themselves (rather than estimates for the variances). NB:  the vols
    // returned are for the Geometric Brownian Motion, ie thet are for d(Px)/Px
    // rather than for d(Px) itself:
    //
    double GetBidVol()   const
    {
      return Arbalete::Log(m_bidStats.max() / m_bidStats.min()) * m_volCoeff;
    }

    double GetAskVol()   const
    {
      return Arbalete::Log(m_askStats.max() / m_askStats.min()) * m_volCoeff;
    }

    // Trend Pressure:
    double GetTrendPressure()
      { return m_pressureCounter.Pressure(); }

  private:
    //-----------------------------------------------------------------------//
    // "Delete":                                                             //
    //-----------------------------------------------------------------------//
    // NB: If "price" and/or "qty" are not 0, they are verified in the process
    // of deletion (a warning is produced if they do not match, and "false" is
    // returned if the operation did not succeed):
    //
    bool Delete
    (
      bool                  is_bid,
      int                   level,
      double                price,
      int                   qty,
      utxx::time_val        recv_time,
      utxx::time_val        event_ts,
      Events::SeqNum        seq_num     // Use 0 if unknwon
    );

    //-----------------------------------------------------------------------//
    // Utils:                                                                //
    //-----------------------------------------------------------------------//
    bool VerifySide(Events::SeqNum seq_num, bool is_bid) const;
  };

  //=========================================================================//
  // Helper Types:                                                           //
  //=========================================================================//
  typedef std::map<Events::SecID, OrderBook> OrderBooksT;
}
#endif  // MAQUETTE_STRATEGYADAPTOR_ORDERBOOK_H
