// vim:ts=2:et
//===========================================================================//
//               "Connectors/TWIME/EConnector_TWIME_FORTS.cpp":              //
//      Order Management Embedded Connector for the FORTS TWIME Protocol     //
//===========================================================================//
#include "Basis/TimeValUtils.hpp"
#include "Connectors/TCP_Connector.hpp"
#include "Connectors/TWIME/EConnector_TWIME_FORTS.h"
#include "Connectors/TWIME/Processor.hpp"
#include "Venues/FORTS/Configs_TWIME.hpp"
#include "Connectors/EConnector_OrdMgmt.hpp"
#include <utxx/compiler_hints.hpp>
#include <utxx/error.hpp>
#include <cstring>
#include <cassert>

using namespace std;

namespace MAQUETTE
{
  //=========================================================================//
  // "EConnector_TWIME_FORTS" Ctor:                                          //
  //=========================================================================//
  EConnector_TWIME_FORTS::EConnector_TWIME_FORTS
  (
    EPollReactor*                       a_reactor,
    SecDefsMgr*                         a_sec_defs_mgr,
    vector<string>              const*  a_only_symbols,
    RiskMgr*                            a_risk_mgr,
    boost::property_tree::ptree const&  a_params
  )
  : //-----------------------------------------------------------------------//
    // "EConnector": Virtual Base:                                           //
    //-----------------------------------------------------------------------//
    EConnector
    (
      a_params.get<std::string>("AccountKey"),  // Becomes "m_name"
      "FORTS",
      0,                        // XXX: No extra ShM data at the moment...
      a_reactor,
      false,                    // No BusyWait in TWIME
      a_sec_defs_mgr,
      FORTS::GetSecDefs_All     // All Fut and Opt Instrs:
        (EConnector::GetMQTEnv (a_params.get<string>("AccountKey"))),
      a_only_symbols,
      a_params.get<bool>       ("ExplSettlDatesOnly", true),
      FORTS::UseTenorsInSecIDs, // (false)
      a_risk_mgr,
      a_params,
      QT,
      false                     // !WithFracQtys
    ),
    //-----------------------------------------------------------------------//
    // "EConnector_OrdMgmt":                                                 //
    //-----------------------------------------------------------------------//
    // "AccountKey" can be used as a unique name for this instance of the TWIME
    // Connector, in the following format: {AccountPfx}-TWIME-FORTS-{Env}:
    //
    // Also note that although TWIME uses ExchOrdIDs in "Modify"  and "Cancel"
    // msgs, there is no need to maintain ExchOrdIDsMap to find Req12s in pro-
    // cessing the Resps:
    //
    EConnector_OrdMgmt
    (
      true,                     // Yes, OMC is Enabled!
      a_params,
      1,                        // Throttling Period: 1 sec
      GetTWIMEAccountConfig(EConnector::m_name).m_MaxReqsPerSec,
      PipeLineModeT::Wait1,     // Must wait: ExchOrdIDs are to be Sent
      true,                     // Yes, sending ExchOrdIDs in Cancel|Modify
      false,                    // No need for ExchOrdIDsMap on Receiving
      true,                     // Yes, TWIME has "atomic" "CancelReplace"
      true,                     // Yes, TWIME has PartFilledModify
      true,                     // Yes, TWIME has ExecIDs
      false                     // No   MktOrders in TWIME
    ),
    //-----------------------------------------------------------------------//
    // "TCP_Connector":                                                      //
    //-----------------------------------------------------------------------//
    TCPC
    (
      EConnector::m_name,       // Internal name
      GetTWIMESessionConfig(EConnector::m_name).m_MainIP,
      GetTWIMESessionConfig(EConnector::m_name).m_MainPort,
      128 * 1024,               // BuffSz  = 128k
      1024,                     // BuffLWM = 1k
      "",                       // XXX: ClientIP not provided
      a_params.get<int>   ("MaxConnAttempts", 5),
      GetTWIMESessionConfig(EConnector::m_name).m_LogOnTimeOutSec,
      GetTWIMESessionConfig(EConnector::m_name).m_LogOffTimeOutSec,
      GetTWIMESessionConfig(EConnector::m_name).m_ReConnectSec,
      GetTWIMESessionConfig(EConnector::m_name).m_HeartBeatSec,
      false,                    // TWIME does not use TLS
      "",                       // No TLS --  HostName not required
      IO::TLS_ALPN::UNDEFINED,  // No TWIME in ALPN anyway
      false,                    // No Server Certificate verification
      "",                       // No TLS --  no Server CA File
      "",                       // No TLS --  no ClientCertFile
      "",                       // Similarly, no ClientPrivKeyFile
      a_params.get<string>("ProtoLogFile", "")
    ),
    //-----------------------------------------------------------------------//
    // Now "EConnector_TWIME_FORTS"-specific flds:                           //
    //-----------------------------------------------------------------------//
    // Copy of the Session Config:
    m_sessConf     (GetTWIMESessionConfig(EConnector::m_name)),

    // Credentials:
    m_clientID
      (MkCredential(GetTWIMEAccountConfig(EConnector::m_name).m_ClientID)),
    m_account
      (MkCredential(GetTWIMEAccountConfig(EConnector::m_name).m_Account)),

    // Compatibility Flds:
    m_txSN            (m_TxSN),
    m_rxSN            (m_RxSN),

    // Time-Mgmt etc: XXX: To avoid occasional connection termonations  by the
    // TWIME server due to untimely receiving of HeartBeats, the actual period
    // to be used, is less than the declated one ny 1 sec:
    m_resetSeqNums    (a_params.get<bool>("ResetSeqNums")),
    m_heartBeatDecl   (unsigned(m_heartBeatSec * 1000)),
    m_heartBeatMSec   (max<unsigned> (m_heartBeatDecl, 2000) - 1000),
    m_termCode        (TWIME::TerminationCodeE::Finished), // Unless different
    m_recoveryMode    (false),
    m_reSendCountCurr (0),
    m_reSendCountTotal(0),

    // OutBuff: Initially empty:
    m_outBuff         (),
    m_outBuffLen      (0)
  {
    //-----------------------------------------------------------------------//
    // Checks:                                                               //
    //-----------------------------------------------------------------------//
    CHECK_ONLY
    (
      // Verify the HeartBeat Interval:
      if (utxx::unlikely(m_heartBeatDecl < 1000 || m_heartBeatDecl > 60000))
        throw utxx::badarg_error
              ("EConnector_TWIME_FORTS::Ctor: Invalid HeartBeatDecl=",
               m_heartBeatDecl);

      assert(1000 <= m_heartBeatMSec && m_heartBeatMSec <= 59000);

      // Verify the Account Length:
      if (utxx::unlikely(strnlen(m_account.data(), sizeof(m_account)) > 7))
        throw utxx::badarg_error
              ("EConnector_TWIME_FORTS::Ctor: Account Too Long "
               "(must be <= 7)");
    )
  }

  //=========================================================================//
  // Dtor:                                                                   //
  //=========================================================================//
  // At the moment, we only cancel the PusherTask Timer:
  //
  EConnector_TWIME_FORTS::~EConnector_TWIME_FORTS()  noexcept
  {
    try   { EConnector_OrdMgmt::RemovePusherTimer(); }
    catch (...) {}
  }

  //=========================================================================//
  // Start / Stop Mgmt:                                                      //
  //=========================================================================//
  //=========================================================================//
  // "Start":                                                                //
  //=========================================================================//
  //
  void EConnector_TWIME_FORTS::Start()
  {
    // Create a periodic PusherTask for "AllInds" (which are cleared here):
    EConnector_OrdMgmt::template CreatePusherTask
      <EConnector_TWIME_FORTS, EConnector_TWIME_FORTS>(this, this);

    // Initiate the TCP connection -- no msgs are sent as yet:
    TCPC::Start();
  }

  //=========================================================================//
  // "InitLogOn":                                                            //
  //=========================================================================//
  // Call-back invoked by the "TCP_Connector" once the TCP connection has been
  // established. Will send an "Establish" msg to the TWIME server:
  //
  inline void EConnector_TWIME_FORTS::InitLogOn()
    { SendEstablish(); }

  //=========================================================================//
  // "Stop*":                                                                //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // Graceful External / Virtual "Stop":                                     //
  //-------------------------------------------------------------------------//
  void EConnector_TWIME_FORTS::Stop()
  {
    // Try to FullStop the TCP_Connector. Normally, the Stop is graceful (not
    // immediate), but it could sometimes turn into immediate:
    //
    (void) Stop<true>(TWIME::TerminationCodeE::Finished);
  }

  //-------------------------------------------------------------------------//
  // Graceful Internal / Templated "Stop":                                   //
  //-------------------------------------------------------------------------//
  template<bool IsFull>
  inline typename   EConnector_TWIME_FORTS ::TCPC::StopResT
  EConnector_TWIME_FORTS::Stop(TWIME::TerminationCodeE a_tcode)
  {
    // Memoise the Termination Code:
    m_termCode = a_tcode;

    // Cancel the PusherTask Timer:
    EConnector_OrdMgmt::RemovePusherTimer();

    return TCPC::template Stop<IsFull>();
  }

  //-------------------------------------------------------------------------//
  // Non-Graceful Internal "StopNow":                                        //
  //-------------------------------------------------------------------------//
  // This is an immediate (non-graceful) stop; it may be FullStop or a restart-
  // able Stop:
  template<bool FullStop>
  inline void EConnector_TWIME_FORTS::StopNow
  (
    char const*     a_where,
    utxx::time_val  a_ts_recv   // RecvTime of the triggering event
  )
  {
    assert(a_where != nullptr);

    // Stop the TCP Client (this also provides logging):
    TCPC::template DisConnect<FullStop>(a_where);

    // The TCP Socket is now closed, propagate it to the SessData as well:
    assert(m_fd == -1 && IsInactive());

    // To be on a safe side, cancel the PusherTask Timer once again:
    EConnector_OrdMgmt::RemovePusherTimer();

    // Notify the Subscribers at the Application Level of this Stop:
    ProcessStartStop(false, nullptr, 0, a_ts_recv, a_ts_recv);

    LOG_INFO(1,
      "EConnector_TWIME_FORTS::StopNow: Connector STOPPED{}",
      FullStop ? "." : ", but will re-start")
  }

  //=========================================================================//
  // State Mgmt / Transitions:                                               //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "IsActive":                                                             //
  //-------------------------------------------------------------------------//
  bool EConnector_TWIME_FORTS::IsActive() const
    { return TCPC::IsActive();  }

  //-------------------------------------------------------------------------//
  // "InitGracefulStop":                                                     //
  //-------------------------------------------------------------------------//
  // Call-Backs to be used by "TCP_Connector": NB: it does NOT perform "LogOut"
  // yet:
  void EConnector_TWIME_FORTS::InitGracefulStop()
  {
    // Cancel all active orders (to be safe and polite; we may also have auto-
    // matic Cancel-on-Disconnect):
    CancelAllOrders(0, nullptr, FIX::SideT::UNDEFINED, nullptr);
  }

  //-------------------------------------------------------------------------//
  // "GracefulStopInProgress":                                               //
  //-------------------------------------------------------------------------//
  // Now really LoggingOut:
  void EConnector_TWIME_FORTS::GracefulStopInProgress()
  {
    m_termCode = TWIME::TerminationCodeE::Finished;
    SendTerminate();
  }

  //=========================================================================//
  // "GET_{SESS,APPL}_MSG" Macros:                                           //
  //=========================================================================//
  // (*) In case of MsgLen mismatch, we terminate the Session, because all other
  //     msgs are unlikely to be received correctly in this case;
  // (*) any exceptions in msg processing are caught locally (the mere presence
  //     of an exception handler is 0-cost), and the ReadingLoop continues:
  // (*) Application-Level Msgs (as opposed to Session-Level ones) also require
  //     SeqNums mgmt:
  // (*) IMPORTANT: "LogMsg" is to be invoked BEFORE "Process", even though the
  //     former causes some overhead (but in the fully-optimised mode, such log-
  //     ging would be disabled anyway); this is because "Process" may close the
  //     socket and destroy the curr buffer where "tmsg" points into!
  //
# ifdef  GET_SESS_MSG
# undef  GET_SESS_MSG
# endif

# define GET_SESS_MSG(MsgType)     \
    case TWIME::MsgType::s_id:     \
    { \
      TWIME::MsgType const* tmsg = \
        static_cast<TWIME::MsgType const*>(msgHdr); \
      CHECK_ONLY(LogMsg<false>(*tmsg);) \
      cont = Process(*tmsg, a_ts_recv); \
      break; \
    }

# ifdef  GET_APPL_MSG
# undef  GET_APPL_MSG
# endif

# define GET_APPL_MSG(MsgType)     \
    case TWIME::MsgType::s_id:     \
    { \
      TWIME::MsgType const* tmsg = \
        static_cast<TWIME::MsgType const*>(msgHdr); \
      CHECK_ONLY(LogMsg<false>(*tmsg);)             \
      bool cont1 = Process(*tmsg, a_ts_recv);       \
      bool cont2 = ManageApplSeqNums(TWIME::MsgType::s_id, a_ts_recv); \
      cont = cont1 && cont2;       \
      break; \
    }

  //=========================================================================//
  // "ReadHandler":                                                          //
  //=========================================================================//
  // HERE the TWIME msgs are received, and Processors dispatched.   NB: In FIX,
  // it is a part of the ProtocolEngine, because msg boundaries can only be de-
  // termined using  the Protocol-specific info. In TWIME, however, msg bounds
  // are easy to detect:
  //
  int EConnector_TWIME_FORTS::ReadHandler
  (
    int             DEBUG_ONLY(a_fd),
    char const*     a_buff,       // Points to dynamic buffer of Reactor
    int             a_size,
    utxx::time_val  a_ts_recv
  )
  {
    assert(a_fd >= 0 && a_fd == m_fd && a_buff != nullptr && a_size > 0);

    //-----------------------------------------------------------------------//
    // Get the Msgs (may be > 1) from the Byte Stream:                       //
    //-----------------------------------------------------------------------//
    char const* msgBegin = a_buff;
    char const* chunkEnd = a_buff + a_size;
    assert(msgBegin < chunkEnd);
    do
    {
      //---------------------------------------------------------------------//
      // Get the MsgHdr:                                                     //
      //---------------------------------------------------------------------//
      TWIME::MsgHdr const* msgHdr =
        reinterpret_cast<TWIME::MsgHdr const*>(msgBegin);

      // Verify the MsgHdr validity:
      CHECK_ONLY
      (
        if (utxx::unlikely
           (msgHdr->m_SchemaID != TWIME::SchemaID ||
            msgHdr->m_Version  != TWIME::SchemaVer))
        {
          // Terminate the curr session immediately -- our BuffPtr is probably
          // wrong -- but may re-start it later:
          LOG_ERROR(2,
            "EConnector_TWIME_FORTS::ReadHandler: Invalid MsgHdr: SchemaID={}, "
            "SchemaVer={}", msgHdr->m_SchemaID, msgHdr->m_Version)

          // Stop the Connector and further reading immediately. XXX: In dowing
          // so, we may lose some state info, but with this kind of error,   we
          // are unlikelyto be able to receive any further msgs, anyway:
          StopNow<false>("ReadHandler: MsgHdr Error", a_ts_recv);
          return -1;
        }
      )
      //---------------------------------------------------------------------//
      // Check the Msg Size:                                                 //
      //---------------------------------------------------------------------//
      // NB: BlockLength does NOT include the size of MsgHdr!
      //
      uint16_t tid   = msgHdr->m_TemplateID;
      uint16_t gotSz = uint16_t(msgHdr->m_BlockLength + sizeof(TWIME::MsgHdr));

      CHECK_ONLY
      (
        uint16_t expSz = TWIME::GetMsgSize(tid);

        if (utxx::unlikely(expSz != gotSz))
        {
          LOG_ERROR(2,
            "EConnector_TWIME_FORTS::ReadHandler: Invalid MsgLen for TID={} "
            "({}): Got={}, Expected={}: Stopping...",
            tid, TWIME::GetMsgTypeName(tid), gotSz, expSz)

          // Stop the Connector and further reading immediately. Again, sacri-
          // fice the state -- we cannot receive further msgs, anyway:
          StopNow<false>("ReadHandler: MsgLen Error", a_ts_recv);
          return -1;
        }

        // Also check that the complete msg has been received. If not, do not
        // try to process it:
        if (utxx::unlikely(msgBegin + gotSz > chunkEnd))
          break;
      )
      //---------------------------------------------------------------------//
      // De-Multiplex and Process the Msg:                                   //
      //---------------------------------------------------------------------//
      // NB: Some Msg Types are not expected to be received by the Client, so
      // they are NOT listed below:
      //
      bool cont = false;
      try
      {
        switch (tid)
        {
          // Session-Level Server Resps (NB: "Establish" and "ReTransmitRequest"
          // should not be received by the Client!):
          GET_SESS_MSG(EstablishmentAck)
          GET_SESS_MSG(EstablishmentReject)
          GET_SESS_MSG(Terminate)
          GET_SESS_MSG(ReTransmission)
          GET_SESS_MSG(Sequence)
          GET_SESS_MSG(FloodReject)
          GET_SESS_MSG(SessionReject)

          // Application-Level Server Responses:
          GET_APPL_MSG(NewOrderSingleResponse)
          GET_APPL_MSG(NewOrderMultiLegResponse)
          GET_APPL_MSG(NewOrderReject)
          GET_APPL_MSG(OrderCancelResponse)
          GET_APPL_MSG(OrderCancelReject)
          GET_APPL_MSG(OrderReplaceResponse)
          GET_APPL_MSG(OrderReplaceReject)
          GET_APPL_MSG(OrderMassCancelResponse)
          GET_APPL_MSG(ExecutionSingleReport)
          GET_APPL_MSG(ExecutionMultiLegReport)
          GET_APPL_MSG(EmptyBook)
          GET_APPL_MSG(SystemEvent)

        default:
          // Unrecognised TemplateID: Skip the msg but continue the ReadingLoop:
          LOG_WARN(2,
            "EConnector_TWIME_FORTS::ReadHandler: UnExpected TID={}: Msg "
            "Skipped", tid)
        }
      }
      catch (EPollReactor::ExitRun const&)
      { throw; }    // This exception is propagated
      catch (exception const& exc)
      {
        // Exceptions while processing msgs are caught, logged, but otherwise
        // ignored:
        LOG_ERROR(1,
          "EConnector_TWIME_FORTS::ReadHandler: Exception in MsgProcessor: "
          "TID={} ({}): {}", tid, TWIME::GetMsgTypeName(tid), exc.what())
      }

      //---------------------------------------------------------------------//
      // To the next Msg:                                                    //
      //---------------------------------------------------------------------//
      // IMPORTANT: Stop the reading loop immediately if any of the above call-
      // backs had closed the socket and Removed it from the Reactor.  In that
      // case, the Reactor-provided buffer is now destroyed, so  an attempt of
      // further reading from it will result in a segfault.  Also stop if any
      // of the above "Process" etc calls returned "false":
      //
      if (utxx::unlikely(!cont || IsInactive()))
        return -1;

      // If OK: go on:
      msgBegin += gotSz;
    }
    while (msgBegin < chunkEnd);

    //-----------------------------------------------------------------------//
    // Return the number of bytes consumed (succ read and processed):        //
    //-----------------------------------------------------------------------//
    int    consumed = int(msgBegin - a_buff);
    assert(consumed >= 0);
    return consumed;
  }

  //=========================================================================//
  // "ManageApplSeqNums":                                                    //
  //=========================================================================//
  bool EConnector_TWIME_FORTS::ManageApplSeqNums
  (
    uint16_t        DEBUG_ONLY(a_tid),
    utxx::time_val  a_ts_recv
  )
  {
    // This must indeed be an Appl-Level Response:
    assert(TWIME::IsApplRespMsg(a_tid));

    // It does not carry its RxSeqNum in itself -- SeqNum is implicit:
    SeqNum& RxSN = *m_RxSN;  // Ref!
    ++RxSN;

    // Are we receiving a re-transmission? Normally not:
    if (utxx::likely(m_reSendCountCurr == 0))
    {
      assert(m_reSendCountTotal == 0);
      return true;
    }

    // Yes: Waiting for re-transmissions to be completed. This  could be in the
    // RecoverMode (if need to re-transmit > 10 msgs), or in the normal on-line
    // mode:
    assert(0 < m_reSendCountCurr && m_reSendCountCurr <= MaxReSends &&
           m_reSendCountCurr     <= m_reSendCountTotal);

    // Thus, a re-transmission has just been received, so decrement both counts:
    --m_reSendCountCurr;
    --m_reSendCountTotal;

    if (m_reSendCountCurr == 0)
    {
      // The current "batch" of re-transmissions has been done; do we need to
      // run another one?
      if (m_reSendCountTotal == 0)
      {
        if (utxx::likely(!m_recoveryMode))
          // Re-transmissions done. If we are in the main on-line mode  (NOT in
          // the RecoveryMode), we have completed the LogOn process:
          //
          LogOnCompleted(a_ts_recv);
        else
        {
          // Otherwise, exit the RecoveryMode by terminating and re-starting the
          // Session (XXX: non-graceful termination as yet -- otherwise it would
          // complicate the state mgmt logic too much):
          //
          m_recoveryMode = false;
          TCPC::template DisConnect<true>("RecoveryMode Completed");

          // Restore the "main" connectivity params:
          InitFixedStr(const_cast<SymKey*>(&m_serverIP),  m_sessConf.m_MainIP);
          const_cast<int&>               (m_serverPort) = m_sessConf.m_MainPort;
          m_fullStop     = false;

          // Start a normal LogOn once again:
          Start();

          // Do NOT continue reading of the curr socket after "DisConnect":
          return false;
        }
      }
      else
      {
        // The curr batch of re-transmissions done, but more re-transmissions
        // are required. This can only happen in the RecoveryMode:
        assert(m_reSendCountTotal > 0 && m_recoveryMode);

        // Request another batch of re-transmissions:
        m_reSendCountCurr = m_reSendCountTotal;
        if (m_reSendCountCurr > MaxReSends)
          m_reSendCountCurr =   MaxReSends;

        SendReTransmitReq  ();
      }
    }
    // Otherwise, just continue to receive re-transmits:
    return true;
  }

  //=========================================================================//
  // "ServerInactivityTimeOut":                                              //
  //=========================================================================//
  void EConnector_TWIME_FORTS::ServerInactivityTimeOut
    (utxx::time_val a_ts_recv)
  {
    // Stop immediately, but will try to re-start. We sacrifice the state, but
    // the Server is not responsive anyway. XXX: More detailed logging?
    //
    StopNow<false>("ServerInactivityTimeOut", a_ts_recv);
  }
} // End namespace MAQUETTE
