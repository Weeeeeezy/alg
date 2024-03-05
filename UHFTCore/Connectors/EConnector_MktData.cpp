// vim:ts=2:et
//===========================================================================//
//                    "Connectors/EConnector_MktData.cpp":                   //
//   Non-Templated Methods (and Method Instances) of "EConnector_MktData"    //
//===========================================================================//
#include "Connectors/EConnector_MktData.hpp"

using namespace std;

namespace MAQUETTE
{
  //=========================================================================//
  // Non-Default Ctor:                                                       //
  //=========================================================================//
  EConnector_MktData::EConnector_MktData
  (
    bool                                a_is_enabled,
    EConnector_MktData*                 a_primary_mdc,
    boost::property_tree::ptree const&  a_params,
    bool                                a_is_full_amt,
    bool                                a_is_sparse,
    bool                                a_with_seq_nums,
    bool                                a_with_rpt_seqs,
    bool                                a_cont_rpt_seqs,
    int                                 a_mkt_depth,    // 0 -> +oo
    bool                                a_with_expl_trades,
    bool                                a_with_ol_trades,
    bool                                a_use_dyn_init  // DynInitMode applies?
  )
  : // NB:
    // (*) Virtual base ("EConnector") is NOT initialized here!
    // (*) OrderBooks are allocated only for a Primary MDC, whereas ProtoSubscr-
    //     IDs may be required for any MDC;
    m_isEnabled         (a_is_enabled),
    m_primaryMDC        (a_primary_mdc),          // NULL => WE ARE PRIMARY!
    m_orderBooks        (a_is_enabled
                         ?(IsPrimaryMDC()
                           ? new OrderBooksVec
                           : m_primaryMDC->m_orderBooks)
                         : nullptr
                        ),
    m_mktDepth          (a_mkt_depth),
    m_maxOrderBookLevels(0),                      // As yet
    m_isFullAmt         (a_is_full_amt),
    m_isSparse          (a_is_sparse),
    m_withSeqNums       (a_with_seq_nums),
    m_withRptSeqs       (a_with_rpt_seqs),
    m_contRptSeqs       (a_cont_rpt_seqs),
    m_withExplTrades    (a_with_expl_trades),
    m_withOLTrades      (a_with_ol_trades),
    m_maxOrders         (a_params.get<long>("MaxOrders", 0)),
    m_updated           (),                       // Initially empty
    m_useDynInit        (a_use_dyn_init),
    m_dynInitMode       (a_use_dyn_init),         // DynInitMode NOW!
    m_wasLastFragm      (false),                  // No updates yet...
    m_delayedNotify     (false),                  // ditto
    m_obSubscrIDs       (a_is_enabled
                         ? new ProtoSubscrIDsVec  // Empty as yet
                         : nullptr),
    m_trdSubscrIDs      (a_is_enabled && HasTrades()
                         ? new ProtoSubscrIDsVec  // Empty as yet
                         : nullptr),
    // "LatencyStats" ptrs are set to NULL here, and properly initialised later:
    m_socketStats       (nullptr),
    m_procStats         (nullptr),
    m_updtStats         (nullptr),
    m_stratStats        (nullptr),
    m_overAllStats      (nullptr)
  {
    //-----------------------------------------------------------------------//
    // MDC Functionality Enabled?                                            //
    //-----------------------------------------------------------------------//
    if (!m_isEnabled)
      return;

    //-----------------------------------------------------------------------//
    // Check the Args:                                                       //
    //-----------------------------------------------------------------------//
    // If this MDC is a Secondary one (ie another PrimaryMDC is specified), the
    // latter must be a Top-Primary one: nested Secondary MDCs are not allowed.
    // Also, a Secondary MDC cannot be a "Hist" one:
    //
    CHECK_ONLY
    (
      if (utxx::unlikely(a_params.empty() || a_mkt_depth < 0))
        throw utxx::badarg_error
              ("EConnector_MktData::Ctor: ", m_name, ": Invalid Params");

      if (m_primaryMDC != nullptr)
      {
        if (utxx::unlikely(!(m_primaryMDC->IsPrimaryMDC())))
          throw utxx::badarg_error
                ("EConnector_MktData::Ctor: ", m_name, ": PrimaryMDC=",
                 m_primaryMDC->m_name, " is itself a Secondary");
        if (utxx::unlikely(IsHistEnv()))
          throw utxx::badarg_error
                ("EConnector_MktData::Ctor: ", m_name, ": SecondaryMDC cannot "
                 "be in Hist mode");
      }
    )

    // Similar, MaxOrderBookLevels: Must be > 0 unless we use Sparse OrderBooks:
    if (!m_isSparse)
    {
      m_maxOrderBookLevels = a_params.get<int>("MaxOrderBookLevels");
      CHECK_ONLY
      (
        if (utxx::unlikely(m_maxOrderBookLevels <= 0))
          throw utxx::badarg_error
                ("EConnector_MktData::Ctor: ",    m_name,
                 ": Invalid MaxOrderBookLevels=", m_maxOrderBookLevels);
      )
    }
    else
      m_maxOrderBookLevels = 0;

    //-----------------------------------------------------------------------//
    // Set Ptrs to Stats:                                                    //
    //-----------------------------------------------------------------------//
    if (!IsHistEnv())
    {
      // For a non-Hist Connector, the PersistMgr must be initialised at this
      // point (done in "EConnector" Ctor):
      assert(!m_pm.IsEmpty());
      auto   segm  = m_pm.GetSegm();
      assert(segm != nullptr);

      // The Stats Ptrs will then point to ShM:
      m_socketStats   =
        segm->find_or_construct<LatencyStats>
              (SocketStatsON()) ("HWRecv-to-Socket     Latency", 0.0, 10000.0);
      m_procStats     =
        segm->find_or_construct<LatencyStats>
              (ProcStatsON())   ("Socket-to-ProcBuff   Latency", 0.0, 10000.0);
      m_updtStats     =
        segm->find_or_construct<LatencyStats>
              (UpdtStatsON())   ("ProcBuff-to-OBUpdate Latency", 0.0, 10000.0);
      m_stratStats    =
        segm->find_or_construct<LatencyStats>
              (StratStatsON())  ("OBUpdate-to-Strategy Latency", 0.0, 10000.0);
      m_overAllStats  =
        segm->find_or_construct<LatencyStats>
              (OverAllStatsON())("HWRecv-to-Strategy   Latency", 0.0, 20000.0);
    }
    else
    {
      // In Hist mode, Stats objs will need to be created explicitly on Heap
      // (this is for integrity only -- they make little sense in this case):
      m_socketStats =
        new LatencyStats("HWRecv-to-Socket     Latency", 0.0, 10000.0);
      m_procStats     =
        new LatencyStats("Socket-to-ProcBuff   Latency", 0.0, 10000.0);
      m_updtStats     =
        new LatencyStats("ProcBuff-to-OBUpdate Latency", 0.0, 10000.0);
      m_stratStats    =
        new LatencyStats("OBUpdate-to-Strategy Latency", 0.0, 10000.0);
      m_overAllStats  =
        new LatencyStats("HWRecv-to-Strategy   Latency", 0.0, 20000.0);
    }
    // So the Stats Ptrs must be non-NULL in any case:
    assert(m_socketStats  != nullptr && m_procStats  != nullptr &&
           m_updtStats    != nullptr && m_stratStats != nullptr &&
           m_overAllStats != nullptr);

    //-----------------------------------------------------------------------//
    // Create OrderBooks for all "SecDefD"s created by the parent:           //
    //-----------------------------------------------------------------------//
    // IMPORTANT: This includes also those "SecDefD"s which were left over from
    // a previous invocation in ShM, and so may not have been created in *this*
    // invocation. This is to avoid any inconsistencies when a "SecDefD" exists
    // but there is no corresp OrderBook (even an empty one).
    // XXX: Doing that other way round (removing left-over "SecDefD"s) is wrong
    // because such SecDefs could also be referenced from elsewhere (eg from the
    // RiskMgr):
    for (SecDefD const* instr: m_usedSecDefs)
    {
      assert(instr != nullptr);
      CreateOrderBook(*instr);

      LOG_INFO(2,
        "EConnector_MktData::Ctor: OrderBook Created: {}",
        instr->m_FullName.data())
    }
  }

  //=========================================================================//
  // Virtual Dtor:                                                           //
  //=========================================================================//
  EConnector_MktData::~EConnector_MktData() noexcept
  {
    // Prevent any exceptions from propagating out of the Dtor:
    try
    {
      // XXX: Presumably, the following code does not throw any exceptions:
      // For a Primary MDC, de-allocate the OrderBooks as well:
      if (m_orderBooks != nullptr)
      {
        if (IsPrimaryMDC())
          delete m_orderBooks;
        m_orderBooks     = nullptr;
      }

      // For any MDC, de-allocate the OBSubscrIDs and TrdSubscrIDs:
      if (m_obSubscrIDs  != nullptr)
      {
        delete m_obSubscrIDs;
        m_obSubscrIDs  = nullptr;
      }
      if (m_trdSubscrIDs != nullptr)
      {
        delete m_trdSubscrIDs;
        m_trdSubscrIDs = nullptr;
      }

      // In Hist mode, de-allocate Stats Objs (they were allocated on Heap in
      // that case); otherwise, they are located in ShM, and preserved there:
      if (IsHistEnv())
      {
        Delete0(m_socketStats);
        Delete0(m_procStats);
        Delete0(m_updtStats);
        Delete0(m_stratStats);
        Delete0(m_overAllStats);
      }
    }
    catch(...) {}
  }

  //=========================================================================//
  // "SubscribeMktData":                                                     //
  //=========================================================================//
  // Common templated implementation of virtual "Subscribe" (see below). A
  // Strategy of any type (but providing the necessary Call-Backs), subsc-
  // ribes to this Mkt Data Connector to receive event notifications:
  //
  void EConnector_MktData::SubscribeMktData
  (
    Strategy*                 a_strat,
    SecDefD const&            a_instr,
    OrderBook::UpdateEffectT  a_min_cb_level, // Min Strat Notification Level
    bool                      a_reg_instr_risks
  )
  {
    assert(a_strat != nullptr);

    // First of all, the MDC must be Enabled:
    CHECK_ONLY
    (
      if (utxx::unlikely(!m_isEnabled))
        throw utxx::badarg_error
              ("EConnector_MktData::SubscribeMktData: ", m_name,
               ": MDC Not Enabled");
    )
    // Get the corresp "OrderBook" -- it must exist:
    OrderBook* ob = FindOrderBook(a_instr);
    CHECK_ONLY
    (
      if (utxx::unlikely(ob == nullptr))
        throw utxx::badarg_error
              ("EConnector_MktData::SubscribeMktData: ", m_name,
               ": OrderBook Not Found for: ",    a_instr.m_FullName.data());
    )
    assert(&(ob->GetInstr()) == &a_instr);

    // If the Book is not initialised, or is empty or half-empty, or is incons-
    // istent -- produce a warning but still proceed:
    CHECK_ONLY
    (
      if (utxx::unlikely
         (ob->GetLastUpdateRptSeq() < 0 || ob->GetLastUpdateSeqNum() < 0 ||
          !(ob->IsReady())              || !(ob->IsConsistent())))
        LOG_WARN(2,
          "EConnector_MktData::SubscribeMktData: Instr={}: OrderBook not ready "
          "yet", a_instr.m_FullName.data())
    )
    // Install the subscription (call-backs) in the OrderBook itself   (it will
    // check for NULL ptrs and ignore a empty subscription):
    //
    ob->AddSubscr(a_strat, a_min_cb_level);

    // If requested, register the Subscribed Instrument and its OrderBook with
    // the RiskMgr. (NB: this does NOT allow us to register a different Order-
    // Book for the evaluation of risks of this Instr; if such functionality is
    // required, the caller must perform registeration explicitly).    Multiple
    // registration are OK -- they are simply ignored:
    //
    if (utxx::likely(m_riskMgr != nullptr && a_reg_instr_risks))
      m_riskMgr->Register(a_instr, ob);

    // Subscribe to Trading Session Events:
    EConnector::Subscribe(a_strat);

    // Finally, install the subscribed SecID in the Strategy itself -- it will
    // be needed for filtering SecID-specific notifications:
    //
    a_strat->Subscribe(a_instr.m_SecID);

    // Do NOT send the Strategy the current status of this OrderBook just upon
    // subscription, even if the OrderBook is already initialised and is  con-
    // sistent. In doing so, we could get various adverse effects, eg too wide
    // Bid-Ask spread:
    // All Done:
    LOG_INFO(1,
      "EConnector_MktData::SubscribeMktData: Subscribed: Instr={}, SecID={}"
      ", Strat={}",
      a_instr.m_FullName.data(), a_instr.m_SecID, a_strat->GetName())
  }

  //=========================================================================//
  // "UnSubscribe":                                                          //
  //=========================================================================//
  // XXX:
  // (*) At the moment, we unsubscribe the entire Strategy, ie all its Symbols
  //     (OrderBook Updates and Trades) and TradingEvents. Alternatively,  one
  //     could do per-Symbol granularity of "UnSubscribe", but  it  is  rarely
  //     useful (and also not clear what to do with TradingEvents then);
  // (*) This method overrides "EConnector::UnSubscribe":
  //
  void EConnector_MktData::UnSubscribe(Strategy const* a_strat)
  {
    assert(a_strat != nullptr && m_orderBooks != nullptr);

    // Traverse all "OrderBook"s:
    for (OrderBook& ob: *m_orderBooks)
    {
      // Remove subscriptions by "a_strat":
      SecDefD const& instr = ob.GetInstr();
      (void)  ob.RemoveSubscr(a_strat);

      // Other way round, remove this SecID from the "subscribed" list of the
      // Strategy. NB: We cannot simply clear that list, because it may cont-
      // ain SecIDs from multiple Connectors  --  and here we unsubscribe the
      // Strategy from just one Connector:
      //
      a_strat->UnSubscribe(instr.m_SecID);

      LOG_INFO(1,
        "EConnector_MktData::UnSubscribe: UnSubscribed: Instr={}, Strat={}",
        instr.m_FullName.data(), a_strat->GetName())
    }
    // Also UnSubscribe from Trading Session Events:
    EConnector::UnSubscribe(a_strat);
  }

  //=========================================================================//
  // "UnSubscribeAll":                                                       //
  //=========================================================================//
  // Remove ALL subscribed Strategies -- eg if the Connector is to be paused or
  // shut down. This method overrides "EConnector::UnSubscribeAll":
  //
  void EConnector_MktData::UnSubscribeAll()
  {
    assert(m_orderBooks != nullptr);

    for (OrderBook& ob: *m_orderBooks)
    {
      SecDefD const& instr = ob.GetInstr();
      (void)  ob.RemoveAllSubscrs();
      assert(ob.GetSubscrs().empty());

      // XXX: But do NOT invalidate the OrderBook  --  it may sometimes need to
      // continue receiving updates even if no Strategies are listening to them!
      LOG_INFO(1,
        "EConnector_MktData::UnSubscribe: UnSubscribed: Instr={}: "
        "From All Strats", instr.m_FullName.data())
    }
    // Also remove all Strategies' subscriptions for Trading Session Events:
    EConnector::UnSubscribeAll();
  }

  //=========================================================================//
  // "GetOrderBook": For external use:                                       //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // By SecDef:                                                              //
  //-------------------------------------------------------------------------//
  OrderBook const* EConnector_MktData::GetOrderBookOpt(SecDefD const& a_instr)
  const
    { return const_cast<EConnector_MktData*>(this)->FindOrderBook(a_instr); }

  //-------------------------------------------------------------------------//
  // By SecDef:                                                              //
  //-------------------------------------------------------------------------//
  OrderBook const& EConnector_MktData::GetOrderBook(SecDefD const& a_instr)
  const
  {
    OrderBook const* ob =
      const_cast<EConnector_MktData*>(this)->FindOrderBook(a_instr);

    CHECK_ONLY
    (
      if (utxx::unlikely(ob == nullptr))
        throw utxx::badarg_error
              ("EConnector_MktData::GetOrderBook: ", m_name, ", Instr=",
               a_instr.m_FullName.data(), ": Not Found");
    )
    // If OK:
    return *ob;
  }

  //-------------------------------------------------------------------------//
  // By SecID:                                                               //
  //-------------------------------------------------------------------------//
  OrderBook const& EConnector_MktData::GetOrderBook(SecID a_sec_id) const
  {
    OrderBook const* ob =
      const_cast<EConnector_MktData*>(this)->FindOrderBook(a_sec_id);

    CHECK_ONLY
    (
      if (utxx::unlikely(ob == nullptr))
        throw utxx::badarg_error
              ("EConnector_MktData::GetOrderBook: ", m_name, ", SecID=",
               a_sec_id, ": Not Found");
    )
    // If OK:
    return *ob;
  }

  //=========================================================================//
  // "FindOrderBook": For internal use: non-const, returning a ptr:          //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // By SecDef:                                                              //
  //-------------------------------------------------------------------------//
  OrderBook* EConnector_MktData::FindOrderBook(SecDefD const& a_instr)
  {
    // Use Linear Search -- it is OK because the number of constructed Order-
    // Books is typically small. SecDefs are compared by PTR:
    assert(m_orderBooks != nullptr);
    auto it =
      boost::container::find_if
      (
        m_orderBooks->begin(), m_orderBooks->end(),
        [&a_instr](OrderBook const& a_curr)->bool
        { return &(a_curr.GetInstr()) == &a_instr; }
      );
    return
      (utxx::likely(it != m_orderBooks->end()))
      ? &(*it)
      : nullptr;
  }

  //-------------------------------------------------------------------------//
  // By SecID:                                                               //
  //-------------------------------------------------------------------------//
  OrderBook* EConnector_MktData::FindOrderBook(SecID a_sec_id)
  {
    // XXX: In theory, SecID may be 0 -- but then the result is surely  NULL,
    // which may or may not be OK for the Caller. Again, use Linear Search by
    // SecID:
    assert(m_orderBooks != nullptr);
    auto it =
      boost::container::find_if
      (
        m_orderBooks->begin(), m_orderBooks->end(),
        [a_sec_id](OrderBook const& a_curr)->bool
        { return (a_curr.GetInstr().m_SecID == a_sec_id); }
      );
    return
      (utxx::likely(it != m_orderBooks->end()))
      ? &(*it)
      : nullptr;
  }

  //-------------------------------------------------------------------------//
  // By Symbol or AltSymbol:                                                 //
  //-------------------------------------------------------------------------//
  // XXX: This method is somewhat dangerous:   If more than one Instr has same
  // Symbol (eg with different Tenors), it will return the 1st entry found. So
  // it is for internal use only when we KNOW that Symbols are unique. For ex-
  // tra safety, in the Debug mode only, we  scan  all further OrderBooks, and
  // make sure there are no repeated Symbols:
  //
  template<bool UseAltSymbol>
  OrderBook* EConnector_MktData::FindOrderBook(char const* a_some_symbol)
  {
    // Try to find this Symbol (XXX: We do not check here whether it is empty
    // or not, but an empty one would surely result in NULL --  this is to be
    // handled by the Caller):
    assert(a_some_symbol != nullptr && m_orderBooks != nullptr);
    auto selector =
      [a_some_symbol](OrderBook const& a_curr)->bool
      { return
          UseAltSymbol
          ? (strcmp(a_curr.GetInstr().m_AltSymbol.data(), a_some_symbol) == 0)
          : (strcmp(a_curr.GetInstr().m_Symbol   .data(), a_some_symbol) == 0);
      };
    auto it =
      boost::container::find_if
      (m_orderBooks->begin(), m_orderBooks->end(), selector);

    DEBUG_ONLY(
    // If found, check that the Symbol is unique:
    if (utxx::likely(it != m_orderBooks->end()))
    {
      auto it2 =
        boost::container::find_if(std::next(it), m_orderBooks->end(), selector);

      if (utxx::unlikely(it2 != m_orderBooks->end()))
        throw utxx::runtime_error
              ("EConnector_MktData::FindOrderBook: ",
               (UseAltSymbol ? "AltSymbol=" : "Symbol="), a_some_symbol,
               ": Not Unique");
      }
    )
    // If OK (even if not found):
    return
      (utxx::likely(it != m_orderBooks->end()))
      ? &(*it)
      : nullptr;
  }

  //-------------------------------------------------------------------------//
  // Explicit Specialisations:                                               //
  //-------------------------------------------------------------------------//
  template
  OrderBook* EConnector_MktData::FindOrderBook<false>
    (char const* a_some_symbol);

  template
  OrderBook* EConnector_MktData::FindOrderBook<true>
    (char const* a_some_symbol);

  //=========================================================================//
  // "CreateOrderBook":                                                      //
  //=========================================================================//
  // Creates and installs a (yet empty) OrderBook and returns a ptr to it:
  //
  void EConnector_MktData::CreateOrderBook(SecDefD const& a_instr)
  {
    // Unlike SecDefs, OrderBooks are NOT persistent (not stored in ShM); so
    // each new OrderBook construction must be for a valid SettlDate: ALWAYS
    // PERFORM THIS CHECK! NB: SettlDate==0 is OK:
    //
    if (utxx::unlikely
       (a_instr.m_SettlDate != 0 && a_instr.m_SettlDate < GetCurrDateInt()))
      throw utxx::badarg_error
            ("EConnector_MktData::CreateOrderBook: ", m_name, ": Instr=",
             a_instr.m_FullName.data(), ": Expired SettlDate=",
             a_instr.m_SettlDate);

    // Check (once again if checked already) that an OrderBook for this SecID
    // does not exist yet:
    CHECK_ONLY
    (
      if (utxx::unlikely(FindOrderBook(a_instr) != nullptr))
        throw utxx::badarg_error
              ("EConnector_MktData::CreateOrderBook: ", m_name,
               ": OrderBook already exists: ",  a_instr.m_FullName.data(),
               "(SecID=",  a_instr.m_SecID, ')');
    )
    // Create and install a new empty OrderBook:
    assert(m_orderBooks != nullptr);
    m_orderBooks->emplace_back
    (
      // NB: "OrderBook" Ctor is invoked HERE with the following args:
      this,
      &a_instr,
      m_isFullAmt,
      m_isSparse,
      m_qt,
      m_withFracQtys,
      m_withSeqNums,
      m_withRptSeqs,
      m_contRptSeqs,
      m_maxOrderBookLevels,
      m_mktDepth,
      m_maxOrders
    );

    // In parallel with OrderBook creation, install {OB,Trd}SubscrID place-
    // holders:
    assert(m_obSubscrIDs != nullptr);
    m_obSubscrIDs->push_back(0);
    assert(m_obSubscrIDs->size() == m_orderBooks->size());

    if (HasTrades())
    {
      assert(m_trdSubscrIDs != nullptr);
      m_trdSubscrIDs->push_back(0);
      assert(m_trdSubscrIDs->size() == m_orderBooks->size());
    }
    else
      assert(m_trdSubscrIDs == nullptr);
    // All Done!
  }



  //=========================================================================//
  // "RegisterOrder":                                                        //
  //=========================================================================//
  void EConnector_MktData::RegisterOrder(Req12 const& a_req)
  {
    // Get the MDEntryID (NOT the ExchOrdID, as yet) from "a_req"; it must be a
    // numerical one:
    OrderID mdeID = a_req.m_mdEntryID.GetOrderID();
    CHECK_ONLY
    (
      if (utxx::unlikely(mdeID == 0))
      {
        // Cannot register this order  -- it is not a terribe error, so ignore
        // it and possibly produce a warning:
        LOG_WARN(2,
          "EConnector_MktData::RegisterOrder: ReqID={}: Invalid MDEntryID={}",
          a_req.m_id,  a_req.m_mdEntryID.ToString())
        return;
      }
    )
    // Find the OrderInfo for this "mdeID". Again, if not found, produce a warn:
    auto ob = FindOrderBook(a_req.m_aos->m_instr->m_SecID);
    OrderBook::OrderInfo* order =
      const_cast<OrderBook::OrderInfo*>(ob->GetOrderInfo(mdeID));
    CHECK_ONLY
    (
      if (utxx::unlikely(order == nullptr))
      {
        LOG_WARN(2,
          "EConnector_MktData::RegisterOrder: ReqID={}, MDEntryID={}: "
          "OrderInfo slot not found", a_req.m_id, mdeID)
        return;
      }
    )
    // If the Order already has a DIFFERENT Req12 registered, it is a more seri-
    // ous error -- but still do NOT throw an exception:
    CHECK_ONLY
    (
      if (utxx::unlikely(order->m_req12 != nullptr && order->m_req12 != &a_req))
      {
        LOG_ERROR(2,
          "EConnector_MktData::RegisterOrder: ReqID={}, MDEntryID={}: Another "
          "ReqID={} already registered",
          a_req.m_id, mdeID, order->m_req12->m_id)
        return;
      }
    )
    // Finally: Install the Req12 ptr in the corresp OrderInfo. Thus, the Req12
    // ptr will mark the AOS and OMC of our Order:
    order->m_req12 = &a_req;

    LOG_INFO(4,
      "EConnector_MktData::RegisterOrder: ReqID={}, MDEntryID={} registered",
      a_req.m_id, mdeID)
  }

  //=========================================================================//
  // "AddToUpdatedList":                                                     //
  //=========================================================================//
  bool EConnector_MktData::AddToUpdatedList
  (
    OrderBook*                a_ob,
    OrderBook::UpdateEffectT  a_upd,
    OrderBook::UpdatedSidesT  a_sides,
    utxx::time_val            a_ts_exch,
    utxx::time_val            a_ts_recv,
    utxx::time_val            a_ts_handl,
    utxx::time_val            a_ts_proc
  )
  const
  {
    // XXX: In the "DynInit" mode, Strategies are not notified yet, so there is
    // no point in adding any items to the  Updated list  -- it would only make
    // that list unnecessarily long. Still, return "true" in that case -- it is
    // NOT an error:
    if (utxx::unlikely(m_dynInitMode))
      return true;

    // Generic Case:
    utxx::time_val updtTS =   utxx::now_utc();
    assert(a_ob != nullptr && !(a_ts_recv.empty()));

    // (*) "NONE"    results are ignored   (if they get here by any means);
    // (*) all other results are installed  in "m_updated":
    //
    if (utxx::likely(a_upd != OrderBook::UpdateEffectT::NONE))
    {
      // Check if this OrderBook has already been updated; if so, most likely,
      // it's the last one in the list, so start from back down:
      auto it =
        boost::container::find_if
        (
          m_updated.rbegin(), m_updated.rend(),
          [a_ob](OBUpdateInfo const& curr)->bool
          { return curr.m_ob == a_ob; }
        );

      if (it != m_updated.rend())
      {
        // Update the entry -- Set the MAX (strongest) UpdateEffect (NB: it
        // could be ERROR!):
        OBUpdateInfo& obui = *it;
        obui.m_effect  = max(obui.m_effect, a_upd);

        // XXX: Also update the time stamps, although it means  that some stats
        // can be lost / distorted as as result. The preferrable way would   be
        // to keep all individual updates separate, but invoke Subscribers call-
        // backs on cumulative max effects only:
        obui.m_exchTS  = a_ts_exch;
        obui.m_recvTS  = a_ts_recv;
        obui.m_handlTS = a_ts_handl;
        obui.m_procTS  = a_ts_proc;
      }
      else
      {
        // This OrderBook is not on the "m_updated" list yet, so try to inst-
        // all it now:
        CHECK_ONLY
        (
          if (utxx::unlikely(m_updated.size() >= m_updated.capacity()))
          {
            // No room to install:
            LOG_ERROR(1,
              "EConnector_MktData::AddToUpdatedList: {}: OverFlow",
              a_ob->GetInstr().m_FullName.data())
            return false;
          }
        )
        // If OK:
        m_updated.emplace_back
          (a_ob, a_upd, a_sides, a_ts_exch, a_ts_recv, a_ts_handl, a_ts_proc,
           updtTS);
      }
    }
    // Iff there was an error in this MDE, return "false":
    return (a_upd < OrderBook::UpdateEffectT::ERROR);
  }

  //=========================================================================//
  // "ResetOrderBooksAndOrders":                                             //
  //=========================================================================//
  // Typically invoked before "Start", and also if eg a SnapShot init has failed
  // and we need to clean-up the state before trying again:
  //
  void EConnector_MktData::ResetOrderBooksAndOrders()
  {
    assert(m_orderBooks   != nullptr && m_obSubscrIDs != nullptr &&
           m_obSubscrIDs->size()     == m_orderBooks->size()     &&
          (!HasTrades()                                          ||
          (m_trdSubscrIDs != nullptr &&
           m_trdSubscrIDs->size()    == m_orderBooks->size()) ));

    // {OB,Trd}SubscrIDs probably need to be reset as well:
    for (OrderID& osi: *m_obSubscrIDs)
      osi = 0;

    if (m_trdSubscrIDs != nullptr)
      for (OrderID& tsi: *m_trdSubscrIDs)
        tsi = 0;

    // The rest is only done for a PRIMARY MDC, otherwise multiple invalidations
    // of shared OrderBooks may happen:
    if (!IsPrimaryMDC())
      return;

    for (OrderBook& ob: *m_orderBooks)
      // Invalidate the Book contents:
      ob.Invalidate();

    // Also, reset the following flds into their initial vals (which will  be
    // needed when the OrderBooks are re-initialised). XXX: The Boolean flags
    // are only relevant for UDP-based Connectors:
    m_updated.clear();
    m_dynInitMode   = m_useDynInit;
    m_wasLastFragm  = false;
    m_delayedNotify = false;
  }

  //=========================================================================//
  // "UpdateLatencyStats":                                                   //
  //=========================================================================//
  void EConnector_MktData::UpdateLatencyStats
  (
    OBUpdateInfo const& a_obui,     // Contains most of the TimeStamps
    utxx::time_val      a_ts_strat
  )
  const
  {
    // Compute the Latencies (in nsec) from RecvTS to the following TSes.
    long socketLat  = (a_obui.m_handlTS - a_obui.m_recvTS) .nanoseconds();
    long procLat    = (a_obui.m_procTS  - a_obui.m_handlTS).nanoseconds();
    long updtLat    = (a_obui.m_updtTS  - a_obui.m_procTS) .nanoseconds();
    long stratLat   = (a_ts_strat       - a_obui.m_updtTS) .nanoseconds();
    long overAllLat = (a_ts_strat       - a_obui.m_recvTS) .nanoseconds();

    // Update the Stats (cut off the extreme vals shich are certainly invalid,
    // and would only distort the stats):
    constexpr long MaxNS = 1'000'000'000;   // 1 sec is a reasonable limit

    if (utxx::likely(0 < socketLat  && socketLat  < MaxNS))
      m_socketStats -> Update(double(socketLat));

    if (utxx::likely(0 < procLat    && procLat    < MaxNS))
      m_procStats   -> Update(double(procLat));

    if (utxx::likely(0 < updtLat    && updtLat    < MaxNS))
      m_updtStats   -> Update(double(updtLat));

    if (utxx::likely(0 < stratLat   && stratLat   < MaxNS))
      m_stratStats  -> Update(double(stratLat));

    if (utxx::likely(0 < overAllLat && overAllLat < MaxNS))
      m_overAllStats-> Update(double(overAllLat));
  }
}
// End namespace MAQUETTE
