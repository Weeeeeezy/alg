// vim:ts=2:et
//===========================================================================//
//                         "Connectors/TCP_Connector.h":                     //
//                Common Base of TCP-Based Connectors (Clients)              //
//===========================================================================//
#pragma once

#include "Basis/BaseTypes.hpp"
#include "Basis/IOUtils.h"
#include <utxx/compiler_hints.hpp>
#include <utxx/enum.hpp>
#include <utxx/time_val.hpp>
#include <spdlog/spdlog.h>
#include <boost/core/noncopyable.hpp>

namespace MAQUETTE
{
  class EPollReactor;

  //=========================================================================//
  // "TCP_Connector" Class:                                                  //
  //=========================================================================//
  // NB: "Derived" class implements the following functionality:
  // (*) ReadHandler (because it depends on the actual MsgProtocol being used);
  // (*) Some hooks  (call-backs) required by "TCP_Connector";
  // (*) State / Session Mgmt;
  // (*) Application-Level Processing (invoked from ReadHandler):
  // (*) EPollReactor, Logger, ...
  // (*) In case of multi-level inheritance, "Derived" should nprmally be the
  //     BOTTOM ("final") class in the hierarchy.
  //
  // More precisely, the following API is assumed from "Derived":
  //     m_name
  //     m_debugLevel
  //     m_logger
  //     m_reactor
  //     InitSession()               Eg start HTTP/WS exchange
  //     InitLogOn()                 Eg submit client authentication tokens
  //     ReadHandler()               Protocol-Specific
  //     UpdateRxStats()
  //     ServerInactivityTimeOut()   If HeartBeat was not recvd from Server
  //     SendHeartBeat()
  //     InitGracefulStop()          Eg CancelOrders, SyncWithServer
  //     GracefulStopInProgress()    Eg UnSubscribe,  SendLogOff
  //     StopNow<bool FullStop>()    Call this->DisConnect(), NotifySubscribers
  //
  // And "Derived" should call the following methods of this class at appropri-
  // ate times:
  //     SendImpl()                  When any data are to be sent to the Server
  //     SessionInitCompleted()      When SessionInit          has been done
  //     LogOnCompleted()            When LogOn/authentication has been done
  //     Stop()                      When graceful  stop needs to be initiated
  //     DisConnect()                When immediate stop is required
  //==========================================================================//
  template<typename Derived>
  class TCP_Connector: public boost::noncopyable
  {
  public:
    //=======================================================================//
    // TCP Connector Status Type:                                            //
    //=======================================================================//
    UTXX_ENUM(
    StatusT, char,
      Inactive,        // Obvious
      Connecting,      // Performing TCP Connect (w/ TLS HandShake if reqd)
      SessionInit,     // Eg if the Prococol is over HTTP/WS
      LoggingOn,       // LogOn has been sent
      Active,          // Steady-mode operation
      Stopping,        // GracefulStop initiated: eg cancelling active orders
      LoggingOut       // LogOut was sent to terminate the session
    );

    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    // Connection and IO Params. NB: "const" flds are made "public":
    SymKey  const           m_serverIP;       // 15 chars are just OK
    int     const           m_serverPort;
    int     const           m_buffSz;         // IOBuff Size
    int     const           m_buffLWM;        // IOBuff LWM (Rd)
    // Client Socket Binding/Marking:
    SymKey  const           m_clientIP;
    // Timing Params (except HeartBeats -- they are in "TCP_SessBase"):
    int     const           m_maxConnAttempts;
    int     const           m_onTimeOutSec;
    int     const           m_offTimeOutSec;
    int     const           m_reConnSec;
    int     const           m_heartBeatSec;   // Inactivity Monitoring Interval

  protected:
    // Mutable Data:
    mutable int             m_fd;             // Main TCP socket of this Session
    mutable int             m_timerFD;        // For periodic actions
    mutable StatusT         m_status;
    mutable int             m_failedConns;
    mutable bool            m_fullStop;
    // Stats:
    mutable size_t          m_bytesTx;        // Bytes sent  in this Session
    mutable size_t          m_bytesRx;        // Bytes recvd in this Session
    mutable utxx::time_val  m_lastTxTS;       // When last msg  was  sent
    mutable utxx::time_val  m_lastRxTS;       // When last msg  was recvd

  public:
    // TLS Support:
    bool         const      m_useTLS;
    std::string  const      m_serverName;     // Flds below are for TLS only...
    IO::TLS_ALPN const      m_tlsALPN;
    bool         const      m_verifyServerCert;
    std::string  const      m_serverCAFiles;  // Comma-separated list, or ""
    std::string  const      m_clientCertFile;
    std::string  const      m_clientPrivKeyFile;

  protected:
    // Logger for raw protocol data:
    std::shared_ptr<spdlog::logger>
                            m_protoLoggerShP;
    spdlog::logger*         m_protoLogger;    // Direct raw ptr (alias)

    // Default Ctor is deleted:
    TCP_Connector() = delete;

    //=======================================================================//
    // Non-Default Ctor, Dtor:                                               //
    //=======================================================================//
    TCP_Connector
    (
      std::string const&    a_name,                 // Internal name, non-""
      // Connection and IO Params:
      std::string const&    a_server_ip,            // Where to connect to
      int                   a_server_port,          //
      int                   a_buff_sz,              // IO Buff Sz
      int                   a_buff_lwm,             // IO Buff LWM
      // Client Socket Binding/Marking:
      std::string const&    a_client_ip,
      // Timing Params:
      int                   a_max_conn_attempts,    // > 0
      int                   a_on_timeout_sec,       // > 0
      int                   a_off_timeout_sec,      // > 0
      int                   a_reconn_sec,           // > 0
      int                   a_heart_beat_sec,       // >= 0
      // TLS Support:
      bool                  a_use_tls,
      std::string const&    a_server_name,          // Empty OK   if no TLS
      IO::TLS_ALPN          a_tls_alpn,             // UNDEFINED  if no TLS
      bool                  a_verify_server_cert,   // Irrelevant if no TLS
      std::string const&    a_server_ca_files,      // Empty OK
      std::string const&    a_client_cert_file,     // Empty OK   if no TLS
      std::string const&    a_client_priv_key_file, // Empty OK   if no TLS
      // Protocol Logging:
      std::string const&    a_proto_log_file        // May be empty
    );

    // The Dtor is currently trivial:
    ~TCP_Connector() noexcept {}

  public:
    //=======================================================================//
    // "Start", "Stop*":                                                     //
    //=======================================================================//
    // XXX: These methods are made "public" so that they can be invoked direct-
    // ly on the "Derived" objs:
    // "Start":
    void Start();

    // "Stop" can result in the following 3 conditions:
    enum class StopResT
    {
      Ignored,            // Because already stopping
      StoppedImmediately,
      GracefulStopInited
    };

    // "Stop":
    // NB: If "FullStop" is set, there is no re-start after "Stop"; otherwise,
    // we try to re-start after a certain period of time:
    //
    template<bool FullStop>
    StopResT Stop();

    // "DisConnect" is an immediate (non-graceful) TCP/TLS DisConnect, unlike
    // the externally-visible graceful "Stop" which tries to perform a proper
    // LogOut first. Again, it could be a FullStop or a restartable one:
    //
    template<bool FullStop>
    void DisConnect(char const* a_where);

    //=======================================================================//
    // Some Properties:                                                      //
    //=======================================================================//
    bool IsActive()      const;
    bool IsInactive()    const;
    bool IsSessionInit() const;
    bool IsLoggingOn()   const;

  private:
    //=======================================================================//
    // Handlers for EPollReactor (except ReadHandler):                       //
    //=======================================================================//
    // NB: ReadHandler is highly Protocol-specific, so it is provided by the
    // "Derived" class:
    //
    void ConnectHandler(int a_fd);

    void ErrHandler
    (
      int         a_fd,
      int         a_err_code,
      uint32_t    a_events,
      char const* a_msg
    );

    void TimerHandler(int a_fd);

    // "SetUpFiringTimer" (when it fires up, "TimerHandler" is invoked):
    //
    void SetUpFiringTimer
    (
      int         a_init_sec,
      int         a_period_sec,
      char const* a_where
    );

  protected:
    //=======================================================================//
    // Misc Utils:                                                           //
    //=======================================================================//
    // "SessionInitCompleted":
    // Must be invoked by "Derived" when the SessionInit stage is complete (and
    // therefore, Connection has been completed as well):
    //
    void SessionInitCompleted();

    // "LogOnCompleted":
    // Must be invoked by "Derived" when the Logging-On  stage is complete (and
    // therefore, Connection and SessionInit have been completed as well):
    //
    void LogOnCompleted();

    // "SendImpl":
    // Sends the data out via the Reactor:
    //
  public:
    utxx::time_val SendImpl(char const* a_buff, int a_len) const;

  private:
    // Removing the FDs from the Reactor, and closing them:                  //
    //
    void CloseMainFD (char const* a_where)       const;
    void CloseTimerFD(char const* a_where)       const;

    // Save invocation of GNUTLS API functions:
    void CheckTLS    (int rc, char const* where) const;
  };
} // End namespace MAQUETTE
