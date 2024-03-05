// vim:ts=2:et
//===========================================================================//
//                      "Connectors/TWIME/Processor.hpp":                    //
//  "EConnector_TWIME_FORTS": Processing of Sess- and Appl-Level TWIME Msgs  //
//===========================================================================//
#pragma  once

#include "Basis/BaseTypes.hpp"
#include "Connectors/TWIME/EConnector_TWIME_FORTS.h"
#include "Protocols/TWIME/Msgs.h"
#include <utxx/convert.hpp>

namespace MAQUETTE
{
  //=========================================================================//
  // Session-Level TWIME Msgs:                                               //
  //=========================================================================//
  //=========================================================================//
  // "Process(EstablishmentAck)":                                            //
  //=========================================================================//
  inline bool EConnector_TWIME_FORTS::Process
  (
    TWIME::EstablishmentAck const& a_tmsg,
    utxx::time_val                 a_ts_recv
  )
  {
    //=======================================================================//
    // Check the HeartBeat Interval:                                         //
    //=======================================================================//
    // Presumably, the HeartBeat interval we get back in this msg, must be the
    // same as we have requisted. If it is not, we may get into trouble later,
    // so better disconnect now:
    CHECK_ONLY
    (
      if (utxx::unlikely(a_tmsg.m_KeepAliveInterval != m_heartBeatDecl))
      {
        LOG_ERROR(1,
          "EConnector_TWIME_FORTS::Process(EstablishmentAck): Got "
          "HeartBeatMSec={}, Requested={}: Better stop now...",
          a_tmsg.m_KeepAliveInterval, m_heartBeatDecl)

        // XXX: Stop immediately and without re-starting -- the problem needs
        // to be fixed off-line. As we are just logging on, no state loss hap-
        // pens:
        StopNow<true>
          ("EConnector_TWIME_FORTS::Process(EstablishmentAck): Invalid "
           "HeartBeat", a_ts_recv);
        return false;
      }
    )
    //=======================================================================//
    // RxSeqNum Mgmt:                                                        //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // In the Recovery Mode?                                                 //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely(m_recoveryMode))
    {
      LOG_INFO(2,
        "EConnector_TWIME_FORTS::Process(EstablishmentAck): LoggedOn in the "
        "RecoveryMode")

      // Cancel the LogOn TimeOut in the "TCP_Connector":
      TCPC::LogOnCompleted();

      // Just issue a "ReTransmitReq" and continue in the RecoveryMode:
      SendReTransmitReq();
      return true;
    }

    // Otherwise: Generic Case:
    // NB: SeqNums mgmt is limited to this method; in all other cases, we assume
    // that the reliable underlying TCP protocol makes SeqNums corruption impos-
    // sible; ie any discrepancies can only occur when we are disconnected.  So:
    //
    SeqNum  next =  SeqNum(a_tmsg.m_NextSeqNo); // NB: Can be -1 if empty!
    SeqNum& RxSN =  *m_RxSN;                    // Ref!
    assert (RxSN >= 0 && next >= 0);            // XXX: 0 is possible in TWIME?

    //-----------------------------------------------------------------------//
    // "Normal" Case:                                                        //
    //-----------------------------------------------------------------------//
    // XXX: We don't know for sure whether TWIME required RxSN to be initialised
    // (at the beginning of a trading session) to 0 or 1; as  EConnector_OrdMgmt
    // normally uses 1, we reset it to 0 if we got 0: this is not an error:
    //
    if (utxx::likely(RxSN == next || (RxSN == 1 && next == 0)))
    {
      RxSN = next;
      LogOnCompleted(a_ts_recv);
      return true;
    }

    //-----------------------------------------------------------------------//
    // Otherwise: If the "next" is LESS than expected: Serious Error Cond:   //
    //-----------------------------------------------------------------------//
    // (Similar to eg FIX):
    //
    CHECK_ONLY
    (
      if (utxx::unlikely(next < RxSN))
      {
        LOG_CRIT(1,
          "EConnector_TWIME_FORTS::Process(EstablishmentAck): Invalid backward "
          "RxSN: Expected={}, Got={}: Stopping Immediately!", RxSN, next)

        // There is probably no point in automatic re-connection: The result
        // will be the same:
        StopNow<true>
          ("EConnector_TWIME_FORTS::Process(EstablishmentAck): RxSN "
          "Backwardation", a_ts_recv);
        return false;
      }
    )
    //-----------------------------------------------------------------------//
    // Otherwise: "next" is GREATER than expected, ie a Gap was encountered: //
    //-----------------------------------------------------------------------//
    assert(RxSN < next);

    if (utxx::unlikely(m_resetSeqNums))
    {
      //---------------------------------------------------------------------//
      // First, "ResetSeqNums" mode: Just accept the Gap:                    //
      //---------------------------------------------------------------------//
      // NOT doing a re-transmission request -- but XXX this may result in los-
      // ing some order state info!
      LOG_WARN(2,
        "EConnector_TWIME_FORTS::Process(EstablishmentAck): RxSN gap ignored: "
        "Got={}, Expected={}", next, RxSN)

      RxSN = next;
      assert(RxSN >= 0);
      // The LogOn process is now considered to be complete:
      LogOnCompleted(a_ts_recv);
    }
    else
    {
      //---------------------------------------------------------------------//
      // Otherwise: Re-transmission(s) will be required:                     //
      //---------------------------------------------------------------------//
      m_reSendCountTotal  =   int(next - RxSN);
      m_reSendCountCurr   =   m_reSendCountTotal;

      // IMPORTANT: Depending on how many msgs really need to be re-transmitted,
      // we may do it "on-line", or we may have to go into full Recovery Mode:
      //
      if (utxx::likely(m_reSendCountTotal <= 10))
        // It's OK to do it by a single "ReTransmitReq". After re-transmission
        // is received,  we will be LoggedOn:
        SendReTransmitReq();
      else
      {
        //-------------------------------------------------------------------//
        // Otherwise: Will go into the RecoveryMode:                         //
        //-------------------------------------------------------------------//
        assert(!m_recoveryMode);
        m_recoveryMode = true;

        // Even in the off-line Recovery Mode, there is a limit on the number
        // msgs which can be re-transmitted in one req. XXX: Using a non-grac-
        // eful stop here:
        if (m_reSendCountCurr > MaxReSends)
          m_reSendCountCurr =   MaxReSends;

        // Close the curr Session at the TCP level -- the Recovery one will use
        // a different TCP connection!
        TCPC::template DisConnect<true>
          ("Process(EstablishmentAck: Going into the RecoveryMode");

        // XXX: Dirty Trick: Substitute the IP and Port in the "TCP_Connector"
        // by those used in the RecoveryMode:
        InitFixedStr
          (const_cast<SymKey*>(&m_serverIP),    m_sessConf.m_RecoveryIP);
        const_cast<int&>      ( m_serverPort) = m_sessConf.m_RecoveryPort;
        m_fullStop   = false;

        // Start again -- it will initiate a LogOn into the Recovery Server:
        Start();

        // But do NOT continue reading from the original socket -- it has been
        // closed by now (the "TCP_Connector" may not have noticed that, because
        // the FD may have been re-used -- so tell it explicitly!):
        return false;
      }
    }
    // If we got here, continue normally:
    return true;
  }

  //=========================================================================//
  // "LogOnCompleted": Wrapper around "TCPC::LogOnCompleted":                //
  //=========================================================================//
  inline void EConnector_TWIME_FORTS::LogOnCompleted(utxx::time_val a_ts_recv)
  {
    // NB: This only applies to the Main OnLine Mode, NOT to the Recovery Mode:
    assert(!m_recoveryMode);

    // Make state transition in the "TCP_Client":
    TCPC::LogOnCompleted();

    // Confirm that there is no need to re-send anything:
    m_reSendCountCurr  = 0;
    m_reSendCountTotal = 0;

    // Notify the Application-Level Subscribers that we are now on-line:
    // ON=true, for all Sessions and SecIDs, with TimeStamps :
    // XXX: "ExchTS" is not available -- using "RecvTS" twice:
    //
    ProcessStartStop(true, nullptr, 0, a_ts_recv, a_ts_recv);

    // At this stage, the over-all Connector status becomes "Active":
    assert(IsActive());

    LOG_INFO(1,
      "EConnector_TWIME_FORTS::LogOnCompleted: Connector is now ACTIVE")
  }

  //=========================================================================//
  // "Process(Sequence)":                                                    //
  //=========================================================================//
  // Processing a HeartBeat sent by the Server:
  //
  inline bool EConnector_TWIME_FORTS::Process
  (
    TWIME::Sequence const& CHECK_ONLY  (a_tmsg),
    utxx::time_val         UNUSED_PARAM(a_ts_recv)
  )
  {
    // Normally, "Sequence" msgs sent from the TWIME Server to the Client con-
    // tain a SeqNum which must be checked against our "RxSN". XXX: We expect
    // that they must match even if we are in the "re-transmission" mode:
    CHECK_ONLY
    (
      SeqNum next = SeqNum(a_tmsg.m_NextSeqNo);
      SeqNum RxSN = *m_RxSN;

      if (utxx::unlikely(next >= 0 && next != RxSN))
      {
        LOG_ERROR(2,
          "EConnector_TWIME_FORTS::Process(Sequence): Expected RxSN={}, Got={},"
          " TotalReTransmit={}, CurrReTransmit={}",
          RxSN, next, m_reSendCountTotal, m_reSendCountCurr)

        // This is a serious error condition, so we terminate the session -- but
        // gracefully, to prevent state loss (and we re-connect later):
        //
        (void) Stop<false>(TWIME::TerminationCodeE::InvalidSequenceNumber);
      }
    )
    // Still continue -- there was no immediate Stop yet:
    return true;
  }

  //=========================================================================//
  // "Process(ReTransmission)":                                              //
  //=========================================================================//
  inline bool EConnector_TWIME_FORTS::Process
  (
    TWIME::ReTransmission const& CHECK_ONLY(a_tmsg),
    utxx::time_val               CHECK_ONLY(a_ts_recv)
  )
  {
    // This msg serves as confirmation that it will be followed by a stream of
    // re-transmitted msgs.    Check that the re-transmission params are as we
    // have requested. Also, if we have received this msg, we should be in the
    // "LoggingOn" state:
    //
    CHECK_ONLY
    (
      SeqNum next  = SeqNum(a_tmsg.m_NextSeqNo);
      int    count = int   (a_tmsg.m_Count);
      SeqNum RxSN  = *m_RxSN;

      if (utxx::unlikely
         (next != RxSN    || count != m_reSendCountCurr  ||
         (!m_recoveryMode && m_status != TCP_Connector::StatusT::LoggingOn)))
      {
        LOG_ERROR(2,
          "EConnector_TWIME_FORTS::Process(ReTransmission): Expected RxSN={}, "
          "Got={}; Expected Count={}, Got={}; TotalCount={}, Status={}, "
          "RecoverMode={}",
          RxSN, next, m_reSendCountCurr, count, m_reSendCountTotal,
          m_status.to_string(), m_recoveryMode)

        // Again, this is considered to be a serious error  cond which warrants
        // immediate disconnection (but NOT a FullStop). As we *should* just be
        // logging on, there is normally no risk of state loss:
        StopNow<false>("Process(ReTransmission)", a_ts_recv);

        // Always stop reading after "StopNow":
        return false;
      }
    )
    // If we got here:
    return true;
  }

  //=========================================================================//
  // "Process(EstablishmentReject)":                                         //
  //=========================================================================//
  inline bool EConnector_TWIME_FORTS::Process
  (
    TWIME::EstablishmentReject const& a_tmsg,
    utxx::time_val                    a_ts_recv
  )
  {
    // TODO: Symbolic ErrMsgs:
    LOG_WARN(1,
      "EConnector_TWIME_FORTS::Process(EstablishmentReject): Connection "
      "Rejected: {}", TWIME::EstablishmentRejectCodeE::c_str
                      (a_tmsg.m_EstablishmentRejectCode))

    // Stop the Connector, but try to re-connect later. No risk of state loss:
    StopNow<false>("Process(EstablishmentReject)",   a_ts_recv);
    return false;
  }

  //=========================================================================//
  // "Process(Terminate)":                                                   //
  //=========================================================================//
  inline bool EConnector_TWIME_FORTS::Process
  (
    TWIME::Terminate const& a_tmsg,
    utxx::time_val          a_ts_recv
  )
  {
    // The Server has confirmed that we have logged out:
    TWIME::TerminationCodeE code = a_tmsg.m_TerminationCode;

    if (utxx::likely(code == TWIME::TerminationCodeE::Finished))
    {
      // Correct Termination:
      LOG_INFO(1,
        "EConnector_TWIME_FORTS::Process(Terminate): LogOut CONFIRMED")
    }
    else
    {
      // TODO: Symbolic ErrMsgs:
      LOG_WARN(1,
        "EConnector_TWIME_FORTS::Process(Terminate): Terminated Abnormally: "
        "{}", code.c_str())
    }
    // We can now FullStop the Connector. This will invoke internal notificat-
    // ions.  All state processing has already been done,  so stop fully  and
    // immediately:
    StopNow<true>("Process(Terminate)", a_ts_recv);
    return false;
  }

  //=========================================================================//
  // "Process(SessionReject)":                                               //
  //=========================================================================//
  inline bool EConnector_TWIME_FORTS::Process
  (
    TWIME::SessionReject const& a_tmsg,
    utxx::time_val              a_ts_recv
  )
  {
    // Log the error:
    char const* errMsg =
      TWIME::SessionRejectReasonE::c_str(a_tmsg.m_SessionRejectReason);
    assert(errMsg != nullptr);

    LOG_ERROR(2,
      "EConnector_TWIME_FORST::Process(SessionReject): REJECTED: ClOrdID={}, "
      "RefTagID={}: {}", a_tmsg.m_ClOrdID, a_tmsg.m_RefTagID, errMsg)

    // Invoke the OrderMgr. NB: Unlike FIX,  we do NOT need to search for the
    // failed msg by its SeqNum -- in TWIME, we get the ClOrdID instead:
    // BySeqNum = false:
    (void) ReqRejectedBySession<QT,QR>
        (this, this, false, long(a_tmsg.m_ClOrdID), 0, errMsg,
         a_ts_recv,  a_ts_recv);

    // Attempt a graceful session termination, but no re-start -- the problem
    // needs to be fixed off-line:
    (void) Stop<false>(TWIME::TerminationCodeE::UnspecifiedError);

    // Still continue reading:
    return true;
  }

  //=========================================================================//
  // "Process(FloodReject)":                                                 //
  //=========================================================================//
  inline bool EConnector_TWIME_FORTS::Process
  (
    TWIME::FloodReject const& a_tmsg,
    utxx::time_val            a_ts_recv
  )
  {
    // Log the error:
    LOG_ERROR(2,
      "EConnector_TWIME_FORTS::Process(FloodReject): THROTTLED: ClOrdID={}, "
      "QueueSz={}, PenaltyRemain={}",
      a_tmsg.m_ClOrdID, a_tmsg.m_QueueSize, a_tmsg.m_PenaltyRemain)

    // Invoke the OrderMgr (BySeqNum = false):
    (void) ReqRejectedBySession<QT,QR>
      (this, this, false, long(a_tmsg.m_ClOrdID), 0, "FloodReject",
       a_ts_recv,  a_ts_recv);

    // Because "EConnector_OrdMgmt" is supposed to provide  its onw throttling
    // and msg sending time, we should not get "FloodRejects" at all. So if we
    // got one, do a gracful stop (with a re-start):
    //
    (void) Stop<false>(TWIME::TerminationCodeE::TooFastClient);

    // Still continue reading:
    return true;
  }

  //=========================================================================//
  // Semi-Application-Level TWIME Msgs:                                      //
  //=========================================================================//
  //=========================================================================//
  // "Process(EmptyBook)":                                                   //
  //=========================================================================//
  inline bool EConnector_TWIME_FORTS::Process
  (
    TWIME::EmptyBook const& UNUSED_PARAM(a_tmsg),
    utxx::time_val          UNUSED_PARAM(a_ts_recv)
  )
  {
    // This means that the TradingSession has been closed. XXX: The exact mea-
    // ning of the TradingSessionID provided by this msg is unclear. In FORTS,
    // we assume that it applies to all Sessions and all SecIDs.   To be on a
    // safe side, perform a full graceful Stop:
    LOG_INFO(1,
      "EConnector_TWIME_FORTS::Process(EmptyBook): Stopping the Connector...")
    Stop();

    // Still continue reading:
    return true;
  }

  //=========================================================================//
  // "Process(SystemEvent)":                                                 //
  //=========================================================================//
  // This is somewhat similar to "EmptyBook". XXX: At the moment, EConnectors
  // have no suspension / resumption capability for the Clearing Periods,  so
  // they would result in Stopping the Connector:
  //
  inline bool EConnector_TWIME_FORTS::Process
  (
    TWIME::SystemEvent const& a_tmsg,
    utxx::time_val            UNUSED_PARAM(a_ts_recv)
  )
  {
    // Generic Case:
    TWIME::TradSesEventE event = a_tmsg.m_TradSesEvent;

    if (event == TWIME::TradSesEventE::IntradayClearingStarted ||
        event == TWIME::TradSesEventE::ClearingStarted)
    {
      LOG_INFO(1,
        "EConnector_TWIME_FORTS::Process(SystemEvent): {}: Stopping the "
        "Connector...", event.c_str())
      Stop();
    }
    // Other Event Types are ignored for now...
    // Still continue reading:
    return true;
  }

  //=========================================================================//
  // "Process(OrderMassCancelResponse)":                                     //
  //=========================================================================//
  // XXX: Currently, "MassCancelRequest" has no state in the State Mgr; so this
  // method (a Report on such a Request) is for logging only:
  //
  inline bool EConnector_TWIME_FORTS::Process
  (
    TWIME::OrderMassCancelResponse const& CHECK_ONLY  (a_tmsg),
    utxx::time_val                        UNUSED_PARAM(a_ts_recv)
  )
  {
    // A MassCancel failure is a serious error condition, so log it if it has
    // happened:
    CHECK_ONLY
    (
      if (utxx::unlikely(a_tmsg.m_OrdRejReason != 0))
      {
        LOG_ERROR(2,
          "EConnector_TWIME_FORTS::Process(OrderMassCancelResponse): Mass"
          "Cancel FAILED: ReqID={}, RejReason={}",
          a_tmsg.m_ClOrdID, TWIME::GetRejectReasonStr(a_tmsg.m_OrdRejReason))
      }
      else
      {
        LOG_INFO(2,
          "EConnector_TWIME_FORTS::Process(OrderMassCancelResponse): Cancelled "
          "{} Orders", a_tmsg.m_TotalAffectedOrders)
      }
    )
    // Continue reading:
    return true;
  }

  //=========================================================================//
  // Real Application-Level Msgs:                                            //
  //=========================================================================//
  // Flags corresponding to the Order Cancellation:
  constexpr int64_t CancelMask  =
    // Cancel  MassCancel CrossTrade   CancellOnDisconnect
    0x200000 | 0x400000 | 0x20000000 | 0x100000000;

  // Flag indicating Order Replacement:
  constexpr int64_t ReplaceMask = 0x100000;

  //=========================================================================//
  // "Process(NewOrderSingleResponse)":                                      //
  //=========================================================================//
  inline bool EConnector_TWIME_FORTS::Process
  (
    TWIME::NewOrderSingleResponse const& a_tmsg,
    utxx::time_val                       a_ts_recv
  )
  {
    //-----------------------------------------------------------------------//
    // The Order has been Confirmed. Invoke the "EConnector_OrdMgmt":        //
    //-----------------------------------------------------------------------//
    CHECK_ONLY(Req12 const& req =) OrderConfirmedNew<QT,QR>
    (
      this,            // ProtoEng
      this,            // SessMgr
      a_tmsg.m_ClOrdID,
      0,
      ExchOrdID(OrderID(a_tmsg.m_OrderID)),
      ExchOrdID(0),    // No MDEntryID -- currently no links between OMC and MDC
      PriceT   (double(a_tmsg.m_Price)),
      QtyN     (long  (a_tmsg.m_OrderQty)),
      TWIME::TimeStampToUTC(a_tmsg.m_TimeStamp),
      a_ts_recv
    );

    //-----------------------------------------------------------------------//
    // In the Debug Mode, do more checks:                                    //
    //-----------------------------------------------------------------------//
#   if !UNCHECKED_MODE
    if (utxx::unlikely(EConnector::m_debugLevel >= 2))
    {
      // The Flags must be consistent with the OrderType:
      assert(req.m_aos != nullptr);
      AOS const&   aos  = *req.m_aos;
      assert(aos.m_instr != nullptr && aos.m_strategy != nullptr);

      // The following order params MUST be exactly correct:
      SecID  gotSecID   = SecID  (a_tmsg.m_SecurityID);
      SecID  expSecID   = aos.m_instr->m_SecID;
      bool   gotIsBid   = (a_tmsg.m_Side == TWIME::SideE::Buy);
      bool   expIsBid   = aos.m_isBuy;
      PriceT gotPx        (double(a_tmsg.m_Price));
      PriceT expPx      = req.m_px;
      QtyN   gotQty       (long  (a_tmsg.m_OrderQty));
      QtyN   expQty     = req.GetQty<QT,QR>();
      int    gotHash    = a_tmsg .m_ClOrdLinkID;
      int    expHash    = int(aos.m_strategy->GetHash48() & Mask32);

      if (utxx::unlikely
         (gotSecID != expSecID || gotIsBid != expIsBid || gotPx != expPx ||
          gotQty   != expQty   || gotHash  != expHash))
        // Log an error:
        LOG_ERROR(2,
          "EConnector_TWIME_FORTS::Process(NewOrderSingleResponse): AOSID={},"
          " ReqID={}: Inconsistency : EXPECTED: SecID={}, IsBid={}, Px={}, "
          "Qty={}, Hash={}; GOT: SecID={}, IsBid={}, Px={}, Qty={}, Hash={}",
          aos.m_id, req.m_id,
          expSecID, expIsBid, double(expPx), QR(expQty), expHash,
          gotSecID, gotIsBid, double(gotPx), QR(gotQty), gotHash)

      // Other params are for info only:
      int64_t flags = a_tmsg.m_Flags;
      if (utxx::unlikely
         (bool(flags & CancelMask)  || bool(flags & ReplaceMask)         ||
          bool(flags & 0x04)        || // OTC???
          bool(flags & 0x01)        !=
            (aos.m_timeInForce      == FIX::TimeInForceT::Day)           ||
          bool(flags & 0x02)        !=
            (aos.m_timeInForce      == FIX::TimeInForceT::ImmedOrCancel) ||
          bool(flags & 0x00080000)  !=
          (aos.m_timeInForce      == FIX::TimeInForceT::FillOrKill)))
      {
        // Produce a warning:
        char  buff[64];
        char* out = buff;
        int  len  = utxx::itoa_hex(flags, out, sizeof(buff));
        buff[len] = '\0';
        LOG_WARN(2,
          "EConnector_TWIME_FORTS::Process(NewOrderSingleResponse): ReqID={},"
          " AOSID={}: Invalid Flags={}", req.m_id, aos.m_id, buff)
      }
    }
#   endif // !UNCHECKED_MODE
    // Continue reading:
    return true;
  }

  //=========================================================================//
  // "Process(NewOrderMultiLegResponse)":                                    //
  //=========================================================================//
  inline bool EConnector_TWIME_FORTS::Process
  (
    TWIME::NewOrderMultiLegResponse const& UNUSED_PARAM(a_tmsg),
    utxx::time_val                         UNUSED_PARAM(a_ts_recv)
  )
  {
    LOG_ERROR(2,
      "EConnector_TWIME_FORTS::Process(NewOrderMultiLegResponse): Multi-Legged"
      " Orders are currently not implemented")
    // Still continue reading:
    return true;
  }

  //=========================================================================//
  // "Process(NewOrderReject)":                                              //
  //=========================================================================//
  inline bool EConnector_TWIME_FORTS::Process
  (
    TWIME::NewOrderReject const& a_tmsg,
    utxx::time_val               a_ts_recv
  )
  {
    (void) OrderRejected<QT,QR>
    (
      this,             // ProtoEng
      this,             // SessMgr
      0,                // No SeqNum here (we don't have TxSNs in TWIME)
      a_tmsg.m_ClOrdID,
      0,
      FuzzyBool::True,  // It's New -- so we KNOW it does not exist anymore:w
      int(a_tmsg.m_OrdRejReason),
      TWIME::GetRejectReasonStr(a_tmsg.m_OrdRejReason),
      TWIME::TimeStampToUTC    (a_tmsg.m_TimeStamp),
      a_ts_recv
    );
    // Still continue reading:
    return true;
  }

  //=========================================================================//
  // "Process(OrderCancelResponse)":                                         //
  //=========================================================================//
  inline bool EConnector_TWIME_FORTS::Process
  (
    TWIME::OrderCancelResponse const& a_tmsg,
    utxx::time_val                    a_ts_recv
  )
  {
    //-----------------------------------------------------------------------//
    // The Order has been Cancelled. Invoke the "EConnector_OrdMgmt":        //
    //-----------------------------------------------------------------------//
    CHECK_ONLY(AOS const& aos =) OrderCancelled<QT,QR>
    (
      this,    // ProtoEng
      this,    // SessMgr
      // NB: ClOrdID is unavailable if the order was eg Cancelled-on-Disconnect:
      (a_tmsg.m_ClOrdID != TWIME::EmptyUInt64) ? a_tmsg.m_ClOrdID : 0,

      // The OrigClOrdID and AOSID are unavailable in any case -- "EConnector_
      // OrdMgmt" will have to use the above "ClOrdID" or "ExchID" to find the
      // target:
      0,
      0,
      ExchOrdID(OrderID(a_tmsg.m_OrderID)),

      // No MDEntryID yet (no MDC->OMC link), and no OrigPx info.  But we have
      // the Cancelled LeavesQty (XXX: Is it really a LeavesQty, not TotalQty?)
      ExchOrdID(0),
      PriceT(),
      QtyN  (long(a_tmsg.m_OrderQty)),

      // Time Stamps:
      TWIME::TimeStampToUTC    (a_tmsg.m_TimeStamp),
      a_ts_recv
    );

    //-----------------------------------------------------------------------//
    // In the Debug Mode, do more checks:                                    //
    //-----------------------------------------------------------------------//
    CHECK_ONLY
    (
      if (utxx::unlikely(EConnector::m_debugLevel >= 2))
      {
        // Check the Hash (it MUST match); XXX: we do not check the CancelledQty
        // here:
        int   gotHash = a_tmsg .m_ClOrdLinkID;
        int   expHash = int(aos.m_strategy->GetHash48() & Mask32);

        if (utxx::unlikely(gotHash != expHash))
          // Log an error:
          LOG_ERROR(2,
            "EConnector_TWIME_FORTS::Process(OrderCancelResponse): AOSID={}: "
            "Hash Inconsistency: Expected={}, Got={}",
            aos.m_id, expHash, gotHash)

        // Check the Flags:
        int64_t flags = a_tmsg.m_Flags;
        if (utxx::unlikely
           (!(bool(flags & CancelMask)) || bool(flags & ReplaceMask)))
        {
          // Produce a warning:
          char buff[64];
          char* out = buff;
          int  len  = utxx::itoa_hex(flags, out, sizeof(buff));
          buff[len] = '\0';
          LOG_WARN(2,
            "EConnector_TWIME_FORTS::Process(OrderCancelResponse): AOSID={}: "
            "Invalid Flags={}", aos.m_id, buff)
        }
      }
    )
    // Continue reading:
    return true;
  }

  //=========================================================================//
  // "Process(OrderCancelReject)":                                           //
  //=========================================================================//
  inline bool EConnector_TWIME_FORTS::Process
  (
    TWIME::OrderCancelReject    const& a_tmsg,
    utxx::time_val                     a_ts_recv
  )
  {
    int errCode = a_tmsg.m_OrdRejReason;

    (void) OrderCancelOrReplaceRejected<QT,QR>
    (
      this,                 // ProtoEng
      this,                 // SessMgr
      a_tmsg.m_ClOrdID,
      0,                    // Will have to find the OrigReq
      0,                    // AOSID is not available either
      nullptr,              // No ExchOrdID here
      FuzzyBool::UNDEFINED, // Not sure if it was a Fill
      FuzzyBool::True,      // Non-Existent: The only reason for Cancel fail?
      errCode,
      TWIME::GetRejectReasonStr(errCode),
      TWIME::TimeStampToUTC    (a_tmsg.m_TimeStamp),
      a_ts_recv
    );

    // Still continue reading:
    return true;
  }

  //=========================================================================//
  // "Process(OrderReplaceResponse)":                                        //
  //=========================================================================//
  inline bool EConnector_TWIME_FORTS::Process
  (
    TWIME::OrderReplaceResponse const& a_tmsg,
    utxx::time_val                     a_ts_recv
  )
  {
    //-----------------------------------------------------------------------//
    // The Order has been Replaced. Invoke the "EConnector_OrdMgmt":         //
    //-----------------------------------------------------------------------//
    CHECK_ONLY(Req12 const& req =) OrderReplaced<QT,QR>
    (
      this,                   // ProtoEng
      this,                   // SessMgr
      a_tmsg.m_ClOrdID,
      0,                      // Again, will have to find the OrigReq
      0,                      // No AOS either
      ExchOrdID(OrderID(a_tmsg.m_OrderID)),
      ExchOrdID(OrderID(a_tmsg.m_PrevOrderID)),
      ExchOrdID(0),           // No MDEntryID yet (MDC->OMC link)
      PriceT   (double(a_tmsg.m_Price)),
      QtyN     (long  (a_tmsg.m_OrderQty)),      // New LeavesQty
      TWIME::TimeStampToUTC(a_tmsg.m_TimeStamp),
      a_ts_recv
    );

    //-----------------------------------------------------------------------//
    // In the Debug Mode, do more checks:                                    //
    //-----------------------------------------------------------------------//
#   if !UNCHECKED_MODE
    if (utxx::unlikely(EConnector::m_debugLevel >= 2))
    {
      int64_t    flags    = a_tmsg.m_Flags;
      AOS const* aos      = req.m_aos;
      assert(aos != nullptr &&   aos->m_strategy != nullptr);

      PriceT     gotPx (double(a_tmsg.m_Price));
      PriceT     expPx      = req.m_px;
      QtyN       gotQty(long  (a_tmsg.m_OrderQty));
      QtyN       expQty     = req.GetQty<QT,QR> ();
      int        gotHash    = a_tmsg.m_ClOrdLinkID;
      int        expHash    = int(aos->m_strategy->GetHash48() & Mask32);

      // A Px, Qty or Hash mismatch triggers an exception:
      if (utxx::unlikely
         (gotPx != expPx || gotQty  != expQty || gotHash  != expHash))
        LOG_ERROR(2,
          "EConnector_TWIME_FORTS::Process(OrderReplaceResponse): AOSID={}, "
          "ReqID={}: Inconsistency: EXPECTED: Px={}, Qty={}, Hash={}; GOT: "
          "Px={}, Qty={}, Hash={}",
          aos->m_id,  req.m_id, double(expPx),
          QR(expQty), expHash,  double(gotPx), QR(gotQty), gotHash)

      // Also check Flags, but here we only produce a warning if it fails:
      if (utxx::unlikely
         (!(bool(flags & ReplaceMask)) || bool(flags & CancelMask)))
      {
        // Log an error but continue:
        char  buff[64];
        char* out = buff;
        int  len  = utxx::itoa_hex(flags, out, sizeof(buff));
        buff[len] = '\0';

        LOG_ERROR(2,
          "EConnector_TWIME_FORTS::Process(OrderReplaceResponse): "
          "Inconsistency: AOSID={}, ReqID={}: Flags={}", aos->m_id, req.m_id,
          buff)
      }
    }
#   endif // !UNCHECKED_MODE
    // Continue reading:
    return true;
  }

  //=========================================================================//
  // "Process(OrderReplaceReject)":                                          //
  //=========================================================================//
  inline bool EConnector_TWIME_FORTS::Process
  (
    TWIME::OrderReplaceReject const& a_tmsg,
    utxx::time_val                   a_ts_recv
  )
  {
    // Get the hint from the Protocol Layer if the order which we tried to Re-
    // place, still exists:
    int       errCode  = a_tmsg.m_OrdRejReason;
    FuzzyBool nonExist =
      (errCode == 14 || errCode == 50)
      ? FuzzyBool::True
      : FuzzyBool::UNDEFINED;

    (void) OrderCancelOrReplaceRejected<QT,QR>
    (
      this,                 // ProtoEng
      this,                 // SessMgr
      a_tmsg.m_ClOrdID,
      0,                    // Again, will have to find the OrigReq
      0,                    // Again, no AOSID
      nullptr,              // No ExchOrdID here
      FuzzyBool::UNDEFINED, // Don't know if Filled
      nonExist,
      errCode,
      TWIME::GetRejectReasonStr(errCode),
      TWIME::TimeStampToUTC    (a_tmsg.m_TimeStamp),
      a_ts_recv
    );

    // Still continue reading:
    return true;
  }

  //=========================================================================//
  // "Process(ExecutionSingleReport)":                                       //
  //=========================================================================//
  inline bool EConnector_TWIME_FORTS::Process
  (
    TWIME::ExecutionSingleReport const& a_tmsg,
    utxx::time_val                      a_ts_recv
  )
  {
    //-----------------------------------------------------------------------//
    // The Order has been Traded. Invoke the "EConnector_OrdMgmt":           //
    //-----------------------------------------------------------------------//
    QtyN leavesQty(long(a_tmsg.m_OrderQty));

    FIX::SideT ourSide  =
      (a_tmsg.m_Side == TWIME::SideE::Buy)
      ? FIX::SideT::Buy  :
      (a_tmsg.m_Side == TWIME::SideE::Sell)
      ? FIX::SideT::Sell
      : FIX::SideT::UNDEFINED;

    FuzzyBool completeFill =
      IsZero(leavesQty) ? FuzzyBool::True : FuzzyBool::False;

    CHECK_ONLY(Trade const* tr =) OrderTraded<QT, QR, QT, QR>
    (                               // Not from Cancel/Modify Failure!
      this,                         // ProtoEng
      this,                         // SessionMgr
      nullptr,                      // Not from an MDC!
      a_tmsg.m_ClOrdID,
      0,
      nullptr,                      // Req12 not known yet
      ourSide,                      // Side of our order
      FIX::SideT::UNDEFINED,        // Aggressor is not known
      ExchOrdID(OrderID(a_tmsg.m_OrderID)),
      ExchOrdID(0),                 // No MDEntryID (MDC->OMC link) yet...
      ExchOrdID(OrderID(a_tmsg.m_TrdMatchID)),
      PriceT   (),                  // The OrigPx is not available
      PriceT   (double(a_tmsg.m_LastPx) ),
      QtyN     (long  (a_tmsg.m_LastQty)),
      leavesQty,
      QtyN(),                       // CumQty    is not known, 0 is OK
      QtyN::Invalid(),              // FIXME: Install real Fees data instead!
      0,                            // SettlDate is not known
      completeFill,
      TWIME::TimeStampToUTC(a_tmsg.m_TimeStamp),
      a_ts_recv
    );

    //-----------------------------------------------------------------------//
    // Some Checks:                                                          //
    //-----------------------------------------------------------------------//
    CHECK_ONLY
    (
      // NB: "req" may theoretically be NULL if this Trade was ignored by the
      // Order State Mgr for any reason, but normally it would be non-NULL:
      //
      if (tr != nullptr && EConnector::m_debugLevel >= 2)
      {
        Req12 const* req = tr->m_ourReq;
        assert(req != nullptr);
        AOS   const* aos = req->m_aos;
        assert(aos != nullptr &&  aos->m_instr != nullptr &&
               aos->m_strategy != nullptr);

        SecID gotSecID = SecID(a_tmsg.m_SecurityID);
        SecID expSecID = aos->m_instr->m_SecID;
        bool  gotIsBid = (a_tmsg.m_Side == TWIME::SideE::Buy);
        bool  expIsBid = aos->m_isBuy;
        int   gotHash  = a_tmsg.m_ClOrdLinkID;
        int   expHash  = int (aos->m_strategy->GetHash48() & Mask32);

        // TODO: More stringint checks (eg wrt Px and Qty) need to be done!
        if (utxx::unlikely
           (gotSecID != expSecID || gotIsBid != expIsBid || gotHash != expHash))
          LOG_ERROR(2,
            "EConnector_TWIME_FORTS::Process(ExecutionSingleReport): "
            "Inconsistency: AOSID={}, ReqID={}: EXPECTED: SecID={}, IsBid={}, "
            "Hash={}; GOT: SecID={}, IsBid={}, Hash={}",
            aos->m_id, req->m_id, expSecID, expIsBid,
            expHash,   gotSecID,  gotIsBid, gotHash)
      }
    )
    // Continue reading:
    return true;
  }

  //=========================================================================//
  // "Process(ExecutionMultyiLegReport)":                                    //
  //=========================================================================//
  inline bool EConnector_TWIME_FORTS::Process
  (
    TWIME::ExecutionMultiLegReport const& UNUSED_PARAM(a_tmsg),
    utxx::time_val                        UNUSED_PARAM(a_ts_recv)
  )
  {
    LOG_ERROR(2,
      "EConnector_TWIME_FORTS::Process(ExecutionMultiLegReport): Multi-"
      "Legged Orders are currently not implemented")
    // Still continue reading:
    return true;
  }
} // End namespace MAQUETTE
