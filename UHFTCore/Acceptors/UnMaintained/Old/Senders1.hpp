// vim:ts=2:et
//===========================================================================//
//                                "FIX/Senders1.hpp":                        //
//                         Sender of Session-Level FIX Msgs                  //
//===========================================================================//
#pragma once

#include "FixEngine/Acceptor/EAcceptor_FIX.h"
#include "FixEngine/Acceptor/UtilsMacros.hpp"

namespace AlfaRobot
{
  //=========================================================================//
  // "EAcceptor_FIX::LogMsg":                                                //
  //=========================================================================//
  // Performs logging of sent or received FIX msgs. NB: A received msg can al-
  // ready be parsed, and in the process of doing so,  SOHs are replaced with
  // '\0's, so replace BOTH of them with '|'s for readability:
  //
  template<FIX::DialectT::type D,  typename Conn, typename Derived>
  template<bool IsSend>
  inline void EAcceptor_FIX<D, Conn, Derived>::LogMsg
    (char* a_buff, int a_len)
  const
  {
    // Check if logging of raw FIX msgs is enabled  (eg, it may or may not be
    // disabled for MktData Acceptors):
    if (utxx::unlikely(m_rawLogger == nullptr))
      return;
    assert(a_buff != nullptr && a_len > 0);

    // Replace "SOH"s (or possibly '\0's) with '|'s for better log readability:
    char* end  = a_buff + a_len;
    for (char*  curr = a_buff; curr < end; ++curr)
      if (utxx::unlikely(*curr == FIX::soh || (!IsSend && *curr == '\0')))
        *curr = '|';

    // Install the terminator instead of the last SOH -- NOT after it, as that
    // could over-write the beginning of the next msg (in a READ buffer;  this
    // does not apply to logging of SENT msgs -- the latter is done only after
    // the msg has been output into the socket):
    *(end-1)  = '\0';

    // Now actually log it:
    // "-->" is a Client-initiated traffic;
    // "<==" is our traffic sentto the Client:
    m_rawLogger->info(IsSend ? "--> " : "<== ") << a_buff;
  }

  //=========================================================================//
  // "EAcceptor_FIX::MkPreamble":                                            //
  //=========================================================================//
  // Returns: (curr_buff_ptr, msg_body_ptr, curr_utc):
  // NB: unlike the Client-side implementation, here we do NOT support sending
  // multiple FIX msgs at once (vuffer accumulation):
  //
  template<FIX::DialectT::type D, typename Conn, typename Derived>
  template<FIX::MsgT   Type>
  inline std::tuple<char*, char*, utxx::time_val>
    EAcceptor_FIX<D, Conn, Derived>::MkPreamble(FIXSession* a_session) const
  {
    assert(a_session != nullptr);

    // TODO: At the moment, only "SequenceReset-GapFill" can be a "Dup", but
    // this will change when proper session-level resending is implemented:
    constexpr bool IsDup = (Type == FIX::MsgT::SequenceReset);

    char* curr = m_outBuff;
    STR_FLD( "8=", FIX::ProtocolFeatures<D>::s_fixVersion)  // BeginString
    EXPL_FLD("9=XXXX")                                       // BodyLength
    char* msgBody = curr;

    CHAR_FLD("35=", Type)                                    // MsgType
    STR_FLD( "49=", a_session->m_serverCompID.data())        // SenderCompID
    // XXX: We assume that on the Server-side, there is no SubID as yet

    STR_FLD("56=",  a_session->m_clientCompID.data())        // TargetCompID

    // But the Client may have a SubID, and we may need to send it:
    if (FIX::ProtocolFeatures<D>::s_useSubID)
    {
      if (utxx::likely(!(a_session->m_clientSubID.empty())))
        STR_FLD("57=",   a_session->m_clientSubID.data())    // TargetSubID
    }

    // Output the TxSN but do NOT increment it yet -- that will be done only
    // after the msg has been successfully sent out (otherwise, in case of a
    // sending error, the Client would get a TxSN gap):
    INT_FLD("34=", a_session->m_txSN)                        // MsgSeqNum
    if (IsDup)
      EXPL_FLD("43=Y")                                       // Duplicate?

    // Output the Time Stamp as "YYYYMMDD-hh:mm:ss.sss", so the length of this
    // fld's value is 21.   IMPORTANT: For efficiency, we use pre-defined Date
    // string; only the Time part is to be stringified:
    curr  = stpcpy(curr, "52=");                             // Sending DateTime
    curr  = stpcpy(curr, GetCurrDateStr());
    *curr = '-';
    ++curr;
    utxx::time_val bts = utxx::now_utc();
    curr  = FIX::OutputTime(bts - GetCurrDate(), curr);
    *curr = FIX::soh;
    ++curr;

    // NB: At this point, curr = TimeStamp + 22 (incl SOH); this is useful if
    // we need to get the TimeStamp  ptr:
    return std::make_tuple(curr, msgBody, bts);
  }

  //=========================================================================//
  // "EAcceptor_FIX::CompleteSendLog":                                       //
  //=========================================================================//
  // To Complete, Send Out and Log a FIX msg. Returns the socket send time (may
  // be empty if the send was unsuccessful):
  //
  template<FIX::DialectT::type D, typename Conn, typename Derived>
  template<FIX::MsgT Type>
  inline utxx::time_val EAcceptor_FIX<D, Conn, Derived>::CompleteSendLog
  (
    FIXSession* a_session,
    char*       a_body_begin,
    char*       a_body_end
  )
  const
  {
    assert(a_session != nullptr && a_body_begin != nullptr &&
           a_body_begin < a_body_end);

    // Install the CheckSum and calculate the total Msg Length (incl the very
    // final terminating SOH):
    int len = FIX::CompleteMsg(m_outBuff, a_body_begin, a_body_end);

    // Send the Msg up:
    // NB: [a_body_begin .. a_body_end] should be within the address ragne of
    //     [m_outBuff    .. m_outBuff + len]:
    assert(m_outBuff < a_body_begin && a_body_end < m_outBuff + len);

    std::cout << "SEND(" << std::string(m_outBuff, static_cast<unsigned  long>(len)) << ")" << std::endl;
    utxx::time_val sendTime = m_reactor->Send(m_outBuff, len, a_session->m_fd);

    // After a successful send, increment the TxSN and update the stats:
    ++(a_session->m_txSN);
    a_session->m_bytesTx += size_t(len);
    m_totBytesTx         += size_t(len);

    // Log the msg sent:
    LogMsg<true>(m_outBuff, len);

    return sendTime;
  }

  //=========================================================================//
  // Sending Out Session-Level (Admin) Msgs:                                 //
  //=========================================================================//
  //=========================================================================//
  // "EAcceptor_FIX::SendLogOn":                                             //
  //=========================================================================//
  template<FIX::DialectT::type D,  typename Conn, typename Derived>
  inline void EAcceptor_FIX<D, Conn, Derived>::SendLogOn
    (FIXSession* a_session) const
  {
    assert(a_session != nullptr);

    // If, from our point of view, both "SeqNum"s for this Client are 1, then
    // we will instruct the Client to do a reset as well:
    bool resetSNs = (a_session->m_rxSN == 1) && (a_session->m_txSN == 1);

    auto preamble = MkPreamble<FIX::MsgT::LogOn>(a_session);
    char* curr    = std::get<0>(preamble);
    char* msgBody = std::get<1>(preamble);

    // Specific "LogOn" Flds:
    EXPL_FLD("98=0")                                    // EncryptMethod

    // For the HeartBeatInt, we will re-confirm what the Client has proposed
    // (if e are in this method, we have accepted that proposal):
    INT_FLD( "108=", a_session->m_heartBeatSec)         // HeartBeatInt

    if (resetSNs)
      EXPL_FLD("141=Y")                                 // ResetSeqNumFlag

    // XXX: Password updating by Clients over the FIX protocol is not supported
    // yet (and may be a bad idea in general), so there is no such fld here...

    // Go!
    (void) CompleteSendLog<FIX::MsgT::LogOn>(a_session, msgBody, curr);
  }

  //=========================================================================//
  // "EAcceptor_FIX::SendLogOut":                                            //
  //=========================================================================//
  template<FIX::DialectT::type D,  typename Conn, typename Derived>
  inline void EAcceptor_FIX<D, Conn, Derived>::SendLogOut
  (
    FIXSession* a_session,
    char const* a_msg   // May be NULL or empty
  )
  const
  {
    auto preamble = MkPreamble<FIX::MsgT::LogOut>(a_session);
    char* curr    = std::get<0>(preamble);
    char* msgBody = std::get<1>(preamble);

    // Install the text msg (if provided):
    if (a_msg != nullptr && *a_msg != '\0')
      STR_FLD("58=", a_msg);

    // Go:
    (void) CompleteSendLog<FIX::MsgT::LogOut>(a_session, msgBody, curr);
  }

  //=========================================================================//
  // "EAcceptor_FIX::SendResendRequest":                                     //
  //=========================================================================//
  // NB: Here "upto" can be 0, which means "resend all msgs from the given Seq-
  // Num, and continue on...":
  //
  template<FIX::DialectT::type D,  typename Conn, typename Derived>
  inline void EAcceptor_FIX<D, Conn, Derived>::SendResendRequest
  (
    FIXSession* a_session,
    SeqNum      a_from,
    SeqNum      a_upto
  )
  const
  {
    assert(a_session != nullptr);

    if (utxx::unlikely(a_from <= 0 || (a_upto != 0 && a_upto < a_from)))
      throw utxx::badarg_error
            ("EAcceptor_FIX::ResendRequest: Invalid Range: ",
             a_from, " .. ", a_upto);

    auto preamble = MkPreamble<FIX::MsgT::ResendRequest>(a_session);
    char* curr    = std::get<0>(preamble);
    char* msgBody = std::get<1>(preamble);

    // Specific "ResendRequest" Flds:
    INT_FLD("7=",  a_from)
    INT_FLD("16=", a_upto)

    // Go:
    (void) CompleteSendLog<FIX::MsgT::ResendRequest>(a_session, msgBody, curr);
  }

  //=========================================================================//
  // "EAcceptor_FIX::SendGapFill":                                           //
  //=========================================================================//
  // Generates as "SequenceReset-GapFill" msg. NB: we currently do NOT support
  // a "SequenceReset-Reset"; in case of serious error, we rather re-connect:
  // NB: Here both "from" and "upto" must be non-0:
  //
  template<FIX::DialectT::type D, typename Conn, typename Derived>
  inline void EAcceptor_FIX<D, Conn, Derived>::SendGapFill
  (
    FIXSession* a_session,
    SeqNum      a_from,
    SeqNum      a_upto
  )
  const
  {
    assert(a_session != nullptr);

    if (utxx::unlikely(a_from <= 0 || a_upto  < a_from))
      throw utxx::badarg_error
            ("EAcceptor_FIX::GapFill: Invalid Range: ",
             a_from, " .. ",  a_upto);

    // Preamble: Because it is a "GapFill", it is sent in response to "Resend-
    // Request"  from a Client (instead of actually re-sending  that range of
    // msgs), so it MUST be a "Dup".
    // And to avoid any confusion, the "SequenceReset-GapFill" msg itself must
    // be sent with the curr SeqNum=a_from   (the initial one expected by the
    // counter-party):
    //
    a_session->m_txSN = a_from;
    auto preamble     = MkPreamble<FIX::MsgT::SequenceReset>(a_session);
    char* curr        = std::get<0>(preamble);
    char* msgBody     = std::get<1>(preamble);

    // Specific "GapFill" fld: SeqNum beyond the gap:
    SeqNum next   = a_upto + 1;
    INT_FLD( "36=", next)      // Where to reset to
    EXPL_FLD("123=Y")          // GapFill

    // Go:
    (void) CompleteSendLog<FIX::MsgT::ResendRequest>(a_session, msgBody, curr);

    // After that, adjust our next TxSN:
    assert(next > a_from);
    a_session->m_txSN = next;
  }

  //=========================================================================//
  // "EAcceptor_FIX::SendTestRequest":                                       //
  //=========================================================================//
  // Useful, in particular, just after "LogOn" to verify that the "SeqNum"s are
  // indeed in sync between the Server and the Client:
  //
  template<FIX::DialectT::type D,  typename Conn, typename Derived>
  inline void EAcceptor_FIX<D, Conn, Derived>::SendTestRequest
    (FIXSession* a_session)
  const
  {
    assert(a_session != nullptr);

    auto preamble = MkPreamble<FIX::MsgT::TestRequest>(a_session);
    char* curr    = std::get<0>(preamble);
    char* msgBody = std::get<1>(preamble);

    // "TestRequest" Specific fld: TestReqID. XXX: We take a random 31-bit int
    // coming from the curr microsecond time stamp:
    int reqID = int(utxx::now_utc().microseconds() & 0x0FFFFFFFL);
    INT_FLD("112=", reqID)

    // Go:
    (void) CompleteSendLog<FIX::MsgT::ResendRequest>(a_session, msgBody, curr);
  }

  //=========================================================================//
  // "EAcceptor_FIX::SendHeartBeat":                                         //
  //=========================================================================//
  template<FIX::DialectT::type D,  typename Conn, typename Derived>
  inline void EAcceptor_FIX<D, Conn, Derived>::SendHeartBeat
  (
    FIXSession* a_session,
    char const* a_test_req_id   // May be NULL
  )
  const
  {
    assert(a_session != nullptr);

    auto preamble = MkPreamble<FIX::MsgT::HeartBeat>(a_session);
    char* curr    = std::get<0>(preamble);
    char* msgBody = std::get<1>(preamble);

    // Specific "HeartBeat" fld: Install TestReqID if provided:
    if (a_test_req_id != nullptr && *a_test_req_id != '\0')
      STR_FLD("112=", a_test_req_id)

    // Go:
    (void) CompleteSendLog<FIX::MsgT::HeartBeat>(a_session, msgBody, curr);
  }

  //=========================================================================//
  // "EAcceptor_FIX::SendReject":                                            //
  //=========================================================================//
  template<FIX::DialectT::type D,  typename Conn, typename Derived>
  inline void EAcceptor_FIX<D, Conn, Derived>::SendReject
  (
    FIXSession*          a_session,
    SeqNum               a_rej_sn,
    FIX::MsgT            a_rej_mtype,
    FIX::SessRejReasonT  a_reason,
    char const*          a_msg
  )
  const
  {
    assert(a_session != nullptr);

    auto preamble = MkPreamble<FIX::MsgT::Reject>(a_session);
    char* curr    = std::get<0>(preamble);
    char* msgBody = std::get<1>(preamble);

    // Specific "Reject" flds:
    // XXX: At the moment, we do not support per-Tag error granularity, so it
    // cannot be reported  in "Reject", either...
    INT_FLD ("45=",  a_rej_sn)
    CHAR_FLD("372=", char(a_rej_mtype))
    INT_FLD( "373=", int (a_reason))
    STR_FLD( "58=",  a_msg)

    // Go:
    (void) CompleteSendLog<FIX::MsgT::Reject>(a_session, msgBody, curr);
  }

  //=========================================================================//
  // "EAcceptor_FIX::SendSessionStatus":                                     //
  //=========================================================================//
  template<FIX::DialectT::type D,  typename Conn, typename Derived>
  inline void EAcceptor_FIX<D, Conn, Derived>::SendSessionStatus
  (
    FIXSession*          a_session,
    FIX::TradSesStatusT  a_status
  )
  const
  {
    assert(a_session != nullptr);

    auto preamble = MkPreamble<FIX::MsgT::TradingSessionStatus>(a_session);
    char* curr    = std::get<0>(preamble);
    char* msgBody = std::get<1>(preamble);

    // Specific "TradingSessionStatus" fld(s):

    // TODO YYY  convert
    CHAR_FLD("340=", '0' + char(a_status))

    // TODO YYY
    EXPL_FLD("336=Trade")

    // Go:
    (void) CompleteSendLog<FIX::MsgT::TradingSessionStatus>
      (a_session, msgBody, curr);
  }
} // End namespace AlfaRobot
