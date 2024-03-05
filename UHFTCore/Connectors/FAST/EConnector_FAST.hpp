// vim:ts=2:et
//===========================================================================//
//                   "Connectors/FAST/EConnector_FAST.hpp":                  //
//     Simple Generic FAST Connector -- Base for Real-Life FAST Connectors   //
//===========================================================================//
#pragma once

#include "Connectors/EConnector_MktData.hpp"
#include "Connectors/SeqNumBuffer.hpp"
#include "Connectors/FAST/SnapShotsCh.hpp"
#include "Connectors/FAST/IncrsCh.hpp"
#include <boost/property_tree/ptree.hpp>
#include <boost/algorithm/string.hpp>
#include <utility>
#include <vector>

namespace MAQUETTE
{
  //=========================================================================//
  // "EConnector_FAST" Class:                                                //
  //=========================================================================//
  // Template Params:
  // (*) "IncrementalRefreshOB": InrementalRefresh msg type used for updating
  //     the OrderBooks  (either Aggregated or OrdersLog-based, see below);
  // (*) "SnapShotOB"          : SnapShot msg type for OrderBooks (again, Agg-
  //     regated or OrdersLog-based);
  // (*) "WithOrdersLog"       : If set, the OrderBooks are OrdersLog-based (ie
  //     constructed via individual Orders), otherwise Aggregated;
  // (*) "IncrementalRefreshTr": InrementalRefresh msg type used for receiving
  //     Trades info (sometimes it is same as "IncrementalRefreshOB", or may
  //     be empty);
  // (*) "TradingSessionStatus": msg type for receiving global status updates;
  // (*) "IncrementalRefreshOB_TID", "SnapShot_TID", "IncrementalRefreshTr_TID",
  //     "TradingSessionStatus_TID": corresp TIDs:
  // XXX It would be nice not to have all those template params explicitly  but
  //     to pass them via "EConnector_FAST_Der". Unfortunately it looks like we
  //     cannot do that, because "EConnector_FAST_Der" would produce an "incomp-
  //     lete type" error...
  template
  <
    typename  SnapShotOB,
    FAST::TID SnapShotOB_TID,
    typename  IncrementalRefreshOB,
    FAST::TID IncrementalRefreshOB_TID,
    bool      WithOrdersLog,
    typename  IncrementalRefreshTr,
    FAST::TID IncrementalRefreshTr_TID,
    typename  TradingSessionStatus,
    FAST::TID TradingSessionStatus_TID,
    FAST::TID HeartBeat_TID,
    QtyTypeT  QT,
    typename  QR,
    typename  EConnector_FAST_Der,
    typename  OMC
  >
  class EConnector_FAST: public EConnector_MktData
  {
  private:
    //=======================================================================//
    // Exported Template Params (to "Channels"):                             //
    //=======================================================================//
    using SnapShotOBT = SnapShotOB;
    using IncrRefrOBT = IncrementalRefreshOB;
    using IncrRefrTrT = IncrementalRefreshTr;
    using TrSessStatT = TradingSessionStatus;
    using FQR         = QR;
    using OMCT        = OMC;

    constexpr static QtyTypeT  FQT = QT;

    constexpr static FAST::TID GetSnapShotOBTID()
      { return SnapShotOB_TID;           }

    constexpr static FAST::TID GetIncrRefrOBTID()
      { return IncrementalRefreshOB_TID; }

    constexpr static FAST::TID GetIncrRefrTrTID()
      { return IncrementalRefreshTr_TID; }

    constexpr static bool      HasOrdersLog()
      { return WithOrdersLog;            }

    constexpr static FAST::TID GetTrSessStatTID()
      { return TradingSessionStatus_TID; }

    constexpr static FAST::TID GetHeartBeatTID ()
      { return HeartBeat_TID;            }

    //=======================================================================//
    // Channel Types:                                                        //
    //=======================================================================//
    // NB:
    // (*) "{Order|Trade}IncrsBuff" and "{Order|Trade}IncrsCh" are mutually-re-
    //     cursive templated types!
    // (*) Channels are parametersied by both this class ("EConnector_FAST") and
    //     its final derivative ("EConnector_FAST_Der");  the former is required
    //     eg fot propagating template params, and the latter for extended  sem-
    //     antics (eg "AssetClass") which is only available there:
    //-----------------------------------------------------------------------//
    // "SnapShotsCh" :                                                       //
    //-----------------------------------------------------------------------//
    // NB: It does not use the "SeqNumBuffer", so it implements the Emplacer and
    // the Processor by itself:
    using SnapShotsCh  = FAST::SnapShotsCh
                        <EConnector_FAST, EConnector_FAST_Der>;

    friend class         FAST::SnapShotsCh
                        <EConnector_FAST, EConnector_FAST_Der>;

    //-----------------------------------------------------------------------//
    // "OBIncrsCh":                                                          //
    //-----------------------------------------------------------------------//
    // NB: Emplacer  is "OrderIncrsCh" itself,
    //     Processor is "OBsProc" of this class,
    //     ForOrderBooks = true  (so will use "OBIncrsBuff"):
    class OBsProc;
    using OBIncrsCh =    FAST::IncrsCh
                        <EConnector_FAST, EConnector_FAST_Der, true>;
    friend class         FAST::IncrsCh
                        <EConnector_FAST, EConnector_FAST_Der, true>;

    //----------------------------------------------------------------------//
    // "TradeIncrsCh":                                                      //
    //----------------------------------------------------------------------//
    // NB: Emplacer  is "TradeIncrsCh" itself,
    //     Processor is "TradeProc" of this class,
    //     ForOrderBooks = false (so will use "TradeIncrsBuff"):
    class TradeProc;
    using TradeIncrsCh = FAST::IncrsCh
                        <EConnector_FAST, EConnector_FAST_Der, false>;

    friend class         FAST::IncrsCh
                        <EConnector_FAST, EConnector_FAST_Der, false>;

    //-----------------------------------------------------------------------//
    // "{OB,Trade}IncrsBuff":                                                //
    //-----------------------------------------------------------------------//
    // Buffer for receiving out-of-order Incremental Updates on OBs/Trades.
    // (*) This is a "2nd-level" buffers (for DECODED msgs); the 1st-level  one
    //     is for RAW msgs, and is located inside the "EPollReactor" (that buff
    //     is created when the corresp "IncrsCh" is attached to the Reactor);
    // (*) "{OB,Trade}IncrsCh" also acts as the STATEFUL  "Emplacer" (but  NOT
    //     Processor) for the "{OB,Trade}IncrsBuff" resp; a ptr to the Channel
    //     ('A' or 'B') is passed to each call to "Put";
    // (*) "OBIncrsBuff" Ctor will get InitMode=true param for OBs,   since the
    //     OBIncrs are to be accumulated until the SnapShots are fully received,
    //     while InitMode=false for "TradeIncrsBuff" (because there are no Trade
    //     SnapShots);
    // (*) Processors are proided by "{OBs,Trades}Proc" classes, resp:
    //
    using OBIncrsBuff =
          SeqNumBuffer<IncrementalRefreshOB, OBIncrsCh,    OBsProc>;

    using TradeIncrsBuff =
          SeqNumBuffer<IncrementalRefreshTr, TradeIncrsCh, TradeProc>;

    //=======================================================================//
    // "OBsProc": Provides a "Processor" call-back for "OBIncrsBuff":        //
    //=======================================================================//
    class OBsProc: public boost::noncopyable
    {
    private:
      //---------------------------------------------------------------------//
      // Data Flds:                                                          //
      //---------------------------------------------------------------------//
      EConnector_FAST* const m_mdc;

      // (Ptrs to OrderBooks updated while processing 1 msg, + term NULL)
      OBsProc() = delete;

    public:
      //---------------------------------------------------------------------//
      // Non-Default Ctor:                                                   //
      //---------------------------------------------------------------------//
      OBsProc(EConnector_FAST* a_mdc)
      : m_mdc(a_mdc)
      { assert (m_mdc != nullptr); }

      //---------------------------------------------------------------------//
      // Processor itself:                                                   //
      //---------------------------------------------------------------------//
      // Effectively, this is just a wrapper around "UpdateOrderBooks", so its
      // invocation is not time-stamped:
      //
      void operator()
      (
        SeqNum                      a_sn,
        IncrementalRefreshOB const& a_msg,
        bool                        a_init_mode,
        utxx::time_val              a_ts_recv,
        utxx::time_val              a_ts_handl
      )
      {
        assert(a_sn == a_msg.m_MsgSeqNum);

        // Perform OrderBook(s) Updates according to "a_msg":
        // BEWARE: "a_msg" may be empty (buffer placeholder for a SeqNum, where-
        // as in fact that SeqNum was used by another msg type):
        if (utxx::unlikely(a_msg.m_NoMDEntries == 0))
          return;

        // Install this Incremental Update in the corresp OrderBook:
        (void) m_mdc->template UpdateOrderBooks
        <
          false,              // !IsSnapShot
          true,               // IsMultiCast
          true,               // WithIncrUpdates
          WithOrdersLog,
          false,              // !WithRelaxedOBs
          true,               // ChangeIsPartFill
          false,              // !NewEncdAsChange
          false,              // !ChangeEncdAsNew
          false,              // !IsFullAmt
          false,              // !IsSparse
          FindOrderBookBy::SecID,
          QT,
          QR,
          OMC
        >
        (a_sn, a_msg, a_init_mode, a_ts_recv, a_ts_handl);
      }
    };
    friend class OBsProc;

    //=======================================================================//
    // "TradeProc": Provides a "Processor" call-back for "TradeIncrsBuffer": //
    //=======================================================================//
    class TradeProc: public boost::noncopyable
    {
    private:
      //---------------------------------------------------------------------//
      // Data Flds:                                                          //
      //---------------------------------------------------------------------//
      EConnector_FAST* const m_mdc;
      TradeProc()  = delete;

    public:
      //---------------------------------------------------------------------//
      // Non-Default Ctor:                                                   //
      //---------------------------------------------------------------------//
      TradeProc(EConnector_FAST* a_mdc)
      : m_mdc  (a_mdc)
      { assert (m_mdc != nullptr); }

      //---------------------------------------------------------------------//
      // Trades Processor itself:                                            //
      //---------------------------------------------------------------------//
      // This is little more than a wrapper around "ProcessTrade", so its invo-
      // cation is not time-stamped by itself:
      //
      void operator()
      (
        SeqNum                      DEBUG_ONLY(a_sn),
        IncrementalRefreshTr const& a_msg,
        bool                        DEBUG_ONLY(a_init_mode),
        utxx::time_val              a_ts_recv,
        utxx::time_val              UNUSED_PARAM(a_ts_handl)
      )
      {
        // This method is only invoked if we are processing Explicitly-Subscred
        // Trades:
        assert(!a_init_mode && a_sn == a_msg.m_MsgSeqNum && m_mdc != nullptr &&
               m_mdc->m_withExplTrades);

        // Get the Trades from the "a_msg". NB: it may be empty (eg a buffer
        // placeholder for a SeqNum, whereas in fact that SeqNum was used by
        // another msg type -- eg some Index Stats coming from the same Chnl):
        //
        for (int i = 0; i < a_msg.GetNEntries(); ++i)
        {
          typename IncrementalRefreshTr::MDEntry const& mde = a_msg.GetEntry(i);

          // Get the OrderBook -- we assume Trades are IncrUpdates, so there
          // is a separate SecID / Symbol / OrderBook for each MDE -- no opt-
          // imisation here:
          OrderBook const* obp =
            m_mdc->template FindOrderBook<FindOrderBookBy::SecID>
              (a_msg, mde, "EConnector_FAST::TradeProc");

          if (obp == nullptr)
            continue;

          // If OK: Use Msg/MDE to extract the Trade Data:
          // XXX:   Even if it is a FullOrdersLog-based MDC, we pass order=NULL
          // at this point: "ProcessTrade" will try to get the OrderID again:
          //
          m_mdc->ProcessTrade
            <WithOrdersLog, QT, QR, OMC>
            (a_msg,  mde,  nullptr, *obp, Qty<QT,QR>::Invalid(),
             FIX::SideT::UNDEFINED, a_ts_recv);
        }
      }
    };
    friend class TradeProc;

    //=======================================================================//
    // Internal Data Flds:                                                   //
    //=======================================================================//
    // If Aggregated OrderBooks are used (ie "WithOrdersLog" is "false"), the
    // following in the Depth of OrderBook updates we receive.  Otherwise, it
    // is set to +oo:
  protected:
    mutable bool      m_isStarting;
    mutable bool      m_orderBooksInited;
  private:
    // Channel (only one -- eg 'A') for receiving OB SnapShots. It is ONLY used
    // if the corresp MDC is Primary. The ptr is OWNED:
    SnapShotsCh*      m_snapShotsChA;

    // Channels A and B for receiving Incremental Updates on OrderBooks, and a
    // common "SeqNumBuffer" for them. Used for both Primary and Secondary MDCs:
    OBsProc           m_obsProc;
    OBIncrsBuff       m_obIncrsBuff;
    OBIncrsCh         m_obIncrsChA;
    OBIncrsCh         m_obIncrsChB;

    // Similar Channels and the Buffer for Trades (but they are OPTIONAL, depen-
    // ding on the "m_withExplTrades" flag in the parent class: eg Trades can be
    // disabled at all, or can be inferred from the FullOrdersLog only). If used
    // at all, then typically present in both Primary and Secondary MDCs.
    // NB: all ptrs below ARE OWNED:
    TradeProc*        m_tradeProc;
    TradeIncrsBuff*   m_tradeIncrsBuff;
    TradeIncrsCh*     m_tradeIncrsChA;
    TradeIncrsCh*     m_tradeIncrsChB;

    // Default Ctor is deleted:
    EConnector_FAST() = delete;

  protected:
    //=======================================================================//
    // Non-Default Ctor, Dtor, Properties:                                   //
    //=======================================================================//
    // NB:
    // (*) this Ctor is "protected" -- only invoked via Ctors of Derived clses;
    // (*) "technical" params are passed via "boost::property_tree" where they
    //     must be located at Level 1 from the root:
    // (*) "EConnector" (virtual base) invocation is NOT required here:
    //
    EConnector_FAST
    (
      EConnector_MktData*                      a_primary_mdc,
      boost::property_tree::ptree       const& a_params,
      bool                                     a_cont_rpt_seqs,
      int                                      a_mkt_depth,
      bool                                     a_with_expl_trades,
      bool                                     a_with_ol_trades,
      SSM_Config                        const& a_snap_shots_conf,   // 'A' only
      std::pair<SSM_Config, SSM_Config> const& a_order_incrs_confs, // 'A','B'
      std::pair<SSM_Config, SSM_Config> const* a_trades_confs       // 'A','B'
    )                                                               // (NULL OK)
    : //---------------------------------------------------------------------//
      // "EConnector_MktData":                                               //
      //---------------------------------------------------------------------//
      EConnector_MktData
      (
        true,            // Yes, MDC functionality is enabled!
        a_primary_mdc,
        a_params,
        false,           // We do NOT have FullAmt (non-sweepable) pxs in FAST!
        false,           // Do not need Sparse OrderBooks either
        true,            // WithSeqNums=true: FAST OrderBooks use SeqNums
        true,            // WithRptSeqs=true: FAST OrderBooks use RptSeqs
        a_cont_rpt_seqs,
        a_mkt_depth,
        a_with_expl_trades,
        a_with_ol_trades,
        true             // FAST uses DynInitMode!
      ),
      //---------------------------------------------------------------------//
      // OrderBooks Buffer and Channels:                                     //
      //---------------------------------------------------------------------//
      m_isStarting       (false),
      m_orderBooksInited (false),
      m_snapShotsChA
      (
        // SnapShot Channels are for initialisation only, so they are only cre-
        // ated  in Primary MDCs:
        IsPrimaryMDC()
        ? new SnapShotsCh
          (
            a_snap_shots_conf,
            GetInterfaceIP   (a_params, 'A'),
            static_cast<EConnector_FAST_Der*>(this),
            a_params.get<int>("MaxInitRounds",   5)
          )
        : nullptr
      ),
      m_obsProc          (this),

      m_obIncrsBuff
      (
        a_params.get<int>("OBIncrsBuffCapacity"),
        a_params.get<int>("OBIncrsBuffMaxGap"),
        &m_obsProc,      // Processor
        m_logger,
        m_debugLevel,
        true             // With InitMode
      ),

      m_obIncrsChA
      (
        'A',
        a_order_incrs_confs.first,
        GetInterfaceIP   (a_params, 'A'),
        this             // This MDC (incl Buffer, Processor, etc)
      ),
      m_obIncrsChB
      (
        'B',
        a_order_incrs_confs.second,
        GetInterfaceIP   (a_params, 'B'),
        this             // Ditto
      ),

      //---------------------------------------------------------------------//
      // Trades Processor and Buffer: Optional on "WithExplTrades":          //
      //---------------------------------------------------------------------//
      // NB:
      // (*) TradeProc is created iff "WithExplTrades" flag is set in the parent
      //     class, AND we have the corresp configs available.
      // (*) Then TradeIncrsBuff & TradeIncrChs are created under the same cond:
      //
      m_tradeProc
        ((m_withExplTrades            &&
          a_trades_confs != nullptr   &&
          IncrementalRefreshTr_TID != 0)
         ? new TradeProc(this) : nullptr),

      m_tradeIncrsBuff
        ((m_tradeProc != nullptr)
         ? new TradeIncrsBuff
           (
             a_params.get<int>("TradesBuffCapacity"),
             a_params.get<int>("TradesBuffMaxGap"),
             m_tradeProc,    // Processor
             m_logger,
             m_debugLevel,
             false           // W/o  InitMode
           )
         : nullptr),

      //---------------------------------------------------------------------//
      // Trades Channels: Also optional on "WithExplTrades":                 //
      //---------------------------------------------------------------------//
      m_tradeIncrsChA
        ((m_tradeProc != nullptr)
         ? new TradeIncrsCh
               ('A', a_trades_confs->first,
                GetInterfaceIP(a_params, 'A'), this) // This MDC incl Buff, Proc
         : nullptr),

      m_tradeIncrsChB
        ((m_tradeProc != nullptr)
         ? new TradeIncrsCh
               ('B', a_trades_confs->second,
                GetInterfaceIP(a_params, 'B'), this) // Ditto
         : nullptr)
    {
      //---------------------------------------------------------------------//
      // Checks:                                                             //
      //---------------------------------------------------------------------//
      // Check that "m_withExplTrades" was not accidentially disabled  by  mis-
      // configuration (static or dynamic), or other way round -- Trade configs
      // provided but "m_withExplTrades" not set:
      //
      CHECK_ONLY
      (
        bool explTradesProvided =
          (a_trades_confs != nullptr && IncrementalRefreshTr_TID != 0);

        if (utxx::unlikely(m_withExplTrades != explTradesProvided))
          throw utxx::badarg_error
                ("EConnector_FAST::Ctor: TradesChannels Inconsistency: "
                 "Required=",    m_withExplTrades,
                 ", Provided=",  explTradesProvided);
      )
    }

    //=======================================================================//
    // Dtor -- still "virtual" (this class is NOT "final" yet):              //
    //=======================================================================//
    virtual ~EConnector_FAST() noexcept override
    {
      // Do not allow any exceptions to propagate from this Dtor:
      try
      {
        // XXX: Ideally, we would like to Stop() the Connector now if it is not
        // stopped yet,  but calling virtual funcs from the Dtor  is dangerous,
        // so don't do it...
        // De-allocate dynamic data structs:
        if (m_snapShotsChA   != nullptr)
        {
          delete m_snapShotsChA;
          m_snapShotsChA   = nullptr;
        }
        if (m_tradeProc      != nullptr)
        {
          delete m_tradeProc;
          m_tradeProc = nullptr;
        }
        if (m_tradeIncrsBuff != nullptr)
        {
          delete m_tradeIncrsBuff;
          m_tradeIncrsBuff = nullptr;
        }
        if (m_tradeIncrsChA  != nullptr)
        {
          delete m_tradeIncrsChA;
          m_tradeIncrsChA  = nullptr;
        }
        if (m_tradeIncrsChB  != nullptr)
        {
          delete m_tradeIncrsChB;
          m_tradeIncrsChB  = nullptr;
        }
      }
      catch (...) {}
    }

  public:
    //=======================================================================//
    // "Start", "Stop":                                                      //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "Start":                                                              //
    //-----------------------------------------------------------------------//
    virtual void Start() override
    {
      // Do not start it again if it is already Active or Starting:
      if (utxx::unlikely(IsActive() || m_isStarting))
      {
        LOG_WARN(2, "EConnector_FAST::Start: Already Started")
        return;
      }
      // For extra safety in case of a Re-Start, invalidate all OrderBooks now,
      // incl the Strats Subscr Info:
      ResetOrderBooksAndOrders();

      // Always start the OrderBook Channels (1 for Order SnapShots  which then
      // auto-terminates, and 2 for Order Increments which always keep running):
      if (m_snapShotsChA != nullptr)
        m_snapShotsChA->Start();

      m_obIncrsChA  .Start();
      m_obIncrsChB  .Start();

      // If receiving Trades data is enabled, start 2 Channels for Trade Incrs,
      // unless no separate Channles for trades rae required:
      if (m_tradeIncrsChA != nullptr)
        m_tradeIncrsChA->Start();
      if (m_tradeIncrsChB != nullptr)
        m_tradeIncrsChB->Start();

      // NB: We CANNOT state yet that the Connector is Active -- it needs to re-
      // ceive and install all SnapShots and pending IncrUpdates etc first. For
      // the same reason, do not set "m_orderBooksInited" yet.   Start has just
      // been  successfully commenced:
      m_isStarting = true;
    }

    //-----------------------------------------------------------------------//
    // "Stop":                                                               //
    //-----------------------------------------------------------------------//
    virtual void Stop() override
    {
      // Prevent multiple stopping attempts (as this could lead to inconsiten-
      // sies or even infinite recursion).  Ie, the curr state must be Active
      // or Starting in order to proceed with "Stop":
      // NB:
      if (utxx::unlikely(!(IsActive() || m_isStarting)))
      {
        LOG_WARN(2, "EConnector_FAST::Stop: Already Stopped")
        return;
      }

      // Stop the Channels in the reverse order of "Start"ing them (see above):
      if (m_tradeIncrsChA != nullptr)
        m_tradeIncrsChA->Stop();
      if (m_tradeIncrsChB != nullptr)
        m_tradeIncrsChB->Stop();

      m_obIncrsChB  .Stop();
      m_obIncrsChA  .Stop();

      if (m_snapShotsChA != nullptr)
        m_snapShotsChA->Stop();   // This Channel, most likely, stopped itself
                                  // long time ago...

      // XXX: For a Primary MDC only (which owns all its OrderBooks):
      if (IsPrimaryMDC())
      {
        // Reset all OrderBooks (and Orders, if applicable):
        ResetOrderBooksAndOrders();

        // The OrderBooks will need to be initialise from scratch by Start(), as
        // all intermediate updates will be lost, and the OrderBooks will become
        // out-of-sync. Thus, also reset the "Inited" and (just in case) "Start-
        // ing" flags. This marks the Connector as Inactive  (XXX: all secondary
        // MDCs should stop updating the OrderBooks as well):
        m_orderBooksInited = false;
        m_isStarting       = false;
        assert(!(IsActive()));

        // Notify the subscribed Strategies:
        // ON=false, for all SecIDs, no ExchTS, has ConnectorTS:
        utxx::time_val now = utxx::now_utc();
        ProcessStartStop(false, nullptr, 0, now, now);
      }
      // Finally:
      LOG_INFO(1, "EConnector_FAST::Stop: Connector STOPPED")
      m_logger->flush();
    }

    //-----------------------------------------------------------------------//
    // NB: "IsActive" remains purely-virtual                                 //
    //-----------------------------------------------------------------------//
    // That's why this class does NOT require the "EConnector" virtual based
    // ctor!

  private:
    //=======================================================================//
    // Internal Utils:                                                       //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "Process(TradingSessionStatus)":                                      //
    //-----------------------------------------------------------------------//
    bool Process
      (TradingSessionStatus const& a_msg, utxx::time_val a_ts_recv)
    {
      // Log the event:
      LOG_INFO(2,
        "EConnector_FAST::Process(TradingSessionStatus): Got Status={}",
        a_msg.m_TradSesStatus)

      // ExchTS: XXX: Normally, "SendingTime" would be available in this Msg,
      // but we currently do not use it, so take it simple:
      utxx::time_val exchTS = a_ts_recv;

      switch (a_msg.m_TradSesStatus)
      {
      case 1:
      case 3:
      case 101:
        // TradingSession has been Halted or Closed or otherwise Discontinued.
        // XXX: This could be a normal intra-day event (eg Clearing Break), or
        // an end-of-day closing, or an unplanned energency. FIXME: At the  mo-
        // ment, we cannot distinguish between those cases, or tell whether we
        // need to Stop / Restart the Connector -- we notify the Strategies and
        // let them do what they see fit (eg they can maintain a trading sched-
        // ule and check whether the stop is planned or not):
        //
        // ON=false, for all SecIDs, both ExchTS and ConnTS are available:
        ProcessStartStop(false, nullptr, 0, exchTS, a_ts_recv);
        break;

      case   2:
      case 100:
      case 102:
        // Trading is now Active (either a beginning-of-day Open, or after a
        // scheduled break, ot after an unplanned interruption):
        //
        // ON=true,  for all SecIDs, both ExchTS and ConnTS are available:
        ProcessStartStop(true, nullptr, 0, exchTS, a_ts_recv);
        break;

      case 103:
        // Trading Engine Shut-Down in such a way that the Connector state can
        // NOT be preserved, so need to terminate it.   `We do NOT re-start the
        // Connector automatically:
        Stop();
        return false;

      default: ;
        // All other event types are currently ignored
      }

      // If we got here: can continue reading msgs:
      return true;
    }

    //-----------------------------------------------------------------------//
    // "GetIncrsBuff" (For OrderBooks or Trades):                            //
    //-----------------------------------------------------------------------//
    // The args are dummy NULLs -- used only for overloading resolution:
    //
    OBIncrsBuff*    GetIncrsBuff(OBIncrsBuff*)    { return &m_obIncrsBuff;    }
    TradeIncrsBuff* GetIncrsBuff(TradeIncrsBuff*) { return  m_tradeIncrsBuff; }

    //-----------------------------------------------------------------------//
    // "GetInterfaceIP": For Reliable MCast Mgmt:                            //
    //-----------------------------------------------------------------------//
  protected:
    static std::string GetInterfaceIP
    (
      boost::property_tree::ptree const& a_params,
      char                               a_ab
    )
    {
      assert(a_ab == 'A' || a_ab == 'B');
      std::string ipsStr = a_params.get<std::string>("InterfaceIPs", "");
      if (ipsStr.empty())
        return "";  // This is valid, but not recommended

      // Otherwise: There could be up to 2 IPs specified:
      std::vector<std::string>  ips;
      boost::split(ips, ipsStr, boost::is_any_of(",;"));

      switch (ips.size())
      {
      case 1:
        // There is only 1 InterfaceIP, so return it:
        return ips[0];
      case 2:
        // Select the IP according to the 'A' or 'B' Channel:
        return ips[(a_ab == 'A') ? 0 : 1];
      default:
        // An error:
        throw utxx::badarg_error
              ("EConnector_FAST::GetInterfaceIP: Invalid IPs=", ipsStr);
      }
    }
  };
  // End of "EConnector_FAST" class decl
}
// End namespace MAQUETTE
