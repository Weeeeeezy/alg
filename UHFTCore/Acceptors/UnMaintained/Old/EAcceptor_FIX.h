// vim:ts=2:et
//===========================================================================//
//                              "EAcceptor_FIX.h":                           //
//                    Generic Embedded FIX Protocol Acceptor                 //
//===========================================================================//
#pragma once

#include "utxx/time_val.hpp"
#include "utxx/error.hpp"
#include "FixEngine/Basis/BaseTypes.hpp"
#include "FixEngine/Basis/EPollReactor.h"
#include "FixEngine/Acceptor/Msgs.h"
#include "FixEngine/Acceptor/Features.h"

// Integration with the rest of ECN:
#include "FixEngine/OrderAcceptorWrapper.hpp"
#include "FixEngine/MarketAcceptorWrapper.hpp"

#include <boost/container/static_vector.hpp>
#include <boost/interprocess/managed_mapped_file.hpp>
#include <boost/core/noncopyable.hpp>
#include <spdlog/spdlog.h>
#include <memory>
#include <tuple>
#include <netinet/in.h>
#include <cassert>

namespace AlfaRobot
{
  //=========================================================================//
  // "FIXSession" Struct:                                                    //
  //=========================================================================//
  struct FIXSession
  {
    // NB: On the Acceptor side, FIXSession status is only binary ("IsActive"):
    // (*) there is no "Conecting" state here, because the Client socket is only
    //     created after the connection was accepted;
    // (*) there is no  "LoggingOn" state either, because the Session does not
    //     exist before the initial "LogOn" is processed (no ClientCompID yet);
    // (*) and there is no "LogginOut" state, because we always process LogOut
    //     events synchronously, whether they have been initiated by us or by
    //     Client:
    //-----------------------------------------------------------------------//
    // "FIXSession" Data Flds:                                               //
    //-----------------------------------------------------------------------//
    SymKey                  m_clientCompID;   // ClientID
    unsigned long           m_clientCompHash; // Hash of the above
    SymKey                  m_clientSubID;    // Rarely used, hence no hash
    SymKey                  m_serverCompID;   // Normally same for all Clients
    int                     m_fd;             // Client-Server TCP socket
    time_t                  m_heartBeatSec;   // HeartBeat period, sec

    mutable SeqNum          m_txSN;           // Next Tx SeqNum to be used
    mutable SeqNum          m_rxSN;           // Next Rx SeqNum expected
    mutable size_t          m_bytesTx;        // Bytes sent  in this Session
    mutable size_t          m_bytesRx;        // Bytes recvd in this Session
    mutable utxx::time_val  m_lastTxTS;       // When last msg  was  sent
    mutable utxx::time_val  m_lastRxTS;       // When last msg  was received
    mutable utxx::time_val  m_testReqTS;      // When TestReq   was sent
    mutable utxx::time_val  m_resendReqTS;    // When ResendReq was sent

    // The following is for sending Session-specific events via "Conn";
    // "LiquidityFixSession" actually only contains the CompIDs:
    //
    Ecn::Basis::SPtr<Ecn::FixServer::LiquidityFixSession> m_ecnSession;

    //-----------------------------------------------------------------------//
    // Default Ctor:                                                         //
    //-----------------------------------------------------------------------//
    FIXSession()
    : m_clientCompID  (MkSymKey("")),
      m_clientCompHash(0),
      m_clientSubID   (m_clientCompID),
      m_serverCompID  (m_clientCompID),
      m_fd            (-1),
      m_heartBeatSec  (0),
      m_txSN          (1),    // Next expected by default
      m_rxSN          (1),    // Next expected by default
      m_bytesTx       (0),
      m_bytesRx       (0),
      m_lastTxTS      (),
      m_lastRxTS      (),
      m_testReqTS     (),
      m_resendReqTS   (),
      m_ecnSession    ()      // Initially empty (NULL)
    {}

    //-----------------------------------------------------------------------//
    // "IsActive":                                                           //
    //-----------------------------------------------------------------------//
    // The Session is Active iff "m_fd" is set. This also implies that the Cli-
    // entCompID must be set, but the converse is not true: the Session may go
    // dormant with its ClientCompID still there:
    //
    bool IsActive() const
    {
      assert (m_fd <  0 || !IsEmpty(m_clientCompID));
      return (m_fd >= 0);
    }
  };

  //=========================================================================//
  // "EAcceptor_FIX" Class:                                                  //
  //=========================================================================//
  // (*) "Conn" is, resp,  either an OrdMgmt or MktData "Connector" (this is a
  //     misnomer -- that "Connector" simply forwards events to the ECN Core,
  //     would better be called "Forwarded"):
  // (*) "Derived" is either "EAcceptor_FIX_OrdMgmt" or "EAcceptor_FIX_MktData";
  //     the corresp "Conn" params are different in those cases:
  //
  template<FIX::DialectT::type D, typename Conn, typename Derived>
  class EAcceptor_FIX: public boost::noncopyable
  {
  protected:
    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // From Config:                                                          //
    //-----------------------------------------------------------------------//
    std::string const               m_name;          // Instance Name
    sockaddr_in                     m_accptAddr;     // IP and Port to Listen

    // Limits: XXX: Currently, requests rate limit is same for all Sessions /
    // Clients; in the future, they can be configured individually:
    constexpr   static size_t MaxSessions =  3;  // Largest prime < 65536
    constexpr   static int    MaxFDs      =  32768;
    int         const               m_throttlPeriodSec;
    int         const               m_maxReqsPerPeriod;
    // FIX params:
    bool        const               m_resetSeqNumsOnLogOn;
    int         const               m_heartBeatSec;
    int         const               m_maxGapMSec;    // Wait after ResendReq
    int         const               m_maxLatency;    // max Clnt-Srvr drift

    //-----------------------------------------------------------------------//
    // Run-Time Setup:                                                       //
    //-----------------------------------------------------------------------//
    // NB: EPollReactor obj may or may not be owned (normally, it should NOT
    // be):
    std::string const               m_storeFile;     // File to be MMaped
    BIPC::managed_mapped_file       m_storeMMap;
    std::shared_ptr<spdlog::logger> m_loggerShP;
    spdlog::logger*                 m_logger;        // Direct ptr to the above
    int         const               m_debugLevel;
    std::shared_ptr<spdlog::logger> m_rawLoggerShP;  // Special FIX msgs logger
    spdlog::logger*                 m_rawLogger;     // Direct ptr to the above
    std::unique_ptr<EPollReactor>   m_reactorUnP;    // Owned if non-empty
    EPollReactor*                   m_reactor;       // Ptr NOT owned!

    // FIX Sessions (running or dormant): in ShM:
    FIXSession*                     m_sessions;

    // Acceptor always contains an Acceptor Socket. We assume that there is only
    // 1 such socket per Acceptor, though in theory there could be several -- in
    // the Non-Blocking mode, this is entirely feasible:
    mutable int                     m_accptFD;
    mutable int                     m_timerFD;

    // XXX: There is just 1 out-bound msg buffer:  if a send  did not completely
    // empty the buffer, the tail is saved in a per-socket buffer managed by the
    // the EPollReactor itself:
    constexpr static int MaxMsgSize = 8192;
    mutable char                    m_outBuff[MaxMsgSize]; // For out-bound msgs
    mutable char                    m_currMsg[MaxMsgSize]; // On reading
    mutable size_t                  m_currMsgLen;          // <= MaxMszSize

    // Statistics: Total number of bytes sent (Tx) and received (Rx). In parti-
    // cular, it can be used for liveness monitoring:
    mutable size_t                  m_totBytesTx;
    mutable size_t                  m_totBytesRx;

    //-----------------------------------------------------------------------//
    // Buffers for Receiving Typed FIX Msgs:                                 //
    //-----------------------------------------------------------------------//
    // NB: Because mshs are received synchronously from all srcs using EPoll,
    // it is sufficient to have just 1 set of Buffers (NOT per-Session buffs):
    //
    // Session- (Admin-) Level Msgs:
    mutable FIX::LogOn             m_LogOn;
    mutable FIX::LogOut            m_LogOut;
    mutable FIX::ResendRequest     m_ResendRequest;
    mutable FIX::SequenceReset     m_SequenceReset;
    mutable FIX::TestRequest       m_TestRequest;
    mutable FIX::HeartBeat         m_HeartBeat;
    mutable FIX::Reject            m_Reject;

    //-----------------------------------------------------------------------//
    // Integration with the rest of the ECN:                                 //
    //-----------------------------------------------------------------------//
    // Logins: Need a reasonably-efficient map for them:
    // Hash(ClientCompID) => Login:
    std::unordered_map<unsigned long, Ecn::Model::Login>  m_logins;
    Conn*                                                 m_conn;

    //-----------------------------------------------------------------------//
    // Default Ctor is deleted:                                              //
    //-----------------------------------------------------------------------//
    EAcceptor_FIX() = delete;

  public:
    //=======================================================================//
    // Non-Default Ctor, Dtor:                                               //
    //=======================================================================//
    // XXX: The Ctor currently uses Ecn Param types -- would be better to make
    // an environment-independent Ctor:
    EAcceptor_FIX
    (
      Ecn::Basis::Part::TId                        a_id,
      Ecn::FixServer::FixAcceptorSettings const&   a_config,
      Ecn::Basis::SPtrPack<Ecn::Model::Login>      a_logins,
      Conn&                                        a_conn,     // Non-Const Ref!
      EPollReactor*                                a_reactor = nullptr
    );

    ~EAcceptor_FIX() noexcept;

    //=======================================================================//
    // "Start", "Stop", Properties:                                          //
    //=======================================================================//
    void Start();
    void Stop ();

    // "GetName" (Name is same as ServerCompID):
    char const*    GetName()        const { return m_name.data(); }

    // Data Volume Statistics:
    unsigned long  GetTotBytesTx()  const { return m_totBytesTx;  }
    unsigned long  GetTotBytesRx()  const { return m_totBytesRx;  }

    // Is this Acceptor currently Active?
    bool           IsActive()       const { return (m_accptFD >= 0); }

    //=======================================================================//
    // Static API for "{Order,Market}AcceptorWrapper" (ECN) compatibility:   //
    //=======================================================================//
    void ProcessStart() { Start(); }
    void ProcessStop () { Stop (); }

    // One step of Events Loop, driven by an external Caller:
    void Poll()
    {
      // TODO YYY do not call Poll if we have stopped
      if (m_reactor)
      {
        m_reactor->Poll();
      }
    }

  protected:
    //=======================================================================//
    // Handlers:                                                             //
    //=======================================================================//
    // "AcceptHandler":
    // Returns "true" iff conenction from this Client is to be retained (so we
    // proceed to the LogOn stage):
    //
    bool AcceptHandler
    (
      int               a_acceptor_fd,
      int               a_client_fd,
      IO::IUAddr const* a_client_addr,
      int               a_client_addr_len
    );

    // NB: "ReadHandler" is provided by the "Derived" class!

    // "TimerHandler":
    void TimerHandler();

    // "ErrHandler":
    // Call-back invoked on any IO error (via "m_errH" which is registered with
    // the Reactor):
    //
    void ErrHandler
      (int a_fd, int a_err_code, uint32_t a_events, char const* a_msg);

    //=======================================================================//
    // FIX Msg Parsers and Processors:                                       //
    //=======================================================================//
    // Parsers of all msg types (incl those which are processed by derived cls)
    // are located in THIS class, because Parsers are auto-generated:
    //-----------------------------------------------------------------------//
    // Session- (Admin-) Level Msgs:                                         //
    //-----------------------------------------------------------------------//
    void ParseLogOn
         (char* a_msg_body,  char const*    a_body_end,
          int   a_msg_sz,    utxx::time_val a_recv_time) const;

    void ParseLogOut
         (char* a_msg_body,  char const*    a_body_end,
          int   a_msg_sz,    utxx::time_val a_recv_time) const;

    void ParseResendRequest
         (char* a_msg_body,  char const*    a_body_end,
          int   a_msg_sz,    utxx::time_val a_recv_time) const;

    void ParseSequenceReset
         (char* a_msg_body,  char const*    a_body_end,
          int   a_msg_sz,    utxx::time_val a_recv_time) const;

    void ParseTestRequest
         (char* a_msg_body,  char const*    a_body_end,
          int   a_msg_sz,    utxx::time_val a_recv_time) const;

    void ParseHeartBeat
         (char* a_msg_body,  char const*    a_body_end,
          int   a_msg_sz,    utxx::time_val a_recv_time) const;

    void ParseReject
         (char* a_msg_body,  char const*    a_body_end,
          int   a_msg_sz,    utxx::time_val a_recv_time) const;

    void ProcessLogOn        (FIXSession*   a_session);
    void ProcessLogOut       (FIXSession*   a_session);
    void ProcessResendRequest(FIXSession*   a_session);
    void ProcessSequenceReset(FIXSession*   a_session);
    void ProcessTestRequest  (FIXSession*   a_session);
    void ProcessHeartBeat    (FIXSession*   a_session);
    void ProcessReject       (FIXSession*   a_session);

    //-----------------------------------------------------------------------//
    // Application-Level Msgs: Provided by the "Derived" class               //
    //-----------------------------------------------------------------------//

    //=======================================================================//
    // FIX Msg Senders:                                                      //
    //=======================================================================//
    // Sending Session-Level Msgs:
    //
    void SendLogOn        (FIXSession* a_session)                        const;
    void SendLogOut       (FIXSession* a_session, char const* a_msg)     const;
    void SendTestRequest  (FIXSession* a_session)                        const;
    void SendHeartBeat    (FIXSession* a_session, char const* a_treq_id) const;

    void SendSessionStatus
    (
      FIXSession*         a_session,
      FIX::TradSesStatusT a_status
    )
    const;

    void SendResendRequest
    (
      FIXSession*         a_session,
      SeqNum              a_from,
      SeqNum              a_upto
    )
    const;

    void SendGapFill
    (
      FIXSession*         a_session,
      SeqNum              a_from,
      SeqNum              a_upto
    )
    const;

    void SendReject
    (
      FIXSession*         a_session,
      SeqNum              a_rej_sn,
      FIX::MsgT           a_rej_mtype,
      FIX::SessRejReasonT a_reason,
      char const*         a_msg
    )
    const;

    // NB: Senders for Application-Level Msgs are provided by the "Derived" cls
    // (as they differ between OrdMgmt and MktData)

    //=======================================================================//
    // Internal Utils:                                                       //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // Utils for Session Mgmt:                                               //
    //-----------------------------------------------------------------------//
    // Calculate the size of the ShM Segment to be allocated:
    size_t GetMMapSize() const;

    // "GetFIXSession":
    // Non-throwing, can be used in exception handlers; invokes "Stop" in case
    // of critical unrecoverable errors; here "a_fd" is a SocketFD:
    //
    FIXSession* GetFIXSession(int a_fd)           const;
    FIXSession* GetFIXSession(char const* a_ccid) const;
    FIXSession* GetFIXSession
      (Ecn::FixServer::LiquidityFixSession const& a_sess) const;

    // "CheckFIXSession":
    // Verifies FIXSession against the Msg; Re-sets the FIXSession ptr  to NULL
    // in case of a failure, and terminates the Session. Returns "true" iff the
    // Session remains valid, AND the msg can be processed further:
    //
    template<FIX::MsgT MT>
    bool CheckFIXSession
      (int a_fd, FIX::MsgPrefix const& a_msg, FIXSession** a_session) const;

    // "GetCurrMsgSeqNum" -- scanning "m_currMsg" if SeqNum is not available
    // otherwise:
    SeqNum      GetCurrMsgSeqNum()       const;

    // Verify that the Reactor and FIXSession SocketFDs are consistent. In case
    // if they are not, "StopNow" is invoked and "false" returned:
    bool CheckFDConsistency(int a_fd, FIXSession* a_session) const;

    // Terminating a Session: Gracefully (a kind of -- with "LogOut" sent) or
    // Immediately (by a TCP disconnect); the Session itself is identified ei-
    // ther by SocketFD ("a_fd"):
    // NB: Graceful => FindSession. If FindSession is "false", the method sim-
    // ply detaches and closes "a_fd":
    //
    template<bool Graceful, bool FindSession = true>
    void TerminateSession
    (
      int         a_fd,                 // SocketFD
      char const* a_msg,
      char const* a_where
    )
    const;

    // As above, but the Session is identified by a direct ptr:
    template<bool Graceful>
    void TerminateSession
    (
      FIXSession* a_session,            // Must be non-NULL
      char const* a_reason,
      char const* a_where
    )
    const;

    // TODO YYY
    bool CheckReceiveTime(utxx::time_val aReceiveTime) const
    {
      utxx::time_val now = utxx::time_val::universal_time();
      long diff = now.milliseconds() - aReceiveTime.milliseconds();
      if (diff < 0) diff = -diff;
      return diff < m_maxLatency*1000;
    }

    // "RejectMsg" is a wrapper which either:
    // (*) invokes "SendReject", preserves the Session   and returns "true";
    // (*) failing the above, invokes "TerminateSession" and returns "false";
    // makes every effort NOT to propagate exceptions:
    //
    bool RejectMsg
    (
      FIXSession*          a_session,  // Must be non-NULL
      SeqNum               a_rejected_sn,
      FIX::MsgT            a_rejected_mtype,
      FIX::SessRejReasonT  a_reason,
      char const*          a_msg,
      char const*          a_where
    )
    const;

    // Emergency (non-graceful) termination of the whole FIX Acceptor:
    template<bool Graceful>
    void StopNow(char const* a_where) const;

    //-----------------------------------------------------------------------//
    // Utils for Receiving and Sending of FIX Msgs:                          //
    //-----------------------------------------------------------------------//
    // SeqNum verification on received msgs:
    template<bool InLogOn>
    bool CheckRxSN
    (
      FIXSession*           a_session,
      FIX::MsgPrefix const& a_msg,
      char           const* a_where
    )
    const;

    // Logging of Sent and Received FIX Msgs:
    template<bool IsSend>
    void LogMsg(char* a_buff, int a_len) const;

    // Output a FIX Msg Preamble:
    // Returns: (curr_buff_ptr, msg_body_ptr, send_time):
    template<FIX::MsgT Type>
    std::tuple<char*, char*,  utxx::time_val> MkPreamble
      (FIXSession* a_session) const;

    // Complete (incl Check-Sum), Send Out and Log a FIX Msg. Returns send time:
    template<FIX::MsgT Type>
    utxx::time_val CompleteSendLog
      (FIXSession* a_session, char* a_body_begin, char* a_body_end) const;

    // Sending Date and DateTime Flds (ECN Types):
    static char* OutputDateFld
      (char const* a_tag, Ecn::Basis::Date  const&   a_date, char* a_curr);

    static char* OutputDateTimeFld
      (char const* a_tag, Ecn::Basis::DateTime const& a_dt,  char* a_curr);

    // Sending OrdStatus:
    static char* OutputOrdStatusFld
      (Ecn::FixServer::LiquidityOrderStatus const& a_status, char* a_curr);
  };
} // End namespace AlfaRobot
