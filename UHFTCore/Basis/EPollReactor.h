// vim:ts=2:et
//===========================================================================//
//                            "Basis/EPollReactor.h":                        //
//                        EPoll-Based IO Events Reactor                      //
//===========================================================================//
#pragma  once

#include "Basis/BaseTypes.hpp"
#include "Basis/IOUtils.h"
#include <utxx/compiler_hints.hpp>
#include <boost/container/static_vector.hpp>
#include <spdlog/logger.h>
#include <gnutls/gnutls.h>
#include <string>
#include <cstring>
#include <csignal>
#include <memory>
#include <utility>
#include <poll.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/uio.h>

namespace MAQUETTE
{
  //=========================================================================//
  // "EPollReactor":                                                         //
  //=========================================================================//
  class EPollReactor: public boost::noncopyable
  {
  private:
    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    int const        m_MaxFDs;
    bool const       m_usePoll;      // Use Poll instead of EPoll
    pollfd*          m_pollFDs;      // If Poll  is used
    int              m_epollFD;      // The EPoll MasterFD
    int const        m_MaxEvents;    // Only for EPoll
    epoll_event*     m_epEvents;     // Events received from EPoll
    IO::FDInfo*      m_fdis;
    int              m_topFD;        // m_topFD < m_MaxFDs

    bool             m_useKernelTLS; // Use Linux Kernel TLS, not GNUTLS

    // Are we using the Mellanox LibVMA kernel by-pass library? It implements
    // nearly the same Socket API, but there are still some semantic differen-
    // ces we must be aware of.
    // FIXME: This flag should be set on per-FD basis, NOT globally, as LibVMA
    // allows us to manage sockets selectively:
    bool             m_useVMA;

    // The following is a single per-Reactor set of data structs for receiving
    // UDP msgs, placed here for optimisation reasons. For incoming UDP traffic,
    // all msgs are supposed to be consumed synchronously, so there is no need
    // for per-FDInfo utxx::dynamic_io_buffer on recv  (but it is STILL needed
    // for UDP sends!) So to improve data locality, use a Reactor-common UDP Rd
    // buffer and related structs in that case:
    constexpr static int NU = 1024;
    IO::UDPRX*       m_udprxs;

    // TLS Support: Credentials are per-Reactor:
    gnutls_certificate_credentials_t m_tlsCreds;

    // Misc HouseKeeping stuff:
    spdlog::logger*  m_logger;       // Ptr NOT owned
    int              m_debugLevel;
    mutable uint64_t m_currInstID;   // Used to assign InstIDs to FDInfos
    mutable bool     m_isRunning;    // We are in the infinite "Run" loop!

    // We only allow a limited number of "GenericHandler"s, so use a "static_
    // vector" for them:
    constexpr static unsigned MaxGHs = 8;
    boost::container::static_vector
      <std::pair<ObjName, IO::FDInfo::GenericHandler>, MaxGHs>  m_ghs;

    // Deleted / Hidden stuff:
    EPollReactor            (EPollReactor const&) = delete;
    EPollReactor            (EPollReactor&&)      = delete;
    EPollReactor& operator= (EPollReactor const&) = delete;
    EPollReactor& operator= (EPollReactor&&)      = delete;
    bool          operator==(EPollReactor const&) const;
    bool          operator!=(EPollReactor const&) const;

    // No Default Ctor:
    EPollReactor() = delete;

  public:
    //=======================================================================//
    // Non-Default Ctor, Dtor:                                               //
    //=======================================================================//
    EPollReactor
    (
      spdlog::logger* a_logger,             // Must NOT be NULL
      int             a_debug_level,
      bool            a_use_poll       = false,
      bool            a_use_kernel_tls = true,
      int             a_max_fds        = 1024,
      int             a_cpu            = -1
    );

    ~EPollReactor() noexcept;

    //=======================================================================//
    // Adding FDs and their Handlers (Call-Backs) to the Reactor:            //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "AddDataStream":                                                      //
    //-----------------------------------------------------------------------//
    // NB: Raw TCP  and TLS-enabled Streams are supported. Creates and returns
    // a non-blocking TCP socket (also creates a TLS session if requested, but
    // it is not associated with that socket before it gets connected):
    //
    int AddDataStream
    (
      char const*                       a_name,       // May be NULL (then "")
      int                               a_fd,         // (-1) for Client-side
      // Handlers:
      IO::FDInfo::ReadHandler    const& a_on_read,
      IO::FDInfo::ConnectHandler const& a_on_connect, // May be empty
      IO::FDInfo::ErrHandler     const& a_on_error,
      // Buffer params:
      int                               a_rd_bufsz,   // RdBuff Size
      int                               a_rd_lwm,     // RdBuff LowWaterMark
      int                               a_wr_bufsz,   // WrBuff Size
      int                               a_wr_lwm,     // WrBuff LowWaterMark
      // Binding params:
      char const*                       a_ip          = nullptr,  // mb NULL|""
      // TLS-related params:
      bool                              a_use_tls     = false,
      char const*                       a_server_name = nullptr,
      IO::TLS_ALPN                      a_tls_alpn    = IO::TLS_ALPN::UNDEFINED,
      bool                              a_verify_server_cert   = false,
      char const*                       a_server_ca_files      = nullptr,
      char const*                       a_client_cert_file     = nullptr,
      char const*                       a_client_priv_key_file = nullptr
    );

    //-----------------------------------------------------------------------//
    // "AddDataGram":                                                        //
    //-----------------------------------------------------------------------//
    // For details, see the imp. Returns the FD:
    int AddDataGram
    (
      char const*                     a_name,        // May be NULL (then "")
      char const*                     a_local_addr,  // May be NULL
      int                             a_local_port,  // May be (< 0)
      IO::FDInfo::RecvHandler const&  a_on_recv,
      IO::FDInfo::ErrHandler  const&  a_on_error,
      int                             a_wr_bufsz,    // WrBuff Size
      int                             a_wr_lwm,      // WrBuff LowWaterMark
      char const*                     a_remote_addr, // To connect to: may be
      int                             a_remote_port  //   empty/invalid
    );

    //-----------------------------------------------------------------------//
    // "AddRawInput":                                                        //
    //-----------------------------------------------------------------------//
    // NB:
    // (*) only the Readability and Error conds on "a_fd" wil be managed by the
    //     Reactor itself; all IO operations are done by the "RawInputHandler";
    // (*) the socket ("a_fd") can be edge-triggered, or level-triggered:
    //
    void AddRawInput
    (
      char const*                        a_name,     // May be NULL (then "")
      int                                a_fd,
      bool                               a_is_edge_triggered,
      IO::FDInfo::RawInputHandler const& a_on_rawin,
      IO::FDInfo::ErrHandler      const& a_on_error
    );

    //-----------------------------------------------------------------------//
    // "AddTimer" (TimerFD is created, added and returned):                  //
    //-----------------------------------------------------------------------//
    int AddTimer
    (
      char const*                        a_name,     // May be NULL (then "")
      uint32_t                           a_initial_msec,
      uint32_t                           a_period_msec,
      IO::FDInfo::TimerHandler const&    a_on_timer,
      IO::FDInfo::ErrHandler   const&    a_on_error
    );

    // "ChangeTimer" modifies an existing Timer (given by the TimerFD):
    void ChangeTimer
    (
      int         a_timer_fd,
      uint32_t    a_initial_msec,
      uint32_t    a_period_msec
    );

    //-----------------------------------------------------------------------//
    // "AddSignals" (Signal Handler is added, SignalFD returned):            //
    //-----------------------------------------------------------------------//
    // Installs specified Handlers for all of the given Signals.  Returns (-1)
    // if at least one error was encountered (incl LibVMA mode, which does not
    // currenly support SignalFDs). Returns 0 on success:
    //
    int AddSignals
    (
      char const*                       a_name,       // May be NULL (then "")
      std::vector<int>          const&  a_signals,
      IO::FDInfo::SigHandler    const&  a_on_signal,
      IO::FDInfo::ErrHandler    const&  a_on_error
    );

    //-----------------------------------------------------------------------//
    // "AddAcceptor":                                                        //
    //-----------------------------------------------------------------------//
    // NB: "bind" and "listen" on "a_fd" are the Caller's responsibility!
    //
    void AddAcceptor
    (
      char const*                       a_name,      // May be NULL (same as "")
      int                               a_fd,        // AcceptorFD
      IO::FDInfo::AcceptHandler const&  a_on_accept,
      IO::FDInfo::ErrHandler    const&  a_on_error
    );

    //-----------------------------------------------------------------------//
    // "AddGeneric" (arbitrary event src):                                   //
    //-----------------------------------------------------------------------//
    // Adds a function to be invoked before the EPoll syscall  in each  "Poll"
    // invocation (and thus in each iteartion within "Run"). It would normally
    // check some non-FD-based event srcs (eg a MsgQueue) and, if an event is
    // available, process it:
    //
    void AddGeneric
    (
      char const*                       a_name,    // NOT NULL or "" here!
      IO::FDInfo::GenericHandler const& a_gh
    );

    //-----------------------------------------------------------------------//
    // "Remove":                                                             //
    //-----------------------------------------------------------------------//
    // Remove the FD from the EPollReactor; but the FD itself is NOT closed
    // automatically:
    void Remove(int a_fd);

    //-----------------------------------------------------------------------//
    // "RemoveGeneric":                                                      //
    //-----------------------------------------------------------------------//
    // Because there is no FD in this case, the "GenericHandler" is identified
    // by its name. Returns "true" iff it was indeed found and removed:
    //
    bool RemoveGeneric(char const* a_name);

    //-----------------------------------------------------------------------//
    // "IsLive":                                                             //
    //-----------------------------------------------------------------------//
    // Is the given FD active?
    //
    bool IsLive(int a_fd) const
    {
      if (0 <= a_fd && a_fd < m_MaxFDs)
      {
        IO::FDInfo const& info  = m_fdis[a_fd];
        return (info.m_fd == a_fd && info.m_inst_id != 0);
      }
      return false;
    }

    //=======================================================================//
    // Non-Blocking Connection Management:                                   //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "Connect":                                                            //
    //-----------------------------------------------------------------------//
    // Initiates a non-blocking connection of "a_fd" (which must be a stream
    // TCP or UDS socket) to the specified server.
    // TODO: Time-out (to be handled by the Reactor in a delayed manner). Cur-
    // rently, the "connect" initiated by this method would be in progress un-
    // til it succeeds, or until an explicit error is encountered:
    //
    void Connect
    (
      int         a_fd,
      char const* a_server_addr,        // IP address, or UDS filename
      int         a_server_port = -1    // Not used for UDS
    );

  public:
    //-----------------------------------------------------------------------//
    // "IsConnected":                                                        //
    //-----------------------------------------------------------------------//
    // Check the connection status. If the FD is connected,   "peer" and "peer_
    // len" are filled in accordingly (if not NULL), and "true" is returned.
    // Otherwise, those args are zeroed-out, and "false" is returned:
    //
    bool IsConnected(int a_fd, IO::IUAddr* a_peer, int* a_peer_len);

    //=======================================================================//
    // Main Reactor Loop:                                                    //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "Run", "Poll":                                                        //
    //-----------------------------------------------------------------------//
    // "Run":
    // Runs an inifinite loop (until "ExitRun" exception is signalled). Any ex-
    // ceptions are caught and handled: they may be logged and then ignored, or
    // may result in the loop termination (depending on the 1st arg); "ExitRun"
    // always results in the loop termination:
    //
    void Run
    (
      bool a_exit_on_excepts,   // Terminate the loop after any propagated exn
      bool a_busy_wait = false  // Useful if "GenericHandler"s are present
    );

    // "Poll":
    // Performs a single event loop iteration;  by default,  it is NON-BLOCKING
    // (ie timeout is 0). NB: Any exceptions occurring in the Call-Backs  (incl
    // "ExitRun"!) are propagated to the Caller:
    //
    void Poll(int a_timeout_ms = 0);

    //-----------------------------------------------------------------------//
    // Termination:                                                          //
    //-----------------------------------------------------------------------//
    // XXX: There is currently no other way of exiting the "Run" infinite loop
    // but to throw the following exception from any of the above Handlers; so
    // this is NOT actually an error condition!
    // Because this exception is not really an error, we do not inherit it from
    // std::exception to avoid boilerplates like
    // try
    // {
    //  (do something)
    // } catch (ExitRun const&)
    // {
    //   throw;
    // }
    // catch (std::exception const&)
    // {
    //   (do specific error handling)
    // }
    // NB: Copy ctor for an object thrown as an exception must be declared
    // "noexcept", so we can't use "std::string" inside this class. So use
    // "std::runtime_error" instead:
    //
    class ExitRun final
    {
    private:
      std::runtime_error   m_msg;

    public:
      ExitRun(char const*  a_where)
      : m_msg(a_where)
      {}

      char const* what() const noexcept
        { return m_msg.what(); }
    };

    // Exiting the Run Loop by throwing "ExitRun":
    void ExitImmediately(char const* a_where) const
    {
      if (utxx::likely(m_isRunning))
        throw ExitRun(a_where);
    }

    //=======================================================================//
    // "Send":                                                               //
    //=======================================================================//
    // NB: Reactor-based "Recv" and "Send" operations are quite DISSIMILAR to
    // each other!
    // (*) There is no need for an explicit user-initiated "Recv"; data are re-
    //     ceived when the Readability condition is encountered on the corresp
    //     socket; "ReadUntilEAgain" is automatically invoked, data are receiv-
    //     ed into the "m_rd_buff", the "ReadHandler" is invoked from insi-
    //     de of "ReadUntilEAgain", and it manages all incoming data chunks.
    // (*) On the contrary, "Send" is always client-initiated: the Reactor does
    //     not need to wait for the Writability condition on the socket (and
    //     this condition is almost always on, anyway). The client uses "Send"
    //     to send data into the socket until EAgain is encountered; then the
    //     rest of data are placed by "Send" into the "m_wr_buff", from which
    //     they are actually sent out later by the Reactor once the Writability
    //     condition on the socket is encountered again.
    // (*) Return value: send time if all "len" bytes have been sent through the
    //     socket (XXX: there is currently no reliable way to determine when
    //     they have actually been put on the wire), or empty value if the actu-
    //     al send was delayed and the data were buffered pending the writabili-
    //     ty condition on the socket:
    //
    utxx::time_val Send
    (
      char const*       a_data,
      int               a_len,
      int               a_fd
    );

    //=======================================================================//
    // "SendTo":                                                             //
    //=======================================================================//
    // For DataGram sockets only; both Internet and UNIX domains are supported:
    //
    utxx::time_val SendTo
    (
      char const*       a_data,
      int               a_len,
      int               a_fd,
      IO::IUAddr const& a_dest_addr
    );

    //=======================================================================//
    // Utils:                                                                //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // Event Mgmt:                                                           //
    //-----------------------------------------------------------------------//
    bool IsError   (unsigned a_ev) const;
    bool IsReadable(unsigned a_ev) const;
    bool IsWritable(unsigned a_ev) const;

    // Allow/Disallow OUT events to be received on a particular FD:
    template<bool ON>
    void SetOUTEventMask(IO::FDInfo* a_info) const;

  public:
    //-----------------------------------------------------------------------//
    // External Utils:                                                       //
    //-----------------------------------------------------------------------//
    std::string EPollEventsToStr       (uint32_t        a_events) const;
    static std::string SigMembersToStr (sigset_t const& a_set);

    //-----------------------------------------------------------------------//
    // Accessing "FDInfo" props for a given FD:                              //
    //-----------------------------------------------------------------------//
    // NB: Only "UserData" and Socket Name are accessible from outside; the
    // rest of "FDInfo" is currently not:
    //
    template<typename T>
    T&   GetUserData(int a_fd)
    {
      // The FDInfo must already exist (IsNew=false); IsRemove=false either:
      IO::FDInfo& info =
        CheckFD<false>(nullptr, a_fd, "EPollReactor::GetUserData");
      return info.m_userData.Get<T>();
    }

    // NB: Unlike "GetUserData", "GetName" returns a const string:
    //
    char const* GetName(int a_fd) const
    {
      IO::FDInfo const& info =
        CheckFD<false>(nullptr, a_fd, "EPollReactor::GetName");
      return  info.m_name.data();
    }

    //-----------------------------------------------------------------------//
    // Traverse all currently active "FDInfo"s:                              //
    //-----------------------------------------------------------------------//
    // Action:     bool(FDInfo const&):
    // Return val: "true" -> continue, "false" -> stop now:
    //
    template<typename Action>
    void TraverseFDInfos(Action const& a_action) const
    {
      assert(m_topFD < m_MaxFDs);
      for (int fd = 0; fd <= m_topFD; ++fd)
      {
        IO::FDInfo const& curr = m_fdis[fd];
        // FDs are allocated by the underlyying OS more or less sequentially,
        // and we assume taht most of those allocated FDs are "Add"ed to this
        // Reactor, so this "FDInfo" is likely to be active -- otherwise, Ac-
        // tion is not invoked:
        if (curr.m_fd == fd && !a_action(curr))
          break;
      }
    }

    //-----------------------------------------------------------------------//
    // "FDInfo" for a given FD (XXX: use with caution!):                     //
    //-----------------------------------------------------------------------//
    IO::FDInfo const& GetFDInfo(int a_fd) const
      { return CheckFD<false, false>(nullptr, a_fd, "GetFDInfo"); }

    //-----------------------------------------------------------------------//
    // Whether LibVMA is used:                                               //
    //-----------------------------------------------------------------------//
    bool WithLibVMA() const { return m_useVMA; }

    //-----------------------------------------------------------------------//
    // Access to the Logger:                                                 //
    //-----------------------------------------------------------------------//
    spdlog::logger* GetLogger() const { return m_logger; }

  private:
    //-----------------------------------------------------------------------//
    // Internal Utils:                                                       //
    //-----------------------------------------------------------------------//
    // "CheckFD": Verifies the FD and obtains the corresp FDInfo; non-const and
    // const versions:
    //
    template<bool IsNew, bool IsRemove = false>
    IO::FDInfo& CheckFD
    (
      char const* a_name,  // May be NULL (then ""; for diagnostics only)
      int         a_fd,
      char const* a_where  // Non-NULL
    );

    template<bool IsNew, bool IsRemove = false>
    IO::FDInfo const& CheckFD
    (
      char const* a_name,  // May be NULL (then ""; for diagnostics only)
      int         a_fd,
      char const* a_where  // Non-NULL
    )
    const;

    // Low-level method for registering already created "FDInfo"s with the
    // "epoll" or "poll" mechanism. Possibly increases "m_topFD":
    void AddToEPoll
    (
      IO::FDInfo* a_info,
      uint32_t    a_events,
      char const* a_where
    );

    // "Clear": possibly decreases "m_topFD":
    void Clear(IO::FDInfo* a_info);

    // Internal event handlers -- invoked before any user-level handler:
    void HandleDataStream(IO::FDInfo* a_info);
    void HandleDataGram  (IO::FDInfo* a_info);
    void HandleRawInput  (IO::FDInfo* a_info);
    void HandleTimer     (IO::FDInfo* a_info);
    void HandleSignal    (IO::FDInfo* a_info);
    void HandleAccept    (IO::FDInfo* a_info);
    void DelayedSend     (IO::FDInfo* a_info);

    // "HandleIOError":
    // "StopReactor" is for severe "internal" errors;
    // "Throwing"    is for calls from the User-Level API:
    //
    template<bool StopReactor, bool Throwing, typename... ErrMsgArgs>
    void HandleIOError   (IO::FDInfo const& a_info, ErrMsgArgs const&... a_emas)
    const;

    //-----------------------------------------------------------------------//
    // TLS-Related Methods:                                                  //
    //-----------------------------------------------------------------------//
    // "GNUTLSHandShake":
    // Initiates a Non-Blocking GNUTLS (user-level) HandShake. The socket must
    // already be TCP-connected prior to this call. Returns the flag indicating
    // whether we are still using user-level GNUTLS after this call:
    //
    bool GNUTLSHandShake(IO::FDInfo* a_info);

    // GNUTLS (user-level) error handling:
    void CheckGNUTLSError
    (
      int               a_tls_rc,
      IO::FDInfo const& a_info,
      char const*       a_where,
      char const*       a_extra_info = nullptr
    )
    const;
  };
} // End namespace MAQUETTE
