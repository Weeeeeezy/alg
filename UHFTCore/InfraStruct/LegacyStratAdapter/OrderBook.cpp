// vim:ts=2:et:sw=2
//===========================================================================//
//                             "OrderBook.cpp":                              //
//                Top-Level Order Book Types and Functions                   //
//===========================================================================//
#include "StrategyAdaptor/OrderBook.h"
#include <utxx/compiler_hints.hpp>
#include <Infrastructure/Logger.h>
#include <Infrastructure/Logger.h>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <stdexcept>
#include <cassert>
#include <cstddef>    // For "offsetof"

namespace
{
  // Thereshold (Tolerance) used for Equality Comaprison of Prices:
  // The following value is currently known to be sufficient for all asset
  // classes, as the minimum price increment (in FX) is 1e-5. On the other
  // hand, this value is large enough not to be affected by rounding errors:
  //
  double const PxTol = 1e-6;
}

namespace
{
  using namespace MAQUETTE;
  using namespace Arbalete;
  using namespace std;

  //=========================================================================//
  // "DeleteEntry":                                                          //
  //=========================================================================//
  // (Shifting the levels donw...):
  //
  inline void DeleteEntry(OrderBook::Entry* entrs, int level, int* n)
  {
    assert(entrs != NULL && n != NULL && 0 <= level && level < (*n));

    // Move all entries beginning from (level + 1), downwards:
    for (int i = level + 1; i < (*n); ++i)
      entrs[i-1] = entrs[i];

    // In any case, the curr size is reduced: as we had 0 <= level and
    // level < n, that is level <= n-1,   thus 0 <= level <= n-1, thus
    // n >= 1, so can indeed decrement it:
    (*n)--;
    assert(0 <= (*n));
  }

  //=========================================================================//
  // "ShiftLevelsUp":                                                        //
  //=========================================================================//
  inline void ShiftLevelsUp
    (OrderBook::Entry* entrs, int level, int* n, int depth)
  {
    assert(entrs != NULL && n != NULL && 0 <= level && level < (*n) &&
           (*n)  <= depth);

    // "last" is the last level to be shifted up (to become (last+1)):
    // thus the conditions:
    //  last     <= n - 1
    //  last + 1 <= depth - 1, thus last <= depth - 2:
    //
    int last = min<int>((*n)-1, depth-2);

    for (int i = last; i >= level; --i)
      entrs[i+1] = entrs[i];

    if ((*n) < depth)
    {
      // The curr size increases. This means:  n <= depth-1, n-1 <= depth-2,
      // therefore last=n-1,
      // and it was shifted to last+1 = n, and n <= depth-1, so can indeed
      // increment n:
      (*n)++;
      assert((*n) <= depth);
    }
  }

  //=========================================================================//
  // "CheckBidAsk":                                                          //
  //=========================================================================//
  template<bool IsBid>
  inline void CheckBidAsk
  (
    OrderBook::Entry const* thisSide,
    int                     thisN,
    OrderBook::Entry*       oppSide,
    int*                    oppN,
    OrderBook::IDMap*       idMap,
    bool                    debug,
    Events::SymKey const&   symbol
  )
  {
    // Best price on this side:
    assert(thisSide != NULL && oppSide != NULL && oppN != NULL &&
           idMap    != NULL);
    if (thisN == 0  || (*oppN) == 0)
      return;
    double bestPx = thisSide[0].m_price;

    // Count potentially "offending" entries from the opposite side:
    int off = 0;
    while ((off < *oppN) &&
           (( IsBid && (bestPx - oppSide[off].m_price) > -PxTol) ||
            (!IsBid && (oppSide[off].m_price - bestPx) > -PxTol)))
      off++;

    // If there are indeed some "offending" opposite entries: Remove them:
    if (off >= 1)
    {
      // Max/min offending px:
      assert(off <= (*oppN));
      double worstPx = oppSide[off-1].m_price;

      if (debug)
        LOG_WARNING("CheckBidAsk: %s: %.5f purged %d %s entries to avoid "
                    "collision (oppos side was %.5f, worstPx=%.5f)\n",
                    Events::ToCStr(symbol),  bestPx, off,
                    (IsBid ? "BID" : "ASK"), oppSide[0].m_price, worstPx);

      // Shift the opposite side (NB: using "memcpy" is OK here in spite of
      // dest / src overlap):
      memcpy((void*)(oppSide), (void*)(oppSide + off),
             ((*oppN) - off) *  sizeof(OrderBook::Entry));
      *oppN -= off;

      // Remove all IDs with "offending" pxs from the IDMap:
      for (OrderBook::IDMap::iterator it = idMap->begin(); it != idMap->end(); )
      {
        OrderBook::En3 const& en3 = it->second;
        if (en3.m_isBid != IsBid &&
           (( IsBid && en3.m_price <= worstPx) ||  // Opposite = Asks
            (!IsBid && en3.m_price >= worstPx)))   // Opposite = Bids
        {
          // "it" is to be deleted:
          OrderBook::IDMap::iterator nxt = next(it);
          idMap->erase(it);
          it = nxt; // Original "it" was invalidated
        }
        else
          ++it;     // Go on...
      }
      // All done!
    }
  }
}

namespace MAQUETTE
{
  using namespace std;

  //=========================================================================//
  // Macros:                                                                 //
  //=========================================================================//
  // Check if SeqNum is NON-DECREASING -- refuse to proceed if it is not (so if
  // the SeqNums have been reset, the books must be reset as well).
  // In that case, WE DO NOT EVEN SET "m_lastUpdate". But all-0s are allowed if
  // "SeqNum"s are not maintained at all.
  // NB: "SeqNums" are NOT REQUIRED TO BE STRICTLY INCREASING, because multiple
  // eg "Update" calls may originate from an update msg with the same SeqNum
  // (say, for different levels).
  // This error can only occur during application of SnapShots, because for all
  // Incremental Updates, SeqNums are carelully managed by the Caller:
  //
# define CHECK_SEQ_NUM(SeqNum, Where, FailValue) \
  if ((SeqNum != 0 || m_lastUpdate != 0     || m_upToDateAs != 0) &&    \
      (SeqNum <= 0 || SeqNum < m_lastUpdate || SeqNum < m_upToDateAs)) \
  { \
    SLOG(ERROR) << "OrderBook::" Where ": Symbol=" \
               << Events::ToCStr(m_symbol)        \
               << ": UnOrdered SeqNum=" << SeqNum << ", LastUpdate=" \
               << m_lastUpdate << ", UpToDateAs=" << m_upToDateAs << endl; \
    return FailValue; \
  }

  //=========================================================================//
  // "ToString":                                                             //
  //=========================================================================//
  string OrderBook::ToString(Action action)
  {
    switch (action)
    {
      case Action::New:    return string("New");
      case Action::Change: return string("Change");
      case Action::Delete: return string("Delete");
      default:             assert(false); return string();
    }
  }

  //=========================================================================//
  // Non-Default Ctor:                                                       //
  //=========================================================================//
  OrderBook::OrderBook
  (
    Events::SecID         sec_id,
    Events::SymKey const& symbol,
    int                   depth,
    bool                  with_ids,
    int                   vol_window,
    int                   press_win_sec,
    int                   debug,
    double                min_price_incr
  )
  : m_secID           (sec_id),
    m_symbol          (symbol),
    m_depth           (depth),
    m_eventTS         (),
    m_currBids        (0),
    m_currAsks        (0),
    m_lastUpdate      (0),
    m_upToDateAs      (0),
    m_lastUpdateWasBid(false),
    m_debug           (debug),
    m_idAlloc         (with_ids ? (new IDEnAlloc) : NULL),
    m_idMap           (with_ids
                       ? (new IDMap(less<Events::OrderID>(), *m_idAlloc))
                       : NULL),
    m_bidStats        (vol_window),
    m_askStats        (vol_window),
    m_pressureCounter (press_win_sec),
    m_volCoeff        (SqRt(Pi<double>() / (8.0 * double(vol_window)))),
    m_prevBestBid     (),
    m_prevBestAsk     ()
  {
    if (depth <= 0 || depth > MaxDepth)
      throw invalid_argument
            ("OrderBook::Ctor: Invalid MktDepth: "+ to_string(depth));

    // Zero-out the Bids and Asks:
    memset((char*)(m_bids), '\0', sizeof(Entry));
    memset((char*)(m_asks), '\0', sizeof(Entry));
  }

  //=========================================================================//
  // Move Ctor:                                                              //
  //=========================================================================//
  OrderBook::OrderBook(OrderBook&& right)
  : m_secID           (right.m_secID),
    m_symbol          (right.m_symbol),
    m_depth           (right.m_depth),
    m_eventTS         (right.m_eventTS),
    m_currBids        (right.m_currBids),
    m_currAsks        (right.m_currAsks),
    m_lastUpdate      (right.m_lastUpdate),
    m_upToDateAs      (right.m_upToDateAs),
    m_recvTime        (right.m_recvTime),
    m_lastUpdateWasBid(right.m_lastUpdateWasBid),
    m_debug           (right.m_debug),
    m_idAlloc         (std::move(right.m_idAlloc)),
    m_idMap           (std::move(right.m_idMap)),
    m_bidStats        (std::move(right.m_bidStats)),
    m_askStats        (std::move(right.m_askStats)),
    m_pressureCounter (right.m_pressureCounter),
    m_volCoeff        (right.m_volCoeff),
    m_prevBestBid     (right.m_prevBestBid),
    m_prevBestAsk     (right.m_prevBestAsk)
  {
    // Copy the actual OrderBook Sides:
    // (1) Bids:
    assert(0 <= m_currBids && m_currBids <= MaxDepth);

    memcpy((void*)(m_bids), (void const*)(right.m_bids),
           m_currBids * sizeof(Entry));

    memset((void*)(m_bids + m_currBids), '\0',
           (MaxDepth - m_currBids) * sizeof(Entry));

    // (2) Asks:
    assert(0 <= m_currAsks && m_currAsks <= MaxDepth);

    memcpy((void*)(m_asks), (void const*)(right.m_asks),
           m_currAsks * sizeof(Entry));

    memset((void*)(m_asks + m_currAsks), '\0',
           (MaxDepth - m_currAsks) * sizeof(Entry));

    // XXX: "right" is not cleaned up -- otherwise getting compilation trouble
    // with "offsetof"...
  }

  //=========================================================================//
  // Dtor:                                                                   //
  //=========================================================================//
  OrderBook::~OrderBook()
  {
    Invalidate();
    // NB: "m_idMap" and "m_idAlloc" are finalised automatically
  }

  //=========================================================================//
  // "Clear":                                                                //
  //=========================================================================//
  // Clears an existing book, but it remains valid, and the "SeqNum"s properly
  // updated. NB: Here "seq_num" is NOT checked -- always set as it stands:
  //
  void OrderBook::Clear
  (
    utxx::time_val        recv_time,
    utxx::time_val        event_ts,
    Events::SeqNum        seq_num
  )
  {
    m_eventTS          = event_ts;
    m_currBids         = 0;
    m_currAsks         = 0;

    m_recvTime         = recv_time;
    m_lastUpdate       = seq_num;
    m_upToDateAs       = seq_num;
    m_lastUpdateWasBid = false;

    // Just in case, zero-out the Bids and Asks:
    memset((char*)(m_bids), '\0', sizeof(Entry));
    memset((char*)(m_asks), '\0', sizeof(Entry));

    // Also remove all entries from "m_idMap"; but keep "m_idAlloc". NB: it is
    // NOT UNCOMMON to get m_idMap==NULL here, as Clear is a part of the Dtor,
    // and the Dtor can be called on an object which was previously devastated
    // as a "move" argument:
    //
    if (m_idMap != NULL)
      m_idMap->clear();

    // Clear the statistics as well:
    m_bidStats.clear();
    m_askStats.clear();
  }

  //=========================================================================//
  // "Invalidate":                                                           //
  //=========================================================================//
  void OrderBook::Invalidate(Events::SeqNum seq_num)
  {
    Clear({0, 0}, utxx::time_val(), seq_num);
  }

  //=========================================================================//
  // "NewOrder" (with OrderID):                                              //
  //=========================================================================//
  // Returns the insertion level (from 0), or (-1) in case of error:
  //
  int OrderBook::NewOrder
  (
    Events::OrderID       order_id,
    bool                  is_bid,
    double                price,
    long                  qty,
    utxx::time_val        recv_time,
    utxx::time_val        event_ts,
    Events::SeqNum        seq_num
  )
  {
    // Install this ID in the map -- it must not exist yet:
    if (utxx::unlikely(m_idMap == NULL))
      throw runtime_error("OrderBook::NewOrder: OrderIDs not supported");

    // Obviously, "qty" must be > 0:
    if (utxx::unlikely(qty <= 0))
    {
      if (m_debug)
        // NB: Here we do NOT convert "order_id" to hex:
        SLOG(WARNING) << "OrderBook::NewOrder: DROPPED: Symbol="
                     << Events::ToCStr(m_symbol)   << ", SecID="
                     << m_secID      << ", IsBid=" << is_bid
                     << ", OrderID=" << order_id   << ": Invalid Qty="
                     << qty          << endl;
      return -1;
    }

    En3  en3 { is_bid, price, qty };
    bool ins = (m_idMap->insert(make_pair(order_id, en3))).second;

    if (utxx::unlikely(!ins))
    {
      if (m_debug)
        // NB: Here we do NOT convert "order_id" to hex:
        SLOG(WARNING) << "OrderBook::NewOrder: DROPPED: Symbol="
                     << Events::ToCStr(m_symbol)   << ", SecID="
                     << m_secID      << ", IsBid=" << is_bid
                     << ", OrderID=" << order_id   << ": Already Exists"
                     << endl;
      return -1;
    }

    // Now actually update the OrderBook; SeqNum is not used. The "qty" provi-
    // ded is a delta to the qty at that price level:
    //
    return Update(is_bid, price, qty, true, recv_time, event_ts, seq_num);
  }

  //=========================================================================//
  // "DeleteOrder" (with OrderID):                                           //
  //=========================================================================//
  // Returns the deletion level (from 0), or (-1) in case of error. If "what_
  // deleted" is non-NULL, fills in more info about the order deleted:
  //
  int OrderBook::DeleteOrder
  (
    Events::OrderID       order_id,
    utxx::time_val        recv_time,
    utxx::time_val        event_ts,
    Events::SeqNum        seq_num,
    En3*                  what_deleted
  )
  {
    // Find the order by ID:
    if (utxx::unlikely(m_idMap == NULL))
      throw runtime_error("OrderBook::DeleteOrder: OrderIDs not supported");

    IDMap::iterator it = m_idMap->find(order_id);
    if (utxx::unlikely(it == m_idMap->end()))
    {
      if (m_debug)
        // NB: here we do NOT convert "order_id" to hex:
        SLOG(WARNING) << "OrderBook::DeleteOrder: DROPPED: Symbol="
                     << Events::ToCStr(m_symbol) << ", SecID=" << m_secID
                     << ", OrderID=" << order_id << ": Not Found"
                     << endl;
      if (what_deleted != NULL)
        *what_deleted = { 0 };
      return -1;
    }
    // Otherwise: Delete it from the OrderBook. Compute the Qty Delta:
    En3 const&   en3 = it->second;
    long   qty_delta = - en3.m_qty;

    if (utxx::unlikely(qty_delta >= 0))
      throw logic_error
            ("OrderBook::DeleteOrder: OrderID="     + to_string(order_id) +
             ", IsBid=" + to_string(en3.m_isBid)    + ", Px="             +
             to_string(en3.m_price) + ": QtyDelta=" + to_string(qty_delta));

    // Carry out the update:
    int res = Update(en3.m_isBid, en3.m_price, qty_delta, true, recv_time,
                     event_ts,    seq_num);

    if (what_deleted != NULL)
    {
      static const En3 s_empty = { 0 };
      *what_deleted = (res >= 0) ? en3 : s_empty;
    }

    // Delete the order from the IDMap as well:
    m_idMap->erase(it);
    return   res;
  }

  //=========================================================================//
  // "ModifyOrder" (with OrderID):                                           //
  //=========================================================================//
  // NB: It's only the Qty which can be modified:
  //
  int OrderBook::ModifyOrder
  (
    Events::OrderID       order_id,
    long                  new_qty,
    utxx::time_val        recv_time,
    utxx::time_val        event_ts,
    Events::SeqNum        seq_num
  )
  {
    // Find the order by ID:
    if (utxx::unlikely(m_idMap == NULL))
      throw runtime_error("OrderBook::ModifyOrder: OrderIDs not supported");

    // NB: new_qty==0 is not allowed (as well as < 0), since it would be a
    // "Delete" rather than "Modify":
    //
    if (utxx::unlikely(new_qty <= 0))
    {
      if (m_debug)
        SLOG(WARNING) << "OrderBook::ModifyOrder: DROPPED: Symbol="
                     << Events::ToCStr(m_symbol) << ", SecID=" << m_secID
                     << ", OrderID="             << order_id
                     << ": Invalid Qty="         << new_qty    << endl;
      return -1;
    }

    IDMap::iterator it = m_idMap->find(order_id);
    if (utxx::unlikely(it == m_idMap->end()))
    {
      if (m_debug)
        // NB: here we do NOT convert "order_id" to hex:
        SLOG(WARNING) << "OrderBook::ModifyOrder: DROPPED: Symbol="
                     << Events::ToCStr(m_symbol) << ", SecID="    << m_secID
                     << ", OrderID=" << order_id << ": Not Found" << endl;
      return -1;
    }
    // Compute the QtyDelta and perform "Update":
    En3 const&   en3 = it->second;
    long   qty_delta = new_qty - en3.m_qty;

    return Update(en3.m_isBid, en3.m_price, qty_delta, true, recv_time,
                  event_ts,    seq_num);
  }

  //=========================================================================//
  // "UpsertOrder" (with OrderID):                                           //
  //=========================================================================//
  /// Add or modify an order given its OrderID (combination of "NewOrder" and
  /// "ModifyOrder"). Unlike "Modify", "Upsert" allows us to change both Px &
  /// Qty of a given OrderID:
  std::pair<bool, int> OrderBook::UpsertOrder
  (
    Events::OrderID       order_id,
    bool                  is_bid,
    double                price,
    long                  qty,
    utxx::time_val        recv_time,
    utxx::time_val        event_ts,
    Events::SeqNum        seq_num
  )
  {
    // Install this ID in the map -- it must not exist yet:
    if (utxx::unlikely(m_idMap == NULL))
      throw runtime_error("OrderBook::UpsertOrder: OrderIDs not supported");

    // Obviously, "qty" must be > 0:
    if (utxx::unlikely(qty <= 0))
    {
      if (m_debug)
        // NB: Here we do NOT convert "order_id" to hex:
        SLOG(WARNING) << "OrderBook::UpsertOrder: DROPPED: Symbol="
                     << Events::ToString(m_symbol) << ", SecID="
                     << m_secID      << ", IsBid=" << is_bid
                     << ", OrderID=" << order_id   << ": Invalid Qty="
                     << qty          << endl;
      return std::make_pair(false, -1);
    }

    En3  newEn3    { is_bid, price, qty };
    auto res       = m_idMap->insert(make_pair(order_id, newEn3));
    bool inserted  = res.second;
    En3& en3       = res.first->second;
    long qty_delta = 0;

    if (inserted)
      // OrderID is not found, so definitely no update in-place. This means
      // that a new qty block will be inserted, "qty" is the increment of
      // total qty at the corresp price level:
      qty_delta = qty;
      // Fall through to "Update"
    else
    {
      // OrderID already exists. We need to consider 2 cases:
      //
      if (utxx::likely(Abs(price - en3.m_price) < PxTol))
        // (1) Same price: update of the qty to another qty at the same px:
        qty_delta = qty - en3.m_qty;
        // Fall through to "Update"
      else
      {
        // (2) Price changes (and possibly qty as well). Remove this order's
        // qty from the old price level:
        Update(is_bid, en3.m_price, -en3.m_qty, true, recv_time, event_ts, seq_num);

        en3.m_isBid = is_bid;
        en3.m_price = price;
        en3.m_qty   = qty;

        // For the new level, "qty_delta" is our order "qty":
        qty_delta   = qty;
      }
    }

    // Now update the OrderBook with "qty_delta":
    return std::make_pair
      (inserted,
       Update(is_bid, price, qty_delta, true, recv_time, event_ts, seq_num));
  }

  //=========================================================================//
  // "Update": The level is not specified:                                   //
  //=========================================================================//
  // The level will be determined automatically.   If the "qty" is 0, and the
  // exact price level is found, it will be deleted. If the request cannot be
  // carried out, it will be logged but no exception produced:
  //
  int OrderBook::Update
  (
    bool                  is_bid,
    double                price,
    long                  qty,
    bool                  qty_is_delta,
    utxx::time_val        recv_time,
    utxx::time_val        event_ts,
    Events::SeqNum        seq_num
  )
  {
    // The following pre-condition is verified by the Caller. NB: in some rare
    // cases, "price" can be negative as well (eg for FX swaps):
    CHECK_SEQ_NUM(seq_num, "Update(NoLevel)", -1)

    // If "qty" is not a delta, it must always be >= 0 (0 is for delete):
    if (utxx::unlikely(!qty_is_delta && qty < 0))
    {
      if (m_debug)
        SLOG(WARNING) << "OrderBook::Update: DROPPED: Symbol="
                     << Events::ToCStr(m_symbol) << ", SecID="   << m_secID
                     << ", IsBid=" << is_bid     << ", Px="      << price
                     << ", Qty="   << qty        << " (INVALID)" << endl;
      return -1;
    }

    // The book is never left in an inconsistent state as a result of this "Up-
    // date", irrespective to whether it has actually been updated or not,  so
    // we can always set the "UpToDateAs" counter now:
    m_upToDateAs = seq_num;

    // Level to insert new price (if insertion is required). Bid prices come in
    // the descending order:
    Entry* entrs = is_bid ? m_bids      : m_asks;
    int*   n     = is_bid ? &m_currBids : &m_currAsks;

    assert(0 <= (*n) && (*n) <= m_depth && m_depth <= MaxDepth);
    int    level = 0;
    bool   done  = false;

    for (; level < (*n); ++level)
    {
      // NB: Do not use equality of floating-point pxs:
      double pxl   = entrs[level].m_price;
      bool   equal = Abs(pxl - price) < PxTol;
      if (equal)
      {
        //-------------------------------------------------------------------//
        // Update exactly that "level":                                      //
        //-------------------------------------------------------------------//
        long& updatable_qty = entrs[level].m_qty;

        if (qty_is_delta)
        {
          long newQty = updatable_qty + qty;
          if (newQty <= 0)
          {
            // XXX: Still proceed in this case -- Delete. Do not even log a
            // warning  if newQty==0:
            if (utxx::unlikely(m_debug && newQty < 0))
              SLOG(WARNING) << "OrderBook::Update: Symbol="
                           << Events::ToCStr(m_symbol)       << ", IsBid="
                           << is_bid << ", Px="  << price    << ", QtyDelta="
                           << qty    << ": NewQty would be " << newQty
                           << ", perform Deletion instead"   << endl;
            DeleteEntry(entrs, level, n);
          }
          else
            // If "newQty" is OK: perform the update:
            updatable_qty = newQty;
        }
        else
        // !qty_is_delta, so it must be >= 0:
        if (utxx::likely(qty != 0))
        {
          assert(qty > 0);   // Was already checked
          updatable_qty = qty;
        }
        else
          // Delete:
          DeleteEntry(entrs, level, n);

        // NB: "DeleteEntry" decrements (*n), so upon exiting the loop here, it
        // is possible that level==(*n) and "done" is "true":
        //
        done = true;
        break;
      }
      else
      if ((is_bid && pxl < price) || (!is_bid && pxl > price))
      {
        //-------------------------------------------------------------------//
        // Insert in the Middle:                                             //
        //-------------------------------------------------------------------//
        // Will insert a new entry BEFORE curr "level", ie at this "level", but
        // the curr "level" and all subsequent levels are to be shifted.
        // NB: because the price level does not exist yet, it does not matter
        // whether the qty is a delta or not -- but it must be > 0  (otherwise
        // it would be an unsuccessful search for Delete -- the exact price to
        // be deleted does not exist):
        assert(level < (*n));
        if (utxx::likely(qty > 0))
        {
          ShiftLevelsUp(entrs, level, n, m_depth);
          Entry&  en = entrs[level];
          en.m_price = price;
          en.m_qty   = qty;
          done       = true;
          break;
        }
      }
    }
    // End of "level" loop
    assert(level <= (*n));

    if (!done)
    {
      // "n" entries have been traversed, and still not done:
      assert(level == (*n));

      //---------------------------------------------------------------------//
      // Append at the End:                                                  //
      //---------------------------------------------------------------------//
      // Install update at level "n" if we still can (n <= m_depth - 1), and if
      // the qty is non-0  (NB: qty=0  would mean  that we were trying a Delete
      // but did not find that price):
      //
      if ((*n) < m_depth && qty > 0)
      {
        Entry& en  = entrs[*n];
        en.m_price = price;
        en.m_qty   = qty;
        (*n)++;            // Will increment m_currBids or m_currAsks
        done       = true;
      }
    }

    if (utxx::unlikely(!done))
    {
      //---------------------------------------------------------------------//
      // The update has been dropped: no-where to install it:                //
      //---------------------------------------------------------------------//
      // However, if Qty is a Delta, and is non-positive, then indeed, there
      // is no way to apply it beyond known levels, so it's normal:
      if (m_debug >= 3 && (!qty_is_delta || qty > 0) && *n != (m_depth-1))
        SLOG(WARNING) << "OrderBook::Update: DROPPED: No level to update and "
                        "could not insert a new level: "
                     << Events::ToCStr(m_symbol) << ", SecID="    << m_secID
                     << ", IsBid="  << is_bid    << ", Px="       << price
                     << (qty_is_delta ? ", QtyDelta=" : ", Qty=") << qty
                     << ": Beyond " << (*n)      << " Levels"     << endl;
      return -1;
    }

    //-----------------------------------------------------------------------//
    // Verify the Bid-Ask Spread:                                            //
    //-----------------------------------------------------------------------//
    // If verification failes, cut the head of the OPPOSITE side (since it was
    // updated earlier than the curr one,  and may have suffered from a missed
    // "Delete" giving rise to this problem):
    //
    if (is_bid)
      CheckBidAsk<true>
        (m_bids, m_currBids, m_asks, &m_currAsks, m_idMap.get(), m_debug,
         m_symbol);
    else
      CheckBidAsk<false>
        (m_asks, m_currAsks, m_bids, &m_currBids, m_idMap.get(), m_debug,
         m_symbol);

    //-----------------------------------------------------------------------//
    // SUCCESS:                                                              //
    //-----------------------------------------------------------------------//
    // If we got here: The book has really been updated, so record the stamps
    // for THIS particular update:
    m_recvTime         = recv_time;
    m_eventTS          = event_ts;
    m_lastUpdate       = seq_num;
    m_lastUpdateWasBid = is_bid;

    return level;
  };

  //=========================================================================//
  // "Update" (Level is specified):                                          //
  //=========================================================================//
  bool OrderBook::Update
  (
    bool                  is_bid,
    int                   level,     // 1 .. depth
    Action                action,
    double                price,
    long                  qty,
    utxx::time_val        recv_time,
    utxx::time_val        event_ts,
    Events::SeqNum        seq_num
  )
  {
    // The following pre-condition is verified by the Caller. XXX: again, in
    // some rare cases, "price" can be negative as well (eg for FX swaps):
    assert(qty >= 0 && level >= 1);

    // Is it actually a "Delete"? If so, the corresp method will do the SeqNum
    // management as well:
    if (qty == 0 || action == Action::Delete)
      // NB: Here "price = 0" is tolerated (means: do not check price):
      return Delete(is_bid, level, price, qty, recv_time, event_ts, seq_num);

    // Not a Delete: Further Checks:
    CHECK_SEQ_NUM(seq_num, "Update(Level)", false)

    // The book is never left in an inconsistent state as a result of this "Up-
    // date", irrespective to whether it has actually been updated or not,  so
    // we can always set the "UpToDateAs" counter now:
    m_upToDateAs = seq_num;

    if (level > m_depth)
    {
      if (m_debug)
        SLOG(WARNING) << "OrderBook::Update("   << ToString(action)
                     << "): Symbol="           << Events::ToCStr(m_symbol)
                     << ": UnSupported Level=" << level << " (Depth="
                     << m_depth << ')' << endl;
      return false;
    }

    // PROCEED:
    // Update the Time Stamp (XXX: without any verification for monotonicity):
    Entry* entrs = is_bid ? m_bids : m_asks;

    // The level must, in any case, be within the depth:
    level--;
    assert(0 <= level && level <= m_depth - 1);

    int*   n  = is_bid ? &m_currBids  : &m_currAsks;
    assert(0 <= (*n) && (*n) <= m_depth && m_depth <= MaxDepth);

    // Produce a warning if the "level" is at a gap with the existing levels,
    // but STILL carry out the update:
    bool gapChange = (action == Action::Change && level >= (*n));
    bool gapNew    = (action == Action::New    && level >  (*n));
    if  ((gapChange || gapNew) && m_debug)
      SLOG(WARNING) << "OrderBook::Update(" << ToString(action)   << "): Symbol="
                   << Events::ToCStr(m_symbol)   << ": Level="   << (level+1)
                   << " is beyond the curr range: 1 .. " << (*n) << endl;

    if (level >= (*n))
    {
      assert(level < m_depth);
      // Need to increase "n". XXX: If there is a gap, intermediate levels are
      // filled with 0s:
      for (int i = (*n); i < level; ++i)
      {
        entrs[i].m_price = 0.0;
        entrs[i].m_qty   = 0;
      }
      *n = level + 1;
      assert((*n) <= m_depth);
    }
    else
    if (action == Action::New)
    {
      // Move up all levels beginning from the curr one, through the last one:
      assert(level < (*n));
      ShiftLevelsUp(entrs, level, n, m_depth);
    }

    // Update the "level". Ie, if we got here, the update succeeds -- the only
    // problem we could have got, is paddings installed above:
    assert(level < (*n));
    entrs[level].m_price = price;
    entrs[level].m_qty   = qty;

    // If we got here: The book has really been updated, so record the stamps
    // for THIS particular update:
    m_recvTime           = recv_time;
    m_eventTS            = event_ts;
    m_lastUpdate         = seq_num;
    m_lastUpdateWasBid   = is_bid;

    return true;
  }

  //=========================================================================//
  // "Delete" (at a given level):                                            //
  //=========================================================================//
  bool OrderBook::Delete
  (
    bool                  is_bid,
    int                   level,
    double                price,
    int                   qty,
    utxx::time_val        recv_time,
    utxx::time_val        event_ts,
    Events::SeqNum        seq_num
  )
  {
    assert(level >= 1); // Verified by the Caller
    CHECK_SEQ_NUM(seq_num, "Delete", false)

    // The book is never left in an inconsistent state as a result of this "De-
    // lete", irrespective to whether it has actually been updated or not,  so
    // we can always set the "UpToDateAs" counter now:
    m_upToDateAs = seq_num;

    int*   n  = is_bid ? &m_currBids : &m_currAsks;
    assert(0 <= (*n) && (*n) <= m_depth && m_depth <= MaxDepth);

    // The level must be within the range 1..n, otherwise we simply cannot
    // carry out the delete operation:
    if (level > (*n))
    {
      if (m_debug)
        SLOG(WARNING) << "OrderBook::Delete: Symbol="
                     << Events::ToCStr(m_symbol) << ": Level=" << level
                     << " is beyond the curr range: 1 .. "     << (*n) << endl;
      return false;
    }

    // Generic Case:
    level--;       // Now in 0 .. (n-1):
    assert(0 <= level && level  < (*n));

    Entry* entrs = is_bid ? m_bids : m_asks;

    // If "price" is given, verify it. XXX: If it fails, we STILL proceed with
    // delete at the level specified:
    if (m_debug)
    {
      if (price != 0.0 && entrs[level].m_price != price)
        SLOG(WARNING) << "OrderBook::Delete: Symbol="
                     << Events::ToCStr(m_symbol) << ", Level="  << (level+1)
                     << ": Inconsistent Px="     << price << ", expected="
                     << entrs[level].m_price     << endl;

      // If "qty" (original) is given, verify it (same comment as above):
      if (qty != 0 && entrs[level].m_qty != qty)
        SLOG(WARNING) << "OrderBook::Delete: Symbol="
                     << Events::ToCStr(m_symbol) << ", Level=" << (level+1)
                     << ": Inconsistent qty="    << qty   << ", expected="
                     << entrs[level].m_qty       << endl;
    }
    // OK: Can carry out Deletion:
    DeleteEntry(entrs, level, n);

    // If we got here: The book has really been updated, so record the stamps
    // for THIS particular update:
    m_recvTime         = recv_time;
    m_eventTS          = event_ts;
    m_lastUpdate       = seq_num;
    m_lastUpdateWasBid = is_bid;

    return true;
  }

  //=========================================================================//
  // "Verify" (One Side):                                                    //
  //=========================================================================//
  // If verification failes, "false" is returned and error / warning msgs are
  // logged -- but no exceptions are thrown:
  //
  bool OrderBook::VerifySide(Events::SeqNum seq_num, bool is_bid) const
  {
    Entry const* entrs  = is_bid ? m_bids     : m_asks;
    int    n            = is_bid ? m_currBids : m_currAsks;
    char const* side    = is_bid ? "Bids"     : "Asks";

    if (!IsUpToDate())
    {
      SLOG(WARNING) << "OrderBook::Verify: Symbol=" << Events::ToCStr(m_symbol)
                   << ": FAILED: The Book is Not Up-To-Date: SeqNum="
                   << seq_num         << ", LastUpdate=" << m_lastUpdate
                   << ", UpToDateAs=" << m_upToDateAs    << endl;
      return false;
    }

    if (n < 0 || n > m_depth || m_depth > MaxDepth)
    {
      SLOG(ERROR) << "OrderBook::Verify: Symbol="   << Events::ToCStr(m_symbol)
                 << ": FAILED: SeqNum=" << seq_num << ", Curr" << side << '='
                 << n  <<    ", Depth=" << m_depth << endl;
      return false;
    }

    for (int i = 0; i < n; ++i)
    {
      double price = entrs[i].m_price;
      int    qty   = entrs[i].m_qty;

      // XXX: Negative Prices are allowed (may occur for eg FX swaps), but not
      // negative Qtys:
      if (qty <= 0.0)
      {
        // This may happen, eg, if gaps exist in the Book:
        SLOG(WARNING) << "OrderBook::Verify: Symbol="
                     << Events::ToCStr(m_symbol)  << ": FAILED: SeqNum="
                     << seq_num << ", "   << side << "Level=" << (i+1)
                     << ": Px=" << price  << ", Qty=" << qty  << endl;
        return false;
      }

      if (i == 0)
        continue;

      // Check the strong monotonicity of prices:
      if (( is_bid && price >= entrs[i-1].m_price) ||
          (!is_bid && price <= entrs[i-1].m_price))
      {
        SLOG(WARNING) << "OrderBook::Verify: Symbol="
                     << Events::ToCStr(m_symbol)
                     << ": FAILED: SeqNum=" << seq_num << ", "  << side
                     << "Level="   << (i+1) << ": Px=" << price << ", PrevPx="
                     << entrs[i-1].m_price  << endl;
        return false;
      }
    }
    return true;
  }

  //=========================================================================//
  // Verify (Both Sides):                                                    //
  //=========================================================================//
  bool OrderBook::Verify(Events::SeqNum seq_num, bool detailed)
  {
    // Check that Min(Ask) > Max(Bid). If not,
    if (m_currBids >= 1 && m_currAsks >= 1   &&
        m_asks[0].m_price <= m_bids[0].m_price)
    {
      SLOG(ERROR) << "OrderBook::Verify: Symbol="   << Events::ToCStr(m_symbol)
                 << ": FAILED: SeqNum=" << seq_num << ", Max(Bid)="
                 << m_bids[0].m_price   << ", Min(Ask)="
                 << m_asks[0].m_price   << endl;
      return false;
    }

    // In the "detailed" mode, do separate Bids and Asks verification:
    return
      detailed
      ? (VerifySide(seq_num, true) && VerifySide(seq_num, false))
      : true;
  }

  //=========================================================================//
  // "Print":                                                                //
  //=========================================================================//
  void OrderBook::Print() const
  {
    int n = std::max(m_currAsks, m_currBids);
    for (int i = 0; i < n; i++)
    {
      if (i < m_currBids)
        fprintf(stderr, "  B%02d %10.5f %12ld |",
                (i+1), m_bids[i].m_price, m_bids[i].m_qty);
      else
        fprintf(stderr, "  %*s |", 27, " ");

      if (i < m_currAsks)
        fprintf(stderr, " A%02d %10.5f %12ld\n",
                (i+1), m_asks[i].m_price, m_asks[i].m_qty);
      else
        fprintf(stderr, " %*s\n", 28, " ");
    }
    fflush(stderr);
  }

  //=========================================================================//
  // "UpdateStats":                                                          //
  //=========================================================================//
  void OrderBook::UpdateStats
  (
    utxx::time_val        a_now,
    Entry const&          a_best_bid,
    Entry const&          a_best_ask
  )
  {
    // XXX: We only perform an update if both Bids and Asks are non-empty, AND
    // at least one of Best Bid Px, Best Ask Px have changed. Otherwise, simply
    // return:
    assert(a_best_bid.m_price < a_best_ask.m_price);

    m_pressureCounter.Add
      (a_now,
       int(a_best_bid.m_price == m_prevBestBid.m_price),
       int(a_best_ask.m_price == m_prevBestAsk.m_price));

    // Save the curr vals:
    m_prevBestBid = a_best_bid;
    m_prevBestAsk = a_best_ask;

    // Update the prices:
    m_bidStats.add(a_best_bid.m_price);
    m_askStats.add(a_best_ask.m_price);
  }
}
