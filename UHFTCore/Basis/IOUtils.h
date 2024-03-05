// vim:ts=2:et
//===========================================================================//
//                             "Basis/IOUtils.h":                            //
//                Socket and File IO Utils, and Misc OS Stuff                //
//===========================================================================//
// NB: See "Basis/IOUtils.hpp" for extra (templated) functions!
//
#pragma  once

#include "Basis/BaseTypes.hpp"
#include "Basis/IOSendEmail.hpp"
#include <utxx/time_val.hpp>
#include <utxx/buffer.hpp>
#include <utxx/enum.hpp>
#include <boost/core/noncopyable.hpp>
#include <gnutls/gnutls.h>
#include <functional>
#include <sys/signalfd.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>
#include <utility>
#include <vector>
#include <spdlog/spdlog.h>
#include <boost/property_tree/ptree_fwd.hpp>

namespace MAQUETTE
{
namespace IO
{
  //=========================================================================//
  // "GlobalInit": To be invoked as early on as possible:                    //
  //=========================================================================//
  // (*) Blocks the specified signals (to be handled synchronously);
  // (*) Initialised the Logger Thread Pool
  // (*) Initialised GNUTLS (no harm if not required):
  //
  void GlobalInit(std::vector<int> const& a_sync_sig_nums);

  //=========================================================================//
  // Networking and TLS Support:                                             //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // Union of AF_INET and AF_UNIX addresses:                                 //
  //-------------------------------------------------------------------------//
  // NB: This is similar to (but may not be exactly same as) "struct sockaddr":
  union IUAddr
  {
    struct sockaddr_in m_inet;
    struct sockaddr_un m_unix;

    // Default Ctor:
    IUAddr() { memset(this, 0, sizeof(IUAddr)); }

    // Non-default ctor: Promotion from INET and UNIX adds:
    explicit IUAddr(sockaddr_in a_inet): m_inet(a_inet) {}
    explicit IUAddr(sockaddr_un a_unix): m_unix(a_unix) {}
  };

  //-------------------------------------------------------------------------//
  // "TransTypeT": Transport Protocols:                                      //
  //-------------------------------------------------------------------------//
  UTXX_ENUM(
  TransTypeT, int,    // The default is UNDEFINED(0)
    (RawUDP, "UDP")   // Maybe, just maybe...
    (RawTCP, "TCP")
    (HTTP11, "H1" )   // HTTP/1.1
    (WS,     "WS" )   // WS on top of HTTP/1.1
    (HTTP2,  "H2" )   // HTTP/2
  );

  //------------------------------------------------------------------------//
  // TLS_ALPN: Application-Level Protocol Negotiation in TLS Hand-Shake:    //
  //------------------------------------------------------------------------//
  enum class TLS_ALPN: int
  {
    UNDEFINED = 0,     // Any protocol not listed below (eg FIX)
    HTTP11    = 1,     // HTTP/1.1, incl WS
    HTTP2     = 2      // HTTP/2
  };

  //-------------------------------------------------------------------------//
  // "TLSTypeT": Type of TLS Support in a Socket:                            //
  //-------------------------------------------------------------------------//
  enum class TLSTypeT: int
  {
    None      = 0,    // Plain socket
    GNUTLS    = 1,    // With associated GNUTLS session obj
    KernelTLS = 2     // Linux Kernel TLS support
  };

  //-------------------------------------------------------------------------//
  // "HostInfo": (Name and/or IP) and Port to connect to:                    //
  //-------------------------------------------------------------------------//
  // XXX: Not memory-efficient (using std::string):
  //
  struct HostInfo
  {
    // Data Flds:
    std::string          m_name      {};
    std::string          m_ip        {};
    int                  m_port    = -1;
    bool                 m_isProxy = false;

    // Default Ctor:
    HostInfo() = default;

    // Non-Default Ctor:
    HostInfo
    (
      std::string const& a_name,
      std::string const& a_ip,
      int                a_port,
      bool               a_is_proxy
    );

    // Properties:
    bool IsEmpty() const
    {
      bool   emp =  (m_port < 0);
      assert(emp == (m_name.empty() && m_ip.empty()));
      return emp;
    }

    bool IPProvided  () const { return !m_ip.empty  (); }
    bool NameProvided() const { return !m_name.empty(); }
  };

  //-------------------------------------------------------------------------//
  // "ConnectInfo": Includes TLS and Proxied connection settings:            //
  //-------------------------------------------------------------------------//
  // XXX: Again, not memory-efficient (using std::string):
  //
  struct ConnectInfo
  {
    //-----------------------------------------------------------------------//
    // Data Flds:                                                            //
    //-----------------------------------------------------------------------//
    std::string   m_pfx    {};  // Technical fld
    HostInfo      m_host   {};  // Ultimate host to connect to
    HostInfo      m_proxy  {};  // Possible proxy host (may be empty)

    // Possible proxy in use? (atm only SOCKS5 w/o auth is supported):
    bool          m_useProxy         = false;

    // Possible  bind to a local IP (local port is normally selected by dflt);
    // typically used for routing purposes:
    std::string   m_bindIP             {};

    // TLS config:
    bool          m_useTLS           = false;
    TLS_ALPN      m_tlsALPN          = TLS_ALPN::UNDEFINED;  // UNDEF OK
    bool          m_verifyServerCert = false;
    std::string   m_serverCAFiles      {};     // File[,File...]
    std::string   m_clientCertFile     {};
    std::string   m_clientPrivKeyFile  {};

    //-----------------------------------------------------------------------//
    // Ctors:                                                                //
    //-----------------------------------------------------------------------//
    // Default Ctor:
    ConnectInfo() = default;

    // Non-Default Ctor: Flds vals taken from PTree:
    ConnectInfo
    (
      boost::property_tree::ptree const& a_params,
      TransTypeT                         a_tt,
      std::string                 const& a_bind_ip = ""
    );
  };

  //=========================================================================//
  // "FDInfo" Class: File Descriptor with Extended Details:                  //
  //=========================================================================//
  struct FDInfo: public boost::noncopyable
  {
    //=======================================================================//
    // Lowest-Level Event-Handling Call-Back Types:                          //
    //=======================================================================//
    // NB: Call-Back invovation is performed via the "std::function" mechanism.
    // The reason is the following:
    // (*) because call-backs are invoked through the FD table anyway, a fully-
    //     static interface based on variadic templates would not be consistent
    //     with that:  if we either get an advantage of a static interface,  we
    //     must lose the advantage of table-driven invocation (XXX currently no
    //     way of getting both of them);
    // (*) then we are left either with low-level type erasure via the "void*"
    //     to the underlying object, or "std::function", or the classical vir-
    //     tual methods mechanism; of then, "std::function" is known to have a
    //     very low invocation overhead (maybe just 2 instructions on top of
    //     the direct call), and is the most flexible, hence it is selected.
    //
    //-----------------------------------------------------------------------//
    // "ReadHandler":                                                        //
    //-----------------------------------------------------------------------//
    // When a Readability condition is encountered on "a_fd" for which IO hand-
    // lers are registered, the Reactor automatically performs "ReadUntilEAgain"
    // which internally invokes the "ReadHandler". The handler  must return the
    // number of bytes actually consumed by it  (which, for Stream sockets, may
    // be less than the number of bytes actually read and placed into "a_buf"),
    // which is used to control the buffer: the consumed bytes are expelled from
    // the buffer, the remaining bytes are kept within.
    // NB: This handler is for Stream sockets only!
    // Return value:
    //     >= 0: number of bytes consumed (0 is OK) used in Stream buffer mgmt;
    //     <  0: reset the buffer and stop immediately:
    //
    using ReadHandler =
          std::function<int (int a_fd,      char const* a_buff, int a_size,
                             utxx::time_val a_ts_recv)>;

    //-----------------------------------------------------------------------//
    // "RecvHandler":                                                        //
    //-----------------------------------------------------------------------//
    // Similar to "ReadHandler", but for DataGram sockets:
    // (*) Invoked on each DataGram read in;
    // (*) Also invoked (with empty args except "a_fd") when got EAgain;
    // Return value:
    //   "true" to continue, "false" to stop immediately:
    //
    using RecvHandler =
          std::function<bool(int  a_fd,     char const* a_buff, int a_size,
                             utxx::time_val a_ts_recv,
                             IUAddr const*  a_sender_addr)>;

    // XXX: There is currently no "{Write,Send,SendTo}Handler" (refer to "Send"
    // for a detailed discussion of Read/Write asymmetry)...

    //-----------------------------------------------------------------------//
    // "RawInputHandler":                                                    //
    //-----------------------------------------------------------------------//
    // This is for "raw" sockets which are primarily managed outside of the Re-
    // actor (eg those provided by 3rd-party libraries); they could be TCP, UDP
    // or any other. The Reactor is only responsible for detecting Readability
    // conditions on them; the actual input (of any kind) is  performed  by the
    // Client; in partricular, no buffers for such sockets is provided   by the
    // Reactor:
    //
    using RawInputHandler = std::function<void(int a_fd)>;

    //-----------------------------------------------------------------------//
    // "TimerHandler", "SigHandler":                                         //
    //-----------------------------------------------------------------------//
    // This handler type is for reacting to Timer events:
    using TimerHandler = std::function<void(int a_fd)>;

    // This handler type is for reacting to Signal events:
    using SigHandler   =
          std::function<void(int a_fd, signalfd_siginfo const& a_si)>;

    //-----------------------------------------------------------------------//
    // "AcceptHandler":                                                      //
    //-----------------------------------------------------------------------//
    // When a Readability condition is encountered on an Acceptor socket,  the
    // "accept" call is performed internally, and the following handler is in-
    // voked. Both the "acceptor_fd" and the "client_fd" are passed to it. The
    // "AcceptorHandler" or any other external function is responsible for clo-
    // sing the "client_fd" when it is no longer needed (and also for "Remove"-
    // ing it from the Reactor if it was "Add"ed). Also, "a_addr" is the address
    // of the connected Client (its length is determined by the socket domain):
    //
    using AcceptHandler =
          std::function
          <void(int a_acceptor_fd, int a_client_fd, IUAddr const* a_addr)>;

    //-----------------------------------------------------------------------//
    // "ConnectHandler":                                                     //
    //-----------------------------------------------------------------------//
    // Invoked when a non-blocking "Connect" succeeds (NB: if it fails or a
    // disconnect occurs, "ErrHandler" is invoked instead).
    // XXX: Currently, for TLS-enabled sockets, this handler is invoked SECOND
    // TIME on TLS HandShake completion; the CallEE should be able to distingu-
    // ish between the two states (TCP Connected / TLS HandShaken):
    //
    using ConnectHandler = std::function<void(int a_fd)>;

    //-----------------------------------------------------------------------//
    // "ErrHandler":                                                         //
    //-----------------------------------------------------------------------//
    // This handler type is for reporting Errors  -- can be used in conjunction
    // with any of the above handler types; "err_code" is usually same as "err-
    // no" (but to be sure, we obtain it via the corresp "getsockopt" call).
    // The erroneous FD is automatically detached from the "EPollReactor" in
    // any case, AFTER returning from "ErrHandler":
    // NB: This Handler is NOT intended for handling exceptions occurring in
    // user Call-Backs -- such exceptions are always propagated to the Caller
    // in "Poll", and managed according to the specified policy in "Run":
    // XXX: In case the FD is TLS-enabled, we do NOT pass the TLS Session ptr
    // to the "ErrHandler" -- presumably, the TLS Session can be identified by
    // the CallEE via the FD:
    //
    using ErrHandler =
          std::function<void(int  a_fd,  int a_err_code,  uint32_t a_events,
                             char const* a_msg)>;
    //-----------------------------------------------------------------------//
    // "GenericHandler":                                                     //
    //-----------------------------------------------------------------------//
    // An arbitrary closure which checks for any (typically non-FD-based) event
    // and processes it, incl all posssible error cases:
    //
    using GenericHandler = std::function<void()>;

    //=======================================================================//
    // "HandlerT" Enum:                                                      //
    //=======================================================================//
    // Handler Type (corresponds to call-back prototypes listed above):
    // NB: "GenericHandler" is not provided here, as it is not associated with
    // any FD / "FDInfo":
    //
    UTXX_ENUM(
    HandlerT,  int, // The default is UNDEFINED (0)
      DataStream,   // For a Stream socket,  using "read"
      DataGram,     // For a DGram  socket,  using "recv"
      RawInput,     // For any 3rd-party sockets (the Client performs input)
      Timer,        // For a TimerFD
      Signal,       // For a SignalFD
      Acceptor      // For an Acceptor socket
    );

    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    // Socket/Connection Data:
    ObjName                   m_name {};            // 64-byte in-place string
    int                       m_fd       = -1;
    bool                      m_isSocket = false;
    int                       m_domain   = -1;      // For sockets only
    int                       m_type     = -1;      // For sockets only
    ConnectInfo const*        m_cInfo    = nullptr; // Ptr NOT owned, NULL OK
    TLSTypeT                  m_tlsType  = TLSTypeT::None;
    gnutls_session_t          m_tlsSess  = nullptr; // If TLSType == GNUTLS

    // Our Event Handlers:
    // NB: Actually, at most 3 Handlers could be used per single "FDInfo" (eg
    // ReadHnadler, ConnectHandler & ErrHandler for "HandlerT::DataStream");
    // but for simplicity, we provide all Handler types here:
    HandlerT                  m_htype    = HandlerT::UNDEFINED;
    ReadHandler               m_rh {};     // For "HandlerT::DataStream"
    ConnectHandler            m_ch {};     // For "HandlerT::DataStream"
    RecvHandler               m_rch{};     // For "HandlerT::DataGram"
    RawInputHandler           m_rih{};     // For "HandlerT::RawInput"
    TimerHandler              m_th {};     // For "HandlerT::Time"
    SigHandler                m_sh {};     // For "HandlerT::Signal"
    AcceptHandler             m_ah {};     // For "HandlerT::Accept"
    ErrHandler                m_eh {};     // For all Handler types

    // Buffers and Run-Time Status; NB: "msghdr" and friends are for receiving
    // KernelTLS Control Msgs:
    utxx::dynamic_io_buffer*  m_rd_buff  = nullptr;  // Owned
    utxx::dynamic_io_buffer*  m_wr_buff  = nullptr;  // Owned
    mutable msghdr            m_msghdr;              // For KernelTLS support
    mutable char              m_cmsg[128];           // XXX: Probably enough?
    mutable iovec             m_iov;

    mutable uint64_t          m_inst_id     = 0;     // Instance ID of this FD
    mutable uint32_t          m_waitEvents  = 0;     // Events to wait for
    mutable uint32_t          m_gotEvents   = 0;     // Last events received
    mutable bool              m_connecting  = false; // Non-block conn in progr?
    mutable bool              m_connected   = false; // Connect completed?
    mutable bool              m_handShaking = false; // TLS HandShake in progr?
    mutable bool              m_handShaken  = false; // TLS HandShake complete?
    mutable IUAddr            m_peer {};             // Only if m_peer_len > 0
    mutable int               m_peer_len    = 0;
    mutable IUAddr            m_bound{};             // Local bind IP

    // UserData  (up to 64 bytes, can be installed directly in "FDInfo" for ef-
    // ficiency):
    UserData                  m_userData {};

    //=======================================================================//
    // Default Ctor, "Clear", Dtor:                                          //
    //=======================================================================//
    FDInfo();

    void Clear();

    ~FDInfo   ()  noexcept;

    //=======================================================================//
    // Local Socket Operations:                                              //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "SetSocketProps" (see the Impl):                                      //
    //-----------------------------------------------------------------------//
    // NB: Have to provide "a_name" and "a_fd" explicitly because this FDInfo
    // may be empty yet:
    template<bool IsAcceptor>
    void SetSocketProps
    (
      char const* a_name,  // May be NULL (then implies "")
      int         a_fd,
      char const* a_where
    );

    //-----------------------------------------------------------------------//
    // "Bind":                                                               //
    //-----------------------------------------------------------------------//
    // For INET sockets (TCP or UDP), BindAddr is LocalIP;
    //                                BindPort <= 0 means random allocation;
    // for UNIX sockets,              BindAddr is a FileSystem Path:
    //                                BindPort is ignored for UNIX sockets:
    void Bind(char const* a_bind_addr, int a_bind_port = -1);
  };
  // End of "FDInfo" struct

  //=========================================================================//
  // "UDPRX": For receiving UDP data:                                        //
  //=========================================================================//
  struct UDPRX
  {
    // Data Flds:
    IUAddr          m_fromAddr;   // UDP Sender's addr comes here
    iovec           m_iov;        // Points to the actual UDP Rd buffer
    char            m_cmsg[64];   // For ancillary data: XXX: Enough size?
    msghdr          m_mh;         // Contains ptrs to the above objs
    int             m_len;        // Actual data length
    utxx::time_val  m_ts;         // RecvTS extracted from the socket

    // Default Ctor, Dtor:
    UDPRX ();
    ~UDPRX() noexcept;
  };

  //=========================================================================//
  // "GetIP": DNS Resolution:                                                //
  //=========================================================================//
  // XXX: WARNING: This function may be BLOCKING!  It should be used during the
  // initialization phase ONLY, NOT in real-time operations! In addition, it is
  // quite inefficient (returns std::string):
  //
  std::string GetIP(char const* a_host_name);

  //=========================================================================//
  // EnableKernelTLS:                                                        //
  //=========================================================================//
  // May be invoked after TLS HandShaking has been done in user-space (by GNUTLS
  // library). Enables kernel-level TLS support in the given FD and switches off
  // further use of (user-level) GNUTLS:
  //
  void EnableKernelTLS(FDInfo* a_info);

  //=========================================================================//
  // Non-Blocking Socket IO:                                                 //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "SetBlocking":                                                          //
  //-------------------------------------------------------------------------//
  // Set/clear blocking mode on a given socket file descriptor. Returns old
  // value of the blocking attribute:
  //
  template<bool Set>
  bool SetBlocking(int a_fd);

  //=========================================================================//
  // MultiCast Mgmt Utils:                                                   //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "MCastManage":                                                          //
  //-------------------------------------------------------------------------//
  // An existing FD is attached to, or detached from, a multicast feed:
  // NB: If   an FD was created by "MCastSubscribe" above, then closing the FD
  //  may not cause the multicast routing to be stopped, so the UDP feed would
  //  continue to be routed to the curr host and received by the kernel,   and
  //  then dropped. So invoke MCastManage<false>() before closing the socket:
  //
  template<bool Join>
  void MCastManage
  (
    int         a_fd,
    char const* a_group_ip,
    char const* a_iface_ip,
    char const* a_src_ip = nullptr
  );

  //-------------------------------------------------------------------------//
  // "MkDGramSock":                                                          //
  //-------------------------------------------------------------------------//
  // Creates a new UDP socket or UNIX Domain DataGram socket,  makes  it Non-
  // Blocking, and Binds or Connects to the specified Addrs and Ports.
  // precisely:
  // (*) If "a_local_addr" is NOT a valid IP addr (but a valid string),   and
  //     "a_local_port" is invalid (< 0), then we create a UNIX Domain Socket
  //     (UDS). Otherwise, we create an INET UDP socket.
  // (*) The socket is bound to whatever Local(Addr/Port) we can derived from
  //     the params (or a random one).
  // (*) If Remove(Addr/Port) is valid, the socket is connected to that. Unlike
  //     Stream Sockets, this is a local non-blocking operation.
  // (*) This function is Linux-specific.
  // (*) XXX: There is no similar function in IO  for creating Stream Sockets;
  //     due to close interleaving with TLS support, such functionality is imp-
  //     lemented directly in EPollReactor.
  // (*) Returns (SocketFD, Domain):
  //
  std::pair<int,int> MkDGramSock
  (
    char const* a_local_addr,  // IP (INET) or Path (UNIX); may be NULL or ""
    int         a_local_port,  // May be invalid (< 0)
    char const* a_remote_addr, // IP (INET) or Path (UNIX); may be NULL or ""
    int         a_remote_port  // May be invalid (< 0)
  );

  //=========================================================================//
  // File IO:                                                                //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "FileExists":                                                           //
  //-------------------------------------------------------------------------//
  // NB: "access_flags" are eg "F_OK" as defined in "unistd.h":
  //
  bool FileExists(char const* a_file_path, int a_access_flags = F_OK);

  //=========================================================================//
  // Thread Scheduling Support (XXX: Not really IO...):                      //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "SetCPUAffinity":                                                       //
  //-------------------------------------------------------------------------//
  void SetCPUAffinity(int a_cpu_n);

  //-------------------------------------------------------------------------//
  // "SetRTPriority": Only works in privileged mode:                         //
  //-------------------------------------------------------------------------//
  // NB: 99 is the highest level for user-level processes (and 100 for in-ker-
  // nel processes); 0 is the lowest level (non-RT, actually):
  //
  constexpr int MaxRTPrio = 99;
  void SetRTPriority(int a_prio = MaxRTPrio);

  //=========================================================================//
  // Logger Factory:                                                         //
  //=========================================================================//
  std::shared_ptr<spdlog::logger> MkLogger
  (
    std::string const&  a_logger_name,
    std::string const&  a_log_output,
    bool                a_with_mt       = false,    // Multi-Threaded?
    long                a_log_file_size = 33554432, // 32MB by default
    int                 a_rotations     = 1000      // Keep 1000 rotated files
  );
} // End namespace IO
} // End namespace MAQUETTE
