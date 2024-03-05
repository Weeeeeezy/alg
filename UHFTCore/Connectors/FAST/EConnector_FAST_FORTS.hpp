// vim:ts=2:et
//===========================================================================//
//                "Connectors/FAST/EConnector_FAST_FORTS.hpp":               //
//  Embedded (Synchronous) FAST Protocol Connector for FORTS Futs and Opts   //
//===========================================================================//
// NB: In FORTS FullOrdersLog mode, we receive mkt data for BOTH Fut  and Opt
// Segments (there is no way of separating them);  so we will do both of them
// for SecDefs and SecDefUpdates, as well:
//
#pragma once

#include "Connectors/FAST/EConnector_FAST_FORTS.h"
#include "Connectors/EConnector_MktData.hpp"
#include "InfraStruct/SecDefsMgr.h"
#include "Venues/FORTS/Configs_FAST.h"

namespace MAQUETTE
{
  //=========================================================================//
  // "SecDefsUpdCh" Non-Default Ctor:                                        //
  //=========================================================================//
  template<FAST::FORTS::ProtoVerT::type Ver>
  inline EConnector_FAST_FORTS<Ver>::SecDefsUpdCh::SecDefsUpdCh
  (
    EConnector_FAST_FORTS* a_mdc,
    FORTS::AssetClassT     a_ac,
    std::string const&     a_iface_ip
  )
  : SSM_Channel<SecDefsUpdCh>
    (
      // Get the Config -- XXX: currently, we only use the 'A' feed; it is NOT
      // so critical if a certain update gets missing:
      FAST::FORTS::Configs
        .at(std::make_pair(a_mdc->m_env,  a_ac))
        .at(FAST::FORTS::DataFeedT::Infos_Incr)
        .first,
      a_iface_ip,
      a_mdc->m_name + "-SecDefsUpdCh-A",
      a_mdc->m_reactor,
      a_mdc->m_logger,
      a_mdc->m_debugLevel
    ),
    m_mdc      (a_mdc),
    m_secDefUpd(),
    m_secStat  ()
  {
    assert(m_mdc != nullptr);
  }

  //=========================================================================//
  // "SecDefsUpdCh::RecvHandler":                                            //
  //=========================================================================//
  // "SecurityDefinitionUpdate" and "SecurityStatus" decoding and processing
  // occur HERE:
  //
  template<FAST::FORTS::ProtoVerT::type Ver>
  inline bool EConnector_FAST_FORTS<Ver>::SecDefsUpdCh::RecvHandler
  (
    int            DEBUG_ONLY(a_fd),
    char const*    a_buff,
    int            a_size,
    utxx::time_val a_ts_recv
  )
  {
    //-----------------------------------------------------------------------//
    // Trivial Case: End-of-Chunk Event: Ignore it:                          //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely(a_buff == nullptr || a_size <= 0))
      return true;

    //-----------------------------------------------------------------------//
    // Generic Case: First of all, update the Rx Stats on the MDC:           //
    //-----------------------------------------------------------------------//
    assert(a_fd  == this->m_fd && a_buff != nullptr && a_size > 0 &&
          !a_ts_recv.empty()   && m_mdc  != nullptr);
    m_mdc->UpdateRxStats(a_size,  a_ts_recv);

    //-----------------------------------------------------------------------//
    // Parse the Msgs:                                                       //
    //-----------------------------------------------------------------------//
    // Get the Msg Header:
    FAST::PMap pmap = 0;
    FAST::TID  tid  = 0;
    char const* end = a_buff + a_size;
    // NB: The initial 4 bytes are the SeqNum prefix which we do not use in
    // this case, so skip them:
    a_buff   = FAST::GetMsgHeader
               (a_buff + 4, end, &pmap, &tid, this->m_shortName.data());

    //-----------------------------------------------------------------------//
    if (tid == FAST::FORTS::SecurityDefinitionUpdateTID<Ver>())
    //-----------------------------------------------------------------------//
    {
      // Parse a "SecurityDefinitionUpdate":
      a_buff    = m_secDefUpd.Decode(a_buff, end, pmap);

      // Find the SecDef (it's generally OK if the instrument is NOT found):
      SecDefD const* instr =
        m_mdc->m_secDefsMgr->FindSecDefOpt(m_secDefUpd.m_SecurityID);

      if (utxx::unlikely(instr == nullptr))
        return true;

      // Check the validity of all vals to be applied:
      assert(instr->m_SecID == m_secDefUpd.m_SecurityID);

      CHECK_ONLY
      (
        if (utxx::unlikely
           // NB: The following vals could be 0 if not set, but never < 0:
          (!IsFinite(m_secDefUpd.m_Volatility.m_val) ||
            m_secDefUpd.m_Volatility.m_val  <  0.0    ||
            !IsFinite(m_secDefUpd.m_TheorPrice.m_val) ||
            m_secDefUpd.m_TheorPrice.m_val  <  0.0))
        {
          LOG_WARN(2,
            "SecDefsUpdCh: Invalid SecDefUpdate Msg: SeqNum={}, Instr={}, "
            "SecID={}: Ignored...",
            m_secDefUpd.m_MsgSeqNum,  instr->m_FullName.data(), instr->m_SecID)
          // Still continue:
          return true;
        }
      )
      // Update the "SecDefD":
      // Install "Volatility" and "TheorPx" of Options:
      instr->m_Volatility = m_secDefUpd.m_Volatility.m_val;
      instr->m_TheorPx    = m_secDefUpd.m_TheorPrice.m_val;
    }
    else
    //-----------------------------------------------------------------------//
    if (tid == FAST::FORTS::SecurityStatusTID<Ver>())
    //-----------------------------------------------------------------------//
    {
      // Parse a "SecurityStatus":
      a_buff  = m_secStat.Decode(a_buff, end, pmap);

      // Get the corresp "SecDefD" (XXX: again, OK if not found):
      SecDefD const* instr =
        m_mdc->m_secDefsMgr->FindSecDefOpt(m_secStat.m_SecurityID);

      if (utxx::unlikely(instr == nullptr))
        return true;

      // Check the validity of all vals to be applied:
      assert(instr->m_SecID == m_secStat.m_SecurityID);

      CHECK_ONLY
      (
        if (utxx::unlikely
           (!IsFinite(m_secStat.m_LowLimitPx.m_val)                       ||
            m_secStat.m_LowLimitPx.m_val              <= 0.0              ||
            !IsFinite(m_secStat.m_HighLimitPx.m_val)                      ||
            m_secStat.m_HighLimitPx.m_val             <= 0.0              ||
            m_secStat.m_HighLimitPx.m_val <= m_secStat.m_LowLimitPx.m_val ||
            // IMs must all be > 0 except the Synthetic one:
            !IsFinite(m_secStat.m_InitialMarginOnBuy.m_val)               ||
            m_secStat.m_InitialMarginOnBuy.m_val      <= 0.0              ||
            !IsFinite(m_secStat.m_InitialMarginOnSell.m_val)              ||
            m_secStat.m_InitialMarginOnSell.m_val     <= 0.0              ||
            !IsFinite(m_secStat.m_InitialMarginSyntetic.m_val)            ||
            m_secStat.m_InitialMarginSyntetic.m_val   <  0.0))
        {
          LOG_WARN(2,
            "SecDefsUpdCh: Invalid SecurityStatus Msg: SeqNum={}, Instr={}, "
            "SecID={}: Ignored...",
            m_secStat.m_MsgSeqNum, instr->m_FullName.data(), instr->m_SecID)

          // Still continue:
          return true;
        }
      )
      // Update the "SecDefD":
      // First of all, update the "SecurityTradingStatus" and inform  Subscrib-
      // ers if trading in this security is suspended or, other way round, res-
      // umed:
      instr->m_TradingStatus =
        FAST::FORTS::GetTradingStatus(m_secStat.m_SecurityTradingStatus);

      // Set PxLimits and InitMargins:
      instr->m_LowLimitPx       = m_secStat.m_LowLimitPx.m_val;
      instr->m_HighLimitPx      = m_secStat.m_HighLimitPx.m_val;
      instr->m_InitMarginOnBuy  = m_secStat.m_InitialMarginOnBuy.m_val;
      instr->m_InitMarginOnSell = m_secStat.m_InitialMarginOnSell.m_val;
      instr->m_InitMarginHedged = m_secStat.m_InitialMarginSyntetic.m_val;

      // FIXME: In the following call we convert "instr->m_TradingStatus" into a
      // Boolean flag, and after that, it is converted back into "SecTrStatusT":
      // XXX:  "ExchTime" not available here, use "RecvTime" twice:
      m_mdc->ProcessStartStop
        (instr->m_TradingStatus == SecTrStatusT::FullTrading, nullptr,
         instr->m_SecID, a_ts_recv, a_ts_recv);
    }
    //-----------------------------------------------------------------------//
    else
    //-----------------------------------------------------------------------//
      // All other msgs are simply ignored:
      return true;

    // If we got here: In FORTS, there is only 1 msg per UDP pkt -- we must
    // have arrived at the "end":
    CHECK_ONLY
    (
      if (utxx::unlikely(a_buff != end))
        LOG_WARN(2,
          "SecDefsUpdCh::RecvHandler: Name={}: End-ptr wrong by {} bytes",
          this->m_longName, int(a_buff - end))
    )
    return true;
  }

  //=========================================================================//
  // "SecDefsUpdCh::ErrHandler":                                             //
  //=========================================================================//
  // Errors in this Channel are not critical. We simply restart the Channel in
  // case of any errors:
  //
  template<FAST::FORTS::ProtoVerT::type Ver>
  inline void EConnector_FAST_FORTS<Ver>::SecDefsUpdCh::ErrHandler
  (
    int               a_fd,
    int               a_err_code,
    uint32_t          a_events,
    char const*       a_msg
  )
  {
    assert(a_fd == this->m_fd && a_msg != nullptr);

    // Log the error:
    LOG_ERROR(1,
      "EConnector_FAST_FORTS::SecDefsUpdCh::ErrHandler: IOError: FD={}, "
      "ErrNo={}, Events={}: {}: Will re-start the Channel",
      a_fd, a_err_code, this->m_reactor->EPollEventsToStr(a_events), a_msg)

    this->Stop ();
    this->Start();
  }

  //=========================================================================//
  // "EConnector_FAST_FORTS" Non-Default Ctor:                               //
  //=========================================================================//
  template<FAST::FORTS::ProtoVerT::type Ver>
  inline EConnector_FAST_FORTS<Ver>::EConnector_FAST_FORTS
  (
    FAST::FORTS::EnvT                   a_env,
    EPollReactor*                       a_reactor,
    SecDefsMgr*                         a_sec_defs_mgr,
    std::vector<std::string>    const*  a_only_symbols,    // NULL=UseFullList
    RiskMgr*                            a_risk_mgr,
    boost::property_tree::ptree const&  a_params,
    EConnector_MktData*                 a_primary_mdc
  )
  : //-----------------------------------------------------------------------//
    // "EConnector": Virtual Base:                                           //
    //-----------------------------------------------------------------------//
    EConnector
    (
      // Generate the ConnectorName (aka "AccountKey") from "AccountPfx":
      a_params.get<std::string>("AccountPfx") + "-FAST-FORTS-" +
                  FAST::FORTS::ProtoVerT::to_string(Ver) + "-" +
                  FAST::FORTS::EnvT::to_string   (a_env),
      "FORTS",
      0,                          // XXX: No "native" MDC ShM data yet...
      a_reactor,
      false,                      // No BusyWait in FAST -- using UDP MCast
      a_sec_defs_mgr,
      FORTS::GetSecDefs_All(FAST::FORTS::GetMQTEnv(a_env)),
      a_only_symbols,
      a_params.get<bool>("ExplSettlDatesOnly", true),
      FORTS::UseTenorsInSecIDs,   // (false -- no need for Tenors)
      a_risk_mgr,
      a_params,
      FAST::FORTS::QT,
      FAST::FORTS::WithFracQtys
    ),
    //-----------------------------------------------------------------------//
    // "EConnector_FAST":                                                    //
    //-----------------------------------------------------------------------//
    EConnector_FAST
    <
      FAST::FORTS::OrdersLogSnapShot      <Ver>,
      FAST::FORTS::OrdersLogSnapShotTID   <Ver>(),
      FAST::FORTS::OrdersLogIncrRefresh   <Ver>,
      FAST::FORTS::OrdersLogIncrRefreshTID<Ver>(),
      true,
      FAST::FORTS::OrdersLogIncrRefresh   <Ver>,
      0,
      FAST::FORTS::TradingSessionStatus   <Ver>,
      FAST::FORTS::TradingSessionStatusTID<Ver>(),
      FAST::FORTS::HeartBeatTID<Ver>(),
      FAST::FORTS::QT,
      FAST::FORTS::QR,
      EConnector_FAST_FORTS,
      EConnector_TWIME_FORTS
    >
    (
      a_primary_mdc,
      a_params,
      false,      // No, FORTS RptSeqs are Non-Continuous
      0,          // MktDepth=+oo (encoded as 0) in FORTS with OrdersLog

      false,      // XXX: Explicitly-Subscribed Trades are not implemented yet
                  // for  FORTS, and they would probably be useless anyway, be-
                  // cause there is no way of identifying the MDEntryID(s)  of
                  // the Trade parties

      a_params.get<bool>("WithTrades"),
                  // Then "WithTrades" is equivalent to having FullOrdersLog-
                  // inferred Trades

      // Config  for OrdersLog SnapShots  ('A' only):
      FAST::FORTS::Configs
        .at(std::make_pair(a_env, FORTS::AssetClassT::FOL))
        .at(FAST::FORTS::DataFeedT::OrdersLogM_Snap)
        .first,

      // Configs for OrdersLog Increments ('A' and 'B'):
      FAST::FORTS::Configs
        .at(std::make_pair(a_env, FORTS::AssetClassT::FOL))
        .at(FAST::FORTS::DataFeedT::OrdersLogM_Incr),

      // Configs for Trades Increments ('A' and 'B'): They are NOT used in
      // FORTS -- Trades are inferred from the FullOrdersLog:
      nullptr
    ),
    //-----------------------------------------------------------------------//
    // Finally, "EConnector_FAST_FORTS" itself:                              //
    //-----------------------------------------------------------------------//
    m_env              (a_env),
    m_secDefsUpdCh_Fut (this, FORTS::AssetClassT::Fut,
                        ParentT::GetInterfaceIP(a_params, 'A')),
    m_secDefsUpdCh_Opt (this, FORTS::AssetClassT::Opt,
                        ParentT::GetInterfaceIP(a_params, 'A')),
    m_secDefsInited    (false)
  {
    // NB: In FORTS, "Env" actually implies "Ver", so verify their consistency:
    CHECK_ONLY
    (
      if (utxx::unlikely(Ver != FAST::FORTS::ImpliedProtoVerT(a_env)))
        throw utxx::badarg_error
              ("EConnector_FAST_FORTS::Ctor: Inconsistency: ProtoVer=",
               FAST::FORTS::ProtoVerT::to_string(Ver),        ", Env=",
               FAST::FORTS::EnvT::to_string(a_env));
    )
  }

  //=========================================================================//
  // "EConnector_FAST_FORTS::IsFullyInited":                                 //
  //=========================================================================//
  // XXX: We are NOT waiting for receiving all SecDefUpdates -- they may not be
  // necessary (and so managed at the Strategy level if required):
  //
  template<FAST::FORTS::ProtoVerT::type Ver>
  inline bool EConnector_FAST_FORTS<Ver>::IsFullyInited() const
      { return this->m_orderBooksInited; }

  //=========================================================================//
  // "EConnector_FAST_FORTS::IsActive":                                      //
  //=========================================================================//
  template<FAST::FORTS::ProtoVerT::type Ver>
  inline bool EConnector_FAST_FORTS<Ver>::IsActive()      const
  {
    // NB: If it IsFullyInited, then it is Active, and not Starting anymore:
    bool   res = IsFullyInited();
    assert(!(res && this->m_isStarting));
    return res;
  }

  //=========================================================================//
  // "EConnector_FAST_FORTS::Start":                                         //
  //=========================================================================//
  template<FAST::FORTS::ProtoVerT::type Ver>
  inline void EConnector_FAST_FORTS<Ver>::Start()
  {
    this->ParentT::Start();
    // Also start the FORTS-specific Channels:
    m_secDefsUpdCh_Fut.Start();
    m_secDefsUpdCh_Opt.Start();
  }

  //=========================================================================//
  // "EConnector_FAST_FORTS::Stop":                                          //
  //=========================================================================//
  template<FAST::FORTS::ProtoVerT::type Ver>
  inline void EConnector_FAST_FORTS<Ver>::Stop()
  {
    this->ParentT::Stop();
    // Also try to stop the FORTS-specific Channels:
    m_secDefsUpdCh_Fut.Stop();
    m_secDefsUpdCh_Opt.Stop();
  }

  //=========================================================================//
  // "EConnector_FAST_FORTS::SubscrMktData":                                 //
  //=========================================================================//
  // Overriding (extending) "EConnector_MktData::SubscribeMktData" is this par-
  // ticular case:
  //
  template<FAST::FORTS::ProtoVerT::type Ver>
  inline void EConnector_FAST_FORTS<Ver>::SubscribeMktData
  (
    Strategy*                 a_strat,
    SecDefD const&            a_instr,
    OrderBook::UpdateEffectT  a_min_cb_level,
    bool                      a_reg_instr_risks
  )
  {
    CHECK_ONLY
    (
      // XXX: Check that MaxCallBackLevel is consistent  with the MaxDepth  of
      // this Connector -- if the Caller requests L2 updates but we know  that
      // the Connector does not support that, throw an exception. (XXX: but we
      // still formally allow subscriptions at NONE level, though this is cle-
      // arly a degenerate case):
      if (utxx::unlikely
         (a_min_cb_level   == OrderBook::UpdateEffectT::L2 &&
          this->m_mktDepth == 1))
        throw utxx::badarg_error
              ("EConnector_FAST_FORTS::SubscribeMktData: Instr=",
               a_instr.m_FullName.data(), ": L2 MktData Not Available");

      // Also check that this Security is actually traded -- if not, we can
      // still subscribe to it, but produce a warning:
      if (utxx::unlikely
         (a_instr.m_TradingStatus != SecTrStatusT::FullTrading))
        LOG_WARN(2,
          "EConnector_FAST_FORTS::SubscribeMktData: Instr={}: Currently Not "
          "Trading", a_instr.m_FullName.data())
    )
    // Do the actual subscription:
    ParentT::SubscribeMktData
      (a_strat, a_instr, a_min_cb_level, a_reg_instr_risks);
  }

  //=========================================================================//
  // "EConnector_FAST_FORTS::DecodeOBSnapShot":                              //
  //=========================================================================//
  // Decoder for FullOrdersLog SnapShots (just a wrapper code):
  //
  template<FAST::FORTS::ProtoVerT::type Ver>
  inline char const* EConnector_FAST_FORTS<Ver>::DecodeOBSnapShot
  (
    char const*                             a_buff,
    char const*                             a_end,
    FAST::PMap                              a_pmap,
    FAST::FORTS::OrdersLogSnapShot<Ver>*    a_snap_shot
  )
  {
    assert(a_snap_shot != nullptr);
    return a_snap_shot->Decode(a_buff, a_end, a_pmap);
  }

  //=========================================================================//
  // "EConnector_FAST_FORTS::DecodeIncr":                                    //
  //=========================================================================//
  // Decoder for FullOrdersLog Increments (just a wrapper code):
  //
  template<FAST::FORTS::ProtoVerT::type Ver>
  inline char const* EConnector_FAST_FORTS<Ver>::DecodeIncr
  (
    char const*                             a_buff,
    char const*                             a_end,
    FAST::PMap                              a_pmap,
    FAST::FORTS::OrdersLogIncrRefresh<Ver>* a_incr
  )
  {
    assert(a_incr != nullptr);
    return a_incr->Decode(a_buff, a_end, a_pmap);
  }
}
// End namespace MAQUETTE
