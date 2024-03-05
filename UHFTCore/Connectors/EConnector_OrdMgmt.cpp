 // vim:ts=2:et
//===========================================================================//
//                     "Connectors/EConnector_OrdMgmt.cpp":                  //
//            Init/Fini, Non-Templated Utils, Explicit Instances             //
//===========================================================================//
#include "Basis/IOUtils.h"
#include "Connectors/EConnector_OrdMgmt_Impl.hpp"
#include "Connectors/EConnector_OrdMgmt.hpp"
using namespace std;

namespace MAQUETTE
{
  //=========================================================================//
  // Non-Default Ctor:                                                       //
  //=========================================================================//
  EConnector_OrdMgmt::EConnector_OrdMgmt
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
  )
  : // NB: "EConnector" (virtual base) invocation is NOT required here...
    m_isEnabled            (a_is_enabled),

    // ShM setup: Done below (after the checks):
    m_maxAOSes             (0),
    m_AOSes                (nullptr),
    m_maxReq12s            (0),
    m_Req12s               (nullptr),
    m_maxTrades            (0),
    m_Trades               (nullptr),

    // Conventional map: NumericExchID => Req12 and other Properties:
    m_reqsByExchID         (),
    // PipeLineMode: can be moved to a higher (but not lower) degree of synchr-
    // onicity based on the run-time "a_params":
    m_pipeLineMode         (std::max<PipeLineModeT>
                            (a_pipeline_mode,
                             PipeLineModeT(a_params.get<int>("PipeLineMode",1))
                           )),
    m_sendExchOrdIDs       (a_send_exch_ord_ids),
    m_useExchOrdIDsMap     (a_use_exch_ord_ids_map),
    m_hasAtomicModify      (a_has_atomic_modify),
    m_hasPartFilledModify  (a_has_part_filled_modify),
    m_hasExecIDs           (a_has_exec_ids),
    m_hasMktOrders         (a_has_mkt_orders),
    m_cancelsNotThrottled  (a_params.get<bool>("CancelsNotThrottled", false)),

    // The following ptrs to Persistent Counters may be obtained later:
    m_RxSN                 (nullptr),
    m_TxSN                 (nullptr),
    m_OrdN                 (nullptr),
    m_ReqN                 (nullptr),
    m_TrdN                 (nullptr),
    m_nBuffdReqs           (0),
    m_linkedMDCs           (),

    m_throttlingPeriodSec  (a_throttling_period_sec),
    m_maxReqsPerPeriod     (a_max_reqs_per_period),
    m_reqRateThrottler     (m_throttlingPeriodSec), // Rate is per given Period
    // Pre-allocate space for a reasonable upper bound of the instantaneous num-
    // ber of Indications across all AOSes:
    m_allInds              (m_isEnabled
                            ? new boost::container::static_vector
                                  <Req12*,MaxInds>
                            : nullptr),
    m_pusherFD             (-1),                    // Not yet
    m_tradesLogger         (),                      // Initialised below
    m_balances             ()
  {
    //-----------------------------------------------------------------------//
    // OMC Functionality Enabled?                                            //
    //-----------------------------------------------------------------------//
    if (!m_isEnabled)
      return;

    //-----------------------------------------------------------------------//
    // Check the Args:                                                       //
    //-----------------------------------------------------------------------//
    // NB: Hist Mode is currently NOT supported for OMCs:
    //
    if (utxx::unlikely
       (m_throttlingPeriodSec <= 0 || m_maxReqsPerPeriod < 0 ||
        a_params.empty()           || IsHistEnv()))
      throw utxx::badarg_error
            ("EConnector_OrdMgmt:Ctor: ", m_name, ": Invalid Params");

    // (*) "PipeLineModeT::Wait0" is incompativle with "SendExchOrdIDs", becau-
    //     se in case of "Wait0", ExchOrdIDs are not yet available;
    // (*) "PipeLineModeT::Wait2" is only available with Non-Atomic "Modify":
    //
    if (utxx::unlikely
       ((m_pipeLineMode == PipeLineModeT::Wait0 && m_sendExchOrdIDs) ||
        (m_pipeLineMode == PipeLineModeT::Wait2 && m_hasAtomicModify)))
      throw utxx::badarg_error
            ("EConnector_OrdMgmt:Ctor: ", m_name, ":InCompatible Params: "
             "PipeLineMode=Wait", int(m_pipeLineMode), ", SendExchOrdIDs=",
             m_sendExchOrdIDs, ", HasAtomicModify=",   m_hasAtomicModify);

    //-----------------------------------------------------------------------//
    // Get AOSes, Req12s and Counters from ShM:                              //
    //-----------------------------------------------------------------------//
    // As we are in a Non-Hist mode, the "PersistMgr" must be initialised:
    assert(!(IsHistEnv() || m_pm.IsEmpty()));

    m_maxAOSes  = a_params.get<unsigned>("MaxAOSes" );
    m_maxReq12s = a_params.get<unsigned>("MaxReq12s");
    m_maxTrades = a_params.get<unsigned>("MaxTrades");

    if (utxx::unlikely
       (m_maxAOSes == 0 || m_maxReq12s < m_maxAOSes || m_maxTrades == 0))
      throw utxx::badarg_error
            ("EConnector_OrdMgmt:Ctor: ", m_name,
             ": Must have 0 < MaxAOSes <= MaxReqs, 0 < MaxTrades");

    // NB: Using our re-implementation of "find_or_construct", which also veri-
    // fies the size of an existing array (if found):
    m_AOSes  = m_pm.FindOrConstruct<AOS>  (AOSesON(),  &m_maxAOSes);
    m_Req12s = m_pm.FindOrConstruct<Req12>(Req12sON(), &m_maxReq12s);
    m_Trades = m_pm.FindOrConstruct<Trade>(TradesON(), &m_maxTrades);
    assert(m_AOSes != nullptr && m_Req12s != nullptr && m_Trades != nullptr);

    //-----------------------------------------------------------------------//
    // Reset the ptrs in AOSes:                                              //
    //-----------------------------------------------------------------------//
    // If the AOSes were created by a previous process instance, then unmapped
    // and mapped back in,  the following ptrs are still VALID  (as they point
    // to ShM locations):
    //   m_instr, m_lastReq (as well as "m_stratHash48"),
    // but the following ones are NOT valid as they pointed to the normal heap
    // of the original process:
    //   m_omc, m_strategy;
    // So:
    // (*) the OMC ptrs  are modified now to "this";
    // (*) Strategy ptrs are zeroed-out now, modified on new "Subscribe":
    // (*) UserData is zeroed-out as it may contain dangling ptrs:
    //
    for (AOS* curr = m_AOSes, *end = m_AOSes + m_maxAOSes; curr < end; ++curr)
    {
      const_cast<Strategy*&>          (curr->m_strategy) = nullptr;
      const_cast<EConnector_OrdMgmt*&>(curr->m_omc)      = this;
      curr->m_userData                                   = UserData();
    }

    //-----------------------------------------------------------------------//
    // Now initialise the Counters properly:                                 //
    //-----------------------------------------------------------------------//
    // NB: OrdN, ReqN and TrdN are normally initialised with 1, but that can
    // change if eg the prev state was lost on the Client side, but still exists
    // on the Server side -- in that case, we need an initial offset;
    // XXX: Obviously, if "initOrdN"/"initReqN"/"initTrdN" is too large, we will
    // eventually get an exception of AOS / Req12 / Trade allocation:
    //
    SeqNum  initRxSN = a_params.get<SeqNum> ("InitRxSN", 1);
    SeqNum  initTxSN = a_params.get<SeqNum> ("InitTxSN", 1);
    OrderID initOrdN = a_params.get<OrderID>("InitOrdN", 1);
    OrderID initReqN = a_params.get<OrderID>("InitReqN", 1);
    OrderID initTrdN = a_params.get<OrderID>("InitTrdN", 1);

    auto   segm  = m_pm.GetSegm();
    assert(segm != nullptr);

    // Get Counter Ptrs. Because the Counters are not arrays, the standard
    // "find_or_construct" is sufficient:
    m_RxSN = segm->find_or_construct<SeqNum> (RxSN_ON())(initRxSN);
    m_TxSN = segm->find_or_construct<SeqNum> (TxSN_ON())(initTxSN);
    m_OrdN = segm->find_or_construct<OrderID>(OrdN_ON())(initOrdN);
    m_ReqN = segm->find_or_construct<OrderID>(ReqN_ON())(initReqN);
    m_TrdN = segm->find_or_construct<OrderID>(TrdN_ON())(initTrdN);

    // All Counters must now exist:
    assert(m_RxSN != nullptr && m_TxSN != nullptr && m_OrdN != nullptr &&
           m_ReqN != nullptr && m_TrdN != nullptr);

    // NB: If "OrdN" / "ReqN" already exists in "find_or_construct" above, the
    // "init{Ord,Req}N" val would not be applied; but that would be wrong  in
    // case if we really need an advance. It does NOT apply to "TrdN" which is
    // an internal counter (always 0-based):
    //
    *m_OrdN = max<OrderID>(*m_OrdN, initOrdN);
    *m_ReqN = max<OrderID>(*m_ReqN, initReqN);

    // And for SeqNums, it is other way around: we may want to move the expec-
    // ted values BACKWARDS, so that a "ResendRequest" was generated. But only
    // do that if a resp explicit (non-1) SeqNum was specified; otherwise we
    // would always be re-setting SeqNums to 1!
    //
    if (initRxSN > 1)
      *m_RxSN = min<SeqNum>(*m_RxSN, initRxSN);

    if (initTxSN > 1)
      *m_TxSN = min<SeqNum>(*m_TxSN, initTxSN);

    //-----------------------------------------------------------------------//
    // Initialise the Trades Logger:                                         //
    //-----------------------------------------------------------------------//
    string tradesLogFile = a_params.get<string>("TradesLogFile");

    if (utxx::likely(!tradesLogFile.empty()))
    {
      // NB: It will be Single-Threaded:
      m_tradesLoggerShP = IO::MkLogger(m_name + "-Trades", tradesLogFile);
      m_tradesLogger    = m_tradesLoggerShP.get();
      assert(m_tradesLogger != nullptr);
    }

    //-----------------------------------------------------------------------//
    // Adjust and verify the Throttling Params:                              //
    //-----------------------------------------------------------------------//
    // Settings given in "a_params", if BOTH valid, will OVERRIDE the ones given
    // in the explicit args (because the latter would typically come from static
    // defaults):
    int throttlPeriod    = a_params.get<int>("ThrottlingPeriodSec", 0);
    int maxReqsPerPeriod = a_params.get<int>("MaxReqsPerPeriod",   -1);

    if (throttlPeriod > 0 && maxReqsPerPeriod >= 0)
    {
      m_throttlingPeriodSec = throttlPeriod;
      m_maxReqsPerPeriod    = maxReqsPerPeriod;
      // Don't forget to re-initialise the Throttler itself:
      m_reqRateThrottler.init(m_throttlingPeriodSec);
    }
    LOG_INFO(1, "EConnector_OrdMgmt::Ctor: MaxReqsPerPeriod={}, Period={}s",
                m_maxReqsPerPeriod, m_throttlingPeriodSec)

    if (utxx::unlikely
       (m_throttlingPeriodSec <= 0 || m_maxReqsPerPeriod < 0))
      // NB: MaxReqsPerPeriod == 0 is valid, and means "+oo"!
      throw utxx::badarg_error
            ("EConnector_OrdMgmt::Ctor: ", m_name,
             ": Invalid ReqRateThrottling Params: Period=",
             m_throttlingPeriodSec, " sec, MaxReqsPerPeriod=",
             m_maxReqsPerPeriod);

    //-----------------------------------------------------------------------//
    // Optimise the HashTable:                                               //
    //-----------------------------------------------------------------------//
    if (m_useExchOrdIDsMap)
    {
      // XXX: PipeLinedReqs may or may not be compatible with UseExchOrdIDsMap.
      // Currently, ExchOrdIDsMap is a secondary (after ReqID/AOSID) method of
      // finding the Req12 for a given order -- so we ALLOW it with PipeLined-
      // Reqs:
      m_reqsByExchID.max_load_factor(1.0);
      m_reqsByExchID.reserve        (m_maxReq12s);
    }
    //-----------------------------------------------------------------------//
    // Finally, if the RiskMgr is used, register this OMC with it:           //
    //-----------------------------------------------------------------------//
    if (utxx::likely(m_riskMgr != nullptr))
      m_riskMgr->Register(this);
  }

  //=========================================================================//
  // Virtual Dtor:                                                           //
  //=========================================================================//
  EConnector_OrdMgmt::~EConnector_OrdMgmt() noexcept
  {
    // Make sure exceptions do not propagate out of the Dtor (although in this
    // case there are probably none):
    try
    {
      // Once again, we are in a Non-Hist mode, so no need to de-allocate Coun-
      // ters etc:
      assert(!IsHistEnv());

      // Remove the PusherTask Timer (NoOp if it is not active). NB: Must do it
      // BEFORE "m_allInds" are invaliadated:
      RemovePusherTimer();

      m_tradesLogger = nullptr;
      m_RxSN         = nullptr;
      m_TxSN         = nullptr;
      m_OrdN         = nullptr;
      m_ReqN         = nullptr;

      if (m_allInds != nullptr)
      {
        delete m_allInds;
        m_allInds    = nullptr;
      }
    }
    catch(...){}
  }

  //=========================================================================//
  // Virtual "Subscribe" (specifically for OrdMgmt):                         //
  //=========================================================================//
  void EConnector_OrdMgmt::Subscribe(Strategy* a_strat)
  {
    CHECK_ONLY
    (
      if (utxx::unlikely(a_strat == nullptr))
        throw utxx::badarg_error
              ("EConnector_OrdMgmt::Subscribe: ", m_name,
               ": Strat must not be NULL");
    )
    // For AOSes created by a previous instance of this Strategy (with the sa-
    // me Hash48 code), re-target Strategy ptrs to the curr instance:
    unsigned long hash48 = a_strat->GetHash48();

    for (AOS* curr = m_AOSes, *end = m_AOSes + m_maxAOSes; curr < end; ++curr)
      if (curr->m_stratHash48 == hash48)
        const_cast<Strategy*&>(curr->m_strategy) = a_strat;

    // Also, subscribe to the TradingEvents using the base class method (OK
    // to do it multiple times):
    EConnector::Subscribe(a_strat);
  }

  //=========================================================================//
  // "LinkMDC":                                                              //
  //=========================================================================//
  void EConnector_OrdMgmt::LinkMDC(EConnector_MktData* a_mdc)
  {
    CHECK_ONLY
    (
      if (utxx::unlikely(a_mdc == nullptr))
        throw utxx::badarg_error("EConnector_OrdMgmt::LinkMDC: NULL MDC");

      assert(m_linkedMDCs.size() <= MaxLinkedMDCs);
      if (utxx::unlikely(m_linkedMDCs.size() == MaxLinkedMDCs))
        throw utxx::badarg_error("EConnector_OrdMgmt::LinkMDC: Too many MDCs");
    )
    // If OK:
    m_linkedMDCs.push_back(a_mdc);

    LOG_INFO(2, "MDC={} linked to OMC={}", a_mdc->GetName(), GetName())

    // XXX: But Orders which have already been made (if any), will NOT be reg-
    // istered in the newly-linked MDC!
  }

  //=========================================================================//
  // "GetReq12BySeqNum":                                                     //
  //=========================================================================//
  // NB: This is currently a NON-throwing function (in case of the SeqNum is
  // not found -- NULL is returned):
  //
  Req12* EConnector_OrdMgmt::GetReq12BySeqNum
  (
    SeqNum      a_sn,
    char const* CHECK_ONLY(a_where)
  )
  const
  {
    assert(a_where != nullptr);
    if (utxx::unlikely(a_sn <= 0))
      return nullptr;

    // NB: ReqN is the *next* ReqID, and ReqID=1 (not 0) is  the initial one:
    // Traverse the Reqs backwards;  because "SeqNum"s can be reset, stop if
    // non-monotonicity is encountered. XXX: We will be searching through ALL
    // available Req12s, which may be TERRIBLY inefficient:
    //
    assert(*m_ReqN <= m_maxReq12s);
    Req12* last     = m_Req12s + (*m_ReqN - 1);
    Req12* first    = m_Req12s + 1;
    SeqNum prevSN   = LONG_MAX;

    for (Req12* req = last; req >= first; --req)
    {
      if (req->m_seqNum == a_sn)
      {
        // Check the validity of this "req" before returning it:
        CHECK_ONLY
        (
          OrderID id = OrderID(req - m_Req12s);

          if (utxx::unlikely(req->m_id != id || req->m_aos == nullptr))
            throw utxx::logic_error
                  ("EConnector_OrdMgmt::GetReq12BySeqNum(", a_where, "): ",
                   m_name, ": Uninitialised Req12 @ ", id, ": StoredID=",
                   req->m_id, ((req->m_aos == nullptr) ? "AOS=NULL" : ""));
        )
        // If OK:
        return req;
      }
      if (utxx::unlikely(req->m_seqNum < a_sn || req->m_seqNum >= prevSN))
        // Either went too far down, or non-monotonic:
        break;
      prevSN = req->m_seqNum;
    }
    // If we got here: Not found:
    return nullptr;
  }

  //=========================================================================//
  // "BackPropagateSendTS":                                                  //
  //=========================================================================//
  void EConnector_OrdMgmt::BackPropagateSendTS
    (utxx::time_val a_ts_send, OrderID a_from_id)
  {
    // XXX: Theoretically, "a_from_id" could be 0 if there was no prev Req12
    // (this is harmless), or it could become wrapped over (which  is a very
    // serious error cond -- better check it):
    assert(!a_ts_send.empty() && a_from_id < *m_ReqN && *m_ReqN < m_maxReq12s);

    // Also, to prevent long looping, we never go back for more than "m_nBuffd-
    // Reqs" (which is the upper bound -- the actual number of buffered Reqs
    // may be smaller if over-writing of Indicated ones has occurred):
    int j = 0;
    for (OrderID i = a_from_id; i >= 1 && j < m_nBuffdReqs; --i, ++j)
    {
      Req12* ri = m_Req12s + i;
      if (ri->m_ts_sent.empty())
        ri->m_ts_sent = a_ts_send;
      else
        // Encountered a valid SendTS -- do not go any deeper!
        // XXX: Actually, we may want to enfore the monotonicity of TSs  (ie
        // ri->m_ts_sent <= a_ts_send), but this invariant is not critically
        // important, so do not check it for now:
        break;
    }
  }

  //=========================================================================//
  // PusherTask TimerFD Mgmt:                                                //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "RemovePusherTimer":                                                    //
  //-------------------------------------------------------------------------//
  void EConnector_OrdMgmt::RemovePusherTimer()
  {
    if (m_pusherFD >= 0)
    {
      if (utxx::likely(EConnector::m_reactor != nullptr))
        EConnector::m_reactor->Remove(m_pusherFD);

      LOG_INFO(2, "EConnector_OrdMgmt::RemovePusherTimer: PusherFD={} removed",
               m_pusherFD)

      // Close the socket:
      (void) close(m_pusherFD);
      m_pusherFD = -1;
    }
    // XXX: It is also a good idea to invalidate the "AllInds" vector, as there
    // is no PusherTask to process it anyway:
    if (m_allInds != nullptr)
      m_allInds->clear();
  }

  //-------------------------------------------------------------------------//
  // "PusherTimerErrHandler":                                                //
  //-------------------------------------------------------------------------//
  void EConnector_OrdMgmt::PusherTimerErrHandler
    (int a_err_code, uint32_t a_events, char const* a_msg)
  {
    // There should be NO errors associated with this Timer at all. If one occ-
    // urs, we better shut down completely:
    LOG_ERROR(1,
      "EConnector_OrdMgmt::PusherTimerErrHandler: TimerFD={}, ErrCode={}, "
      "Events={}, Msg={}: STOPPING",
      m_pusherFD, a_err_code,
      EConnector::m_reactor->EPollEventsToStr(a_events),
      (a_msg != nullptr) ? a_msg : "")
    Stop();
  }
}
// End namespace MAQUETTE
