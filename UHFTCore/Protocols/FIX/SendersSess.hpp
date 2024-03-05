// vim:ts=2:et
//===========================================================================//
//                       "Protocols/FIX/SendersSess.hpp":                    //
//        FIX Protocol Engine Sender of Session-Level and MktData Msgs       //
//===========================================================================//
#pragma once

#include "Basis/BaseTypes.hpp"
#include "Basis/PxsQtys.h"
#include "Basis/SecDefs.h"
#include "Protocols/FIX/Features.h"
#include "Protocols/FIX/Msgs.h"
#include "Protocols/FIX/ProtoEngine.h"
#include "Protocols/FIX/SessData.hpp"
#include "Protocols/FIX/UtilsMacros.hpp"
#include <utxx/compiler_hints.hpp>

namespace MAQUETTE
{
namespace FIX
{
  //=========================================================================//
  // "LogMsg":                                                               //
  //=========================================================================//
  // Performs logging of sent or received FIX msgs. NB: A received msg can al-
  // ready be parsed, and in the process of doing so,  SOHs are replaced with
  // '\0's, so replace BOTH of them with '|'s for readability:
  //
  template
  <
    DialectT::type D,
    bool           IsServer,
    typename       SessMgr,
    typename       Processor
  >
  template<bool    IsSend>
  inline   void ProtoEngine<D, IsServer, SessMgr, Processor>::LogMsg
    (char* a_buff, int a_len)
  const
  {
    // Check if logging of raw FIX msgs is enabled:
    if (utxx::unlikely(m_sessMgr->m_protoLogger == nullptr))
      return;
    assert(a_buff != nullptr && a_len > 0);

    // Replace "SOH"s (or possibly '\0's) with '|'s for better log readability:
    char* end  = a_buff + a_len;
    for (char*  curr = a_buff; curr < end; ++curr)
      if (utxx::unlikely(*curr == soh || (!IsSend && *curr == '\0')))
        *curr = '|';

    // Install the terminator instead of the last SOH -- NOT after it, as that
    // could over-write the beginning of the next msg (in a READ buffer;  this
    // does not apply to logging of SENT msgs -- the latter is done only after
    // the msg has been output into the socket):
    *(end-1)  = '\0';

    // Now actually log it:
    m_sessMgr->m_protoLogger->info("{} {}", IsSend ? "<--" : "==>", a_buff);
    DEBUG_ONLY(m_sessMgr->m_protoLogger->flush();)
  }

  //=========================================================================//
  // "MkPreamble":                                                           //
  //=========================================================================//
  // Returns: (curr_buff_ptr, msg_body_ptr, create_ts, txSN);   "create_ts" is
  // propagated from the arg if the latter is non-empty:
  template
  <
    DialectT::type  D,
    bool            IsServer,
    typename        SessMgr,
    typename        Processor
  >
  template<MsgT::type Type>
  inline   std::tuple<char*, char*, utxx::time_val, SeqNum>
  ProtoEngine<D, IsServer, SessMgr, Processor>::MkPreamble
  (
    SessData*       a_sess,
    utxx::time_val  a_ts_create
  )
  const
  {
    assert(a_sess != nullptr);

    // Compte the CreateTS if it was not provided by the Caller:
    if (a_ts_create.empty())
      a_ts_create = utxx::now_utc();

    constexpr bool IsDup = (Type == MsgT::SequenceReset);

    // XXX: In theory, overrun of "m_outBuff" is possible, but is very unlikely:
    CHECK_ONLY
    (
      if (utxx::unlikely (int(sizeof(m_outBuff) - 512) < m_outBuffLen))
        throw utxx::runtime_error
              ("FIX::ProtoEngine::MkPreamble: OutBuff Full or Nearly Full: "
               "CurrSize=", m_outBuffLen, ", MaxSize=", sizeof(m_outBuff));
    )
    char*  curr = m_outBuff + m_outBuffLen;
    curr = stpcpy(curr, MkPrefixStr(ProtocolFeatures<D>::s_fixVersionNum));
                                                             // BeginString
    EXPL_FLD("9=XXXX")                                       // BodyLength
    char* msgBody = curr;

    CHAR2_FLD("35=", Type)                                   // MsgType
    STR_FLD(  "49=", a_sess->m_ourCompID.data())             // SenderCompID

    if constexpr (ProtocolFeatures<D>::s_useSubID)
    {
      if (utxx::likely(!IsEmpty(a_sess->m_ourSubID)))
        STR_FLD("50=", a_sess->m_ourSubID.data())            // SenderSubID
    }

    STR_FLD("56=",  a_sess->m_contrCompID.data())            // TargetCompID
    if constexpr (ProtocolFeatures<D>::s_useSubID)
    {
      if (utxx::likely(!IsEmpty(a_sess->m_contrSubID)))
        STR_FLD("57=", a_sess->m_contrSubID.data())          // TargetSubID
    }

    if constexpr (ProtocolFeatures<D>::s_useOnBehalfOfCompIDInsteadOfClientID)
    {
      // Use ClientID as well if it is non-empty (normally for FIX.4.2):
      if (utxx::likely(!IsEmpty(a_sess->m_clientID)))
        STR_FLD("115=", a_sess->m_clientID.data())           // OnBehalfOfCompID
    }

    SeqNum txSN =  *(a_sess->m_txSN);
    INT_FLD("34=", txSN)                                     // MsgSeqNum
    if constexpr (IsDup)
      EXPL_FLD("43=Y")                                       // Duplicate?

    // Output the Time Stamp as "YYYYMMDD-hh:mm:ss.sss", so the length of this
    // fld's value is 21.   IMPORTANT: For efficiency, we use pre-defined Date
    // string; only the Time part is to be stringified:
    curr  = stpcpy(curr, "52=");                             // Sending DateTime
    curr  = OutputDateTimeFIX(a_ts_create, curr);
    *curr = soh;
    ++curr;

    // NB: At this point, curr = TimeStamp + 22; this is useful if we need to
    // get the TimeStamp  ptr:
    return std::make_tuple(curr, msgBody, a_ts_create, txSN);
  }

  //=========================================================================//
  // "MkReqID":                                                              //
  //=========================================================================//
  // ReqID format:
  // [PartyID//][CurrDate-]SeqReqID
  // where
  // (*) PartyID  is used if supported  by  the  Protocol Dialect and present;
  // (*) XXX: the "//" separator is currently non-configurable;   can be made
  //     configurable in the future;
  // (*) CurrDate is used if configured for this Protocol Dialect  (typically
  //     needed if the server side is running 24/5, because we currently reset
  //     our ReqIDs daily):
  //
  template
  <
    DialectT::type D,
    bool           IsServer,
    typename       SessMgr,
    typename       Processor
  >
  inline char* ProtoEngine<D, IsServer, SessMgr, Processor>::MkReqID
  (
    SessData*      a_sess,
    char*          a_curr,
    char const*    a_tag,
    OrderID        a_req_id
  )
  const
  {
    assert(a_sess != nullptr && a_curr != nullptr && a_tag != nullptr);

    // Output the Tag -- NB: this advances "a_curr":
    a_curr = stpcpy (a_curr, a_tag);

    // From now on, keep "a_curr" unchanged and advance "curr":
    // (*) for compatibility with INT_VAL macro below;
    // (*) for prefix size validation:
    char* curr = a_curr;

    // Possibly output the PartyID (if it is to be used in "NewOrder", it will
    // obviously be used in ALL ReqIDs):
    //
    if (ProtocolFeatures<D>::s_usePartiesInNew && !IsEmpty(a_sess->m_partyID))
    {
      curr  = stpcpy(curr, a_sess->m_partyID.data());
      curr  = stpcpy(curr, "//");
    }

    // Possibly output the CurrDate:
    if constexpr (ProtocolFeatures<D>::s_useCurrDateInReqID)
    {
      curr  = stpcpy(curr, GetCurrDateStr());
      *curr = '-';
      ++curr;
    }
    // By now, "curr" must be advanced exactly by "m_reqIDPfxSz":
    assert(int(curr - a_curr) == m_reqIDPfxSz);

    // Finally, output the numerical ReqID itself, and increment "curr":
    INT_VAL(a_req_id)
    *curr  = soh;
    ++curr;
    return curr;
  }

  //=========================================================================//
  // "MkSegmSessDest": By Instrument:                                        //
  //=========================================================================//
  // Installs a "TradingSessionID" / "MktSegment" / or even "SettlDate":
  //
  template
  <
    DialectT::type D,
    bool           IsServer,
    typename       SessMgr,
    typename       Processor
  >
  inline char*
  ProtoEngine<D,   IsServer, SessMgr, Processor>::MkSegmSessDest
  (
    char*          a_curr,
    SecDefD const& a_instr
  )
  const
  {
    // NB: It is not possible that  both SettlDate and Tenor  are used  as
    // Segments (although it is possible that none of them is used as such):
    static_assert(!(ProtocolFeatures<D>::s_useSettlDateAsSegmOrd &&
                    ProtocolFeatures<D>::s_useTenorAsSegmOrd)    &&
                  !(ProtocolFeatures<D>::s_useSettlDateAsSegmMkt &&
                    ProtocolFeatures<D>::s_useTenorAsSegmMkt),
                  "FIX::ProtoEngine: SettlDate and Tenor cannot simultaneously"
                  " be used as Segments");
    assert(a_curr != nullptr);

    char*       curr = a_curr;
    char const* val  = nullptr;

    if constexpr (ProtocolFeatures<D>::s_useTradingSessionID)
    {
      if constexpr (ProtocolFeatures<D>::s_grpTradingSessionID)
        EXPL_FLD("386=1")
      val = a_instr.m_SessOrSegmID.data();
      STR_FLD("336=", val)
    }
    else
    if constexpr (ProtocolFeatures<D>::s_useMktSegmentID) // Eg FORTS
    {
      val = a_instr.m_SessOrSegmID.data();
      STR_FLD("1300=", val)
    }
    else
    if ((m_sessMgr->IsOrdMgmt()  &&
          ProtocolFeatures<D>::s_useSettlDateAsSegmOrd) ||
        (m_sessMgr->IsMktData()  &&
          ProtocolFeatures<D>::s_useSettlDateAsSegmMkt))
    {
      // Use the stringified SettlDate with Tag=64 (Tenor or SettlDate):
      val = a_instr.m_SettlDateStr;
      STR_FLD("64=", val)
    }
    else
    if ((m_sessMgr->IsOrdMgmt()  &&
          ProtocolFeatures<D>::s_useTenorAsSegmOrd)     ||
        (m_sessMgr->IsMktData()  &&
          ProtocolFeatures<D>::s_useTenorAsSegmMkt))
    {
      // Use the same Tag=64 but with the symbolic Tenor:
      val = a_instr.m_Tenor.data();
      STR_FLD("64=", val)

      // If, furthermore, this instrument is a Swap (has non-empty Tenor2),
      // output it as well:
      char const* val2 = a_instr.m_Tenor2.data();
      if (utxx::unlikely(*val2 != '\0'))
        STR_FLD("193=",   val2)
    }
    // Otherwise, None specified: Nothing to do...

    // If we have output anything at all, it must be a non-empty "val":
    CHECK_ONLY
    (
      if (utxx::unlikely(val != nullptr && *val == '\0'))
        throw utxx::badarg_error
              ("FIX::ProtoEngine::MkReqID: Empty Segment/Sess/Tenor/SettlDate");
    )
    return curr;
  }

  //=========================================================================//
  // "MkSegmSessDest": By Explicit ID:                                       //
  //=========================================================================//
  // Similar to above, but is currently used on the Server-Side only:
  //
  template
  <
    DialectT::type D,
    bool           IsServer,
    typename       SessMgr,
    typename       Processor
  >
  inline char*
  ProtoEngine<D,   IsServer,   SessMgr, Processor>::MkSegmSessDest
  (
    char*          a_curr,
    char const*    a_segm_id
  )
  const
  {
    assert(a_curr != nullptr && a_segm_id != nullptr &&
          *a_segm_id != '\0' && IsServer);

    char* curr = a_curr;

    if constexpr (ProtocolFeatures<D>::s_useTradingSessionID)
    {
      if constexpr (ProtocolFeatures<D>::s_grpTradingSessionID)
        EXPL_FLD("386=1")
      STR_FLD("336=",  a_segm_id)
    }
    else
    if constexpr (ProtocolFeatures<D>::s_useMktSegmentID) // Eg FORTS
      STR_FLD("1300=", a_segm_id)
    else
    if ((m_sessMgr->IsOrdMgmt()   &&
          (ProtocolFeatures<D>::s_useSettlDateAsSegmOrd ||
           ProtocolFeatures<D>::s_useTenorAsSegmOrd))   ||
        (m_sessMgr->IsMktData()   &&
          (ProtocolFeatures<D>::s_useSettlDateAsSegmMkt ||
           ProtocolFeatures<D>::s_useTenorAsSegmMkt)))
      STR_FLD("64=",   a_segm_id)

    // Otherwise, None spefified: Nothing to do:
    return curr;
  }

  //=========================================================================//
  // "CompleteSendLog":                                                      //
  //=========================================================================//
  // To Complete, Send Out and Log a FIX msg. Returns the socket send time (may
  // be empty if the send was unsuccessful):
  //
  template
  <
    DialectT::type D,
    bool           IsServer,
    typename       SessMgr,
    typename       Processor
  >
  template<MsgT::type Type>
  inline utxx::time_val
  ProtoEngine<D,   IsServer, SessMgr, Processor>::CompleteSendLog
  (
    SessData*      a_sess,
    char*          a_body_begin,
    char*          a_body_end,
    bool           a_batch_send
  )
  const
  {
    assert(a_sess != nullptr);

    // CheckSum and Length (incl the very final terminating SOH): NB: The curr
    // msg begins from "m_outBuffLen" offset:
    int len = CompleteMsg(m_outBuff + m_outBuffLen, a_body_begin, a_body_end);

    // Increment the total buff length. NB: This is only done if the msg is suc-
    // cessfully places into the buffer (a partial msg, if remains there for any
    // reason, will be over-written):
    m_outBuffLen += len;

    CHECK_ONLY
    (
      if (utxx::unlikely(m_outBuffLen > int(sizeof(m_outBuff))))
      {
        // XXX: Dump the whole buffer -- we want to see why we got an overflow:
        m_sessMgr->m_logger->critical
          ("FIX::ProtoEngine::CompleteSendLog: OutBuff OverFlow: CurrSize={}, "
           "MaxSize={}:\n{}", m_outBuffLen,   sizeof(m_outBuff), m_outBuff);
        m_sessMgr->m_logger->flush();
        throw utxx::runtime_error
              ("FIX::ProtoEngine::CompleteSendLog: OutBuff OverFlow");
      }
    )
    // Send it up. NB: "sendTime" may be empty if the msg was buffered rather
    // than sent out straight away, but we don't correct it here   because it
    // may be unused by the caller:
    utxx::time_val    sendTime;     // Initially empty
    assert(m_outBuffLen > 0);       // There is something in the OutBuff!

    if (!a_batch_send)
      sendTime = FlushOrdersImpl(a_sess);

    // Increment the TxSN in any case (even if the send was delayed) -- but do
    // it AFTER  the possible send:
    ++(*(a_sess->m_txSN));

    // NB: No logging or buffer re-setting occurs in case of BatchSend. In the
    // latter case, "sendTime" also remains empty:
    return sendTime;
  }

  //=========================================================================//
  // "FlushOrdersImpl":                                                      //
  //=========================================================================//
  // Returns SendTime:
  template
  <
    DialectT::type D,
    bool           IsServer,
    typename       SessMgr,
    typename       Processor
  >
  inline utxx::time_val
  ProtoEngine<D,   IsServer, SessMgr, Processor>::FlushOrdersImpl
    (SessData* a_sess)
  const
  {
    assert(a_sess != nullptr && 0 <= m_outBuffLen);
    CHECK_ONLY
    (
      if (utxx::unlikely(m_outBuffLen > int(sizeof(m_outBuff))))
        throw utxx::runtime_error
              ("FIX::ProtoEngine::FlushOrdersImpl: OutFull OverFlow: CurrSize=",
               m_outBuffLen, ", MaxSize=", sizeof(m_outBuff));
    )
    if (utxx::unlikely(m_outBuffLen == 0))
      // This could only happen if this method was invoked from "FlushOrders-
      // Impl" on an empty Buffer. In this case, there is no SendTime:
      return utxx::time_val();

    // Generic Case: Send out the Buffer contents now! Ie, there are data to
    // send in the OutBuff, and the send is NOT to be delayed:
    // Attempt actual sending (the msg may still be buffered  if the socket
    // is not directly writable, and then sent out automatically). XXX:  We
    // do NOT deal with Reactor and FDs here -- the actual send is delegated
    // to the SessMgr:
    utxx::time_val sendTime =
      m_sessMgr->SendImpl(a_sess, m_outBuff, m_outBuffLen);

    // Log the msg(s) sent:
    LogMsg<true>(m_outBuff, m_outBuffLen);

    // Reset the Buffer in any case -- we have sent, or pretended to have sent,
    // all data accumulated in the Buffer:
    m_outBuffLen = 0;
    return  sendTime;
  }

  //=========================================================================//
  // Sending Out Session-Level (Admin) Msgs:                                 //
  //=========================================================================//
  //=========================================================================//
  // "SendLogOn":                                                            //
  //=========================================================================//
  template
  <
    DialectT::type D,
    bool           IsServer,
    typename       SessMgr,
    typename       Processor
  >
  inline void ProtoEngine<D, IsServer, SessMgr, Processor>::SendLogOn
    (SessData* a_sess)
  const
  {
    // NB:
    // (*) If both the curr TxSN and the curr RxSN are 1,  then we request the
    //     Server to perform full "SeqNum"s reset on its side as well;
    // (*) If this is is a MktData-only session, XXX we do not maintain persis-
    //     tent SNs, so always reset them to 1:
    //
    assert(a_sess != nullptr && *(a_sess->m_txSN) > 0 && *(a_sess->m_rxSN) > 0);
    if (m_sessMgr->IsMktData()  && !(m_sessMgr->IsOrdMgmt()))
    {
      *(a_sess->m_txSN) = 1;
      *(a_sess->m_rxSN) = 1;
    }
    bool resetSeqNums = (*(a_sess->m_txSN) == 1 && *(a_sess->m_rxSN) == 1);

    auto preamble = MkPreamble<MsgT::LogOn>(a_sess);
    char* curr    = std::get<0>(preamble);
    char* msgBody = std::get<1>(preamble);

    // Specific "LogOn" Flds:
    EXPL_FLD("98=0")                                    // EncryptMethod
    INT_FLD( "108=",   a_sess->m_heartBeatSec)          // HeartBeatInt

    if (resetSeqNums)
      EXPL_FLD("141=Y")                                 // ResetSeqNumFlag

    if (utxx::likely (!IsEmpty(a_sess->m_userName)))
      STR_FLD("553=", a_sess->m_userName.data())
                                                        // UserName, if present
    if (utxx::likely (!IsEmpty(a_sess->m_passwd)))
    {
      // Password, if present
      if constexpr (D == DialectT::TT)
        STR_FLD("96=", a_sess->m_passwd.data())
      else
        STR_FLD("554=", a_sess->m_passwd.data())

    }
    // Request Passwd Change (if supported)?
    // the new passwd is same as the curr one:
    if (utxx::unlikely
       (ProtocolFeatures<D>::s_hasPasswdChange &&
        !IsEmpty(a_sess->m_newPasswd)          &&
        a_sess->m_newPasswd != a_sess->m_passwd))
      STR_FLD("925=", a_sess->m_newPasswd.data())

    if constexpr
      (ProtocolFeatures<D>::s_fixVersionNum >= ApplVerIDT::FIX50)
      CHAR_FLD("1137=", ProtocolFeatures<D>::s_fixVersionNum)

    // Go! (Never Delayed):
    (void) CompleteSendLog<MsgT::LogOn>(a_sess, msgBody, curr, false);
  }

  //=========================================================================//
  // Sending Out Application-Level Logon:                                    //
  //=========================================================================//
  //=========================================================================//
  // "SendAppLogOn":                                                         //
  //=========================================================================//
  template
  <
    DialectT::type D,
    bool           IsServer,
    typename       SessMgr,
    typename       Processor
  >
  inline void
  ProtoEngine<D, IsServer, SessMgr, Processor>::SendAppLogOn
    (SessData* a_sess)
  const
  {
    assert(a_sess != nullptr);

    auto preamble = MkPreamble<MsgT::UserRequest>(a_sess);
    char* curr              = std::get<0>(preamble);
    char* msgBody           = std::get<1>(preamble);
    utxx::time_val createTS = std::get<2>(preamble);

    // Specific "AppLogOn" Flds:
    if constexpr (*(ProtocolFeatures<D>::s_custApplVerID) != '\0')
      STR_FLD("1129=", ProtocolFeatures<D>::s_custApplVerID)
                                                     // CustApplVerID

    OrderID reqID = MkTmpReqID(createTS);
    curr  = MkReqID(a_sess, curr, "923=", reqID);    // UseRequestID

    // Store last user request id into session object
    a_sess->m_userReqID = reqID;
    CHAR_FLD("924=", UserRequestT::LogOn)            // UserRequestType

    CHECK_ONLY
    (
      if (utxx::unlikely (IsEmpty(a_sess->m_userName)))
        throw utxx::runtime_error
                  ("FIX::SendersSess::SendAppLogOn: Empty Username");
    )
    STR_FLD("553=", a_sess->m_userName.data())       // UserName, if present

    if (utxx::likely (!IsEmpty(a_sess->m_passwd)))
      STR_FLD("554=", a_sess->m_passwd.data())
                                                     // Password, if present
    // Go! (Never Delayed):
    (void) CompleteSendLog<MsgT::UserRequest>(a_sess, msgBody, curr, false);
  }

  //=========================================================================//
  // "SendLogOut":                                                           //
  //=========================================================================//
  template
  <
    DialectT::type D,
    bool           IsServer,
    typename       SessMgr,
    typename       Processor
  >
  inline void
  ProtoEngine<D, IsServer, SessMgr, Processor>::SendLogOut
    (SessData* a_sess)
  const
  {
    assert(a_sess != nullptr);
    auto  preamble = MkPreamble<MsgT::LogOut>(a_sess);
    char* curr     = std::get<0>(preamble);
    char* msgBody  = std::get<1>(preamble);

    // No specific "LogOut" flds to be installed. So go (and do not buffer the
    // msg deliberately):
    (void) CompleteSendLog<MsgT::LogOut>(a_sess, msgBody, curr, false);
  }

  //=========================================================================//
  // "SendAppLogOff":                                                        //
  //=========================================================================//
  template
  <
    DialectT::type D,
    bool           IsServer,
    typename       SessMgr,
    typename       Processor
  >
  inline void
  ProtoEngine<D, IsServer, SessMgr, Processor>::SendAppLogOff
    (SessData* a_sess)
  const
  {
    assert(a_sess != nullptr);
    auto  preamble = MkPreamble<MsgT::UserRequest>(a_sess);
    char* curr     = std::get<0>(preamble);
    char* msgBody  = std::get<1>(preamble);

    // TODO STR_FLD( "923=", a_user_req_id);         // UseRequestID

    CHAR_FLD("924=", UserRequestT::LogOff)           // UserRequestType

    if (utxx::likely(!IsEmpty(a_sess->m_userName)))
      STR_FLD("553=", a_sess->m_userName.data())     // UserName, if present

    (void) CompleteSendLog<MsgT::UserRequest>(a_sess, msgBody, curr, false);
  }

  //=========================================================================//
  // "SendResendRequest":                                                    //
  //=========================================================================//
  // NB: Here "upto" can be 0, which means "resend all msgs from the given Seq-
  // Num, and continue on...":
  template
  <
    DialectT::type D,
    bool           IsServer,
    typename       SessMgr,
    typename       Processor
  >
  inline void
  ProtoEngine<D, IsServer, SessMgr, Processor>::SendResendRequest
  (
    SessData*      a_sess,
    SeqNum         a_from,
    SeqNum         a_upto
  )
  const
  {
    assert(a_sess != nullptr);
    CHECK_ONLY
    (
      if (utxx::unlikely(a_from <= 0 || (a_upto != 0 && a_upto < a_from)))
        throw utxx::badarg_error
              ("FIX::ProtoEngine::ResendRequest: Invalid Range: ",
               a_from, " .. ", a_upto);
    )
    auto  preamble = MkPreamble<MsgT::ResendRequest>(a_sess);
    char* curr     = std::get<0>(preamble);
    char* msgBody  = std::get<1>(preamble);

    // Specific "ResendRequest" Flds:
    INT_FLD("7=",  a_from)
    INT_FLD("16=", a_upto)

    // Go (and do not buffer the msg deliberately):
    a_sess->m_resendReqTS =
      CompleteSendLog<MsgT::ResendRequest>(a_sess, msgBody, curr, false);
  }

  //=========================================================================//
  // "SendGapFill":                                                          //
  //=========================================================================//
  // Generates as "SequenceReset-GapFill" msg. NB: we currently do NOT support
  // a "SequenceReset-Reset"; in case of serious error, we rather re-connect:
  // NB: Here both "from" and "upto" must be non-0:
  template
  <
    DialectT::type D,
    bool           IsServer,
    typename       SessMgr,
    typename       Processor
  >
  inline void
  ProtoEngine<D, IsServer, SessMgr, Processor>::SendGapFill
  (
    SessData*      a_sess,
    SeqNum         a_from,
    SeqNum         a_upto
  )
  const
  {
    assert(a_sess != nullptr);
    CHECK_ONLY
    (
      if (utxx::unlikely(a_from <= 0 || a_upto  < a_from))
        throw utxx::badarg_error
              ("FIX::ProtoEngine::GapFill: Invalid Range: ",
               a_from, " .. ", a_upto);
    )
    // Preamble: Because it is a "GapFill", it is sent in response to "Resend-
    // Request" from the Server (instead of actually re-sending that range of
    // msgs -- we never do the latter for semantical reasons),  so it MUST be
    // a "Dup".
    // And to avoid any confusion, the "SequenceReset-GapFill" msg itself must
    // be sent with the curr SeqNum=a_from:
    //
    *(a_sess->m_txSN) = a_from;
    auto preamble     = MkPreamble<MsgT::SequenceReset>(a_sess);
    char* curr        = std::get<0>(preamble);
    char* msgBody     = std::get<1>(preamble);

    // Specific "GapFill" fld: SeqNum beyond the gap:
    SeqNum next    = a_upto + 1;
    INT_FLD( "36=",  next)     // Where to reset to
    EXPL_FLD("123=Y")          // GapFill

    // Go (and do not buffer the msg deliberately):
    (void) CompleteSendLog<MsgT::ResendRequest>
      (a_sess, msgBody, curr, false);

    // Adjust our next TxSN: This is always guaranteed to be monotonic:
    assert(next > a_from);
    *(a_sess->m_txSN) = next;
  }

  //=========================================================================//
  // "SendTestRequest":                                                      //
  //=========================================================================//
  // Useful, in particular, just after "LogOn" to verify that the "SeqNum"s are
  // indeed in sync between the Server and the Client:
  template
  <
    DialectT::type D,
    bool           IsServer,
    typename       SessMgr,
    typename       Processor
  >
  inline void
  ProtoEngine<D, IsServer, SessMgr, Processor>::SendTestRequest
    (SessData* a_sess)
  const
  {
    assert(a_sess != nullptr);
    auto preamble = MkPreamble<MsgT::TestRequest>(a_sess);
    char* curr              = std::get<0>(preamble);
    char* msgBody           = std::get<1>(preamble);
    utxx::time_val createTS = std::get<2>(preamble);

    // "TestRequest" Specific fld: TestReqID. XXX: We take a random 31-bit int
    // coming from the curr microsecond time stamp:
    OrderID reqID = MkTmpReqID(createTS);
    INT_FLD("112=", reqID)

    // Go (and do not buffer the msg deliberately). The TestReqTS is memoised;
    // it is currently used on the Server-Side only:
    a_sess->m_testReqTS =
      CompleteSendLog<MsgT::ResendRequest>(a_sess, msgBody, curr, false);
  }

  //=========================================================================//
  // "SendHeartBeat":                                                        //
  //=========================================================================//
  template
  <
    DialectT::type D,
    bool           IsServer,
    typename       SessMgr,
    typename       Processor
  >
  inline void
  ProtoEngine<D, IsServer, SessMgr, Processor>::SendHeartBeat
  (
    SessData*      a_sess,
    char const*    a_test_req_id
  )
  const
  {
    auto  preamble = MkPreamble<MsgT::HeartBeat>(a_sess);
    char* curr     = std::get<0>(preamble);
    char* msgBody  = std::get<1>(preamble);

    // Specific "HeartBeat" fld: Install TestReqID if provided:
    if (a_test_req_id != nullptr && *a_test_req_id != '\0')
      STR_FLD("112=", a_test_req_id)

    // Go (and do not buffer the msg deliberately):
    (void) CompleteSendLog<MsgT::HeartBeat>
      (a_sess, msgBody, curr, false);
  }

  //=========================================================================//
  // "SendMarketDataRequest":                                                //
  //=========================================================================//
  template
  <
    DialectT::type D,
    bool           IsServer,
    typename       SessMgr,
    typename       Processor
  >
  template<bool IsSubscribe>
  inline OrderID
  ProtoEngine<D, IsServer, SessMgr, Processor>::SendMarketDataRequest
  (
    SessData*      a_sess,
    SecDefD const& a_instr,
    OrderID        a_subscr_id,
    int            a_mkt_depth,
    bool           a_with_obs,
    bool           a_with_trds
  )
  const
  {
    assert(a_sess != nullptr && a_mkt_depth >= 0);
    //-----------------------------------------------------------------------//
    // Checks & Preamble:                                                    //
    //-----------------------------------------------------------------------//
    CHECK_ONLY
    (
      if (utxx::unlikely(!(m_sessMgr->IsMktData())))
        throw utxx::runtime_error
              ("FIX::ProtoEngine::MarketDataRequest: Not Supported");
    )
    // First of all, UnSubscribe with ReqID=0 is ignored:
    if (utxx::unlikely(!IsSubscribe && a_subscr_id == 0))
      return 0;

    bool tradesOnly = a_with_trds && !a_with_obs;
    CHECK_ONLY
    (
      // Check the Params:
      if (utxx::unlikely(IsSubscribe != (a_subscr_id == 0)))
        throw utxx::badarg_error
              ("FIX::ProtoEngine::MarketDataRequest: IsSubscribe=", IsSubscribe,
               " but SubscrID=", a_subscr_id);

      // We require that the Connector must be fully Active when sending this
      // Req:
      if (utxx::unlikely(!(m_sessMgr->IsActiveSess(a_sess))))
        throw utxx::runtime_error
              ("FIX::ProtoEngine::MarketDataRequest: FIX Session Not Active -- "
               "Cannot Proceed");

      // At least one of OrderBooks and Trades must be requested:
      if (utxx::unlikely(!(a_with_obs || a_with_trds)))
        throw utxx::badarg_error
              ("FIX::ProtoEngine::MarkeDataRequest: Neither OBs nor Trds");

      // XXX: If only Trades are requested, MktDepth must be 1:
      if (utxx::unlikely(tradesOnly && a_mkt_depth != 1))
        throw utxx::badarg_error
              ("FIX::ProtoEngine::MarkeDataRequest: Trades-only MktDepth must "
               "be 1");

      // If Full Orders Log is in use, then MktDepth must be infinite (unless
      // TradesOnly is set):
      if (utxx::unlikely
         (ProtocolFeatures<D>::s_useOrdersLog  &&
          !tradesOnly      &&  a_mkt_depth != 0))
        throw utxx::badarg_error
              ("FIX::ProtoEngine::MarkeDataRequest: FullOrdersLog implies Mkt"
               "Depth=0");
    )
    auto  preamble = MkPreamble<MsgT::MarketDataRequest>(a_sess);
    char*          curr     = std::get<0>(preamble);
    char*          msgBody  = std::get<1>(preamble);
    utxx::time_val createTS = std::get<2>(preamble);

    //-----------------------------------------------------------------------//
    // "MarketDataRequest": Specific Flds:                                   //
    //-----------------------------------------------------------------------//
    // RequestID: Similar to "SendTestRequest", we make this ID from the curr
    // microsecond time stamp:
    //
    OrderID  reqID = IsSubscribe ? OrderID(MkTmpReqID(createTS)): a_subscr_id;
    curr = MkReqID(a_sess, curr, "262=", reqID);            // MDReqID

    // Subscribe or UnSubscribe? NB: There is a special val here for TradesOnly,
    // and special treatment of AlfaFIX2:
    CHAR_FLD
    (
      "263=",                                               // SubscrReqType
      IsSubscribe
      ? (tradesOnly                                         // XXX Special...
         ? SubscrReqTypeT::TradesOnly
         : (D == DialectT::AlfaFIX2)
           ? SubscrReqTypeT::SnapShot                       // XXX Special...
           : SubscrReqTypeT::Subscribe)                     // Generic Case
      : SubscrReqTypeT::UnSubscribe
      )
    // XXX: The rest of the fields are theoretically not needed for UnSubscr,
    // but almost all FIX Dialects still require them, and there is no harm
    // in providing them anyway:
    //
    // MarketDepth: 0 is OK, and means "Full Depth" -- XXX: do we need it for
    // UnSubsr as well.  XXX: Currently, we subscribe Full Depth of the Order
    // Book no matter how many levels are actually used by the Client;   Full
    // Depth may be required for correct reconstruction of the Book: Depth==0
    // means +oo. Also required for UnSubscribe!
    //
    if constexpr (ProtocolFeatures<D>::s_hasMktDepth)
      INT_FLD("264=", a_mkt_depth)                          // MarketDepth

    // We request 1, 2 or 3 entry types: Trades, Bids, Offers:
    CHECK_ONLY
    (
      // If the Trades are requested but not supported by the Protocol, throw
      // an exception:
      if (utxx::unlikely
         (a_with_trds && !ProtocolFeatures<D>::s_hasMktDataTrades))
        throw utxx::badarg_error
              ("FIX::ProtoEngine::MarketDataRequest: Trades Not Supported");

      // If the Protocol supports Trades subscription separately from OrderBook
      // Updates, they must not be requested together:
      if (utxx::unlikely
         (a_with_trds && a_with_obs &&
          ProtocolFeatures<D>::s_hasSepMktDataTrdSubscrs))
        throw utxx::badarg_error
              ("FIX::ProtoEngine::MarketDataRequest: Trades must be managed "
               "separately from OrderBooks");
    )
    // In a peculiar case of "AlfaFIX2", the following group is not supported
    // at all; in all normal cases, it its needed:
    if constexpr (D != DialectT::AlfaFIX2)
    {
      if (a_with_obs)
      {
        // With Trades: 3 Entries; otherwise, 2:
        if (a_with_trds)
        {
          EXPL_FLD("267=3")                               // NoMDEntryTypes
          CHAR_FLD("269=", MDEntryTypeT::Trade)           // Trades
        }
        else
          EXPL_FLD("267=2")                               // NoMDEntryTypes

        // Always request Bid and Ask Pxs:
        CHAR_FLD("269=", MDEntryTypeT::Bid)               // MDEntryTypes...
        CHAR_FLD("269=", MDEntryTypeT::Offer)
      }
      else
      {
        // No OrderBooks, ie Trades ony
        assert(tradesOnly);
        EXPL_FLD("267=1")
        CHAR_FLD("269=", MDEntryTypeT::Trade)
      }
    }
    // If IncrUpdates are supported: Request them (XXX: otherwise, we would get
    // SnapShots by default -- no need to specify them explicitly):
    if constexpr (ProtocolFeatures<D>::s_hasIncrUpdates)
      CHAR_FLD("265=", MDUpdateTypeT::IncrRefresh)

    // For LMAX we need to explicitly state 265=0
    if constexpr((D == DialectT::LMAX) && IsSubscribe)
      CHAR_FLD("265=", MDUpdateTypeT::FullRefresh)

    // If FullOrdersLog is requested, we may need to say so explicitly (at the
    // moment, for Currenex only):
    if (ProtocolFeatures<D>::s_useExplOrdersLog && !tradesOnly)
    {
      // NB: OrdersLog implies AggregatedBook=N:
      CHAR_FLD("266=", ProtocolFeatures<D>::s_useOrdersLog ? 'N' : 'Y')
    }
    // Symbols come in a Group of Size=1:
    EXPL_FLD("146=1")                                       // NoRelatedSym
    MK_SYMBOL(&a_instr)                                     // Symbol

    // Market Segment (same for all Symbols), if required. In particular, it
    // could be the SettlDate ("64="):
    if constexpr (ProtocolFeatures<D>::s_useSegmInMDSubscr)
      curr = MkSegmSessDest(curr, a_instr);

    //-----------------------------------------------------------------------//
    // Go! (No deliberate buffering for this msg type):                      //
    //-----------------------------------------------------------------------//
    (void) CompleteSendLog<MsgT::MarketDataRequest>
      (a_sess, msgBody, curr, false);

    // Log this particular req: As there is no "Req12" created for it, if it
    // fails, we could manually inspect the main log to find out the details:
    //
    if (m_sessMgr->m_debugLevel >= 1)
      m_sessMgr->m_logger->info
        ("FIX::ProtoEngine::MarketDataRequest: Request Sent: Symbol={}: {}, "
         "ReqID={}", a_instr.m_Symbol.data(),
         IsSubscribe ? "Subscribe" : "UnSubscribe",  reqID);

    return reqID;
  }

  //=========================================================================//
  // "SendQuoteRequest":                                                     //
  //=========================================================================//
  template
  <
    DialectT::type D,
    bool           IsServer,
    typename       SessMgr,
    typename       Processor
  >
  template<QtyTypeT QT1>
  inline OrderID ProtoEngine<D, IsServer, SessMgr, Processor>::SendQuoteRequest
  (
    SessData*      a_sess,
    SecDefD const& a_instr,
    Qty<QT1,QR>    a_quantity
  )
  const
  {
    assert(a_sess != nullptr);
    CHECK_ONLY
    (
      // We require that the Connector must be fully Active when sending this
      // Req:
      if (utxx::unlikely(!(m_sessMgr->IsActiveSess(a_sess))))
        throw utxx::runtime_error
              ("FIX::ProtoEngine::SendQuoteRequest: FIX Session Not Active -- "
               "Cannot Proceed");

      if (utxx::unlikely
         (a_quantity > 0 && QT1 != QtyTypeT::QtyA && QT1 != QtyTypeT::QtyB))
        throw utxx::runtime_error
              ("FIX::ProtoEngine::SendQuoteRequest: Quantity Type must be "
               "either QtyA or QtyB");
    )
    auto  preamble = MkPreamble<MsgT::QuoteRequest>(a_sess);
    char*          curr     = std::get<0>(preamble);
    char*          msgBody  = std::get<1>(preamble);
    utxx::time_val createTS = std::get<2>(preamble);

    //-----------------------------------------------------------------------//
    // "QuoteRequest": Specific Flds:                                        //
    //-----------------------------------------------------------------------//
    // RequestID: Similar to "SendTestRequest", we make this ID from the curr
    // microsecond time stamp:
    //
    OrderID  reqID = OrderID(MkTmpReqID(createTS));
    curr = MkReqID(a_sess, curr, "131=", reqID);            // QuoteReqID

    // Symbols come in a Group of Size=1:
    EXPL_FLD("146=1")                                       // NoRelatedSym
    MK_SYMBOL(&a_instr)                                     // Symbol

    // If a_quantity is not 0, add the quantity and corresponding ccy:
    if (a_quantity > 0)
    {
      QTY_FLD("38=", a_quantity)

      char const* ccy =
        (QT1 == QtyTypeT::QtyA)
        ? a_instr.m_AssetA.data()
        : a_instr.m_QuoteCcy.data();
      STR_FLD("15=", ccy)
    }
    //-----------------------------------------------------------------------//
    // Go! (No deliberate buffering for this msg type):                      //
    //-----------------------------------------------------------------------//
    (void) CompleteSendLog<MsgT::QuoteRequest>
      (a_sess, msgBody, curr, false);

    // Log this particular req: As there is no "Req12" created for it, if it
    // fails, we could manually inspect the main log to find out the details:
    //
    if (m_sessMgr->m_debugLevel >= 1)
      m_sessMgr->m_logger->info
        ("FIX::ProtoEngine::QuoteRequest: Request Sent: Symbol={}, ReqID={}",
            a_instr.m_Symbol.data(), reqID);

    return reqID;
  }

  //=========================================================================//
  // "SendQuoteCancel":                                                      //
  //=========================================================================//
  template
  <
    DialectT::type D,
    bool           IsServer,
    typename       SessMgr,
    typename       Processor
  >
  inline void
  ProtoEngine<D, IsServer, SessMgr, Processor>::SendQuoteCancel
  (
    SessData*         a_sess,
    const char        a_quote_id[64],
    SeqNum            a_quote_version     // Cumberland only
  )
  const
  {
    assert(a_sess != nullptr);
    CHECK_ONLY
    (
      // We require that the Connector must be fully Active when sending this
      // Req:
      if (utxx::unlikely(!(m_sessMgr->IsActiveSess(a_sess))))
        throw utxx::runtime_error
              ("FIX::ProtoEngine::SendQuoteCancel: FIX Session Not Active -- "
               "Cannot Proceed");
    )
    auto  preamble = MkPreamble<MsgT::QuoteCancel>(a_sess);
    char*          curr     = std::get<0>(preamble);
    char*          msgBody  = std::get<1>(preamble);

    STR_FLD("117=", a_quote_id)

    // XXX: Cumberland requires a very peculiar QuoteVersion:
    if constexpr (D == DialectT::Cumberland)
      { INT_FLD("20000=", a_quote_version) }

    //-----------------------------------------------------------------------//
    // Go! (No deliberate buffering for this msg type):                      //
    //-----------------------------------------------------------------------//
    (void) CompleteSendLog<MsgT::QuoteCancel>
      (a_sess, msgBody, curr, false);

    // Log this particular req: As there is no "Req12" created for it, if it
    // fails, we could manually inspect the main log to find out the details:
    //
    if (m_sessMgr->m_debugLevel >= 1)
      m_sessMgr->m_logger->info
        ("FIX::ProtoEngine::QuoteCancel: Request Sent: QuoteID={}",
            a_quote_version);
  }

  //=========================================================================//
  // "SendSecurityListRequest":                                              //
  //=========================================================================//
  // Send a SecurityListRequest
  template
  <
    DialectT::type D,
    bool           IsServer,
    typename       SessMgr,
    typename       Processor
  >
  inline void
  ProtoEngine<D, IsServer, SessMgr, Processor>::SendSecurityListRequest
    (SessData*   a_sess)
  const
  {
    assert(a_sess != nullptr);
    auto preamble = MkPreamble<MsgT::SecurityListRequest>(a_sess);
    char* curr              = std::get<0>(preamble);
    char* msgBody           = std::get<1>(preamble);
    utxx::time_val createTS = std::get<2>(preamble);

    // "SecurityReqID" Specific fld: SecurityReqID. XXX: We take a random 31-bit
    // int coming from the curr microsecond time stamp:
    OrderID reqID = MkTmpReqID(createTS);
    INT_FLD( "320=", reqID)

    // XXX: The following is highly Dialect-specific:
    if constexpr (D == DialectT::Cumberland)
    {
      EXPL_FLD("559=4")  // SecurityListRequestType = AllSecurities
      EXPL_FLD("263=1")  // SubscrRequestType       = Subscribe
    }
    else
    if constexpr (D == DialectT::Currenex)
    {
      EXPL_FLD("559=0")  // SecurityListRequestType = Symbol
      EXPL_FLD("55=NA")  // But Symbol is NA!
      EXPL_FLD("460=4")  // Product = Currency
    }
    // Go (and do not buffer the msg deliberately):
    a_sess->m_testReqTS =
      CompleteSendLog<MsgT::ResendRequest>(a_sess, msgBody, curr, false);
  }

  //=========================================================================//
  // "SendSecurityDefinitionRequest":                                        //
  //=========================================================================//
  // Send a SecurityDefinitionRequest
  template
  <
    DialectT::type D,
    bool           IsServer,
    typename       SessMgr,
    typename       Processor
  >
  inline void
  ProtoEngine<D, IsServer, SessMgr, Processor>::SendSecurityDefinitionRequest
    (SessData* a_sess, const char* a_sec_type, const char* a_exch_id)
  const
  {
    assert(a_sess != nullptr);
    auto preamble = MkPreamble<MsgT::SecurityDefinitionRequest>(a_sess);
    char* curr              = std::get<0>(preamble);
    char* msgBody           = std::get<1>(preamble);
    utxx::time_val createTS = std::get<2>(preamble);

    // "SecurityReqID" Specific fld: SecurityReqID. XXX: We take a random 31-bit
    // int coming from the curr microsecond time stamp:
    OrderID reqID = MkTmpReqID(createTS);
    INT_FLD( "320=", reqID)
    STR_FLD( "167=", a_sec_type)
    STR_FLD( "207=", a_exch_id)
    EXPL_FLD("17000=Y")

    // Go (and do not buffer the msg deliberately):
    a_sess->m_testReqTS =
      CompleteSendLog<MsgT::ResendRequest>(a_sess, msgBody, curr, false);
  }

} // End namespace FIX
} // End namespace MAQUETTE
