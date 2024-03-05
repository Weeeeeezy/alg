// vim:ts=2:et
//===========================================================================//
//                          "Acceptors/TCP_Acceptor.hpp":                    //
//                  Common Base of TCP-Based Acceptors (Servers)             //
//===========================================================================//
#pragma once
#include "Acceptors/TCP_Acceptor.h"
#include "Basis/EPollReactor.h"
#include <utxx/compiler_hints.hpp>
#include <utxx/error.hpp>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cassert>

namespace MAQUETTE
{
  //=========================================================================//
  // Non-Default Ctor:                                                       //
  //=========================================================================//
  template<typename Derived>
  inline TCP_Acceptor<Derived>::TCP_Acceptor
  (
    // General Params:
    std::string const&                  a_name,
    EPollReactor  *                     a_reactor,       // Non-NULL
    spdlog::logger*                     a_logger,        // Non-NULL
    int                                 a_debug_level,
    // IO Params:
    std::string const&                  a_accpt_ip,
    int                                 a_accpt_port,
    int                                 a_buff_sz,
    int                                 a_buff_lwm,
    // Session Control:
    int                                 a_max_curr_sess,
    int                                 a_heart_beat_sec,
    // Any extra params (for the Derived class, ShM control etc): Optional:
    boost::property_tree::ptree const*  a_params
  )
  : // NB: UseDateInSegmName=false (unlile "EConnector"):
    PersistMgr<>  (a_name, a_params, false, GetMapSize()),
    m_name        (a_name),
    m_reactor     (a_reactor),
    m_logger      (a_logger),
    m_debugLevel  (a_debug_level),

    m_accptAddr   (),         // Initialised below
    m_accptFD     (-1),       // Not open yet
    m_timerFD     (-1),       // ditto
    m_maxCurrSess (a_max_curr_sess),
    m_currSess    (0),
    m_sessions    (m_segm->find_or_construct<typename Derived::SessionT>
                  (SessionsON())[MaxSess]()),
    m_heartBeatSec(a_heart_beat_sec),
    m_buffSz      (a_buff_sz),
    m_buffLWM     (a_buff_lwm),
    m_totBytesTx  (m_segm->find_or_construct<unsigned long>(TotBytesTxON())(0)),
    m_totBytesRx  (m_segm->find_or_construct<unsigned long>(TotBytesRxON())(0))
  {
    //-----------------------------------------------------------------------//
    // Check the Params:                                                     //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely
       (m_name.empty()      || m_reactor == nullptr || m_logger == nullptr ||
        m_maxCurrSess  <= 0 || m_heartBeatSec  <= 0 || m_buffSz <= 0       ||
        m_heartBeatSec <= 0 || m_buffLWM <= m_buffSz))
      throw utxx::badarg_error("TCP_Acceptor::Ctor: Invalid param(s)");

    if (utxx::unlikely(m_maxCurrSess > MaxFDs))
      throw utxx::badarg_error
            ("TCP_Acceptor::Ctor: Invalid setup: MaxCurrSess=", m_maxCurrSess,
             " but MaxFDs=", MaxFDs);

    //-----------------------------------------------------------------------//
    // Construct the Listening Address:                                      //
    //-----------------------------------------------------------------------//
    m_accptAddr.sin_family      = AF_INET;
    m_accptAddr.sin_addr.s_addr = inet_addr     (a_accpt_ip.data());
    m_accptAddr.sin_port        = htons(uint16_t(a_accpt_port));

    if (utxx::unlikely
       (m_accptAddr.sin_addr.s_addr == INADDR_NONE ||
        a_accpt_port < 0 || a_accpt_port > 65535))
      throw utxx::badarg_error
            ("TCP_Acceptor::Ctor: Invalid IP=", a_accpt_ip, " and/or Port=",
             a_accpt_port);

    //-----------------------------------------------------------------------//
    // Reset all Sessions:                                                   //
    //-----------------------------------------------------------------------//
    for (int i = 0; i < int(MaxSess); ++i)
    {
      typename Derived::SessionT& sess = m_sessions[i];
      sess.m_fd           = -1;
      sess.m_heartBeatSec = m_heartBeatSec;
    }
  }

  //=========================================================================//
  // "Start":                                                                //
  //=========================================================================//
  // Creates ListenerFD and TimerFD:
  //
  template<typename Derived>
  inline void TCP_Acceptor<Derived>::Start()
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
        m_logger->warn("TCP_Acceptor::Start: Already Started");
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
        SystemError(-1, "TCP_Acceptor::Start: Cannot create Acceptor Socket");

      // XXX: Any errors here result in socket being closed, and an exception
      // propagated. There is no way we can continue if the AcceptorSocket is
      // not created properly:
      int yes = 1;
      if (utxx::unlikely
         (setsockopt(m_accptFD, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))
          < 0))
        SocketError<true>
          (-1, m_accptFD,
           "TCP_Acceptor::Start: setsockopt(SO_REUSEADDR) failed");

      if (utxx::unlikely
         (bind(m_accptFD, reinterpret_cast<struct sockaddr *>(&m_accptAddr),
               sizeof(m_accptAddr)) < 0))
        SocketError<true>(-1, m_accptFD, "TCP_Acceptor::Start: bind() failed");

      // Create a Listening Queue on this socket; the Queue size corresponds to
      // the max number of allowed concurrent connections:
      //
      if (utxx::unlikely(listen(m_accptFD, m_maxCurrSess) < 0))
        SocketError<true>(-1, m_accptFD,
                          "TCP_Acceptor::Start: listen() failed");

      // Create the AcceptHandler:
      IO::FDInfo::AcceptHandler  accH
      (
        [this]
        (int a_acceptor_fd, int a_client_fd, IO::IUAddr const* a_addr) -> void
        { this->AcceptHandler(a_acceptor_fd, a_client_fd, a_addr); }
      );

      // Create the ErrHandler:
      IO::FDInfo::ErrHandler     errH
      (
        [this](int a_fd, int a_err_code, uint32_t a_events, char const* a_msg)
        -> void
        { this->ErrHandler(a_fd,  a_err_code, a_events, a_msg); }
      );

      // Attach the Acceptor Socket to the Reactor:
      m_reactor->AddAcceptor(m_name.data(), m_accptFD, accH, errH);

      //---------------------------------------------------------------------//
      // Create a Periodic Timer for managing HeartBeats:                    //
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

      //---------------------------------------------------------------------//
      // Successfully started:                                               //
      //---------------------------------------------------------------------//
      m_logger->info
        ("TCP_Acceptor::Start: Acceptor STARTED: AccptFD={}, TimerFD={}",
         m_accptFD, m_timerFD);
    }
    //-----------------------------------------------------------------------//
    // Clean-Up on any Exceptions:                                           //
    //-----------------------------------------------------------------------//
    catch (std::exception const& exc)
    {
      // XXX: The following fragment should not (normally) throw any exceptions
      // by itself:
      CloseAccptFD("Start");
      CloseTimerFD("Start");

      // Log the error and propagate it further:
      m_logger->critical("TCP_Acceptor::Start: EXCEPTION: {}", exc.what());
      throw;
    }
  }

  //=========================================================================//
  // "StopNow":                                                              //
  //=========================================================================//
  template<typename Derived>
  template<bool Graceful>
  inline void TCP_Acceptor<Derived>::StopNow(char const* a_where)
  {
    // Terminate all Sessions according to the "Graceful" flag. The actual ter-
    // mination is performed by the Derived class:
    for (int i = 0; i < MaxSess; ++i)
    {
      typename Derived::SessionT* sess = m_sessions + i;

      // NB: The following call is expected NOT to propagate any exceptions, and
      // to do nothing if "sess" is currently not connected (ie is dormant):
      // XXX: Does graceful termination make sense if we close the TimerFD below
      // anyway? We think it should still be OK:
      //
      static_cast<Derived*>(this)->template TerminateSession<Graceful>
        (sess, "StopNow", a_where);

      // XXX: "TerminateSession" is responsible for decrementing the "m_curr-
      // Sess" counter...
    }

    // Now detach and close the AcceptorFD and the TimerFD:
    CloseAccptFD("StopNow");
    CloseTimerFD("StopNow");

    m_logger->info
      ("TCP_Acceptor::StopNow({}), Graceful={}: Acceptpt STOPPED",
       a_where, Graceful);
  }

  //=========================================================================//
  // "Stop":                                                                 //
  //=========================================================================//
  // For the moment, it is just Graceful "StopNow":
  //
  template<typename Derived>
  inline void TCP_Acceptor<Derived>::Stop()
    { StopNow<true>("Stop"); }

  //=========================================================================//
  // "AcceptHandler":                                                        //
  //=========================================================================//
  // TODO: Implement filetring of incoming connections by IP Addr. For the mo-
  // ment, we accept connections from the interface(s) to which the AccptFD is
  // bound.
  // This method is invoked AFTER "accept" has been performed within the EPoll-
  // Reactor, and the ClientFD was returned:
  //
  template<typename Derived>
  inline void TCP_Acceptor<Derived>::AcceptHandler
  (
    int               DEBUG_ONLY(a_acceptor_fd),
    int               a_client_fd,
    IO::IUAddr const* DEBUG_ONLY(a_client_addr)
  )
  {
    assert(a_acceptor_fd == m_accptFD && a_client_fd >= 0 &&
           a_client_addr != nullptr);

    // If the limit for active Sessions is reached, we cannot create a new
    // Session -- so drop this Connection:
    if (utxx::unlikely(m_currSess >= m_maxCurrSess))
    {
      m_logger->warn
        ("TCP_Acceptor::AcceptHandler: Too many active Sessions: {}, DROPPING "
         "new Session with ClientFD={}", m_currSess,   a_client_fd);
      (void) close(a_client_fd);
      return;
    }

    // Otherwise: Generic Case:
    // ReadHandler for the Client Socket:
    // The actual ReadHandler is provided by the Derived class:
    IO::FDInfo::ReadHandler readH
    (
      [this, a_client_fd]
      (int DEBUG_ONLY(a_fd), char const* a_buff, int a_size,
       utxx::time_val a_ts_recv) -> int
      {
        assert(a_fd == a_client_fd);
        return static_cast<Derived*>(this)->ReadHandler
               (a_client_fd, a_buff, a_size, a_ts_recv);
      }
    );

    // ConnectHandler for the Client Socket:
    // Notionally required by "AddDataStream" below, but in fact, does nothing
    // since the Client Socket is already connected:
    //
    IO::FDInfo::ConnectHandler connH ([](int)->void{});

    // ErrorHandler for the Client Socket -- XXX: currently same as for the
    // Acceptor Socket:
    IO::FDInfo::ErrHandler     errH
    (
      [this](int a_fd, int a_err_code, uint32_t a_events, char const* a_msg)
      -> void
      { this->ErrHandler(a_fd,  a_err_code, a_events, a_msg); }
    );

    // Now attach the Client Socket to the Reactor (no name yet -- can be set
    // later by the Derived class' Session):
    assert(m_reactor != nullptr);
    m_reactor->AddDataStream
      (nullptr,  a_client_fd, readH,    connH,   errH,
       m_buffSz, m_buffLWM,   m_buffSz, m_buffLWM);

    // All Done!
  }

  //=========================================================================//
  // "ErrHandler":                                                           //
  //=========================================================================//
  template<typename Derived>
  inline void TCP_Acceptor<Derived>::ErrHandler
  (
    int         a_fd,
    int         a_err_code,
    uint32_t    a_events,
    char const* a_msg
  )
  {
    // First of all, if this IO error is related to the AcceptorFD or TimerFD,
    // then it is a fatal error which results in the restart of the whole TCP_
    // Acceptor:
    if (utxx::unlikely(a_fd == m_timerFD || a_fd == m_accptFD))
    {
      m_logger->critical
        ("TCP_Acceptor::ErrHandler: {}: Error: FD={}, ErrNo={}, Events={}: {}",
         (a_fd == m_timerFD) ? "TimerFD" : "AcceptorFD", a_fd,  a_err_code,
         m_reactor->EPollEventsToStr(a_events),  a_msg);

      // Terminate the whole Acceptor -- still try to do it gracefully:
      StopNow<true>("ErrHandler");
      return;
    }

    // Otherwise: The error should be in a Client Session socket. Get the Sess:
    typename Derived::SessionT* sess = GetSession(a_fd);

    // If, for any reason, Session was not found, there is nothing to do:
    if (utxx::unlikely(sess == nullptr))
    {
      if (m_debugLevel >= 1)
        m_logger->warn
         ("TCP_Acceptor::ErrHandler: ClientFD={}, ErrNo={}, Events={}: {}: "
          "Session not found",
          a_fd, a_err_code, m_reactor->EPollEventsToStr(a_events), a_msg);
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
    static_cast<Derived*>(this)->TerminateSession<false>(sess, p, "ErrHandler");

    // XXX: "TerminateSession" is responsible for decrementing the "m_currSess"
    // counter...
  }

  //=========================================================================//
  // "TimerHandler":                                                         //
  //=========================================================================//
  template<typename Derived>
  inline void TCP_Acceptor<Derived>::TimerHandler()
  {
    // XXX: On each 1-sec timer tick, we have to traverse all ACTIVE Sessions;
    // for efficiency, we traverse not Sessions but "FDInfo"s attached to the
    // Reactor:
    utxx::time_val now = utxx::now_utc();
    Derived*       der = static_cast<Derived*>(this);

    auto FDAction =
      [this, der, now](IO::FDInfo const& a_info)->bool
      {
        // If we got into this call-back, the FD on the Reactor side must be
        // valid:
        assert(0 <= a_info.m_fd && a_info.m_fd < MaxFDs);

        // Get the corresp Session:
        typename Derived::SessionT* sess =
          a_info.m_userData.Get<typename Derived::SessionT*>();

        if (utxx::unlikely(sess == nullptr))
          // This may happen, eg, for the Acceptor Socket, or for a Client which
          // has connected but not logged on yet.  Still continue:
          return true;

        // Check the FDs consistency -- similar to "GetSession(FD)":
        if (utxx::unlikely(!CheckFDConsistency(a_info.m_fd, sess)))
          return false;  // Acceptor is stopped in this case, so don't continue

        // Now OK:
        assert(a_info.m_fd == sess->m_fd);

        // The actual HearBeats mgmt is performed by the Derived Class:
        der->ManageHeartBeats(sess, now);

        // Continue traversing:
        return true;
      };

    // Perform the Traversal:
    assert(m_reactor != nullptr);
    m_reactor->TraverseFDInfos(FDAction);
  }

  //=========================================================================//
  // "GetMapSize":                                                           //
  //=========================================================================//
  template<typename Derived>
  inline size_t TCP_Acceptor<Derived>::GetMapSize()
  {
    // NB: ShM is primarily for storing the Sessions data (plus some counters),
    // but the Derived class may store some extra data there as well. Also inc-
    // lude some fixed overhead of 64k:
    size_t res =
      MaxSess * sizeof(typename Derived::SessionT) +
      Derived::GetExtraMapSize() + 65536;

    // Since we reserve "MaxMapSz" of address space  to each mapped segm (see
    // "GetMapAddr"), the size must be limited by that value:
    if (utxx::unlikely(res > PersistMgr<>::MaxSegmSz))
      throw utxx::runtime_error
            ("TCP_Acceptor::GetMapSize: Calculated Size Too Large: ", res);

    // If OK:
    return res;
  }

  //=========================================================================//
  // "CloseAccptFD":                                                         //
  //=========================================================================//
  template<typename Derived>
  inline void TCP_Acceptor<Derived>::CloseAccptFD(char const* a_where) const
  {
    assert(a_where != nullptr);

    // Detach and close "m_accptFD":
    if (m_accptFD >= 0)
    {
      m_reactor->Remove(m_accptFD);

      if (m_debugLevel >= 2)
        m_logger->info
          ("TCP_Acceptor::CloseAccptFD({}): OldAccptFD={}: TCP Socket Closed",
           a_where, m_accptFD);
      m_accptFD = -1;
    }
  }

  //=========================================================================//
  // "CloseTimerFD":                                                         //
  //=========================================================================//
  template<typename Derived>
  inline void TCP_Acceptor<Derived>::CloseTimerFD(char const* a_where) const
  {
    assert(a_where != nullptr);

    // Detach and close "m_timerFD":
    if (utxx::likely(m_timerFD >= 0))
    {
      m_reactor->Remove(m_timerFD);

      if (m_debugLevel >= 2)
        m_logger->info
          ("TCP_Acceptor::CloseTimerFD({}): OldTimerFD={}: Timer Removed",
           a_where, m_timerFD);
      m_timerFD = -1;
    }
  }

  //=========================================================================//
  // "GetSession(FD)":                                                       //
  //=========================================================================//
  // NB: "GetSession" method tends not to throw any exceptions:
  // (*) if Session is not found, NULL ptr is returned;
  // (*) if gross inconsistencies are encountered, the whole TCP Acceptor is
  //     stopped:
  //
  template<typename Derived>
  inline typename Derived::SessionT* TCP_Acceptor<Derived>::GetSession
    (int a_fd)
  const
  {
    if (utxx::unlikely(a_fd < 0 || a_fd >= MaxFDs))
      return nullptr;

    // NB:
    // (*) It is assumed that the Session Ptr is installed as UserData associa-
    //     ted with the Client FD in the Reactor;
    // (*) if it is not found taht way,  direct search over all active FDs  is
    //     performed;
    // (*) need "try-catch" here because "GetUserData" throws an exception  if
    //     "a_fd", even in a valid range, is not attached to the Reactor (which
    //     is unlikely but theoretically possible):
    //
    typename Derived::SessionT* res = nullptr;
    try   { res = m_reactor->GetUserData<typename Derived::SessionT*>(a_fd); }
    catch (...) {}

    if (utxx::unlikely(res == nullptr))
      // Perform direct search by FD. NB: The "m_sessions" array is NOT indexed
      // by FDs, because it may also contain currently-inactive Sessions; it is
      // indexed by the Session Name hash codes:
      for (int i = 0; i < MaxSess; ++i)
        if (utxx::unlikely(m_sessions[i].m_fd == a_fd))
        {
          res = m_sessions + i;
          break;
        }
    // Still not found???
    if (utxx::unlikely(res == nullptr))
      return nullptr;

    // Generic Case:
    // Yes, got a Session ptr. Perform a "soft-check" for consistency between
    // the Reactor and the Session SocketFDs    (if it fails, the Acceptor is
    // stopped and NULL is returned):
    //
    if (utxx::unlikely(!CheckFDConsistency(a_fd, res)))
      return nullptr;

    // If OK:
    assert(res != nullptr);
    return res;
  }

  //=========================================================================//
  // "CheckFDConsistency":                                                   //
  //=========================================================================//
  // If a Session is referred to from the Reactor (via "a_fd" attached  to the
  // latter), Session's "m_fd" should be same as "a_fd". If not, it could be a
  // correctabe or a critical error:
  //
  template<typename Derived>
  inline bool TCP_Acceptor<Derived>::CheckFDConsistency
    (int a_fd, typename Derived::SessionT* a_sess)
  {
    // "a_fd" comes from the Reactor, so it must be below MaxFDs:
    assert(0 <= a_fd && a_fd < MaxFDs && a_sess != nullptr);

    // Is the Session's recorded SocketFD valid?
    bool validSock  = (0 <= a_sess->m_fd && a_sess->m_fd < MaxFDs);

    if (utxx::unlikely(!validSock))
    {
      // SessionFD is not valid: This is strange but still correcatble:
      m_logger->warn
        ("TCP_Acceptor::CheckFDConsistency: ReactorFD={}, Slot={}, SessName={}"
         ": SessFD was not set properly, fixing it!",
         a_fd, a_sess - m_sessions, a_sess->GetName()),

      // Repare the Session's SocketFD and fall through:
      a_sess->m_fd = a_fd;
    }
    else
    if (a_sess->m_fd != a_fd)
    {
      // SessionFD is formally valid, but does not match "a_fd": This is a cri-
      // tical error:
      m_logger->critical
        ("TCP_Acceptor::CheckFDConsistency: Invariant Violation: ReactorFD={}, "
         "Slot={}, SessName={}: Stopping the Acceptor",
         a_fd, a_sess - m_sessions, a_sess->GetName());

      // XXX: Throwing "logic_error" would not be enough, as this error cond may
      // affect multiple Sessions.  And do NOT stop gracefully in this case:
      StopNow<false>("CheckFDConsistency");
      return  false;
    }
    // If we got here: OK:
    return true;
  }
} // End namespace MAQUETTE
