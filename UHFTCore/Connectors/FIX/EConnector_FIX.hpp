// vims:ts=2:et
//===========================================================================//
//                    "Connectors/FIX/EConnector_FIX.hpp":                   //
//===========================================================================//
#pragma once

#include "Connectors/FIX/EConnector_FIX.h"
#include "Connectors/EConnector_OrdMgmt.hpp"
#include "Connectors/FIX/FIX_ConnectorSessMgr.hpp"
#include "Basis/QtyConvs.hpp"
#include <cassert>
#include <utxx/compiler_hints.hpp>

// XXX: Need to include all FIX Venue-Specific Configs:
#include "Protocols/FIX/Features.h"
#include "Protocols/FIX/Msgs.h"
#if (!CRYPTO_ONLY)
#include "Venues/AlfaECN/Configs_FIX.hpp"
#include "Venues/AlfaFIX2/Configs_FIX.hpp"
#include "Venues/EBS/Configs_FIX.hpp"
#include "Venues/FORTS/Configs_FIX.hpp"
#include "Venues/FXBA/Configs_FIX.hpp"
#include "Venues/MICEX/Configs_FIX.hpp"
#include "Venues/NTProgress/Configs_FIX.hpp"
#include "Venues/Currenex/Configs_FIX.hpp"
#include "Venues/HotSpotFX/Configs_FIX.hpp"
#endif
#include "Venues/Cumberland/Configs_FIX.hpp"
#include "Venues/LMAX/Configs_FIX.hpp"
#include "Venues/TT/Configs_FIX.hpp"

namespace MAQUETTE
{
  //=========================================================================//
  // Non-Default Ctor:                                                       //
  //=========================================================================//
  // NB: "AccountKey" can be used as a unique name for this instance of the FIX
  // Connector (eg, it would normally include the word "FIX", the Exchange name,
  // and would imply OurCompID):
  //
  template<FIX::DialectT::type D>
  inline EConnector_FIX<D>::EConnector_FIX
  (
    EPollReactor*                      a_reactor,
    SecDefsMgr*                        a_sec_defs_mgr,
    std::vector<std::string>    const* a_only_symbols,
    RiskMgr*                           a_risk_mgr,
    boost::property_tree::ptree const& a_params,
    EConnector_MktData*                a_primary_mdc
  )
  : //-----------------------------------------------------------------------//
    // "EConnector": Virtual Base:                                           //
    //-----------------------------------------------------------------------//
    EConnector
    (
      a_params.get<std::string>("AccountKey"),   // Becomes "m_name"
      FIX::ProtocolFeatures<D>::s_exchange,
      0,                       // No extra ShM...
      a_reactor,
      false,                   // No need for BusyWait in FIX!
      a_sec_defs_mgr,
      GetFIXSrcSecDefs<D>(EConnector::GetMQTEnv
        (a_params.get<std::string>("AccountKey"))),
      a_only_symbols,
      a_params.get<bool>       ("ExplSettlDatesOnly",     true),
      FIX::ProtocolFeatures<D>::s_useTenorInSecID,
      a_risk_mgr,
      a_params,
      QT,
      FIX::ProtocolFeatures<D>::s_hasFracQtys
    ),
    //-----------------------------------------------------------------------//
    // "EConnector_OrdMgmt":                                                 //
    //-----------------------------------------------------------------------//
    EConnector_OrdMgmt
    (
      GetFIXSessionConfig<D>(EConnector::m_name).m_IsOrdMgmt, // IsEnabled?
      a_params,
      GetFIXAccountConfig<D>(EConnector::m_name).m_ThrottlingPeriodSec,
      GetFIXAccountConfig<D>(EConnector::m_name).m_MaxReqsPerPeriod,
      (FIX::ProtocolFeatures<D>::s_hasPipeLinedReqs
        ? PipeLineModeT::Wait0
        : PipeLineModeT::Wait1
      ),                        // Wait2 not used  in FIX
      false,                    // No SendExchOrdIDs: FIX is based on ClOrdIDs
      false,                    // No ExchOrdIDsMap : FIX is based on ClOrdIDs
      true,                     // FIX always has "atomic" "CancelReplace"
      FIX::ProtocolFeatures<D>::s_hasPartFilledModify,
      true,                     // FIX should always have ExecIDs
      FIX::ProtocolFeatures<D>::s_hasMktOrders
    ),
    //-----------------------------------------------------------------------//
    // "EConnector_MktData":                                                 //
    //-----------------------------------------------------------------------//
    EConnector_MktData
    (
      GetFIXSessionConfig<D>(EConnector::m_name).m_IsMktData, // IsEnabled?
      a_primary_mdc,
      a_params,
      false,          // XXX: FullAmt pxs not supported yet!
      IsSparse,
      true,           // Use SeqNums (but with care!)
      false,          // But no RptSeqs in FIX
      false,          // So their Continuity does not matter
      // MktDepth: Always use 0 (means +oo) for OrdersLog-based OrderBooks:
      FIX::ProtocolFeatures<D>::s_useOrdersLog
        ? 0
        : int(a_params.get<unsigned>("MktDepth")),
      a_params.get<bool>("WithTrades", true),
      false,          // No OrdersLog-based Trades in FIX?
      false           // No DynInitMode for TCP!
    ),
    //-----------------------------------------------------------------------//
    // "SessMgr":                                                            //
    //-----------------------------------------------------------------------//
    // "EConnector_FIX" will serve as the Processor for "SessMgr":
    //
    SessMgr
    (
      a_params.get<std::string>("AccountKey"),
      m_reactor,
      this,           // Processor
      EConnector::m_logger,
      a_params,
      GetFIXSessionConfig<D>(a_params.get<std::string>("AccountKey")),
      GetFIXAccountConfig<D>(a_params.get<std::string>("AccountKey")),
      // TxSN and RxSN are persistent for OMC only:
      GetFIXSessionConfig<D>(
        a_params.get<std::string>("AccountKey")).m_IsOrdMgmt
        ? EConnector_OrdMgmt::m_TxSN : nullptr,
      GetFIXSessionConfig<D>(
        a_params.get<std::string>("AccountKey")).m_IsOrdMgmt
        ? EConnector_OrdMgmt::m_RxSN : nullptr
    ),
    //-----------------------------------------------------------------------//
    // "EConnector_FIX":                                                     //
    //-----------------------------------------------------------------------//
    m_secDefHandler(),
    m_secListHandler(),
    m_quoteIDs      (GetFIXSessionConfig<D>(EConnector::m_name).m_IsMktData
                     ? new ProtoQuoteIDsVec    // Empty as yet
                     : nullptr)
  {
    //-----------------------------------------------------------------------//
    // Checks:                                                               //
    //-----------------------------------------------------------------------//
    // Check the Consistency of settings in the "EConnector*" and in the "FIX_
    // ConnectorSessMgr" (actually in "FIX::SessData"). XXX: Better get rid of
    // all such data duplication completely, but this is difficult:
    assert
      (EConnector::m_name       == SessMgr::m_name      &&
       EConnector::m_reactor    == SessMgr::m_reactor   &&
       this                     == SessMgr::m_processor &&
       EConnector::m_logger     == SessMgr::m_logger    &&
       EConnector::m_debugLevel == SessMgr::m_debugLevel);
  }

  //=========================================================================//
  // Dtor, "Start", "Stop", "IsActive":                                      //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // Dtor:                                                                   //
  //-------------------------------------------------------------------------//
  template<FIX::DialectT::type D>
  inline EConnector_FIX<D>::~EConnector_FIX() noexcept
  {
    // Cancel the PusherTask Timer (OMC only):
    if (SessMgr::IsOrdMgmt())
      EConnector_OrdMgmt::RemovePusherTimer();
  }

  //-------------------------------------------------------------------------//
  // "Start":                                                                //
  //-------------------------------------------------------------------------//
  template<FIX::DialectT::type D>
  inline void   EConnector_FIX<D>::Start()
  {
    // Create a periodic PusherTask for "AllInds" (which are cleared here):
    // OMC only:
    if (SessMgr::IsOrdMgmt())
      EConnector_OrdMgmt::template CreatePusherTask
        <EConnector_FIX<D>, SessMgr>(this, this);

    // In an MDC, reset all state for safety before the new Start, so there are
    // no left-over data:
    if (SessMgr::IsMktData())
      EConnector_MktData::ResetOrderBooksAndOrders();

    // The rest delegated to the SessMgr (IsOrdMgmt=true); this in particular
    // initialises the TCP connection:
    SessMgr::Start();
  }

  //-------------------------------------------------------------------------//
  // "Stop":                                                                 //
  //-------------------------------------------------------------------------//
  template<FIX::DialectT::type D>
  inline void   EConnector_FIX<D>::Stop ()
  {
    // Delegated to the SessMgr with FullStop=true:
    SessMgr::template Stop<true>();

    // Also remove the PusherTask (also invalidates "AllInds"), OMC only:
    if (SessMgr::IsMktData())
      EConnector_OrdMgmt::RemovePusherTimer();

    // NB: In an MDC, do NOT "ResetOrderBooksAndOrders" -- the MktData will
    // continue to arrive until "StopNow" is invoked,    and we do not want
    // bogus errors!
  }

  //-------------------------------------------------------------------------//
  // "IsActive":                                                             //
  //-------------------------------------------------------------------------//
  template<FIX::DialectT::type D>
  inline bool   EConnector_FIX<D>::IsActive() const
  {
    // Delegated to SessMgr:
    return SessMgr::IsActive();
  }

  //=========================================================================//
  // "NewOrder":                                                             //
  //=========================================================================//
  template<FIX::DialectT::type D>
  inline AOS const* EConnector_FIX<D>::NewOrder
  (
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
  )
  {
    // Invoke                EConnector_OrdMgmt::NewOrderGen (),
    // which in turn invokes   FIX::ProtoEngine::NewOrderImpl():
    //
    return EConnector_OrdMgmt::template NewOrderGen<QT, QR>
    (
      this,             this,
      a_strategy,       a_instr,        a_ord_type,    a_is_buy,
      a_px,             a_is_aggr,      a_qty,         a_ts_md_exch,
      a_ts_md_conn,     a_ts_md_strat,  a_batch_send,  a_time_in_force,
      a_expire_date,    a_qty_show,     a_qty_min,     a_peg_side,
      a_peg_offset
    );
  };

  //=========================================================================//
  // "NewStreamingQuoteOrder":                                               //
  //=========================================================================//
  /*
  template<FIX::DialectT::type D>
  inline AOS* AOS* NewStreamingQuoteOrder
    (
      // The originating Stratergy:
      Strategy*           a_strategy,

      // Main order params:
      OrderID             a_quote_id,
      SecDefD const&      a_instr,
      bool                a_is_buy,
      QtyAny              a_qty         = QtyAny::PosInf(),

      // Temporal params of the triggering MktData event:
      utxx::time_val      a_ts_md_exch  = utxx::time_val(),
      utxx::time_val      a_ts_md_conn  = utxx::time_val(),
      utxx::time_val      a_ts_md_strat = utxx::time_val(),

      // Optional order params:
      // NB: TimeInForceT::UNDEFINED must automatically resolve to a Connector-
      // specific default:
      bool                a_batch_send  = false,    // Recommendation Only!
    );
    {
      return
        NewOrder
        (
          a_strategy,     a_instr, FIX::OrderTypeT::Market, a_is_buy,
          PriceT(),       false,           a_qty,
          a_ts_md_exch,   a_ts_md_conn,    a_ts_md_strat,
          a_batch_send,   a_time_in_force, a_expire_date,   a_qty_show,
          a_qty_min,      a_peg_side,      a_peg_offset
        );
    }
  */
  //=========================================================================//
  // "CancelOrder":                                                          //
  //=========================================================================//
  template<FIX::DialectT::type D>
  inline bool   EConnector_FIX<D>::CancelOrder
  (
    AOS const*          a_aos,          // Non-NULL
    utxx::time_val      a_ts_md_exch,
    utxx::time_val      a_ts_md_conn,
    utxx::time_val      a_ts_md_strat,
    bool                a_batch_send
  )
  {
    // Invoke                EConnector_OrdMgmt::CancelOrderGen (),
    // which in turn invokes   FIX::ProtoEngine::CancelOrderImpl():
    //
    return EConnector_OrdMgmt::template CancelOrderGen<QT, QR>
    (
      this,             this,         const_cast<AOS*>(a_aos),
      a_ts_md_exch,     a_ts_md_conn, a_ts_md_strat,   a_batch_send
    );
  }

  //=========================================================================//
  // "ModifyOrder":                                                          //
  //=========================================================================//
  template<FIX::DialectT::type D>
  inline bool   EConnector_FIX<D>::ModifyOrder
  (
    AOS const*          a_aos,       // Non-NULL
    PriceT              a_new_px,
    bool                a_is_aggr,
    QtyAny              a_new_qty,
    utxx::time_val      a_ts_md_exch,
    utxx::time_val      a_ts_md_conn,
    utxx::time_val      a_ts_md_strat,
    bool                a_batch_send,
    QtyAny              a_new_qty_show,
    QtyAny              a_new_qty_min
  )
  {
    // Invoke                EConnector_OrdMgmt::ModifyOrderGen (),
    // which in turn invokes   FIX::ProtoEngine::ModifyOrderImpl():
    //
    return EConnector_OrdMgmt::template ModifyOrderGen<QT, QR>
    (
      this,             this,         const_cast<AOS*>(a_aos),
      a_new_px,         a_is_aggr,    a_new_qty,
      a_ts_md_exch,     a_ts_md_conn, a_ts_md_strat,   a_batch_send,
      a_new_qty_show,   a_new_qty_min
    );
  }

  //=========================================================================//
  // "CancelAllOrders":                                                      //
  //=========================================================================//
  template<FIX::DialectT::type D>
  inline void   EConnector_FIX<D>::CancelAllOrders
  (
    unsigned long       a_strat_hash48,
    SecDefD const*      a_instr,
    FIX::SideT          a_side,
    char const*         a_segm_id
  )
  {
    // Invokes               EConnector_OrdMgmt::CancelAllOrdersGen (),
    // which in turn invokes EConnector_OrdMgmt::EmulateMassCancel  ()
    // or                      FIX::ProtoEngine::CancelAllOrdersImpl():
    //
    EConnector_OrdMgmt::template CancelAllOrdersGen<QT, QR>
      (this, this, a_strat_hash48, a_instr, a_side, a_segm_id);
  }

  //=========================================================================//
  // "FlushOrders":                                                          //
  //=========================================================================//
  template<FIX::DialectT::type D>
  inline utxx::time_val EConnector_FIX<D>::FlushOrders()
  {
    // Invokes                EConnector_OrdMgmt  ::FlushOrdersGen ()
    // which in turn invokes  FIX::ProtoEngine    ::FlushOrdersImpl()
    // which in turn invokes  FIX_ConnectorSessMgr::SendImpl():
    // (if there are actual data to sent out):
    //
    return EConnector_OrdMgmt::FlushOrdersGen(this, this);
  }

  //=========================================================================//
  // "GetSecurityDefinition":                                                //
  //=========================================================================//
  template<FIX::DialectT::type   D>
  inline void EConnector_FIX<D>::GetSecurityDefinition(
    std::function<void(FIX::SecurityDefinition const&)> a_handler,
    const std::string & a_sec_type,
    const std::string & a_exch_id
  )
  {
    // Set up a (one-off) handler for the SecirityDefintion msg:
    m_secDefHandler = a_handler;
    ProtoEngT::SendSecurityDefinitionRequest(this, a_sec_type.c_str(),
      a_exch_id.c_str());
  }

  //=========================================================================//
  // "GetSecurityList":                                                      //
  //=========================================================================//
  template<FIX::DialectT::type   D>
  inline void EConnector_FIX<D>::GetSecurityList
    (std::function<void(FIX::SecurityList const&)> a_handler)
  {
    if constexpr (!FIX::ProtocolFeatures<D>::s_hasSecurityList)
      throw utxx::runtime_error
            ("FIX::EConnector_FIX::GetSecurityList: Not Supported");

    // Set up a (one-off) handler for the SecirityList msg:
    m_secListHandler = a_handler;
    ProtoEngT::SendSecurityListRequest(this);
  }

  //=========================================================================//
  // Processing of Application-Level Order Mgmt Msgs:                        //
  //=========================================================================//
  //=========================================================================//
  // "Process(no_msg)":                                                      //
  //=========================================================================//
  // This method is invoked by the FIX Reader when the end of the currently-
  // available data chunk has been reached, and all complete msgs contained
  // therein have been processed:
  // This event is currently used only by the MktData  Connector: it triggers
  // invocation of Strategy Call-Backs on the updated OrderBooks: for effici-
  // ency, we first install the maximum (readily-available) OrderBook update,
  // and then invoke the Call-Backs once on that whole update:
  //
  template<FIX::DialectT::type   D>
  inline void EConnector_FIX<D>::Process
    (FIX::SessData const* DEBUG_ONLY(a_sess))
  {
    assert(a_sess == this);
    if (!SessMgr::IsMktData())
      return;

    // OK, MDC Processing:
    constexpr bool IsMultiCast     = false;
    constexpr bool WithIncrUpdates =
      FIX::ProtocolFeatures<D>::s_hasIncrUpdates;
    constexpr bool WithOrdersLog   =
      FIX::ProtocolFeatures<D>::s_useOrdersLog;
    constexpr bool WithRelaxedOBs  =
      FIX::ProtocolFeatures<D>::s_useRelaxedOrderBook;

    // Invoke "ProcessEndOfDataChunk" on the upstream:
    EConnector_MktData::ProcessEndOfDataChunk
      <IsMultiCast, WithIncrUpdates, WithOrdersLog, WithRelaxedOBs, QT, QR>
      ();
  }

  //=========================================================================//
  // "Process(BusinessMessageReject)":                                       //
  //=========================================================================//
  // This is something in between of "ExecReport-Reject", "CancelReject" and a
  // Session-level "Reject" (???):
  //
  template<FIX::DialectT::type   D>
  inline void EConnector_FIX<D>::Process
  (
    FIX::BusinessMessageReject const& a_tmsg,
    FIX::SessData              const* DEBUG_ONLY(a_sess),
    utxx::time_val                    a_ts_recv,
    utxx::time_val                    UNUSED_PARAM(a_ts_handl)
  )
  {
    // Must be an OMC:
    assert(a_sess == this   &&   SessMgr::IsOrdMgmt()         &&
           a_tmsg.m_MsgType == FIX::MsgT::BusinessMessageReject);
    CHECK_ONLY
    (
      if (utxx::unlikely(!SessMgr::CheckRxSN(a_tmsg, a_ts_recv)))
        return;
    )
    // Try to get a hint from the Protocol Level about whether the Order is now
    // Non-Existent:
    int rc = int(a_tmsg.m_BusinessRejectReason);
    FuzzyBool nonExistent =
      FIX::ProtocolFeatures<D>::OrderCeasedToExist(rc, a_tmsg.m_Text);

    // Invoke the State Manager:
    (void)  EConnector_OrdMgmt::template OrderRejected<QT,QR>
    (
      this,                 // ProtoEng
      this,                 // SessMgr
      a_tmsg.m_RefSeqNum,
      a_tmsg.m_BusinessRejectRefID,
      0,
      nonExistent,
      rc,
      a_tmsg.m_Text,
      a_tmsg.m_SendingTime, // Because there is no TransactTime in this case
      a_ts_recv
    );
  }

  //=========================================================================//
  // "Process(ExecutionReport)":                                             //
  //=========================================================================//
  // Here we de-multiplex various Order coinditions and invoke the corresp me-
  // thods from a parent class ("EConnector_OrdMgmt"):
  //
  template<FIX::DialectT::type D>
  inline void   EConnector_FIX<D>::Process
  (
    FIX::ExecutionReport<QtyN> const& a_tmsg,
    FIX::SessData              const* DEBUG_ONLY(a_sess),
    utxx::time_val                    a_ts_recv,
    utxx::time_val                    UNUSED_PARAM(a_ts_handl)
  )
  {
    //-----------------------------------------------------------------------//
    // Before All:                                                           //
    //-----------------------------------------------------------------------//
    // Must be an OMC:
    assert(a_sess == this   &&   SessMgr::IsOrdMgmt()    &&
           a_tmsg.m_MsgType == FIX::MsgT::ExecutionReport);
    CHECK_ONLY
    (
      if (utxx::unlikely(!SessMgr::CheckRxSN(a_tmsg, a_ts_recv)))
        return;
    )
    // NB: The Exchange normally assigns an ExchID to this ExecReport, but can
    // also assign an MDEntryID (eg MICEX), and if it was a Trade, also an Ex-
    // ecID (see below):
    ExchOrdID exchID   (a_tmsg.m_OrderID);
    ExchOrdID mdEntryID(a_tmsg.m_MDEntryID);

    // NB: FIX.4.2 uses "OrdStatus" only, whereas FIX.4.4+ uses "ExecType" pri-
    // marily, but also "OrdStatus". The following conditions provide an abstr-
    // action over protocol versions:
    // Also, if "ForceUseExecType" feature is set, it means that we are forcing
    // FIX.4.4-type behaviour although it may notionally be FIX.4.2, so:
    //
    constexpr bool is42 =
      (FIX::ProtocolFeatures<D>::s_fixVersionNum <= FIX::ApplVerIDT::FIX42) &&
      !FIX::ProtocolFeatures<D>::s_forceUseExecType;

    //-----------------------------------------------------------------------//
    // Traded?                                                               //
    //-----------------------------------------------------------------------//
    // XXX: A very peculiar FIX protocol feature: we may get ExecType = New or
    // Replaced, and at the same time, IN THE SAME FIX MSG, OrdStatus = Filled
    // or PartiallyFilled (or very seldom, Calculated). What happens then:
    // (*) If OrdStatus = Filled or Calculated, it certainly indicates a  Fill
    //     immediately upon New or Replacement, because it is not possible that
    //     the New or Replaced ExecType (event) is generated  for a previously-
    //     Filled order!
    // (*) If ExecType = New and OrdStatus = PartFilled, this is indeed a Part-
    //     Fill immediately upon New, because if that PartFill had already hap-
    //     pened in the past, the ExecType (event) would not be New.
    // (*) But if ExecType = Replaced and OrdStatus = PartFilled, it is unclear
    //     wehther:
    //     (a) a previously-Part-Filled order was just Replaced, or
    //     (b) an order was Replaced and immediately Part-Filled now!
    //     To disambiguate those cases, we look into LastPx and LastQty flds as
    //     well (which must both be valid in case (b) but not in (a)):
    // XXX: So what we will do,  is to use LastQty and LastPx as the PRIMARY in-
    //     dicators of a Trade, and only check ExecType and OrdStatus for  cons-
    //     istency with them:
    if (IsPos(a_tmsg.m_LastQty) && IsFinite(a_tmsg.m_LastPx))
    {
      CHECK_ONLY
      (
        if (EConnector::m_debugLevel >= 2)
        {
          bool tradedStatus   =
            a_tmsg.m_OrdStatus == FIX::OrdStatusT::Filled                ||
            a_tmsg.m_OrdStatus == FIX::OrdStatusT::Calculated            ||
            a_tmsg.m_OrdStatus == FIX::OrdStatusT::PartiallyFilled;

          bool tradedExecType =
            (!is42 && (a_tmsg.m_ExecType == FIX::ExecTypeT::Trade         ||
                       a_tmsg.m_ExecType == FIX::ExecTypeT::Calculated))  ||
            ( is42 && (a_tmsg.m_ExecType == FIX::ExecTypeT::Fill          ||
                       a_tmsg.m_ExecType == FIX::ExecTypeT::PartialFill));

          // In all "peculiar" cases such as those indicated above, produce a
          // warning:
          if (utxx::unlikely(!(tradedStatus && tradedExecType)))
            LOGA_WARN(EConnector, 2,
              "EConnector_FIX::Process(ExecutionReport): ReqID={}, ExchID="
              "{}: Inconsistency: Traded with LastQty={}, LastPx={}, but "
              "ExecType={}, OrdStatus={}",
              a_tmsg.m_ClOrdID,        exchID.ToString(),
              QR(a_tmsg.m_LastQty),    double(a_tmsg.m_LastPx),
              char(a_tmsg.m_ExecType), char(a_tmsg.m_OrdStatus))
        }
      )
      // XXX: In the past, we were trying to extract Exchange and Broker fee
      // data from the FIX msg in this case. However, such data are in  most
      // cases absent or incomplete, so we will skip it altogether and dele-
      // gate the fees computation to the Strategy level.

      // ExecID assigned by the Exchange to this Trade -- this is NOT the same
      // as "exchID" assigned when the Order was received by the Exchange:
      //
      ExchOrdID execID(a_tmsg.m_ExecID);

      // NB: To distinguish between Fill and PartFill, we use "LeavesQty" (0 or
      // > 0, resp). We TRUST the server integrity in this respect, and thus al-
      // ways have a definite value here:
      FuzzyBool isCompleteFill =
        (a_tmsg.m_OrdStatus == FIX::OrdStatusT::Filled)
        ? FuzzyBool::True
        : FuzzyBool::False;

      // For FIX.4.2, do a further consistency check (if fails, it only produces
      // a warning).  NB:  All other checks wrt LeavesQty etc,  are done  by the
      // Parent Class:
      CHECK_ONLY
      (
        if (utxx::unlikely
           (is42 &&
           ((isCompleteFill    == FuzzyBool::True              &&
             a_tmsg.m_ExecType == FIX::ExecTypeT::PartialFill) ||
            (isCompleteFill    == FuzzyBool::False             &&
             a_tmsg.m_ExecType == FIX::ExecTypeT::Fill))       ))
          LOGA_WARN(EConnector, 2,
            "EConnector_FIX::Process(ExecutionReport): ReqID={}, ExchID={}"
            ": FIX.4.2 Inconsistency: OrdStatus={}, ExecType={}",
            a_tmsg.m_ClOrdID,         exchID.ToString(),
            char(a_tmsg.m_OrdStatus), char(a_tmsg.m_ExecType))
      )
      // Now invoke the State Manager:
      // NB: OurSide is given by "a_tmsg"; but who was the Aggressor, is not
      // specified there:
      // FIXME: Get the Fee Info from the FIX Msg. For now, it is Invalid:
      //
      (void) EConnector_OrdMgmt::template OrderTraded<QT, QR, QT, QR>
      (
        this,            this,                        // ProtoEng, SessMgr
        // No MDC:                                    No Req12 yet:
        nullptr,         a_tmsg.m_ClOrdID,   0, nullptr,
        a_tmsg.m_Side,   FIX::SideT::UNDEFINED, exchID,
        mdEntryID,       execID,                a_tmsg.m_Price,
        a_tmsg.m_LastPx, a_tmsg.m_LastQty,      a_tmsg.m_LeavesQty,
        a_tmsg.m_CumQty, QtyN::Invalid(),       a_tmsg.m_SettlDate,
        isCompleteFill,  a_tmsg.m_TransactTime, a_ts_recv
      );
    }
    else
    //-----------------------------------------------------------------------//
    // Order Replaced:                                                       //
    //-----------------------------------------------------------------------//
    // This is probably the most frequent condition, so consider it first:
    //
    if ((!is42  && (a_tmsg.m_ExecType  == FIX::ExecTypeT ::Replaced)) ||
        ( is42  && (a_tmsg.m_OrdStatus == FIX::OrdStatusT::Replaced)))
      //
      // NB: If at the Replace req arrival, the order was already PartFilled,
      // then the following behaviours are possible:
      // (*) Replace is rejected  but the original Order stays, so we would
      //     get a "CancelReject";
      // (*) Replace is rejected  and the original Order is cancelled, so we
      //     would get a "CancelReject" and an "ExecReport-Cancel";
      // (*) Replace is notionally "performed" but the unfilled rest of the
      //     Order is cancelled;  this is almost the same as above, but  we
      //     would only get an "ExecReport-Cancel":
      // But in this case, we got none of those msgs, so:
      //
      (void) EConnector_OrdMgmt::template OrderReplaced<QT, QR>
      (
        this,             // ProtoEng
        this,             // SessMgr
        a_tmsg.m_ClOrdID,
        a_tmsg.m_OrigClOrdID,
        0,
        exchID,
        ExchOrdID(0),     // No OrigExchID in FIX!
        mdEntryID,
        a_tmsg.m_Price,   // NB: Use "Price" here, NOT "LastPx"!
        a_tmsg.m_LeavesQty,
        a_tmsg.m_TransactTime,
        a_ts_recv
      );
    else
    //-----------------------------------------------------------------------//
    // A "Pending" status to be Acknowledged?                                //
    //-----------------------------------------------------------------------//
    if ((!is42 && (a_tmsg.m_ExecType  == FIX::ExecTypeT ::PendingNew      ||
                   a_tmsg.m_ExecType  == FIX::ExecTypeT ::PendingReplace  ||
                   a_tmsg.m_ExecType  == FIX::ExecTypeT ::PendingCancel)) ||
        ( is42 && (a_tmsg.m_OrdStatus == FIX::OrdStatusT::PendingCancel   ||
                   a_tmsg.m_OrdStatus == FIX::OrdStatusT::PendingReplace  ||
                   a_tmsg.m_OrdStatus == FIX::OrdStatusT::PendingNew)))
      //
      // XXX: Compared to the "Order Replaced" case above, here  it is very un-
      // likely that the Order could already be Traded, so don't check for that.
      // Invoke the State Manager; again, use "Price" here, NOT "LastPx":
      //
      EConnector_OrdMgmt::OrderAcknowledged
        (a_tmsg.m_ClOrdID, 0, a_tmsg.m_Price, a_tmsg.m_LeavesQty);
    else
    //-----------------------------------------------------------------------//
    // A "New" status to be Confirmed?                                       //
    //-----------------------------------------------------------------------//
    // XXX: We currently treat "AcceptedForBidding" same way as "New":
    //
    if ((!is42 && (a_tmsg.m_ExecType  == FIX::ExecTypeT ::New))           ||
        ( is42 && (a_tmsg.m_OrdStatus == FIX::OrdStatusT::New             ||
                   a_tmsg.m_OrdStatus == FIX::OrdStatusT::AcceptedForBidding)
       ))
      // Invoke the State Manager:
      (void) EConnector_OrdMgmt::template OrderConfirmedNew<QT, QR>
      (
        this,               // ProtoEng
        this,               // SessMgr
        a_tmsg.m_ClOrdID,
        0,
        exchID,
        mdEntryID,
        a_tmsg.m_Price,     // Again, use "Price": "LastPx" is for Trades only!
        a_tmsg.m_LeavesQty,
        a_tmsg.m_TransactTime,
        a_ts_recv
      );
    else
    //-----------------------------------------------------------------------//
    // Order Cancelled:                                                      //
    //-----------------------------------------------------------------------//
    // This also includes similar conditions eg "DoneForDay", "Expired":
    //
    if ((!is42 && a_tmsg.m_ExecType  == FIX::ExecTypeT ::Cancelled) ||
        ( is42 && a_tmsg.m_OrdStatus == FIX::OrdStatusT::Cancelled))
      //
      // Invoke the State Manager.  But note that if the order was cancelled not
      // by an explicit client req, but rather via MassCancel, Cancel-on-Discon-
      // nect or by similar means, than one of the OrdIDs may be unavailable; eg
      // "ClOrdID" may then mean the cancellation subject, and "OrigClOrdID" wo-
      // uld be 0.
      // Thus below, the following situations are possible:
      // (*) either both IDs are 0, in which case "OrderCancelled" will fail;
      // (*) or exactly 1 ID is non-0, in which case that non-0 ID will be
      //     passed in both args to "OrderCancelled";
      // (*) or both IDs are non-0, in which case they will be passed to "Order-
      //     Cancelled" according to their standard semantics:
      //
      (void) EConnector_OrdMgmt::template OrderCancelled<QT, QR>
      (
        this,           // ProtoEng
        this,           // SessMgr
        // Cancellation Req:
        utxx::likely(a_tmsg.m_ClOrdID     != 0)
          ? a_tmsg.m_ClOrdID     : a_tmsg.m_OrigClOrdID,
        // Cancellation Subject:
        utxx::likely(a_tmsg.m_OrigClOrdID != 0)
          ? a_tmsg.m_OrigClOrdID : a_tmsg.m_ClOrdID,
        0,
        exchID,
        mdEntryID,
        a_tmsg.m_Price, // Again, use "Price", NOT "LastPx" here!
        a_tmsg.m_LeavesQty,
        a_tmsg.m_TransactTime,
        a_ts_recv
      );
    else
    //-----------------------------------------------------------------------//
    // Order Rejected:                                                       //
    //-----------------------------------------------------------------------//
    // Also includes "Stopped" or "Restated" conditions -- but it does NOT
    // include Cancellation or Replacement failure    (which is given by a
    // completely separate "CancelReject" FIX msg).
    // This is similar to "Order Cancelled", but includes additional error
    // msgs (as will result in a different Strategy Call-Back invocation):
    //
    if ((!is42 && (a_tmsg.m_ExecType  == FIX::ExecTypeT ::Rejected        ||
                   a_tmsg.m_ExecType  == FIX::ExecTypeT ::Expired         ||
                   a_tmsg.m_ExecType  == FIX::ExecTypeT ::DoneForDay      ||
                   a_tmsg.m_ExecType  == FIX::ExecTypeT ::Restated))      ||
        ( is42 && (a_tmsg.m_OrdStatus == FIX::OrdStatusT::Rejected        ||
                   a_tmsg.m_OrdStatus == FIX::OrdStatusT::Stopped         ||
                   a_tmsg.m_OrdStatus == FIX::OrdStatusT::Expired         ||
                   a_tmsg.m_OrdStatus == FIX::OrdStatusT::DoneForDay)))
      // Invoke the State Manager:
      (void) EConnector_OrdMgmt::template OrderRejected<QT, QR>
      (
        this,                    // ProtoEng
        this,                    // SessMgr,
        0,                       // No RefSeqNum here
        a_tmsg.m_ClOrdID,
        0,                       // No AOSID
        FuzzyBool::True,         // We KNOW it does not exist anymore!
        int(a_tmsg.m_OrdRejReason),
        a_tmsg.m_Text,
        a_tmsg.m_TransactTime,
        a_ts_recv
      );
    else
    //-----------------------------------------------------------------------//
    // Any other cases: Currently not supported:                             //
    //-----------------------------------------------------------------------//
      LOGA_WARN(EConnector, 2,
        "EConnector_FIX::Process(ExecutionReport): UnSupported/Ignored: "
        "Is42={}, ExecType={}, OrdStatus={}",
        is42, char(a_tmsg.m_ExecType), char(a_tmsg.m_OrdType))
  }

  //=========================================================================//
  // "Process(OrderCancelReject)":                                           //
  //=========================================================================//
  template<FIX::DialectT::type   D>
  inline void EConnector_FIX<D>::Process
  (
    FIX::OrderCancelReject const& a_tmsg,
    FIX::SessData          const* DEBUG_ONLY(a_sess),
    utxx::time_val                a_ts_recv,
    utxx::time_val                UNUSED_PARAM(a_ts_handl)
  )
  {
    // Must be an OMC:
    assert(a_sess == this   &&   SessMgr::IsOrdMgmt()      &&
           a_tmsg.m_MsgType == FIX::MsgT::OrderCancelReject);
    CHECK_ONLY
    (
      if (utxx::unlikely(!SessMgr::CheckRxSN(a_tmsg, a_ts_recv)))
        return;
    )
    ExchOrdID exchID(a_tmsg.m_OrderID);

    // NB: Be very careful here. The State Manager will need to figure out whe-
    // ther this implies that the whole order does not exist anymore, eg:
    // (*) the Req was "Cancel" and the order does not exist because it was al-
    //     ready filled;
    // (*) the Req was "Cancel" and it failed because it was targeting a previ-
    //     ously-failed "Modify" ReqID, but the whole order still exists;
    // (*) the Req was "Modify" and it failed because the order was already
    //     filled, so it does not exist anymore;
    // (*) the Req was "Modify" and it failed because the order was part-filled;
    //     in this case, the order may or may not be automatically cancelled,
    //     and in the latter case, it may laready have happened, or would happen
    //     subsequently;
    // (*) the Req was "Modify", and it failed because of eg credit control is-
    //     sues, whereas the whole order still exists;
    // (*) the Req was "Cancel" or "Modify" and it failed because of a protocol
    //     error on our side, eg using completely wrong subject ReqID (maybe
    //     of itself), whereas the whole order continues to exist:
    //
    // So we need some indications/hints from the FIX Protocol  which will help
    // the StateManager to decide whether the order still exists:
    //
    int rc = int(a_tmsg.m_CxlRejReason);

    // XXX: Generically, the only reliable flag we currently use is "TooLateTo-
    // Cancel" -- if set, the order certainly does not exist anymore (most lik-
    // ely, Filled):
    FuzzyBool nonExistent =
      (a_tmsg.m_CxlRejResponseTo == FIX::CxlRejRespT::Cancel          &&
       a_tmsg.m_CxlRejReason     == FIX::CxlRejReasonT::TooLateToCancel)
      ? FuzzyBool::True
      : // But there may also be DialectT-specific indicators which will help us
        // to determine whether the Order still exists:
        FIX::ProtocolFeatures<D>::OrderCeasedToExist(rc, a_tmsg.m_Text);

    // For the following flags, use Protocol-based indicators:
    FuzzyBool filled =
      FIX::ProtocolFeatures<D>::FailedOrderWasFilled(rc, a_tmsg.m_Text);

    // Invoke the State Manager:
    // NB: This "a_tmsg" does not contain "TransactTime", so will use "Sending-
    // Time" instead:
    (void) EConnector_OrdMgmt::template OrderCancelOrReplaceRejected<QT, QR>
    (
      this,               // ProtoEng
      this,               // SessMgr
      a_tmsg.m_ClOrdID,
      a_tmsg.m_OrigClOrdID,
      0,
      &exchID,
      filled,
      nonExistent,
      rc,
      a_tmsg.m_Text,
      a_tmsg.m_SendingTime,
      a_ts_recv
    );
  }

  //=========================================================================//
  // Dummy OMC Methods (Server-Side or RFS/RFQ):                             //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "Process(NewOrderSingle)":                                              //
  //-------------------------------------------------------------------------//
  // NB: "NewOrderSingle" is typically processed by Server-Side FIX Engines, but
  // it may be used on the Client (Connector) Side as well, eg if Streaming Quo-
  // tes are published (and then the counter-parties initiate NewOrders). Here,
  // however, this method is empty: TODO!
  //
  template<FIX::DialectT::type   D>
  inline void EConnector_FIX<D>::Process
  (
    FIX::NewOrderSingle<QtyN> const& CHECK_ONLY(a_tmsg),
    FIX::SessData             const* DEBUG_ONLY(a_sess),
    utxx::time_val                   CHECK_ONLY(a_ts_recv),
    utxx::time_val                   UNUSED_PARAM(a_ts_handl)
  )
  {
    assert(a_sess == this   &&   SessMgr::IsOrdMgmt()   &&
           a_tmsg.m_MsgType == FIX::MsgT::NewOrderSingle);

    CHECK_ONLY((void) SessMgr::CheckRxSN(a_tmsg, a_ts_recv);)
  }

  //-------------------------------------------------------------------------//
  // "Process(OrderCancelRequest)":                                          //
  //-------------------------------------------------------------------------//
  // This method is for Server-Side only, here is a stub for static interface
  // completeness:  TODO!
  //
  template<FIX::DialectT::type   D>
  inline void EConnector_FIX<D>::Process
  (
    FIX::OrderCancelRequest const& CHECK_ONLY(a_tmsg),
    FIX::SessData           const* DEBUG_ONLY(a_sess),
    utxx::time_val                 CHECK_ONLY(a_ts_recv),
    utxx::time_val                 UNUSED_PARAM(a_ts_handl)
  )
  {
    assert(a_sess == this   &&   SessMgr::IsOrdMgmt()       &&
           a_tmsg.m_MsgType == FIX::MsgT::OrderCancelRequest);

    CHECK_ONLY((void) SessMgr::CheckRxSN(a_tmsg, a_ts_recv);)
  }

  //-------------------------------------------------------------------------//
  // "Process(OrderCancelReplaceRequest)":                                   //
  //-------------------------------------------------------------------------//
  // Again, this method is for Server-Side only, here is a stub for static in-
  // terface completeness: TODO!
  //
  template<FIX::DialectT::type   D>
  inline void EConnector_FIX<D>::Process
  (
    FIX::OrderCancelReplaceRequest<QtyN> const& CHECK_ONLY(a_tmsg),
    FIX::SessData                        const* DEBUG_ONLY(a_sess),
    utxx::time_val                              CHECK_ONLY(a_ts_recv),
    utxx::time_val                              UNUSED_PARAM(a_ts_handl)
  )
  {
    assert(a_sess == this   &&   SessMgr::IsOrdMgmt()             &&
           a_tmsg.m_MsgType == FIX::MsgT::OrderCancelReplaceRequest);

    CHECK_ONLY((void) SessMgr::CheckRxSN(a_tmsg, a_ts_recv);)
  }

  //=========================================================================//
  // Protocol-Level MktData Subscription Mgmt:                               //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "SubscribeStreamingQuote":                                              //
  //-------------------------------------------------------------------------//
  // FIXME: Consider unifying this with "SendMktDataSubscrReqs":
  //
  template<FIX::DialectT::type D>
  template <QtyTypeT QtyType>
  inline void EConnector_FIX<D>::SubscribeStreamingQuote
  (
    Strategy*                 a_strat,
    SecDefD const&            a_instr,
    bool                      a_reg_instr_risks,     // Interact with RiskMgr?
    Qty<QtyType, QR>          a_quantity             // optional
  )
  {
    if constexpr (!FIX::ProtocolFeatures<D>::s_hasStreamingQuotes)
      throw utxx::runtime_error
            ("FIX::EConnector_FIX::SubscribeStreamingQuote: Not Supported");

    if constexpr (D == FIX::DialectT::Cumberland) {
      if (utxx::unlikely(a_quantity <= 0))
        throw utxx::runtime_error
        ("FIX::EConnector_FIX::SubscribeStreamingQuote: Must provide "
        "quantity for Cumberland");
    }

    // NB: Subscribing to trades not implemented yet:
    assert(EConnector_MktData::m_orderBooks          != nullptr  &&
           EConnector_MktData::m_obSubscrIDs         != nullptr  &&
           EConnector_MktData::m_obSubscrIDs->size() ==
           EConnector_MktData::m_orderBooks->size());

    auto ob = EConnector_MktData::FindOrderBook(a_instr.m_SecID);
    assert(ob != nullptr);

    int ob_id = int(ob - EConnector_MktData::m_orderBooks->data());
    assert(ob_id >= 0  &&
           ob_id < int  (EConnector_MktData::m_orderBooks->size()));

    OrderID ores = ProtoEngT::SendQuoteRequest(this, a_instr, a_quantity);

    // Memoise the ReqID (the QuoteID is empty yet):
    assert(ores != 0);
    EConnector_MktData::m_obSubscrIDs->data()[ob_id] = ores;
    ZeroOut(m_quoteIDs->data() + ob_id);

    LOGA_INFO(EConnector, 2,
      "EConnector_FIX::SubscribeStreamingQuote: Symbol={}: Subscribed "
      "(ReqID={})", a_instr.m_Symbol.data(), ores)

    // Streaming Quotes only provide level 1 price and quantity updates
    EConnector_MktData::SubscribeMktData
      (a_strat, a_instr, OrderBook::UpdateEffectT::L1Px, a_reg_instr_risks);
  }

  //-------------------------------------------------------------------------//
  // "StreamingQuoteCancel":                                                 //
  //-------------------------------------------------------------------------//
  // FIXME: Consider unifying this with "SendMktDataSubscrReqs":
  //
  template<FIX::DialectT::type D>
  inline void EConnector_FIX<D>::CancelStreamingQuote
  (
    SecDefD const&            a_instr
  )
  {
    if constexpr (!FIX::ProtocolFeatures<D>::s_hasStreamingQuotes)
      throw utxx::runtime_error
            ("FIX::EConnector_FIX::CancelStreamingQuote: Not Supported");

    assert(EConnector_MktData::m_orderBooks          != nullptr  &&
           EConnector_MktData::m_obSubscrIDs         != nullptr  &&
           EConnector_MktData::m_obSubscrIDs->size() ==
           EConnector_MktData::m_orderBooks->size());

    // Find the quote ID:
    auto ob = EConnector_MktData::FindOrderBook(a_instr.m_SecID);
    assert(ob != nullptr);

    int ob_id = int(ob - EConnector_MktData::m_orderBooks->data());
    assert(ob_id >= 0 &&
           ob_id < int  (EConnector_MktData::m_orderBooks->size()));

    ObjName& quote_id =  m_quoteIDs->data()[ob_id]; // Ref!

    if (utxx::unlikely(IsEmpty(quote_id)))
      throw utxx::runtime_error
          ("FIX::EConnector_FIX::CancelStreamingQuote: No active streaming "
          "quote for Symbol={}", a_instr.m_Symbol.data());

    // If OK: Send the Cancel for the "quote_id":
    ProtoEngT::SendQuoteCancel
      (this, quote_id.data(), ob->GetLastUpdateSeqNum());

    // Reset subscription ID and quote ID:
    EConnector_MktData::m_obSubscrIDs->data()[ob_id] = 0;
    ZeroOut(&quote_id);

    LOGA_INFO(EConnector, 2,
      "EConnector_FIX::CancelStreamingQuote: Canceling streaming quote for "
      "Symbol={}", a_instr.m_Symbol.data())
  }

  //-------------------------------------------------------------------------//
  // "SendMktDataSubscrReqs":                                                //
  //-------------------------------------------------------------------------//
  template<FIX::DialectT::type D>
  template<bool IsSubscr>
  inline void   EConnector_FIX<D>::SendMktDataSubscrReqs()
  {
    if constexpr (FIX::ProtocolFeatures<D>::s_hasStreamingQuotes)
      throw utxx::runtime_error
            ("FIX::EConnector_FIX::SendMktDataSubscrReqs: Not Supported");

    //-----------------------------------------------------------------------//
    // Setup:                                                                //
    //-----------------------------------------------------------------------//
    assert(EConnector_MktData::m_orderBooks           != nullptr &&
           EConnector_MktData::m_obSubscrIDs          != nullptr &&
           EConnector_MktData::m_obSubscrIDs->size()  ==
           EConnector_MktData::m_orderBooks->size()              &&
           SessMgr::IsMktData());

    int L = int(EConnector_MktData::m_orderBooks->size());
    OrderBook const* obs   = EConnector_MktData::m_orderBooks ->data();
    OrderID*         obIDs = EConnector_MktData::m_obSubscrIDs->data();

    // NB: If some Dialects, we have to send separate SubscrReqs for OrderBooks
    // and Trades:
    bool sepTrades = FIX::ProtocolFeatures<D>::s_hasSepMktDataTrdSubscrs &&
                     EConnector_MktData::m_withExplTrades;

    // Obviously, if "sepTrades" is set, TrdSubscrIDs must be valid:
    assert(!sepTrades                                             ||
          (EConnector_MktData::m_trdSubscrIDs         != nullptr  &&
           EConnector_MktData::m_trdSubscrIDs->size() == size_t(L)));

    OrderID* trdIDs =
      sepTrades ? EConnector_MktData::m_trdSubscrIDs->data() : nullptr;

    //-----------------------------------------------------------------------//
    // For all OrderBooks, perform Subscribe or UnSubscribe ops:             //
    //-----------------------------------------------------------------------//
    for (int i = 0; i < L; ++i)
    {
      OrderBook const& ob    = obs[i];
      SecDefD   const& instr = ob.GetInstr();

      // NB:
      // (*) For "Subscribe"   ops, the orig ReqID is not known, hence 0; it is
      //     then saved in the corresp vector;
      // (*) For "UnSubscribe" ops, the orig ReqID is taken from the corresp
      //     vector, the result is ignored:
      //
      // OrderBook Updates ID is always used:
      //
      OrderID  oid  = IsSubscr ? 0 : obIDs[i];

      if (sepTrades)
      {
        // Then Trades only (XXX: here MktDepth=1):
        OrderID tid  = IsSubscr ? 0 : trdIDs[i];

        if constexpr (IsSubscr)
        {
          // OrderBook Updates only:
          OrderID ores =
            ProtoEngT::template SendMarketDataRequest<IsSubscr>
              (this, instr, oid, EConnector_MktData::m_mktDepth, true, false);

          OrderID tres =
            ProtoEngT::template SendMarketDataRequest<IsSubscr>
              (this, instr, tid, 1, false, true);

          // Memoise the ReqIDs:
          assert(ores != 0 && tres != 0);
          obIDs [i] = ores;
          trdIDs[i] = tres;

          LOGA_INFO(EConnector, 2,
            "EConnector_FIX::SendMktDataSubscrReqs: Symbol={}: Subscribed "
            "OBUpdates (ReqID={}) and Trades (ReqID={})",
            instr.m_Symbol.data(), ores, tres)
        }
        else
        {
          LOGA_INFO(EConnector, 2,
            "EConnector_FIX::SendMktDataSubscrReqs: Symbol={}: UnSubscrib"
            "ed OBUpdates (OldReqID={}) and Trades (OldReqID={})",
            instr.m_Symbol.data(), oid, tid)
        }
      }
      else
      {
        // OrderBook Updates and Trades (if requested) together; trdIDs[i] is
        // untouched, and may not even exist:
        //
        if constexpr (IsSubscr)
        {
          OrderID ores =
            ProtoEngT::template SendMarketDataRequest<IsSubscr>
              (this, instr, 0, EConnector_MktData::m_mktDepth, true,
               EConnector_MktData::m_withExplTrades);

          // Memoise the ReqID:
          assert(ores != 0);
          obIDs[i] = ores;

          LOGA_INFO(EConnector, 3,
            "EConnector_FIX::SendMktDataSubscrReqs: Symbol={}: Subscribed "
            "OBUpdates{} (ReqID={})", instr.m_Symbol.data(),
            (EConnector_MktData::m_withExplTrades ? " with Trades" : ""), ores)
        }
        else
        {
          LOGA_INFO(EConnector, 3,
            "EConnector_FIX::SendMktDataSubscrReqs: Symbol={}: UnSubscrib"
            "ed OBUpdates{} (OldReqID={})", instr.m_Symbol.data(),
            (EConnector_MktData::m_withExplTrades ? " with Trades" : ""), oid)
        }
      }
    }
    // All Done!
  }

  //=========================================================================//
  // Processing of Application-Level MktData Msgs:                           //
  //=========================================================================//
  //=========================================================================//
  // "Process(MarketDataRequestReject)":                                     //
  //=========================================================================//
  template<FIX::DialectT::type D>
  inline void   EConnector_FIX<D>::Process
  (
    FIX::MarketDataRequestReject const& a_tmsg,
    FIX::SessData                const* DEBUG_ONLY  (a_sess),
    utxx::time_val                      CHECK_ONLY  (a_ts_recv),
    utxx::time_val                      UNUSED_PARAM(a_ts_handl)
  )
  {
    // XXX: At the moment, we treat this as a "No-No Event": Will stop the Con-
    // nector. Must be an MDC Mode of course:
    assert(a_sess == this   &&   SessMgr::IsMktData()            &&
           a_tmsg.m_MsgType == FIX::MsgT::MarketDataRequestReject);
    CHECK_ONLY
    (
      if (utxx::unlikely(!SessMgr::CheckRxSN(a_tmsg, a_ts_recv)))
        return;
    )
    // XXX: We currently do not memoise the MktData subscr reqs (under normal
    // circumstances, they are supposed to be transient),  so we can't tell
    // straight away which Symbol has failed -- (yet this can be figured out
    // post-mortem via logs):
    LOGA_ERROR(EConnector, 1,
      "EConnector_FIX::Process(MarketDataRequestReject): ReqID={} Failed, "
      "ErrCode={}: {}: Terminating the Connector...",
      a_tmsg.m_MDReqID, char(a_tmsg.m_MDReqRejReason), a_tmsg.m_Text)

    // Most likely, something is generically wrong with the instrument  being
    // subscribed,  so we stop gracefully,  but do NOT attempt re-connection:
    Stop();
  }

  //=========================================================================//
  // "Process(MarketDataSnapShot)":                                          //
  //=========================================================================//
  // This is effectively a wrapper around "UpdateOrderBooks" (which may also
  // process Trades!), so it is not TimeStamped on its own:
  //
  template<FIX::DialectT::type D>
  inline void   EConnector_FIX<D>::Process
  (
    FIX::MarketDataSnapShot<D, QtyN> const& a_tmsg,
    FIX::SessData                    const* DEBUG_ONLY(a_sess),
    utxx::time_val                          a_ts_recv,
    utxx::time_val                          a_ts_handl
  )
  {
    assert(a_sess == this   &&   SessMgr::IsMktData()       &&
           a_tmsg.m_MsgType == FIX::MsgT::MarketDataSnapShot);
    CHECK_ONLY
    (
      if (utxx::unlikely(!SessMgr::CheckRxSN(a_tmsg, a_ts_recv)))
        return;
    )
    // XXX: In FIX, we assume that there is no DynInitMode: Even if we have both
    // SnapShots and IncrUpdates, and currently are processing a SnapShot, the
    // latter should result in a valid OrderBook (because there is no limit on
    // the number of MDEntries in TCP, and there are no concurrent IncrUpdates).
    // BEWARE: "MsgSeqNum" below may be Session-specific, so we can only use it
    // safely because all OrderBooks are re-initialised when the FIX Session re-
    // starts:
    bool ok =
      EConnector_MktData::template UpdateOrderBooks      // Incl Trades!
      <
        true,                                            // IsSnapShot  = true
        false,                                           // IsMultiCast = false
        FIX::ProtocolFeatures<D>::s_hasIncrUpdates,      // WithIncrUpdates
        FIX::ProtocolFeatures<D>::s_useOrdersLog,        // WithOrdersLog
        FIX::ProtocolFeatures<D>::s_useRelaxedOrderBook, // WithRelaxedOBs
        FIX::ProtocolFeatures<D>::s_hasChangeIsPartFill, // ChangeIsPartFill
        FIX::ProtocolFeatures<D>::s_hasNewEncdAsChange,  // NewEncdAsChange
        FIX::ProtocolFeatures<D>::s_hasChangeEncdAsNew,  // ChangeEncdAsNew
        false,                                           // IsFullAmt   = false
        IsSparse,
        FIX::ProtocolFeatures<D>::s_findOrderBooksBy,
        QT,
        QR,
        OMC
      >
      (a_tmsg.m_MsgSeqNum, a_tmsg, false,                // DynInitMode = false
       a_ts_recv, a_ts_handl);

    // We assume that a SnapShot failure is not tolerable, unless we have ONLY
    // SnapShots:
    if (utxx::unlikely(FIX::ProtocolFeatures<D>::s_hasIncrUpdates && !ok))
    {
      LOGA_ERROR(EConnector, 1,
        "EConnector_FIX::Process(MarketDataSnapShot): Symbol={}: SnapShot "
        "Failed, Terminating the Connector...",    a_tmsg.m_Symbol.data())
      Stop();
    }
  }

  //=========================================================================//
  // "Process(MarketDataIncrementalRefresh)":                                //
  //=========================================================================//
  // This is effectively a wrapper around "UpdateOrderBooks" (which may also
  // process Trades!), so it is not TimeStamped on its own:
  //
  template<FIX::DialectT::type D>
  inline void   EConnector_FIX<D>::Process
  (
    FIX::MarketDataIncrementalRefresh<D, QtyN> const& a_tmsg,
    FIX::SessData                              const* DEBUG_ONLY(a_sess),
    utxx::time_val                                    a_ts_recv,
    utxx::time_val                                    a_ts_handl
  )
  {
    assert(a_sess == this   &&   SessMgr::IsMktData()                &&
           a_tmsg.m_MsgType == FIX::MsgT::MarketDataIncrementalRefresh);
    CHECK_ONLY
    (
      if (utxx::unlikely(!SessMgr::CheckRxSN(a_tmsg, a_ts_recv)))
        return;
    )
    // XXX: The HasIncrUpdates flag might be off -- in this case,  "MarketData-
    // IncrementalRefresh" might still be received, but it has some special me-
    // aninhg and is not processed as usual:
    if (!FIX::ProtocolFeatures<D>::s_hasIncrUpdates)
    {
      LOGA_WARN(EConnector, 2,
        "EConnector_FIX::MarketDataIncrementalRefresh: UnSupported, Ignored")
      return;
    }
    // NB: The result flag of "UpdateOrderBooks" is ignored -- errors are mana-
    // ged via the "m_updated" list and Client notifications. Again, "MsgSeqNum"
    // is only safe because OrderBooks are re-initialised on  Connector/Session
    // re-start:
    (void) EConnector_MktData::template UpdateOrderBooks
    <                                               // Incl Trades!
      false,                                        // IsSnapShot     = false
      false,                                        // IsMultiCast    = false
      true,                                         // HasIncrUpdates = true
      FIX::ProtocolFeatures<D>::s_useOrdersLog,
      FIX::ProtocolFeatures<D>::s_useRelaxedOrderBook,
      FIX::ProtocolFeatures<D>::s_hasChangeIsPartFill,
      FIX::ProtocolFeatures<D>::s_hasNewEncdAsChange,
      FIX::ProtocolFeatures<D>::s_hasChangeEncdAsNew,
      false,                                        // IsFullAmt      = false
      IsSparse,
      FIX::ProtocolFeatures<D>::s_findOrderBooksBy,
      QT,
      QR,
      OMC
    >
    (a_tmsg.m_MsgSeqNum, a_tmsg, false,             // DynInitMode    = false
     a_ts_recv, a_ts_handl);
  }

  //=========================================================================//
  // "Process(QuoteRequestReject)":                                          //
  //=========================================================================//
  template<FIX::DialectT::type   D>
  inline void EConnector_FIX<D>::Process
  (
    FIX::QuoteRequestReject const& a_tmsg,
    FIX::SessData           const* DEBUG_ONLY(a_sess),
    utxx::time_val                 CHECK_ONLY(a_ts_recv),
    utxx::time_val                 UNUSED_PARAM(a_ts_handl)
  )
  {
    assert(a_sess == this && a_tmsg.m_MsgType == FIX::MsgT::QuoteRequestReject);
    CHECK_ONLY((void) SessMgr::CheckRxSN(a_tmsg, a_ts_recv);)

    // XXX: We currently do not memoise the MktData subscr reqs (under normal
    // circumstances, they are supposed to be transient),  so we can't tell
    // straight away which Symbol has failed -- (yet this can be figured out
    // post-mortem via logs):
    LOGA_ERROR(EConnector, 1,
      "EConnector_FIX::Process(QuoteRequestReject): ReqID={} Failed, "
      "ErrCode={}: {}: Terminating the Connector...",
      a_tmsg.m_QuoteReqID, a_tmsg.m_QuoteReqRejReason, a_tmsg.m_Text)

    // Most likely, something is generically wrong with the instrument  being
    // subscribed,  so we stop gracefully,  but do NOT attempt re-connection
    Stop();
  }

  //=========================================================================//
  // "Process(Quote)":                                                       //
  //=========================================================================//
  template<FIX::DialectT::type   D>
  inline void EConnector_FIX<D>::Process
  (
    FIX::Quote<QtyN>        const& a_tmsg,
    FIX::SessData           const* DEBUG_ONLY(a_sess),
    utxx::time_val                 a_ts_recv,
    utxx::time_val                 a_ts_handl
  )
  {
    assert(a_sess == this && a_tmsg.m_MsgType == FIX::MsgT::Quote);
    CHECK_ONLY((void) SessMgr::CheckRxSN(a_tmsg, a_ts_recv);)

    // we need to get the order book pointer so we can memoize the quote ID
    OrderBook const* ob = nullptr;

    bool ok =
      EConnector_MktData::template UpdateOrderBooks      // Incl Trades!
      <
        true,                                            // IsSnapShot  = true
        false,                                           // IsMultiCast = false
        FIX::ProtocolFeatures<D>::s_hasIncrUpdates,
        FIX::ProtocolFeatures<D>::s_useOrdersLog,
        FIX::ProtocolFeatures<D>::s_useRelaxedOrderBook,
        FIX::ProtocolFeatures<D>::s_hasChangeIsPartFill,
        FIX::ProtocolFeatures<D>::s_hasNewEncdAsChange,
        FIX::ProtocolFeatures<D>::s_hasChangeEncdAsNew,
        false,                                           // IsFullAmt   = false
        IsSparse,
        FIX::ProtocolFeatures<D>::s_findOrderBooksBy,
        QT,
        QR,
        OMC
      >
      // use quote version instead of message sequence number
      (a_tmsg.m_QuoteVersion, a_tmsg, false,             // DynInitMode = false
       a_ts_recv, a_ts_handl, &ob);

    if (utxx::unlikely(!ok))
    {
      LOGA_ERROR(EConnector, 1,
        "EConnector_FIX::Process(Quote): Symbol={}: Quote "
        "Failed, Terminating the Connector...",    a_tmsg.m_Symbol.data())
      Stop();
    }

    if (utxx::unlikely(ob == nullptr))
    {
      LOGA_ERROR(EConnector, 1,
        "EConnector_FIX::Process(Quote): Symbol={}: No order book found, "
        "Terminating the Connector...",    a_tmsg.m_Symbol.data())
      Stop();
    }

    // check if the quote ID for this order book is already set
    int ob_id = int(ob - EConnector_MktData::m_orderBooks->data());
    assert(ob_id >= 0 &&
           ob_id < int  (EConnector_MktData::m_orderBooks->size()));

    ObjName& quote_id = m_quoteIDs->data()[ob_id]; // Ref!

    if (utxx::unlikely(IsEmpty(quote_id)))
      // Set the quote ID:
      InitFixedStr<sizeof(ObjName), sizeof(ObjName)>
        (&quote_id, a_tmsg.m_QuoteID);
  }

  //=========================================================================//
  // "Process(QuoteStatusReport)":                                           //
  //=========================================================================//
  template<FIX::DialectT::type   D>
  inline void EConnector_FIX<D>::Process
  (
    FIX::QuoteStatusReport  const& a_tmsg,
    FIX::SessData           const* DEBUG_ONLY  (a_sess),
    utxx::time_val                 CHECK_ONLY  (a_ts_recv),
    utxx::time_val                 UNUSED_PARAM(a_ts_handl)
  )
  {
    assert(a_sess == this && a_tmsg.m_MsgType == FIX::MsgT::QuoteStatusReport);
    CHECK_ONLY((void) SessMgr::CheckRxSN(a_tmsg, a_ts_recv);)

    char const* status =
      (a_tmsg.m_QuoteStatus == FIX::QuoteStatusT::RemovedFromMarket)
      ? "Removed from Market" :
      (a_tmsg.m_QuoteStatus == FIX::QuoteStatusT::QuoteNotFound)
      ? "Quote not found"
      : "Cancelled";

    LOGA_INFO(EConnector, 2,
        "EConnector_FIX::Process(QuoteStatusReport): Symbol={}, Status={}, "
        "Text={}", a_tmsg.m_Symbol.data(), status, a_tmsg.m_Text)

    auto ob = EConnector_MktData::FindOrderBook<false>(a_tmsg.m_Symbol.data());
    assert(ob != nullptr);

    int ob_id = int(ob - EConnector_MktData::m_orderBooks->data());
    assert(ob_id >= 0 &&
           ob_id < int  (EConnector_MktData::m_orderBooks->size()));

    // Reset the subscription req ID and quote ID:
    EConnector_MktData::m_obSubscrIDs->data()[ob_id] = 0;
    ZeroOut(m_quoteIDs->data() + ob_id);
  }

  //=========================================================================//
  // "Process(SecurityList)":                                                //
  //=========================================================================//
  template<FIX::DialectT::type   D>
  inline void EConnector_FIX<D>::Process
  (
    FIX::SecurityList       const& a_tmsg,
    FIX::SessData           const* DEBUG_ONLY(a_sess),
    utxx::time_val                 CHECK_ONLY(a_ts_recv),
    utxx::time_val                 UNUSED_PARAM(a_ts_handl)
  )
  {
    assert(a_sess == this && a_tmsg.m_MsgType == FIX::MsgT::SecurityList);
    CHECK_ONLY((void) SessMgr::CheckRxSN(a_tmsg, a_ts_recv);)

    // Call user handler (if present):
    if (utxx::likely(bool(m_secListHandler)))
      m_secListHandler(a_tmsg);
  }

  //=========================================================================//
  // Dummy MDC Methods (Server-Side):                                        //
  //=========================================================================//
  //--------------------------------------------------------------------------/
  // "Process(SecurityDefinition)":                                          //
  //-------------------------------------------------------------------------//
  // XXX: Currently, all SecDefs are configured off-line:
  //
  template<FIX::DialectT::type   D>
  inline void EConnector_FIX<D>::Process
  (
    FIX::SecurityDefinition const& a_tmsg,
    FIX::SessData           const* DEBUG_ONLY(a_sess),
    utxx::time_val                 CHECK_ONLY(a_ts_recv),
    utxx::time_val                 UNUSED_PARAM(a_ts_handl)
  )
  {
    assert(a_sess == this   &&   SessMgr::IsMktData()      &&
           a_tmsg.m_MsgType == FIX::MsgT::SecurityDefinition);

    CHECK_ONLY((void) SessMgr::CheckRxSN(a_tmsg, a_ts_recv);)

    // Call user handler (if present):
    if (utxx::likely(bool(m_secDefHandler)))
      m_secDefHandler(a_tmsg);
  }

  //-------------------------------------------------------------------------//
  // "Process(SecurityStatus)": TODO:                                        //
  //-------------------------------------------------------------------------//
  template<FIX::DialectT::type D>
  inline void   EConnector_FIX<D>::Process
  (
    FIX::SecurityStatus     const& CHECK_ONLY(a_tmsg),
    FIX::SessData           const* DEBUG_ONLY(a_sess),
    utxx::time_val                 CHECK_ONLY(a_ts_recv),
    utxx::time_val                 UNUSED_PARAM(a_ts_handl)
  )
  {
    assert(a_sess == this   &&   SessMgr::IsMktData()   &&
           a_tmsg.m_MsgType == FIX::MsgT::SecurityStatus);

    CHECK_ONLY((void) SessMgr::CheckRxSN(a_tmsg, a_ts_recv);)
  }

  //-------------------------------------------------------------------------//
  // "Process(MarketDataRequest)": TODO:                                     //
  //-------------------------------------------------------------------------//
  template<FIX::DialectT::type D>
  inline void   EConnector_FIX<D>::Process
  (
    FIX::MarketDataRequest  const& CHECK_ONLY(a_tmsg),
    FIX::SessData           const* DEBUG_ONLY(a_sess),
    utxx::time_val                 CHECK_ONLY(a_ts_recv),
    utxx::time_val                 UNUSED_PARAM(a_ts_handl)
  )
  {
    assert(a_sess == this   &&   SessMgr::IsMktData()   &&
           a_tmsg.m_MsgType == FIX::MsgT::MarketDataRequest);

    CHECK_ONLY((void) SessMgr::CheckRxSN(a_tmsg, a_ts_recv);)
  }
} // End namespace MAQUETTE
