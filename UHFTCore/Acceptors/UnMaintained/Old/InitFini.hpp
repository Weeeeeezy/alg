// vim:ts=2:et
//===========================================================================//
//                                "InitFini.hpp":                            //
//                   Ctor/Dtor/Start/Stop etc of "EAcceptor_FIX"             //
//===========================================================================//
#pragma once

#include "utxx/compiler_hints.hpp"
#include "utxx/convert.hpp"
#include "Basis/BaseTypes.hpp"
#include "FixEngine/Acceptor/EAcceptor_FIX.h"
#include "FixEngine/Basis/XXHash.hpp"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>
#include <utility>

namespace AlfaRobot
{
  //=========================================================================//
  // "EAcceptor_FIX" Ctor:                                                   //
  //=========================================================================//
  // NB: "a_reactor" may or may not be NULL; in the former case, an internal
  // Reactor is created:
  //
  template<FIX::DialectT::type D, typename Conn, typename Derived>
  inline EAcceptor_FIX<D, Conn, Derived>::EAcceptor_FIX
  (
    Ecn::Basis::Part::TId                       a_id,
    Ecn::FixServer::FixAcceptorSettings const&  a_config,
    Ecn::Basis::SPtrPack<Ecn::Model::Login>     a_logins,
    Conn&                                       a_conn,      // Non-Const Ref!
    EPollReactor*                               a_reactor
  )
  : // Main Configs:
    m_name                ("AlfaECN-FIX-" + std::to_string(a_id)),
    m_accptAddr           (),       // Initialised below
    // Limits:
    m_throttlPeriodSec    (a_config.ThrottlPeriodSec),
    m_maxReqsPerPeriod    (a_config.MaxReqsPerPeriod),
    // FIX Params:
    m_resetSeqNumsOnLogOn (a_config.ResetSession),
    m_heartBeatSec        (a_config.HeartbeatInterval),
    m_maxGapMSec          (a_config.MaxGapMSec),
    m_maxLatency          (a_config.MaxLatency),
    // Run-Time Setup:
    m_storeFile           (a_config.StorePath  + "/" + m_name + ".mm"),
    m_storeMMap           (),       // Initialised later
    m_loggerShP           (IO::MkLogger
                            (m_name, a_config.LogPath + "/" + m_name)),
                                    // FIXME: Need other Logger params!
    m_logger              (m_loggerShP.get()),
    m_debugLevel          (a_config.DebugLevel),
    // XXX: Be careful while appending "-Raw" to the log file name -- "stdout"
    // and "stderr" must not be changed:
    m_rawLoggerShP        (IO::MkLogger
                            (m_name + "-Raw",
                            (a_config.LogPath != "stdout"  &&
                             a_config.LogPath != "stderr")
                             ? (a_config.LogPath + "/" + m_name + "-Raw")
                             :  a_config.LogPath)
                          ),
    m_rawLogger           (m_rawLoggerShP.get()),
    // If the Reactor is specified, its ptr is stored in "m_reactor", whereas
    // "m_reactorUnP" remains empty;
    // otherwise, a new Reactor is created and stored in "m_reactorUnP";  the
    // raw ptr comes into "m_reactor". Thus, "m_reactor" is any case NON-owned:
    m_reactorUnP          ((a_reactor == nullptr)
                           ? new EPollReactor(m_logger, m_debugLevel)
                           : nullptr),
    m_reactor             ((a_reactor == nullptr)
                           ? m_reactorUnP.get()
                           : a_reactor),
    m_sessions            (nullptr),  // Initialised later
    m_accptFD             (-1),
    m_timerFD             (-1),
    m_outBuff             { 0 },
    m_currMsg             { 0 },
    m_totBytesTx          (0),
    m_totBytesRx          (0),
    // ECN Integration:
    m_logins              (),         // Empty, then populated from "a_logins"
    m_conn                (&a_conn)
  {
    //-----------------------------------------------------------------------//
    // Verify the Settings:                                                  //
    //-----------------------------------------------------------------------//
    // Reactor and Loggers must be non-NULL by construction:
    assert(m_logger  != nullptr && m_rawLogger != nullptr &&
           m_reactor != nullptr && m_conn      != nullptr);

    // Reqs throttling (per Client):
    // Either both params are 0 (and then throttling is not used), or they must
    // be positive for real throttle settings:
    if (utxx::unlikely
       (m_throttlPeriodSec != 0 || m_maxReqsPerPeriod != 0) &&
       (m_throttlPeriodSec <= 0 || m_maxReqsPerPeriod <= 0))
      throw utxx::badarg_error
            ("EAcceptor_FIX::Ctor: Invalid Throttling param(s): PeriodSec=",
             m_throttlPeriodSec,  ", PeriodLimit=", m_maxReqsPerPeriod);

    // FIX timing params:
    if (utxx::unlikely(m_heartBeatSec <= 0 || m_maxGapMSec <= 0))
      throw utxx::badarg_error
            ("EAcceptor_FIX::Ctor: Invalid HearBeatPeriodSec=", m_heartBeatSec,
             " and/or WaitAfterResendReq=",   m_maxGapMSec);

    // Obviously, the FIX Protocol Dialect must support the Acceptor mode (Ord-
    // Mgmt or, otherwise, MktData):
    // IsOrdMgmt => HasOrdMgmt:
    static_assert
        (!Derived::IsOrdMgmt || FIX::ProtocolFeatures<D>::s_hasOrdMgmt,
         "OrdMgmt mode is not supported by this FIX Dialect");

    // IsMktData => HashMktData:
    static_assert
        (!Derived::IsMktData || FIX::ProtocolFeatures<D>::s_hasMktData,
         "MktData mode is not supported by this FIX Dialect");

    //-----------------------------------------------------------------------//
    // Construct the Listening Address:                                      //
    //-----------------------------------------------------------------------//
    // Check the IP and Port (the latter is checked in Config, to prevent in-
    // correct conversion into "uint16_t"):
    m_accptAddr.sin_family      = AF_INET;
    m_accptAddr.sin_addr.s_addr = inet_addr     (a_config.ListenIP.data());
    m_accptAddr.sin_port        = htons(uint16_t(a_config.ListenPort));

    if (m_accptAddr.sin_addr.s_addr == INADDR_NONE || a_config.ListenPort < 0 ||
        a_config.ListenPort > 65535)
      throw utxx::badarg_error
            ("EAcceptor_FIX::Ctor: Invalid IP=", a_config.ListenIP.data(),
             " and/or Port=",  a_config.ListenPort);

    //-----------------------------------------------------------------------//
    // Open or create the persistent segment:                                //
    //-----------------------------------------------------------------------//
    // NB: This is done irrespective to whether "PersistMessages" are set -- in
    // any case, we need to persist Session data such as Tx, Rx, ...:
    //
    void*  mapAddr  = GetMMapAddr(m_storeFile.data(), a_config.StoreMMapAddr);
    assert(mapAddr != nullptr);

    if (utxx::unlikely(a_config.ResetStore))
    {
      // Check if the StoreMMap object already exists -- if so, try to remove
      // it first. If removal fails, the object is probably mapped by another
      // process  -- it is a critical error:
      bool exists = false;
      try
      {
        BIPC::managed_mapped_file tmp
          (BIPC::open_read_only, m_storeFile.data(), mapAddr);

        exists = true;
        // "tmp" is immediately unmapped on exit from this context
      }
      catch (...){}

      // If it does not exist, do not even try to remove it.   XXX: Can we get
      // false negative for "exists" somehow?  -- We would then fail to remove
      // the segment shich actually needs to be removed!
      if (utxx::unlikely
         (exists && !BIPC::file_mapping::remove(m_storeFile.data())))
        throw utxx::runtime_error
              ("EAcceptor_FIX::Ctor: Cannot remove StoreFile=", m_storeFile,
               ": Held by another process?");
    }
    // Done possible re-setting
    // OK, can now map it "for good":
    try
    {
      m_storeMMap = BIPC::managed_mapped_file
      (
        BIPC::open_or_create, m_storeFile.data(), GetMMapSize(), mapAddr,
        BIPC::permissions(0660)
      );
    }
    catch (std::exception const& exc)
    {
      throw utxx::runtime_error
            ("EAcceptor_FIX::Ctor: Cannot MMap the StoreFile: ", m_storeFile,
             ": ", exc.what());
    }

    //-----------------------------------------------------------------------//
    // Find or allocate "FIXSessions":                                       //
    //-----------------------------------------------------------------------//
    m_sessions =
      m_storeMMap.find_or_construct<FIXSession>("FIXSessions")[MaxSessions]();
    assert(m_sessions != nullptr);

    // IMPORTANT: Invalidate all "SPtr"s in "FIXSession"s -- they are stale.
    // Also clean-up other transient data (but NOT the Session state):
    //
    for (int i = 0; i < int(MaxSessions); ++i)
    {
      FIXSession& sess    = m_sessions[i];
      sess.m_fd           = -1;
      sess.m_heartBeatSec = m_heartBeatSec; // Re-negotiated on LogOn
      sess.m_ecnSession   =
        Ecn::Basis::NullSPtr<Ecn::FixServer::LiquidityFixSession>();

      if (sess.m_clientCompID[0])
        std::cout << "SESSION >>> " << sess.m_clientCompID.data() << " txSN=" << sess.m_txSN << " rxSN=" << sess.m_rxSN << std::endl;
    }

    //-----------------------------------------------------------------------//
    // Populate the "Login"s map:                                            //
    //-----------------------------------------------------------------------//
    for (auto sLogin: *a_logins)
    {
      Ecn::Model::Login const&  login = *sLogin;
      unsigned long ccidH = MkHashCode64(login.FixTargetCompId.c_str()); // << TODO YYY

      auto ins = m_logins.insert(std::make_pair(ccidH, login));
      if (utxx::unlikely(!ins.second))
        // This means a repeated ClientCompID (or, extremely unlikely, a hash
        // clash) -- not allowed:
        throw utxx::badarg_error
              ("EAcceptor_FIX::Ctor: Repeated ClientCompID in Logins? Old=",
               ins.first->second.FixSenderCompId.c_str(), ", New=",
               login.            FixSenderCompId.c_str());

      // TODO YYY
      auto& session = m_sessions[ccidH % MaxSessions];
      StrNCpy<false, SymKeySz>
        (session.m_clientCompID.data(), login.FixTargetCompId.c_str());
      StrNCpy<false, SymKeySz>
        (session.m_serverCompID.data(), login.FixSenderCompId.c_str());
    }
    // Allow no more than 1 entry per bucket, to facilitate look-ups. The hash
    // table may be automatically enlarged as a result of the following:
    m_logins.max_load_factor(1.0);

    std::cout << "CREATED!!! port="  << a_config.ListenPort << std::endl;
  }

  //=========================================================================//
  // "EAcceptor_FIX" Dtor:                                                   //
  //=========================================================================//
  template<FIX::DialectT::type D, typename Conn, typename Derived>
  inline EAcceptor_FIX<D, Conn, Derived>::~EAcceptor_FIX() noexcept
  {
    // Do not allow any exceptions to propagate out of the Dtor:
    try
    {
      // Perform a non-graceful stop:
      StopNow<false>("Dtor");

      // Reset the ptrs (just in case):
      m_reactor   = nullptr;
      m_logger    = nullptr;
      m_rawLogger = nullptr;

      // NB: The MMap segment will be unmapped (but preserved!) automatically
    }
    catch(...){}
  }

  //=========================================================================//
  // "EAcceptor_FIX::Start":                                                 //
  //=========================================================================//
  // Creates AcceptorFD and TimerFD:
  //
  template<FIX::DialectT::type D, typename Conn, typename Derived>
  inline void EAcceptor_FIX<D, Conn, Derived>::Start()
  {
    //-----------------------------------------------------------------------//
    // Check the curr Status:                                                //
    //-----------------------------------------------------------------------//
    // Either both AcceptorFD and TimerFD are valid, or both invalid:
    assert((m_accptFD >= 0 && m_timerFD >= 0) ||
           (m_accptFD  < 0 && m_timerFD  < 0));

    if (utxx::unlikely(m_accptFD >= 0))
    {
      // Nothing else to do:
      if (utxx::unlikely(m_debugLevel >= 2))
        m_logger->warn("EAcceptor_FIX::Start: Already Started");
      return;
    }
    assert(m_reactor != nullptr);

    //-----------------------------------------------------------------------//
    // To ensure consistency of AcceptorFD and TimerFD:                      //
    //-----------------------------------------------------------------------//
    try
    {
      //---------------------------------------------------------------------//
      // Create the Acceptor Socket and attach it to the Reactor:            //
      //---------------------------------------------------------------------//
      // NB: The new socket is non-blocking from the beginning:
      //
      m_accptFD =
        socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);

      if (utxx::unlikely(m_accptFD < 0))
        SystemError(-1, "EAcceptor_FIX::Start: Cannot create Acceptor Socket");

      // TODO YYY
      // Any errors here result in socket being closed, and an exception propa-
      // gated:
      int yes = 1;
      if (utxx::unlikely
         (setsockopt(m_accptFD, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))
          < 0))
        SocketError<true>
          (-1, m_accptFD,
           "EAcceptor_FIX::Start: setsockopt(SO_REUSEADDR) failed");

      if (utxx::unlikely
         (bind(m_accptFD, reinterpret_cast<struct sockaddr *>(&m_accptAddr),
               sizeof(m_accptAddr)) < 0))
        SocketError<true>(-1, m_accptFD, "EAcceptor_FIX::Start: bind failed");

      // Create a Listening Queue on this socket; 1024 simultaneous connection
      // attempts is typically OK:
      if (utxx::unlikely(listen(m_accptFD, 1024) < 0))
        SocketError<true>(-1, m_accptFD, "EAcceptor_FIX::Start: listen failed");

      // AcceptHandler:
      IO::FDInfo::AcceptHandler  accH
      (
        [this](int        a_acceptor_fd, int a_client_fd,
               IO::IUAddr const* a_addr, int a_addr_len) -> bool
        {
          return this->AcceptHandler
                 (a_acceptor_fd, a_client_fd, a_addr, a_addr_len);
        }
      );

      // ErrHandler:
      IO::FDInfo::ErrHandler    errH
      (
        [this](int a_fd, int a_err_code, uint32_t a_events, char const* a_msg)
        -> void
        { this->ErrHandler(a_fd,  a_err_code, a_events, a_msg); }
      );

      // Attach this Socket to the Reactor:
      m_reactor->AddAcceptor(m_name.data(), m_accptFD, accH, errH);

      //---------------------------------------------------------------------//
      // Create a periodic timer for managing HeartBeats:                    //
      //---------------------------------------------------------------------//
      // XXX: If, for any reason, there was a left-over TimerFD for this Session
      // and it was not properly closed, we would get a double HeartBeat rate --
      // but this is not a critical error:
      // TimerHandler:
      IO::FDInfo::TimerHandler timerH
        ([this](int DEBUG_ONLY(a_tfd))->void
        {
          assert(a_tfd == this->m_timerFD);
          this->TimerHandler();
        });

      // The Timer will have a 1-sec period (it is sufficient because all Heart-
      // Beat periods are multiples of 1 sec):
      m_timerFD = m_reactor->AddTimer
      (
        (m_name + "-Timer").data(),
        1000,   // Initial interval: 1 sec
        1000,   // Period:           1 sec
        timerH,
        errH
      );

      // TODO YYY
      //---------------------------------------------------------------------//
      // Report successful start                                             //
      //---------------------------------------------------------------------//
      m_conn->SendConnectorState(
        Ecn::Basis::MakeSPtr<Ecn::FixServer::LiquidityConnectorState>(
          Ecn::Model::TechnicalStateType::Started));

      //---------------------------------------------------------------------//
      // Successfully started:                                               //
      //---------------------------------------------------------------------//
      m_logger->info
        ("EAcceptor_FIX::Start: Acceptor Started: AccptFD=")
        << m_accptFD << ", TimerFD=" << m_timerFD;
    }
    //-----------------------------------------------------------------------//
    // Clean-Up on any Exceptions:                                           //
    //-----------------------------------------------------------------------//
    catch (std::exception const& exc)
    {
      // XXX: The following fragment should not (normally) throw any exceptions
      // by itself:
      m_reactor->Remove(m_accptFD);
      m_reactor->Remove(m_timerFD);
      m_accptFD = -1;
      m_timerFD = -1;

      // Log the error and propagate it further:
      m_logger->critical("EAcceptor_FIX::Start: Exception: ") << exc.what();
      throw;
    }

    std::cout << "STARTED!!!! " << std::endl;
  }

  // TODO YYY added Stop() impl
  template<FIX::DialectT::type D, typename Conn, typename Derived>
  inline void EAcceptor_FIX<D, Conn, Derived>::Stop()
  {
    std::cout << "EAcceptor_FIX STOP()" << std::endl;
    try
    {
      StopNow<true>("Stop");
    }
    catch (EPollReactor::ExitRun&)
    {
    }

    m_reactorUnP.reset();
    m_reactor = nullptr;

    m_conn->SendConnectorState(
        Ecn::Basis::MakeSPtr<Ecn::FixServer::LiquidityConnectorState>(
            Ecn::Model::TechnicalStateType::Stopped));
  }

  //=========================================================================//
  // "EAcceptor_FIX::StopNow":                                               //
  //=========================================================================//
  template<FIX::DialectT::type D, typename Conn, typename Derived>
  template<bool  Graceful>
  inline void EAcceptor_FIX<D, Conn, Derived>::StopNow(char const* a_where)
  const
  {
    // Terminate all Sessions -- gracefully or otherwise. XXX: Unfortunately, we
    // will have to iterate over the whole array -- but for "StopNow", it's OK:
    for (size_t i = 0; i < MaxSessions; ++i)
    {
      FIXSession* sess = m_sessions + i;

      // NB: The following call does NOT propagate any exceptions, and does no-
      // thing if "sess" is not connected (ie is dormant):
      TerminateSession<Graceful>(sess, "StopNow", a_where);
    }

    // Now detach and close the AcceptorFD and the TimerFD:
    m_reactor->Remove(m_accptFD);
    m_reactor->Remove(m_timerFD);
    m_accptFD = -1;
    m_timerFD = -1;

    m_logger->info
      ("EAcceptor_FIX::StopNow(") << a_where << "): Acceptor Stopped";

    // XXX: Furthermore, if this Acceptor uses an internal Reactor, it is prob-
    // ably a good idea to stop the latter as well.  In that case, this method
    // does not even return:
    if (m_reactorUnP)
    {
      assert(m_reactorUnP.get() == m_reactor && m_reactor != nullptr);
      EPollReactor::ExitImmediately(a_where);
    }
  }

  //=========================================================================//
  // "EAcceptor_FIX::AcceptHandler":                                         //
  //=========================================================================//
  // TODO: We currently accept connections from all Clients; need to implement
  // Client IP filtering:
  //
  template<FIX::DialectT::type D, typename Conn, typename Derived>
  inline bool EAcceptor_FIX<D, Conn, Derived>::AcceptHandler
  (
    int               DEBUG_ONLY(a_acceptor_fd),
    int               a_client_fd,
    IO::IUAddr const* DEBUG_ONLY(a_client_addr),
    int               DEBUG_ONLY(a_client_addr_len)
  )
  {
    assert(a_acceptor_fd == m_accptFD && a_client_fd >= 0    &&
           a_client_addr != nullptr   && a_client_addr_len > 0);

    // ReadHandler for the Client Socket:
    // The actual ReadHandler is provided by the Derived class:
    IO::FDInfo::ReadHandler  readH
    (
      [this, a_client_fd]
        (int        DEBUG_ONLY(a_fd),  char const* a_buff, int a_size,
         utxx::time_val a_recv_time)
      -> int // TODO YYY return type bool --> int
      {
        assert(a_fd == a_client_fd);

        // TODO YYY
        return static_cast<Derived*>(this)->ReadHandler
               (a_client_fd, a_buff, a_size, a_recv_time);
      }
    );

    // ConnectHandler: Nothing to do, this is a Client socket which is already
    // connected:
    IO::FDInfo::ConnectHandler connH ([](int)->void{});

    // ErrorHandler for the Client Socket -- same as for the Acceptor Socket:
    IO::FDInfo::ErrHandler     errH
    (
      [this](int a_fd, int a_err_code, uint32_t a_events, char const* a_msg)
      -> void
      { this->ErrHandler(a_fd,  a_err_code, a_events, a_msg); }
    );

    // The EPollReactor will, in particular, create dynamic buffers to  handle
    // stream IO on this Socket.  NB: the typical size of a FIX msg is < 1K, so
    // it should be perfectly OK to use 1M buffers for both Read and Send:
    //
    constexpr size_t BuffSz    = 1024 * 1024; // 1M
    constexpr size_t BuffLWM   = MaxMsgSize;  // 8k to be safe

    // Attach this Socket to the Reactor. There is no name of this socket  yet;
    // later, it will be set to the corresp ClientCompID. For now, install the
    // NULL Session ptr as well:
    assert(m_reactor != nullptr);
    m_reactor->AddDataStream
      (nullptr, a_client_fd, readH,  connH,   errH,
       BuffSz,  BuffLWM,     BuffSz, BuffLWM);

    // Indicate that "a_client_fd" has been put into use (so the caller should
    // NOT close it now):
    return true;
  }

  //=========================================================================//
  // "EAcceptor_FIX::ErrHandler":                                            //
  //=========================================================================//
  template<FIX::DialectT::type D, typename Conn, typename Derived>
  inline void EAcceptor_FIX<D, Conn, Derived>::ErrHandler
  (
    int         a_fd,
    int         a_err_code,
    uint32_t    a_events,
    char const* a_msg
  )
  {
    // First of all, if this IO error is related to the AcceptorFD or TimerFD,
    // then it is a fatal error which results in the restart of the whole FIX
    // engine:
    if (utxx::unlikely(a_fd == m_timerFD || a_fd == m_accptFD))
    {
      m_logger->critical("EAcceptor_FIX::ErrHandler: ")
        << ((a_fd == m_timerFD) ? "TimerFD" : "AcceptorFD") << " Error: FD="
        << a_fd   << ", ErrNo=" << a_err_code       << ", Events="
        << m_reactor->EPollEventsToStr(a_events) << ": " << a_msg;

      // Terminate the whole Acceptor -- still try to do it gracefully:
      StopNow<true>("ErrHandler");
      return;
    }

    // Otherwise: The error should be in a Client Session socket.
    // Get the Session:
    FIXSession* sess = GetFIXSession(a_fd);

    if (utxx::unlikely(sess == nullptr))
    {
      if (m_debugLevel >= 1)
        m_logger->warn("EAcceptor_FIX::ErrHandler: FD=")
          << a_fd << ", ErrNo=" << a_err_code      << ", Events="
          << m_reactor->EPollEventsToStr(a_events) << ": " << a_msg
          << ", but no FIX session found";
      return;
    }

    // Otherwise: Generic case: Got a Session. We cannot terminate it gracefully
    // because the conenction to the Client is probably lost, so:
    char  errMsg[512];
    char* p =  stpcpy(errMsg, "Socket I/O Error: ErrNo=");
    utxx::itoa(a_err_code, p);
    p       =  stpcpy(p,      ": ");
    p       =  stpcpy(p,      a_msg);
    // Graceful=false:
    TerminateSession<false>(sess, p, "ErrHandler");
  }

  //=========================================================================//
  // "EAcceptor_FIX::TimerHandler":                                          //
  //=========================================================================//
  template<FIX::DialectT::type D, typename Conn, typename Derived>
  inline void EAcceptor_FIX<D, Conn, Derived>::TimerHandler()
  {
    // XXX: On each 1-sec timer tick, we have to traverse all ACTIVE Sessions;
    // for efficiency, we traverse not Sessions but "FDInfo"s attached to the
    // Reactor:
    utxx::time_val now = utxx::now_utc();

    auto FDAction =
      [this, now](IO::FDInfo const& a_info)->bool
      {
        // If we got into this call-back, the FD on the Reactor side must be
        // valid:
        assert(0 <= a_info.m_fd && a_info.m_fd < MaxFDs);

        // Get the corresp Session:
        FIXSession* sess = a_info.m_userData.Get<FIXSession*>();

        if (utxx::unlikely(sess == nullptr))
          // This may happen, eg, for the Acceptor Socket, or for a Client which
          // has connected but not logged on yet.  Still continue:
          return true;

        // Check the FDs consistency -- similar to "GetFIXSession":
        if (utxx::unlikely(!CheckFDConsistency(a_info.m_fd, sess)))
          return false;  // Acceptor is stopped in this case, so don't continue

        // Now OK:
        assert(a_info.m_fd == sess->m_fd);

        // Did we request a HeartBeat from this Client, and did not get a resp
        // in time?
        if (utxx::unlikely
           (!(sess->m_testReqTS.empty()) &&
            (now - sess->m_testReqTS).sec() > sess->m_heartBeatSec))
          // The client did not respond to a "TestRequest" for another HeartBeat
          // period, so terminate that Session now (still gracefully):
          TerminateSession<true>(sess, "No HeartBeat received", "TimerHandler");
        else
        // Is it time to request a HeartBeat (if there was no activity for a
        // while)?
        if (!(sess->m_lastRxTS.empty())  &&
            (now - sess->m_lastRxTS).sec () > sess->m_heartBeatSec)
        {
          sess->m_testReqTS = now;
          SendTestRequest(sess);
        }

        // Other way round, maybe it's time to send our own HeartBeat to the
        // Client (not waiting for its TestRequest)?
        if (!(sess->m_lastTxTS.empty())  &&
            (now - sess->m_lastTxTS).sec () > sess->m_heartBeatSec)
          SendHeartBeat(sess, nullptr);

        // Continue traversing:
        return true;
      };

    // Perform the traversal:
    m_reactor->TraverseFDInfos(FDAction);
  }

  //=========================================================================//
  // "GetMMapSize":                                                          //
  //=========================================================================//
  template<FIX::DialectT::type D, typename Conn, typename Derived>
  inline size_t EAcceptor_FIX<D, Conn, Derived>::GetMMapSize() const
  {
    // XXX: Currently, MMap is used for Sessions data only. Add some overhead
    // for the Seqment Mgr internal data:
    size_t res = MaxSessions * sizeof(FIXSession) + 65536;

    // Since we reserve "MaxMMapSegmSz" of address space  to each mapped segm
    // (see "GetMMapAddr"), the size must be limited by that value:
    if (utxx::unlikely(res > MaxMMapSegmSz))
      throw utxx::runtime_error
            ("EAcceptor::GetMMapSize: Calculated Size Too Large: ", res);

    // If OK:
    return res;
  }

  //=========================================================================//
  // "GetFIXSession(FD)":                                                    //
  //=========================================================================//
  // NB: "GetFIXSession" method tends not to throw any exceptions:
  // (*) if Session is not found, NULL ptr is returned;
  // (*) if gross inconsistencies are encountered, the whole FIX Acceptor is
  //     stopped:
  //
  template<FIX::DialectT::type D, typename Conn, typename Derived>
  inline FIXSession* EAcceptor_FIX<D, Conn, Derived>::GetFIXSession
    (int a_fd)
  const
  {
    if (utxx::unlikely(a_fd < 0 || a_fd >= MaxFDs))
      return nullptr;

    // NB: Need "try-catch" here because "GetUserData" throws an exception  if
    // "a_fd", even in a valid range, is not attached to the Reactor (which is
    // unlikely but possible):
    FIXSession* res = nullptr;
    try   { res = m_reactor->GetUserData<FIXSession*>(a_fd); }
    catch (...) {}

    if (utxx::unlikely(res == nullptr))
      return nullptr;

    // Otherwise: Got a FIXSession ptr.  Perform a "soft-check" of  consistency
    // between the Reactor and the Session SocketFDs (if it fails, the Acceptor
    // is stopped and NULL is returned):
    if (utxx::unlikely(!CheckFDConsistency(a_fd, res)))
      return nullptr;

    // If OK:
    assert(res != nullptr);
    return res;
  }

  //=========================================================================//
  // "GetFIXSession(ClientCompID)":                                          //
  //=========================================================================//
  // Again, no exceptions are thrown; NULL is returned if the ClientCompID is
  // empty:
  //
  template<FIX::DialectT::type D, typename Conn, typename Derived>
  inline FIXSession* EAcceptor_FIX<D, Conn, Derived>::GetFIXSession
    (char const* a_ccid)
  const
  {
    if (utxx::unlikely(a_ccid == nullptr || *a_ccid == '\0'))
      return nullptr;

    // Generic Case:
    int         s   = int(MkHashCode64(a_ccid) % MaxSessions);
    FIXSession* res = m_sessions + s;

    std::cout << "res->m_clientCompID.data() = " << res->m_clientCompID.data() << std::endl;
    std::cout << "a_ccid                     = " << a_ccid << std::endl;

    // TODO YYY Process case if client sends unknown FIX credentials.
    // TODO YYY User may send any CompID, and hash collision with existing session may occur.
    // TODO YYY We must not crash on assert in this case.
    if (strcmp(res->m_clientCompID.data(), a_ccid) != 0)
    {
      // TODO YYY Now will check of result for null when this func is used.
      return nullptr;
    }

    // There must be no hash collisions:
    // TODO YYY this assert was failing.
    assert(strcmp(res->m_clientCompID.data(), a_ccid) == 0);

    // NB: We do NOT check the contents of (*res) because it may not be initia-
    // lised yet:
    // TODO YYY for now it is always initialised.
    return res;
  }

  //=========================================================================//
  // "GetFIXSession(LiquidtyFixSession)":                                    //
  //=========================================================================//
  // Yet another "GetFIXSession" -- by an internal ECN Session obj:
  //
  template<FIX::DialectT::type D, typename Conn, typename Derived>
  inline FIXSession* EAcceptor_FIX<D, Conn, Derived>::GetFIXSession
    (Ecn::FixServer::LiquidityFixSession const& a_sess)
  const
  {
    char const* ccid = a_sess.SenderCompId.c_str();
    char const* scid = a_sess.TargetCompId.c_str();

    // TODO YYY SenderCompId --> TargetCompId
    FIXSession* res  = GetFIXSession(scid);

    // The "res" must be valid and active:
    if (utxx::unlikely
       (res == nullptr   ||   !(res->IsActive())      ||

       // TODO YYY swap scid <----> ccid
        strcmp(res->m_clientCompID.data(), scid) != 0 ||
        strcmp(res->m_serverCompID.data(), ccid) != 0))
      throw utxx::badarg_error

          // TODO YYY SenderCompId --> TargetCompId
            ("EAcceptor_FIX::GetFIXSession: Invalid or Inactive TargetCompId=",
             scid);
    // If OK:
    return res;
  }

  //=========================================================================//
  // "CheckFDConsistency":                                                   //
  //=========================================================================//
  // If a Session is referred to from the Reactor (via "a_fd" attached  to the
  // latter), Session's "m_fd" should be same as "a_fd". If not, it could be a
  // correctabe or a critical error:
  //
  template<FIX::DialectT::type D, typename Conn, typename Derived>
  inline bool EAcceptor_FIX<D, Conn, Derived>::CheckFDConsistency
    (int a_fd, FIXSession* a_session)
  const
  {
    assert(0 <= a_fd && a_fd < MaxFDs && a_session != nullptr);

    // Is the Session's SocketFD valid?
    bool validSock  = (0 <= a_session->m_fd && a_session->m_fd < MaxFDs);

    if (utxx::unlikely(!validSock))
    {
      // SessionFD is not valid: This is strange but still correcatble:
      m_logger->warn
        ("EAcceptor_FIX::CheckFDConsistency: ReactorFD=")
        << a_fd << ", Slot=" << (a_session - m_sessions) << ", ClientCompID="
        << a_session->m_clientCompID.data() << ": SessionFD was not set";

      // Repare the Session's SocketFD and fall through:
      a_session->m_fd = a_fd;
    }
    else
    if (a_session->m_fd != a_fd)
    {
      // SessionFD is formally valid, but does not match "a_fd": This is a cri-
      // tical error:
      m_logger->critical
        ("EAcceptor_FIX::CheckFDConsistency: Invariant Violation: ReactorFD=")
        << a_fd << ", Slot=" << (a_session - m_sessions) << ", ClientCompID="
        << a_session->m_clientCompID.data()  << ": Stopping the FIX Acceptor";

      // XXX: Throwing "logic_error" would not be enough, as this error cond may
      // affect multiple Sessions. And do not stop gracefully in this case:
      StopNow<false>("CheckFDConsistency");
      return false;
    }
    // If we got here: OK:
    return true;
  }

  //=========================================================================//
  // "CheckFIXSession":                                                      //
  //=========================================================================//
  // Checks the given FIXSession against the Msg's {Sender|Target}CompID;  the
  // FD is only needed if Session is NULL (ie we are undergoing a LogOn):
  // Returns "true" iff the Session is valid and we can continue processing of
  // this msg:
  //
  template<FIX::DialectT::type D, typename Conn, typename Derived>
  template<FIX::MsgT  MT>
  inline bool EAcceptor_FIX<D, Conn, Derived>::CheckFIXSession
  (
    int                    a_fd,
    FIX::MsgPrefix const&  a_msg,
    FIXSession**           a_session
  )
  const
  {
    //-----------------------------------------------------------------------//
    // Verify the Params:                                                    //
    //-----------------------------------------------------------------------//
    // Name of this function:
    constexpr char FuncName[]
      { 'G', 'e', 't', 'F', 'I', 'X', 'S', 'e', 's', 's', 'i', 'o', 'n', '<',
        char(MT), '>', '\0' };

    enum  StatusT  { OK = 0, MsgRejected = 1, SessionLost = 2 };
    StatusT status = OK;

    assert(0 <= a_fd && a_fd < MaxFDs && a_session != nullptr);
    FIXSession* sess = *a_session;

    // "sess" must already be consistent with the "a_fd" (see "CheckFDConsist-
    // ency"  above):
    assert(sess == nullptr || sess->m_fd == a_fd);

    //-----------------------------------------------------------------------//
    // "sess" is allowed to be NULL only for LogOn msgs:                     //
    //-----------------------------------------------------------------------//
    // (In that case, no association between FDs and Sessions is established yet
    // -- this is done below):
    if (utxx::unlikely(MT != FIX::MsgT::LogOn && sess == nullptr))
    {
      // This means, we got a non-LogOn msg but the Client is not logged on yet;
      // because of this, we do not even send a "Reject" back -- terminate the
      // Session immediately: Graceful=false, FindSession=false:
      //
      TerminateSession<false, false>(a_fd, "Not Logged On", FuncName);
      status = SessionLost;
    }
    else
    if (utxx::unlikely(MT == FIX::MsgT::LogOn && sess != nullptr))
      // Other way round -- it's LogOn, but the Session is already configured,
      // so it's a repeated LogOn. In this case, send a "Reject", and only if
      // it fails, terminate the Session:
      status =
        (RejectMsg
          (sess, a_msg.m_MsgSeqNum, MT, FIX::SessRejReasonT::Other,
           "Repeated LogOn", FuncName))
        ? MsgRejected
        : SessionLost;
    else
    //-----------------------------------------------------------------------//
    // "sess" is OK (or NULL for LogOn), verify the {Client|Server}CompID:   //
    //-----------------------------------------------------------------------//
    // (*) Sender(Client)CompID in "msg" must always be present;
    // (*) for non-LogOn msgs, it must always coincide with the one stored in
    //     "*sess" obj (in this case, "sess" is always non-NULL):
    //
    if (utxx::unlikely(IsEmpty(a_msg.m_SenderCompID) ||
                       IsEmpty(a_msg.m_TargetCompID)))
    {
      // Missing ClientCompID: Do a Reject (if logged on) or a Non-Graceful
      // termination (otherwise):
      char const* errMsg = "Missing SenderCompID or TargetCompID";
      if (sess != nullptr)
        status =
          (RejectMsg
            (sess, a_msg.m_MsgSeqNum, MT,
             FIX::SessRejReasonT::CompIDProblem, errMsg,  FuncName))
          ? MsgRejected
          : SessionLost;
      else
      {
        // Graceful=false, FindSession=false:
        TerminateSession<false, false>(a_fd, errMsg, FuncName);
        status = SessionLost;
      }
    }
    else
    if (utxx::unlikely
       (sess != nullptr                              &&
       (sess->m_clientCompID != a_msg.m_SenderCompID ||
        sess->m_serverCompID != a_msg.m_TargetCompID)))
      // "a_msg" came with a different SenderCompID than the one  previously
      // used in LogOn, or has an inconsistent TargetCompID: Reject this msg
      // as well:
      // TODO: We may also need to consider the ClientSubID (for Dialects like
      // HotSpotFX), but this is not implemented yet...
      status =
        (RejectMsg
          (sess, a_msg.m_MsgSeqNum,  MT, FIX::SessRejReasonT::CompIDProblem,
          "UnExpected SenderCompID", FuncName))
        ? MsgRejected
        : SessionLost;

    //-----------------------------------------------------------------------//
    // Error Handling:                                                       //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely(status == MsgRejected))
    {
      // Msg rejected (so cannot be processed further -- return "false"), but
      // the Session is valid, and is not reset:
      assert(sess != nullptr);
      return false;
    }
    else
    if (utxx::unlikely(status == SessionLost))
    {
      // TCP session was already closed -- so cannot continue with this msg,
      // AND reset the session:
      *a_session = nullptr;
      return false;
    }

    //-----------------------------------------------------------------------//
    // Special treatment of "LogOn":                                         //
    //-----------------------------------------------------------------------//
    // XXX: Should we do it here or in "ProcessLogOn"? We prefer the former way,
    // so that this method would return a non-NULL Session ptr for all Msg types
    // if a msg was not rejected:
    //
    if (MT == FIX::MsgT::LogOn)
    {
      // Get the Session obj slot for this Session via a ClientCompID hash:
      assert(sess == nullptr && !IsEmpty(a_msg.m_SenderCompID));

      sess = GetFIXSession(a_msg.m_SenderCompID.data());

      // TODO YYY sess may be null, if client is logging on with wrong id.
      //assert(sess != nullptr);   // Because SenderCompID is non-empty
      if (sess == nullptr)
      {
        return false;
      }

      // The ClientCompID saved in the "sess" must either be empty, or coincide
      // with the one specified in LogOn msg -- otherwise we got a collision of
      // ClientCompIDs:
      // TODO YYY chnged condition for collisions
      if (utxx::unlikely(
        !sess->m_clientCompID.empty())
        && sess->m_clientCompID != a_msg.m_SenderCompID)
      {
        m_logger->critical
          ("EAcceptor_FIX::")
          << FuncName  << ": SenderCompID=" << a_msg.m_SenderCompID.data()
          << ": Collision with existing ClientCompID="
          << sess->m_clientCompID.data()    << ", Slot="
          << int(sess - m_sessions);
        return false;
      }

      // TODO YYY checking for duplicate sessions
      if (sess->IsActive())
      {
        // Other way round -- it's LogOn, but the Session is already configured,
        // so it's a repeated LogOn. In this case, send a "Reject", and only if
        // it fails, terminate the Session:
        //        status =
        //            (RejectMsg
        //                (sess, a_msg.m_MsgSeqNum, MT, FIX::SessRejReasonT::Other,
        //                 "Repeated LogOn", FuncName))
        //            ? MsgRejected
        //            : SessionLost;

        return false;
      }

      // Otherwise: Got a Session (either an existing one, or a new one):
      *a_session = sess;

      // In case it this Session is created for the first time, save its
      // ClientCompID, and compute or re-compute the ClientCompHash:
      //
      sess->m_clientCompID   = a_msg.m_SenderCompID;
      sess->m_clientCompHash = MkHashCode64(sess->m_clientCompID.data());
      sess->m_clientSubID    = a_msg.m_SenderSubID;  // XXX: Currently not used
      sess->m_serverCompID   = a_msg.m_TargetCompID;

      // Allocate a new "FixSession" Obj to hold the above IDs (required  for
      // sending Session-specific events to the ECN Core):
      // IMPORTANT: The "EAcceptior_FIX" Ctor takes care that such "SPtr"s do
      // NOT become stale / invalid when the Acceptor re-starts (because  the
      // "FIXSession" objs containing those "SPtr"s persist in MMap):
      //
      sess->m_ecnSession     =
        Ecn::Basis::MakeSPtr<Ecn::FixServer::LiquidityFixSession>
        (
          // FIXME: Is this the correct order (is it is taken from the SERVER's
          // perspective)?
          Ecn::Model::Login::TFixSessionId(sess->m_serverCompID.data()), // Sndr
          Ecn::Model::Login::TFixSessionId(sess->m_clientCompID.data())  // Targ
        );

      // Associate the SocketFD with the Session:
      m_reactor->SetUserData<FIXSession*>(a_fd, sess);

      // XXX: What if there are left-over SocketFD in the Session obj?  --  Be-
      // cause that FD could have been re-used, it is safer NOT to close it but
      // simply override (though this can result in a leak of FDs);   and log a
      //warning:
      if (utxx::unlikely(sess->m_fd >= 0))
        m_logger->warn
          ("EAcceptor_FIX::ProcessLogOn: ClientCompID=")
          << a_msg.m_SenderCompID.data() << ", Slot="
          << int(sess - m_sessions)      << ": Left-over SocketFD="
          << sess->m_fd                  << ": Over-writing it...";

      // Save the Session Socket in the Session obj:
      sess->m_fd = a_fd;
    }
    else
      // If it is not a LogOn, the following consistency conds must hold (NB:
      // CompIDs were already checked above):
      assert(sess != nullptr && sess->m_fd == a_fd);

    //-----------------------------------------------------------------------//
    // All Done:                                                             //
    //-----------------------------------------------------------------------//
    // The Session is now known:
    assert(sess != nullptr);

    // Because of this, we can update the LastRx TimeStamp for this Session, and
    // the data size stats:
    sess->m_lastRxTS = a_msg.m_RecvTime;
    sess->m_bytesRx += size_t(a_msg.m_OrigMsgSz);

    // Also, because we got a msg pertinent to this Session, any previous Test-
    // Req (issued because the Client did not send a HeartBeat in time) is can-
    // celled:
    sess->m_testReqTS = utxx::time_val();

    // TODO YYY
    if (!this->CheckReceiveTime(a_msg.m_SendingTime))
    {
      throw utxx::runtime_error("Client-server clock drift.");
    }

    return true;
  }

  //=========================================================================//
  // "GetCurrMsgSeqNum":                                                     //
  //=========================================================================//
  // Scans "m_currMsg" for SeqNum (if the latter was not otherwise available):
  //
  template<FIX::DialectT::type D, typename Conn, typename Derived>
  inline SeqNum EAcceptor_FIX<D, Conn, Derived>::GetCurrMsgSeqNum() const
  {
    // Search for "|34=" in "m_currMsg" where "|" can also be '\x00' (most lik-
    // ely) or '\x01:
    void* p = memmem(m_currMsg, m_currMsgLen, "\x00" "34=", 4);
    if (p == nullptr)
      p =     memmem(m_currMsg, m_currMsgLen, "\x01" "34=", 4);
    if (p == nullptr)
      p =     memmem(m_currMsg, m_currMsgLen, "|34=",       4);

    if (utxx::unlikely(p == nullptr))
      // No SeqNum at all!
      return 0;

    // Otherwise: Get the SeqNum until a non-digital char (TollEOF=false):
    SeqNum res = 0;
    char const* begin = static_cast<char const*>(p) +  4;
    char const* end   = static_cast<char const*>(p) + 14;
    (void) utxx::fast_atoi<SeqNum, false>(begin, end, res);

    // NB: "res" can still be 0, but should not be negative:
    res  = std::max<SeqNum>(res, 0);
    return res;
  }

  //=========================================================================//
  // "TerminateSession":                                                     //
  //=========================================================================//
  // NB: These methods try not to throw any exceptions -- it is often invoked
  // from an exception handler!
  //=========================================================================//
  // "TerminateSession": By SocketFD:                                        //
  //=========================================================================//
  template<FIX::DialectT::type D, typename Conn, typename Derived>
  template<bool  Graceful, bool FindSession>
  inline void EAcceptor_FIX<D, Conn, Derived>::TerminateSession
  (
    int           a_fd,
    char const*   a_reason,
    char const*   a_where
  )
  const
  {
    std::cout << "TerminateSession<Graceful=" << Graceful << ", FindSession=" << FindSession << ">(" << a_fd << ", "<< a_reason << ", " << a_where << ")" << std::endl;
    // "Graceful" mode is only possible when "FindSession" is set.   The latter
    // may not be set when we know for sure that so such Session exists -- only
    // "a_fd" is to be closed:
    static_assert(FindSession || !Graceful,
                  "EAcceptor::TerminateSession: Graceful implies FindSession");

    // If "a_fd" is outright invalid, there is nothing to do:
    if (utxx::unlikely(a_fd < 0 || a_fd > MaxFDs))
      return;

    // Should we detatch and close "a_fd" by itsefl? Will see. At least, this is
    // always done if we handle the SocketFD only  and  NOT trying  to  find the
    // Session:
    bool doFD = !FindSession;

    if (FindSession)
    {
      // Get the Session.  The following can still return NULL:
      FIXSession*  sess = GetFIXSession(a_fd);

      if (utxx::likely(sess != nullptr))
      {
        // In this case, either the Session FD iis invalid, or valid and coin-
        // cides with "a_fd" (checked by "GetFIXSession"). In the former case,
        // "a_fd" will not be closed automatically along  with the Session, so
        // we will have to do it ourselves:
        assert (sess->m_fd < 0 || sess->m_fd == a_fd);
        doFD = (sess->m_fd != a_fd);

        // Invoke the Session-specific version:
        TerminateSession<Graceful>(sess, a_reason, a_where);
      }
      else
        // "sess" is NULL, then we certainly need to close and detach "a_fd":
        doFD = true;
    }

    // Finally:
    if (doFD)
    {
      std::cout << "Remove1 " << a_fd << std::endl;
      m_reactor->Remove(a_fd);
    }
  }

  //=========================================================================//
  // "TerminateSession": By FIXSession:                                      //
  //=========================================================================//
  template<FIX::DialectT::type D, typename Conn, typename Derived>
  template<bool  Graceful>
  inline void EAcceptor_FIX<D, Conn, Derived>::TerminateSession
  (
    FIXSession*  a_session,     // Non-NULL
    char const*  a_reason,
    char const*  a_where
  )
  const
  {
    assert(a_session != nullptr);

    int  fd        = a_session->m_fd;
    if (utxx::unlikely(fd < 0))
      // Nothing really to do:
      return;

    std::cout << "TerminateSession<Graceful=" << Graceful << ">( {" << a_session->m_fd << " "<< a_session->m_clientCompID.data() << " " << a_session->m_serverCompID.data()  << "} , " << a_reason << ", " << a_where << ")" << std::endl;
    std::cout << "(2) a_session->m_txSN=" << a_session->m_txSN << " a_session->m_rxSN=" << a_session->m_rxSN << std::endl;

    // Otherwise:
    bool realGrace = Graceful;

    // In "Graceful" mode, send a LogOut msg to the Client, but do NOT wait for
    // any confirmation from the Client. Again, cautch any exceptions:
    if (Graceful)
    try  { SendLogOut(a_session, a_reason);  }
    catch (std::exception const& exc)
    {
      if (m_debugLevel >= 1)
        m_logger->warn
          ("EAcceptor_FIX::TerminateSession(")
          << a_where << "): ClientCompID="
          << a_session->m_clientCompID.data()   << ", FD="  << fd
          << ": Exception in Graceful LogOut: " << exc.what();
      realGrace = false;
    }

    // Detach and close the Session Socket. Again, for extra safety, catch pos-
    // sible exceptions -- but unlike the above, if an exceptin occurs here, it
    // is a very serious error condition:
    // NB: The following also NULLifies the UserData (Session back-ptr). Norma-
    // lly, it should not throw any exceptions:
    //

    std::cout << "Remove2 " << fd << " " << a_reason << " " << a_where << " " << std::endl;
    m_reactor->Remove(fd);
    a_session->m_fd = -1;

    // The Session is now Inactive:
    assert(!(a_session->IsActive()));

    // Log this event:
    if (m_debugLevel >= 1)
      m_logger->info
        ("EAcceptor_FIX::TerminateSession(")
        << a_where      << "): ClientCompID="
        <<  a_session->m_clientCompID.data() << ", Slot="
        << (a_session - m_sessions)          << ", Socket=" << fd
        << ": Session Terminated "
        << (realGrace ? "Gracefully" : "Non-Gracefully")    << "; Reason: "
        << a_reason;
  }

  //=========================================================================//
  // "RejectMsg":                                                            //
  //=========================================================================//
  // Tries to respond with a proper "Reject" (then returns "true"); if that was
  // not possible, terminates the curr session and returns "false". Similar to
  // "TerminateSession", we are trying NOT  to propagate  any  exceptions from
  // this method:
  //
  template<FIX::DialectT::type D, typename Conn, typename Derived>
  inline bool EAcceptor_FIX<D, Conn, Derived>::RejectMsg
  (
    FIXSession*          a_session,     // Must be non-NULL
    SeqNum               a_rejected_sn, // If invalid, will be auto-determined
    FIX::MsgT            a_rejected_mtype,
    FIX::SessRejReasonT  a_reason,
    char const*          a_msg,
    char const*          a_where
  )
  const
  {
    assert(a_session != nullptr);

    // Try to determine the SeqNum of the failed (rejected) Client msg:
    if (a_rejected_sn <= 0)
      a_rejected_sn = GetCurrMsgSeqNum();

    // If got a proper SeqNum: try to send a "Reject" msg to the Client:
    if (utxx::likely(a_rejected_sn > 0))
      try
      {
        SendReject(a_session, a_rejected_sn, a_rejected_mtype, a_reason, a_msg);

        // Log this event:
        if (utxx::unlikely(m_debugLevel >= 2))
          m_logger->info
            ("EAcceptor_FIX::RejectMsg(")
            << a_where << "): ClientCompID="
            << a_session->m_clientCompID.data() << ", SeqNum=" << a_rejected_sn
            << ": Reject Sent: Reason=" << int(a_reason)       << ": "
            << a_msg;

        // We do NOT disconnect the Session in this case:
        return true;
      }
      catch (std::exception const& exc)
      {
        // "SendReject" was unsuccessful:
        if (m_debugLevel >= 1)
          m_logger->warn
            ("EAcceptor_FIX::RejectMsg(")
            << a_where    << "): ClientCompID="
            << a_session->m_clientCompID.data() << ": SendReject failed: "
            << exc.what() << ", will terminate the session";

        // Will do a NON-GRACEFUL Session termination:
        TerminateSession<false>(a_session, a_msg, a_where);
        return false;
      }
    else
    {
      // No RejectedSN -- will have to do "TerminateSession", but can still try
      // to do it gracefullly:
      TerminateSession<true>(a_session, a_msg, a_where);
      return false;
    }
    // This point is not reachable...
  }
} // End namespace AlfaRobot
