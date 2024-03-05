// vim:ts=2:et
//===========================================================================//
//                          "Connectors/OrderBook.cpp":                      //
//                      Init/Fini and Non-Templated Methods                  //
//===========================================================================//
#include "Connectors/OrderBook.hpp"
using namespace std;

namespace MAQUETTE
{
  //=========================================================================//
  // "OrderBookBase::SetBestBidPx":                                          //
  //=========================================================================//
  template<bool IsRelaxed>
  bool OrderBookBase::SetBestBidPx(PriceT a_px) const
  {
    // Set it if the arg is valid:
    if (utxx::unlikely(!IsFinite(a_px)))
      return false;
    m_BBPx = a_px;

    // Check for a possible collision with Ask:
    if (utxx::unlikely
       ((!IsRelaxed && m_BBPx >= m_BAPx) || (IsRelaxed && m_BBPx > m_BAPx)))
    {
      // Invalidate the BestAskPx as being the "outdated" one:
      m_BAPx = PriceT();  // NaN
      return false;
    }
    else
      return true;        // Yes, all was fine
  }

  //-------------------------------------------------------------------------//
  // "OrderBookBase::SetBestBidPx" Instances:                                //
  //-------------------------------------------------------------------------//
  template bool OrderBookBase::SetBestBidPx<false>(PriceT a_px) const;
  template bool OrderBookBase::SetBestBidPx<true> (PriceT a_px) const;

  //=========================================================================//
  // "OrderBookBase::SetBestAskPx":                                          //
  //=========================================================================//
  template<bool IsRelaxed>
  bool OrderBookBase::SetBestAskPx(PriceT a_px) const
  {
    // Set it if the arg is valid:
    if (utxx::unlikely(!IsFinite(a_px)))
      return false;
    m_BAPx = a_px;

    // Check for a possible collision with Bid:
    if (utxx::unlikely
       ((!IsRelaxed && m_BBPx >= m_BAPx) || (IsRelaxed && m_BBPx > m_BAPx)))
    {
      // Invalidate the BestBidPx as being the "outdated" one:
      m_BBPx = PriceT();  // NaN
      return false;
    }
    else
      return true;        // Yes, all was fine
  }

  //-------------------------------------------------------------------------//
  // "OrderBookBase::SetBestAskPx" Instances:                                //
  //-------------------------------------------------------------------------//
  template bool OrderBookBase::SetBestAskPx<false>(PriceT a_px) const;
  template bool OrderBookBase::SetBestAskPx<true> (PriceT a_px) const;

  //=========================================================================//
  // "OrderBook": Default Ctor:                                              //
  //=========================================================================//
  OrderBook::OrderBook ()
  : // Base:
    OrderBookBase      (nullptr, nullptr),
    // Props:
    m_isFullAmt        (false),
    m_isSparse         (false),
    m_qt               (QtyTypeT::UNDEFINED),
    m_withFracQtys     (false),
    m_withSeqNums      (false),
    m_withRptSeqs      (false),
    m_contRptSeqs      (false),
    // Dense Bids and Asks (as Arrays):
    m_NL               (0),
    m_ND               (0),
    m_LBI              (-1),
    m_BBI              (-1),
    m_BD               (0), // Or (-1)?
    m_bids             (nullptr),
    m_BAI              (-1),
    m_HAI              (-1),
    m_AD               (0), // Or (-1)?
    m_asks             (nullptr),
    // Sparse Bids and Asks (as Maps):
    m_bidsMap          (),
    m_asksMap          (),
    // Orders
    m_nOrders           (0),
    m_orders            (nullptr),
    // Temporal:
    m_initModeOver     (false),
    m_isInitialised    (false),
    m_lastUpdateRptSeq (-1),
    m_lastUpdateSeqNum (-1),
    m_lastUpdatedBid   (false),
    // Subscrs and Logging:
    m_subscrs          (),
    m_logger           (nullptr),
    m_debugLevel       (0)
  {}

  //=========================================================================//
  // "OrderBook": Non-Default Ctor:                                          //
  //=========================================================================//
  OrderBook::OrderBook
  (
    EConnector_MktData const*  a_mdc,
    SecDefD  const*            a_instr,
    bool                       a_is_full_amt,
    bool                       a_is_sparse,
    QtyTypeT                   a_qt,
    bool                       a_with_frac_qtys,
    bool                       a_with_seq_nums,
    bool                       a_with_rpt_seqs,
    bool                       a_cont_rpt_seqs,
    int                        a_total_levels,
    int                        a_max_depth,
    long                       a_max_orders
  )
  : // Base:
    OrderBookBase      (a_mdc, a_instr),
    // Props:
    m_isFullAmt        (a_is_full_amt),
    m_isSparse         (a_is_sparse),
    m_qt               (a_qt),
    m_withFracQtys     (a_with_frac_qtys),
    m_withSeqNums      (a_with_seq_nums),
    m_withRptSeqs      (a_with_rpt_seqs),
    m_contRptSeqs      (a_cont_rpt_seqs),
    // Dense Bids and Asks (as Arrays):
    m_NL               (m_isSparse ? 0 : a_total_levels),
    m_ND               (m_isSparse ? 0 : a_max_depth),
    m_LBI              (-1),
    m_BBI              (-1),              // Not yet known: no Mkt Data
    m_BD               (0),               // Curr Bids Depth is 0
    m_bids             ((m_NL > 0) ? new OBEntry[size_t(m_NL)]  : nullptr),
    m_BAI              (-1),              // ditto
    m_HAI              (-1),
    m_AD               (0),               // Curr Asks Depth is 0
    m_asks             ((m_NL > 0) ? new OBEntry[size_t(m_NL)]  : nullptr),
    // Sparse Bids and Asks (as Maps):
    m_bidsMap          (),
    m_asksMap          (),
    // Orders
    m_nOrders           (0),              // for now
    m_orders            (nullptr),        // for now
    // Temporal:
    m_initModeOver     (false),
    m_isInitialised    (false),
    // NB: The following "SeqNum"s are signed, and are initialised to (-1) in
    // case if (very unlikely) 0 appears to be a valid SeqNum value:
    m_lastUpdateRptSeq (-1),
    m_lastUpdateSeqNum (-1),
    m_lastUpdatedBid   (false),           // Not yet known, hence "false"
    // Subscrs and Logging:
    m_subscrs          (),
    m_logger           ((m_mdc != nullptr) ? m_mdc->GetLogger()     : nullptr),
    m_debugLevel       ((m_mdc != nullptr) ? m_mdc->GetDebugLevel() : 0)
  {
    CHECK_ONLY
    (
      if (utxx::unlikely
         (m_instr == nullptr       || m_instr->m_SecID == 0 ||
          m_instr->m_PxStep <= 0.0 || (!m_isSparse && (m_NL <= 0 || m_ND < 0))))
        // NB: m_ND==0 is perfectly fine (= +oo):
        throw utxx::badarg_error("OrderBook::Ctor: Invalid param(s)");
    )
    // Zero-out all Qtys:
    memset(m_bids, '\0', size_t(m_NL) * sizeof(OBEntry));
    memset(m_asks, '\0', size_t(m_NL) * sizeof(OBEntry));

    // Now possibly construct the Orders array:
    if (a_max_orders > 0)
    {
      m_orders = new OrderInfo[size_t(a_max_orders)];
      m_nOrders = a_max_orders;
    }
    // For all allocated "OBEntry"s, install back-ptrs to this OrderBook:
    for (int i = 0; i < m_NL; ++i)
    {
      m_bids[i].m_ob = this;
      m_asks[i].m_ob = this;
    }
  }

  //=========================================================================//
  // Dtor:                                                                   //
  //=========================================================================//
  OrderBook::~OrderBook() noexcept
  {
    if (utxx::likely(m_bids != nullptr))
      delete[] m_bids;

    if (utxx::likely(m_asks != nullptr))
      delete[] m_asks;

    if (m_orders != nullptr)
      delete[] m_orders;

    // Zero-out the contents, but some flds still get non-0 invalid values:
    memset(this, '\0', sizeof(OrderBook));

    m_LBI  = -1;
    m_BBI  = -1;
    m_BAI  = -1;
    m_HAI  = -1;
  }

  //=========================================================================//
  // "Invalidate":                                                           //
  //=========================================================================//
  void OrderBook::Invalidate()
  {
    // NB: The "m_initModeOver" and "m_isInitialised" flags are reset HERE:
    m_initModeOver  = false;
    m_isInitialised = false;
    (void) Clear<true>(0, 0);

    // If we are OrdersLog-based, it is probably a good idea to invalidate all
    // "OrderInfo"s as well:
    if (m_orders != nullptr)
    {
      assert(m_nOrders > 0);
      for (long i = 0; i < m_nOrders; ++i)
        m_orders[i] = OrderInfo();
    }
    else
      assert(m_nOrders == 0);
  }

  //=========================================================================//
  // "AddSubscr":                                                            //
  //=========================================================================//
  void OrderBook::AddSubscr
  (
    Strategy*     a_strat,
    UpdateEffectT a_min_cb_level
  )
  {
    //-----------------------------------------------------------------------//
    // Check the Args:                                                       //
    //-----------------------------------------------------------------------//
    CHECK_ONLY
    (
      if (utxx::unlikely(a_strat == nullptr))
        throw utxx::badarg_error
              ("OrderBook::AddSubscr: ", m_instr->m_FullName.data(),
               ": Strategy must not be NULL");
    )
    //-----------------------------------------------------------------------//
    // Check if "a_strat" is already subscribed to this OrderBook:           //
    //-----------------------------------------------------------------------//
    bool amended = false;

    for (SubscrInfo& si: m_subscrs)
      if (utxx::unlikely(si.m_strategy == a_strat))
      {
        // Yes, it is already there -- but we may need to AMEND it; all flds
        // except Strategy (the key) are actually modifyable:
        si.m_minCBLevel  = a_min_cb_level;
        amended          = true;
        break;
      }

    //-----------------------------------------------------------------------//
    // Generic case: Nothing to amend, will install a new "SubscrInfo":      //
    //-----------------------------------------------------------------------//
    if (utxx::likely(!amended))
    {
      // Do we still have room available? (XXX: It is not clear from the seman-
      // tics of "static_vector" what would happen otherwise):
      //
      if (utxx::likely(m_subscrs.size() < m_subscrs.capacity()))
        m_subscrs.emplace_back(a_strat, a_min_cb_level);
      else
        throw utxx::runtime_error
              ("OrderBook::AddSubscr: ", m_instr->m_FullName.data(),
               ": Too Many Strategies");

      // SORT all "SubscrInfo"s in the ASCENDING order of MinLevels:  This is
      // for optimal search  of  applicable Call-Backs  after an OrderBook is
      // updated: for an Event level E*, the "left-most" Strategies with Min-
      // Levels L <= E* will need to be notified:
      sort
      (
        m_subscrs.begin(),
        m_subscrs.end(),
        [](SubscrInfo const& a_left,  SubscrInfo const& a_right)->bool
          { return a_left.m_minCBLevel < a_right.m_minCBLevel; }
      );
    }
  }

  //=========================================================================//
  // "RemoveSubscr":                                                         //
  //=========================================================================//
  // Returnes "true" if the corresp subscription was indeed found, and removed:
  //
  bool OrderBook::RemoveSubscr(Strategy const* a_strat)
  {
    // Is "a_strat" really on the list?
    assert(a_strat != nullptr);   // Though NULL would be handled as well!..

    auto it =
      boost::container::find_if
      (
        m_subscrs.begin(), m_subscrs.end(),
        [a_strat](SubscrInfo const& a_curr)->bool
        { return (a_curr.m_strategy == a_strat); }
      );

    if (utxx::likely(it != m_subscrs.end()))
    {
      m_subscrs.erase(it);
      return true;
    }
    return false;
  }

  //=========================================================================//
  // "RemoveAllSubscrs":                                                     //
  //=========================================================================//
  bool OrderBook::RemoveAllSubscrs()
  {
    if (utxx::unlikely(m_subscrs.empty()))
      return false;   // Nothing to do

    // Generic case:
    m_subscrs.clear();
    return true;
  }

  //=========================================================================//
  // "GetOrderInfo" (by OrderID):                                            //
  //=========================================================================//
  // (*) Returns NULL if OrdersLog is not supported at all, or if OrderID is 0,
  //     or if the OrderInfo slot was not found due to overflow;
  // (*) In all normal cases returns a non-NULL ptr to the OrderInfo slot (but
  //     that slot may still be empty);
  // (*) Does NOT throw exceptions:
  //
  OrderBook::OrderInfo const* OrderBook::GetOrderInfo(OrderID a_order_id) const
  {
    // Check if we have OrdersLog at all. Also, OrderID=0 should not be used:
    assert((m_orders != nullptr) == (m_nOrders > 0));
    CHECK_ONLY
    (
      if (utxx::unlikely(m_nOrders <= 0 || a_order_id == 0))
        return nullptr;
    )
    // Generic case: Get the offset modulo the size:
    long   off = long(a_order_id) % m_nOrders;
    assert(0  <= off && off < m_nOrders && m_orders != nullptr);

    // XXX: However, modular collisions can happen -- if so, perform a resolu-
    // tion. In the simplest and most frequent case, (i==off) will do.  Other-
    // wise, search to the end for an empty slot:
    //
    for (long i = off; i < m_nOrders; ++i)
    {
      OrderBook::OrderInfo* res =  m_orders + i;
      if (utxx::likely(res->m_id == a_order_id && res->m_ob == this))
        // Found an existing non-empty slot for this ID and OrderBook:
        return res;
      else
      if (utxx::likely(res->IsEmpty()))
      {
        // Found an empty slot: Install the proper OrderID and the OuterPtr,
        // and use this entry:
        res->m_id = a_order_id;
        res->m_ob = this;
        return res;
      }
    }
    // If a valid slot was still not found, perform a new search from the
    // beginning:
    for (long i = 0; i < off; ++i)
    {
      OrderBook::OrderInfo* res =  m_orders + i;

      // The following invariant must hold: Either the entry is completely
      // empty, or it has a non-0 ID and this OrderBook ptr:
      assert((res->m_id == 0 && res->m_ob == nullptr) ||
             (res->m_id != 0 && res->m_ob == this));

      if (utxx::likely(res->m_id == a_order_id && res->m_ob == this))
        // Found an existing non-empty slot for this ID and OrderBook:
        return res;
      else
      if (utxx::likely(res->IsEmpty()))
      {
        // Found an empty slot: Install the proper OrderID and the OuterPtr,
        // and use this entry:
        res->m_id = a_order_id;
        res->m_ob = this;
        return res;
      }
    }
    // If we reached this point, "m_orders" are full, and the slot cannot be
    // found:
    return nullptr;
  }

  //=========================================================================//
  // "ResetOrders":                                                          //
  //=========================================================================//
  void OrderBook::ResetOrders(OrderInfo* a_first_order)
  {
    // This method is invoked on non-empty Entries,  "WithOrdersLog" set,  so
    // "a_first_order" should be non-NULL:
    assert(a_first_order != nullptr);

    // XXX: "OrderInfo"s are managed by "EConnector_MktData",  not by "Order-
    // Book". Nevertheless, we traverse and reset all "OrderInfo"s at this Px
    // Level:
    for (OrderInfo* curr = a_first_order; curr != nullptr; )
    {
      OrderInfo* next = curr->m_next;
      *curr = OrderInfo();
      curr  = next;
    }
  }

  //=========================================================================//
  // "Clear":                                                                //
  //=========================================================================//
  // XXX: "Clear" can be VERY inefficient for some badly-constructed OrderBooks
  // (eg deep but sparsely-populated ones, because "memset" would then need  to
  // traverse the whole depth anyway, incuding all empty levels); and note that
  // "Clear" is frequently invoked for SnapShots-only MktData streams. TODO: in
  // such cases, an alternative (Sparse) OrderBook representatio would  be adv-
  // antageous:
  //
  template<bool InitMode>
  inline OrderBook::UpdateEffectT OrderBook::Clear
    (SeqNum a_rpt_seq, SeqNum a_seq_num)
  {
    //-----------------------------------------------------------------------//
    // Checks:                                                               //
    //-----------------------------------------------------------------------//
    // Verify the params (unless in the InitMode -- this could actually be
    // "Invalidate"):
    if constexpr (!InitMode)
      CheckSeqNums<InitMode>(a_rpt_seq, a_seq_num, "Clear");

    // Was the Bool already empty?
    bool wasEmpty  =
      m_isSparse
      ? (m_bidsMap.empty() && m_asksMap.empty())
      : (m_BBI == -1       && m_BAI == -1);

    assert(m_isSparse || (wasEmpty == (m_LBI == -1 && m_HAI == -1)));
    assert(wasEmpty   == !(IsFinite(m_BBPx)  ||   IsFinite(m_BAPx)));

    if (utxx::unlikely(wasEmpty))
      return UpdateEffectT::NONE;   // Because nothing really has changed

    //-----------------------------------------------------------------------//
    // GENERIC CASE:                                                         //
    //-----------------------------------------------------------------------//
    m_lastUpdatedBid  = false;      // Absolutely does not matter...

    if (m_isSparse)
    {
      // Sparse Rep: Just clear the Maps:
      m_bidsMap.clear();
      m_asksMap.clear();
    }
    else
    {
      // Dense  Rep: More complex:
      wasEmpty =         (m_BBI == -1 && m_BAI == -1);
      assert(wasEmpty == (m_LBI == -1 && m_HAI == -1));

      // EFFICIENTLY zero-out all Qtys: Only if the corresp side is non-empty,
      // and within the known range. NB: Do NOT zero-out all "m_NL" entries --
      // this could be very expensive if "m_NL" is large:
      //
      if (utxx::likely(m_BBI != -1))
      {
        assert(0 <= m_LBI  && m_LBI <= m_BBI && m_BBI < m_NL);
        memset(m_bids + m_LBI, '\0',
               size_t(m_BBI  - m_LBI + 1) * sizeof(OBEntry));
      }
      else
        assert(!IsFinite(m_BBPx) && m_LBI == -1);

      if (utxx::likely(m_BAI != -1))
      {
        assert(0 <= m_BAI  && m_BAI <= m_HAI && m_HAI < m_NL);
        memset(m_asks + m_BAI, '\0',
               size_t(m_HAI  - m_BAI + 1) * sizeof(OBEntry));
      }
      else
        assert(!IsFinite(m_BAPx) && m_HAI == -1);

      // Because there is no BestBid or bestAsk anymore, we reset the corresp
      // Indices and Pxs:
      m_LBI  = -1;
      m_BBI  = -1;
      m_BD   =  0;

      m_BAI  = -1;
      m_HAI  = -1;
      m_AD   =  0;
    }
    // Always reset the Best Bid and Ask Pxs:
    m_BBPx = PriceT();
    m_BAPx = PriceT();

    return UpdateEffectT::L1Px;   // Becase  L1 Px, in particular, was reset
  }

  //------------------------------------------------------------------------//
  // "Clear" Instances:                                                     //
  //------------------------------------------------------------------------//
  template
  OrderBook::UpdateEffectT OrderBook::Clear<false>
    (SeqNum a_rpt_seq, SeqNum a_seq_num);

  template
  OrderBook::UpdateEffectT OrderBook::Clear<true>
    (SeqNum a_rpt_seq, SeqNum a_seq_num);

  //=========================================================================//
  // Top-of-the-Book Accessors:                                              //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "GetBestBidNOrders":                                                    //
  //-------------------------------------------------------------------------//
  int  OrderBook::GetBestBidNOrders() const
  {
    return
      m_isSparse
      ? (utxx::unlikely(m_bidsMap.empty())
        ? 0 : m_bidsMap.begin()->second.m_nOrders)

      : (utxx::likely(m_BBI >= 0)
        ? m_bids[m_BBI].m_nOrders : 0);
  }

  //-------------------------------------------------------------------------//
  // "GetBestBidEntry":                                                      //
  //-------------------------------------------------------------------------//
  OrderBook::OBEntry OrderBook::GetBestBidEntry() const
  {
    return
      m_isSparse
      ? (utxx::unlikely(m_bidsMap.empty())
        ? OBEntry() : m_bidsMap.begin()->second)

      : (utxx::likely(m_BBI >= 0)
        ? m_bids[m_BBI] : OBEntry());
  }

  //-------------------------------------------------------------------------//
  // "GetBestAskNOrders":                                                    //
  //-------------------------------------------------------------------------//
  int  OrderBook::GetBestAskNOrders() const
  {
    return
      m_isSparse
      ? (utxx::unlikely(m_asksMap.empty())
        ? 0 : m_asksMap.begin()->second.m_nOrders)

      : (utxx::likely(m_BAI >= 0)
        ? m_asks[m_BAI].m_nOrders : 0);
  }

  //-------------------------------------------------------------------------//
  // "GetBestAskEntry":                                                      //
  //-------------------------------------------------------------------------//
  OrderBook::OBEntry OrderBook::GetBestAskEntry() const
  {
    return
      m_isSparse
      ? (utxx::unlikely(m_asksMap.empty())
        ? OBEntry() : m_asksMap.begin()->second)

      : (utxx::likely(m_BAI >= 0)
        ? m_asks[m_BAI] : OBEntry());
  }

  //-------------------------------------------------------------------------//
  // "IsReady":                                                              //
  //-------------------------------------------------------------------------//
  bool OrderBook::IsReady() const
  {
    return
      m_isInitialised          &&
        (m_isSparse
         ? !(m_bidsMap.empty() || m_asksMap.empty())
         : (m_BBI >= 0         && m_BAI >= 0));
  }
} // End namespace MAQUETTE
