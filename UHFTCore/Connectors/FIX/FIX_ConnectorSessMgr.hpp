// vim:ts=2:et
//===========================================================================//
//                 "Connectors/FIX/FIX_ConnectorSessMgr.hpp":                //
//===========================================================================//
#pragma once

#include "Basis/TimeValUtils.hpp"
#include "Connectors/TCP_Connector.hpp"
#include "Connectors/FIX/FIX_ConnectorSessMgr.h"
#include <utxx/compiler_hints.hpp>
#include <utxx/error.hpp>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <cassert>

namespace MAQUETTE
{
  //=========================================================================//
  // "FIX_ConnectorSessMgr" Non-Default Ctor:                                //
  //=========================================================================//
  template<FIX::DialectT::type   D, typename Processor>
  inline   FIX_ConnectorSessMgr <D, Processor>::FIX_ConnectorSessMgr
  (
    std::string const&                  a_name,
    EPollReactor  *                     a_reactor,
    Processor     *                     a_processor,
    spdlog::logger*                     a_logger,
    boost::property_tree::ptree const&  a_params,
    FIXSessionConfig const&             a_sess_conf,
    FIXAccountConfig const&             a_acct_conf,
    SeqNum*                             a_tx_sn,
    SeqNum*                             a_rx_sn
  )
  : //-----------------------------------------------------------------------//
    // TCP_Connector Ctor:                                                   //
    //-----------------------------------------------------------------------//
    TCPC
    (
      a_name,                   // Just for internal use
      a_sess_conf.m_ServerIP,
      a_sess_conf.m_ServerPort,
      8192 * 1024,              // BuffSz  = 8M (hard-wired)
      8192,                     // BuffLWM = 8k (hard-wired)
      "",                       // XXX: ClientIP not provided
      a_params.get<int>        ("MaxConnAttempts", 5),
      a_sess_conf.m_LogOnTimeOutSec,
      1,                        // LogOffTimeOut=1 sec (hard-wired)
      a_sess_conf.m_ReConnectSec,
      a_sess_conf.m_HeartBeatSec,
      // NB: If UseTLS is set, VerifyServerCert is set by default as well:
      a_sess_conf.m_UseTLS,
      a_sess_conf.m_ServerName, // Non-empty if TLS is used
      IO::TLS_ALPN::UNDEFINED,  // FIX is not in IATA registry anyway
      a_params.get<bool>       ("VerifyServerCert",  a_sess_conf.m_UseTLS),
      a_params.get<std::string>("ServerCAFiles",     ""),
      a_params.get<std::string>("ClientCertFile",    ""),
      a_params.get<std::string>("ClientPrivKeyFile", ""),
      a_params.get<std::string>("ProtoLogFile",      "")
    ),
    //-----------------------------------------------------------------------//
    // FIX Session Data:                                                     //
    //-----------------------------------------------------------------------//
    FIX::SessData
    (
      // The following params are common for Clients and Servers (but this is a
      // Client!):
      a_acct_conf.m_OurCompID,
      a_sess_conf.m_ServerCompID,
      a_tx_sn,
      a_rx_sn,
      a_sess_conf.m_HeartBeatSec,
      a_sess_conf.m_MaxGapMSec,
      a_acct_conf.m_OurSubID,
      "",
      // Our User Credentials. NB: NewPasswd is typically not set:
      a_acct_conf.m_UserName,
      a_acct_conf.m_Passwd,
      a_params.get<std::string>("NewPasswd", ""),
      a_acct_conf.m_Account,
      a_acct_conf.m_ClientID,
      a_acct_conf.m_PartyID,
      a_acct_conf.m_PartyIDSrc,
      a_acct_conf.m_PartyRole
    ),
    //-----------------------------------------------------------------------//
    // Now initialise the Protocol Engine:                                   //
    //-----------------------------------------------------------------------//
    //XXX: The following Ctor will call "GetFIXSession" on THIS obj which is
    // still being constructed, but this is OK since that method is non-virtual,
    // and "TCP_Connector" "FIX::SessData" have already been constructed above:
    //
    ProtoEngT
    (
      this,                     // SessMgr
      a_processor               // Processor
    ),
    //-----------------------------------------------------------------------//
    // Finally, this class:                                                  //
    //-----------------------------------------------------------------------//
    m_name      (a_name),
    m_reactor   (a_reactor),
    m_processor (a_processor),     // Eg the "EConnector_FIX"
    m_logger    (a_logger),
    m_debugLevel(a_params.get<int>("DebugLevel")),
    m_isOrdMgmt (a_sess_conf.m_IsOrdMgmt),
    m_isMktData (a_sess_conf.m_IsMktData)
  {
    //-----------------------------------------------------------------------//
    // Do some verification of Configs:                                      //
    //-----------------------------------------------------------------------//
    // Reactor and Logger are mandatory (but Processor may be absent if it is a
    // pure SessnLevel impl):
    CHECK_ONLY
    (
      if (utxx::unlikely(m_reactor == nullptr || m_logger == nullptr))
        throw utxx::badarg_error
              ("FIX_ConnectorSessMgr::Ctor: Missing Reactor and/or Logger");

      // Check that the dynamic OrdMgmt/MktData settings of this Config match
      // the FIX Dialect capabilites:
      if (utxx::unlikely
         ((m_isOrdMgmt && !FIX::ProtocolFeatures<D>::s_hasOrdMgmt) ||
          (m_isMktData && !FIX::ProtocolFeatures<D>::s_hasMktData) ))
        throw utxx::badarg_error
              ("FIX_ConnectorSessMgr::Ctor: Mode MisMatch: Conf.IsOrdMgmt=",
               m_isOrdMgmt,       ", Proto.HasOrdMgmt=",
               FIX::ProtocolFeatures<D>::s_hasOrdMgmt,  "; Conf.IsMktData=",
               m_isMktData,       ", Proto.HasMktData=",
               FIX::ProtocolFeatures<D>::s_hasMktData);
    )
    // Also, the HeartBeat params in TCP_Connector (for actual dynamic sending
    // of HeartBeats) and in FIX::SessData (for use as a FIX fld) must be cons-
    // istent:
    assert(TCPC::m_heartBeatSec == FIX::SessData::m_heartBeatSec);

    //-----------------------------------------------------------------------//
    // Possibly reset the "SeqNum"s:                                         //
    //-----------------------------------------------------------------------//
    // XXX: We do it if requested, and also if it is an MDC and not an OMC. The
    // latter condition for reset is also applied in FIX::ProtoEngine::SendLog-
    // On. We maintain the reset logic in BOTH places: Here at Connector const-
    // ruction, and in FIX::ProtoEngine at dynamic LogOns.  NB:  "ResetSeqNums"
    // param is not even used if (MDC && !OMC) cond is fulfilled:
    //
    if ((IsMktData() && !IsOrdMgmt()) || a_params.get<bool>("ResetSeqNums"))
    {
      *FIX::SessData::m_txSN = 1;
      *FIX::SessData::m_rxSN = 1;
    }
  }

  //=========================================================================//
  // "FIX_ConnectorSessMgr" Dtor:                                            //
  //=========================================================================//
  template<FIX::DialectT::type   D, typename Processor>
  inline   FIX_ConnectorSessMgr <D, Processor>::~FIX_ConnectorSessMgr()
  noexcept
  {
    // Do not allow any exceptions to propagate from this Dtor. For extra
    // safety, we do NOT invoke "Stop" from the Dtor, hence nothing to do...
  }

  //=========================================================================//
  // "InitLogOn":                                                            //
  //=========================================================================//
  template   <FIX::DialectT::type  D, typename Processor>
  inline void FIX_ConnectorSessMgr<D, Processor>::InitLogOn()
  {
    // IMPORTANT: Before sening the LogOn msg, drop all curr content of the out-
    // buff which may still be there:
    ProtoEngT::ResetOutBuff();
    ProtoEngT::SendLogOn   (this);
  }

  //=========================================================================//
  // "Stop":                                                                 //
  //=========================================================================//
  // Returns "true" unless already stopping:
  //
  template<FIX::DialectT::type       D, typename Processor>
  template<bool FullStop>
  inline   void FIX_ConnectorSessMgr<D, Processor>::Stop()
  {
    // Being polite to the server, send the MktData UnSubscribe Reqs
    // (IsSubscr=false):
    if (IsMktData() && !FIX::ProtocolFeatures<D>::s_hasStreamingQuotes)
      m_processor->template SendMktDataSubscrReqs<false>();

    // Initiate Stop of the "TCP_Connector":
    (void) TCPC::template Stop<FullStop>();
  }

  //=========================================================================//
  // "InitGracefulStop":                                                     //
  //=========================================================================//
  // Call-Back to be used by "TCP_Connector":
  //
  template   <FIX::DialectT::type  D, typename Processor>
  inline void FIX_ConnectorSessMgr<D, Processor>::InitGracefulStop()
  {
    if (IsOrdMgmt())
    {
      // Cancel all active orders (in case if there is no automatic Cancel-on-
      // Disconnect). XXX: However, if this op is not supported by the FIX pro-
      // tocol itself (ie requires emulation), we cannot do it at this level,
      // as it would require a call-back into the Application Level:
      //
      if constexpr (FIX::ProtocolFeatures<D>::s_hasMassCancel          &&
                    FIX::ProtocolFeatures<D>::s_hasAllSessnsInMassCancel)
        ProtoEngT::CancelAllOrdersImpl
          (this, 0, nullptr, FIX::SideT::UNDEFINED, nullptr);

      // XXX: Issue a "TestRequest" before "LogOut" to finally synchronise the
      // "SeqNum"s between the Client and the Server,  though not sure whether
      // it is really useful:
      ProtoEngT::SendTestRequest(this);

      // Do NOT send a "LogOut" yet -- this was only the 1st stage of the Grace-
      // fulStop, we still need to receive responses on "MassCancel"...
    }
  }

  //=========================================================================//
  // "GracefulStopInProgress":                                               //
  //=========================================================================//
  // Another call-back from "TCP_Connector". Now perform a "LogOut". Applies to
  // any Connector Type (OMC or MDC):
  template   <FIX::DialectT::type  D, typename Processor>
  inline void FIX_ConnectorSessMgr<D, Processor>::GracefulStopInProgress()
  {
    // As this is a MktData Session, it's probably a good idea to explicitly
    // "UnSubscribeAll":
    if (IsMktData() && m_processor != nullptr)
      m_processor->UnSubscribeAll();

    if constexpr
      (FIX::ProtocolFeatures<D>::s_fixVersionNum >= FIX::ApplVerIDT::FIX50)
      ProtoEngT::SendAppLogOff(this);
    else
      ProtoEngT::SendLogOut   (this);
  }

  //=========================================================================//
  // "StopNow":                                                              //
  //=========================================================================//
  template   <FIX::DialectT::type  D, typename Processor>
  template   <bool FullStop>
  inline void FIX_ConnectorSessMgr<D, Processor>::StopNow
  (
    char const*     a_where,
    utxx::time_val  a_ts_recv
  )
  {
    assert(a_where != nullptr);

    // Perform DisConnect at the TCP Level:
    TCPC::template DisConnect<FullStop>(a_where);

    // The TCP Socket is now closed, propagate it to the SessData as well:
    assert(TCPC::m_fd == -1 && IsInactiveSess(this));

    if (m_processor != nullptr)
    {
      // For a MktData Session, reset all MktData:
      if (IsMktData())
        m_processor->ResetOrderBooksAndOrders();

      // Then invoke the Processor notify the Subscribers at the Application
      // Level of this Stop (ie On = false):
      m_processor->ProcessStartStop(false, nullptr, 0, a_ts_recv, a_ts_recv);
    }
    LOG_INFO(1,
      "FIX_ConnectorSessMgr::StopNow: Session is now STOPPED{}",
      FullStop ? "." : ", but will re-start")
  }

  //=========================================================================//
  // "ServerInactivityTimeOut":                                              //
  //=========================================================================//
  template   <FIX::DialectT::type  D, typename Processor>
  inline void FIX_ConnectorSessMgr<D, Processor>::ServerInactivityTimeOut
    (utxx::time_val a_ts_recv)
  {
    // Stop immediately, but will try to re-start. XXX: Detailed logging???
    this->StopNow<false>("ServerInactivityTimeOut", a_ts_recv);
  }

  //=========================================================================//
  // Static SessMgr API Required by the FIX Protocol Layer:                  //
  //=========================================================================//
  //=========================================================================//
  // "GetFIXSession":                                                        //
  //=========================================================================//
  template<FIX::DialectT::type D, typename Processor>
  inline   FIX::SessData*  FIX_ConnectorSessMgr<D, Processor>::GetFIXSession
    (int DEBUG_ONLY(a_fd))
  {
    assert(a_fd == TCPC::m_fd);
    return this;
  }

  //=========================================================================//
  // "CheckFIXSession":                                                      //
  //=========================================================================//
  // Always returns "true" for the Client-Side. In fact, this method is requi-
  // red by the static interface, but is never actually called on the  Client-
  // Side:
  template   <FIX::DialectT::type  D, typename Processor>
  template   <FIX::MsgT::type>
  inline bool FIX_ConnectorSessMgr<D, Processor>::CheckFIXSession
  (
    int                    DEBUG_ONLY(a_fd),
    FIX::MsgPrefix const*  DEBUG_ONLY(a_msg_pfx),
    FIX::SessData**        DEBUG_ONLY(a_sess)
  )
  const
  {
    assert(a_fd == TCPC::m_fd && a_msg_pfx != nullptr && *a_sess == this);
    return true;
  }

  //=========================================================================//
  // "IsActiveSess", "IsActive":                                             //
  //=========================================================================//
  // For safety, use the Conjunction of the following conds:
  //
  template   <FIX::DialectT::type  D, typename Processor>
  inline bool FIX_ConnectorSessMgr<D, Processor>::IsActiveSess
    (FIX::SessData const* DEBUG_ONLY(a_sess))
  const
  {
    assert (a_sess  == this);
    return TCPC::m_status == TCPC::StatusT::Active &&
           m_reactor->IsLive(TCPC::m_fd);
  }

  template   <FIX::DialectT::type  D, typename Processor>
  inline bool FIX_ConnectorSessMgr<D, Processor>::IsActive() const
    { return IsActiveSess(this); }

  //=========================================================================//
  // "IsInactiveSess":                                                       //
  //=========================================================================//
  // NB: The 3 conds below shold generally be equivalent, but we use a Disjun-
  // ction of them for extra safety:
  // NB: In general, IsInactiveSess() != !IsActiveSess(); there are intermedi-
  // ate states (eg when the Session is LoggingOn) which are neither fully Ac-
  // tive nor fully Inactive. But obviosly, this 2 states are mutually-exclus-
  // ive:
  template   <FIX::DialectT::type  D, typename Processor>
  inline bool FIX_ConnectorSessMgr<D, Processor>::IsInactiveSess
    (FIX::SessData const* DEBUG_ONLY(a_sess))
  const
  {
    assert (a_sess == this);
    return TCPC::m_status == TCPC::StatusT::Inactive ||
         !(m_reactor->IsLive(TCPC::m_fd));
  }

  //=========================================================================//
  // "TerminateSession":                                                     //
  //=========================================================================//
  // Graceful or Non-Graceful Session Termination. This is a Call-Back required
  // by the Proto Engine. XXX: On the Client-Side, even if it "IsGraceful",  we
  // do "StopNow" (with FullStop=false) rather than a deferred "Stop",   so the
  // template param is effectively ignored.  This is because "TerminateSession"
  // is invoked by the ProtoEngine in case of rather serious  protocol  errors;
  // on the client side, it is better to Stop and Restart:
  //
  template   <FIX::DialectT::type  D, typename Processor>
  template   <bool IsGraceful>
  inline void FIX_ConnectorSessMgr<D, Processor>::TerminateSession
  (
    int                  DEBUG_ONLY(a_fd),
    FIX::SessData const* DEBUG_ONLY(a_sess),
    char const*          a_err_msg,
    utxx::time_val       a_ts_recv
  )
  {
    assert (a_fd == TCPC::m_fd && a_sess == this && a_err_msg != nullptr);
    this->StopNow<false>(a_err_msg, a_ts_recv);
  }

  //=========================================================================//
  // "RejectMsg":                                                            //
  //=========================================================================//
  // Currently, on the Client-Side, this method is NOT called. So it does noth-
  // ing and formally returns "false" -- as to instruct the Caller to terminate
  // the Session afetr such a cond was encountered (because a Client should ne-
  // ver reject Server's msgs!):
  //
  template   <FIX::DialectT::type  D, typename Processor>
  inline bool FIX_ConnectorSessMgr<D, Processor>::RejectMsg
  (
    int                  DEBUG_ONLY  (a_fd),
    FIX::SessData const* DEBUG_ONLY  (a_sess),
    FIX::MsgT            UNUSED_PARAM(a_msg_type),
    SeqNum               UNUSED_PARAM(a_seq_num),
    FIX::SessRejReasonT  UNUSED_PARAM(a_rej_reason),
    char const*          UNUSED_PARAM(a_err_msg)
  )
  const
  {
    assert(a_fd == TCPC::m_fd && a_sess == this);
    return false;
  }

  //=========================================================================//
  // "SendImpl":                                                             //
  //=========================================================================//
  template<FIX::DialectT::type D,   typename    Processor>
  inline utxx::time_val FIX_ConnectorSessMgr<D, Processor>::SendImpl
  (
    FIX::SessData const* DEBUG_ONLY(a_sess),
    char const*          a_buff,
    int                  a_len
  )
  const
  {
    // Run the TCP_Connector's Send (which in turn invokes the Reactor Send).
    // Any error conds are handled by "ErrHandler", after which, exceptions may
    // propafate to the Caller:
    assert(a_sess == this);
    return TCPC::SendImpl(a_buff, a_len);
  }

  //=========================================================================//
  // Processing of Session- (Admin-) Level FIX Msgs:                         //
  //=========================================================================//
  //=========================================================================//
  // "Process(LogOn)":                                                       //
  //=========================================================================//
  template   <FIX::DialectT::type  D, typename Processor>
  inline void FIX_ConnectorSessMgr<D, Processor>::Process
  (
    FIX::LogOn    const& a_tmsg,
    FIX::SessData const* a_sess,
    utxx::time_val       CHECK_ONLY(a_ts_recv)
  )
  {
    assert(TCPC::m_status   == TCPC::StatusT::LoggingOn && a_sess == this &&
           a_tmsg.m_MsgType == FIX::MsgT::LogOn);
    CHECK_ONLY
    (
      if (utxx::unlikely(!CheckRxSN(a_tmsg, a_ts_recv)))
        return;
    )
    // We may get Passwd Change Status in this (if Passwd Change is available,
    // and was requested):
    if constexpr (FIX::ProtocolFeatures<D>::s_hasPasswdChange)
    {
      switch (a_tmsg.m_PasswdChangeStatus)
      {
      case FIX::PasswdChangeStatusT::InvalidPasswdChange:
        LOG_ERROR
          (1, "FIX_ConnectorSessMgr::Process(LogOn): Passwd change has FAILED, "
              "Old Passwd still in place: {}", FIX::SessData::m_passwd.data())
        // It's probably better to FullStop after that -- but still gracefully:
        Stop<true>();
        return;

      case FIX::PasswdChangeStatusT::PasswdChangeOK:
        LOG_INFO
          (1, "FIX_ConnectorSessMgr::Process(LogOn): Passwd change OK: "
              "New Passwd: {}", a_sess->m_newPasswd.data())
        // We also FullStop in this case so external actions to complete the
        // passwd change could be carried out:
        Stop<true>();
        return;

      default: ;
        // There was no passwd change request, nothing to do here
      }
    }
    // So, in any case: Logged on successfully (XXX: but not completely yet  --
    // will still have to synchronise the SeqNums, so no notificatiosn are sent
    // yet!):
    LOG_INFO(1,"FIX_ConnectorSessMgr::Process(LogOn): LogOn CONFIRMED")

    // To verify that all "SeqNum"s are in sync with the server,  it might be a
    // good idea to send a "HeartBeat" and to request a "HeartBeat" now (unless
    // we have to wait for a "TradingSessionStatus" -- in that case, do not send
    // anything yet):
    // XXX: Do NOT make our status "Active" or notify the Strategies yet -- wait
    // until "SeqNum"s correct synchronisation is confirmed (a "HeartBeat" is
    // received), or "TradingSessionStatus" is received, etc:
    //
    if constexpr
      (FIX::ProtocolFeatures<D>::s_fixVersionNum >= FIX::ApplVerIDT::FIX50)
      ProtoEngT::SendAppLogOn   (this);
    else
    if constexpr (!FIX::ProtocolFeatures<D>::s_waitForTrSessStatus)
    {
      SendHeartBeat();
      ProtoEngT::SendTestRequest(this);
    }
  }

  //=========================================================================//
  // "Process(UserResponse)":                                                //
  //=========================================================================//
  template   <FIX::DialectT::type  D, typename    Processor>
  inline void FIX_ConnectorSessMgr<D, Processor>::Process
  (
    FIX::UserResponse const& a_tmsg,
    FIX::SessData     const* CHECK_ONLY(a_sess),
    utxx::time_val           a_ts_recv
  )
  {
    assert(TCPC::m_status   == TCPC::StatusT::LoggingOn && a_sess == this &&
           a_tmsg.m_MsgType == FIX::MsgT::UserResponse);

    CHECK_ONLY
    (
    if (utxx::unlikely(!CheckRxSN(a_tmsg, a_ts_recv)))
      return;

    if (utxx::unlikely(a_sess->m_userReqID != a_tmsg.m_UserReqID))
    {
      // This must not happen. Fall through to "StopNow":
      LOG_ERROR(1,
        "FIX_ConnectorSessMgr::Process(UserResponse): Invalid UserReqID={},"
        " Expected={}", a_tmsg.m_UserReqID, a_sess->m_userReqID)
    }
    else
    )
    // End CHECK_ONLY
    //
    if (a_tmsg.m_UserStat == FIX::UserStatT::LoggedIn)
    {
      // So, in any case: Logged on successfully (XXX: but not completely yet
      // will still have to synchronise the SeqNums, so no notificatiosn are
      // sent yet!):
      LOG_INFO(1, "FIX_ConnectorSessMgr::Process(UserResponse): CONFIRMED")

      if constexpr (!FIX::ProtocolFeatures<D>::s_waitForTrSessStatus)
      {
        // Send TestRequest -- similar to Process(LogOn) above:
        SendHeartBeat();
        ProtoEngT::SendTestRequest(this);
      }
      // All Done:
      return;
    }
    else
    {
      // Any other UserStat -- unsuccessful LogOn:
      LOG_ERROR(1,
        "FIX_ConnectorSessMgr::Process(UserResponse): UNSUCCESSFUL: {} {}",
        a_tmsg.m_UserStat.to_string(), a_tmsg.m_UserStatText)
    }
    // If we got here: Stop the Connector immediately. Will NOT attempt to re-
    // connect; "StopNow" (FullStop=true) will invoke  internal Notifications:
    //
    this->StopNow<true>("Process(UserResponse)", a_ts_recv);
  }

  //=========================================================================//
  // "Process(LogOut)":                                                      //
  //=========================================================================//
  template   <FIX::DialectT::type  D, typename    Processor>
  inline void FIX_ConnectorSessMgr<D, Processor>::Process
  (
    FIX::LogOut   const& a_tmsg,
    FIX::SessData const* DEBUG_ONLY(a_sess),
    utxx::time_val       a_ts_recv
  )
  {
    assert(a_sess == this && a_tmsg.m_MsgType == FIX::MsgT::LogOut);

    // The Server has confirmed that we have logged out. We STILL need to pro-
    // cess SeqNums (at least to get a valid RxSN for the next run):
    (void) CheckRxSN(a_tmsg, a_ts_recv);

    LOG_INFO(1,
      "FIX_ConnectorSessMgr::Process(LogOut): LogOut CONFIRMED: {}",
      a_tmsg.m_Text)

    // We can now stop the Connector immediately. Will NOT attempt re-connect;
    // "StopNow" (FullStop=true)  will invoke internal Notifications:
    //
    this->StopNow<true>("Process(LogOut)", a_ts_recv);
  }

  //=========================================================================//
  // "Process(ResendRequest)":                                               //
  //=========================================================================//
  template   <FIX::DialectT::type  D, typename    Processor>
  inline void FIX_ConnectorSessMgr<D, Processor>::Process
  (
    FIX::ResendRequest const& a_tmsg,
    FIX::SessData      const* DEBUG_ONLY(a_sess),
    utxx::time_val            a_ts_recv
  )
  {
    assert(a_sess == this && a_tmsg.m_MsgType == FIX::MsgT::ResendRequest);

    // NB: The msg itself must have come with a valid SeqNum:
    CHECK_ONLY
    (
      if (utxx::unlikely(!CheckRxSN(a_tmsg, a_ts_recv)))
        return;
    )
    //-----------------------------------------------------------------------//
    // Construct the GapFill Range:                                          //
    //-----------------------------------------------------------------------//
    // The Server is asking us to re-send some of our msgs. We NEVER do this
    // directly,  primarily because such msgs could by now  be semantically-
    // outdated,  and only the corresp Strategy can decide whether they  are
    // still required. So we send a "GapFill" instead:
    LOG_INFO(2,
      "FIX_ConnectorSessMgr::Process(ResendRequest): [{}..{}]",
      a_tmsg.m_BeginSeqNo, a_tmsg.m_EndSeqNo)

    // Determine the bounds for the GapFill.  If 0 (means +oo)  upper bound is
    // requested, make it the largest SeqNum sent so far, ie (TxSN-1), because
    // TxSN is NEXT one to be sent:
    //
    SeqNum from = a_tmsg.m_BeginSeqNo;
    SeqNum upto =
      ( a_tmsg.m_EndSeqNo != 0 )
      ? a_tmsg.m_EndSeqNo
      : (*FIX::SessData::m_txSN - 1);

    // XXX: The following is for logging only -- but we still avoid "sprintf"!
    char    rangeStr[64];
    {
      char* curr = rangeStr;
      *curr = '[';
      curr  = utxx::itoa_left<SeqNum, 20>(curr + 1, from);
      curr  = stpcpy(curr, "..");
      curr  = utxx::itoa_left<SeqNum, 20>(curr,     upto);
      curr  = stpcpy(curr, "]" );
      assert(*curr == '\0' && curr < rangeStr + sizeof(rangeStr));
    }

    CHECK_ONLY
    (
      if (utxx::unlikely(from <= 0 || from > upto))
      {
        // This would be a serious error condition:
        HardSeqNumError(rangeStr, 0, a_tmsg.m_MsgType, a_ts_recv);
        return;
      }
    )
    //-----------------------------------------------------------------------//
    // Update the State for Sent Msgs which fall into that Range:            //
    //-----------------------------------------------------------------------//
    // NB:
    // (*) This requires the Processor invocation.
    // (*) We may or may not have actually sent them; currently, the procedure
    //     is the same in both cases; if we did not even send those msgs, this
    //     will result in false warnings logged.
    // (*) So: if we are NOT in the "Active" state (eg "Logging{On|Out}"), then
    //     "ResendRequest" has occurred likely because of some SeqNums mismatch
    //     (eg we did not reset them to 1 at the beginning), so there is no need
    //     to invoke "ReqRejectedBySession" on the StateMgr:
    // (*) All this makes sense for an OrdMgmt Connector only; otherwise, it's
    //     an outright failure:
    //
    if (IsOrdMgmt())
    {
      if (TCPC::m_status == TCPC::StatusT::Active && m_processor != nullptr)
        for (SeqNum s = from; s <= upto; ++s)
          (void) m_processor->template ReqRejectedBySession<QT, QR>
                (m_processor, m_processor, true, s, 0, rangeStr,
                 a_tmsg.m_SendingTime,     a_ts_recv);
    }
    else
      // Terminate the Session now (FullStop=true):
      this->StopNow<true>
        ("FIX_ConnectorSessMgr::Process(ResendRequest): No OrdMgmt", a_ts_recv);

    //-----------------------------------------------------------------------//
    // Send "GapFill" etc out:                                               //
    //-----------------------------------------------------------------------//
    ProtoEngT::SendGapFill    (this, from, upto);

    // XXX: It's probably also a good idea to send a "HeartBeat" and "TestReq"
    // with the following-on SeqNums, to make sure the Server has accepted our
    // GapFill:
    SendHeartBeat();
    ProtoEngT::SendTestRequest(this);

    // All Done!
  }

  //=========================================================================//
  // "Process(SequenceReset)":                                               //
  //=========================================================================//
  template   <FIX::DialectT::type  D, typename    Processor>
  inline void FIX_ConnectorSessMgr<D, Processor>::Process
  (
    FIX::SequenceReset const& a_tmsg,
    FIX::SessData      const* DEBUG_ONLY(a_sess),
    utxx::time_val            CHECK_ONLY(a_ts_recv)
  )
  {
    assert(a_sess == this && a_tmsg.m_MsgType == FIX::MsgT::SequenceReset);

    //-----------------------------------------------------------------------//
    // Check the monotonicity of "newRSN":                                   //
    //-----------------------------------------------------------------------//
    // In any case, decreasing the RxSN is not allowed, no matter whether is a
    // true Reset or a GapFill:
    //
    SeqNum&  RxSN = *FIX::SessData::m_rxSN; // Ref!
    SeqNum newRSN = a_tmsg.m_NewSeqNo;
    assert(newRSN > 0);                     // Because it was parsed as "Pos"

    CHECK_ONLY
    (
      if (utxx::unlikely(newRSN < RxSN))
      {
        // This is a serious error condition in any case: Terminate the Session,
        // and then start it with a full reset of SeqNums:
        HardSeqNumError
          ("Backward SequenceReset: NewRxSN=", newRSN, a_tmsg.m_MsgType,
          a_ts_recv);
        return;
      }
      else
      if (utxx::unlikely(newRSN == RxSN))
        // Then do not even analyse the msg in detail -- nothing to do anyway:
        return;

      assert(newRSN > RxSN);
    )
    //-----------------------------------------------------------------------//
    // If it is a "SequenceReset-Reset" (not "-GapFill"):                    //
    //-----------------------------------------------------------------------//
    // then the msg's own SeqNum (NOT "newRSN") is NOT verified against the ex-
    // pected one. This condition should be very rare as well. We allow such a
    // msg to be unsolicited  (ie not in response to our previous  "ResendRequ-
    // est"):
    //
    if (utxx::unlikely(!a_tmsg.m_GapFillFlag))
    {
      // "GapFillFlag" is not set -- this is a "SequenceReset-Reset" indeed.
      // Do the reset as instructed   (we have already checked that the new
      // SeqNum is non-decreasing):
      RxSN = newRSN;

      LOG_INFO(2,
        "FIX_ConnectorSessMgr::Process(SequenceReset): Hard Reset: "
        "NewRxSN={}", newRSN)
      return;
    }
    //-----------------------------------------------------------------------//
    // Generic Case: It is a "SequenceReset-GapFill":                        //
    //-----------------------------------------------------------------------//
    // Then first verify the own SeqNum of this msg -- it must be valid:
    CHECK_ONLY
    (
      if (utxx::unlikely(!CheckRxSN(a_tmsg, a_ts_recv)))
        return;

      // XXX: Could it be unsolicited -- not in response to our previous  "Re-
      // sendRequest"? Also, it should carry the "Dup" flag. -- We allow these
      // conds to be violated, but log a warning in that case:
      //
      if (utxx::unlikely
         (FIX::SessData::m_resendReqTS.empty() || !a_tmsg.m_PossDupFlag))
        LOG_WARN(2,
          "FIX_ConnectorSessMgr::Process(SequenceReset): Bogus SequenceReset-"
          "GapFill? NewSeqNo={}, IsDup={}, RxSN={}: But still doing it...",
          newRSN,   a_tmsg.m_PossDupFlag,  RxSN)
    )
    //------------------------------------------------------------------------//
    // So yes, reset the RxSN:                                                //
    //------------------------------------------------------------------------//
    RxSN = a_tmsg.m_NewSeqNo;

    LOG_INFO(2,
      "FIX_ConnectorSessMgr::Process(SequenceReset): GapFill: NewRxSN={}",
      a_tmsg.m_NewSeqNo)

    // All Done!
  }

  //=========================================================================//
  // "Process(TestRequest)":                                                 //
  //=========================================================================//
  template   <FIX::DialectT::type  D, typename    Processor>
  inline void FIX_ConnectorSessMgr<D, Processor>::Process
  (
    FIX::TestRequest const& a_tmsg,
    FIX::SessData    const* DEBUG_ONLY(a_sess),
    utxx::time_val          CHECK_ONLY(a_ts_recv)
  )
  {
    assert(a_sess == this && a_tmsg.m_MsgType == FIX::MsgT::TestRequest);
    CHECK_ONLY
    (
      if (utxx::unlikely(!CheckRxSN(a_tmsg, a_ts_recv)))
        return;
    )
    // Generate an immediate "HeartBeat" in response:
    ProtoEngT::SendHeartBeat(this, a_tmsg.m_TestReqID);
  }

  //=========================================================================//
  // "Process(HeartBeat)":                                                   //
  //=========================================================================//
  template   <FIX::DialectT::type  D, typename    Processor>
  inline void FIX_ConnectorSessMgr<D, Processor>::Process
  (
    FIX::HeartBeat const& a_tmsg,
    FIX::SessData  const* DEBUG_ONLY(a_sess),
    utxx::time_val        a_ts_recv
  )
  {
    assert(a_sess == this && a_tmsg.m_MsgType == FIX::MsgT::HeartBeat);
    CHECK_ONLY
    (
      if (utxx::unlikely(!CheckRxSN(a_tmsg, a_ts_recv)))
      return;
    )
    // If we are in the "LoggingIn" status, and got to here, it means that all
    // "SeqNum"s are indeed correctly synchronised between the Client and  the
    // Server,   so we can finally become "Active":
    if (utxx::unlikely(TCPC::m_status == TCPC::StatusT::LoggingOn))
    {
      // But if we are waiting for a "TradingSessionStatus" and did not get it
      // yet, continue to wait:
      if constexpr (FIX::ProtocolFeatures<D>::s_waitForTrSessStatus)
        return;

      // For FIX5.0 or upper we need application level session
      if constexpr
        (FIX::ProtocolFeatures<D>::s_fixVersionNum >= FIX::ApplVerIDT::FIX50)
        return;

      // Otherwise: The LogOn process is now complete:
      LogOnCompleted(a_tmsg.m_SendingTime, a_ts_recv);
    }
    // In all other cases, "HeartBeat" is ignored
  }

  //=========================================================================//
  // "Process(Reject)":                                                      //
  //=========================================================================//
  template   <FIX::DialectT::type  D, typename    Processor>
  inline void FIX_ConnectorSessMgr<D, Processor>::Process
  (
    FIX::Reject   const& a_tmsg,
    FIX::SessData const* DEBUG_ONLY(a_sess),
    utxx::time_val       a_ts_recv
  )
  {
    assert(a_sess == this && a_tmsg.m_MsgType == FIX::MsgT::Reject);
    CHECK_ONLY
    (
      if (utxx::unlikely(!CheckRxSN(a_tmsg, a_ts_recv)))
        return;
    )
    // XXX: This may be a VERY SERIOUS ERROR CONDITION implying some protocol
    // incompatibilties between ourselves and the Exchange (or something else,
    // eg Flood Control):
    char      reasonStr[512];
    char* p = reasonStr;
    (void) utxx::itoa(int(a_tmsg.m_SessionRejectReason), p);
    p = stpcpy(p, ": ");
    p = stpcpy(p, a_tmsg.m_Text);
    assert(size_t(p - reasonStr) < sizeof(reasonStr));

    LOG_ERROR(2,
      "FIX_ConnectorSessMgr::Process(Reject): REJECTED: RefSeqNum={}, "
      "RefTagID={}, RefMsgType={}, Reason={}",
      a_tmsg.m_RefSeqNum,  a_tmsg.m_RefTagID, a_tmsg.m_RefMsgType, reasonStr)

    // Invoke the Processor if it is an OrdMgmt Session; otherwise, we have an
    // outright failure:
    if (utxx::likely(IsOrdMgmt() && m_processor != nullptr))
      (void) m_processor->template ReqRejectedBySession<QT, QR>
            (m_processor, m_processor, true, a_tmsg.m_RefSeqNum, 0, reasonStr,
             a_tmsg.m_SendingTime, a_ts_recv);
    else
      // Terminate the Session -- something is completey wrong (FullStop=true):
      this->StopNow<true>
        ("FIX_ConnectorSessMgr::Process(Reject): No OrdMgmt", a_ts_recv);
  }

  //=========================================================================//
  // "Process(News)":                                                        //
  //=========================================================================//
  template   <FIX::DialectT::type  D, typename    Processor>
  inline void FIX_ConnectorSessMgr<D, Processor>::Process
  (
    FIX::News     const& a_tmsg,
    FIX::SessData const* DEBUG_ONLY(a_sess),
    utxx::time_val       CHECK_ONLY(a_ts_recv)
  )
  {
    assert(a_sess == this && a_tmsg.m_MsgType == FIX::MsgT::News);
    CHECK_ONLY
    (
      if (utxx::unlikely(!CheckRxSN(a_tmsg, a_ts_recv)))
        return;
    )

    LOG_INFO(1,
      "FIX_ConnectorSessMgr::Process(News): NEWS: Headline={}, "
      "{} lines of text:",
      a_tmsg.m_Headline,  a_tmsg.m_NumLinesOfText)

    for (int i = 0; i < a_tmsg.m_NumLinesOfText; ++i)
      LOG_INFO(1, "FIX_ConnectorSessMgr::Process(News): NEWS: {}",
        a_tmsg.m_LinesOfText[i])
  }

  //=========================================================================//
  // "Process(TradingSessionStatus)":                                        //
  //=========================================================================//
  // NB: This applies to both OrdMgmt and MktData Connectors:
  //
  template   <FIX::DialectT::type  D, typename    Processor>
  inline void FIX_ConnectorSessMgr<D, Processor>::Process
  (
    FIX::TradingSessionStatus const& a_tmsg,
    FIX::SessData             const* DEBUG_ONLY(a_sess),
    utxx::time_val                   a_ts_recv
  )
  {
    assert(a_sess == this &&
           a_tmsg.m_MsgType == FIX::MsgT::TradingSessionStatus);
    CHECK_ONLY
    (
      if (utxx::unlikely(!CheckRxSN(a_tmsg, a_ts_recv)))
        return;
    )
    FIX::TradSesStatusT tsst =  a_tmsg.m_TradSesStatus;
    if (utxx::unlikely (tsst == FIX::TradSesStatusT::UNDEFINED))
      // XXX: This may sometimes happen, eg for AlfaFIX2:
      return;

    // Otherwise: Really examine the Status:
    switch (tsst)
    {
      // NB: All of the following conditions indicate that trading is NOT acti-
      // ve (eg, we are not interested in "PreOpen" or "PreClose" modes):
      case FIX::TradSesStatusT::Halted:
      case FIX::TradSesStatusT::Closed:
      case FIX::TradSesStatusT::PreOpen:
      case FIX::TradSesStatusT::PreClose:
      case FIX::TradSesStatusT::GracefulDisconnectFromTS:
      case FIX::TradSesStatusT::RoughDisconnectFromTS:
        // Notify the Application-Level Subscribers (the event applies to all
        // SecIDs, hence 0): On = false:
        // NB: Here "TradingSessionID" may be important -- say a BlockTrades
        // session may be halted, but the main one goes ahead:
        if (utxx::likely(m_processor != nullptr))
          m_processor->ProcessStartStop
            (false, a_tmsg.m_TradingSessionID, 0,
             a_tmsg.m_SendingTime,    a_ts_recv);
        break;

      // The conditions below mean that trading is ACTIVE:
      case FIX::TradSesStatusT::Open:
      case FIX::TradSesStatusT::TSRestarted:
      case FIX::TradSesStatusT::ConnectedToTS:
      case FIX::TradSesStatusT::ReconnectedToTS:
        // First of all, if we are logging on and waiting for the initial "Tra-
        // dingSessionStatus", complete the logon:
        if (FIX::ProtocolFeatures<D>::s_waitForTrSessStatus &&
            TCPC::m_status == TCPC::StatusT::LoggingOn)
          LogOnCompleted(a_tmsg.m_SendingTime, a_ts_recv);
        else
        // Otherwise, notify the Application-Level Subscribers (On=true, for all
        // SecIDs) anyway -- but this will not change our status. Again, we must
        // use the TradingSessionID here:
        if (m_processor != nullptr)
          m_processor->ProcessStartStop
            (true,  a_tmsg.m_TradingSessionID, 0,
             a_tmsg.m_SendingTime, a_ts_recv);
        break;

      default:
        // UnSupported TradingSessionStatus, log it, but otherwise ignore:
        LOG_ERROR(2,
          "FIX_ConnectorSessMgr::Process(TradingSessionStatus): UnSupported "
          "Status={}", char(tsst))
    }
  }

  //=========================================================================//
  // "Process(OrderMassCancelReport)":                                       //
  //=========================================================================//
  // XXX: Currently, "MassCancelRequest" has no state in the State Mgr; so this
  // method (a Report on such a Request) is for logging only. This is why it is
  // processed at the Session Level:
  //
  template   <FIX::DialectT::type  D, typename    Processor>
  inline void FIX_ConnectorSessMgr<D, Processor>::Process
  (
    FIX::OrderMassCancelReport const& a_tmsg,
    FIX::SessData              const* DEBUG_ONLY(a_sess),
    utxx::time_val                    CHECK_ONLY(a_ts_recv)
  )
  {
    assert(a_sess == this &&
           a_tmsg.m_MsgType == FIX::MsgT::OrderMassCancelReport);
    CHECK_ONLY
    (
      if (utxx::unlikely(!CheckRxSN(a_tmsg, a_ts_recv)))
        return;
    )
    // A MassCancel failure is a serious error condition, so log it if it has
    // happened  -- but no more corrective action is performed. FIXME: How to
    // notify the Strategy about this failure?    Can MassCancel be issued by
    // OMC itself?
    bool wasError =
      (a_tmsg.m_MassCancelResponse  ==
       FIX::MassCancelRespT::CancelRequestRejected);

    if (utxx::unlikely(wasError))
      LOG_ERROR(1,
        "FIX_ConnectorSessMgr::Process(OrderMassCancelReport): MassCancel "
        "FAILED: ReqID={}, ReqType={}, RejReason={}: {}", a_tmsg.m_ClOrdID,
        char(a_tmsg.m_MassCancelRequestType),
        int (a_tmsg.m_MassCancelRejectReason), a_tmsg.m_Text)
  }

  //=========================================================================//
  // Utils:                                                                  //
  //=========================================================================//
  //=========================================================================//
  // "LogOnCompleted":  Wrapper around TCPC::LogOnCompleted:                 //
  //=========================================================================//
  template   <FIX::DialectT::type  D, typename    Processor>
  inline void FIX_ConnectorSessMgr<D, Processor>::LogOnCompleted
  (
    utxx::time_val  a_ts_exch,
    utxx::time_val  a_ts_recv
  )
  {
    // First, process this event at the "TCP_Connector" level:
    TCPC::LogOnCompleted();

    // At this stage, the over-all Session/Connector status becomes "Active":
    assert(IsActive());

    if (utxx::likely(m_processor != nullptr))
    {
      // Then notify the Application-Level Subscribers that we are now on-line:
      // ON=true, for all Sessions and SecIDs, with TimeStamps. For MDC it will
      // also Re-Subscribe the MktData (IsSubscr=true):
      if (IsMktData() && !FIX::ProtocolFeatures<D>::s_hasStreamingQuotes)
        m_processor->template SendMktDataSubscrReqs<true>();

      m_processor->ProcessStartStop(true, nullptr, 0, a_ts_exch, a_ts_recv);
    }
    LOG_INFO(1,
      "FIX_ConnectorSessMgr::LogOnCompleted: Session is now ACTIVE")
  }

  //=========================================================================//
  // "HardSeqNumError":                                                      //
  //=========================================================================//
  template   <FIX::DialectT::type  D, typename    Processor>
  inline void FIX_ConnectorSessMgr<D, Processor>::HardSeqNumError
  (
    char const*     a_err_msg,
    SeqNum          a_got_sn,
    FIX::MsgT       a_msg_type,
    utxx::time_val  a_ts_recv
  )
  {
    assert(a_err_msg != nullptr);

    // Log this event:
    if (utxx::likely(a_got_sn != 0))
    {
      // In this case, there is a mismatch between the Actual and Expected RxSN:
      LOG_ERROR(1,
        "FIX_ConnectorSessMgr::HardSeqNumError: MsgType={}: {}{}, ExpectedRxSN="
        "{}, STOPPING...",
        a_msg_type.to_string(), a_err_msg, a_got_sn, *FIX::SessData::m_rxSN)
    }
    else
    {
      // In this case, there is a range error fully specified by "ErrMsg":
      LOG_ERROR(1,
        "FIX_ConnectorSessMgr::HardSeqNumError: Invalid SeqReset: {}, "
        "STOPPING...", a_err_msg)
    }
    // Stop the Connector immediately. Still, we will attempt re-connection:
    // it is NOT a FullStop:
    this->StopNow<false>("HardSeqNumError", a_ts_recv);

    // Reset BOTH "TxSN" and "RxSN":
    *FIX::SessData::m_txSN = 1;
    *FIX::SessData::m_rxSN = 1;
  }

  //=========================================================================//
  // "CheckRxSN":                                                            //
  //=========================================================================//
  // Verifies the RxSN of the received msg. Returns "true" if OK, "false" if
  // the verification fails.  If an uncorrectable error is encountered, this
  // method invokes HardSeqNumError() -> StopNow() before returning:
  //
  template   <FIX::DialectT::type  D, typename    Processor>
  inline bool FIX_ConnectorSessMgr<D, Processor>::CheckRxSN
  (
    FIX::MsgPrefix const& a_prefix,
    utxx::time_val        a_ts_recv
  )
  {
    //-----------------------------------------------------------------------//
    // Generic -- and the most likely -- case: RxSN is OK:                   //
    //-----------------------------------------------------------------------//
    SeqNum currSN = a_prefix.m_MsgSeqNum;
    assert(currSN > 0);

    SeqNum&  RxSN = *FIX::SessData::m_rxSN;  // Ref!
    SeqNum&  TxSN = *FIX::SessData::m_txSN;  // Ref!

    if (utxx::likely(currSN == RxSN))
    {
      // Reset the Gap timer (in case it was active) -- no need for any Resend:
      FIX::SessData::m_resendReqTS = utxx::time_val();
      // Increment the Expected RxSN for the next msg:
      ++RxSN;
      return true;
    }

    //-----------------------------------------------------------------------//
    // Otherwise: Consider the following error conditions:                   //
    //-----------------------------------------------------------------------//
    if (utxx::likely(currSN > RxSN))
    {
      //---------------------------------------------------------------------//
      // RxSN Gap was encountered:                                           //
      //---------------------------------------------------------------------//
      // If this happened just in LogOn, never mind -- log the discrepancy and
      // adjust our RxSN without any other action:
      //
      if (utxx::likely(a_prefix.m_MsgType == FIX::MsgT::LogOn))
      {
        LOG_WARN(2,
          "FIX_ConnectorSessMgr::CheckRxSN(LogOn): Adjusted RxSN: Expected="
          "{}, Actual={}, New={}",  RxSN,  currSN, currSN + 1)

        RxSN = currSN + 1;       // Don't forget the increment!
        return true;
      }

      // Otherwise: It's a Gap!
      // If we are already in the "Gap Mode", we ignore this error for a short
      // while -- a "ResendRequest" was made to the Server, but before if was
      // processed, the Server could continue sending us out-of-seq msgs.   So
      // impose a time limit on that:
      //
      if (!FIX::SessData::m_resendReqTS.empty())
      {
        //-------------------------------------------------------------------//
        // Yes, already in the "Gap Mode":                                   //
        //-------------------------------------------------------------------//
        long gap_ms = (a_ts_recv - FIX::SessData::m_resendReqTS).milliseconds();

        if  (gap_ms > FIX::SessData::m_maxGapMSec)
          // Cannot wait for SeqNum recovery anymore:
          HardSeqNumError
            ("Could not recover from SeqNum Gap: CurrRxSN=", currSN,
             a_prefix.m_MsgType,  a_ts_recv);
        else
          // Otherwise, we simply drop this out-of-seq msg with a warning:
          LOG_WARN(2,
            "FIX_ConnectorSessMgr::CheckRxSN: Still in Gap: CurrRxSN={}, need"
            " {}: Msg Dropped", currSN, RxSN)
      }
      else
      {
        //-------------------------------------------------------------------//
        // Not in the "Gap Mode" yet -- enter it now:                        //
        //-------------------------------------------------------------------//
        LOG_WARN(2,
          "FIX_ConnectorSessMgr::CheckRxSN: Gap encountered: CurrRxSN={}, "
          "Expected={}: Will issue a ResendRequest", currSN, RxSN)

        // Issue a "ResendRequest" from the expected SeqNum to infinity (repre-
        // sented as 0). This is the PREFERRED way   (rather than specifying a
        // concrete upper bound -- the latter may be wrong  if the Server  has
        // already sent more out-of-seq msgs):
        //
        ProtoEngT::SendResendRequest(this, RxSN, 0);

        // From now on, the timing of the Gap will start (but no need for a Ti-
        // merFD -- see above):
        assert(!FIX::SessData::m_resendReqTS.empty());
      }
    }
    else
    {
      //---------------------------------------------------------------------//
      // Other way round: RxSN moved BackWards:                              //
      //---------------------------------------------------------------------//
      assert(currSN < RxSN);

      // This is normally NOT allowed, except in the follong cases:
      // (*) On LogOn: Even if we may have  requested  NOT to reset the SNs, the
      //     server may insist on re-setting them (and would then return SN=1).
      //     This is perfectly OK: consider this case first:
      //
      if (a_prefix.m_MsgType == FIX::MsgT::LogOn)
      {
        FIX::LogOn const& tmsg = static_cast<FIX::LogOn const&>(a_prefix);

        if (tmsg.m_ResetSeqNumFlag)
        {
          if (utxx::unlikely(currSN != 1))
          {
            // This would be very strange: The server requested a reset, but did
            // not reset their own SN: Consider this to be an error.   Will fall
            // through to an error case:
            LOG_ERROR(2,
              "FIX_ConnectorSessMgr::CheckRxSN(LogOn): ResetSeqNumFlag is ON "
              "but got SN={}", currSN)
          }
          else
          {
            // Reset our SeqNums: Set the straigt to 2, not to 1, because 1 is
            // presumably already taken by exchanging LogOns:
            LOG_INFO(2,
              "FIX_ConnectorSessMgr::CheckRxSN(LogOn): Forced RxSN=TxSN=2")
            RxSN = 2;
            TxSN = 2;
            return true;  // This is OK!
          }
        }
        // If ResetSeqNumFlag is not set, it is definitely a HardSeqNumError:
        // fall through...
      }
      else
      // (*) It is possible that multiple "ResendReqeusts" were issued, but we
      //     don't do a "ResendRequest" if we are already in a Gap Mode   (see
      //     above). If "PossDup" flag is set, we produce a warning and return
      //     "false", so this msg will be ignored:
      //
      if (utxx::likely(a_prefix.m_PossDupFlag))
      {
        LOG_WARN(2,
          "FIX_ConnectorSessMgr::CheckRxSN: Got stale msg with PossDup=Y: "
          "SeqNum={}, Expected={}, MsgType={}",
           currSN, RxSN, a_prefix.m_MsgType.to_string())
        return false;
      }

      // In all other cases, it's a serious error condition:
      HardSeqNumError
        ("Backward CurrRxSN=", currSN, a_prefix.m_MsgType, a_ts_recv);
    }
    //-----------------------------------------------------------------------//
    // If we got here: It was an error (Hard or otherwise):                  //
    //-----------------------------------------------------------------------//
    return false;
  }
} // End namespace MAQUETTE
