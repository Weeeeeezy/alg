// vim:ts=2:et
//===========================================================================//
//                         "Connectors/TCP_Connector.hpp":                   //
//                Common Base of TCP-Based Connectors (Clients)              //
//===========================================================================//
#pragma once

#include "Basis/EPollReactor.h"
#include "Basis/IOUtils.h"
#include "Connectors/TCP_Connector.h"
#include <utxx/compiler_hints.hpp>
#include <utxx/error.hpp>
#include <cassert>

//===========================================================================//
// Macros:                                                                   //
//===========================================================================//
// Short-cuts for accessinhg the Flds and Methods of the "Derived" class:
// Non-Const:
#ifdef  DER
#undef  DER
#endif
#define DER  static_cast<Derived*>(this)->Derived

// Const:
#ifdef  CDER
#undef  CDER
#endif
#define CDER static_cast<Derived const*>(this)->Derived

namespace MAQUETTE
{
  //=========================================================================//
  // Non-Default Ctor:                                                       //
  //=========================================================================//
  template<typename Derived>
  inline TCP_Connector<Derived>::TCP_Connector
  (
    // Connection and IO Params:
    std::string const&    a_name,                 // Internal name
    std::string const&    a_server_ip,            // Where to connect to
    int                   a_server_port,          //
    int                   a_buff_sz,              // IO Buff Sz
    int                   a_buff_lwm,             // IO Buff LWM
    // Client Socket Binding/Marking:
    std::string const&    a_client_ip,
    // Timing Params:
    int                   a_max_conn_attempts,    // >  0
    int                   a_on_timeout_sec,       // >  0
    int                   a_off_timeout_sec,      // >  0
    int                   a_reconn_sec,           // >  0
    int                   a_heart_beat_sec,       // >= 0 (0: no HeartBeats)
    // TLS Support:
    bool                  a_use_tls,              // TLS enabled?
    std::string const&    a_server_name,          // Required for TLS only
    IO::TLS_ALPN          a_tls_alpn,             // Underlying Appl Protocol
    bool                  a_verify_server_cert,
    std::string const&    a_server_ca_files,      // Empty OK
    std::string const&    a_client_cert_file,     // Required for TLS only
    std::string const&    a_client_priv_key_file, // ditto
    // Protocol Logging:
    std::string const&    a_proto_log_file        // May be empty
  )
  : // Immutable Flds:
    m_serverIP            (MkSymKey(a_server_ip)),
    m_serverPort          (a_server_port),
    m_buffSz              (a_buff_sz),
    m_buffLWM             (a_buff_lwm),
    m_clientIP            (MkSymKey(a_client_ip)),
    m_maxConnAttempts     (a_max_conn_attempts),
    m_onTimeOutSec        (a_on_timeout_sec),
    m_offTimeOutSec       (a_off_timeout_sec),
    m_reConnSec           (a_reconn_sec),
    m_heartBeatSec        (a_heart_beat_sec),
    // Mutable Flds:
    m_fd                  (-1),
    m_timerFD             (-1),
    m_status              (StatusT::Inactive),
    m_failedConns         (0),
    m_fullStop            (false),
    // Stats (also mutable of course):
    m_bytesTx             (0),
    m_bytesRx             (0),
    m_lastTxTS            (),
    m_lastRxTS            (),
    // TLS Support:
    m_useTLS              (a_use_tls),
    m_serverName          (a_server_name),        // NOT checked against IP!
    m_tlsALPN             (a_tls_alpn),           // UNDEFINED if no TLS
    m_verifyServerCert    (a_verify_server_cert),
    m_serverCAFiles       (a_server_ca_files),
    m_clientCertFile      (a_client_cert_file),
    m_clientPrivKeyFile   (a_client_priv_key_file),
    // Raw Protocol Logging:
    m_protoLoggerShP      (),                     // Initialized below
    m_protoLogger         (nullptr)
  {
    //-----------------------------------------------------------------------//
    // Check the params. NB: ServerName is compulsory if TLS is used:        //
    //-----------------------------------------------------------------------//
    CHECK_ONLY
    (
      if (utxx::unlikely
         (a_name.empty()         || IsEmpty(m_serverIP)    ||
          m_serverPort < 0       || m_serverPort > 65535   ||
          m_buffLWM <= 0         || m_buffSz <= m_buffLWM  ||
          m_maxConnAttempts <= 0 || m_onTimeOutSec    <= 0 ||
          m_offTimeOutSec   <= 0 || m_reConnSec       <= 0 ||
          m_heartBeatSec    <  0 || (m_useTLS && m_serverName.empty()) ))
        throw utxx::badarg_error("TCP_Connector::Ctor: Invalid param(s)");
    )
    //-----------------------------------------------------------------------//
    // Optionally, create the Logger for Raw TCP Msgs:                       //
    //-----------------------------------------------------------------------//
    if (!a_proto_log_file.empty())
    {
      m_protoLoggerShP = IO::MkLogger(a_name + "-Raw", a_proto_log_file);
      m_protoLogger    = m_protoLoggerShP.get();
      assert(m_protoLogger != nullptr);
    }
  }

  //=========================================================================//
  // Some Properties:                                                        //
  //=========================================================================//
  template<typename Derived>
  inline bool TCP_Connector<Derived>::IsActive()      const
  {
    return m_status == StatusT::Active   && CDER::m_reactor->IsLive(m_fd);
  }

  template<typename Derived>
  inline bool TCP_Connector<Derived>::IsInactive()    const
  {
    return m_status == StatusT::Inactive || !(CDER::m_reactor->IsLive(m_fd));
  }

  template<typename Derived>
  inline bool TCP_Connector<Derived>::IsSessionInit() const
  {
    return m_status == StatusT::SessionInit && CDER::m_reactor->IsLive(m_fd);
  }

  template<typename Derived>
  inline bool TCP_Connector<Derived>::IsLoggingOn()   const
  {
    return  m_status == StatusT::LoggingOn  && CDER::m_reactor->IsLive(m_fd);
  }

  //=========================================================================//
  // "Start":                                                                //
  //=========================================================================//
  template<typename  Derived>
  inline void TCP_Connector<Derived>::Start()
  {
    //-----------------------------------------------------------------------//
    // Check the curr Status:                                                //
    //-----------------------------------------------------------------------//
    // NB: "Start" can only be performed from "Inactive" status:
    if (utxx::unlikely(m_status != StatusT::Inactive))
    {
      LOGA_WARN(CDER, 2, "TCP_Connector::Start: Already Started")
      return;
    }
    // Otherwise: There must be no socket yet:
    assert(m_fd == -1);

    // NB: Normally, there would be no TimerFD either, because it we were given
    // a delayed re-start, the Timer would be cancelled after firing up. But it
    // is possible that the TimerFD is still ticking and the Start() was invoked
    // by some internal Caller, so cancel the Timer if it exists:
    //
    if (utxx::unlikely(m_timerFD >= 0))
      CloseTimerFD("Start");

    // We must NOT be in the "FullStop" mode of course:
    if (utxx::unlikely(m_fullStop))
    {
      LOGA_ERROR(CDER, 2,
        "TCP_Connector::Start: Refusing to Start after FullStop")
      return;
    }

    //-----------------------------------------------------------------------//
    // Create the Handlers:                                                  //
    //-----------------------------------------------------------------------//
    // The Reactor will, in particular, create dynamic buffers to handle Stream
    // IO on this Socket:
    // ReadHandler: Its actual body is provided by the Derived class:
    IO::FDInfo::ReadHandler    readH
    (
      [this](int a_fd, char const* a_buff, int a_size,
             utxx::time_val a_ts_recv) ->  int
      {
        int consumed = DER::ReadHandler(a_fd, a_buff, a_size, a_ts_recv);

        // Update the Rx stats:
        if (utxx::likely(consumed > 0))
        {
          // Per-session stats:
          m_bytesRx += size_t(consumed);
          m_lastRxTS = a_ts_recv;

          // And a separate set of stats which may be maintained by the Derived
          // class (eg in ShM):
          DER::UpdateRxStats(consumed, a_ts_recv);
        }
        return consumed;
      }
    );
    // ConnectHandler:
    IO::FDInfo::ConnectHandler connH
      ( [this](int a_fd)->void { this->ConnectHandler(a_fd); });

    // ErrHandler:
    IO::FDInfo::ErrHandler     errH
    (
      [this](int a_fd,     int  a_err_code, uint32_t  a_events,
             char const* a_msg) -> void
      { this->ErrHandler(a_fd,  a_err_code, a_events, a_msg); }
    );

    //-----------------------------------------------------------------------//
    // Create a TCP Socket and attach it to the Reactor:                     //
    //-----------------------------------------------------------------------//
    m_fd = DER::m_reactor->AddDataStream
    (
      (DER::m_name + "-TCP").data(),
      -1,                   // No given FD (it's a Client!)
      // Handlers:
      readH,
      connH,
      errH,
      // Buffer Params:
      m_buffSz, m_buffLWM,  // ReadBuff
      m_buffSz, m_buffLWM,  // WriteBuff
      // Binding/Marking params:
      m_clientIP.data(),
      // TLS Params:
      m_useTLS,
      m_useTLS ? m_serverName.data()        : nullptr,
      m_useTLS ? m_tlsALPN                  : IO::TLS_ALPN::UNDEFINED,
      m_useTLS ? m_verifyServerCert         : false,
      m_useTLS ? m_serverCAFiles.data()     : nullptr,  // Empty if no TLS
      m_useTLS ? m_clientCertFile.data()    : nullptr,
      m_useTLS ? m_clientPrivKeyFile.data() : nullptr
    );
    assert(m_fd >= 0);
    //-----------------------------------------------------------------------//
    // Issue a Non-Blocking Connect request:                                 //
    //-----------------------------------------------------------------------//
    // The status now is "Connecting".  NB: Set it BEFORE the "Connect" call,
    // because the latter one can in theory return synchnonously (though EXT-
    // REMELY unlikely):
    LOGA_INFO(CDER, 2,
      "TCP_Connector::Start: FD={}: Connecting to IP={}, Port={}",
      m_fd, m_serverIP.data(), m_serverPort)

    // Status change: Inactive -> Connecting:
    assert(m_status == StatusT::Inactive);
    m_status = StatusT::Connecting;

    // Set up a Time-Out for the over-all Connect / LogOn process:
    SetUpFiringTimer(m_onTimeOutSec, 0, "Start--Connect--LogOn");

    // Now really initaite the connection. Any exceptions here propagate sync-
    // ronously:
    DER::m_reactor->Connect(m_fd, m_serverIP.data(), m_serverPort);

    // Start successfully initiated:
    assert(m_fd >= 0);
  }

  //=========================================================================//
  // "Stop":                                                                 //
  //=========================================================================//
  template<typename Derived>
  template<bool     FullStop>
  inline typename TCP_Connector<Derived>::StopResT
    TCP_Connector<Derived>::Stop()
  {
    utxx::time_val now = utxx::now_utc();
    // Save the flag:
    m_fullStop |= FullStop;

    // If we are already Inactive, or GracefulStop has already been Inited, then
    // probably no point in doing anything:
    if (utxx::unlikely
       (m_status == StatusT::Inactive || m_status >= StatusT::Stopping))
    {
      LOGA_INFO(CDER, 2, "TCP_Connector::Stop: Already Stopped / Stopping")
      return StopResT::Ignored;
    }

    // In any other case, if we are not in the Active state (eg Connecting), or
    // the TCP connect has failed, then perform a non-graceful (immediate) stop:
    //
    if (utxx::unlikely(m_status != StatusT::Active || m_fd == -1))
    {
      // NB: Invoke Derived::StopNow, not just this->DisConnect:
      DER::template StopNow<FullStop>("TCP_Connector::Stop", now);
      return StopResT::StoppedImmediately;
    }

    // Otherwise, perform a Graceful Stop. Its implementation is delegated to
    // the Derived class:
    // Status change: Active -> Stopping:
    assert(m_status == StatusT::Active);
    m_status = StatusT::Stopping;

    // Set up an over-all Time-Out for Stopping and LoggingOut:
    SetUpFiringTimer(m_offTimeOutSec, 0, "GracefulStop--LogOut");

    LOGA_INFO(CDER, 1,
      "TCP_Connector::Stop: Graceful LogOut/Stop Initiated...")
    DER::InitGracefulStop();

    // Stop has been successfully initiated:
    return StopResT::GracefulStopInited;
  }

  //=========================================================================//
  // "DisConnect":                                                           //
  //=========================================================================//
  // This is an immediate (NON-GRACEFUL) TCP/TLS DisConnect, unlike the extern-
  // ally-visible graceful "Stop" which tries to perform a proper LogOut first.
  // Again, it could be a FullStop or a restartable one:
  // Typical invocation chain would in general be:
  // Derived::Stop -> TCPC::Stop -> Derived::StopNow -> TCPC::DisConnect:
  //
  template<typename Derived>
  template<bool FullStop>
  inline void TCP_Connector<Derived>::DisConnect(char const* a_where)
  {
    assert(a_where != nullptr);

    // Save the flag:
    m_fullStop |= FullStop;

    // NB: Unlike "Start", "DisConnect" can be performed from any status; it is
    // just a no-op if the status is already "Inactive":
    if (utxx::unlikely(m_status == StatusT::Inactive))
    {
      LOGA_WARN(CDER, 2,
        "TCP_Connector::DisConnect({}): Already Stopped", a_where)
      return;
    }
    LOGA_INFO(CDER, 2,
      "TCP_Connector::DisConnect({}): Stopping Immediately!..", a_where)

    // Detach and close all sockets (if open):
    CloseMainFD ("DisConnect");      // Also closes the TLS Session  (if any)
    CloseTimerFD("DisConnect");
    assert(m_fd == -1 && m_timerFD == -1);

    // If we were in the "Connecting", "SessionInit" or "LogginOn" mode,  then
    // DisConnect() means that we could not connect successfully, so count the
    // number of sequent ial unsuccessful connect attempts:
    if (StatusT::Connecting <= m_status && m_status <= StatusT::LoggingOn)
    {
      ++m_failedConns;
      if (m_failedConns >= m_maxConnAttempts)
        // Abandon re-commection attempts:
        m_fullStop = true;
      LOGA_INFO(CDER, 1,
        "TCP_Connector::DisConnect({}): Connection Attempts={}",
        a_where, m_failedConns)
    }

    // If we got here: Status change: !Inactive -> Inactive:
    assert(m_status != StatusT::Inactive);
    m_status = StatusT::Inactive;

    // Set up a Timer to attempt re-connection:
    if (!m_fullStop)
    {
      SetUpFiringTimer(m_reConnSec, 0, "Wait-Then-ReStart");

      LOGA_INFO(CDER, 1,
        "TCP_Connector::DisConnect({}): Status={}, will try re-connection in "
        "{} sec", a_where, m_status.to_string(), m_reConnSec)
    }
    else
      LOGA_INFO(CDER, 1,
        "TCP_Connector::DisConnect({}): STOPPED.",  a_where)

    // NB: No Call-Backs into "Derived" here -- once again, this method is by
    // itself typically invoked as the last one in a sequence:
    // Derived::Stop -> TCPC::Stop -> Derived::StopNow -> TCPC::DisConnect
  }

  //=========================================================================//
  // "ConnectHandler":                                                       //
  //=========================================================================//
  template<typename  Derived>
  inline void TCP_Connector<Derived>::ConnectHandler(int a_fd)
  {
    // NB: This method is invoked if:
    // (1) TCP Connect                   has  succeeded (no   TLS);
    // (2) TCP Connect and TLS HandShake have succeeded (with TLS);
    // In those cases, same over-all Timer applies to:
    // (1) Connect +              + SessionInit + LogOn
    // (2) Connect + TLSHandShake + SessionInit + LogOn:
    //
    bool fdOK  = (a_fd >= 0 && a_fd == m_fd);

    if  (fdOK && m_status == StatusT::Connecting)
    {
      // TCP Connect appears to be done. But is this socket really connected,
      // and if so, to whom?
      IO::IUAddr peer;
      int        peerLen  = 0;

      CHECK_ONLY(bool isConnected =)
        DER::m_reactor->IsConnected(m_fd, &peer, &peerLen);

      CHECK_ONLY
      (
        if (utxx::unlikely(!isConnected))
          throw utxx::runtime_error
                ("TCP_Connector::ConnectHandler: FD=", m_fd, ": Not Connected");
      )
      // Generic Case: Connect Succeeded!   XXX: Currently, all connections are
      // over Internet -- no UNIX Domain Sockets, so record the Server IP. What
      // we do next, depends on whether the socket is TLS-enabled:
      //
      LOGA_INFO(CDER, 2,
        "TCP_Connector::ConnectHandler: FD={}: Successfully Connected to IP="
        "{}, Port={}{}",
        m_fd, inet_ntoa(peer.m_inet.sin_addr), ntohs(peer.m_inet.sin_port),
        (m_useTLS ? "; TLSHandShake Succeeded" : ""))

     // The "Derived" will now proceed with SessionInit. For some protocols,
     // it is essential (eg HTTP/WS), for others it may not be required (eg
     // FIX, TWIME, ITCH etc), in which case the "Derived" will signal comp-
     // letion of SessionInit immediately:
     //
     m_status = StatusT::SessionInit;
     DER::InitSession();
    }
    else
      // In any other cases, this method MUST NOT be invoked:
      throw utxx::runtime_error
            ("TCP_Connector::ConnectHandler: Invalid or Extraneous FD=", a_fd,
             ", StoredFD=", m_fd, "; Status=", m_status.to_string());
  }

  //=========================================================================//
  // "ErrHandler":                                                           //
  //=========================================================================//
  template<typename  Derived>
  inline void TCP_Connector<Derived>::ErrHandler
  (
    int         a_fd,
    int         a_err_code,
    uint32_t    a_events,
    char const* a_msg
  )
  {
    assert(a_msg != nullptr && (a_fd == m_timerFD || a_fd == m_fd));
    utxx::time_val now = utxx::now_utc();

    // Log the error:
    LOGA_ERROR(CDER, 2,
      "TCP_Connector::ErrHandler: Status={}: IO Error: FD={}, MainFD={}, "
      "TimerFD={}, ErrNo={}, Events={}: {}",
      m_status.to_string(), a_fd, m_fd, m_timerFD, a_err_code,
      CDER::m_reactor->EPollEventsToStr(a_events), a_msg)

    if (utxx::unlikely(a_fd == m_timerFD))
      // If this IO error is related to the TimerFD, then something is fundam-
      // entally wrong -- fatal error, invoke Derived::StopNow with FullStop =
      // true (not just this->DisConnect):
      //
      DER::template StopNow<true>("ErrHandler: TimerFD",  now);
    else
    if (utxx::likely(a_fd == m_fd))
      // Main Socket error. Stop the SessMgr immediately -- but may attempt to
      // re-connect later (so it's NOT a FullStop):
      // XXX: If an error occurred during "Send", no attept is made to determ-
      // ine how many bytes were actually sent (we probably cannot reliably do
      // it anyway, because there is no access to Kernel Buffers)...
      //
      DER::template StopNow<false>("ErrHandler: TCP Socket",  now);
    else
      // Unrelated socket -- this is a serious error cond, stop immediately:
      DER::template StopNow<true> ("ErrHandler: Spurious FD?", now);
  }

  //=========================================================================//
  // "SetUpFiringTimer":                                                     //
  //=========================================================================//
  template<typename  Derived>
  inline void TCP_Connector<Derived>::SetUpFiringTimer
  (
    int         a_init_sec,
    int         a_period_sec,
    char const* a_name
  )
  {
    assert(a_name != nullptr);
    CHECK_ONLY
    (
      if (utxx::unlikely(a_init_sec < 0 || a_period_sec < 0))
        throw utxx::badarg_error
              ("TCP_Connector::SetUpFiringTimer(",  a_name,
               "): Negative timing(s)");
    )
    // In each Status, we only need at most 1 timer to be ticking, so if a
    // previous timer exists, cancel it first:
    CloseTimerFD("SetUpFiringTimer");
    assert(m_timerFD == -1);

    // TimerHandler:
    IO::FDInfo::TimerHandler timerH
      ([this](int a_fd)->void   { this->TimerHandler (a_fd); });

    // ErrHandler:
    IO::FDInfo::ErrHandler errH
    (
      [this](int a_fd,   int   a_err_code, uint32_t  a_events,
             char const* a_msg) -> void
      { this->ErrHandler(a_fd, a_err_code, a_events, a_msg); }
    );

    // Create a new Timer as requested, and add it to the Reactor:
    m_timerFD = DER::m_reactor->AddTimer
    (
      (DER::m_name + "-Timer").data(),
      unsigned(a_init_sec   * 1000),  // Initial interval in msec
      unsigned(a_period_sec * 1000),  // Period in msec
      timerH,
      errH
    );
    assert(m_timerFD >= 0);

    LOGA_INFO(CDER, 2,
      "TCP_Connector::SetUpFiringTimer({}): State={}: Timer Set: Init={} "
      "sec, Period={} sec, FD={}",
      a_name, m_status.to_string(), a_init_sec, a_period_sec, m_timerFD)
  }

  //=========================================================================//
  // "TimerHandler":                                                         //
  //=========================================================================//
  // This Handler may be invoked in the following situations:
  // (*) Status==Inactive:
  //     Previous connection attempt failed,  it's time to try re-connect now;
  // (*) Status==Connecting|LoggingOn:
  //     Connect / LogOn process did not finish within the specified time (we
  //     either did not get ConnectHandler invocation, or did not get a "Log-
  //     On" response from the Server); will need to disconnect and try again;
  // (*) Status==Active:
  //     It's time to send a periodic HeartBeat;
  // (*) Status==Stopping:
  //     Invoke a Call-Back for the "intermediate" stopping stage, and set up
  //     another timer for the "LoggingOut" state:
  // (*) Status==LoggingOut:
  //     If we did not receive a LogOut confirm  before this timer event,  we
  //     will forcibly do Derived::StopNow (not just this->DisConnect):
  //
  template<typename  Derived>
  inline void TCP_Connector<Derived>::TimerHandler(int a_fd)
  {
    assert(a_fd == m_timerFD && m_timerFD >= 0);

    utxx::time_val now = utxx::now_utc();

    // First of all:   Unless the status is "Active" (periodic heart-beat  is
    // due), the timer event is a one-off; for extra safety we will close the
    // TimerFD explicitly:
    if (utxx::unlikely(m_status != StatusT::Active))
      CloseTimerFD("TimerHandler");

    switch (m_status)
    {
    //-----------------------------------------------------------------------//
    case StatusT::Connecting:
    case StatusT::SessionInit:
    case StatusT::LoggingOn:
    //-----------------------------------------------------------------------//
    {
      // Was not able to complete all Connects and/or LogOn within the time
      // required -- stop but possibly re-try (!FullStop):
      char const* msg =
        (m_status == StatusT::Connecting)
        ? (m_useTLS     ? "Connect" : "Connect+TLSHandShake") :
        (m_status == StatusT::SessionInit)
        ? "SessionInit" : "LogOn";

      LOGA_ERROR(CDER, 1,
        "TCP_Connector::TimerHandler: FD={}, {} TimeOut", a_fd, msg)

      // Invoke Derived::StopNow:
      DER::template StopNow<false>(msg, now);
      break;
    }

    //-----------------------------------------------------------------------//
    case StatusT::Active:
    //-----------------------------------------------------------------------//
      // Don't do anything if, by any chance, there are no HeartBeats:
      if (m_heartBeatSec > 0)
      {
        if (utxx::unlikely
           (!m_lastRxTS.empty()          &&
            (now - m_lastRxTS).seconds() >= 2 * m_heartBeatSec))
          // If the Server Inactivity Time-Out since the last Check-Point has
          // elapsed, signal an error. XXX: For simplicity, we do NOT use any
          // TestReq invocations or so (they may  or may not  be supported by
          // the Derived class) -- simply use 2 * HeartBeatPeriod as above:
          //
          DER::ServerInactivityTimeOut(now);
        else
          // Otherwise, simply send a HeartBeat:
          DER::SendHeartBeat();
      }
      break;

    //-----------------------------------------------------------------------//
    case StatusT::Stopping:
    //-----------------------------------------------------------------------//
      // NB: This is normal. The "Stopping" phase is time-driven because
      // times
      // The state is now "LoggingOut":
      m_status = StatusT::LoggingOut;

      // Just invoke a Call-Back:
      DER::GracefulStopInProgress();

      // Set up a time-out for the final "LogOut":
      SetUpFiringTimer(m_offTimeOutSec, 0, "GracefulStop-Stopping");
      break;

    //-----------------------------------------------------------------------//
    case StatusT::LoggingOut:
    //-----------------------------------------------------------------------//
      LOGA_WARN(CDER, 1,
        "TCP_Connector::TimeHandler: Graceful LogOut timed-out (FD={}), force "
        "it", a_fd)

      // Invoke Derived::StopNow -- with or w/o a Restart:
      if (m_fullStop)
        DER::template StopNow<true> ("TimerHandler--LoggingOut", now);
      else
        DER::template StopNow<false>("TimerHandler--LoggingOut", now);
      break;

    //-----------------------------------------------------------------------//
    case StatusT::Inactive:
    //-----------------------------------------------------------------------//
      // Try connecting again (but double-check that FullStop is not set). XXX:
      // is this->Start sufficient as opposed to Derived::Start? Hopefully, as
      // we do not need to notify the Strategies about anything at this point,
      // only when we become Active again:
      //
      LOGA_WARN(CDER, 1,
        "TCP_Connector::TimeHandler: InActive, {} on FD={}",
        (m_fullStop?"FullStop":"Restarting"), a_fd)

      if (utxx::likely(!m_fullStop))
        // BEWARE: Not DER::Start()!
        this->Start();
      break;

    default: ;
    }
  }

  //=========================================================================//
  // "CloseMainFD":                                                          //
  //=========================================================================//
  template<typename  Derived>
  inline void TCP_Connector<Derived>::CloseMainFD(char const* a_where) const
  {
    assert(a_where != nullptr);

    // Detach and close "m_fd". If it is TLS-enabled, the TLS Session will be
    // destroyed as well:
    if (m_fd >= 0)
    {
      CDER::m_reactor->Remove(m_fd);

      LOGA_INFO(CDER, 2,
        "TCP_Connector::CloseMainFD({}): State={}, OldFD={}: {}TCP Socket "
        "Closed", a_where,  m_status.to_string(),  m_fd,
        (m_useTLS ? "TLS Session Closed, " : ""))
      m_fd = -1;
    }
  }

  //=========================================================================//
  // "CloseTimerFD":                                                         //
  //=========================================================================//
  template<typename  Derived>
  inline void TCP_Connector<Derived>::CloseTimerFD(char const* a_where) const
  {
    assert(a_where != nullptr);

    // Detach and close "m_timerFD":
    if (utxx::likely(m_timerFD >= 0))
    {
      CDER::m_reactor->Remove(m_timerFD);

      LOGA_INFO(CDER, 2,
        "TCP_Connector::CloseTimerFD({}): State={}, OldTimerFD={}: Timer "
        "Removed", a_where,  m_status.to_string(),  m_timerFD)
      m_timerFD = -1;
    }
  }

  //=========================================================================//
  // "SessionInitCompleted":                                                 //
  //=========================================================================//
  template<typename  Derived>
  inline void TCP_Connector<Derived>::SessionInitCompleted()
  {
    assert(m_status == StatusT::SessionInit);

    // Can now proceed to LogOn (under the same over-all Timer):
    LOGA_INFO(CDER, 2,
      "TCP_Connector::SessionInitCompleted: Now LoggingOn...")

    m_status = StatusT::LoggingOn;
    DER::InitLogOn();
  }

  //=========================================================================//
  // "LogOnCompleted":                                                       //
  //=========================================================================//
  // Logging-On stage is now complete (and thus Connection and SessionInit are
  // done as well):
  //
  template<typename  Derived>
  inline void TCP_Connector<Derived>::LogOnCompleted()
  {
    // If we are already Active, there is nothing to do (so this method can be
    // safely invoked multiple times):
    if (utxx::unlikely(m_status == StatusT::Active))
      return;

    // Otherwise, it must be Status change: LoggingOn->Active:
    assert(m_status == StatusT::LoggingOn);
    m_status = StatusT::Active;

    // Clear the "unsuccessful connects" counter:
    m_failedConns = 0;

    // Set up a periodic Timer to send "HeartBeat"s in the future (though we
    // are not fully logged on yet):
    if (m_heartBeatSec > 0)
      SetUpFiringTimer
        (m_heartBeatSec, m_heartBeatSec, "LogOnCompleted--SetHeartBeats");

    LOGA_INFO(CDER, 2,
      "TCP_Connector::LogOnCompleted: Connector is now ACTIVE")
  }

  //=========================================================================//
  // "SendImpl":                                                             //
  //=========================================================================//
  template<typename  Derived>
  inline  utxx::time_val TCP_Connector<Derived>::SendImpl
    (char const* a_buff, int a_len)
  const
  {
    // Just run the Reactor Send. Any error conds are handled by "ErrHandler",
    // after which, exceptions may propafate to the Caller:
    assert(a_buff != nullptr && a_len > 0);

    // XXX: This check should probably be done much earlier on: If there is no
    // socket at all, then cannot send! (But on the other hand, the  Connector
    // does not need to be fully Active --  otherwise we would not be  able to
    // initiate a LogOn!):
    // XXX: However, if the Status is Inactive,   it is normal that we  cannot
    // send some left-over data, so only a warning   is produced in that case,
    // not an exception:
    CHECK_ONLY
    (
      if (utxx::unlikely(m_fd < 0))
      {
        if (m_status == StatusT::Inactive)
        {
          LOGA_WARN(CDER,  2,
            "TCP_Connector::Send: Cannot Send {} bytes: Session InActive",
            a_len)
          return utxx::time_val();
        }
        else
          LOGA_ERROR(CDER, 2,
            "TCP_Connector::Send: Cannot Send {} bytes: No Socket; Status=",
            a_len, m_status.to_string())
      }
    )
    // If OK: Try actual sending via the Reactor. Any exceptions here are pro-
    // pagated synchronously:
    assert(m_fd >= 0);
    utxx::time_val sendTS = CDER::m_reactor->Send(a_buff, a_len, m_fd);

    if (utxx::likely(!sendTS.empty()))
      m_lastTxTS = sendTS;

    // In any case, update the stats (both per-Session and Derived-class stats;
    // the latter are separate, and may eg be stored in ShM):
    m_bytesTx += size_t(a_len);
    CDER::UpdateTxStats(a_len, sendTS);

    return sendTS; // XXX: Even if empty...
  }
} // End namespace MAQUETTE
