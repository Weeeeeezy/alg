// vim:ts=2:et
//===========================================================================//
//                             "FIX/Processors1.hpp":                        //
//                      Processors of Session-Level FIX Msgs                 //
//===========================================================================//
#pragma once

#include "FixEngine/Acceptor/EAcceptor_FIX.h"
#include <cstring>
#include <cassert>

namespace AlfaRobot
{
  //=========================================================================//
  // Utils:                                                                  //
  //=========================================================================//
  //=========================================================================//
  // "FIXSession::CheckRxSN":                                                //
  //=========================================================================//
  // Verifies the RxSN of the received msg. Returns "true" if OK, "false" if
  // the verification fails:
  //
  template<FIX::DialectT::type D, typename Conn, typename Derived>
  template<bool InLogOn>
  inline bool EAcceptor_FIX<D, Conn, Derived>::CheckRxSN
  (
    FIXSession*           a_session,
    FIX::MsgPrefix const& a_msg,
    char           const* a_where
  )
  const
  {
    assert(a_session != nullptr  && a_where != nullptr &&
           a_session->m_rxSN > 0 && a_session->m_txSN > 0);

    //-----------------------------------------------------------------------//
    // Generic -- and the most likely -- case: RSN is OK:                    //
    //-----------------------------------------------------------------------//
    SeqNum currSN = a_msg.m_MsgSeqNum;
    assert(currSN > 0);

    if (utxx::likely(currSN == a_session->m_rxSN))
    {
      // OK: Reset the Gap timer (in case it was active):
      a_session->m_resendReqTS = utxx::time_val();

      // Generically, increment the RxSN -- it may later be modified further if
      // the msg is "SequenceReset":
      ++(a_session->m_rxSN);
      return true;
    }

    //-----------------------------------------------------------------------//
    // Otherwise: Consider the following error conditions:                   //
    //-----------------------------------------------------------------------//
    if (currSN > a_session->m_rxSN)
    {
      //---------------------------------------------------------------------//
      // RxSN Gap was encountered:                                           //
      //---------------------------------------------------------------------//
      // If we are already in the "Gap Mode", we ignore  this error for a short
      // while -- a "ResendRequest" was made to the Peer, but before a response
      // is generated and received by us, we could  continue getting out-of-seq
      // msgs. So impose a time limit on that:
      // XXX: Such a time limit is not strictly necessary on  the  Server  side
      // (the Server can wait indefinitely until the Client resends all missing
      // msgs -- we do not spend our Server resources for buffering out-of-ord-
      // er Client msgs), it is still useful as it protects Clients from  send-
      // ing too many out-of-order msgs which would eventually be ignored  any-
      // way:
      if (!(a_session->m_resendReqTS.empty()))
      {
        //-------------------------------------------------------------------//
        // Yes, already in the "Gap Mode":                                   //
        //-------------------------------------------------------------------//
        long gap_ms =
            (a_msg.m_RecvTime - a_session->m_resendReqTS).milliseconds();
        if  (gap_ms > m_maxGapMSec)
        {
          // Cannot wait for SeqNum recovery anymore, terminate the session:
          char  buff[256];
          char* p = stpcpy(buff, "SeqNum Gap: Expected=");
          (void)utxx::itoa(a_session->m_rxSN, p);
          p       = stpcpy(p,    ", Got=");
          (void)utxx::itoa(currSN, p);
          p       = stpcpy(p,    ": No re-send after ");
          (void)utxx::itoa(gap_ms, p);
          p       = stpcpy(p,    " msec");
          assert(size_t(p - buff) < sizeof(buff));

          // Try graceful termination:
          TerminateSession<true>(a_session, buff, a_where);
        }
        else
        {
          // Otherwise, we simply drop this out-of-seq msg with a warning --
          // this means that the Client did not respond to our ResendRequest
          // yet:
          if (m_debugLevel >= 1)
            m_logger->warn
              ("EAcceptor_FIX::CheckRxSN(")
              << a_where  <<  "): ClientCompID="
              << a_session->m_clientCompID.data()
              << ": Still in SeqNum Gap: Expected=" << a_session->m_rxSN
              << ", Got=" << currSN <<  ": Msg Dropped";
        }
      }
      else
      {
        //-------------------------------------------------------------------//
        // Not in the "Gap Mode" yet -- enter it now:                        //
        //-------------------------------------------------------------------//
        if (m_debugLevel >= 1)
          m_logger->warn
            ("EAcceptor_FIX::CheckRxSN(")
            << a_where  <<  "): ClientCompID="
            << a_session->m_clientCompID.data()
            << ": SeqNum Gap encountered: Expected=" << a_session->m_rxSN
            << ", Got=" << currSN <<   ": Will issue a ResendRequest";

        // If the client is still logging on, issue the "LogOn" confirmation
        // before "ResendRequest":
        if (InLogOn)
          SendLogOn(a_session);

        // Issue a "ResendRequest" from the expected SeqNum to infinity (repre-
        // sented as 0). This is the PREFERRED way   (rather than specifying a
        // concrete upper bound -- the latter may be wrong  if the Server  has
        // already sent more out-of-seq msgs):
        //
        SendResendRequest(a_session, a_session->m_rxSN, 0);

        // From now on, the timing of the Gap will start (but no need for a Ti-
        // merFD -- see above).     XXX: To avoid an additional "now_utc" call,
        // reuse RecvTime" though it might be slightly inaccurate:
        //
        a_session->m_resendReqTS = a_msg.m_RecvTime;
      }
    }
    else
    {
      //---------------------------------------------------------------------//
      // Other way round: RxSN moved BackWards:                              //
      //---------------------------------------------------------------------//
      assert(currSN < a_session->m_rxSN);

      // If the msg is marked as "PossDup", it's OK -- we simply ignore it (in
      // all other cases, "PossDup" flag has no effect at all -- because   our
      // "RxSN"s are strictly sequential, the first msg with a given RxSN   is
      // delivered to the application layer, and all others are ignored anyway):
      //
      // Otherwise (not a "PossDup"): This is a serious error  condition  which
      // results in session termination. (One possibility of how this condition
      // could potentially occur is if multiple "ResendRequests"  were  issued,
      // but on our case we don't do a "ResendRequest" if we are  already  in a
      // Gap Mode -- see above):
      if (!a_msg.m_PossDupFlag)
      {
        char  buff[256];
        char* p = stpcpy(buff, "SeqNum Backwardation: Expected=");
        (void)utxx::itoa(a_session->m_rxSN, p);
        p       = stpcpy(p,    ", Got=");
        (void)utxx::itoa(currSN, p);
        assert(size_t(p - buff) < sizeof(buff));

        // Still try a graceful session termination:
        TerminateSession<true>(a_session, buff, a_where);
      }
    }
    //-----------------------------------------------------------------------//
    // So if we got here: it was an RxSN error:                              //
    //-----------------------------------------------------------------------//
    // NB: a_session->m_rxSN remains unchanged:
    return false;
  }

  //=========================================================================//
  // Session- (Admin-) Level FIX Msgs:                                       //
  //=========================================================================//
  //=========================================================================//
  // "EAcceptor_FIX::ProcessLogOn":                                          //
  //=========================================================================//
  // XXX: Password Change by the Clients is not supported yet:
  //
  template<FIX::DialectT::type D, typename Conn, typename Derived>
  inline void EAcceptor_FIX<D, Conn, Derived>::ProcessLogOn
    (FIXSession* a_session)
  {
    assert(a_session != nullptr);
    FIX::LogOn const& tmsg = m_LogOn;

    //-----------------------------------------------------------------------//
    // Client Authentication:                                                //
    //-----------------------------------------------------------------------//
    // Session was obtained based on "ClientCompID" in "tmsg"; its "ClientSubID"
    // (optional) and "ServerCompID" were taken from the same msg. Now verify it
    // against the configured Logins:
    assert(a_session->m_clientCompID  == tmsg.m_SenderCompID);

    // Get the corresp Login info by ClientCompID (more precisely, its hash va-
    // lue):
    auto res = m_logins.find(a_session->m_clientCompHash);
    if (utxx::unlikely(res == m_logins.end()))
    {
      // Login for this ClientCompID not found at all: This login attempt will
      // be rejected, and the session dropped (still gracefully):
      TerminateSession<true>(a_session, "Unknown SenderCompID", "ProcessLogOn");
      return;
    }

    // Otherwise: Found Login info. NB: It was actually found using the hash of
    // ClientCompID, not the ClientCompID itself, but a collision in XXHash  is
    // extremely improbable:
    // FIXME: Presumable, Server- and Target-CompIDs have different meaning  in
    // "Ecn::Model::Login" and in "Ecn::FixServer::LiquidityFixSession":
    // (*) in the former case, they are taken from Client's perspective;
    // (*) in the latter case, -- from the Server's perspective (???)
    //
    Ecn::Model::Login const& login = res->second;

    // TODO YYY
//    assert(strcmp(login.FixSenderCompId.c_str(),
//                   a_session->m_clientCompID.data()) == 0);
//
//    // XXX: "ClientSubID" is currently NOT in "Login", but "ServerCompID" is --
//    // check it;
//    if (utxx::unlikely
//       (strcmp(login.FixTargetCompId.c_str(), a_session->m_serverCompID.data())
//        != 0))
//    {
//      TerminateSession<true>(a_session, "Invalid TargetCompID", "ProcessLogOn");
//      return;
//    }

    // Check permissions in the Login:
    if (Derived::IsOrdMgmt)
    {
      // Must have OrderEntry permissions ("Open"):
      if (utxx::unlikely
         (login.Permissions          != Ecn::Model::UserPermissions::Open   ||
          login.Account->Permissions != Ecn::Model::UserPermissions::Open   ||
          login.LoginType            != Ecn::Model::LoginType::FixOrder))
      {
        TerminateSession<true>
          (a_session, "No OrderEntry Permissions or Invalid LoginType",
           "ProcessLogOn");
        return;
      }
    }
    if (Derived::IsMktData)
    {
      // Must have at least the MktData permissions ("Indicateive"), ie any but
      // "Closed":
      if (utxx::unlikely
         (login.Permissions          == Ecn::Model::UserPermissions::Closed ||
          login.Account->Permissions == Ecn::Model::UserPermissions::Closed ||
          login.LoginType            != Ecn::Model::LoginType::FixMarket))
      {
        TerminateSession<true>
          (a_session, "No MktData Permissions or Invalid LoginType",
           "ProcessLogOn");
        return;
      }
    }
    // XXX: FIX Logins are currently NOT password-protected (only GUI Logins
    // are!)
    // But in FIX, there is filtering by originating IP address -- to be ob-
    // tained from the Reactor:
    //
    IO::FDInfo const& fdi = m_reactor->GetFDInfo(a_session->m_fd);

    // XXX: The valid originating IP in "Login" is not quite versatile: it is
    // a single IP, not a lost or a netmask(?)
    if (utxx::unlikely
       (login.FixIncomingConnectionIpBin != INADDR_ANY &&
        login.FixIncomingConnectionIpBin != fdi.m_peer.m_inet.sin_addr.s_addr))
    {
      TerminateSession<true>
        (a_session, "Invalid Originating IP Address", "ProcessLogon");
      return;
    }

    //-----------------------------------------------------------------------//
    // "SeqNum"s Mgmt:                                                       //
    //-----------------------------------------------------------------------//
    // May need to reset both SeqNums first (always do so for MktData, or if the
    // corresp global flag is set):

    std::cout << "a_session->m_txSN=" << a_session->m_txSN << " a_session->m_rxSN=" << a_session->m_rxSN << std::endl;
    std::cout << "Derived::IsMktData=" << Derived::IsMktData << " m_resetSeqNumsOnLogOn=" << m_resetSeqNumsOnLogOn << " tmsg.m_ResetSeqNumFlag=" << tmsg.m_ResetSeqNumFlag << std::endl;
    if (Derived::IsMktData || m_resetSeqNumsOnLogOn || tmsg.m_ResetSeqNumFlag)
    {
      a_session->m_txSN = 1;
      a_session->m_rxSN = 1;
    }

    // Check the RxSN (IsLogOn=true):
    if (utxx::unlikely(!CheckRxSN<true>(a_session, tmsg, "ProcessLogOn")))
      // Something wrong with RxSeqNum -- the condition has already been hand-
      // led and logged:
      return;

    //-----------------------------------------------------------------------//
    // Set the mutually-agreed HeartBeat Interval:                           //
    //-----------------------------------------------------------------------//
    // XXX: At the moment, we will
    // be more or less flexible on that, and accept the Client's period it  is
    // in [T/2 .. 2*T] of our standard period T:
    // (*) If the Client's period is too short (eg 1 sec), sending too frequent
    //     HeartBeats is an unnecessary (though not terrible) strain on our re-
    //     sources;
    // (*) If the period is too long, the Client may become inactive but would
    //     still hold up our  connection resources, which is also not terrible
    //     but undesirable:
    assert(tmsg.m_HeartBtInt > 0);    // Guaranteed by the Parser

    if (utxx::unlikely
       (tmsg.m_HeartBtInt < m_heartBeatSec / 2 ||
        tmsg.m_HeartBtInt > m_heartBeatSec * 2))
    {
      // This value cannot be accepted -- terminate the Session gracefully:
      TerminateSession<true>(a_session, "Invalid HeartBtInt", "ProcessLogOn");
      return;
    }
    // If OK: Set the Client's HeartBeatInt value for this Session:
    a_session->m_heartBeatSec = time_t(tmsg.m_HeartBtInt);

    //-----------------------------------------------------------------------//
    // Notifications:                                                        //
    //-----------------------------------------------------------------------//
    // Send the LogOn response.  NB: "CheckRxSN" would have already sent a
    // "LogOn" resp if there was an error:
    std::cout << "SendLogOn()" << std::endl;
    SendLogOn(a_session);

    // If the Protocol Dialect requires the Client to wait for SessionStatus,
    // send that msg as well:
    if (!FIX::ProtocolFeatures<D>::s_waitForTrSessStatus)
      SendSessionStatus(a_session, FIX::TradSesStatusT::Open);

    // Finally, notify the ECN Core that LogOn has been completed, and the Ses-
    // sion is now active:
    Ecn::Basis::SPtr<Ecn::FixServer::LiquidityFixSessionState> sessState =
      Ecn::Basis::MakeSPtr<Ecn::FixServer::LiquidityFixSessionState>
      (
        Ecn::FixServer::LiquidityFixSessionStateType::Logon,
        a_session->m_ecnSession,
        // NB: "m_ecnSession" must be non-NULL (set in "CheckFIXSession" on
        //     LogOn);
        // FIXME: TargetCompID of this Session is the ClientCompID?
        a_session->m_clientCompID.data()
      );
    m_conn->SendFixSessionState(sessState);
  }

  //=========================================================================//
  // "EAcceptor_FIX::ProcessLogOut":                                         //
  //=========================================================================//
  template<FIX::DialectT::type D, typename Conn, typename Derived>
  inline void EAcceptor_FIX<D, Conn, Derived>::ProcessLogOut
    (FIXSession* a_session)
  {
    assert(a_session != nullptr);
    // The Client has sent us a LogOut msg. Do graceful session termination --
    // XXX: we do not even check the RxSN in this case:
    TerminateSession<true>(a_session, "LogOut by Client", "ProcessLogOut");

    // TODO YYY
    Ecn::Basis::SPtr<Ecn::FixServer::LiquidityFixSessionState> sessState =
        Ecn::Basis::MakeSPtr<Ecn::FixServer::LiquidityFixSessionState>
            (
                Ecn::FixServer::LiquidityFixSessionStateType::Logoff,
                a_session->m_ecnSession,
                // NB: "m_ecnSession" must be non-NULL (set in "CheckFIXSession" on
                //     LogOn);
                // FIXME: TargetCompID of this Session is the ClientCompID?
                a_session->m_clientCompID.data()
            );
    m_conn->SendFixSessionState(sessState);
  }

  //=========================================================================//
  // "EAcceptor_FIX::ProcessResendRequest":                                  //
  //=========================================================================//
  // FIXME: This method is to be re-implemented when actual resends become sup-
  // ported:
  template<FIX::DialectT::type D, typename Conn, typename Derived>
  inline void EAcceptor_FIX<D, Conn, Derived>::ProcessResendRequest
    (FIXSession* a_session)
  {
    //-----------------------------------------------------------------------//
    // Check the RxSN (IsLogOn=false):                                       //
    //-----------------------------------------------------------------------//
    assert(a_session != nullptr);
    FIX::ResendRequest const& tmsg = m_ResendRequest;
    if (utxx::unlikely
       (!CheckRxSN<false>(a_session, tmsg, "ProcessResendRequest")))
      return;

    //-----------------------------------------------------------------------//
    // Construct the GapFill Range:                                          //
    //-----------------------------------------------------------------------//
    // The Server is asking us to re-send some of our msgs. XXX: We  currently
    // do not buffer out-bound msgs, so we send a "GapFill" instead:
    // Determine the bounds for the GapFill.  If 0 (means +oo)  upper bound is
    // requested, make it the largest SeqNum sent so far, ie (TxSN-1), because
    // TxSN is NEXT one to be sent:
    //
    SeqNum from = tmsg.m_BeginSeqNo;
    SeqNum upto =
      (tmsg.m_EndSeqNo != 0) ? tmsg.m_EndSeqNo : ((a_session->m_txSN) - 1);

    // The following invariants must hold (in particular, "upto" must be  less
    // than the curr TxSN -- we cannot resend the msgs which were not sent yet
    // -- though for a generic "GapFill", setting  upto >= TxSN would be poss-
    // ible (then without the "PossDup" flag) -- but we don't use such msgs:
    //
    if (utxx::unlikely(from <= 0 || from > upto || upto >= a_session->m_txSN))
    {
      // This would be a serious error condition. In particular, this could hap-
      // pen if TxSN was 1 and we immediately got a "ResendRequest" (but that is
      // not really possible -- then we were not logged on):
      char  buff[128];
      char* p = stpcpy (buff, "Invalid ResendRequest: ");
      (void) utxx::itoa(tmsg.m_BeginSeqNo, p);
      p       = stpcpy (p,    "..");
      (void) utxx::itoa(tmsg.m_EndSeqNo,   p);
      p       = stpcpy (p,    ", TxSN=");
      (void) utxx::itoa(a_session->m_txSN, p);
      assert(size_t(p - buff) < sizeof(buff));

      // So what should we do -- terminate the session or reject such a Resend-
      // Req? We do the former,  because the chances of successful recovery are
      // low -- but still do it gracefully:
      TerminateSession<true>(a_session, buff, "ProcessResendRequest");
      return;
    }

    //-----------------------------------------------------------------------//
    // Send "GapFil" etc out:                                                //
    //-----------------------------------------------------------------------//
    // NB: Its own SeqNum is "TxSN", and 1 <= from <= upto <= TxSN-1, and the
    // "PossDup" flag is set:
    SendGapFill    (a_session, from, upto);

    // XXX: It's probably also a good idea to send a "HeartBeat" and "TestReq"
    // with the following-on SeqNums, to make sure the  Peer has accepted our
    // GapFill:
    SendHeartBeat  (a_session, nullptr);
    SendTestRequest(a_session);

    // All Done!
  }

  //=========================================================================//
  // "EAcceptor_FIX::ProcessSequenceReset":                                  //
  //=========================================================================//
  template<FIX::DialectT::type D, typename Conn, typename Derived>
  inline void EAcceptor_FIX<D, Conn, Derived>::ProcessSequenceReset
    (FIXSession* a_session)
  {
    assert(a_session != nullptr);
    FIX::SequenceReset const& tmsg = m_SequenceReset;
    SeqNum                  newRSN = tmsg.m_NewSeqNo;
    assert(newRSN > 0);  // Because it was parsed as "Positive"

    //-----------------------------------------------------------------------//
    // Check the monotonicity of "newRSN":                                   //
    //-----------------------------------------------------------------------//
    // In any case, decreasing the RxSN is not allowed, no matter whether is a
    // true Reset or a GapFill, and even if the "PossDup" flag is set, because
    // we never issue a "ResendRequest" for SeqNums below our RxSN (this is in
    // turn  because we advance our RxSN only when it is contiguous):
    //
    if (utxx::unlikely(newRSN < a_session->m_rxSN))
    {
      // This is a serious error condition: Terminate the curr Session -- but
      // still gracefully:
      char  buff[256];
      char* p  = stpcpy(buff,   "Got SequenceReset with NewSeqNo=");
      (void) utxx::itoa(newRSN, p);
      p        = stpcpy(p,      " < RxSN=");
      (void) utxx::itoa(a_session->m_rxSN, p);
      assert(size_t(p - buff) < sizeof(buff));

      TerminateSession<true>(a_session, buff, "ProcessSequenceReset");
      return;
    }

    //-----------------------------------------------------------------------//
    // Verify the SeqNum of this msg itself -- but only for "GapFill":       //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely
       (tmsg.m_GapFillFlag &&
       !CheckRxSN<false>(a_session, tmsg, "ProcessSequenceReset")))
      return;

    //-----------------------------------------------------------------------//
    // Do the reset as instructed:                                           //
    //-----------------------------------------------------------------------//
    // XXX: It currently does not really matter whether this msg is solicited
    // (ie it came in response to our "ResendRequest") or not,  or whether it
    // carries a "PossDup" flag (would be the case if solicited):
    //
    a_session->m_rxSN = newRSN;

    if (m_debugLevel >= 2)
      m_logger->info
        ("EAcceptor_FIX::ProcessSequenceReset: ClientCompID=")
        << a_session->m_clientCompID.data()
        << (tmsg.m_GapFillFlag ?  ": GapFill" : ": HardReset")
        << ": NewRxSN=" << newRSN;
    // All Done!
  }

  //=========================================================================//
  // "EAcceptor_FIX::ProcessTestRequest":                                    //
  //=========================================================================//
  template<FIX::DialectT::type D, typename Conn, typename Derived>
  inline void EAcceptor_FIX<D, Conn, Derived>::ProcessTestRequest
    (FIXSession* a_session)
  {
    assert(a_session != nullptr);
    FIX::TestRequest const& tmsg = m_TestRequest;
    if (utxx::unlikely
       (!CheckRxSN<false>(a_session, tmsg, "ProcessTestRequest")))
      return;

    // Generate an immediate "HeartBeat" in response:
    SendHeartBeat(a_session, tmsg.m_TestReqID);
  }

  //=========================================================================//
  // "EAcceptor_FIX::ProcessHeartBeat":                                      //
  //=========================================================================//
  template<FIX::DialectT::type D, typename Conn, typename Derived>
  inline void EAcceptor_FIX<D, Conn, Derived>::ProcessHeartBeat
              (FIXSession* DEBUG_ONLY(a_session))
  {
    // XXX: Currently, nothing really to do here -- "GetFIXSession" has already
    // set the necessary time stamp etc:
    assert(a_session != nullptr);
  }

  //=========================================================================//
  // "EAcceptor_FIX::ProcessReject":                                         //
  //=========================================================================//
  template<FIX::DialectT::type D, typename Conn, typename Derived>
  inline void EAcceptor_FIX<D, Conn, Derived>::ProcessReject
    (FIXSession* a_session)
  {
    assert(a_session != nullptr);

    // The Client did not understand our msg -- so there is a protocol incompa-
    // tibility.  The best thing we can do is to log the error msg and termina-
    // te the session gracefully (because of this, we do not check the RxSN):
    //
    FIX::Reject const& tmsg = m_Reject;

    m_logger->error
      ("EAcceptor_FIX::ProcessReject: REJECTED: ClientCompID=")
      << a_session->m_clientCompID.data()  << ", RefSeqNum="
      << tmsg.m_RefSeqNum << ", RefTagID=" << tmsg.m_RefTagID
      << ", RefMsgType="  << char(tmsg.m_RefMsgType) << ", Reason="
      << tmsg.m_Text      << ", ErrCode="  << int(tmsg.m_SessionRejectReason);

    TerminateSession<true>(a_session, "Reject Received", "ProcessReject");
  }
} // End namespace AlfaRobot
