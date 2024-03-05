// vim:ts=2:et
//===========================================================================//
//                           "Basis/EPollReactor.cpp":                       //
//                        EPoll-Based IO Events Reactor                      //
//===========================================================================//
#include "Basis/EPollReactor.h"
#include "Basis/IOUtils.hpp"
#include <utxx/compiler_hints.hpp>
#include <utxx/error.hpp>
#include <linux/capability.h>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <sys/signalfd.h>
#include <unistd.h>
#include <sched.h>
#include <cassert>

using namespace std;

namespace MAQUETTE
{
  //=========================================================================//
  // "EPollReactor" Non-Default Ctor:                                        //
  //=========================================================================//
  EPollReactor::EPollReactor
  (
    spdlog::logger* a_logger,
    int             a_debug_level,
    bool            a_use_poll,
    bool            a_use_kernel_tls,
    int             a_max_fds,
    int             a_cpu
  )
  : m_MaxFDs        (a_max_fds),
    m_usePoll       (a_use_poll),
    // For Poll:
    m_pollFDs       (m_usePoll  ? new pollfd[unsigned(m_MaxFDs)] : nullptr),
    // For EPoll:
    m_epollFD       (!m_usePoll ? epoll_create1(0) : -1),
                                        // Master EPollFD created HERE!
    m_MaxEvents     (!m_usePoll ? (2 * m_MaxFDs)   :  0),
                                        // Reasonable estimate
    m_epEvents      (!m_usePoll
                     ? new epoll_event[unsigned(m_MaxEvents)] : nullptr),
    // Generic:
    m_fdis          (new    IO::FDInfo[unsigned(m_MaxFDs)]),
    m_topFD         (-1),               // No active FDs yet
    m_useKernelTLS  (a_use_kernel_tls), // Use Linux Kernel TLS, not GNUTLS
    m_useVMA        (false),            // Set below
    m_udprxs        (new IO::UDPRX[NU]),
    m_tlsCreds      (nullptr),
    m_logger        (a_logger),
    m_debugLevel    (a_debug_level),
    m_currInstID    (0),                // Always-incr, to prevent false reuse
    m_isRunning     (false),            // In Run? Not yet!
    m_ghs           ()
  {
    LOG_VERSION()
    CHECK_ONLY
    ( // Verify the Params:
      // NB: MaxUDPSz may be 0 if we are not going to use UDP at all:
      if (utxx::unlikely(m_logger == nullptr || m_MaxFDs <= 0))
        throw utxx::badarg_error("EPollReactor::Ctor: Invalid arg(s)");
    )
    if (m_usePoll)
    {
      // Poll: Clear all PollFDs:
      for (int fd = 0; fd < m_MaxFDs; ++fd)
      {
        pollfd* pfd  =  m_pollFDs + fd;
        pfd->fd      = -1;
        pfd->events  =  0;
        pfd->revents =  0;
      }
    }
    else
    {
      // EPoll: Create the MasterFD:
      if (utxx::unlikely(m_epollFD < 0))
        IO::SystemError(-1, "EPollReactor::Ctor: epoll_create1() Failed");

      // Just to be safe, zero-out the "epEvents":
      assert(m_epEvents != nullptr);
      memset(m_epEvents, '\0', unsigned(m_MaxEvents) * sizeof(epoll_event));
    }
    // NB: "m_fdis" entries are initialized by "FDInfo" default ctors, so no
    // need to do it explicitly...

    // Check whether LibVMA is used:
    char const* ldPreLoad = getenv("LD_PRELOAD");
    if (ldPreLoad != nullptr && strstr(ldPreLoad, "libvma.so") != nullptr)
    {
      m_useVMA = true;
      LOG_INFO(2, "EPollReactor::Ctor: Using LibVMA!")

      // However, LibVMA is incompatible with KernelTLS:
      if (m_useKernelTLS)
      {
        m_useKernelTLS = false;
        LOG_WARN(1, "EPollReactor::Ctor: UseKernelTLS reset to False "
                    "(incompatible with LibVMA)")
      }
    }
    // If possible, run at the highest RT Priority on the designated CPU:
    if (geteuid() == 0)
    {
      IO::SetRTPriority(IO::MaxRTPrio);
      if (a_cpu >= 0)
        IO::SetCPUAffinity(a_cpu);
    }
  }

  //=========================================================================//
  // "EPollReactor" Dtor:                                                    //
  //=========================================================================//
  EPollReactor::~EPollReactor() noexcept
  {
    // Do not allow any exceptions to propagate from this Dtor:
    try
    {
      // Poll:
      if (utxx::likely(m_pollFDs != nullptr))
      {
        delete[] m_pollFDs;
        m_pollFDs = nullptr;
      }

      // EPoll: Close the Master Socket:
      if (m_epollFD != -1)
      {
        (void) close(m_epollFD);
        m_epollFD = -1;
      }

      // EPoll: De-allocate the Events vector:
      if (utxx::likely(m_epEvents != nullptr))
      {
        delete[]  m_epEvents;
        m_epEvents = nullptr;
      }

      // In any case: Remove the "FDInfo"s:
      if (utxx::likely(m_fdis   != nullptr))
      {
        delete[] m_fdis;   // Will invoke Dtors on all "FDInfo"s
        m_fdis   = nullptr;
      }
      m_topFD    = -1;

      // De-allocate UDPRXs:
      if (utxx::likely(m_udprxs != nullptr))
      {
        delete[] m_udprxs;
        m_udprxs = nullptr;
      }

      // m_loggerShP is finalised automaticaly, but we reset the direct ptr:
      m_logger = nullptr;
    }
    catch (...) {}
  }

  //=========================================================================//
  // "CheckFD":                                                              //
  //=========================================================================//
  // Can be used in both "Add" and "!Add=Remove" modes. Returns the ref to the
  // corresp FDInfo slot:
  //
  template<bool IsNew, bool IsRemove>
  inline IO::FDInfo& EPollReactor::CheckFD
  (
    char const* CHECK_ONLY(a_name),   // Can be NULL
    int         a_fd,
    char const* CHECK_ONLY(a_where)   // Non-NULL
  )
  {
    // Obviously, "IsNew" and "IsRemove" should not both be set. However, it is
    // possible that none of them is set (if "a_fd" is an existing FD which  is
    // NOT being removed now):
    static_assert
      (!(IsNew && IsRemove),
       "EPollReactor::CheckFD: IsNew and IsRemove cannot both be set");

    // Check that "a_fd" is a valid index in "m_fdis". NB: Although in general
    // if would be OK to have a_fd==-1 for Remove, this needs to be checked by
    // the Caller, because this method must return an FDInfo ref in any case:
    assert(a_where != nullptr);
    CHECK_ONLY
    ( if (utxx::unlikely(a_fd < 0 || a_fd >= m_MaxFDs))
        throw utxx::badarg_error
              ("EPollReactor::",    a_where, ": Invalid FD=", a_fd, ", Name=",
              (a_name != nullptr) ? a_name : "NULL");
    )
    // Get the "FDInfo":
    IO::FDInfo& info = m_fdis[a_fd];

    CHECK_ONLY
    (
      // The rest depends on the "IsNew" mode:
      if (IsNew)
      {
        // The "slot" must be empty   (XXX: for efficiency, we only check some
        // critical flds; all other flds must then be empty by construction as
        // well):
        if (utxx::unlikely(info.m_fd >= 0 || info.m_inst_id != 0))
          throw utxx::badarg_error
                ("EPollReactor::", a_where,   ": Slot is not empty: FD=", a_fd,
                 ", StoredFD=",    info.m_fd, ", InstID=",      info.m_inst_id,
                 ", Name=",        info.m_name.data());

      // XXX: But do NOT store "a_fd" and a new InstID in that "info": further
      // checks may need to be performed by the Caller before it is safe to de-
      // clare this slot to be "occupied"...
      }
      else
      {
        // Existing socket: Then, with an exception of "Remove" op,  the stored
        // "m_fd" must be the same as "a_fd".  For "Remove", (m_fd != a_fd)  is
        // allowed, but only if "m_fd" itself is invalid;    this can happen in
        // case of a repeated "Remove".
        bool fdOK1 = (info.m_fd == a_fd)    || (IsRemove && info.m_fd < 0);

        // In any case (whether m_fd is valid or not), the following must hold:
        bool fdOK2 = (info.m_fd <= m_topFD);

        // Thus:
        if (utxx::unlikely(!(fdOK1 && fdOK2)))
          throw utxx::badarg_error
                ("EPollReactor::", a_where,            ": OldFD=", info.m_fd,
                 ", OldName=",     info.m_name.data(), ", NewFD=", a_fd,
                 ", NewName=",    (a_name != nullptr) ? a_name : "NULL",
                 ", TopFD=",       m_topFD);
      }
    )
    return info;
  }

  //-------------------------------------------------------------------------//
  // As above, but "const" version:                                          //
  //-------------------------------------------------------------------------//
  template<bool IsNew, bool IsRemove>
  inline IO::FDInfo const& EPollReactor::CheckFD
  (
    char const* a_name,     // Can be NULL
    int         a_fd,
    char const* a_where     // Non-NULL
  )
  const
  {
    return (const_cast<EPollReactor*>(this))->CheckFD<IsNew, IsRemove>
           (a_name, a_fd, a_where);
  }

  // For the above method, we need an explicit instance (used in "GetFDInfo"
  // which is externally-visible):
  template
  IO::FDInfo const& EPollReactor::CheckFD<false, false>
  (
    char const* a_name,     // Can be NULL
    int         a_fd,
    char const* a_where     // Non-NULL
  )
  const;

  //=========================================================================//
  // "Clear":                                                                //
  //=========================================================================//
  inline void EPollReactor::Clear(IO::FDInfo* a_info)
  {
    assert(a_info != nullptr);

    //-----------------------------------------------------------------------//
    // Detach the FD from the Poll/EPoll mechanism:                          //
    //-----------------------------------------------------------------------//
    int fd = a_info->m_fd;
    if (fd >= 0)
    {
      if (m_usePoll)
      {
        // Poll: It is sufficient just to invalidate the FD:
        if (utxx::likely(fd < m_MaxFDs && m_pollFDs != nullptr))
        {
          pollfd* pfd  = m_pollFDs + fd;
          pfd->fd      = -1;
          pfd->events  =  0;
          pfd->revents =  0;
        }
      }
      else
      {
        // EPoll:
        assert(m_epollFD >= 0);
        CHECK_ONLY(int rc =) epoll_ctl(m_epollFD, EPOLL_CTL_DEL, fd, nullptr);
        CHECK_ONLY
        (
          if (utxx::unlikely(rc < 0))
            IO::SystemError(-1, "EPollReactor::Clear: Name=",
                            a_info->m_name.data());
        )
      }
      // This FD is no longer waiting for any events:
      a_info->m_waitEvents = 0;

      //---------------------------------------------------------------------//
      // Close the socket IF OWNED BY US:                                    //
      //---------------------------------------------------------------------//
      // This applies to all sockets except "RawInput" ones:
      //
      bool doClose = (a_info->m_htype != IO::FDInfo::HandlerT::RawInput);
      if  (doClose)
      {
        // XXX: If there is a TLS Session over this socket, then currently, it
        // is definitely owned by us: Terminate it, ignoring any errors:
        if (a_info->m_tlsSess != nullptr)
        {
          assert(a_info->m_tlsType == IO::TLSTypeT::GNUTLS);
          int res = gnutls_bye(a_info->m_tlsSess, GNUTLS_SHUT_RDWR);

          if (utxx::unlikely(res != GNUTLS_E_SUCCESS))
            { LOG_WARN(2, "EPollReactor::Clear(): FD={}: GNUTLS_Bye: {}",
                       fd, res) }
        }
        // Close the socket, ignoring any errors:
        (void) close(fd);
      }
      LOG_INFO(2,
        "EPollReactor::Clear(): FD={} Detached{}",
        fd, doClose ? " and Closed" : "")
    }
    //-----------------------------------------------------------------------//
    // Clear the "FDInfo" flds; the buffs are deleted iff not in Safe Mode:  //
    //-----------------------------------------------------------------------//
    // NB: This also de-initializes the TLS Session if exists:
    //
    a_info->Clear();

    //-----------------------------------------------------------------------//
    // Possibly reduce "m_topFD":                                            //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely(0 <= fd && fd < m_MaxFDs && fd == m_topFD))
    {
      // Scan "m_topFD" back until we get a valid "FDInfo":
      for (--m_topFD; m_topFD >= 0; --m_topFD)
        if (utxx::likely(m_fdis[m_topFD].m_fd == m_topFD))
          break;
      // NB: We can end up with m_topFD==-1, this is normal:
      assert(m_topFD >= -1);
    }
  }

  //=========================================================================//
  // "AddToEPoll":                                                           //
  //=========================================================================//
  inline void EPollReactor::AddToEPoll
  (
    IO::FDInfo* a_info,
    uint32_t    a_events,
    char const* a_where
  )
  {
    assert(a_info  != nullptr && a_where != nullptr);
    int fd = a_info->m_fd;
    assert(0 <= fd && fd < m_MaxFDs);

    // First of all, possibly increase "m_topFD" -- do it even before the FD is
    // added to the EPoll mechanism, because the corresp "FDInfo"  was  already
    // allocated by the Caller:
    m_topFD = max<int>(m_topFD, a_info->m_fd);
    try
    {
      if (m_usePoll)
      {
        // Poll: Just activate the corresp "pollfd", the "m_pollFDs" vector will
        // be passed to "poll" every time:
        pollfd* pfd  = m_pollFDs + fd;
        pfd->fd      = fd;
        pfd->events  = short(a_events);
        pfd->revents = 0;
      }
      else
      {
        // EPoll: Add it to the Master, really:
        assert(m_epollFD >= 0);

        // If the EPOLLET flag is requested, ALWAYS make the socket Non-Blocking
        // (in case if it is not such already)-- but this only works in the non-
        // LibVMA mode. In the LibVMA mode, the socket must be created Non-Block
        // from the beginning.
        // The reason for Non-Blocking mode in the presence of EPOLLET  is that,
        // in this case, the client will have to read all available data  to the
        // end, because the current readability event will be "spent" at once in
        // ET (Edge-Triggered) mode. But reading until the end would  block when
        // the end is reached, unless the Non-Blocking mode is set.
        // NB: Non-Blocking mode could also be useful for Level-Triggered events
        // but in that case it is not compulsory, and will be set by the Caller:
        //
        if (utxx::likely((a_events & EPOLLET) && !m_useVMA))
          (void) IO::SetBlocking<false>(fd);

        // NB: we must memoise "fd" in the "epoll_event", otherwise we will not
        // be able to know the corresp FDs when we get events via "epoll_wait"!
        epoll_event ev;
        ev.events   = a_events;
        ev.data.fd  = fd;

        CHECK_ONLY(int rc =) epoll_ctl(m_epollFD, EPOLL_CTL_ADD, fd, &ev);
        CHECK_ONLY
        (
          if (utxx::unlikely(rc < 0))
            IO::SystemError(-1, "EPollReactor::", a_where, ", Name=",
                            a_info->m_name.data());
        )
      }
      // Memoise the events this FD is waiting for:
      a_info->m_waitEvents = a_events;
    }
    catch (...)
    {
      // IMPORTANT: This method is invoked AFTER the corresp "FDInfo" has been
      // set, so in case of any errors here, we must clean up that "FDInfo":
      Clear(a_info);
      throw;
    }
    // If we got here: "FDInfo" added successfully:
    LOG_INFO(2,
      "EPollReactor::AddToEPoll({}): Added FD={}, Name={}, HType={}",
      a_where, fd, a_info->m_name.data(), a_info->m_htype.to_string())
  }

  //=========================================================================//
  // "AddDataStream":                                                        //
  //=========================================================================//
  // Creates a TCP socket, attaches it to the Raector and returns the SocketFD:
  //
  int EPollReactor::AddDataStream
  (
    char const*                       a_name,                // May be NULL
    int                               a_fd,                  // (-1) for Client
    IO::FDInfo::ReadHandler    const& a_on_read,
    IO::FDInfo::ConnectHandler const& a_on_connect,          // May be empty
    IO::FDInfo::ErrHandler     const& a_on_error,
    int                               a_rd_bufsz,
    int                               a_rd_lwm,
    int                               a_wr_bufsz,
    int                               a_wr_lwm,
    char const*                       a_ip,
    bool                              a_use_tls,
    char const*                       a_server_name,         // NULL if no TLS
    IO::TLS_ALPN                      a_tls_alpn,
    bool                              a_verify_server_cert,
    char const*                       a_server_ca_files,     // May be empty
    char const*                       a_client_cert_file,    // May be NULL
    char const*                       a_client_priv_key_file // ditto
  )
  {
    //-----------------------------------------------------------------------//
    // Checks:                                                               //
    //-----------------------------------------------------------------------//
    CHECK_ONLY
    (
      if (utxx::unlikely
        (a_rd_bufsz < 0 || a_rd_lwm < 0 || a_wr_bufsz < 0 || a_wr_lwm < 0))
        throw utxx::badarg_error
              ("EPollReactor::AddDataStream: Negative BuffSz or LowWaterMark");

      // Either Read or Write buffer may be absent, but obviously not both:
      if (utxx::unlikely(a_rd_bufsz == 0 && a_wr_bufsz == 0))
        throw utxx::badarg_error
              ("EPollReactor::AddDataStream: Read and Write Buffers must not "
               "both be 0-size");

      // Iff the ReadHandler is non-empty, the corersp buffer size must also be
      // valid -- but again, for DGram sockets it is statically-provided, so no
      // check in that case ("a_rd_bufsz" is ignored for DGram):
      if (utxx::unlikely
         ((a_on_read && a_rd_bufsz <= 0) || (!a_on_read && a_rd_bufsz > 0)))
        throw utxx::badarg_error("EPollReactor::AddDataStream: "
                                 "Inconsistent ReadHandler/ReadBuffSz");
    )
    //-----------------------------------------------------------------------//
    // Create a TCP Socket (UNLESS it is given):                             //
    //-----------------------------------------------------------------------//
    // NB: "a_fd" is given iff we are on the Server-side, so "a_fd" has come
    // from the Acceptor. The new socket is non-blocking from the beginning:
    //
    bool isServer = (a_fd >= 0);
    bool isClient = !isServer;
    int  fd       = -1;
    if (isServer)
    {
      fd = a_fd;
      (void) IO::SetBlocking<false>(fd);
    }
    else
    {
      fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);

      if (utxx::unlikely(fd < 0))
        IO::SystemError
          (-1, "EPollReactor::AddDataStream: Cannot create the TCP socket");
    }
    assert(fd >= 0);

    //-----------------------------------------------------------------------//
    // VERY IMPORTANT: Disable the Nagle algorithm on this socket:           //
    //-----------------------------------------------------------------------//
    // If necessary, we do all the buffering by ourselves in the user space. NB:
    // This CANNOT be done at the OS level via sysctl:
    int one = 1;
    if (utxx::unlikely
       (setsockopt(fd, SOL_TCP, TCP_NODELAY, &one, sizeof(one)) < 0))
      // Close the socket and propagate exception to the Caller:
      IO::SocketError<true>
        (-1, fd, "EPollReactor::AddDataStream: Could not set TCP_NODELAY");

    //-----------------------------------------------------------------------//
    // Also set the Keep-Alive mode on the socket, with 1-second intervals:  //
    //-----------------------------------------------------------------------//
    // 5 missed acks will indicated a disconnect. This is required in order to
    // detect stale connections as early as possible: HeartBeats could also be
    // used to that end, but with much longer latency (eg minutes).
    // XXX: For this reason, we currently ignore Server HeartBeats; connection
    // failure is detected much sooner via the Keep-Alive mode; however, we may
    // still need to send our HeartBeats to the Server, because it pays attent-
    // ion to them:
    if (utxx::unlikely
       (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE,  &one, sizeof(one)) < 0))
      // Again, close the socket and propagate exception to the Caller:
      IO::SocketError<true>
        (-1,  fd,
         "EPollReactor::AddDataStream: Could not enable TCP_KEEPALIVE");

    // The following detailed config is only possible if LibVMA is NOT used
    // (because LibVMA currently does not support such "setsockopt"s):
    if (!m_useVMA)
    {
      int cnt = 5;
      if (utxx::unlikely
         (setsockopt(fd, SOL_TCP, TCP_KEEPIDLE,  &one, sizeof(one)) < 0 ||
          setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, &one, sizeof(one)) < 0 ||
          setsockopt(fd, SOL_TCP, TCP_KEEPCNT,   &cnt, sizeof(cnt)) < 0 ))
        IO::SocketError<true>
          (-1,  fd,
           "EPollReactor::AddDataStream: Could not configure TCP_KEEPALIVE");
    }
    //-----------------------------------------------------------------------//
    // Now get the "FDInfo" (IsNew = true):                                  //
    //-----------------------------------------------------------------------//
    IO::FDInfo& info = CheckFD<true>(a_name, fd, "AddDataStream");

    // Check the Socket Domain and Type -- must be a Stream Socket (and it is
    // NOT an Acceptor): The check only works in the non-LibVMA mode:
    if (!m_useVMA)
    {
      info.SetSocketProps<false>(a_name, fd, "AddDataStream");
      CHECK_ONLY
      (
        if (utxx::unlikely(info.m_type != SOCK_STREAM))
        {
          Clear(&info);
          throw utxx::badarg_error
                ("EPollReactor::AddDataStream: Must be a Stream Socket: FD=",
                 fd, ", Name=", (a_name != nullptr) ? a_name : "NULL");
        }
      )
    }
    else
    // LibVMA: Assume the following Domain and Type (because it's a Stream):
    {
      info.m_domain = AF_INET;
      info.m_type   = SOCK_STREAM;
    }
    //-----------------------------------------------------------------------//
    // Fill in the rest of "FDInfo":                                         //
    //-----------------------------------------------------------------------//
    // NB: already set:
    //   info.m_domain
    //   info.m_type
    //
    InitFixedStr(&info.m_name, (a_name != nullptr) ? a_name : "");
    info.m_fd         = fd;
    info.m_htype      = IO::FDInfo::HandlerT::DataStream;
    info.m_rh         = a_on_read;
    info.m_ch         = a_on_connect;  // Empty is OK
    info.m_eh         = a_on_error;
    info.m_inst_id    = ++m_currInstID;
    info.m_cInfo      = nullptr;       // TODO!

    // Create and attach the Read and Write Buffers:
    assert(info.m_rd_buff == nullptr && info.m_wr_buff == nullptr);

    if (a_rd_bufsz > 0)
    {
      assert(a_on_read);
      info.m_rd_buff = new utxx::dynamic_io_buffer(size_t(a_rd_lwm));
      info.m_rd_buff->reserve(size_t(a_rd_bufsz));
    }
    if (a_wr_bufsz > 0)
    {
      info.m_wr_buff = new utxx::dynamic_io_buffer(size_t(a_wr_lwm));
      info.m_wr_buff->reserve(size_t(a_wr_bufsz));
    }
    //-----------------------------------------------------------------------//
    // Bind to the local IP:                                                 //
    //-----------------------------------------------------------------------//
    // Do that even if the IP is empty; the Port is randomly selected; we will
    // get the resulting Local IP and Port in "info" anyway:
    //
    info.Bind(a_ip, 0);

    //-----------------------------------------------------------------------//
    // If TLS is used: Create a TLS Session:                                 //
    //-----------------------------------------------------------------------//
    // This sets:
    //   info.m_tlsType
    //   info.m_tlsSess
    //
    bool hasClientCert = false;
    if (a_use_tls)
    {
      // Initially (until the HandShake completes), TLS Type is GNUTLS:
      info.m_tlsType = IO::TLSTypeT::GNUTLS;

      // If not done yet, install the Reactor-wide Certificates:
      if (utxx::unlikely(m_tlsCreds == nullptr))
      {
        // Allocate space for Credentials:
        CheckGNUTLSError
          (gnutls_certificate_allocate_credentials(&m_tlsCreds), info,
           "EPollReactor::AddDataStream: Cannot allocate TLS credentials");
        assert(m_tlsCreds != nullptr);

        // Load the Certificates of system-wide trusted CAs:
        CheckGNUTLSError
          (gnutls_certificate_set_x509_system_trust(m_tlsCreds), info,
           "EPollReactor::AddDataStream: Cannot load System-wide CAs");

        // If additional trusted CA file(s) are provided (for verifying the
        // server), load them as well:
        // Split "a_server_ca_files" at ','s. XXX: We temporarily modify the
        // string but then restore it back (as it is managed by the CallER):
        char const* caFile = a_server_ca_files;
        while (caFile != nullptr && *caFile != '\0')
        {
          char* to = const_cast<char*>(strchr(caFile, ','));
          if (to != nullptr)
            *to = '\0';

          // "caFile" now points to a 0-terminated string containing (indeed)
          // the Server CA File name: Load the file:
          CheckGNUTLSError
            (gnutls_certificate_set_x509_trust_file
              (m_tlsCreds, caFile, GNUTLS_X509_FMT_PEM),
              info, "EPollReactor::AddDataStream: Cannot load Server CA: ",
              caFile);

          // Continue to parse the list:
          if (to != nullptr)
            { *to = ','; caFile = to + 1; }
          else
            break;
        }
        // If TLS is enabled, we may also have a Client Certificate which needs
        // to be added to our Ctredentials. Otherwise, the CertFile and PrivKey
        // File are ignored:
        hasClientCert =
          a_client_cert_file     != nullptr &&
         *a_client_cert_file     != '\0'    &&
          a_client_priv_key_file != nullptr &&
         *a_client_priv_key_file != '\0';

        if (hasClientCert)
          CheckGNUTLSError
            (gnutls_certificate_set_x509_key_file
              (m_tlsCreds, a_client_cert_file, a_client_priv_key_file,
               GNUTLS_X509_FMT_PEM),
             info,
             "EPollReactor::AddDataStream: Cannot set Client Credentials: ",
             a_client_cert_file);
      }
      // Initialize the TLS Session. If there was a session already, close it
      // first:
      if (utxx::unlikely(info.m_tlsSess != nullptr))
        gnutls_deinit(info.m_tlsSess);

      // XXX: Do not use any "exotic" options (FalseStart etc):
      unsigned tlsFlags  = isClient ? GNUTLS_CLIENT : GNUTLS_SERVER;
      tlsFlags          |= (GNUTLS_NONBLOCK | GNUTLS_NO_SIGNAL);
      if (isClient && hasClientCert)
        tlsFlags |= GNUTLS_FORCE_CLIENT_CERT;

      CheckGNUTLSError
        (gnutls_init(&info.m_tlsSess, tlsFlags),
         info, "EPollReactor::AddDataStream: Cannot initialize TLS session");

      if (isClient)
      {
        // Set the server name we are going to connect to. XXX: We do not check
        // the consistency of ServerName and ServerIP later used in actual Conn:
        if (utxx::unlikely
           (a_server_name == nullptr || *a_server_name == '\0'))
          throw utxx::badarg_error("MkStreamSock: Missing TLS server name");

        CheckGNUTLSError
          (gnutls_server_name_set
            (info.m_tlsSess, GNUTLS_NAME_DNS, a_server_name,
             strlen(a_server_name)),
             info, "EPollReactor::AddDataStream: Cannot set TLS server name: ",
           a_server_name);
      }
      // Set the default TLS Priorities (on Protocols, Versions, Ciphers etc):
      CheckGNUTLSError
        (gnutls_set_default_priority(info.m_tlsSess), info,
         "EPollReactor::AddDataStream: Cannot set TLS priorities");

      // Install the CAs' Credentials in the current Session:
      CheckGNUTLSError
        (gnutls_credentials_set
        (info.m_tlsSess,  GNUTLS_CRD_CERTIFICATE, m_tlsCreds),  info,
         "EPollReactor::AddDataStream: Cannot install TLS cerificates in "
         "session");

      // Request server certificate verification during TLS hand-shake (in some
      // cases this is NOT done):
      if (isClient && a_verify_server_cert)
        gnutls_session_set_verify_cert(info.m_tlsSess, a_server_name, 0);

      // If the Application-Level Protocol is specified, install it in the Sess:
      char const* protoStr =
        (a_tls_alpn == IO::TLS_ALPN::HTTP11)
        ? "http/1.1" :
        (a_tls_alpn == IO::TLS_ALPN::HTTP2)
        ? "h2"
        : nullptr;

      if (protoStr != nullptr)
      {
        gnutls_datum_t proto;
        proto.data =
          reinterpret_cast<unsigned char*>(const_cast<char*>(protoStr));
        proto.size = unsigned(strlen(protoStr));

        CheckGNUTLSError
          (gnutls_alpn_set_protocols
            (info.m_tlsSess, &proto, 1, GNUTLS_ALPN_MANDATORY),
           info, "EPollReactor::AddDataStream: Cannot set the ALPN params");
      }
      // TLS session is now ready for TCP connection...
    }
    else
    {
      // No TLS:
      info.m_tlsType = IO::TLSTypeT::None;
      info.m_tlsSess = nullptr;
    }

    //-----------------------------------------------------------------------//
    // Finally, attach the socket to the Poll/EPoll mechanism:               //
    //-----------------------------------------------------------------------//
    // Create the Events Mask and attach FD to the Poll/EPoll mechanism. NB: No
    // matter if the socket is intended to be read-only or write-only   (though
    // both cases are highly unlikely: stream sockets are normally used in dup-
    // lex mode), we initially set both IN and OUT events,   as they are needed
    // for "connect":
    if (m_usePoll)
    {
      // Poll:
      uint32_t events = POLLPRI | POLLIN  | POLLOUT  | POLLRDHUP;
      AddToEPoll(&info, events, "AddDataStream(Poll)");
    }
    else
    {
      // EPoll:
      uint32_t events = EPOLLET | EPOLLIN | EPOLLOUT | EPOLLRDHUP;
      AddToEPoll(&info, events, "AddDataStream(EPoll)");
    }
    // All Done:
    return fd;
  }

  //=========================================================================//
  // "AddDataGram":                                                          //
  //=========================================================================//
  // Creates a UNIX Domain Socket or a UDP Socket, connects it to the Remote (if
  // specified) and adds it to the Reactor. The Socket FD is returned:
  //
  int EPollReactor::AddDataGram
  (
    char const*                    a_name,        // Can be empty
    char const*                    a_local_addr,  // Can smtms be empty as well
    int                            a_local_port,  // Can be invalid (< 0)
    IO::FDInfo::RecvHandler const& a_on_recv,
    IO::FDInfo::ErrHandler  const& a_on_error,
    int                            a_wr_bufsz,
    int                            a_wr_lwm,
    char const*                    a_remote_addr, // To "connect" to:
    int                            a_remote_port  //   may be empty/invalid
  )
  {
    //-----------------------------------------------------------------------//
    // Checks:                                                               //
    //-----------------------------------------------------------------------//
    CHECK_ONLY
    (
      if (utxx::unlikely(a_wr_bufsz < 0 || a_wr_lwm < 0))
        throw utxx::badarg_error
              ("EPollReactor::AddDataGram: Negative BuffSz or LowWaterMark");
    )
    // Create an INET or UNIX DGram Socket (and possibly connect it to the re-
    // mote if specified):
    auto res    = IO::MkDGramSock
                  (a_local_addr, a_local_port, a_remote_addr, a_remote_port);
    int  fd     = res.first;
    int  domain = res.second;

    // Get the FDInfo (IsNew=true):
    IO::FDInfo& info = CheckFD<true>(a_name, fd, "AddDataGram");

    // Check the Socket Domain and Type (it is NOT an Acceptor); XXX: the check
    // is only available in the non-LibVMA mode:
    if (!m_useVMA)
    {
      info.SetSocketProps<false>(a_name, fd, "AddDataGram");
      CHECK_ONLY
      (
        if (utxx::unlikely
           (info.m_type != SOCK_DGRAM || info.m_domain != domain))
        {
          Clear(&info);
          throw utxx::badarg_error
                ("EPollReactor::AddDataGram: Must be a DGram ",
                (domain == AF_UNIX ? "UNIX" : "INET"),   " Socket: FD=", fd,
                 ", Name=", (a_name != nullptr) ? a_name : "NULL");
        }
      )
    }
    else
    // LibVMA: Assign the Domain and Type as they should be:
    {
      info.m_domain = domain;
      info.m_type   = SOCK_DGRAM;
    }
    //-----------------------------------------------------------------------//
    // Fill in the main part of "FDInfo":                                    //
    //-----------------------------------------------------------------------//
    InitFixedStr(&info.m_name, (a_name != nullptr) ? a_name : "");
    info.m_fd      = fd;
    info.m_htype   = IO::FDInfo::HandlerT::DataGram;
    info.m_rch     = a_on_recv;     // May be empty, but seldom
    info.m_eh      = a_on_error;
    info.m_inst_id = ++m_currInstID;

    // Create and attach the Read and Write Buffers:
    // NB: The Read Buffer is not required for DGram sockets -- in that case,
    // the Reactor-wide static buffer is used:
    //
    assert(info.m_rd_buff == nullptr && info.m_wr_buff == nullptr);
    if (a_wr_bufsz > 0)
    {
      info.m_wr_buff = new utxx::dynamic_io_buffer(size_t(a_wr_lwm));
      info.m_wr_buff->reserve(size_t(a_wr_bufsz));
    }
    //-----------------------------------------------------------------------//
    // Bind this socket to the Local{Addr/Port} (or any random one):         //
    //-----------------------------------------------------------------------//
    info.Bind(a_local_addr, a_local_port);

    //-----------------------------------------------------------------------//
    // Finally, attach the socket to the EPoll mechanism:                    //
    //-----------------------------------------------------------------------//
    // Create the Events Mask and attach FD to the Poll/EPoll mechanism.  NB:
    // since DGram sockets are connection-less, we specify IN/OUT events only
    // if we really intend to perform the corresp operations  (unlike Stream
    // sockets, where both IN and OUT are required at least during "connect"):
    if (m_usePoll)
    {
      // Poll:
      uint32_t events = 0;
      if (a_on_recv)            // Will be used as IN
        events |= POLLIN;
      if (a_wr_bufsz > 0)       // Will be used as OUT
        events |= POLLOUT;

      AddToEPoll(&info, events, "AddDataGram(Poll)");
    }
    else
    {
      // EPoll:
      uint32_t events = EPOLLET;
      if (a_on_recv)            // Will be used as IN
        events |= EPOLLIN;
      if (a_wr_bufsz > 0)       // Will be used as OUT
        events |= EPOLLOUT;

      AddToEPoll(&info, events, "AddDataGram(EPoll)");
    }
    // All Done:
    return fd;
  }

  //=========================================================================//
  // "AddRawInput":                                                          //
  //=========================================================================//
  // XXX: This is for rare cases like FeedOS (QuantHouse) sockets: Their Reada-
  // bility / Writability conditiobs are determined by EPoll, but the actual IO
  // operations may (or may not) be performed by external libs. Another possib-
  // ility is that an external lib  (eg RDKafka)  uses an FD for signalling  of
  // data availability, but the data are actually delivered in a different way,
  // not via this FD. Unlike other cases, here we attach an existing (NON-OWNED)
  // FD, rather than create and return a new one:
  //
  void EPollReactor::AddRawInput
  (
    char const*                         a_name,               // Can be NULL
    int                                 a_fd,
    bool                                a_is_edge_triggered,  // EPoll only
    IO::FDInfo::RawInputHandler const&  a_on_rawin,
    IO::FDInfo::ErrHandler      const&  a_on_error
  )
  {
    // Get FDInfo and Socket Properties, but do not really check the latter --
    // there are virtually no restrictions here:
    IO::FDInfo& info = CheckFD<true>(a_name, a_fd, "AddRawInput");
    assert(a_fd >= 0);

    // XXX: In the LibVMA mode, we cannot determine the socket Domain and Type,
    // so will have to ASSUME the following (should almost always be OK):
    if (!m_useVMA)
      info.SetSocketProps<false>(a_name, a_fd, "AddRawInput"); // Not Acceptor
    else
    if (info.m_isSocket)
    {
      info.m_domain = AF_INET;
      info.m_type   = SOCK_STREAM;
    }

    // Fill in the main part of "FDInfo". NB: The Rd- and WrBuffs, if required,
    // should be provided by the Client:
    InitFixedStr(&info.m_name, (a_name != nullptr) ? a_name : "");
    info.m_fd       = a_fd;
    info.m_htype    = IO::FDInfo::HandlerT::RawInput;
    info.m_rih      = a_on_rawin;       // May be empty, but seldom!
    info.m_eh       = a_on_error;
    info.m_inst_id  = ++m_currInstID;

    // Finally, attach the socket to the Poll/EPoll mechanism.  NB:  The socket
    // may be Level-Triggered, or Edge-Triggered:
    // XXX: Currently, the Reactor only handles Readability and Error conds  on
    // this socket; if the Client wants to *write* into it, it would need to ma-
    // nage that completely on its own:
    //
    uint32_t events = 0;
    if (m_usePoll)
    {
      CHECK_ONLY
      (
        if (utxx::unlikely(a_is_edge_triggered))
          throw utxx::badarg_error
                ("EPollReactor::AddRawInput: EdgeTriggered not available for "
                 "Poll: FD=", a_fd);
      )
      events = POLLPRI | POLLIN | POLLRDHUP;
    }
    else
    {
      events = EPOLLIN | EPOLLRDHUP;
      if (a_is_edge_triggered)
        events |= EPOLLET;
    }
    AddToEPoll(&info, events, "AddRawInput");
  }

  //=========================================================================//
  // "AddTimer":                                                             //
  //=========================================================================//
  // Returns the TimerFD created:
  //
  int EPollReactor::AddTimer
  (
    char const*                   a_name,           // Can be NULL
    uint32_t                      a_initial_msec,
    uint32_t                      a_period_msec,
    IO::FDInfo::TimerHandler const& a_on_timer,
    IO::FDInfo::ErrHandler   const& a_on_error
  )
  {
    CHECK_ONLY
    (
      // The "TimerHandler" must be non-empty:
      if (utxx::unlikely(!a_on_timer))
        throw utxx::badarg_error
              ("EPollReactor::AddTimer: TimerHandler must be non-empty: Name=",
              (a_name != nullptr) ? a_name : "NULL");
    )
    // Create a TimerFD:
    int fd  = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK | TFD_CLOEXEC);

    CHECK_ONLY
    (
      if (utxx::unlikely(fd < 0))
        IO::SystemError(-1, "EPollReactor::AddTimer: timerfd_create() Failed: "
                        "Name=", (a_name != nullptr) ? a_name : "NULL");
    )
    // Set the "itermerspec" flds:
    time_t initial_sec  =  a_initial_msec / 1000;
    time_t initial_nsec = (a_initial_msec % 1000) * 1000000;

    time_t period_sec   =  a_period_msec  / 1000;
    time_t period_nsec  = (a_period_msec  % 1000) * 1000000;

    struct itimerspec timeout;
    timeout.it_value.tv_sec     = utxx::now_utc().sec() + initial_sec;
    timeout.it_value.tv_nsec    = initial_nsec;
    timeout.it_interval.tv_sec  = period_sec;
    timeout.it_interval.tv_nsec = period_nsec;

    // Set the TimerFD:
    CHECK_ONLY(int rc =)
      timerfd_settime(fd, TFD_TIMER_ABSTIME, &timeout, nullptr);

    CHECK_ONLY
    (
      if (utxx::unlikely(rc < 0))
      {
        // Close the "fd", it is safe to do (it is not attached to the Reactor
        // yet, and has not been propagated anywhere):
        (void) close(fd);
        IO::SystemError(-1, "EPollReactor::AddTimer: timerfd_settime() Failed: "
                        "Name=", (a_name != nullptr) ? a_name : "NULL");
      }
    )
    // Set the "FDInf"o. It must contain a ReadBuffer of a certain size:
    IO::FDInfo& info = CheckFD<true>(a_name, fd, "AddTimer");

    InitFixedStr(&info.m_name, (a_name != nullptr) ? a_name : "");
    info.m_fd      = fd;
    info.m_htype   = IO::FDInfo::HandlerT::Timer;
    info.m_th      = a_on_timer;
    info.m_eh      = a_on_error;
    info.m_inst_id = ++m_currInstID;

    assert(info.m_rd_buff == nullptr && info.m_wr_buff == nullptr);
    info.m_rd_buff  = new utxx::dynamic_io_buffer(8 * 32);

    // Now actually attach it to the Poll/EPoll mechanism. We are only interes-
    // ted in Read (and Error -- awailable by default) events:
    uint32_t events = m_usePoll ? (POLLPRI | POLLIN) : (EPOLLET | EPOLLIN);

    AddToEPoll(&info, events, "AddTimer");
    return fd;
  }

  //=========================================================================//
  // "ChangeTimer":                                                          //
  //=========================================================================//
  // Allows us to change the Initial and/or Period of an active Firing Timer:
  //
  void EPollReactor::ChangeTimer
  (
    int                 a_timer_fd,
    uint32_t            a_initial_msec,
    uint32_t            a_period_msec
  )
  {
#  if !UNCHECKED_MODE
    // Check that this is indeed a TimerFD:
    constexpr bool IsNew    = false;
    constexpr bool IsRemove = false;

    IO::FDInfo& info =
      CheckFD<IsNew, IsRemove>(nullptr, a_timer_fd, "ChangeTimer");

    if (utxx::unlikely(info.m_type != IO::FDInfo::HandlerT::Timer))
      throw utxx::badarg_error
            ("EPollReactor::ChangeTimer: ", a_timer_fd, ": Not a TimerFD");
#   endif
    // Set the "itermerspec" flds:
    time_t initial_sec  =  a_initial_msec / 1000;
    time_t initial_nsec = (a_initial_msec % 1000) * 1000000;

    time_t period_sec   =  a_period_msec  / 1000;
    time_t period_nsec  = (a_period_msec  % 1000) * 1000000;

    auto now = utxx::now_utc();
    itimerspec timeout;
    timeout.it_value.tv_sec =
      now.sec() + initial_sec   + (now.nsec() + initial_nsec) / 1'000'000'000;
    timeout.it_value.tv_nsec    = (now.nsec() + initial_nsec) % 1'000'000;
    timeout.it_interval.tv_sec  = period_sec;
    timeout.it_interval.tv_nsec = period_nsec;

    // Set the TimerFD:
    CHECK_ONLY(int rc =)
    timerfd_settime(a_timer_fd, TFD_TIMER_ABSTIME, &timeout, nullptr);

    CHECK_ONLY
    (
      if (utxx::unlikely(rc < 0))
      {
        // Close the "fd", it is safe to do (it is not attached to the Reactor
        // yet, and has not been propagated anywhere):
        (void) close(a_timer_fd);
        IO::SystemError
        (
          rc, "EPollReactor::ChangeTimer: timerfd_settime() Failed: Name=",
          info.m_name.data()
        );
      }
    )
  }

  //=========================================================================//
  // "AddSignals":                                                           //
  //=========================================================================//
  int EPollReactor::AddSignals
  (
    char const*                     a_name,          // Can be NULL
    vector<int> const&              a_sig_nums,
    IO::FDInfo::SigHandler  const&  a_on_signal,
    IO::FDInfo::ErrHandler  const&  a_on_error
  )
  {
    // If LibVMA is in use, this method will not work:
    if (m_useVMA)
    {
      LOG_INFO(1, "EPollReactor::AddSignals: Incompatible with LibVMA")
      return -1;
    }
    CHECK_ONLY
    (
      // The "SigHandler" must be non-empty, but "ErrHandler" may be empty:
      if (utxx::unlikely(!a_on_signal))
        throw utxx::badarg_error
              ("EPollReactor::AddSignals: SignalHandler must be non-empty: "
               "Name=", (a_name != nullptr) ? a_name : "NULL");
    )
    // Fill in the SigSet:
    sigset_t     sigSet;
    sigemptyset(&sigSet);

    for (int sigNum: a_sig_nums)
      sigaddset(&sigSet, sigNum);

    // Create a new SignalFD -- one for the whole SigSet above:
    int fd = signalfd(-1, &sigSet, SFD_NONBLOCK | SFD_CLOEXEC);

    CHECK_ONLY
    (
      if (utxx::unlikely(fd < 0))
        IO::SystemError(-1, "EPollReactor::AddSignals: signalfd() Failed");

      // Also check that the specified signals are already blocked,  otherwise
      // unexpected behavior may occur in a multi-threaded env (eg with Logger
      // threads):
      sigset_t     blockedSet;
      sigemptyset(&blockedSet);

      int rc = sigprocmask(SIG_UNBLOCK, nullptr, &blockedSet);
      if (utxx::unlikely(rc < 0))
        IO::SystemError(rc, "EPollReactor::AddSignals: sigprocmask() Failed");

      // All signals in "a_sig_nums" must already be blocked:
      for (int sigNum: a_sig_nums)
        if (utxx::unlikely(!sigismember(&blockedSet, sigNum)))
          throw utxx::badarg_error
                ("EPollReactor::AddSignals: SigNum=", sigNum,
                 " needs to be Blocked first via IO::GlobalInit");
    )
    // Set the FDInfo for this SignalFD:
    IO::FDInfo& info = CheckFD<true>(a_name, fd, "AddSignal");

    InitFixedStr(&info.m_name,  (a_name != nullptr) ? a_name : "");
    info.m_fd      = fd;
    info.m_htype   = IO::FDInfo::HandlerT::Signal;
    info.m_sh      = a_on_signal;
    info.m_eh      = a_on_error;
    info.m_inst_id = ++m_currInstID;

    assert(info.m_rd_buff == nullptr && info.m_wr_buff == nullptr);
    info.m_rd_buff  = new utxx::dynamic_io_buffer
                          (16 * sizeof(signalfd_siginfo));

    // Now actually attach it to the Poll/EPoll mechanism. We are only interes-
    // ted in Read (and Error -- awailable by default) events:
    uint32_t events = m_usePoll ? (POLLPRI | POLLIN) : (EPOLLET | EPOLLIN);

    AddToEPoll(&info, events, "AddSignal");
    LOG_INFO(2, "Signals={}", SigMembersToStr(sigSet))
    return fd;
  }

  //=========================================================================//
  // "AddAcceptor":                                                          //
  //=========================================================================//
  void EPollReactor::AddAcceptor
  (
    char const*                      a_name,    // Can be NULL
    int                              a_fd,      // Acceptor Socket
    IO::FDInfo::AcceptHandler const& a_on_accept,
    IO::FDInfo::ErrHandler    const& a_on_error
  )
  {
    CHECK_ONLY
    (
      // The "AcceptHandler" must be non-empty:
      if (utxx::unlikely(!a_on_accept))
        throw utxx::badarg_error
              ("EPollReactor::AddAcceptor: AcceptHandler must be non-empty: "
               "Name=", (a_name != nullptr) ? a_name : "NULL");
    )
    // Check that "a_fd" is in the valid range:
    IO::FDInfo& info = CheckFD<true>(a_name, a_fd, "AddAcceptor");

    // Check the Socket Domain and Type (IsAcceptor = true), but only do so in
    // the non-LibVMA mode:
    if (!m_useVMA)
      info.SetSocketProps<true> (a_name, a_fd, "AddAcceptor");
    else
    {
      info.m_domain = AF_INET;
      info.m_type   = SOCK_STREAM;
    }

    // Set the "FDInfo". We will detect Readability condition on this FD, but
    // not actually read anything, so no buffers are required:
    InitFixedStr(&info.m_name, (a_name != nullptr) ? a_name : "");
    info.m_fd      = a_fd;
    info.m_htype   = IO::FDInfo::HandlerT::Acceptor;
    info.m_ah      = a_on_accept;
    info.m_eh      = a_on_error;
    info.m_inst_id = ++m_currInstID;
    assert(info.m_rd_buff == nullptr && info.m_wr_buff == nullptr);

    // Now actually attach it to the Poll/EPoll mechanism:
    uint32_t events = m_usePoll ? (POLLPRI | POLLIN) : (EPOLLET | EPOLLIN);
    AddToEPoll(&info, events,  "AddAcceptor");
  }

  //=========================================================================//
  // "AddGeneric":                                                           //
  //=========================================================================//
  void EPollReactor::AddGeneric
  (
    char const*                       a_name,   // Must NOT be NULL or empty
    IO::FDInfo::GenericHandler const& a_gh
  )
  {
    CHECK_ONLY
    (
      // Check the args. In particular, a non-trivial Name must be provided,
      // because it will be used later in "RemoveGeneric":
      if (utxx::unlikely(a_name == nullptr || *a_name == '\0'))
        throw utxx::badarg_error
              ("EPollReactor::AddGeneric: Name must be non-empty");

      // The Name must also be unique:
      if (utxx::unlikely
         (boost::container::find_if
           (m_ghs.cbegin(), m_ghs.cend(),
            [a_name]
            (pair<ObjName,  IO::FDInfo::GenericHandler> const& a_curr)->bool
            { return (strcmp(a_name, a_curr.first.data()) == 0); })
         != m_ghs.cend()))
        throw utxx::badarg_error
              ("EPollReactor::AddGeneric: Name already exists: ", a_name);

      // The "GenericHandler" itself must be non-empty:
      if (utxx::unlikely(!a_gh))
        throw utxx::badarg_error
              ("EPollReactor::AddGeneric: GenericHandler must be non-empty: "
               "Name=", (a_name != nullptr) ? a_name : "NULL");

      // We only allow a limited number of "GenericHandler"s:
      assert(m_ghs.size() <= MaxGHs);
      if (utxx::unlikely(m_ghs.size() == MaxGHs))
        throw utxx::badarg_error
              ("EPollReactor::AddGeneric: Too Many GenericHandlers");
    )
    // If all is OK:
    m_ghs.push_back(make_pair(MkObjName(a_name), a_gh));
  }

  //=========================================================================//
  // "Remove":                                                               //
  //=========================================================================//
  // This is just a wrapper wround Clear(&FDInfo), which is in turn a wrapper
  // around FDInfo::Clear():
  //
  void EPollReactor::Remove(int a_fd)
  {
    // Check the FD -- but obvsiously, we do not require that the corresp slot
    // is empty (though it may be -- repeated "Remove" is OK, as well as remov-
    // ing a negative or too large FD which cannot be in "m_fdis"):
    //
    if (utxx::unlikely(a_fd < 0 || a_fd >= m_MaxFDs))
      return;

    // IsNew=false, IsRemove=true:
    IO::FDInfo& info = CheckFD<false, true>(nullptr, a_fd, "Remove");

    // We got the slot, but it might be empty, actually:
    if (utxx::unlikely(info.m_fd < 0 || info.m_inst_id == 0))
      return;                                             // Repreated Remove

    // Otherwise: Beware of repeated "Remove"s, they are to be handled here,
    // not by "CheckFD" above:
    if (utxx::unlikely(info.m_fd < 0 || a_fd > m_topFD))
    {
      LOG_INFO(1,
        "EPollReactor::Remove: FD={}, OldFD={}, TopFD={}: Repeated Remove? "
        "Ignored...", a_fd, info.m_fd, m_topFD)
      return;
    }
    // Otherwise, the following invariant must hold (verified by "CheckFD"):
    assert(0 <= a_fd && a_fd <= m_topFD && a_fd == info.m_fd);

    //-----------------------------------------------------------------------//
    // Generic Case:                                                         //
    //-----------------------------------------------------------------------//
    // Clear the FDInfo". This also removes the TLS Session (if exists), deta-
    // ches the socket from the EPoll mechanism, and closes it (if it is owned
    // by us):
    Clear(&info);
    assert(info.m_fd == -1 && info.m_inst_id == 0); // Other flds not checked...

    LOG_INFO(2, "EPollReactor::Remove: Removed FD={}", a_fd)
  }

  //=========================================================================//
  // "RemoveGeneric":                                                        //
  //=========================================================================//
  bool EPollReactor::RemoveGeneric(char const* a_name)
  {
    CHECK_ONLY
    (
      if (utxx::unlikely(a_name == nullptr || *a_name == '\0'))
        throw utxx::badarg_error
              ("EPollReactor::RemoveGeneric: Name must not be empty");
    )
    // Search for an entry with this name (if it exists, we know by "AddGeneric"
    // that it is unique):
    auto it =
      boost::container::find_if
      (
        m_ghs.begin(), m_ghs.end(),
        [a_name] (pair<ObjName, IO::FDInfo::GenericHandler> const& a_curr)->bool
        { return (strcmp(a_name, a_curr.first.data()) == 0); }
      );

    if (utxx::unlikely(it == m_ghs.end()))
      return false;    // Not found, so not removed

    // Generic case: Perform the removal:
    m_ghs.erase(it);
    return true;
  }

  //=========================================================================//
  // Events Testing:                                                         //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "IsError":                                                              //
  //-------------------------------------------------------------------------//
  inline bool EPollReactor::IsError(unsigned a_ev) const
  {
    return
      a_ev &
        (m_usePoll
         ? unsigned(POLLERR  | POLLHUP  | POLLRDHUP | POLLNVAL)
         : unsigned(EPOLLERR | EPOLLHUP | EPOLLRDHUP));
  }

  //-------------------------------------------------------------------------//
  // "IsReadable":                                                           //
  //-------------------------------------------------------------------------//
  inline bool EPollReactor::IsReadable(unsigned a_ev) const
    { return a_ev & (m_usePoll ? unsigned(POLLIN)  : unsigned(EPOLLIN));  }

  //-------------------------------------------------------------------------//
  // "IsWritable":                                                           //
  //-------------------------------------------------------------------------//
  inline bool EPollReactor::IsWritable(unsigned a_ev) const
    { return a_ev & (m_usePoll ? unsigned(POLLOUT) : unsigned(EPOLLOUT)); }

  //-------------------------------------------------------------------------//
  // "SetOUTEventMask": Allow/DisAllow OUT events:                           //
  //-------------------------------------------------------------------------//
  template<bool ON>
  inline void EPollReactor::SetOUTEventMask(IO::FDInfo* a_info) const
  {
    // This is currently for Stream sockets only:
    int fd = a_info->m_fd;

    assert(a_info != nullptr  && fd >= 0   &&    fd < m_MaxFDs  &&
           a_info->m_isSocket && a_info->m_type  == SOCK_STREAM &&
           a_info->m_htype    == IO::FDInfo::HandlerT::DataStream);

    // First, get the events this FD is waiting for -- HERE we really need
    // "m_waitEvents":
    uint32_t oldEvs = a_info->m_waitEvents;

    // Also check whether we indend to receive any data via this socket
    // (normally yes):
    bool   noRX =  (a_info->m_rd_buff == nullptr);
    assert(noRX == !bool(a_info->m_rh));

    if (m_usePoll)
    {
      // Poll: Just set or clear the resp mask bit -- the mask will be used in
      // all subsequent "poll"s:
      assert(m_pollFDs != nullptr);
      pollfd* pfd = m_pollFDs + fd;
      assert(pfd->fd == fd && pfd->events == short(oldEvs));

      if constexpr(ON)
        pfd->events |=   POLLOUT;
      else
        pfd->events &= (~POLLOUT);

      // Also, in a very unlikely case that the socket is write-only (ie no data
      // are supposed to be received via it), we reset the readability flags:
      if (utxx::unlikely(noRX))
        pfd->events &= (~POLLIN);

      // Also memoise the new events to wait for, in the FDInfo:
      a_info->m_waitEvents = uint32_t(pfd->events);
    }
    else
    {
      // EPoll:
      uint32_t  newEvs = ON ? (oldEvs | EPOLLOUT) : (oldEvs & (~EPOLLOUT));

      // Again, in a very unlikely case that the socket is write-only (ie no
      // data are to be received via it), we reset the readability flags:
      if (utxx::unlikely(noRX))
        newEvs &= (~EPOLLIN);

      // If the mask has really changed, save it back (requires a syscall,
      // that's why we do a check first):
      if (oldEvs != newEvs)
      {
        assert(m_epollFD >= 0);
        // Once again, memoise the "fd" in the "epoll_event":
        epoll_event ev;
        ev.events   = newEvs;
        ev.data.fd  = fd;

        CHECK_ONLY(int rc =) epoll_ctl(m_epollFD, EPOLL_CTL_MOD, fd, &ev);
        CHECK_ONLY
        (
          if (utxx::unlikely(rc < 0))
            IO::SystemError(-1, "EPollReactor::SetOUTEVentMask: Name=",
                            a_info->m_name.data());
        )
        // Also memoise the new events to wait for, in the FDInfo:
        a_info->m_waitEvents = newEvs;
      }
    }
  }

  //=========================================================================//
  // "Poll" (to be invoked by an external driver, or internal "Run"):        //
  //=========================================================================//
  void EPollReactor::Poll(int a_timeout_ms)
  {
    //-----------------------------------------------------------------------//
    // First of all, invoke the GenericHandlers (if any):                    //
    //-----------------------------------------------------------------------//
    // XXX: The following loop must provide for the possibility that  the curr
    // "GenericHandler" is Removed within its own Call-Back (eg if an error is
    // encountered):
    //
    for (unsigned i = 0; i < m_ghs.size(); ++i)
    {
      // Get the curr entry; it must be non-empty:
      auto&  gh = m_ghs[i];
      assert(!IsEmpty(gh.first) && gh.second);

      // Invoke the actual GenericHandler:
      gh.second();
    }

    //-----------------------------------------------------------------------//
    // Now get the next batch of events:                                     //
    //-----------------------------------------------------------------------//
    // NB: Avoid false exits if "epoll_wait" or "poll" gets interrupted -- catch
    // EINTR:
    int rc = 0;
    do
      // NB: The following is OK even if m_topFD==-1:
      rc =
        m_usePoll
        ? poll      (m_pollFDs, nfds_t(m_topFD + 1),     a_timeout_ms)
        : epoll_wait(m_epollFD, m_epEvents, m_MaxEvents, a_timeout_ms);
    while (utxx::unlikely(rc < 0 && errno == EINTR));

    if (utxx::unlikely(rc < 0))
    {
      // NB: This is a critical error which most likely means incorrect logic.
      // For this reason, it is NOT passed to "ErrHandler" and not returned as
      // an error flag: Throw an exception instead:
      assert(errno != EINTR);
      IO::SystemError
        (-1, "EPollReactor::Poll: poll()/epoll_wait() Failed: rc=", rc);
    }
    else
    if (rc == 0)
    {
      // This can only happen if we had a real time-out (>=0), not indefinite
      // wait (-1): No events, so nothing to do:
      assert(a_timeout_ms >= 0);
      return;
    }

    //-----------------------------------------------------------------------//
    // Process the curr events from "m_pollFDs"(Poll) / "m_epEvents"(EPoll): //
    //-----------------------------------------------------------------------//
    int fd = -1;   // Required in case of "poll"
    int p  = -1;   // Required in case of "epoll"

    while (true)
    {
      //---------------------------------------------------------------------//
      // Get the Events:                                                     //
      //---------------------------------------------------------------------//
      uint32_t events = 0;

      if (m_usePoll)
      {
        // Poll: iterate over all valid "fd"s and get events for each of them:
        ++fd;
        if (utxx::unlikely(fd > m_topFD || fd >= m_MaxFDs))
          break;
        pollfd*  pfd  =  m_pollFDs + fd;
        events = uint32_t(pfd->revents);
      }
      else
      {
        // EPoll: iterate over the "rc" events received:
        ++p;
        assert(p >= 0  && rc > 0);
        if (utxx::unlikely(p >= rc))
          break;

        // NB: HERE we make use of the memoised "fd"! It is not just provided by
        // the kernel -- it was saved by us in the prev calls to "epoll_ctl":
        epoll_event const* ev = m_epEvents + p;
        fd     = ev->data.fd;
        events = ev->events;

        // Check the FD obtained:
        if (utxx::unlikely(fd < 0 || fd >= m_MaxFDs || fd > m_topFD))
        {
          LOG_WARN(2,
            "EPollReactor::Poll: UnExpected Event with FD={}: {}; MaxFDs={}. ",
            "TopFD={}", fd, EPollEventsToStr(events), m_MaxFDs, m_topFD)
          continue;
        }
      }
      //---------------------------------------------------------------------//
      // Further checks (and get the "FDInfo"):                              //
      //---------------------------------------------------------------------//
      CHECK_ONLY
      (LOG_INFO(4,
        "EPollReactor::Poll: FD={}: {}", fd, EPollEventsToStr(events)))

      // OK, "fd" is within the valid range:
      assert(0 <= fd && fd < m_MaxFDs && fd <= m_topFD);

      // Get the "FDInfo", it must be valid -- if not, it is a very serious
      // error condition which terminated the whole Reactor:
      IO::FDInfo& info  = m_fdis[fd];

      // Check: If we use Poll, the follwoing two wait-for event sets must be
      // same:
      assert(!m_usePoll ||
            (info.m_waitEvents == uint32_t(m_pollFDs[fd].events)));

      // XXX: The following must normally happen:  If a certain FD was removed
      // from the Reactor,  it was also removed from the Poll/EPoll mechanism,
      // and we must NOT receive events on it. We should also not receive Zero
      // Events. YET it happens sometimes -- so log and ignore such events:
      //
      if (utxx::unlikely
         (events == 0 || info.m_fd != fd || info.m_inst_id == 0))
      {
        CHECK_ONLY
        (
          LOG_WARN(3,
            "EPollReactor::Poll: OldFD={}: Empty FDInfo(?), but still got "
            "Event={} ({}): Ignored (MaxFDs={}, TopFD={}, FDInfo.FD={}, "
            "FDInfo.InstID={}, FDInfo.Name={})",
            fd,        EPollEventsToStr(events), events, m_MaxFDs, m_topFD,
            info.m_fd, info.m_inst_id, info.m_name.data())
        )
        continue;
      }
      // Record the Events received in the FDInfo:
      info.m_gotEvents = events;

      //---------------------------------------------------------------------//
      // Call-Backs Invocation:                                              //
      //---------------------------------------------------------------------//
      // OK, "FDInfo" is valid -- will invoke a Call-Back on it, depending on
      // its "HType":
      // (*) Invoke "regular" IO event processing first, and check for EPoll-
      //     detected errors later. This is because the data could still  be
      //     available for Reading even in the presence  of  an EPoll  error
      //     (however, we will refrain from Writing into the socket if there
      //     was already an error detected);
      // (*) Still, the "Handle*" methods invoked below are aware  of EPoll-
      //     detected errors, so if a secondary error occurs there,  it will
      //     NOT trigger an exception -- rather, the primary error   will be
      //     processed:
      // (*) Otherwise, if any "unexpected" IO error is encountered by Handler,
      //     "HandleIOError" will be invoked but no exception will be thrown;
      //     still, an exception may occur for any other reasons in user-level
      //     handlers -- which is NOT caught here):
      //
      switch (info.m_htype)
      {
        case IO::FDInfo::HandlerT::DataStream:
          HandleDataStream(&info);
          break;
        case IO::FDInfo::HandlerT::DataGram:
          HandleDataGram  (&info);
          break;
        case IO::FDInfo::HandlerT::RawInput:
          HandleRawInput  (&info);
          break;
        case IO::FDInfo::HandlerT::Timer:
          HandleTimer     (&info);
          break;
        case IO::FDInfo::HandlerT::Signal:
          HandleSignal    (&info);
          break;
        case IO::FDInfo::HandlerT::Acceptor:
          HandleAccept    (&info);
          break;
        default:
          LOG_ERROR(1,
            "EPollReactor::Poll: UnExpected Handler type for FD={}: {}", fd,
            int(info.m_htype))
      }
      // And only now, process any possible EPoll-detected errors  (because we
      // may receive useful events along with them, and such events need to be
      // processed first).  In this general case, we do NOT automatically stop
      // the Reactor, and do not throw any exceptions:
      //
      if (utxx::unlikely(IsError(info.m_gotEvents)))
        HandleIOError<false, false>
          (info, "EPollReactor::Poll: Error event received");
    }
  }

  //=========================================================================//
  // "Run":                                                                  //
  //=========================================================================//
  void EPollReactor::Run
  (
    bool a_exit_on_excepts, // Terminate the loop when an exception propagates
    bool a_busy_wait        // Useful if "GenericHandler"s are present
  )
  {
    int const sleepTime = a_busy_wait ? 0 : -1;

    CHECK_ONLY
    (
      // XXX: Normally, BusyWait should be used in the presence of "Generic-
      // Handlers": if not, produce a warning but continue:
      if (utxx::unlikely(!(m_ghs.empty() || a_busy_wait)))
        LOG_WARN(1,
          "EPollReactor::Run: BusyWait is NOT used, but there are {} Generic"
          "Handler(s)", m_ghs.size())
    )
    //-----------------------------------------------------------------------//
    // Main Event Loop:                                                      //
    //-----------------------------------------------------------------------//
    // NB: If there are no FDs registered with Poll/EPoll (ie m_topFD==-1),
    // then we should exit the loop!
    m_isRunning = true;

    while (m_topFD >= 0)
      try
      {
        // Do one step:
        Poll(sleepTime);
      }
      catch (ExitRun const& exitRun)
      {
        // A user Call-Back required an immediate exit:
        LOG_INFO(1,
          "EPollReactor::Poll: ExitRun received: {}: EXITING...",
          exitRun.what())
        // Terminate the loop:
        break;
      }
      catch (exception const& exc)
      {
        // Any other exception: Log it:
        LOG_ERROR(1,
          "EPollReactor::Poll: EXCEPTION: {}{}",
          exc.what(), a_exit_on_excepts ? ": EXITING..." : "")

        // Terminate the loop if requested to do so, otherwise ignore it --
        // there is not much else we can do:
        if (a_exit_on_excepts)
          break;
      }

    // End of Main Event Loop
    m_isRunning = false;
  }

  //=========================================================================//
  // "HandleIOError":                                                        //
  //=========================================================================//
  // XXX: Implementation of this method is quite inefficient. Fortunately, it
  // should be invoked very infrequently:
  //
  template<bool StopReactor, bool Throwing, typename... ErrMsgArgs>
  inline void EPollReactor::HandleIOError
  (
    IO::FDInfo const&    a_info,
    ErrMsgArgs const&... a_emas
  )
  const
  {
    static_assert(!(StopReactor &&  Throwing), "EPollReactor::HandleIoError: "
                   "StopReactor and Throwing modes are incompatible");
    // Extract the exact error code:
    assert(a_info.m_fd >= 0 && a_info.m_inst_id != 0);
    int fd = a_info.m_fd;   // So known to be valid
    int ec = IO::GetSocketError(fd);

    // The following cases are HARMLESS -- just ignore them.  NB: here "EINTR"
    // refers to the "fd", NOT to the "poll" or "epoll_wait" itself. XXX: It is
    // perhaps also OK to get "EINPROGRESS"  if it was a  "connect" attempt --
    // though we should probably not get such an event at all:
    if (utxx::unlikely
       (ec == 0  ||  ec == EINTR || ec == EAGAIN ||
       (ec == EINPROGRESS && a_info.m_connecting)))
      return;

    // OTHERWISE: A REAL ERROR. Log it in any case. NB: "a_msg" may contain its
    // own "errno" value -- it's OK to present both of them:
    // XXX: The following is quite inefficient -- dynamic memory allocation is
    // used here:
    string errMsg = utxx::to_string(a_emas...);

    LOG_ERROR(1,
      "EPollReactor::HandleIOError: FD={}, Name={}, Events={}, errno={}: {}",
      fd, a_info.m_name.data(), EPollEventsToStr(a_info.m_gotEvents), ec,
      errMsg)

    // If the user-level "ErrHandler" is provided, invoke it:
    if (a_info.m_eh)
      a_info.m_eh(fd, ec, a_info.m_gotEvents, errMsg.data());

    // IMPORTANT: Do NOT "Remove" or "close" the FD; this is to be done by the
    // the  user-level "ErrHandler"! This is because the FD creation and "Add"-
    // ing it to the Reactor were managed externally,  and doing "Remove" here
    // can cause inconsistencies (eg the "ErrHandler" could close and re-create
    // this FD, and closing it again would definitely be wrong!)
    // Similarly, if the FD is TLS-enabled, do not close the TLS session here!
    //
    // And if requested (in case of serious system-wide error conditions), stop
    // the Reactor Loop, or throw an exception, or no action at all:
    if (Throwing)
      throw utxx::runtime_error(errMsg);
    if (StopReactor)
      ExitImmediately("EPollReactor::HandleIOError");
  }

  //=========================================================================//
  // "CheckGNUTLSError":                                                     //
  //=========================================================================//
  inline void EPollReactor::CheckGNUTLSError
  (
    int               a_tls_rc,     // >= 0 means OK
    IO::FDInfo const& a_info,
    char       const* a_where,
    char       const* a_extra_info  // May be NULL or empty
  )
  const
  {
    // Upon invocation, "a_info" must indeed use GNUTLS:
    assert(a_where != nullptr && a_info.m_tlsType == IO::TLSTypeT::GNUTLS);

    // Ignore harmless conds:
    if (a_tls_rc == GNUTLS_E_AGAIN || a_tls_rc == GNUTLS_E_INTERRUPTED ||
        a_tls_rc >= 0)
      return;

    // Otherwise: Generate the "errMsg":
    int    fd = a_info.m_fd;
    assert(fd >= 0);
    char   errMsg[512];

    if (a_tls_rc == GNUTLS_E_WARNING_ALERT_RECEIVED ||
        a_tls_rc == GNUTLS_E_FATAL_ALERT_RECEIVED)
    {
      // This is a TLS Alert, which is considered to be an error. Normally,
      // the GNUTLS session should be active at this point:
      char const* alertStr = nullptr;
      if (utxx::likely(a_info.m_tlsSess != nullptr))
        alertStr =
          gnutls_alert_get_strname(gnutls_alert_get(a_info.m_tlsSess));

      if (utxx::unlikely(alertStr == nullptr))
        alertStr = "UnKnown Alert";
      snprintf(errMsg,  sizeof(errMsg), "TLS Alert: %s: FD=%d: %s",
               a_where, fd, alertStr);
    }
    else
    {
      // A "real" TLS Error. XXX: This includes GNUTLS_E_REHANDSHAKE and GNUTLS_
      // E_GOT_APPLICATION_DATA: we currently do not support Re-HandShakes, as
      // their implementation depends on the TLS protocol version (1.2 or 1.3):
      char const* errStr = gnutls_strerror(a_tls_rc);
      assert  (errStr != nullptr);
      snprintf(errMsg,  sizeof(errMsg), "TLS Error: %s: FD=%d: %s",
               a_where, fd, errStr);
    }
    // XXX: Do NOT re-use "HandleIOError" here, because it checks for the OS-lvl
    // socket error, not for a TLS one. Do our own processing:
    // Log it:
    bool withExtraInfo = (a_extra_info != nullptr) && (*a_extra_info != '\0');
    LOG_ERROR(1,
      "EPollReactor::CheckGNUTLSError: {}: {}{}FD={}, Name={}: {}",
      a_where,
      (withExtraInfo ? a_extra_info : ""),
      (withExtraInfo ? ": "         : ""),
      fd,  a_info.m_name.data(),  errMsg)

    // If the user-level "ErrHandler" is provided, invoke it (still call "GetSo-
    // cketError", though it would normally yield 0):
    if (a_info.m_eh)
      a_info.m_eh(fd, IO::GetSocketError(fd), a_info.m_gotEvents, errMsg);

    // But do NOT throw an exception or stop the Reactor at this point!
  }

  //=========================================================================//
  // "HandleDataStream":                                                     //
  //=========================================================================//
  inline void EPollReactor::HandleDataStream(IO::FDInfo* a_info)
  {
    //-----------------------------------------------------------------------//
    // Checks:                                                               //
    //-----------------------------------------------------------------------//
    assert(a_info != nullptr && a_info->m_type == SOCK_STREAM  &&
           a_info->m_htype   == IO::FDInfo::HandlerT::DataStream);

    // XXX: It may happen, though unlikely, that the FDInfo is not valid (has
    // been destroyed) at this point: Then return immediately:
    int      fd  = a_info->m_fd;
    uint64_t iid = a_info->m_inst_id;

    if (utxx::unlikely(fd < 0 || iid == 0))
      return;

    // Otherwise, proceed with handling the activity on this FD:
    uint32_t events     = a_info-> m_gotEvents;
    bool     wasError   = IsError   (events);
    bool     isReadable = IsReadable(events);
    bool     isWritable = IsWritable(events);
    bool     useGNUTLS  = (a_info->m_tlsType == IO::TLSTypeT::GNUTLS);
    assert  (useGNUTLS == (a_info->m_tlsSess != nullptr));

    //-----------------------------------------------------------------------//
    // First of all: Are we in a TLS HandShake or On-Going TCP Connect?      //
    //-----------------------------------------------------------------------//
    // XXX: During the HandShake, we react only on Events which are in the same
    // direction as the last (attempted) IO operation,   to prevent accidential
    // blocking on re-invocation of "GNUTLSHandShake":
    //
    if (utxx::unlikely(useGNUTLS && a_info->m_handShaking && !wasError))
    {
      bool writing =  bool(gnutls_record_get_direction(a_info->m_tlsSess));
      bool reading =  !writing;
      if  (writing == isWritable || reading == isReadable)
        // Yes, can continue with the HandShake. If it succeeds, that cond will
        // be handled inside "GNUTLSHandShake" (unlike TCP Connect below):
        useGNUTLS  =  GNUTLSHandShake(a_info);

      // If the HandShake is complete, we can proceed to regular IO right now,
      // otherwise we must return and wait for further events:
      if (!(a_info->m_handShaken))
        return;
    }
    else
    // TCP Connect Successfully Completed?
    if (utxx::unlikely
       (a_info->m_connecting && !wasError && (isReadable || isWritable)))
    {
      a_info->m_connecting = false;
      a_info->m_connected  = true;

      // NB: We do not use TLS to connect to Proxy -- only afterwards:
      if (!useGNUTLS)
      {
        // No GNUTLS HandShake is required -- possibly reset the OUT event mask
        // and invoke the ConnectHandler NOW:
        assert(a_info->m_tlsType == IO::TLSTypeT::None);
        SetOUTEventMask<false>(a_info);

        LOG_INFO(2,
          "EPollReactor::HandleDataStream: FD={}, Name={}: TCP Connect "
          "Successful", fd, a_info->m_name.data())

        IO::FDInfo::ConnectHandler& ch = a_info->m_ch;
        if (ch)
          ch(fd);
        // As Connect is complete, do NOT return now -- proceed to regular IO!
      }
      else
      {
        LOG_INFO(2,
          "EPollReactor::HandleDataStream: FD={}, Name={}: TCP Connect "
          "Successful, Initiating TLS HandShake", fd, a_info->m_name.data())

        // With GNUTLS, install the now-connected FD in the TLS Session. ONLY
        // NOW the TLS Session gets associated with the socket (FD):
        gnutls_transport_set_int(a_info->m_tlsSess, fd);

        // Initiate a non-blocking TLS HandShake. If, by some magick, it comp-
        // letes synchronously (which is actually impossible), proceed to reg-
        // ular IO, otherwise we must return and wait for further events:
        useGNUTLS = GNUTLSHandShake(a_info);

        if (utxx::likely(!(a_info->m_handShaken)))
          return;
      }
    }
    //-----------------------------------------------------------------------//
    // Special Cases Done:                                                   //
    //-----------------------------------------------------------------------//
    // So: We can get here only after the socket has been connected; if TLS is
    // in use, TLSHandShake has been performed:
    assert(a_info->m_connected);
    assert(a_info->m_tlsType == IO::TLSTypeT::None || a_info->m_handShaken);

    //-----------------------------------------------------------------------//
    // Generic Reading:                                                      //
    //-----------------------------------------------------------------------//
    // NB: We will try to do so even if "wasError" is set:
    //
    if (isReadable)
    {
      IO::FDInfo::ReadHandler& rh = a_info->m_rh;
      CHECK_ONLY
      (
        if (utxx::unlikely(!rh))
        {
          // There was a Readability event but no "ReadHandler" was provided.
          // This is something odd (maybe it's actually a Connect event which
          // was not properly waited for?): log a warning:
          LOG_WARN(1,
            "EPollReactor::HandleDataStream: FD={}, Name={}: Got Events={} "
            "but no ReadHnadler installed",
            fd, a_info->m_name.data(), EPollEventsToStr(events))
          return;
        }
      )
      // NB:
      // (*) "ReadHandler" is not invoked directly -- it is a call-back from
      //     within "IO::ReadUntilEAgain";
      // (*) Since there is a "ReadHandle", the corresp buffer must also  be
      //     non-empty:
      //---------------------------------------------------------------------//
      // Read Action:                                                        //
      //---------------------------------------------------------------------//
      auto onRead  =
        [fd, iid, a_info, &rh]
        (char const* a_data, int a_data_sz, utxx::time_val a_ts_recv) -> int
        {
          // Return the number of bytes  actually consumed by the call-back (or
          // < 0 to reset the buffer and exit immediately):
          int res = rh(fd, a_data, a_data_sz, a_ts_recv);

          // But reset "res" if the socket was detached (by "HandleIOError" or
          // by "ReadHandler" itself) -- we cannot  read  from that socket any-
          // more:
          return
            (utxx::unlikely(a_info->m_fd != fd || a_info->m_inst_id != iid))
            ? (-1)
            : res;
        };
      //---------------------------------------------------------------------//
      // Error Action:                                                       //
      //---------------------------------------------------------------------//
      auto onError =
        [this, a_info, useGNUTLS, wasError](int a_ret, int a_errno) -> void
        {
          // If there was already an EPoll-detected error, ignore the secondary
          // one. Otherwise, it depends on whether we are in TLS Mode:
          if (!wasError)
          {
            if (useGNUTLS)
            {
              assert (a_ret == a_errno);
              CheckGNUTLSError
                (a_errno, *a_info,
                 "EPollReactor::HandleDataStream: IO::ReadUntilEAgain(GNUTLS) "
                 "Failed");
            }
            else   // StopReactor=false, Throwing=false:
              this->HandleIOError<false, false>
                (*a_info, "EPollReactor::HandleDataStream: IO::ReadUntilEAgain"
                 " Failed: ret=", a_ret, ", errno=", a_errno);
          }
        };
      //---------------------------------------------------------------------//
      // Proceed with Reading:                                               //
      //---------------------------------------------------------------------//
      // This depends on TLS Type; IsSocket=true in all cases:
      assert(useGNUTLS == (a_info->m_tlsType == IO::TLSTypeT::GNUTLS) &&
             useGNUTLS == (a_info->m_tlsSess != nullptr));

      switch (a_info->m_tlsType)
      {
        case IO::TLSTypeT::GNUTLS:
          IO::ReadUntilEAgain<true, IO::TLSTypeT::GNUTLS>
            (a_info, onRead, onError,
             "EPollReactor::HandleData(Stream,GNUTLS)");
          break;

        case IO::TLSTypeT::KernelTLS:
          IO::ReadUntilEAgain<true, IO::TLSTypeT::KernelTLS>
            (a_info, onRead, onError,
             "EPollReactor::HandleData(Stream,KernelTLS)");
          break;

        case IO::TLSTypeT::None:
          IO::ReadUntilEAgain<true, IO::TLSTypeT::None>
            (a_info, onRead, onError,
             "EPollReactor::HandleData(Stream,PlainTCP)");
          break;

        default:
          assert(false);
      }
    }
    //-----------------------------------------------------------------------//
    // Generic Writing:                                                      //
    //-----------------------------------------------------------------------//
    // (NB: Reading and Writing are NOT multually-exclusive). Normally, sending
    // data is done synchronously via "Send"; here we only send some data which
    // may remain unsent in the "m_wr_buff":
    // But only do it if there was no previous error, or write will likely fail:
    //
    if (isWritable && !wasError)
      DelayedSend(a_info);
    // All done!
  }

  //=========================================================================//
  // "HandleDataGram":                                                       //
  //=========================================================================//
  inline void EPollReactor::HandleDataGram(IO::FDInfo* a_info)
  {
    //-----------------------------------------------------------------------//
    // Checks:                                                               //
    //-----------------------------------------------------------------------//
    assert(a_info != nullptr && a_info->m_type == SOCK_DGRAM &&
           a_info->m_htype   == IO::FDInfo::HandlerT::DataGram);

    // XXX: It may happen, though unlikely, that the FDInfo is not valid (has
    // been destroyed) at this point: Then return immediately:
    int      fd  = a_info->m_fd;
    uint64_t iid = a_info->m_inst_id;

    if (utxx::unlikely(fd < 0 || iid == 0))
      return;

    // Any prior errors detected by the EPoll mechanism itself?
    bool wasError = IsError(a_info->m_gotEvents);

    //-----------------------------------------------------------------------//
    // Reading:                                                              //
    //-----------------------------------------------------------------------//
    // NB: We will try to do so even if "wasError" is set (though for DGram, it
    // is unlikely to succeed):
    //
    if (IsReadable(a_info->m_gotEvents))
    {
      IO::FDInfo::RecvHandler& rch = a_info->m_rch;
      CHECK_ONLY
      (
        if (utxx::unlikely(!rch))
        {
          // There was a Readability event but no "ReadHandler" was provided.
          // This is something odd (maybe it's actually a Connect event which
          // was not properly waited for?): log a warning:
          LOG_WARN(1,
            "EPollReactor::HandleDataGram: FD={}, Name={}: Got Events={} but "
            "no ReadHandler installed",
            fd,  a_info->m_name.data(), EPollEventsToStr(a_info->m_gotEvents))
          return;
        }
      )
      //---------------------------------------------------------------------//
      // Recv Action:                                                        //
      //---------------------------------------------------------------------//
      // NB: "RecvHandler" is not invoked directly -- it is a call-back from
      //     "recv_action" which is in turn invoked by "IO::RecvUntilEAgain":
      //
      auto recv_action =
        [fd, iid, a_info, &rch]
        (char       const* a_data, int a_data_sz, utxx::time_val a_ts_recv,
         IO::IUAddr const* a_from_addr) -> bool
        {
          // Get the RecvHandler result ("true" is interpreted as "continue",
          // "false" -- as "stop immediately"):
          // NB: Here we CAN continue further reading even if an exception has
          // occurred in the RecvHandler, because the following dgrams are not
          // directly affected:
          // NB: "a_data", "a_data_sz", "a_ts_recv" and "a_from_addr" will be
          // NULL/0/empty for invocaion on EAgain:
          //
          bool res = rch(fd, a_data, a_data_sz, a_ts_recv, a_from_addr);

          // NB: return  "false" if the socket was invalidated, otherwise return
          // whathever "res" we got:
          return
            (utxx::unlikely(a_info->m_fd != fd || a_info->m_inst_id != iid))
            ? false
            : res;
        };

      //---------------------------------------------------------------------//
      // Error Action:                                                       //
      //---------------------------------------------------------------------//
      auto err_action  =
        [this, a_info, wasError](int a_ret, int a_errno) -> void
        {
          // If there was already an EPoll-detected error, ignore the secondary
          // one. Otherwise:
          if (!wasError)
            // StopReactor=false, Throwing=false:
            this->HandleIOError<false, false>
              (*a_info, "EPollReactor::HandleDataGram: IO::RecvUntilEAgain "
                        "Failed: ret=", a_ret, ", errno=", a_errno);
        };

      //---------------------------------------------------------------------//
      // Invoke "IO::RecvUntilEAgain" with a Reactor-wide UDP buffer:        //
      //---------------------------------------------------------------------//
      // XXX: It will produce an error if "m_iov" was de-configured:
      //
      IO::RecvUntilEAgain
        (*a_info, m_udprxs, NU, recv_action, err_action,
         "EPollReactor::HandleDataGram)");
    }

    //-----------------------------------------------------------------------//
    // Writing:                                                              //
    //-----------------------------------------------------------------------//
    // (NB: Reading and Writing are not multually-exclusive). Normally, sending
    // data is done synchronously via "Send"; here we only send some data which
    // may remain unsent in the "m_wr_buff":
    // But only do it if there was no previous error, or we risk losing data:
    //
    if (IsWritable(a_info->m_gotEvents) && !wasError)
      DelayedSend (a_info);
    // All done!
  }

  //=========================================================================//
  // "HandleRawInput":                                                       //
  //=========================================================================//
  inline void EPollReactor::HandleRawInput(IO::FDInfo* a_info)
  {
    //-----------------------------------------------------------------------//
    // Checks:                                                               //
    //-----------------------------------------------------------------------//
    assert(a_info != nullptr &&
           a_info->m_htype   == IO::FDInfo::HandlerT::RawInput);

    // XXX: It may happen, though unlikely, that the FDInfo is not valid (has
    // been destroyed) at this point: Then return immediately:
    int      fd  = a_info->m_fd;
    uint64_t iid = a_info->m_inst_id;

    if (utxx::unlikely(fd < 0 || iid == 0))
      return;

    // Do not perform any actions by the Reactor itself -- invoke the Client-
    // provided Handler,  but prevent any exceptions from propagating:
    // Ie the Reactor has only detected  the Readability  cond on the socket
    // (maybe along with an Error cond -- it will be processed afterwards;
    // XXX: there is no way of making the Client-provided "rih" aware of that
    // and eg change its own error handling pattern):
    //
    IO::FDInfo::RawInputHandler& rih = a_info->m_rih;
    rih(fd);
    // NB: There is no reading loop here, so we do not check the InstID...
  }

  //=========================================================================//
  // "HandleTimer":                                                          //
  //=========================================================================//
  inline void EPollReactor::HandleTimer(IO::FDInfo* a_info)
  {
    //-----------------------------------------------------------------------//
    // Checks:                                                               //
    //-----------------------------------------------------------------------//
    assert(a_info != nullptr &&
           a_info->m_htype   == IO::FDInfo::HandlerT::Timer);

    // XXX: It may happen, though unlikely, that the FDInfo is not valid (has
    // been destroyed) at this point: Then return immediately. Also, from Timer
    // we are only interested in Readability events; all other events are sile-
    // ntly ignored:
    int      fd  = a_info->m_fd;
    uint64_t iid = a_info->m_inst_id;

    if (utxx::unlikely(fd < 0 || iid == 0 || !IsReadable(a_info->m_gotEvents)))
      return;

    // Purge the FD's read buffer. It contains a number (in our case, limited
    // by 32) of 8-byte dwords. The Read Buffer and the TimerHandler  must be
    // present:
    assert(a_info->m_rd_buff != nullptr);

    //-----------------------------------------------------------------------//
    // Read all pending data from the TimerFD (in 8-byte chanks):            //
    //-----------------------------------------------------------------------//
    // (Otherwise we would never receive an Edge again!):
    // NB: TimerFD is considered to be of "Stream" type -- will use "read";
    // IsSocket=false:
    //
    IO::ReadUntilEAgain<false, IO::TLSTypeT::None> // Not a Socket, no TLS
    (
      a_info,

      //---------------------------------------------------------------------//
      // Read Action:                                                        //
      //---------------------------------------------------------------------//
      // XXX: Obviously, "recv_ts" is not meaningful here:
      [](char const*, int a_data_sz, utxx::time_val) -> int
      {
        // All params except "a_data_sz" are ignored.   Count and return the
        // number of bytes in COMPLETE Timer Events, to be expelled from the
        // buffer:
        // NB: There is no user call-back invocation from WITHIN the reader,
        // so the FD cannot be detached in the meantime -- no need to check
        // the TimerFD validity:
        return (a_data_sz / int(sizeof(uint64_t))) * int(sizeof(uint64_t));
      },

      //---------------------------------------------------------------------//
      // ErrorHandler:                                                       //
      //---------------------------------------------------------------------//
      [this, a_info](int a_ret, int a_errno) -> void
      {
        // NB: In this case, we terminate the whole Reactor because it is a
        // serious INTERNAL ErrCond: StopReactor=true, Throwing=false:
        this->HandleIOError<true, false>
          (*a_info, "EPollReactor::HandleTimer: IO::ReadUntilEAgain Failed: "
                    "ret=", a_ret, ", errno=", a_errno);
      },

      "EPollReactor::HandleTimer"
    );

    //-----------------------------------------------------------------------//
    // Invoke the "TimerHandler" -- its only arg is the FD:                  //
    //-----------------------------------------------------------------------//
    // The data read do not matter  -- only the fact that there was an event.
    // NB: The handler is NOT invoked if there was a reading error above, and
    // the FDInfo has become invalid (which may or may not happen, depending
    // on the user-level Error Handler):
    //
    if (utxx::likely(a_info->m_fd >= 0 && a_info->m_inst_id == iid))
    {
      IO::FDInfo::TimerHandler& th = a_info->m_th;
      assert(a_info->m_fd == fd && th);
      th(fd);
    }
  }

  //=========================================================================//
  // "HandleSignal":                                                         //
  //=========================================================================//
  inline void EPollReactor::HandleSignal(IO::FDInfo* a_info)
  {
    //-----------------------------------------------------------------------//
    // Checks:                                                               //
    //-----------------------------------------------------------------------//
    assert(a_info != nullptr &&
           a_info->m_htype   == IO::FDInfo::HandlerT::Signal);

    // XXX: It may happen, though unlikely, that the FDInfo is not valid (has
    // been destroyed) at this point: Then return immediately. Also,  similar
    // to "HandleTimer",  we are only interested in Readability events;   all
    // other events are silently ignored:
    int      fd  = a_info->m_fd;
    uint64_t iid = a_info->m_inst_id;

    if (utxx::unlikely
       (fd < 0 || iid == 0 || !IsReadable(a_info->m_gotEvents)))
      return;

    // We will read in all "signalfd_siginfo" structs available -- they corres-
    // pond to the signals received. The SignalHandler must be present:
    IO::FDInfo::SigHandler& sh = a_info->m_sh;
    assert(sh);

    //-----------------------------------------------------------------------//
    // Reading:                                                              //
    //-----------------------------------------------------------------------//
    // NB: SignalFD is considered to be of "Stream" type -- will use "read";
    // IsSocket=false, No TLS:
    //
    IO::ReadUntilEAgain<false, IO::TLSTypeT::None>
    (
      a_info,

      // Read Action: "recv_ts" is not meaningful here:
      [fd, iid, a_info, &sh]
      (char const* a_data, int a_data_sz, utxx::time_val) -> int
      {
        // "a_data" actually contains "signalfd_siginfo"(s):
        char const* from   = a_data;
        char const* end    = a_data + a_data_sz;
        char const* p      = from;

        for (; p + sizeof(struct signalfd_siginfo) <= end;
             p  += sizeof(struct signalfd_siginfo))
        {
          // So: located a "signalfd_siginfo":
          struct signalfd_siginfo const* si =
            reinterpret_cast<signalfd_siginfo const*>(p);
          assert(si != nullptr);

          // Invoke a "SigHandler" on it.  XXX: If it propagates an exception,
          // the buffer may be left in an inconsistent  state (and the remain-
          // ing Signal events lost) -- but the same unavoidably happens if a
          // "ReadHandler" propagates an exception (because it must return the
          // number of bytes consumed):
          sh(fd, *si);

          // And if the socket was invalidated by "sh",  stop further reading
          // now:
          if (utxx::unlikely(a_info->m_fd < 0 || a_info->m_inst_id != iid))
            return -1;

          // If we got here: The socket must still be valid:
          assert(a_info->m_fd == fd && a_info->m_inst_id == iid);
        }
        // Return the total number of bytes consumed:
        return int(p - from);
      },

      // ErrorHandler:
      [this, a_info](int a_ret, int a_errno) -> void
      {
        // NB: Again, terminate the Reactor -- this is a very serious INTERNAL
        // ErrCond: StopReactor=true, Throwing=false:
        this->HandleIOError<true, false>
          (*a_info, "EPollReactor::HandleSignal: IO::ReadUntilEAgain Failed: "
                    "ret=", a_ret, ", errno=", a_errno);
      },

      "EPollReactor::HandleSignal"
    );
    // All Done!
  }

  //=========================================================================//
  // "HandleAccept":                                                         //
  //=========================================================================//
  inline void EPollReactor::HandleAccept(IO::FDInfo* a_info)
  {
    //-----------------------------------------------------------------------//
    // Checks:                                                               //
    //-----------------------------------------------------------------------//
    assert(a_info != nullptr &&
           a_info->m_htype   == IO::FDInfo::HandlerT::Acceptor);

    // XXX: It may happen, though unlikely, that the FDInfo is not valid (has
    // been destroyed) at this point: Then return immediately.  Similar to
    // "HandleTimer",  we are only interested in Readability events.  Also,
    // do NOT accept the connection if there was an EPoll-detected error on
    // the Acceptor socket (it's just safer to do it this way);   any other
    // events are silently ignored:
    //
    // The FD is guaranteed to be an Acceptor one (this was checked by the cor-
    // resp "Add*" method):
    int      acceptor_fd  = a_info->m_fd;
    uint64_t acceptor_iid = a_info->m_inst_id;

    if (utxx::unlikely
       (acceptor_fd < 0 || acceptor_iid == 0 ||
        !IsReadable(a_info->m_gotEvents)     || IsError(a_info->m_gotEvents)))
      return;

    // In this case, there are no buffers, but there MUST be an "AcceptHanfler":
    IO::FDInfo::AcceptHandler& ah = a_info->m_ah;
    assert(ah);

    // There could be multiple Clients trying to connect at same time, so per-
    // form "accept" repeatedly until EAGAIN is encountered. Get the addrs of
    // connecting Clients:
    //
    assert(a_info->m_domain == AF_INET || a_info->m_domain == AF_UNIX);
    IO::IUAddr addr;           // Initially all 0s
    socklen_t  addr_len =
      socklen_t((a_info->m_domain == AF_INET)
                ? sizeof(addr.m_inet)
                : sizeof(addr.m_unix));

    //-----------------------------------------------------------------------//
    // Accept Incoming Connections until EAGAIN:                             //
    //-----------------------------------------------------------------------//
    while (true)
    {
      // Ignore EINTR errors as usual:
      int client_fd = -1;
      do
        // NB: Use "accept4" actually. The specified flags are immediately set
        // on the  "client_fd" returned:
        client_fd = accept4(acceptor_fd,
                            reinterpret_cast<struct sockaddr*>(&addr),
                            &addr_len,
                            SOCK_NONBLOCK | SOCK_CLOEXEC);
      while(utxx::unlikely(client_fd < 0 && errno == EINTR));

      if (client_fd < 0 && errno == EAGAIN)
        // This is OK: No more incoming connections this time:
        break;
      else
      if (utxx::unlikely(client_fd < 0))
        // Invoke "HandleIOError", which will in turn invoke  the  user-level
        // "ErrHandler" (if provided), and then close and detach the Acceptor
        // FD.   This is a serious error condition -- will TERMINATE the Main
        // Event Loop: StopReactor=true, Throwing=false:
        HandleIOError<true, false>
          (*a_info,   "EPollReactor::HandleAccept: accept4() Failed: ret=",
           client_fd, ", errno=", errno);

      // Otherwise: All OK: Both the Acceptor and the Client FDs are valid:
      assert(a_info->m_fd == acceptor_fd && a_info->m_inst_id == acceptor_iid &&
             client_fd    >= 0           && addr_len > 0);

      // Invoke the user-level "AcceptHandler" -- but do NOT automatically att-
      // ach "client_fd" to this Reactor (the user-level handler may do so  if
      // it wants to):
      ah(acceptor_fd, client_fd, &addr);

      // In any case, we do NOT close "client_fd" here -- although it was creat-
      // ed by "accept" within the Reactor, from now on, its life-cycle is mana-
      // ged externally -- we have no idea when it could be discarded (and whet-
      // ther it was "Add"ed to this of another Reactor)..
      // But it might happen that  the "AcceptHandler"  has closed the Acceptor
      // Socket itself -- in that case, simply exit the Accept Loop:
      //
      if (utxx::unlikely
         (a_info->m_fd != acceptor_fd || a_info->m_inst_id != acceptor_iid))
        break;
    }
    // All Done!
  }

  //=========================================================================//
  // "Connect":                                                              //
  //=========================================================================//
  void EPollReactor::Connect
  (
    int          a_fd,
    char const*  a_server_addr,
    int          a_server_port
  )
  {
    //-----------------------------------------------------------------------//
    // Verify the Domain and Type of "a_fd":                                 //
    //-----------------------------------------------------------------------//
    assert(a_server_addr != nullptr);

    // The FD to be connected, must already be "Add"ed to the Reactor, and the
    // corresp Handler must be of "DataStream" type:
    IO::FDInfo&  info =  CheckFD<false>(nullptr, a_fd, "Connect");
    assert (info.m_fd == a_fd);

    CHECK_ONLY
    (
      if (utxx::unlikely
         (info.m_htype != IO::FDInfo::HandlerT::DataStream ||
          info.m_type  != SOCK_STREAM))
        throw utxx::badarg_error
              ("EPollReactor::Connect: FD=", a_fd, ", Name=",
               info.m_name.data(),   " is not a DataStream");
    )
    // Do not perform repeated connection attempts:
    if (utxx::unlikely(info.m_connecting || info.m_connected))
      throw utxx::badarg_error
            ("EPollReactor::Connect: FD=", a_fd, ", Name=",
             info.m_name.data(),
             ": Socket already connected, or connection is in progress");

    //-----------------------------------------------------------------------//
    // So: which address should we use?                                      //
    //-----------------------------------------------------------------------//
    if (info.m_domain == AF_INET)
    {
      struct sockaddr_in& addr = info.m_peer.m_inet;
      addr.sin_family          = AF_INET;
      addr.sin_addr.s_addr     = inet_addr(a_server_addr);
      addr.sin_port            = htons(uint16_t(a_server_port));
      info.m_peer_len          = sizeof(addr);

      CHECK_ONLY
      (
        // Verify the server IP address:
        if (utxx::unlikely(addr.sin_addr.s_addr == INADDR_NONE))
          throw utxx::badarg_error
                ("EPollReactor::Connect: FD=", a_fd, ", Name=",
                 info.m_name.data(),  ": Invalid server IP: ", a_server_addr);
      )
    }
    else
    {
      // UNIX-Domain Sockets are not supported by LibVMA:
      assert(info.m_domain == AF_UNIX && !m_useVMA);

      struct sockaddr_un& addr = info.m_peer.m_unix;
      addr.sun_family          = AF_UNIX;
      StrNCpy<true>(addr.sun_path, a_server_addr);
      info.m_peer_len          = sizeof(addr);
    }
    assert(info.m_peer_len > 0);

    //-----------------------------------------------------------------------//
    // Clean-up the Rx and Tx Buffers (important if it is a Re-Connect):     //
    //-----------------------------------------------------------------------//
    if (utxx::likely(info.m_rd_buff != nullptr))
      info.m_rd_buff->reset();

    if (utxx::likely(info.m_wr_buff != nullptr))
      info.m_wr_buff->reset();

    //-----------------------------------------------------------------------//
    // Initiate the connection:                                              //
    //-----------------------------------------------------------------------//
    // As usual, ignore EINTR errors (XXX: can they occur here at all?):
    int rc = 0;
    do  rc = connect
             (a_fd, reinterpret_cast<struct sockaddr*>(&info.m_peer),
              socklen_t(info.m_peer_len));
    while (utxx::unlikely(rc < 0 && errno == EINTR));

    // NB: For "connect", unlike "read" and "write",  EAGAIN is a  real error
    // (insufficient entries in the routing cache); EINPROGRESS is OK  -- but
    // only for STREAM sockets which really perform network / IPC exchange to
    // connect (XXX: check for UDS behaviour  --  but we currently do not use
    // them in Stream mode):
    //
    if (rc < 0 && errno == EINPROGRESS)
    {
      // Mark this FD as still "connecting":
      info.m_connecting = true;
      info.m_connected  = false;
      return;
    }
    else
    if (utxx::unlikely(rc < 0))
      // Real error (NB: this theoretically includes the case when the socket
      // is of DataGram type, and "connect" did not succeed):
      // StopReactor=false, Throwing=true (to notify the Caller):
      HandleIOError<false, true>
        (info, "EPollReactor::Connect: connect() Failed: ret=", rc, ", errno=",
         errno);
    else
    {
      // Otherwise: Connection already succeeded (which is nearly impossible):
      assert(rc == 0);
      info.m_connecting = false;
      info.m_connected  = true;

      // Even though it is a synchronous (in fact) connect, invoke the user-
      // level "ConnectHandler" if provided  (currently, this is for Stream
      // sockets only). However, if a TLS HandShake is required, do it first:
      //
      bool   useGNUTLS =  (info.m_tlsSess != nullptr);
      assert(useGNUTLS == (info.m_tlsType == IO::TLSTypeT::GNUTLS));

      if  (!useGNUTLS)
      {
        // XXX: We do not provide any logging here because this situation (sync-
        // ronously-successful Connect) is in fact absolutely impossible, unless
        // it is a UNIX-domain socket. However, technically speaking,   we still
        // need to (possibly) reset the OUT event mask here:
        SetOUTEventMask<false>(&info);

        // Immediately invoke the CallBack:
        IO::FDInfo::ConnectHandler& ch = info.m_ch;
        if (ch)
          ch(a_fd);
      }
      else
        // Initiate TLS HandShake, it may result in "useGNUTLS" being reset:
        useGNUTLS = GNUTLSHandShake(&info);
    }
    // All Done!
  }

  //=========================================================================//
  // "GNUTLSHandShake":                                                      //
  //=========================================================================//
  // Returns the flag indicating whether we will use GNUTLS after this call:
  //
  inline bool EPollReactor::GNUTLSHandShake(IO::FDInfo* a_info)
  {
    assert(a_info != nullptr && a_info->m_tlsType == IO::TLSTypeT::GNUTLS &&
           a_info->m_tlsSess != nullptr);

    a_info->m_handShaking = true;
    a_info->m_handShaken  = false;

    // Invoke (non-blocking) "gnutls_handshake"; immediately re-try on EINTRs:
    int rc = 0;
    do    rc = gnutls_handshake(a_info->m_tlsSess);
    while (utxx::unlikely(rc == GNUTLS_E_INTERRUPTED));

    if (rc == GNUTLS_E_SUCCESS)
    {
      // 0: HandShake has succeeded:
      a_info->m_handShaking = false;
      a_info->m_handShaken  = true;

      // From now on, we may not need the OUT mask to be permenently set:
      SetOUTEventMask<false>(a_info);

      LOG_INFO(2,
        "EPollReactor::GNUTLSHandShake: FD={}, Name={}: Successfully Completed",
        a_info->m_fd, a_info->m_name.data())

      // Now possibly switch from user-level TLS (GNUTLS) to Linux Kernel TLS:
      if (m_useKernelTLS)
      {
        LOG_INFO(2, "EPollReactor::GNUTLSHandShake: FD={}, Name={}: "
          "Enabling KernelTLS", a_info->m_fd, a_info->m_name.data())

        IO::EnableKernelTLS(a_info);
        assert(a_info->m_tlsSess == nullptr &&
               a_info->m_tlsType == IO::TLSTypeT::KernelTLS);
      }
      // Invoke the "ConnectHandler" at last!
      IO::FDInfo::ConnectHandler& ch = a_info->m_ch;
      if (ch)
        ch(a_info->m_fd);
    }
    else
      // Check whether "rc" is harmless or fatal. In particular, GNUTLS_E_AGAIN
      // is completely normal here. Invoke ErrHandler if there was a real error:
      CheckGNUTLSError(rc, *a_info, "GNUTLSHandShake");

    // Return the flag indicating whether we *still* use GNUTLS:
    bool   useGNUTLS =  (a_info->m_tlsType == IO::TLSTypeT::GNUTLS);
    assert(useGNUTLS == (a_info->m_tlsSess != nullptr));
    return useGNUTLS;
  }

  //=========================================================================//
  // "IsConnected":                                                          //
  //=========================================================================//
  // NB: For efficiency, this method entirely relies upon the "m_connected" flg
  // in "FDInfo":
  bool EPollReactor::IsConnected
  (
    int         a_fd,
    IO::IUAddr* a_peer,     // May be NULL -- then not used
    int*        a_peer_len  // May be NULL -- then not used
  )
  {
    IO::FDInfo const& info = CheckFD<false>(nullptr, a_fd, "IsConnected");
    if (info.m_connected)
    {
      assert(!info.m_connecting && info.m_peer_len > 0);
      if (a_peer != nullptr)
        *a_peer   = info.m_peer;

      if (a_peer_len != nullptr)
        *a_peer_len = info.m_peer_len;

      return true;
    }
    else
    {
      if (a_peer     != nullptr)
        memset(a_peer, 0, sizeof(IO::IUAddr));

      if (a_peer_len != nullptr)
        *a_peer_len  = 0;

      return false;
    }
  }

  //=========================================================================//
  // "Send":                                                                 //
  //=========================================================================//
  // User-level method. Attempts to send the data directly using "IO::SendUnt-
  // ilEAgain"; if EAGAIN occurs and there are any unsent data, puts them into
  // the write buffer, from where they will be sent later by "DelayedSend" (ie
  // when Writability is encountered on the socket):
  //
  utxx::time_val EPollReactor::Send
  (
    char const*  a_data,
    int          a_len,
    int          a_fd
  )
  {
    //-----------------------------------------------------------------------//
    // Find the "FDInfo"  for "a_fd" (the slot must NOT be empty, of course) //
    // ----------------------------------------------------------------------//
    // An exception occurs if "a_fd" is not valid:
    IO::FDInfo&  info =  CheckFD<false>(nullptr, a_fd, "Send");
    assert (info.m_fd == a_fd && info.m_inst_id != 0);

    // "FDInfo" must be of any "Data*" type, with a valid Write buffer. Ie,
    // both Stream and DataGram protocols are supported here:
    CHECK_ONLY
    (
      if (utxx::unlikely
         ((info.m_htype   != IO::FDInfo::HandlerT::DataStream &&
           info.m_htype   != IO::FDInfo::HandlerT::DataGram)  ||
           info.m_wr_buff == nullptr))
        throw utxx::badarg_error
              ("EPollReactor::Send: FD=", a_fd, ", Name=", info.m_name.data(),
               ": Socket is not of a Writable Type");

      // We also require that the socket MUST be connected prior to the first
      // "Send" (of both Stream and DataGram type):
      if (utxx::unlikely(!info.m_connected))
        throw utxx::badarg_error
              ("EPollReactor::Send: FD=", a_fd, ", Name=", info.m_name.data(),
               ": Socket must be connected");
    )
    //-----------------------------------------------------------------------//
    // On-Error Action:                                                      //
    //-----------------------------------------------------------------------//
    auto err_action =
      [this, &info] (int DEBUG_ONLY(aa_fd), int a_ret, int a_errno) -> void
      {
        assert(info.m_fd == aa_fd);
        // NB:
        // We do NOT terminate the Reactor but notify the Caller by propagating
        // an exception to it. XXX: Any unsent bytes which remained in our Send
        // Buffer or in the Kernel Buffer, are now LOST (after "HandleIOError").
        // There is currently no way of notifying the Caller of how many bytes
        // were successfuly sent (eg because we don't have access to the Kernel
        // Buffer, anyway):
        //
        if (info.m_tlsSess == nullptr)
          // No TLS: StopReactor=false, Throwing=true:
          this->HandleIOError<false, true>
            (info,  "EPollReactor::Send: IO::SendUntilEAgain Failed: ret=",
             a_ret, ", errno=", a_errno);
        else
        {
          // Using GNUTLS:
          assert(a_ret == a_errno && info.m_tlsType == IO::TLSTypeT::GNUTLS);
          CheckGNUTLSError
            (a_errno, info,
             "EPollReactor::Send: IO::SendUntilEAgain(GNUTLS) Failed");
        }
      };
    //-----------------------------------------------------------------------//
    // Invoke the actual Sender now:                                         //
    //-----------------------------------------------------------------------//
    utxx::time_val sendTS =
      (info.m_tlsSess == nullptr)
      ? // UseGNUTLS=false:
        IO::SendUntilEAgain<false>(a_data, a_len, info, err_action)
      : // UseGNUTLS=true:
        IO::SendUntilEAgain<true> (a_data, a_len, info, err_action);

    // Iff "sendTS" is empty, some data remains unsent, and will be subject to
    // "DelayedSend", so need to enable writability notifications:
    if (utxx::unlikely(sendTS.empty()))
      SetOUTEventMask<true> (&info);
    else
      SetOUTEventMask<false>(&info);

    // All Done!
    return sendTS;
  }

  //=========================================================================//
  // "SendTo":                                                               //
  //=========================================================================//
  utxx::time_val EPollReactor::SendTo
  (
    char const*       a_data,
    int               a_len,
    int               a_fd,
    IO::IUAddr const& a_dest_addr
  )
  {
    //-----------------------------------------------------------------------//
    // Find the "FDInfo"  for "a_fd" (the slot must NOT be empty, of course) //
    //-----------------------------------------------------------------------//
    // An exception occurs if "a_fd" is not valid:
    IO::FDInfo&  info =  CheckFD<false>(nullptr, a_fd, "SendTo");
    assert (info.m_fd == a_fd && info.m_inst_id != 0);

    CHECK_ONLY
    (
      // "FDInfo" must be of "DataGram" type, with a valid Write buffer:
      if (utxx::unlikely
         (info.m_htype   != IO::FDInfo::HandlerT::DataGram ||
          info.m_wr_buff == nullptr))
        throw utxx::badarg_error
              ("EPollReactor::SendTo: FD=", a_fd, ", Name=", info.m_name.data(),
               ": Socket is not of a Packet Writable Type");

      // If the above check is passed, the low-level socket type must always be
      // DGRAM:
      assert(info.m_type == SOCK_DGRAM);
    )
    // Length of "DestAddr" depends on the domain type:
    socklen_t destAddrLen =
      socklen_t((info.m_domain == AF_INET)
                ? sizeof(a_dest_addr.m_inet)
                : sizeof(a_dest_addr.m_unix));

    //-----------------------------------------------------------------------//
    // On-Error Action: Similar to that of "Send" above:                     //
    //-----------------------------------------------------------------------//
    auto onError =
      [this, &info] (int DEBUG_ONLY(aa_fd), int a_ret, int a_errno) -> void
      {
        assert(info.m_fd == aa_fd);
        // StopReactor=false, Throwing=true (to notify the Caller):
        this->HandleIOError<false, true>
              (info,   "EPollReactor::SendTo: IO::SendUntilEAgain Failed: "
               "ret=", a_ret, ", errno=", a_errno);
      };
    //-----------------------------------------------------------------------//
    // Now invoke the actual Sender:                                         //
    //-----------------------------------------------------------------------//
    // As "SendTo" is for DataGrams, there is currently no TLS support:
    utxx::time_val sendTS =
      IO::SendUntilEAgain<false>
        (a_data, a_len, info, onError, &a_dest_addr, int(destAddrLen));

    // Iff "sendTS" is empty, some data remains unsent, and will be subject to
    // "DelayedSend", so need to enable writability notifications:
    if (utxx::unlikely(sendTS.empty()))
      SetOUTEventMask<true> (&info);
    else
      SetOUTEventMask<false>(&info);

    return sendTS;
  }

  //=========================================================================//
  // "DelayedSend":                                                          //
  //=========================================================================//
  // Send out any bytes remaining in the "wrBuff" (which were not sent synchro-
  // nously):
  inline void EPollReactor::DelayedSend(IO::FDInfo* a_info)
  {
    //-----------------------------------------------------------------------//
    // Checks:                                                               //
    //-----------------------------------------------------------------------//
    assert(a_info != nullptr);
    utxx::dynamic_io_buffer* wrBuff = a_info->m_wr_buff;

    // XXX: It might be that "DelayedSend" was invoked AFTER the socket was re-
    // moved due to some error. So:
    if (utxx::unlikely
       (a_info->m_fd < 0  || a_info->m_inst_id == 0 || wrBuff == nullptr))
    {
      LOG_WARN(1, "EPollReactor::DelayedSend: FD has been invalidated? "
                  "FD={}, InstID={}",
                  a_info->m_fd, a_info->m_inst_id,
                  (wrBuff == nullptr ? ", WrBuff Empty":""))
      return;
    }
    // Also, there is nothing to do if the WrBuff is just empty -- which is a
    // very typical case:
    if (utxx::likely(wrBuff->empty()))
      return;

    // XXX: Due to "Send" logic, if buffered data exist, the socket must be
    // connected;  otherwise, "send" will fail for both Stream and DataGram
    // sockets:
    assert(a_info->m_connected);

    //-----------------------------------------------------------------------//
    // On-Error Action:                                                      //
    //-----------------------------------------------------------------------//
    auto errAction =
      [this, a_info] (int DEBUG_ONLY(a_fd), int a_ret, int a_errno) -> void
      {
        assert(a_info->m_fd == a_fd);
        // XXX: Similar to "Send" above, we currently have no way of telling the
        // Caller how many bytes were successfully sent (at least because there
        // is no access to the Kernel Buffer)...
        // StopReactor=false, Throwing=false (because this is DelayedSend):
        //
        if (a_info->m_tlsSess == nullptr)
          // No TLS or KernelTLS:
          this->HandleIOError<false, false>
            (*a_info, "EPollReactor::DelayedSend: IO::SendUntilEAgain Failed: "
             "ret=", a_ret, ", errno=", a_errno);
        else
        {
          // Using GNUTLS:
          assert(a_ret == a_errno && a_info->m_tlsType == IO::TLSTypeT::GNUTLS);
          CheckGNUTLSError
            (a_ret, *a_info,
             "EPollReactor::DelayedSend: IO::SendUntilEAgain(GNUTLS) Failed");
        }
      };
    //-----------------------------------------------------------------------//
    // Finally: Send the data from "wrBuff" (there are no other data):       //
    //-----------------------------------------------------------------------//
    // Any unsent data will remain in the buffer:
    //
    utxx::time_val sendTS =
      (a_info->m_tlsSess == nullptr)
      ? // UseGNUTLS=false:
        IO::SendUntilEAgain<false>(nullptr, 0, *a_info, errAction)
      : // UseGNUTLS=true:
        IO::SendUntilEAgain<true> (nullptr, 0, *a_info, errAction);

    // Iff "sendTS" is empty, some data remains unsent, and will be subject to
    // further "DelayedSend", so need to enable writability notifications:
    if (utxx::unlikely(sendTS.empty()))
      SetOUTEventMask<true> (a_info);
    else
      SetOUTEventMask<false>(a_info);
  }

  //=========================================================================//
  // External Static Utils:                                                  //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "EPollEventsToStr":                                                     //
  //-------------------------------------------------------------------------//
  string EPollReactor::EPollEventsToStr(uint32_t a_events) const
  {
    stringstream s;
    int n = 0;
    if (m_usePoll)
    {
      // Poll:
      if (a_events & POLLIN      ) { if (n++) s << '|'; s << "IN";      }
      if (a_events & POLLPRI     ) { if (n++) s << '|'; s << "PRI";     }
      if (a_events & POLLOUT     ) { if (n++) s << '|'; s << "OUT";     }
      if (a_events & POLLRDNORM  ) { if (n++) s << '|'; s << "RDNORM";  }
      if (a_events & POLLRDBAND  ) { if (n++) s << '|'; s << "RDBAND";  }
      if (a_events & POLLWRNORM  ) { if (n++) s << '|'; s << "WRNORM";  }
      if (a_events & POLLWRBAND  ) { if (n++) s << '|'; s << "WRBAND";  }
      if (a_events & POLLMSG     ) { if (n++) s << '|'; s << "MSG";     }
      if (a_events & POLLRDHUP   ) { if (n++) s << '|'; s << "RDHUP";   }
      if (a_events & POLLERR     ) { if (n++) s << '|'; s << "ERR";     }
      if (a_events & POLLHUP     ) { if (n++) s << '|'; s << "HUP";     }
      if (a_events & POLLNVAL    ) { if (n++) s << '|'; s << "NVAL";    }
    }
    else
    {
      // EPoll:
      if (a_events & EPOLLIN     ) { if (n++) s << '|'; s << "IN";      }
      if (a_events & EPOLLPRI    ) { if (n++) s << '|'; s << "PRI";     }
      if (a_events & EPOLLOUT    ) { if (n++) s << '|'; s << "OUT";     }
      if (a_events & EPOLLRDNORM ) { if (n++) s << '|'; s << "RDNORM";  }
      if (a_events & EPOLLRDBAND ) { if (n++) s << '|'; s << "RDBAND";  }
      if (a_events & EPOLLWRNORM ) { if (n++) s << '|'; s << "WRNORM";  }
      if (a_events & EPOLLWRBAND ) { if (n++) s << '|'; s << "WRBAND";  }
      if (a_events & EPOLLMSG    ) { if (n++) s << '|'; s << "MSG";     }
      if (a_events & EPOLLERR    ) { if (n++) s << '|'; s << "ERR";     }
      if (a_events & EPOLLHUP    ) { if (n++) s << '|'; s << "HUP";     }
      if (a_events & EPOLLRDHUP  ) { if (n++) s << '|'; s << "RDHUP";   }
      if (a_events & EPOLLWAKEUP ) { if (n++) s << '|'; s << "WAKEUP";  }
      if (a_events & EPOLLONESHOT) { if (n++) s << '|'; s << "ONESHOT"; }
      if (a_events & EPOLLET     ) { if (n++) s << '|'; s << "ET";      }
    }
    return s.str();
  }

  //-------------------------------------------------------------------------//
  // "SigMembersToStr":                                                      //
  //-------------------------------------------------------------------------//
  string EPollReactor::SigMembersToStr(sigset_t const& a_set)
  {
    stringstream s;
    int n = 0;
    for (int i = 1; i < NSIG; ++i) // XXX: i==NSIG seems to be wrong...
      if (sigismember(&a_set, i))
        { if (n++) s << '|'; s << strsignal(i); }
    return s.str();
  }
} // End namespace MAQUETTE
