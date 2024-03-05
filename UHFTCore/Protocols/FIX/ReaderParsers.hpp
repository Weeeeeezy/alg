// vim:ts=2:et
//===========================================================================//
//                      "Protocols/FIX/ReaderParsers.hpp":                   //
//            FIX Protocol Engine: Reading and Parsing of FIX Msgs           //
//===========================================================================//
#pragma once

#include "Protocols/FIX/Features.h"
#include "Protocols/FIX/Msgs.h"
#include "Protocols/FIX/ProtoEngine.h"
#include "Protocols/FIX/SessData.hpp"
#include "Protocols/FIX/UtilsMacros.hpp"
#include <utxx/compiler_hints.hpp>
#include <utxx/error.hpp>
#include <typeinfo>
#include <stdexcept>
#include <cstring>
#include <cassert>
#include <utxx/time_val.hpp>

namespace MAQUETTE
{
namespace FIX
{
  //=========================================================================//
  // "ProtoEngine": Non-Default Ctor:                                        //
  //=========================================================================//
  template
  <
    DialectT::type  D,
    bool            IsServer,
    typename        SessMgr,
    typename        Processor
  >
  inline ProtoEngine<D, IsServer, SessMgr, Processor>::ProtoEngine
  (
    SessMgr*        a_sess_mgr,
    Processor*      a_processor
  )
  : m_sessMgr       (a_sess_mgr),
    m_processor     (a_processor),
    //
    // All Typed FIX Msg Buffers use their Default Ctors...
    //
    m_reqIDPfxSz    (0),       // Initialised below
    m_currMsg       (),
    m_currMsgLen    (0),
    m_outBuff       (),
    m_outBuffLen    (0)
  {
    // NB:  Reactor and Processor are allowed to be NULL;  SessMgr and Logger
    // must be non-NULL (in particular, SessMgr cannot be NULL, because other-
    // wise, it would not be possible to get the CurrSession!):
    CHECK_ONLY
    (
      if (utxx::unlikely(m_sessMgr == nullptr))
        throw utxx::badarg_error("FIX::ProtoEngine::Ctor: Missing the SessMgr");
    )
    // Initial Clean-Up of Msg Buffers:
    memset(m_currMsg, '\0', sizeof(m_currMsg));
    memset(m_outBuff, '\0', sizeof(m_outBuff));

    //-----------------------------------------------------------------------//
    // Client-Side Settings:                                                 //
    //-----------------------------------------------------------------------//
    if constexpr (!IsServer)
    {
      // Memoise the ReqID Prefix Size. This is to allow the ReqID pfxes  to be
      // stripped efficiently while processing incoming msgs. For that, we need
      // the CurrSess -- on the Client-Side, it is statically-configured in the
      // SessMgr, so should be available already in the Ctor. The TCP socket is
      // (-1) yet:
      SessData* currSess = m_sessMgr->GetFIXSession(-1);
      CHECK_ONLY
      (
        if (utxx::unlikely(currSess == nullptr))
          throw utxx::logic_error
                ("FIX::ProtoEngine::Ctor: Cannot get the SessData");
      )
      if (ProtocolFeatures<D>::s_usePartiesInNew &&
         !IsEmpty(currSess->m_partyID))
        // Prefix will contain "PartyID//":
        m_reqIDPfxSz += (Length(currSess->m_partyID) + 2);

      if constexpr (ProtocolFeatures<D>::s_useCurrDateInReqID)
        // Prefix will contain "YYYYMMDD-":
        m_reqIDPfxSz += 9;
    }
  }

  //=========================================================================//
  // "ProtoEngine::ReadHandler":                                             //
  //=========================================================================//
  // NB: If this method returns (-1), it means that:
  // (*) the reading loop (inside "ReadHandler") is aborted -- either because
  //     the data were mal-formatted, or because the socket was closed and Re-
  //     moved from the Reactor, and its input buffer destroyed;
  // (*) "ReadUntilEAgain" (invoked from the Reactor) will, on receiving the
  //     returns value of (-1), stop further reading of data from the socket
  //     into the buffer (in particular because the latter may not even exist
  //     then);
  // (*) in addition, if the socket was Removed from the Reactor, there  is a
  //     2nd line of defense based on "FDInfo::m_inst_id" to stop reading, in
  //     case (-1) was not properly returned:
  //
  template
  <
    DialectT::type D,
    bool           IsServer,
    typename       SessMgr,
    typename       Processor
  >
  inline int ProtoEngine<D, IsServer, SessMgr, Processor>::ReadHandler
  (
    int             a_fd,
    char const*     a_buff,       // Points to dynamic buffer of Reactor
    int             a_size,
    utxx::time_val  a_ts_recv
  )
  {
    utxx::time_val  handlTS = utxx::now_utc();
    assert(a_fd >= 0 && a_buff != nullptr && a_size > 0);
    m_last_real_fd = a_fd;

    //-----------------------------------------------------------------------//
    // Get the Session from "a_fd":                                          //
    //-----------------------------------------------------------------------//
    SessData* currSess = m_sessMgr->GetFIXSession(a_fd);

    // On the Server-Side, it is possible that "currSess" is not yet known (eg
    // we are just receiving the LogOn msg); on the Client-Side,  there should
    // always be a unique "currSess":
    CHECK_ONLY
    (
      if (utxx::unlikely(!IsServer && currSess == nullptr))
        throw utxx::logic_error
              ("FIX::ProtoEngine::ReadHandler: No CurrSess for Client-Side");
    )
    //-----------------------------------------------------------------------//
    // Get the Msgs from the Byte Stream:                                    //
    //-----------------------------------------------------------------------//
    // XXX: We ALWAYS try to find the FIX msg delimiter and "consume" the msg,
    // even if (very unlikely) we are now in the Inactive status. We therefore
    // maintain the invariant that "a_buff" is always at msg start.
    // There could be more than 1 complete FIX msg in the buffer,   hence the
    // following loop:
    // XXX: For efficiency, we will have to update the "a_buff" in-place, so use
    // a non-const ptr:
    char*       msgBegin = const_cast<char*>(a_buff);
    char const* chunkEnd = a_buff + a_size;
    do
    {
      // For extra safety, always reset "m_currMsg" every time at this point;
      // when it is finally filled in, it will be processed straight away:
      *m_currMsg   = '\0';
      m_currMsgLen = 0;

      //---------------------------------------------------------------------//
      // Prefix: eg "8=FIX.M.N|9=XXXX|"...                                   //
      //---------------------------------------------------------------------//
      // Enough bytes to get at least the msg length field (9=XXXX)? -- We thus
      // need to read that many bytes at the beginning of the msg:
      //
      constexpr int prefixSz =
        ConstStrLen(MkPrefixStr(ProtocolFeatures<D>::s_fixVersionNum)) +
            (D == DialectT::TT ? 8 : 7);

      if (utxx::unlikely(msgBegin + prefixSz >= chunkEnd))
        // The msg is certainly incomplete: there is no room left for the pfx,
        // let alone for the Body and CheckSum. Can't do anything more in this
        // invocation; leave the existing data in the buffer and wait for more
        // data:
        break;

      // Otherwise: Get the length of the next msg:
      char* f9 = static_cast<char*>(memmem(msgBegin, prefixSz, SOH "9=", 3));
      if (utxx::unlikely(f9  == nullptr))
      {
        // The input is seriously malformed  -- cannot get the length tag! This
        // probably means that we can't correctly receive  the subsequent  msgs
        // either, so terminate the session (Graceful=true):
        m_sessMgr->template TerminateSession<true>
          (a_fd, currSess, "MsgTag9Error", a_ts_recv);
        return -1;    // Stop further reading immediately
      }

      // Otherwise: Length starts at (f9 + 3),  and lasts until the end of the
      // digits-only sequence, but no more than 4 bytes   (XXX: we assume that
      // XXXX < MaxMsgSize = 8192).  NB: "msgBody" is not a "const" because it
      // is used in GET_MSG calls;   TillEOL=false:
      int   bodyLen = 0;
      char* msgBody =
        const_cast<char*>(utxx::fast_atoi<int, false>(f9 + 3,
            f9 + (D == DialectT::TT ? 8 : 7), bodyLen));

      // NB: "msgBody" should now point to the SOH before the assumed "35=",  so
      // it is 1 byte short of the real Msg Body -- but we need to do the checks
      // first:
      if (utxx::unlikely
         (msgBody == nullptr || *msgBody != soh || bodyLen <= 0))
      {
        // Again, the input is malformed -- no SOH after length, or invalid len;
        // cannot do anything, so terminate the session (Graceful=true):
        m_sessMgr->template TerminateSession<true>
          (a_fd, currSess, "MsgLenError", a_ts_recv);
        return -1;    // Stop further reading immediately
      }

      // Now, advance "msgBody" to point to the assumed 35=... (though we don't
      // check that yet), ie to the real beginning of the Msg Body:
      msgBody++;
      int bodyOff = int(msgBody - msgBegin);

      // NB: "bodyLen" is the declared msg body length  (from "35=" through the
      // SOH preceding the "10=CCC|" terminating check-sum);  thus, "throughLen"
      // is "bodyLen" plus 7 bytes (but both lengthes do NOT contain the len of
      // the "8=...|9=...|" prefix; for the latter, use "totalLen"):
      //
      int   throughLen = bodyLen + 7;
      char* msgEnd     = msgBody + throughLen;

      if (utxx::unlikely(msgEnd > chunkEnd))
        // Still do not have the complete msg in the Buffer:
        break;

      //---------------------------------------------------------------------//
      // Otherwise: Yes, can get the complete FIX msg:                       //
      //---------------------------------------------------------------------//
      assert(msgEnd <= chunkEnd);

      // Copy the msg just framed into "m_currMsg".  This incurs a minor per-
      // formance overhead, but greatly enhances the safety of the following-
      // on parsing and processing, because in come cases, the socket can be
      // closed during processing, and the current buff destroyed!
      //
      int totalLen = int(msgEnd - msgBegin);   // The whole msg
      assert(size_t(totalLen) <=  sizeof(m_currMsg));

      m_currMsgLen = size_t(totalLen);
      memcpy(m_currMsg, msgBegin, m_currMsgLen);

      // Log the original msg -- it gets modified while doing so, but we don't
      // need it anymore: IsSend=false:
      LogMsg<false>(msgBegin, totalLen);

      //---------------------------------------------------------------------//
      // Prepare for the next iteration:                                     //
      //---------------------------------------------------------------------//
      // NB:  "msgBegin"  and "msgEnd" are NOT used in the rest of the curr
      // iteration, so can be updated now:
      //
      msgBegin      = msgEnd;
      msgBody       = m_currMsg + bodyOff;
      char* bodyEnd = msgBody   + bodyLen;

      //---------------------------------------------------------------------//
      // The Curr Msg has been framed:                                       //
      //---------------------------------------------------------------------//
      // So from now on, some errors can be tolerated -- we could handle them
      // and skip to the next msg:
      CHECK_ONLY
      (
        // Check the msg terminators:
        char* copyEnd = m_currMsg + totalLen;
        if (utxx::unlikely(*(copyEnd-1) != soh || *(bodyEnd-1) != soh))
        {
          // NB: SeqNum and MsgType are not known yet:
          if (utxx::likely
             (TryRejectMsg
             (a_fd, currSess, MsgT::UNDEFINED,  0,
              SessRejReasonT::IncorrectDataFormat, "InvalidMsgTerminator",
              a_ts_recv)))
            continue;
          else
            return -1;  // Stop further reading immediately
        }

        // Now verify the Msg CheckSum:
        // (From "m_currMsg" to "bodyEnd", ie obviously not including the Check-
        // Sum fld itself!):
        //
        int clcCS = MsgCheckSum(m_currMsg, bodyEnd);
        int embCS = 0;

        // NB: "bodyEnd" must point to "10=", so read the check-sum until SOH
        // (NOT until EOL, so TillEOL=false):
        if (utxx::unlikely
           (utxx::fast_atoi<int, false>(bodyEnd + 3, copyEnd, embCS) == nullptr
            || clcCS != embCS))
        {
          // NB: SeqNum and MsgType are not known yet:
          if (utxx::likely
             (TryRejectMsg
             (a_fd, currSess,  MsgT::UNDEFINED, 0,
              SessRejReasonT::SignatureProblem, "InvalidMsgCheckSum",
              a_ts_recv)))
            continue;
          else
            return -1;  // Stop further reading immediately
        }
      )
      //---------------------------------------------------------------------//
      // Get the Msg Type:                                                   //
      //---------------------------------------------------------------------//
      // NB: "msgBody" must indeed point to "35=". In our case, all Msg Types
      // are of 1 or 2 chars only, so:
      bool   msgTypeLen1 = (msgBody[4] == soh);
      CHECK_ONLY
      (
        bool msgTypeLen2 = !msgTypeLen1 && (msgBody[5] == soh);

        if (utxx::unlikely
           (strncmp(msgBody, "35=", 3) != 0 || !(msgTypeLen1 || msgTypeLen2)))
        {
          // NB: SeqNum and MsgType are not known yet:
          if (utxx::likely
             (TryRejectMsg
             (a_fd, currSess, MsgT::UNDEFINED, 0,
              SessRejReasonT::RequiredTagMissing, "MissingMsgTypeTag",
              a_ts_recv)))
            continue;
          else
            return -1;  // Stop further reading immediately
        }
      )
      //---------------------------------------------------------------------//
      // If MsgType Tag is OK: Get the actual "msgBody" start:               //
      //---------------------------------------------------------------------//
      MsgT msgType = MsgT::UNDEFINED;

      if (utxx::likely(msgTypeLen1))
      {
         msgType  = MsgT(int(LiteralEnum(msgBody[3])));
         msgBody += 5;
      }
      else
      {
        assert(msgTypeLen2);
        msgType   = MsgT(int(LiteralEnum(msgBody[3], msgBody[4])));
        msgBody  += 6;
      }
      // The common base obj of all Msgs, not known yet (depends on MsgType):
      MsgPrefix* msgPrefix = nullptr;

      //---------------------------------------------------------------------//
      // Go by Msg Type:                                                     //
      //---------------------------------------------------------------------//
      // NB: All exceptions are caught HERE!  If we do that, we can correctly
      // continue with the next msg; otherwise, the exception would be caught
      // in the Main Reactor Loop, and most likely leave the buffer in an in-
      // consistent state:
      // NB: "Parse*" and "Process*"  methods for all Msg Types  are  invoked
      // HERE!
      try
      {
        msgPrefix =
          GetMsg(msgType, msgBody,   bodyEnd,  totalLen,  currSess,
                 a_fd,    a_ts_recv, handlTS);

        if (utxx::unlikely(msgPrefix == nullptr))
        {
          // Unrecognised MsgType:
          // NB: "?\0" absorbes a 1- or 2-char MsgT:
          char  errMsg[] = "Unrecognised MsgType=?\0";
          char* curr     = errMsg + 21;
          CHAR2_VAL(msgType)

          // NB: SeqNum may not be known yet:
          if (utxx::likely
             (TryRejectMsg
             (a_fd,   currSess, msgType, 0, SessRejReasonT::InvalidMsgType,
              errMsg, a_ts_recv)))
            continue;
          else
            return -1;  // Stop further reading immediately
        }
      }
      //---------------------------------------------------------------------//
      // Exception Handling:                                                 //
      //---------------------------------------------------------------------//
      //---------------------------------------------------------------------//
      // "ExitRun":                                                          //
      //---------------------------------------------------------------------//
      catch (EPollReactor::ExitRun const&)
      {
        // This exception is always propagated (though in case of "Acceptor",
        // it is NOT advisable to throw it at all!):
        throw;
      }
      //---------------------------------------------------------------------//
      // All other "exception"s:                                             //
      //---------------------------------------------------------------------//
      catch (std::exception const& exc)
      {
        if constexpr (IsServer)
        {
          // FIX Server: Reject this msg but still go ahead with the curr Sessn
          // if we can:
          char   errMsg[512];
          char*  curr = stpcpy(errMsg, "Error in MsgType=");
          CHAR2_VAL(msgType)
          curr = stpcpy(curr, ": ");
          curr = stpcpy(curr, exc.what());
          assert(size_t(curr - errMsg) < sizeof(errMsg));

          // Is is a "logic_error"?
          if (utxx::unlikely(typeid(exc) == typeid(utxx::logic_error) ||
                            (typeid(exc) == typeid(std::logic_error)) ))
          {
            // It is considered to be a very serious error cond. Probably  no
            // point in graceful termination, even if "currSess" is available:
            stpcpy(curr, ": LogicError");
            assert(size_t(curr - errMsg) < sizeof(errMsg));

            // Graceful=false:
            m_sessMgr->template TerminateSession<false>
              (a_fd, currSess, errMsg, a_ts_recv);
            return -1;  // Stop further reading immediately!
          }
          else
          {
            // Any other exception: Try to reject the curr msg:
            // Get the SeqNum of the faild msg  (if already parsed; otherwise,
            // "RejectMsg" will scan "m_currMsg" for it):
            SeqNum failedSN =
              (msgPrefix != nullptr) ? msgPrefix->m_MsgSeqNum : 0;

            // XXX: We don't know what exactly has happened -- use "Incorrecti-
            // DataFormat" as a generic error code:
            if (utxx::likely
               (TryRejectMsg
               (a_fd, currSess,msgType, failedSN,
                SessRejReasonT::IncorrectDataFormat, errMsg, a_ts_recv)))
              continue;
            else
              return -1;  // STop further reading immediately!
          }
        }
        else
        {
          // FIX Client: Log an error. To continue or not, is determined below:
          // Connector  Status (see below):
          m_sessMgr->m_logger->error
            ("FIX::ReadHandler: Exception while processing MsgType={}: {}",
             msgType.to_string(), exc.what());
        }
      }
      //---------------------------------------------------------------------//
      // After-Parse-Proc: Should we continue reading data?                  //
      //---------------------------------------------------------------------//
      // IMPORTANT: If the curr session has been closed and the FD removed from
      // the Reactor, its input buffer has been destroyed -- so continued read-
      // ing of data in this loop will result in a segfault! So check  for the
      // session validity:
      // Normally, by now, "currSess" must always be non-NULL (even on the Ser-
      // ver-Side where it may be NULL initially -- then "CheckFIXSession" must
      // have set it), and the Session must NOT be Inactive -- otherwise do not
      // continue (so return (-1)):
      //
      if (utxx::unlikely
         (currSess == nullptr || m_sessMgr->IsInactiveSess(currSess)))
        return -1;
    }
    while(msgBegin < chunkEnd);
    // End of buffer reading loop

    //-----------------------------------------------------------------------//
    // After Reaching the End of Available Data:                             //
    //-----------------------------------------------------------------------//
    // The Processor might be interested in this condition, so notify it. This
    // is used, eg, for optimal invocation of Strategy Call-Backs on OrderBook
    // Updates (if it is a MktData Connector):
    //
    if (utxx::likely(m_processor != nullptr))
      m_processor->Process(currSess);  // NB: No Msg here!

    // All done: How much data has been read and consumed?
    int    consumed  = int(msgBegin - a_buff);
    assert(consumed >= 0);
    return consumed;
  }

  //=========================================================================//
  // "GetMsg":                                                               //
  //=========================================================================//
  template
  <
    DialectT::type  D,
    bool            IsServer,
    typename        SessMgr,
    typename        Processor
  >
  MsgPrefix* ProtoEngine<D, IsServer, SessMgr, Processor>::GetMsg
  (
    MsgT            a_msg_type,
    char*           a_msg_body,
    char*           a_body_end,
    int             a_total_len,
    SessData*       a_curr_sess,
    int             a_fd,
    utxx::time_val  a_ts_recv,
    utxx::time_val  a_ts_handl
  )
  {
    assert(a_msg_body  != nullptr    && a_body_end  != nullptr &&
           a_msg_body  <  a_body_end && a_total_len >  0       &&
           a_curr_sess != nullptr    && a_fd        >= 0);

    switch (a_msg_type)
    {
      //---------------------------------------------------------------------//
      // Session(Admin)-Level Msgs:                                          //
      //---------------------------------------------------------------------//
      GET_SESS_MSG(LogOn)
      GET_SESS_MSG(UserResponse)
      GET_SESS_MSG(LogOut)
      GET_SESS_MSG(ResendRequest)
      GET_SESS_MSG(SequenceReset)
      GET_SESS_MSG(TestRequest)
      GET_SESS_MSG(HeartBeat)
      GET_SESS_MSG(Reject)
      GET_SESS_MSG(News)
      // XXX: The following MsgTypes are also treated as Session-Level ones:
      GET_SESS_MSG(TradingSessionStatus)
      GET_SESS_MSG(OrderMassCancelReport)

      //---------------------------------------------------------------------//
      // FIX Client: Receiving Application-Level OrdMgmt Msgs:               //
      //---------------------------------------------------------------------//
      GET_MSG(BusinessMessageReject)
      GET_MSG(ExecutionReport)
      GET_MSG(OrderCancelReject)

      //---------------------------------------------------------------------//
      // FIX Server (XXX: Or RFS/RFQ?): Application-Level OrdMgmt Msgs:      //
      //---------------------------------------------------------------------//
      GET_MSG(NewOrderSingle)
      GET_MSG(OrderCancelRequest)
      GET_MSG(OrderCancelReplaceRequest)

      //---------------------------------------------------------------------//
      // FIX Client: Receiving Application-Level MktData Msgs:               //
      //---------------------------------------------------------------------//
      GET_MSG(MarketDataRequestReject)
      GET_MSG(MarketDataIncrementalRefresh)
      GET_MSG(MarketDataSnapShot)
      GET_MSG(SecurityDefinition)
      GET_MSG(SecurityStatus)
      GET_MSG(SecurityList)
      GET_MSG(QuoteRequestReject)
      GET_MSG(Quote)
      GET_MSG(QuoteStatusReport)

      //---------------------------------------------------------------------//
      // FIX Server: Receiving Application-Level MktData Msg(s):             //
      //---------------------------------------------------------------------//
      GET_MSG(MarketDataRequest)

      default: return nullptr;
    }
  }
  //=========================================================================//
  // "TryRejectMsg":                                                         //
  //=========================================================================//
  // Returns "true" iff "ReadHandler" should continue after that:
  //
  template
  <
    DialectT::type D,
    bool           IsServer,
    typename       SessMgr,
    typename       Processor
  >
  inline bool
  ProtoEngine<D, IsServer, SessMgr, Processor>::TryRejectMsg
  (
    [[maybe_unused]] int             a_fd,
                     SessData*       a_curr_sess,
    [[maybe_unused]] MsgT            a_msg_type,
    [[maybe_unused]] SeqNum          a_seq_num,
    [[maybe_unused]] SessRejReasonT  a_rej_reason,
                     char const*     a_err_msg,
    [[maybe_unused]] utxx::time_val  a_ts_recv
  )
  const
  {
    assert(a_err_msg != nullptr);

    if constexpr (IsServer)
    {
      // FIX Server: Reject this msg but still go ahead with the curr Session if
      // we can:
      if (a_curr_sess != nullptr &&
          m_sessMgr->RejectMsg
            (a_fd, a_curr_sess, a_msg_type, a_seq_num, a_rej_reason, a_err_msg))
        // Reject successfully sent -- can continue the Session:
        return true;
      else
      {
        // Reject sending failed (or the Session was not known at all): probably
        // no point in a graceful termination:   Graceful=false:
        m_sessMgr->template TerminateSession<false>
          (a_fd, a_curr_sess, a_err_msg, a_ts_recv);
        return false;
      }
    }
    else
    {
      // FIX Client, or no SessMgr: Log and skip this msg but continue:
      m_sessMgr->m_logger->error
        ("FIX::ReadHandler: Malformed msg received ({}): Skipped", a_err_msg);
      return true;
    }
    // This point is not reachable:
    __builtin_unreachable();
  }

  //=========================================================================//
  // Generate All Msg Parsers:                                               //
  //=========================================================================//
  //=========================================================================//
  // Session- (Admin-) Level Msgs:                                           //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "ParseLogOn":                                                           //
  //-------------------------------------------------------------------------//
  GENERATE_PARSER(LogOn,
    GET_POS      ( 108, m_HeartBtInt)
    GET_BOOL     ( 141, m_ResetSeqNumFlag)
    GET_CRD      ( 553, m_UserName)
    GET_CRD      ( 554, m_Passwd)
    GET_ENUM_CHAR(1409, m_PasswdChangeStatus, PasswdChangeStatusT))

  //-------------------------------------------------------------------------//
  // "ParseUserResponse":                                                    //
  //-------------------------------------------------------------------------//
  GENERATE_PARSER(UserResponse,
    GET_POS      ( 336, m_TradingSessionID)
    GET_REQ_ID   ( 923, m_UserReqID)
    GET_CRD      ( 553, m_UserName)
    GET_ENUM_INT ( 926, m_UserStat, UserStatT)
    GET_STR      ( 927, m_UserStatText))

  //-------------------------------------------------------------------------//
  // "ParseLogOut":                                                          //
  //-------------------------------------------------------------------------//
  GENERATE_PARSER(LogOut,
    GET_STR(58, m_Text))

  //-------------------------------------------------------------------------//
  // "ParseResendRequest":                                                   //
  //-------------------------------------------------------------------------//
  GENERATE_PARSER(ResendRequest,
    GET_POS(7,  m_BeginSeqNo)
    GET_NAT(16, m_EndSeqNo))

  //-------------------------------------------------------------------------//
  // "ParseSequenceReset":                                                   //
  //-------------------------------------------------------------------------//
  GENERATE_PARSER(SequenceReset,
    GET_BOOL(123, m_GapFillFlag)
    GET_POS(  36, m_NewSeqNo))

  //-------------------------------------------------------------------------//
  // "ParseTestRequest":                                                      //
  //-------------------------------------------------------------------------//
  GENERATE_PARSER(TestRequest,
    GET_STR(112, m_TestReqID))

  //-------------------------------------------------------------------------//
  // "ParseHeartBeat":                                                       //
  //-------------------------------------------------------------------------//
  GENERATE_PARSER(HeartBeat, )

  //-------------------------------------------------------------------------//
  // "ParseReject":                                                          //
  //-------------------------------------------------------------------------//
  GENERATE_PARSER(Reject,
    GET_POS(       45, m_RefSeqNum)
    GET_POS(      371, m_RefTagID)
    GET_STR(      372, m_RefMsgType)
    GET_ENUM_INT( 373, m_SessionRejectReason, SessRejReasonT)
    GET_STR(       58, m_Text))

  //-------------------------------------------------------------------------//
  // "ParseNews":                                                          //
  //-------------------------------------------------------------------------//
  GENERATE_PARSER(News,
    GET_STR(      148, m_Headline)
    GET_NAT(       33, m_NumLinesOfText)
    GET_STR(       58, m_LinesOfText[++idx]),

    // Initialisation of the idx
    int idx = -1;)

  //=========================================================================//
  // Application-Level OrdMgmt Msgs (Client-Side):                           //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "ParseBusinessMessageReject":                                           //
  //-------------------------------------------------------------------------//
  GENERATE_PARSER(BusinessMessageReject,
    GET_POS(       45, m_RefSeqNum)
    GET_REQ_ID(   379, m_BusinessRejectRefID)
    GET_ENUM_INT( 380, m_BusinessRejectReason, BusinessRejectReasonT)
    GET_STR(       58, m_Text))

  //-------------------------------------------------------------------------//
  // "ParseExecutionReport":                                                 //
  //-------------------------------------------------------------------------//
  GENERATE_PARSER(ExecutionReport,
    GET_STR(       37, m_OrderID)
    GET_STR(      278, m_MDEntryID)
    GET_STR(       17, m_ExecID)
    GET_REQ_ID(    11, m_ClOrdID)
    GET_REQ_ID(    41, m_OrigClOrdID)
    GET_ENUM_CHAR( 39, m_OrdStatus, OrdStatusT)
    GET_ENUM_CHAR(150, m_ExecType,  ExecTypeT)
    GET_SYM(       55, m_Symbol)
    GET_POS(       64, m_SettlDate)    // YYYYMMDD
    GET_ENUM_CHAR( 54, m_Side,    SideT)
    GET_PX(        44, m_Price)
    GET_ENUM_CHAR( 40, m_OrdType, OrderTypeT)
    GET_QTY(       14, m_CumQty)
    GET_QTY(      151, m_LeavesQty)
    GET_QTY(       32, m_LastQty)
    GET_PX(        31, m_LastPx)
    GET_STR(       58, m_Text)
    GET_ENUM_INT( 103, m_OrdRejReason, OrdRejReasonT)
    GET_DATE_TIME( 60, m_TransactTime),

    // All Pxs are initialised to NaN:
    tmsg.m_LastPx = PriceT();)

  //-------------------------------------------------------------------------//
  // "ParseOrderCancelReject":                                               //
  //-------------------------------------------------------------------------//
  GENERATE_PARSER(OrderCancelReject,
    GET_STR(       37, m_OrderID)
    GET_REQ_ID(    11, m_ClOrdID)
    GET_REQ_ID(    41, m_OrigClOrdID)
    GET_ENUM_CHAR( 39, m_OrdStatus,              OrdStatusT)
    GET_ENUM_INT( 102, m_CxlRejReason,           CxlRejReasonT)
    GET_ENUM_CHAR(434, m_CxlRejResponseTo,       CxlRejRespT)
    GET_STR(       58, m_Text))

  //-------------------------------------------------------------------------//
  // "ParseTradingSessionStatus":                                            //
  //-------------------------------------------------------------------------//
  GENERATE_PARSER(TradingSessionStatus,
    GET_STR(      336, m_TradingSessionID)
    GET_ENUM_INT( 340, m_TradSesStatus,          TradSesStatusT)
    GET_STR(       58, m_Text))

  //-------------------------------------------------------------------------//
  // "ParseOrderMassCancelReport":                                           //
  //-------------------------------------------------------------------------//
  GENERATE_PARSER(OrderMassCancelReport,
    GET_REQ_ID(    11, m_ClOrdID)
    GET_ENUM_CHAR(530, m_MassCancelRequestType,  MassCancelReqT)
    GET_ENUM_CHAR(531, m_MassCancelResponse,     MassCancelRespT)
    GET_ENUM_INT( 532, m_MassCancelRejectReason, MassCancelRejReasonT)
    GET_STR(       58, m_Text))

  //=========================================================================//
  // Application-Level MktData Msgs (Client-Side):                           //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "ParseMarketDataRequestReject":                                         //
  //-------------------------------------------------------------------------//
  GENERATE_PARSER(MarketDataRequestReject,
    GET_REQ_ID(   262, m_MDReqID)
    GET_STR(       58, m_Text)
    GET_ENUM_CHAR(281, m_MDReqRejReason, MDReqRejReasonT))

  //-------------------------------------------------------------------------//
  // "ParseMarketDataIncrementalRefresh":                                    //
  //-------------------------------------------------------------------------//
  // NB: For the sake if generality, Symbol and SettlDate are allowed in BOTH
  // the main msg body and in the "MDEntries" group; unfilled entries will re-
  // main 0s, and will be ignored:
  //
  GENERATE_PARSER(MarketDataIncrementalRefresh,
    GET_REQ_ID(   262, m_MDReqID)
    GET_STR(       64, m_SettlDate)   // YYYYMMDD or TenorStr
    GET_NAT(      268, m_NoMDEntries,
      // Check the msg size:
      CHECK_ONLY
      (
        if (utxx::unlikely(tmsg.m_NoMDEntries > MaxMDEs))
          throw utxx::runtime_error
                ("FIX::ReadMarketDataIncrementalRefresh: Too many MDEs: ",
                 tmsg.m_NoMDEntries);
      )
      // Initialise all MDE Pxs to NaN (Sizes are already 0s):
      for (int i = 0; i < tmsg.m_NoMDEntries; ++i)
        tmsg.m_MDEntries[i].m_MDEntryPx = PriceT();
    )
    // XXX: "MDUpdateAction" signals the start of a new MDE; the order of other
    // flds can be arbitrary:
    GET_ENUM_CHAR(279, m_MDEntries[++mde].m_MDUpdateAction,   MDUpdateActionT)

    GET_QTY(      271, m_MDEntries[  mde].m_MDEntrySize)
    GET_ENUM_CHAR(269, m_MDEntries[  mde].m_MDEntryType,      MDEntryTypeT)
    GET_PX(       270, m_MDEntries[  mde].m_MDEntryPx)
    GET_ENUM_CHAR(274, m_MDEntries[  mde].m_MDTickDirection,  MDTickDirectionT)
    GET_CHAR(    7562, m_MDEntries[  mde].m_MDPaidGiven)  // Currenex only
    GET_STR(      278, m_MDEntries[  mde].m_MDEntryID)
    GET_STR(      280, m_MDEntries[  mde].m_MDEntryRefID)
    GET_SYM(       55, m_MDEntries[  mde].m_Symbol)
    GET_ENUM_CHAR( 63, m_MDEntries[  mde].m_SettlmntType,     SettlmntTypeT)
    GET_ENUM_CHAR(276, m_MDEntries[  mde].m_QuoteCondition,   QuoteConditionT),

    // Initialisation of the "mde" Idx:
    int mde = -1;)

  //-------------------------------------------------------------------------//
  // "ParseMarketDataSnapShot":                                              //
  //-------------------------------------------------------------------------//
  GENERATE_PARSER(MarketDataSnapShot,
    GET_REQ_ID(   262, m_MDReqID)
    GET_SYM(       55, m_Symbol)
    GET_NAT(       48, m_SecID)
    GET_STR(       64, m_SettlDate)   // YYYYMMDD or TenorStr
    GET_STR(      193, m_SettlDate2)  // YYYYMMDD or TenorStr
    GET_QTY(      387, m_TotalVolumeTraded)
    GET_NAT(      268, m_NoMDEntries,
    // Check the msg size:
    CHECK_ONLY
    (
    if (utxx::unlikely(tmsg.m_NoMDEntries > MaxMDEs))
      throw utxx::runtime_error
            ("FIX::ParseMarketDataSnapShot: Too many MDEs: ",
             tmsg.m_NoMDEntries);
    )
    // Initialise all MDE Pxs to NaN:
    for (int i = 0; i < tmsg.m_NoMDEntries; ++i)
      tmsg.m_MDEntries[i].m_MDEntryPx = PriceT();)

    // XXX: "MDEntryType" signals the start of a new MDE; the order of other
    // flds can be arbitrary:
    GET_ENUM_CHAR(269, m_MDEntries[++mde].m_MDEntryType, MDEntryTypeT)

    GET_PX(       270, m_MDEntries[  mde].m_MDEntryPx)
    GET_QTY(      271, m_MDEntries[  mde].m_MDEntrySize)
    GET_ENUM_CHAR(274, m_MDEntries[  mde].m_MDTickDirection,  MDTickDirectionT)
    GET_CHAR(    7562, m_MDEntries[  mde].m_MDPaidGiven)  // Currenex only
    GET_QTY(      110, m_MDEntries[  mde].m_MinQty)
    GET_STR(      299, m_MDEntries[  mde].m_QuoteEntryID)
    GET_ENUM_CHAR(276, m_MDEntries[  mde].m_QuoteCondition,   QuoteConditionT)
    GET_STR(      336, m_MDEntries[  mde].m_TradingSessionID),

    // Initialisation of the "mde" Idx:
    int mde = -1;)

  //-------------------------------------------------------------------------//
  // "ParseSecurityDefinition":                                              //
  //-------------------------------------------------------------------------//
  GENERATE_PARSER(SecurityDefinition,
    GET_REQ_ID(   320, m_SecurityReqID)
    GET_STR(      322, m_SecurityResponseID)
    GET_ENUM_CHAR(323, m_SecurityResponseType, SecRespTypeT)
    GET_ENUM_CHAR(305, m_UnderlyingIDSource,   SecIDSrcT)
    GET_NAT(      393, m_TotalNumSecurities)
    GET_NAT(      146, m_NoRelatedSym,
    // Check the msg size:
    CHECK_ONLY
    (
      if (utxx::unlikely(tmsg.m_NoRelatedSym > MaxRelSyms))
        throw utxx::runtime_error
              ("FIX::SecurityDefinition: Too many Symbols: ",
              tmsg.m_NoRelatedSym);
    )
    // All Pxs are to be initialised to NaN:
    for (int i = 0; i < tmsg.m_NoRelatedSym; ++i)
      tmsg.m_RelatedSyms[i].m_UnderlyingStrikePrice = PriceT();)

    // "UnderlyingSymbol" signals the start of a new "RelatedSym":
    GET_SYM(      311, m_RelatedSyms[++rs].m_UnderlyingSymbol)

    GET_STR(      309, m_RelatedSyms[  rs].m_UnderlyingSecurityID)
    GET_STR(      307, m_RelatedSyms[  rs].m_UnderlyingSecurityDesc)
    GET_CCY(      318, m_RelatedSyms[  rs].m_UnderlyingCurrency)
    GET_POS(      319, m_RelatedSyms[  rs].m_RatioQty)
    GET_STR(      308, m_RelatedSyms[  rs].m_UnderlyingSecurityExchange)
    GET_NAT(      313, m_RelatedSyms[  rs].m_UnderlyingMaturityMonthYear)
    GET_ENUM_CHAR(315, m_RelatedSyms[  rs].m_UnderlyingPutOrCall,   PutOrCallT)
    GET_PX(       316, m_RelatedSyms[  rs].m_UnderlyingStrikePrice)
    GET_STR(      310, m_RelatedSyms[  rs].m_UnderlyingSecurityType)
    GET_STR(      312, m_RelatedSyms[  rs].m_UnderlyingSymbolSfx),

    // Initialisation of the "rs" Idx, and Pxs to NaN:
    int rs = -1;)

  //-------------------------------------------------------------------------//
  // "ParseSecurityStatus":                                                  //
  //-------------------------------------------------------------------------//
  GENERATE_PARSER(SecurityStatus,
    GET_REQ_ID(   324, m_SecurityStatusReqID)
    GET_SYM(       55, m_Symbol)
    GET_STR(      207, m_SecurityExchange)
    GET_STR(      167, m_SecurityType)
    GET_POS(      200, m_MaturityMonthYear)
    GET_POS(      205, m_MaturityDay)
    GET_ENUM_CHAR(201, m_PutOrCall, PutOrCallT)
    GET_PX(       202, m_StrikePrice)
    GET_ENUM_INT( 326, m_SecurityTradingStatus, SecTrStatusT)
    GET_PX(       332, m_HighPx)
    GET_PX(       333, m_LowPx)
    GET_PX(        31, m_LastPx),

    // All Pxs are initialised to NaN:
    tmsg.m_StrikePrice = PriceT();
    tmsg.m_HighPx      = PriceT();
    tmsg.m_LowPx       = PriceT();
    tmsg.m_LastPx      = PriceT();)

  //-------------------------------------------------------------------------//
  // "ParseSecurityList":                                                    //
  //-------------------------------------------------------------------------//
  GENERATE_PARSER(SecurityList,
    GET_STR(      320, m_SecurityReqID)
    GET_NAT(      146, m_NoRelatedSym,
      // Check the msg size:
      CHECK_ONLY
      (
      if (utxx::unlikely(tmsg.m_NoRelatedSym > MaxRelSyms))
        throw utxx::runtime_error
              ("FIX::ParseSecurityList: Too many Symbols: ",
              tmsg.m_NoRelatedSym);
      )
    )

    // XXX: "Symbol" signals the start of a new Instrument; the order of other
    // flds can be arbitrary:
    GET_SYM(         55, m_Instruments[++sym].m_Symbol)
    GET_NAT(      20001, m_Instruments[  sym].m_QuantityScale)
    GET_NAT(      20002, m_Instruments[  sym].m_PriceScale)
    GET_NAT(      20003, m_Instruments[  sym].m_AmountScale)
    GET_ENUM_CHAR(20004, m_Instruments[  sym].m_InstrumentStatus,
                  InstrumentStatusT),

    // Initialisation of the "mde" Idx:
    int sym = -1;)

  //-------------------------------------------------------------------------//
  // "ParseQuoteRequestReject":                                              //
  //-------------------------------------------------------------------------//
  GENERATE_PARSER(QuoteRequestReject,
    GET_STR(      131, m_QuoteReqID)
    GET_ENUM_INT( 658, m_QuoteReqRejReason, QuoteReqRejReasonT)
    GET_NAT(      146, m_NoRelatedSym,
      // Check the number of following symbols -- must be 1:
      CHECK_ONLY
      (
        if (utxx::unlikely(tmsg.m_NoRelatedSym != 1))
          throw utxx::runtime_error
                ("FIX::ParseQuoteRequestReject: Must have just 1 Symbol");
      )
    )
    GET_SYM(       55, m_Symbol)
    GET_STR(       58, m_Text))

  //-------------------------------------------------------------------------//
  // "ParseQuote":                                                           //
  //-------------------------------------------------------------------------//
  GENERATE_PARSER(Quote,
    GET_REQ_ID(   131, m_QuoteReqID)
    GET_STR(      117, m_QuoteID)
    GET_ENUM_CHAR(537, m_QuoteType, QuoteTypeT)
    GET_SYM(       55, m_Symbol)
    GET_QTY(       38, m_OrderQty)
    GET_PX(       132, m_MDEntries[0].m_MDEntryPx)
    GET_PX(       133, m_MDEntries[1].m_MDEntryPx)
    GET_QTY(      134, m_MDEntries[0].m_MDEntrySize)
    GET_QTY(      135, m_MDEntries[1].m_MDEntrySize)
    GET_TIME(      62, m_ValidUntilTime)
    GET_TIME(      60, m_TransactTime)
    GET_STR(       15, m_Currency)
    GET_POS(    20000, m_QuoteVersion), // Cumberland only

    // All Pxs are initialised to NaN:
    tmsg.m_MDEntries[0].m_MDEntryPx = PriceT();
    tmsg.m_MDEntries[1].m_MDEntryPx = PriceT();
    // initialize transact time to now
    tmsg.m_TransactTime = utxx::now_utc();)

  //-------------------------------------------------------------------------//
  // "ParseQuoteStatusReport":                                               //
  //-------------------------------------------------------------------------//
  GENERATE_PARSER(QuoteStatusReport,
    GET_STR(      131, m_QuoteReqID)
    GET_STR(      117, m_QuoteID)
    GET_SYM(       55, m_Symbol)
    GET_ENUM_INT( 297, m_QuoteStatus, QuoteStatusT)
    GET_ENUM_CHAR(537, m_QuoteType,   QuoteTypeT)
    GET_POS(    20000, m_QuoteVersion) // Cumberland only
    GET_STR(       58, m_Text))

  //=========================================================================//
  // Application-Level OrdMgmt Msgs (Server-Side):                           //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "ParseNewOrderSigle":                                                   //
  //-------------------------------------------------------------------------//
  // NB: Here "m_ClOrdID" is a string, not an "OrderID", because the Client can
  // construct it in an arbitrary way:
  //
  GENERATE_PARSER(NewOrderSingle,
    GET_STR(       11, m_ClOrdID)
    GET_ENUM_CHAR( 40, m_OrdType,   OrderTypeT)
    GET_ENUM_CHAR( 59, m_TmInForce, TimeInForceT)
    GET_SYM      ( 55, m_Symbol)
    GET_ENUM_CHAR( 54, m_Side,      SideT)
    GET_QTY(       38, m_Qty)
    GET_PX(        44, m_Px)
    GET_DATE_TIME( 60, m_TransactTime),

    // All Pxs are initialised to NaN:
    tmsg.m_Px = PriceT();)

  //-------------------------------------------------------------------------//
  // "ParseOrderCancelRequest":                                              //
  //-------------------------------------------------------------------------//
  // Again, here "m_ClOrdID" and "m_OrigClOrdID" are strings:
  //
  GENERATE_PARSER(OrderCancelRequest,
    GET_STR(       11, m_ClOrdID)
    GET_STR(       41, m_OrigClOrdID)
    GET_DATE_TIME( 60, m_TransactTime))

  //-------------------------------------------------------------------------//
  // "ParseOrderCancelReplaceRequest":                                       //
  //-------------------------------------------------------------------------//
  GENERATE_PARSER(OrderCancelReplaceRequest,
    GET_STR(       11, m_ClOrdID)
    GET_STR(       41, m_OrigClOrdID)
    GET_QTY(       38, m_Qty)
    GET_PX(        44, m_Px)
    GET_DATE_TIME( 60, m_TransactTime),

    // All Pxs are initialised to NaN:
    tmsg.m_Px = PriceT();)

  //=========================================================================//
  // Application-Level MktData Msgs (Server-Side):                           //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "ParseMarketDataRequest":                                               //
  //-------------------------------------------------------------------------//
  GENERATE_PARSER(MarketDataRequest,
    GET_STR(      262, m_ReqID)
    GET_ENUM_CHAR(263, m_ReqType, SubscrReqTypeT)
    GET_STR(       64, m_SettlDate)   // YYYYMMDD or TenorStr

    GET_NAT(      146, m_NoRelatedSym,
    // Check the number of following symbols -- must be 1:
    CHECK_ONLY
    (
      if (utxx::unlikely(tmsg.m_NoRelatedSym != 1))
        throw utxx::runtime_error
              ("FIX::ParseMarketDataRequest: Must have just 1 Symbol");
    ) )
    GET_SYM(       55, m_Symbol))

} // End namespace FIX
} // End namespace MAQUETTE
