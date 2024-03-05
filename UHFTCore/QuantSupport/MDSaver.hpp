// vim:ts=2:et
//===========================================================================//
//                               "MDSaver.hpp":                              //
//      Formats for Saving Binary MktData (OrderBooks and Aggressions)       //
//===========================================================================//
// XXX: The data types used here are MONOMORPHIC, because they are for offline
//      use only:
// (*)  Pxs and Qtys are stored as "double"s
// (*)  Fractional  Qtys are supported;
// (*)  No semantic templatisation of Qtys with QtyTypeT:
//
#pragma once

#include "Basis/TimeValUtils.hpp"
#include "Basis/Maths.hpp"
#include "Basis/IOUtils.hpp"
#include "Connectors/OrderBook.hpp"
#include <cstring>

namespace MAQUETTE
{
namespace QuantSupport
{
  //=========================================================================//
  // "MDAggrRecord" and its components:                                      //
  //=========================================================================//
  constexpr static int DefaultLDepth = 20;

  //-------------------------------------------------------------------------//
  // "MDAggrOrderBookEntry":                                                 //
  //-------------------------------------------------------------------------//
  // Aggregated Data for an OrderBook Px Level:
  //
  struct MDAggrOrderBookEntry
  {
    // Data Flds:
    double  m_px;                 // Px Level
    double  m_qty;                // Aggregated Qty
    int     m_nOrders;            // #Orders making up the Qty above
    double  m_minOrderQty;        // Min and Max Qty of individual Orders
    double  m_maxOrderQty;        //   at this Px Level

    // Default Ctor:
    MDAggrOrderBookEntry()
    : m_px          (NaN<double>),
      m_qty         (0),
      m_nOrders     (0),
      m_minOrderQty (0),
      m_maxOrderQty (0)
    {}
  };

  //-------------------------------------------------------------------------//
  // "MDAggrOrderBook":                                                      //
  //-------------------------------------------------------------------------//
  // Here "LDepth" is the number of Px Levels stored, as usual:
  //
  template<int LDepth = DefaultLDepth>
  struct MDAggrOrderBook
  {
    static_assert(LDepth > 0, "MDAggrOrderBook");
    MDAggrOrderBookEntry m_bids[LDepth];
    MDAggrOrderBookEntry m_asks[LDepth];
  };

  //-------------------------------------------------------------------------//
  // "MDAggression":                                                         //
  //-------------------------------------------------------------------------//
  // NB: This is a single Aggressive Order which results in 1 or more Matches
  // (Trades). Here we provide the TotalFilledQty and the AvgFillPx. XXX: The
  // name of this struct has howthing to do with "Aggr"  in  Aggregated Order
  // Books!
  struct MDAggression
  {
    // Data Flds:
    double  m_totQty;
    double  m_avgPx;
    bool    m_bidAggr;    // Bid Side (Buyer) was the Aggressor?

    // Default Ctor:
    MDAggression()
    : m_totQty  (0),
      m_avgPx   (0.0),    // XXX: Better 0 thatn NaN!
      m_bidAggr (false)
    {}

    // Empty or Valid?
    bool IsValid() const  // XXX: Px not used:
      { return m_totQty > 0; }
  };

  //-------------------------------------------------------------------------//
  // "MDAggrRecord": Over-All:                                               //
  //-------------------------------------------------------------------------//
  // It is an OrderBook SnapShot, possibly preceded by an Aggression which re-
  // sulted in this OrderBook state. XXX:  They share a common TimeStamp:
  //
  template<int LDepth = DefaultLDepth>
  struct MDAggrRecord
  {
    static_assert(LDepth > 0, "MDAggrRecord");
    // Data Flds:
    utxx::time_val          m_ts;         // TimeStamp (Exchange-Side or Local)
    char                    m_symbol[16]; // At most 15 chars + '\0'
    MDAggression            m_aggr;       // Prev Aggression, may be empty
    MDAggrOrderBook<LDepth> m_ob;         // OrderBook SnapShot

    // Default Ctor:
    MDAggrRecord()
    : m_ts      (),
      m_symbol  (),
      m_aggr    (),
      m_ob      ()
    {
      // Zero-out the Symbol:
      memset(m_symbol, '\0', sizeof(m_symbol));
    }
  };

  //=========================================================================//
  // "SaveAggrOrderBookEntry":                                               //
  //=========================================================================//
  template<typename QR, int LDepth = DefaultLDepth>
  inline bool SaveAggrOrderBookEntry
  (
    int                       a_level,
    PriceT                    a_px,
    OrderBook::OBEntry const& a_entry,
    MDAggrOrderBookEntry    (*a_obs)[LDepth]
  )
  {
    assert(0 <= a_level && a_level < LDepth && a_obs != nullptr);
    MDAggrOrderBookEntry& targ = (*a_obs)[a_level];

    // Save Px and AggrQty of this Entry. XXX: Here a dynamic conversion of
    // QtyU to Double is OK, QT is immaterial:
    constexpr QtyTypeT QT = QtyTypeT::UNDEFINED;
    targ.m_px             = double(a_px);
    targ.m_qty            = double(QR(a_entry.GetAggrQty<QT,QR>()));
    targ.m_nOrders        = a_entry.m_nOrders;

    if (utxx::unlikely(!IsFinite(targ.m_px) || targ.m_qty <= 0))
      throw utxx::logic_error
            ("SaveAggrOrderBook: Level=", a_level, ": Invalid Px=", targ.m_px,
             " or Qty=", targ.m_qty);

    // Do we have Orders?
    bool withOrders = (a_entry.m_nOrders > 0);
    if  (utxx::unlikely(withOrders != (a_entry.m_frstOrder != nullptr)))
      throw utxx::logic_error
          ("SaveAggrOrderBook: Level=",   a_level, ": Inconsistency: NOrders=",
           a_entry.m_nOrders, ", FrstOrder=",
           (a_entry.m_frstOrder != nullptr) ? "Non-NULL" : "NULL");

    if (withOrders)
    {
      // Yes, we have Orders: Get Max and Min Order Qty, verify the Aggr Qty:
      targ.m_minOrderQty = INT_MAX;
      targ.m_maxOrderQty = 0.0;
      double aggrQty     = 0.0;

      for (OrderBook::OrderInfo const* oi = a_entry.m_frstOrder;
           oi != nullptr;   oi = oi->m_next)
      {
        double oQty = double(QR(oi->GetQty<QT,QR>()));
        aggrQty += oQty;

        if (oQty < targ.m_minOrderQty)
          targ.m_minOrderQty = oQty;
        if (oQty > targ.m_maxOrderQty)
          targ.m_maxOrderQty = oQty;
      }
      // Verify the AggrQty (it also checks that conversion to "int" is OK):
      if (utxx::unlikely
         (aggrQty != targ.m_qty || targ.m_minOrderQty <= 0 ||
          targ.m_minOrderQty    >  targ.m_maxOrderQty ||
          targ.m_maxOrderQty    >  targ.m_qty))
        throw utxx::logic_error
              ("SaveAggrOrderBook: Level=", a_level, ": AggrQty=",
               targ.m_qty,     ", SumQty=", aggrQty, ", MinOrderQty=",
               targ.m_minOrderQty,                   ", MaxOrderQty=",
               targ.m_maxOrderQty);
    }
    else
    {
      // No Orders: Set both Min and Max Qtys to 0;
      targ.m_minOrderQty = 0;
      targ.m_maxOrderQty = 0;
    }
    // Continue "Traverse":
    return true;
  }

  //=========================================================================//
  // "SaveAggrOrderBook":                                                    //
  //=========================================================================//
  template<int LDepth = DefaultLDepth>
  inline void SaveAggrOrderBook
  (
    OrderBook const&      a_ob,
    utxx::time_val        a_ts,
    MDAggrRecord<LDepth>* a_mdr
  )
  {
    static_assert(LDepth > 0, "SaveAggrOrderBook");
    assert(a_mdr != nullptr && !(a_ts.empty()));

    a_mdr->m_ts = a_ts;

    // Copy the Symbol from SecDef:
    constexpr int L  = int(sizeof(a_mdr->m_symbol));
    strncpy(a_mdr->m_symbol, a_ob.GetInstr().m_Symbol.data(), L);
    a_mdr->m_symbol[L-1] = '\0';

    // Now fill in the OrderBook Bid and Ask Sides. FracQtys are NOT supported:
    MDAggrOrderBook<LDepth>& mdb = a_mdr->m_ob;

    if (a_ob.WithFracQtys())
    {
      // Qtys are "double"s already:
      // Bids (IsBid=true):
      a_ob.Traverse<true>
      (
        LDepth,
        [&mdb]
        (int a_level, PriceT a_px, OrderBook::OBEntry const& a_entry) -> bool
        { return
            SaveAggrOrderBookEntry<double, LDepth>
              (a_level, a_px, a_entry, &mdb.m_bids);
        }
      );
      // Asks (IsBid = false):
      a_ob.Traverse<false>
      (
        LDepth,
        [&mdb]
        (int a_level, PriceT a_px, OrderBook::OBEntry const& a_entry) -> bool
        { return
            SaveAggrOrderBookEntry<double, LDepth>
              (a_level, a_px, a_entry, &mdb.m_asks);
        }
      );
    }
    else
    {
      // Qtys are "long"s; QT is ignored here (so UNDEFINED):
      // Bids (IsBid=true):
      a_ob.Traverse<true>
      (
        LDepth,
        [&mdb]
        (int a_level, PriceT a_px, OrderBook::OBEntry const& a_entry) -> bool
        { return
            SaveAggrOrderBookEntry<long, LDepth>
              (a_level, a_px, a_entry, &mdb.m_bids);
        }
      );
      // Asks (IsBid = false):
      a_ob.Traverse<false>
      (
        LDepth,
        [&mdb]
        (int a_level, PriceT a_px, OrderBook::OBEntry const& a_entry) -> bool
        { return
            SaveAggrOrderBookEntry<long, LDepth>
              (a_level, a_px, a_entry, &mdb.m_asks);
        }
      );
    }
    // All Done!
  }

  //=========================================================================//
  // "SaveAggression":                                                       //
  //=========================================================================//
  template<int LDepth = DefaultLDepth>
  inline void SaveAggression
  (
    char         const*    a_symbol,
    MDAggression const&    a_aggr,
    utxx::time_val         a_ts,
    MDAggrRecord<LDepth>*  a_mdr
  )
  {
    static_assert(LDepth > 0, "SaveAggression");
    assert(a_symbol != nullptr && a_mdr != nullptr && !(a_ts.empty()));
    assert(a_mdr != nullptr);

    // Similar to "SaveOrderBook" above:
    a_mdr->m_ts = a_ts;

    // Store the Symbol:
    constexpr int L  = int(sizeof(a_mdr->m_symbol));
    strncpy(a_mdr->m_symbol, a_symbol, L);
    a_mdr->m_symbol[L-1] = '\0';

    // Store the actual Trade:
    a_mdr->m_aggr = a_aggr;
  }

  //=========================================================================//
  // "MDOrdersBook" and its components:                                      //
  //=========================================================================//
  constexpr static int  DefaultODepth = 50;

  //-------------------------------------------------------------------------//
  // "MDOrdersBookEntry":                                                    //
  //-------------------------------------------------------------------------//
  // NB: The plural "s" in "Orders"!
  // Unlike "MDAggrOrderBookEntry", this struct contains info on just one Order
  // in the OrderBook, not an Aggregated Px Level:
  //
  struct MDOrdersBookEntry
  {
    // Data Flds:
    OrderID   m_orderID;
    double    m_px;
    double    m_qty;

    // Default Ctor:
    MDOrdersBookEntry()
    : m_orderID(0),
      m_px     (),
      m_qty    ()
    {}
  };

  //-------------------------------------------------------------------------//
  // "MDOrdersBook":                                                         //
  //-------------------------------------------------------------------------//
  // NB: The plural "s" in "Orders"!
  // Unlike "MDAggrOrderBook", this Book contains lists  of individual Orders
  // at both Bid and Ask Sides, in the Descending / Ascending (resp) order of
  // Pxs. Suitable for more precise MktImpact analysis:
  //
  template<int ODepth = DefaultODepth>
  struct MDOrdersBook
  {
    static_assert(0 < ODepth, "MDOrdersBook");
    // Data Flds:
    utxx::time_val    m_ts;           // TimeStamp
    char              m_symbol[16];   // At most 15 chars + '\0'
    MDOrdersBookEntry m_bids[ODepth]; // In non-increasing order of Pxs
    MDOrdersBookEntry m_asks[ODepth]; // In non-decreasing order of Pxs

    // Default Ctor:
    MDOrdersBook()
    : m_ts    (),
      m_symbol(),
      m_bids  (),
      m_asks  ()
    {
      // Zero-out the Symbol:
      for (int i = 0; i < int(sizeof(m_symbol)); ++i)
        m_symbol[i] = '\0';
    }
  };

  //=========================================================================//
  // "SaveLevelOrders":                                                      //
  //=========================================================================//
  // Returns "true" iff there is still space for more Entries:
  //
  template<typename QR, bool IsBid, int ODepth = DefaultODepth>
  inline bool SaveLevelOrders
  (
    PriceT                    a_px,
    OrderBook::OBEntry const& a_entry,
    int*                      a_n_ords,  // Curr #Orders installed
    MDOrdersBookEntry       (*a_obes)[ODepth]
  )
  {
    // Once again,  QT is immaterial:
    constexpr QtyTypeT QT = QtyTypeT::UNDEFINED;

    assert(IsFinite(a_px)     && a_n_ords != nullptr && 0 <= *a_n_ords &&
           *a_n_ords < ODepth && a_obes   != nullptr);

    // Traverse all Orders in the given Entry:
    for (OrderBook::OrderInfo const* oi = a_entry.m_frstOrder;
         oi != nullptr && *a_n_ords < ODepth;   oi = oi->m_next, ++(*a_n_ords))
    {
      assert(IsBid == oi->m_isBid && a_px == oi->m_px);
      MDOrdersBookEntry& targ = (*a_obes)[*a_n_ords];

      targ.m_orderID = oi->m_id;
      targ.m_px      = double(a_px);
      targ.m_qty     = double(QR(oi->GetQty<QT,QR>()));
      assert(targ.m_qty > 0.0);
    }
    // Continue, or no more room for storing Orders?
    return (*a_n_ords < ODepth - 1);
  }

  //=========================================================================//
  // "SaveOrdersBook":                                                       //
  //=========================================================================//
  // Uses the "MDOrdersBook" output fmt rather than "MDAggrOrderBook":
  //
  template<int ODepth = DefaultODepth>
  inline void SaveOrdersBook
  (
    OrderBook const&      a_ob,
    utxx::time_val        a_ts,
    MDOrdersBook<ODepth>* a_mdo
  )
  {
    static_assert(ODepth > 0, "SaveOrdersBook");
    assert(a_mdo != nullptr && !(a_ts.empty()));

    a_mdo->m_ts = a_ts;

    // Copy the Symbol from SecDef:
    constexpr int L  = int(sizeof(a_mdo->m_symbol));
    strncpy(a_mdo->m_symbol, a_ob.GetInstr().m_Symbol.data(), L);
    a_mdo->m_symbol[L-1] = '\0';

    // Now fill in the OrderBook Bid and Ask Sides. FraqQtys are NOT supported.
    // The PxLevelDepth for OrderBook traversal is obviously not greater  than
    // the number of Orders to be traversed, as only non-empty levels are coun-
    // ted:
    int nOrds = 0;

    if (a_ob.WithFracQtys())
    {
      // Bids: QT=UNDEFINED, QR=double, IsBid=true:
      a_ob.Traverse<true>
      (
        ODepth,
        [a_mdo, &nOrds]
        (int, PriceT a_px, OrderBook::OBEntry const& a_entry) -> bool
        { return SaveLevelOrders<double, true,  ODepth>
                 (a_px, a_entry, &nOrds, &(a_mdo->m_bids)); }
      );
      // Asks: QT=UNDEFINED, QR=double, IsBid=false:
      nOrds = 0;
      a_ob.Traverse<false>
      (
        ODepth,
        [a_mdo, &nOrds]
        (int, PriceT a_px, OrderBook::OBEntry const& a_entry) -> bool
        { return SaveLevelOrders<double, false, ODepth>
                 (a_px, a_entry, &nOrds, &(a_mdo->m_asks)); }
      );
    }
    else
    {
      // Bids: QT=UNDEFINED, QR=long, IsBid=true:
      a_ob.Traverse<true>
      (
        ODepth,
        [a_mdo, &nOrds]
        (int, PriceT a_px, OrderBook::OBEntry const& a_entry) -> bool
        { return SaveLevelOrders<long, true,  ODepth>
                 (a_px, a_entry, &nOrds, &(a_mdo->m_bids)); }
      );
      // Asks: QT=UNDEFINED, QR=long, IsBid=false:
      nOrds = 0;
      a_ob.Traverse<false>
      (
        ODepth,
        [a_mdo, &nOrds]
        (int, PriceT a_px, OrderBook::OBEntry const& a_entry) -> bool
        { return SaveLevelOrders<long, false, ODepth>
                 (a_px, a_entry, &nOrds, &(a_mdo->m_asks)); }
      );
    }
    // All Done!
  }

  //=========================================================================//
  // Functions for MktData Analysis:                                         //
  //=========================================================================//
  //=========================================================================//
  // "VWAP" on "MDAggrOrderBookEntry" Array:                                 //
  //=========================================================================//
  // Here "EntryT" is "MDAggrOrderBookEntry" or "MDOrdersBookEntry":
  //
  template<bool IsStrict, typename EntryT, int Depth>
  inline double VWAP
  (
    EntryT const (&a_obs)[Depth],
    double         a_qty
  )
  {
    assert(a_qty  > 0.0);
    double cumPx  = 0.0;
    double cumQty = 0;

    for (int i = 0; cumQty < a_qty && i < Depth; ++i)
    {
      EntryT const& en = a_obs[i];

      // How much we get at this level:
      double currQty = Min(a_qty - cumQty, en.m_qty);
      double currPx  = en.m_px;
      assert(currQty > 0 && currPx > 0.0);

      cumPx  += double(currQty) * double(currPx);
      cumQty += currQty;
    }
    // In the "Strict" mode, if we could not fill the whole "a_qty", throw an
    // exception (and also if the OrderBook side was completely empty):
    assert(0 <= cumQty && cumQty <= a_qty);
    if (utxx::unlikely(cumQty == 0 || (IsStrict && (cumQty < a_qty))))
      throw utxx::runtime_error
            ("VWAP: Insufficient Liquidity: Got ", cumQty, ", needed ", a_qty);
    // If OK:
    return cumPx / double(cumQty);
  }

  //=========================================================================//
  // "TraverseAggrMD":                                                       //
  //=========================================================================//
  // Traverses "MDAggrRecord"s stored in an File, and invokes the Analyser on
  // them:
  // Analyser ::
  //   (MDAggrRecord<LDepth> const* a_mdrs, int a_i, int a_len) -> bool:
  //
  template<int LDepth = DefaultLDepth, typename Analyser>
  inline void  TraverseAggrMD
  (
    std::string const& a_file_name,
    Analyser    const& a_analyser
  )
  {
    using MDR = MDAggrRecord<LDepth>;

    //-----------------------------------------------------------------------//
    // MMap the MktData File (IsRW=false):                                   //
    //-----------------------------------------------------------------------//
    IO::MMapedFile<MDR, false> mdrMap(a_file_name.c_str());
    MDR const* mdrs =          mdrMap.GetPtr  ();
    long       n    =          mdrMap.GetNRecs();
    assert(mdrs != nullptr && n >= 0);

    //-----------------------------------------------------------------------//
    // Traverse the Map:                                                     //
    //-----------------------------------------------------------------------//
    utxx::time_val prev_ts;

    for (long i = 0; i < n; ++i)
    {
      // Check the TimeStamp monotonicity:
      MDR const&     mdi     = mdrs[i];
      utxx::time_val ts      = mdi.m_ts;

      if (utxx::unlikely(!prev_ts.empty() && ts < prev_ts))
        throw utxx::badarg_error
              ("TraverseAggrMD: MDFile=", a_file_name, ", Rec=", i,
               ": Non-Monotonic TS");
      prev_ts = ts;

      // If OK: Invoke the Analyser:
      bool cont = a_analyser(mdrs, n, i);

      if (utxx::unlikely(!cont))
        break;
    }
    // All Done!
    // NB: The file is unmaped and closed automatically
  }
} // End namespace QuantSupport
} // End namespace MAQUETTE
