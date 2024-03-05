// vim:ts=2:et
//===========================================================================//
//                            "Basis/IOUtils.cpp":                           //
//                Socket and File IO Utils, and Misc OS Stuff                //
//===========================================================================//
#include "Basis/IOUtils.hpp"
#include "Basis/ConfigUtils.hpp"

#include <spdlog/common.h>
#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <netinet/tcp.h>

using namespace std;

namespace MAQUETTE
{
namespace IO
{
  //=========================================================================//
  // "GlobalInit":                                                           //
  //=========================================================================//
  // Must be invoked as early as possible, certainly before first  call to "Mk-
  // Logger", otherwise the SpdLog threads could still receive unmaksed signals
  // which we handle synchronously in the main thread:
  //
  bool Inited = false;

  void GlobalInit(vector<int> const& a_sync_sig_nums)
  {
    if (utxx::unlikely(Inited))
      return;  // This is not an error

    //-----------------------------------------------------------------------//
    // First, block all signals we are going to manage synchronously:        //
    //-----------------------------------------------------------------------//
    sigset_t     sigSet;
    sigemptyset(&sigSet);

    for (int sigNum: a_sync_sig_nums)
      sigaddset(&sigSet, sigNum);

    int rc = sigprocmask(SIG_BLOCK, &sigSet, nullptr);
    if (utxx::unlikely(rc < 0))
      SystemError(rc, "GlobalInit: sigprocmask() failed");

    //-----------------------------------------------------------------------//
    // Now initialise the SpdLog Thread Pool:                                //
    //-----------------------------------------------------------------------//
    // XXX: Limit the queue size to 1024, otherwise there will be a substantial
    // memory footprint!
    spdlog::init_thread_pool(1024, 1);

    //-----------------------------------------------------------------------//
    // Initialise the GNUTLS Library (may or may not be required):           //
    //-----------------------------------------------------------------------//
    DEBUG_ONLY(int ok =) gnutls_global_init();
    assert(ok == 0);

    // All Done:
    Inited = true;
  }

  //=========================================================================//
  // "HostInfo" Non-Default Ctor:                                            //
  //=========================================================================//
  // XXX: It performs memory allocation (in "string") and possibly a blocking
  // DNS look-up -- must be invoked only in initialization code,  NOT along a
  // time-critical path:
  //
  HostInfo::HostInfo
  (
    std::string const& a_name,
    std::string const& a_ip,
    int                a_port,
    bool               a_is_proxy
  )
  : m_name   (a_name),
    m_ip     (a_ip),
    m_port   (a_port),
    m_isProxy(a_is_proxy)
  {
    CHECK_ONLY
    (
      // Either both Host and Port are provided, or both are not:
      bool hasHost = NameProvided() || IPProvided();
      bool hasPort = a_port >= 0    && a_port <= 65535;

      if (utxx::unlikely(hasHost != hasPort))
        UTXX_THROW_BADARG_ERROR("Inconsistent Host and Port");

      // Also, unless it is a Proxy, Host and Port *must* be provided:
      if (utxx::unlikely(!(a_is_proxy || (hasHost && hasPort))))
        UTXX_THROW_BADARG_ERROR("Incomplete Host address");
    )
    // If the HostName is provided but the IP is not, perform DNS look-up at
    // this point. XXX:
    // (*) It is a BLOCKING operation!
    // (*) Otherwise, if both HostName and IP are provided,  we do NOT check
    //     their consistency:
    //
    if (!IPProvided() && NameProvided())
      m_ip = GetIP(m_name.data());
  }

  //=========================================================================//
  // "ConnectInfo" Non-Default Ctor:                                         //
  //=========================================================================//
  ConnectInfo::ConnectInfo
  (
    boost::property_tree::ptree const& a_params,
    TransTypeT                         a_tt,
    std::string                 const& a_bind_ip
  )
  : m_pfx(a_tt.to_string()),
    m_host
    {
      a_params.get<std::string>(m_pfx + "HostName", ""),
      a_params.get<std::string>(m_pfx + "HostIP",   ""),
      a_params.get<int>        (m_pfx + "HostPort", -1),
      false
    },
    m_proxy
    {
      a_params.get<std::string>(m_pfx + "ProxyName", ""),
      a_params.get<std::string>(m_pfx + "ProxyIP",   ""),
      a_params.get<int>        (m_pfx + "ProxyPort", -1),
      true
    },
    m_useProxy(!m_proxy.IsEmpty()),
    m_bindIP  (a_bind_ip),
    // NB: The default val for UseTLS depends on the TransType:
    m_useTLS
      (a_params.get<bool>      (m_pfx + "UseTLS",
                                a_tt != TransTypeT::UNDEFINED)),
    m_tlsALPN
      (m_useTLS
       ? ((a_tt == TransTypeT::HTTP2)
          ? TLS_ALPN::HTTP2 :
          (a_tt == TransTypeT::HTTP11 || a_tt == TransTypeT::WS)
          ? TLS_ALPN::HTTP11          // WS is over HTTP/1.1
          : TLS_ALPN::UNDEFINED)
       : TLS_ALPN::UNDEFINED),
    m_verifyServerCert
      (m_useTLS
       ? a_params.get<bool>       (m_pfx + "VerifyServerCert", true)
       : false),
    m_serverCAFiles
      (m_useTLS
       ? a_params.get<std::string>(m_pfx + "ServerCAFiles",  "")
       : ""),
    m_clientCertFile
      (m_useTLS
       ? a_params.get<std::string>(m_pfx + "ClientCertFile",   "")
       : ""),
    m_clientPrivKeyFile
      (m_useTLS
       ? a_params.get<std::string>(m_pfx+"ClientPrivKeyFile",  "")
       : "")
  {
    // To use TLS, HostName must be present:
    CHECK_ONLY
    (
      if (utxx::unlikely(m_useTLS && !m_host.NameProvided()))
        UTXX_THROW_BADARG_ERROR("TLS requires HostName");
    )
  }

  //=========================================================================//
  // "FDInfo" Default Ctor:                                                  //
  //=========================================================================//
  FDInfo::FDInfo()
  {
    // Most flds are initialized to their declared default vals. It remains to
    // set the following ones:
    memset(&m_msghdr, '\0', sizeof(m_msghdr));
    memset(m_cmsg,    '\0', sizeof(m_cmsg));

    // Set up the "msghdr" / "iov" relationships (for KernelTLS):
    m_msghdr.msg_control    = m_cmsg;
    m_msghdr.msg_controllen = sizeof(m_cmsg);
    m_msghdr.msg_iov        = &m_iov;
    m_msghdr.msg_iovlen     = 1;
    m_iov.iov_base          = nullptr;     // As yet!
    m_iov.iov_len           = 0;           // As yet!
  }

  //=========================================================================//
  // "FDInfo::Clear":                                                        //
  //=========================================================================//
  void FDInfo::Clear()
  {
    // NB: do NOT perform any kernels calls here (this is done in EPollReactor
    // ::Clear), just clear the flds:
    //
    memset(m_name.data(), '\0', sizeof(m_name));
    m_fd         = -1;
    m_isSocket   = false;
    m_domain     = -1;
    m_type       = -1;

    // Reset all Handlers:
    m_htype      = HandlerT::UNDEFINED;
    m_rh         = ReadHandler();
    m_ch         = ConnectHandler();
    m_rch        = RecvHandler();
    m_rih        = RawInputHandler();
    m_th         = TimerHandler();
    m_sh         = SigHandler();
    m_ah         = AcceptHandler();
    m_eh         = ErrHandler();

    // Remove the Buffers (if any).
    // POTENTIAL DANGER HERE: In some cases,  "Clear" is invoked  via "Remove"
    // from a Handler of this "FDInfo", and upon returning to the Handler,  it
    // may try to continue using the Buffers via previously-saved ptrs (which
    // are now invalid), which  will be a  very  serious error condition. For
    // the moment, there is no automatic way of  handling that -- it is a res-
    // ponsibility of the caller to provide correct mode of operation, eg NOT
    // to use Buffer ptr aliases:
    //
    if (m_rd_buff != nullptr)
    {
      delete m_rd_buff;
      m_rd_buff  = nullptr;
    }
    if (m_wr_buff != nullptr)
    {
      delete m_wr_buff;
      m_wr_buff  = nullptr;
    }

    // Remove the TLS Session (if present):
    if (m_tlsSess != nullptr)
    {
      gnutls_deinit(m_tlsSess);
      m_tlsSess   = nullptr;
    }
    m_tlsType     = TLSTypeT::None;

    // Reset the following flds (in particular, InstID):
    m_inst_id     = 0;
    m_waitEvents  = 0;
    m_gotEvents   = 0;
    m_connecting  = false;
    m_connected   = false;
    m_handShaking = false;
    m_handShaken  = false;
    memset(&m_peer, 0, sizeof(m_peer));
    m_peer_len    = 0;

    // Verify the "msghdr" relationships:
    m_iov.iov_base          = nullptr;
    m_iov.iov_len           = 0;
    assert(m_msghdr.msg_control    == m_cmsg         &&
           m_msghdr.msg_controllen <= sizeof(m_cmsg) &&
           m_msghdr.msg_iov        == &m_iov         &&
           m_msghdr.msg_iovlen     == 1);

    // Also zero-out the UserData:
    memset(&m_userData, '\0', sizeof(m_userData));
  }

  //=========================================================================//
  // "FDInfo::SetSocketProps":                                               //
  //=========================================================================//
  // XXX: This method may set some flds of this "FDInfo" but it DOES NOT as yet
  // set "m_fd" to "a_fd":
  //
  template<bool IsAcceptor>
  inline void FDInfo::SetSocketProps
  (
    char const* CHECK_ONLY(a_name),   // Can be NULL
    int         a_fd,
    char const* CHECK_ONLY(a_where)
  )
  {
    //-----------------------------------------------------------------------//
    // Checks:                                                               //
    //-----------------------------------------------------------------------//
    // XXX: This method only works in the non-LibVMA mode, otherwise it must
    // NOT be invoked at all -- "m_domain" and "m_type" must be set explicitly
    // by the CallER -- but we cannot tell here whether LibVMA is set, so it
    // is the CallER's responsiblity:
    //
    // NB: "a_fd" must be valid, but "m_fd" might not:
    assert(a_where != nullptr && a_fd >= 0);

    // Is this FD a socket? (Normally yes, but not always):
    struct stat  statRes;
    fstat(a_fd, &statRes);

    m_isSocket = S_ISSOCK(statRes.st_mode);
    m_domain   = -1;
    m_type     = -1;

    // The rest is for Sockets only:
    if (utxx::unlikely(!m_isSocket))
      return;

    //-----------------------------------------------------------------------//
    // Get the socket Domain:                                                //
    //-----------------------------------------------------------------------//
    socklen_t sz      = sizeof(int);
    CHECK_ONLY(int rc =)
      getsockopt(a_fd, SOL_SOCKET, SO_DOMAIN, &m_domain, &sz);

    CHECK_ONLY
    (
      if (utxx::unlikely(rc < 0))
      {
        // XXX: although it is serious error cond, do NOT close "a_fd" here --
        // we don't know who manages its life-cycle.  We assume that the buffs
        // can safely be de-allocated:
        Clear();
        SocketError<false>
          (-1, a_fd, "EPollReactor::", a_where,  ": getsockopt(SO_DOMAIN) "
                     "failed: Name=", (a_name != nullptr) ? a_name : "NULL");
      }
    )
    //-----------------------------------------------------------------------//
    // Get the socket Type:                                                  //
    //-----------------------------------------------------------------------//
    sz = sizeof(int);
    CHECK_ONLY(rc =)
      getsockopt(a_fd, SOL_SOCKET, SO_TYPE, &m_type, &sz);

    CHECK_ONLY
    (
      if (utxx::unlikely(rc < 0))
      {
        // Again, do NOT close "a_fd":
        Clear();
        SocketError<false>(-1, a_fd, "EPollReactor::", a_where,
                               ": getsockopt(SO_TYPE) failed: Name=",
                               (a_name != nullptr) ? a_name : "NULL");
      }
      // The Domain and Type can only be the following:
      if (utxx::unlikely(m_domain != AF_INET && m_domain != AF_UNIX))
        // Again, "a_fd" remains open -- just of wrong Domain:
        throw utxx::badarg_error
              ("EPollReactor::",    a_where, ": UnSupported socket domain: ",
               m_domain, ", FD=",   a_fd,    ", Name=",
              (a_name != nullptr) ? a_name : "NULL");

      if (utxx::unlikely
        (m_type != SOCK_STREAM && m_type != SOCK_DGRAM))
        // Again, "a_fd" remains open -- just of wrong Type:
        throw utxx::badarg_error
              ("EPollReactor::",    a_where, ": UnSupported socket type: ",
               m_type, ", FD=",     a_fd,    ", Name=",
              (a_name != nullptr) ? a_name : "NULL");
    )
    //-----------------------------------------------------------------------//
    // If the socket is NOT an acceptor one:                                 //
    //-----------------------------------------------------------------------//
    // Check if it is already connected, and mark it accordingly:
    if (!IsAcceptor)
    {
      socklen_t plen =
        socklen_t((utxx::likely(m_domain == AF_INET))
                  ? sizeof(m_peer.m_inet)
                  : sizeof(m_peer.m_unix));

      if (getpeername
         (a_fd, reinterpret_cast<struct sockaddr*>(&m_peer), &plen) == 0)
      {
        // Yes, connected:
        assert(plen > 0);
        m_connecting = false;
        m_connected  = true;
        m_peer_len   = int(plen);
      }
      else
      {
        // Just in case, zero out "info.m_peer" once again; this is NOT an
        // error -- the socket is just not connected:
        memset(&m_peer, 0, sizeof(m_peer));
        m_peer_len    = 0;
      }
    }
    CHECK_ONLY
    (
    else
    //---------------------------------------------------------------------//
    // Acceptor socket:                                                    //
    //---------------------------------------------------------------------//
    {
      if (utxx::unlikely(m_type == SOCK_DGRAM))
      {
        // Something is obviously wrong: it cannot be a DGram:
        Clear();
        throw utxx::badarg_error
              ("EPollReactor::", a_where,
               "DGram socket cannot be an Acceptor: FD=", a_fd, ", Name=",
              (a_name != nullptr) ? a_name : "NULL");
      }
      // Otherwise: Check that it is indeed an Acceptor socket, ie has a non-0
      // listening queue created for it:
      int res = 0;
      sz      = sizeof(res);
      if (utxx::unlikely
         (getsockopt(a_fd, SOL_SOCKET, SO_ACCEPTCONN, &res, &sz) < 0 || !res))
        throw utxx::badarg_error
              ("EPollReactor::", a_where,
               ": Not a valid acceptor socket: FD=", a_fd, ", Name=",
              (a_name != nullptr) ? a_name : "NULL");
    } )
    // XXX: Although some flds of this "FDInfo" have been filled in, "a_fd" is
    // NOT YET installed as "m_fd", ie this "FDInfo" is not yet marked as "occ-
    // upied", because further checks may be necessary!
  }

  //-------------------------------------------------------------------------//
  // "FDInfo::SetSocketProps" Instances:                                     //
  //-------------------------------------------------------------------------//
  template void FDInfo::SetSocketProps<false>
    (char const* a_name, int a_fd, char const* a_where);

  template void FDInfo::SetSocketProps<true>
    (char const* a_name, int a_fd, char const* a_where);

  //=========================================================================//
  // "FDInfo::Bind":                                                         //
  //=========================================================================//
  void FDInfo::Bind(char const* a_bind_addr, int a_bind_port)
  {
    // This is for valid sockets (INET (TCP or UDP) or UNIX) only. Otherwise,
    // silently do nothing:
    int fd = m_fd;
    if (utxx::unlikely(fd < 0 || !m_isSocket))
      return;

    if (utxx::likely(m_domain == AF_INET))
    {
      //---------------------------------------------------------------------//
      // INET Socket:                                                        //
      //---------------------------------------------------------------------//
      // NB: BindAddr/Port may be empty (or invalid), in which case we still
      // perform bind to ANY IP and/or port. So have we got the port to bind to?
      //
      in_addr_t ipAddr = inet_addr(a_bind_addr);
      if (utxx::unlikely(ipAddr == INADDR_NONE))
        ipAddr = INADDR_ANY;
      int port         = std::max<int>(a_bind_port, 0);

      // XXX: We DO ALLOW address re-use:
      int yes  = 1;
      if (utxx::unlikely
         (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0))
            // NB: here "fd" will be closed, because it is not in use yet:
            SocketError<true>(-1, fd, "IO::Bind: Cannot reuse the address");

      // Now perform the OS-level "bind":
      struct sockaddr_in        ia;
      ia.sin_family           = AF_INET;
      ia.sin_addr.s_addr      = ipAddr;
      ia.sin_port             = htons(uint16_t(port));
      socklen_t sz            = sizeof(ia);

      if (utxx::unlikely
        (bind(fd, reinterpret_cast<struct sockaddr const*>(&ia), sz) < 0))
      {
        // NB: "fd" will be closed here and an exception thrown:
        Clear();
        SocketError<true>
          (-1, fd, "IO::Bind: Cannot bind the INET socket to ", a_bind_addr,
           ':', a_bind_port);
      }
      // Successful "bind": Get the full local socket addr (IP and Port) back,
      // and memoise them:
      sz = sizeof(ia);
      if (utxx::unlikely
        (getsockname(fd, reinterpret_cast<struct sockaddr*>(&ia), &sz) < 0))
      {
        // Error handling similar to above:
        Clear();
        SocketError<true>(-1, fd, "IO::Bind: Failed to get bound IP and Port");
      }
      // If OK: Save the bind data:
      m_bound.m_inet = ia;
    }
    else
    {
      //---------------------------------------------------------------------//
      // UNIX Socket:                                                        //
      //---------------------------------------------------------------------//
      assert(m_domain == AF_UNIX);

      // In this case, the LocalAddr must be valid, and is interpreted as the
      // FileSystem Path. Again, we always allow re-use of that addr,  simply
      // by removing an existing socket. The Port is ignored:
      //
      if (utxx::unlikely(a_bind_addr == nullptr || *a_bind_addr == '\0'))
        throw utxx::badarg_error("IO::Bind: Invalid Addr/Port for UDS");

      int rc = unlink(a_bind_addr);
      if (utxx::unlikely(rc < 0 || errno != ENOENT))
        throw utxx::badarg_error("IO::Bind: Invalid UDS Path: ", a_bind_addr);

      // Now proceed with OS-level "bind":
      struct sockaddr_un         ua;
      ua.sun_family            = AF_UNIX;
      StrNCpy<true>(ua.sun_path, a_bind_addr);
      socklen_t sz             = sizeof(ua);

      if (utxx::unlikely
         (bind(fd, reinterpret_cast<struct sockaddr const*>(&ua), sz) < 0))
      {
        // Error handling similar to above:
        Clear();
        SocketError<true>
          (-1, fd, "IO::Bind: Cannot bind the UDS socket to ", a_bind_addr);
      }
      // Successful "bind": Get the full local UDS addr back, and memoise it:
      sz = sizeof(ua);
      if (utxx::unlikely
        (getsockname(fd, reinterpret_cast<struct sockaddr*>(&ua), &sz) < 0))
      {
        Clear();
        SocketError<true>(-1, fd, "IO::Bind: Failed to get bound UDS Path");
      }
      // If OK: Save the bind data:
      m_bound.m_unix = ua;
    }
    // All Done!
  }

  //=========================================================================//
  // "UDPRX" Default Ctor:                                                   //
  //=========================================================================//
  UDPRX::UDPRX()
  : m_fromAddr(),   // Zeroed-out
    m_iov     (),
    m_cmsg    (),
    m_mh      (),
    m_len     (0),
    m_ts      ()
  {
    // Initialise the "iovec" and "msghdr" for receiving UDP msgs. XXX: It is
    // currently  one common data structure for the whole Reactor, because re-
    // ceiving UDP msgs is "atomic":
    memset(&m_iov,  '\0', sizeof(m_iov));
    memset(&m_cmsg, '\0', sizeof(m_cmsg));
    memset(&m_mh,   '\0', sizeof(m_mh));

    constexpr unsigned IOVLen = 65536;  // Max size of a UDP dgram

    m_iov.iov_base      = new char[IOVLen];
    m_iov.iov_len       = IOVLen;
    memset(m_iov.iov_base, '\0', IOVLen);

    m_mh.msg_name       = &m_fromAddr;
    m_mh.msg_namelen    = sizeof(m_fromAddr);
    m_mh.msg_iov        = &m_iov;
    m_mh.msg_iovlen     = 1;
    m_mh.msg_control    = m_cmsg;
    m_mh.msg_controllen = sizeof(m_cmsg);
  };

  //=========================================================================//
  // "UDPRX" Dtor:                                                           //
  //=========================================================================//
  UDPRX::~UDPRX() noexcept
  {
    try
    {
      if (utxx::likely(m_iov.iov_base != nullptr))
      {
        delete[] static_cast<char*>(m_iov.iov_base);
        m_iov.iov_base = nullptr;
      }
    }
    // Exceptions must not propagate from the dtor:
    catch (...) {}
  }

  //=========================================================================//
  // "GetIP": DNS Resolution:                                                //
  //=========================================================================//
  // XXX: WARNING: This function may be BLOCKING!  It should be used during the
  // initialization phase ONLY, NOT in real-time operations! In addition, it is
  // quite inefficient (returns string):
  //
  string GetIP(char const* a_host_name)
  {
    assert(a_host_name != nullptr);
    char     buff[1024];
    hostent  he;
    hostent* res = nullptr;
    int      err = 0;
    // Still, use a GNU-specific thread-safe function:
    int rc = gethostbyname_r(a_host_name, &he, buff, sizeof(buff), &res, &err);

    // Check for errors:
    if (utxx::unlikely
       (rc != 0 || res != &he || he.h_length <= 0 || he.h_addrtype != AF_INET))
      throw utxx::badarg_error
            ("GetIP: HostName resolution failed: ", a_host_name, ": ",
             gai_strerror(err));

    // If OK: Get the 1st (XXX) addr returned;    NB: it's "in_addr",
    // NOT "in_addr_t"!
    char const*    addrc = he.h_addr_list[0];
    in_addr const* addri = reinterpret_cast<in_addr const*>(addrc);
    assert(addri != nullptr);

    // Return the addr as a string (XXX: the caller is likely to convert it
    // back into "in_addr"):
    return string(inet_ntoa(*addri));
  }

  //=========================================================================//
  // "EnableKernelTLS":                                                      //
  //=========================================================================//
  // May be invoked after TLS HandShaking has been done in user-space (by GNUTLS
  // library).  Enables kernel-level TLS support in the given FD,   and switches
  // off further use of (user-level) GNUTLS:
  //
  void EnableKernelTLS(FDInfo* a_info)
  {
    // GNUTLS must be enabled at the entry point. Currently, TLS is only avail-
    // able for INET TCP sockets:
    assert(a_info != nullptr                     &&
           a_info->m_tlsType == TLSTypeT::GNUTLS &&
           a_info->m_tlsSess != nullptr          &&
           a_info->m_fd   >= 0                   &&
           a_info->m_domain  == AF_INET          &&
           a_info->m_type == SOCK_STREAM);

    // Kernel CryptoInfo to be filled in:
    union KCryptoInfo
    {
      tls12_crypto_info_aes_gcm_128       m_AESG128;
      tls12_crypto_info_aes_gcm_256       m_AESG256;
      tls12_crypto_info_aes_ccm_128       m_AESC128;
      // The following is only available in very recent Kernel versions:
#     ifdef TLS_CIPHER_CHACHA20_POLY1305
      tls12_crypto_info_chacha20_poly1305 m_C20P1305;
#     endif
    };
    KCryptoInfo kcrInfo;

    // Ptrs to cypher-specific fields (to be set up below):
    tls_crypto_info*  ktci     = nullptr;
    unsigned short    kcType   = 0;
    unsigned char*    kiv      = nullptr;
    size_t            kivLen   = 0;
    unsigned char*    kkey     = nullptr;
    size_t            kkeyLen  = 0;
    unsigned char*    ksalt    = nullptr;
    size_t            ksaltLen = 0;
    unsigned char*    krseq    = nullptr;
    size_t            krseqLen = 0;
    size_t            kcrSz    = 0;

    // Get the active TLS Protocol Version from the curr session:
    gnutls_protocol_t protoVer =
      gnutls_protocol_get_version(a_info->m_tlsSess);

    // Get the Cipher Type from GNUTLS session:
    gnutls_cipher_algorithm_t cipher = gnutls_cipher_get(a_info->m_tlsSess);

    // Select the "kcrInfo" flds according to the Cipher:
    switch (cipher)
    {
      case GNUTLS_CIPHER_AES_128_GCM:
        ktci     = &kcrInfo.m_AESG128.info;
        kcType   =  TLS_CIPHER_AES_GCM_128;
        kiv      =  kcrInfo.m_AESG128.iv;
        kivLen   =  TLS_CIPHER_AES_GCM_128_IV_SIZE;
        kkey     =  kcrInfo.m_AESG128.key;
        kkeyLen  =  TLS_CIPHER_AES_GCM_128_KEY_SIZE;
        ksalt    =  kcrInfo.m_AESG128.salt;
        ksaltLen =  TLS_CIPHER_AES_GCM_128_SALT_SIZE;
        krseq    =  kcrInfo.m_AESG128.rec_seq;
        krseqLen =  TLS_CIPHER_AES_GCM_128_REC_SEQ_SIZE;
        kcrSz    =  sizeof(kcrInfo.m_AESG128);
        break;

      case GNUTLS_CIPHER_AES_256_GCM:
        ktci     = &kcrInfo.m_AESG256.info;
        kcType   =  TLS_CIPHER_AES_GCM_256;
        kiv      =  kcrInfo.m_AESG256.iv;
        kivLen   =  TLS_CIPHER_AES_GCM_256_IV_SIZE;
        kkey     =  kcrInfo.m_AESG256.key;
        kkeyLen  =  TLS_CIPHER_AES_GCM_256_KEY_SIZE;
        ksalt    =  kcrInfo.m_AESG256.salt;
        ksaltLen =  TLS_CIPHER_AES_GCM_256_SALT_SIZE;
        krseq    =  kcrInfo.m_AESG256.rec_seq;
        krseqLen =  TLS_CIPHER_AES_GCM_256_REC_SEQ_SIZE;
        kcrSz    =  sizeof(kcrInfo.m_AESG256);
        break;

      case GNUTLS_CIPHER_AES_128_CCM:
        ktci     = &kcrInfo.m_AESC128.info;
        kcType   =  TLS_CIPHER_AES_CCM_128;
        kiv      =  kcrInfo.m_AESC128.iv;
        kivLen   =  TLS_CIPHER_AES_CCM_128_IV_SIZE;
        kkey     =  kcrInfo.m_AESC128.key;
        kkeyLen  =  TLS_CIPHER_AES_CCM_128_KEY_SIZE;
        ksalt    =  kcrInfo.m_AESC128.salt;
        ksaltLen =  TLS_CIPHER_AES_CCM_128_SALT_SIZE;
        krseq    =  kcrInfo.m_AESC128.rec_seq;
        krseqLen =  TLS_CIPHER_AES_CCM_128_REC_SEQ_SIZE;
        kcrSz    =  sizeof(kcrInfo.m_AESC128);
        break;

      // The following is only available in very recent Kernel versions:
#     ifdef TLS_CIPHER_CHACHA20_POLY1305
      case GNUTLS_CIPHER_CHACHA20_POLY1305:
        ktci     = &kcrInfo.m_C20P1305.info;
        kcType   =  TLS_CIPHER_CHACHA20_POLY1305;
        kiv      =  kcrInfo.m_C20P1305.iv;
        kivLen   =  TLS_CIPHER_CHACHA20_POLY1305_IV_SIZE;
        kkey     =  kcrInfo.m_C20P1305.key;
        kkeyLen  =  TLS_CIPHER_CHACHA20_POLY1305_KEY_SIZE;
        ksalt    =  kcrInfo.m_C20P1305.salt;
        ksaltLen =  TLS_CIPHER_CHACHA20_POLY1305_SALT_SIZE;
        krseq    =  kcrInfo.m_C20P1305.rec_seq;
        krseqLen =  TLS_CIPHER_CHACHA20_POLY1305_REC_SEQ_SIZE;
        kcrSz    =  sizeof(kcrInfo.m_C20P1305);
        break;
#     endif

      default:
        throw utxx::runtime_error
              ("EnableKernelTLS: UnSupported GNUTLS Cipher: ", cipher);
    }
    assert(ktci     != nullptr && kcType   != 0       && kiv     != nullptr &&
           kivLen   != 0       && kkey     != nullptr && kkeyLen != 0       &&
           ksalt    != nullptr && ksaltLen != 0       && krseq   != nullptr &&
           krseqLen != 0       && kcrSz    != 0);

    // Enable (in principle) the Kernel TLS support on the socket:
    char const TLS[4] = "tls";
    int  rc = setsockopt(a_info->m_fd, SOL_TCP, TCP_ULP, TLS, sizeof(TLS));
    if (utxx::unlikely(rc < 0))
      SystemError(-1, "EnableKernelTLS: setsockopt(TCP_ULP) failed");

    // Set the TLS Version (mapping it from GNUTLS to Linux Kernel TLS):
    ktci->version =
      (protoVer == GNUTLS_TLS1_2)
      ? TLS_1_2_VERSION :
      (protoVer == GNUTLS_TLS1_3)
      ? TLS_1_3_VERSION
      : throw utxx::runtime_error
              ("EnableKernelTLS: UnSupported GNUTLS Version: ", protoVer);

    // Set the Cipher Type (again, mapping it from GNUTLS to Linux Kernel TLS):
    ktci->cipher_type = kcType;

    // Get the Session Key components and set the Kernel TLS CryptoInfo (separ-
    // ately for TX and RX):
    for (unsigned isRX = 0; isRX < 2; ++isRX)
    {
      gnutls_datum_t dummy      {nullptr, 0};
      gnutls_datum_t iv         {nullptr, 0};
      gnutls_datum_t cypherKey  {nullptr, 0};
      unsigned char  seqNum[8]  {0};

      rc = gnutls_record_get_state
           (a_info->m_tlsSess, isRX, &dummy, &iv, &cypherKey, seqNum);
      if (utxx::unlikely(rc < 0))
        SystemError(-1,  "EnableKernelTLS(gnutls_record_get_state)");

      // Kernel RSeq <-- GNUTLS SeqNum:
      assert(krseqLen == sizeof(seqNum));
      memcpy(krseq, seqNum, krseqLen);

      // Kernel Key  <-- GNUTLS Key:
      assert(kkeyLen  == cypherKey.size);
      memcpy(kkey,  cypherKey.data, kkeyLen);

      if (protoVer == GNUTLS_TLS1_2)
      {
        // Kernel Salt (Implicit IV) <-- GNUTLS IV
        // Kernel Explicit IV        <-- GNUTLS SeqNum (as well)
        assert(ksaltLen == iv.size && kivLen == sizeof(seqNum));
        memcpy(ksalt, iv.data,  ksaltLen);
        memcpy(kiv,   seqNum,   kivLen  );
      }
      else
      {
        assert(protoVer == GNUTLS_TLS1_3 && ksaltLen + kivLen == iv.size);
        // Kernel Salt (Implicit IV) + Kernel Explicit IV <-- GNUTLS IV:
        memcpy(ksalt, iv.data,  ksaltLen);
        memcpy(kiv,   iv.data + ksaltLen,   kivLen);
      }

      // Set the Kernel TLS params:
      rc = setsockopt(a_info->m_fd, SOL_TLS, bool(isRX) ? TLS_RX : TLS_TX,
                      &kcrInfo,     socklen_t(kcrSz));
      if (utxx::unlikely(rc < 0))
        SystemError(-1, "EnableKernelTLS: setsockopt(TLS) failed");

      // XXX: Should we de-allocate the tmp GNUTLS data to prevent memory leaks?
      // There is apparently no standard way of doing that...
    }

    // Finally, once Kernel TLS has been initialized successfully, disable
    // further use of GNUTLS on this socket:
    gnutls_deinit(a_info->m_tlsSess);
    a_info->m_tlsSess = nullptr;
    a_info->m_tlsType = TLSTypeT::KernelTLS;
  }

  //=========================================================================//
  // Non-Blocking Network IO Utils:                                          //
  //=========================================================================//
  //=========================================================================//
  // "SetBlocking":                                                          //
  //=========================================================================//
  // Set/clear blocking mode on a given socket file descriptor. Returns old
  // value of the blocking attribute:
  //
  template<bool Set>
  inline bool SetBlocking(int a_fd)
  {
    // Get the current mode:
    int v = fcntl(a_fd, F_GETFL, 0);

    CHECK_ONLY
    (
      if (utxx::unlikely(v < 0))
        SystemError(-1, "SetBlocking: fcntl(F_GETFL)");
    )
    bool wasBlocking = !(v & O_NONBLOCK);

    // Actually Set or Clear the Blocking mode, if what we got is not what we
    // want:
    if (wasBlocking != Set)
    {
      CHECK_ONLY(int rc =)
        fcntl(a_fd, F_SETFL, Set ? (v & (~O_NONBLOCK)) : (v | O_NONBLOCK));

      CHECK_ONLY
      (
        if (utxx::unlikely(rc < 0))
          SystemError(-1, "SetBlocking: fcntl(F_SETFL)");
      )
    }
    // Return the now-previous mode:
    return wasBlocking;
  }

  //-------------------------------------------------------------------------//
  // "SetBlocking" Instances:                                                //
  //-------------------------------------------------------------------------//
  template bool SetBlocking<false>(int a_fd);
  template bool SetBlocking<true> (int a_fd);

  //=========================================================================//
  // MultiCast Mgmt Utils:                                                   //
  //=========================================================================//
  //=========================================================================//
  // "MCastManage":                                                          //
  //========================================================================-//
  // An existing FD is attached to, or detached from, a multicast feed:
  // NB: If   an FD was created by "MCastSubscribe" above, then closing the FD
  //  may not cause the multicast routing to be stopped, so the UDP feed would
  //  continue to be routed to the curr host and received by the kernel,   and
  //  then dropped. So invoke MCastManage<false>() before closing the socket:
  //
  template<bool Join>
  inline void MCastManage
  (
    int         a_fd,
    char const* a_group_ip,
    char const* a_iface_ip,
    char const* a_src_ip
  )
  {
    CHECK_ONLY
    (
      if (utxx::unlikely
         (a_fd < 0 || a_group_ip == nullptr || *a_group_ip == '\0'))
        throw utxx::badarg_error("MCastManage: Invalid FD and/or GroupIP");
    )
    assert(a_fd >= 0 && a_group_ip != nullptr);
    in_addr_t groupAddr = inet_addr(a_group_ip);     // Group IP

    CHECK_ONLY
    (
      if (utxx::unlikely(groupAddr == INADDR_NONE))
        throw utxx::badarg_error("MCastManage: Invalid GroupIP=", a_group_ip);
    )
    // Is it a "classical" milticast or SSM? Nowadays, the latter is common:
    bool useSSM = (a_src_ip != nullptr && (*a_src_ip) != '\0');

    // Have we got the Interface IP? We may or may not be able to do without
    // it (in particular, it is normally required under LibVMA):
    in_addr_t ifaceAddr =
      (a_iface_ip != nullptr && *a_iface_ip != '\0')
      ? inet_addr(a_iface_ip)
      : INADDR_ANY;

    CHECK_ONLY
    (
      if (utxx::unlikely(ifaceAddr == INADDR_NONE))
      {
        assert(a_iface_ip != nullptr);
        throw utxx::badarg_error("MCastManage: Invalid IFaceIP=", a_iface_ip);
      }
    )
    if (utxx::likely(useSSM))
    {
      //---------------------------------------------------------------------//
      // Join / Leave an SSM Group:                                          //
      //---------------------------------------------------------------------//
      in_addr_t srcAddr = inet_addr(a_src_ip);  // Src IP

      CHECK_ONLY
      (
        if (utxx::unlikely(srcAddr == INADDR_NONE))
          throw utxx::badarg_error("MCastManage: Invalid SrcIP=", a_src_ip);
      )
      ip_mreq_source mreqs;
      mreqs.imr_multiaddr .s_addr = groupAddr;
      mreqs.imr_sourceaddr.s_addr = srcAddr;
      mreqs.imr_interface .s_addr = ifaceAddr;

      if (utxx::unlikely
         (setsockopt
          (a_fd,
           IPPROTO_IP,
           Join ? IP_ADD_SOURCE_MEMBERSHIP : IP_DROP_SOURCE_MEMBERSHIP,
           &mreqs,
           sizeof(mreqs)) < 0))
        // Propagate an exception but do NOT close "a_fd" here, because its
        // life-cycle is managed by the Caller:
        SocketError<false>
          (-1, a_fd, "MCastManage: Cannot ",     (Join ? "Join" : "Leave"),
                     " the SSM Group: GroupIP=", a_group_ip,    ", SrcIP=",
                     a_src_ip);
    }
    else
    {
      //---------------------------------------------------------------------//
      // Join / Leave a "classical" Multicast Group:                         //
      //---------------------------------------------------------------------//
      // For the mcast-listening interface, is seems to be OK to specify ANY IP
      // -- though XXX this is not obvious:
      //
      ip_mreq  mreq;
      mreq.imr_multiaddr.s_addr = groupAddr;
      mreq.imr_interface.s_addr = ifaceAddr;

      if (utxx::unlikely
         (setsockopt
          (a_fd,
           IPPROTO_IP,
           Join ? IP_ADD_MEMBERSHIP : IP_DROP_MEMBERSHIP,
           &mreq,
           sizeof(mreq)) < 0))
        // Again, propagate an exception but do NOT close "a_fd" here:
        SocketError<false>
          (-1, a_fd, "MCastManage: Cannot ", (Join ? "Join" : "Leave"),
                     " the MCast Group: GroupIP=",         a_group_ip);
    }
  }

  //-------------------------------------------------------------------------//
  // "MCastManage" Instances:                                                //
  //-------------------------------------------------------------------------//
  template void MCastManage<false>
  (
    int         a_fd,
    char const* a_group_ip,
    char const* a_iface_ip,
    char const* a_src_ip
  );

  template void MCastManage<true>
  (
    int         a_fd,
    char const* a_group_ip,
    char const* a_iface_ip,
    char const* a_src_ip
  );

  //=========================================================================//
  // "MkDGramSock":                                                          //
  //=========================================================================//
  pair<int,int> MkDGramSock
  (
    char const* a_local_addr,  // IP (INET) or Path (UNIX); may be NULL or ""
    int         a_local_port,  // May be invalid (< 0)
    char const* a_remote_addr, // IP (INET) or Path (UNIX); may be NULL or ""
    int         a_remote_port  // May be invalid (< 0)
  )
  {
    //-----------------------------------------------------------------------//
    // Checks:                                                               //
    //-----------------------------------------------------------------------//
    in_addr_t inetLAddr  = INADDR_NONE;
    bool      validLAddr = (a_local_addr != nullptr && *a_local_addr != '\0');
    if (validLAddr)
      inetLAddr = inet_addr(a_local_addr);

    // So: UNIX or INET?
    int domain =
      (validLAddr && inetLAddr == INADDR_NONE && a_local_port < 0)
      ? AF_UNIX
      : AF_INET;

    // Create a DataGram socket in that Domain; make it non-blocking from the
    // beginning:
    int fd = socket(domain, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    CHECK_ONLY
    (
      if (utxx::unlikely(fd < 0))
        SystemError(-1, "MkDGramSock: Cannot create the socket");
    )
    //-----------------------------------------------------------------------//
    // Enable receiving of nano-second TimeStamps from this socket:          //
    //-----------------------------------------------------------------------//
    if (domain == AF_INET)
    {
      int yes = 1;
      if (utxx::unlikely
         (setsockopt(fd, SOL_SOCKET, SO_TIMESTAMPNS, &yes, sizeof(yes)) < 0))
        // Again, here "fd" will be closed on error:
        SocketError<true>
          (-1, fd, "MkDGramSock: setsockopt(SO_TIMESTAMPNS) failed");
    }
    //-----------------------------------------------------------------------//
    // Perform a notional "connect" (if Remote{Addr/Port} is valid):         //
    //-----------------------------------------------------------------------//
    if (domain == AF_INET      && a_remote_addr != nullptr &&
        *a_remote_addr != '\0' && a_remote_port > 0)
    {
      // For the Connect Addr, we require fully-valid IP and Port:
      in_addr_t inetRAddr  = inet_addr(a_remote_addr);

      if (inetRAddr != INADDR_NONE && inetRAddr != INADDR_ANY)
      {
        // Proceed with OS-level "connect":
        sockaddr_in              ia;
        ia.sin_family          = AF_INET;
        ia.sin_addr.s_addr     = inetRAddr;
        ia.sin_port            = htons(uint16_t(a_remote_port));
        socklen_t sz           = sizeof(ia);

        if (utxx::unlikely
           (connect(fd, reinterpret_cast<struct sockaddr const*>(&ia), sz) < 0))
          // NB: Again, "fd" will be closed here:
          SocketError<true>(-1, fd,
            "MkDGramSock: Cannot connect the INET socket to ", a_remote_addr,
            ':', a_remote_port);
      }
    }
    else
    if (domain == AF_UNIX      && a_remote_addr !=nullptr &&
        *a_remote_addr != '\0')
    {
      struct sockaddr_un         ua;
      ua.sun_family            = AF_UNIX;
      StrNCpy<true>(ua.sun_path, a_remote_addr);
      socklen_t sz             = sizeof(ua);

      if (utxx::unlikely
         (connect(fd, reinterpret_cast<struct sockaddr const*>(&ua), sz) < 0))
        // NB: Again, "fd" will be closed here:
        SocketError<true>(-1, fd,
          "MkDGramSock: Cannot connect the UNIX socket to ", a_remote_addr);
    }
    //-----------------------------------------------------------------------//
    // All Done:                                                             //
    //-----------------------------------------------------------------------//
    assert(fd >= 0);
    return make_pair(fd, domain);
  }

  //=========================================================================//
  // File IO Utils:                                                          //
  //=========================================================================//
  //=========================================================================//
  // "FileExists":                                                           //
  //=========================================================================//
  // NB: "access_flags" are eg "F_OK" as defined in "unistd.h":
  //
  bool FileExists(char const* a_file_path, int a_access_flags)
  {
    assert (a_file_path != nullptr);

    // First, use "access" with the flags specified to verify that the path
    // exists:
    if (access(a_file_path, a_access_flags) < 0)
      return false;

    // Then, use "fstat" to check that this is a regular file (eg not a direc-
    // tory or special file; symlinks are resolved transitively):
    struct stat s;
    if (stat(a_file_path, &s) < 0)
      return false;
    return S_ISREG(s.st_mode);
  }

  //=========================================================================//
  // Thread Scheduling Support (XXX: Not really IO...):                      //
  //=========================================================================//
  //=========================================================================//
  // "SetCPUAffinity":                                                       //
  //=========================================================================//
  void SetCPUAffinity(int a_cpu_n)
  {
    if (a_cpu_n < 0)
      return; // De-configured, do not produce an error

    cpu_set_t CSet;
    CPU_ZERO(&CSet);
    CPU_SET(a_cpu_n, &CSet);

    CHECK_ONLY(int rc =) sched_setaffinity(0, sizeof(CSet), &CSet);
    CHECK_ONLY
    (
      if (utxx::unlikely(rc < 0))
        SystemError(-1, "SetCPUAffinity failed");
    )
  }

  //=========================================================================//
  // "SetRTPriority": Only works in privileged mode:                         //
  //=========================================================================//
  // NB: 99 is the highest level for user-level processes (and 100 for in-ker-
  // nel processes); 0 is the lowest level (non-RT, actually):
  //
  void SetRTPriority(int a_prio)
  {
    sched_param sch;
    sch.sched_priority = a_prio;

    CHECK_ONLY(int rc =) sched_setscheduler(getpid(), SCHED_FIFO, &sch);
    CHECK_ONLY
    (
      if (utxx::unlikely(rc < 0))
        SystemError(-1, "SetRTPriority failed");
    )
  }

  //=========================================================================//
  // Logger Factory:                                                         //
  //=========================================================================//
  shared_ptr<spdlog::logger> MkLogger
  (
    string const&  a_logger_name,
    string const&  a_log_output,
    bool           a_with_mt,
    long           a_log_file_size,
    int            a_rotations
  )
  {
    CHECK_ONLY
    (
      // Check the args (LogFileSize and Rotations can be 0 for special files --
      // in those cases, they are ignored anyway):
      if (utxx::unlikely(a_logger_name.empty() || a_log_output.empty() ||
                         a_log_file_size < 0   || a_rotations < 0))
        throw utxx::badarg_error("MkLogger: Invalid arg(s)");
    )
    // Generic Case:
    string logLC = a_log_output;
    boost::algorithm::to_lower(logLC);

    // XXX: Unfortunately, in the curreny version of "spdlog" (1.1.0), it appe-
    // ars to be no longer possible to invoke  a custom initizlization function
    // on async worker thread in the thread pool. Previously, this func was us-
    // ed to set CPU Affinity of logger threads, in order not to interfere with
    // the main thread. FIXME: This might be a serious real-time issue:
    try
    {
      std::shared_ptr<spdlog::logger> retLogger;
      retLogger =
        (logLC == "stderr")
        ? (a_with_mt ? spdlog::stderr_logger_mt(a_logger_name)
                     : spdlog::stderr_logger_st(a_logger_name))
        :
        (logLC == "stdout")
        ? (a_with_mt ? spdlog::stdout_logger_mt(a_logger_name)
                     : spdlog::stdout_logger_st(a_logger_name))
        :
        // Assume that "logOutput" is a file path; will use Async and NonBlock
        // logging (overrun log items to be discarded):
        (a_with_mt
         ? spdlog::rotating_logger_mt<spdlog::async_factory_nonblock>
            (a_logger_name,           a_log_output,
             size_t(a_log_file_size), size_t(a_rotations))
         : spdlog::rotating_logger_st<spdlog::async_factory_nonblock>
            (a_logger_name,           a_log_output,
             size_t(a_log_file_size), size_t(a_rotations))
        );
      retLogger->set_pattern("%+", spdlog::pattern_time_type::utc);

      return retLogger;
    }
    catch (exception const& exc)
    {
      throw utxx::runtime_error
            ("MkLogger: Cannot Create: Name=", a_logger_name,   ", Output=",
             a_log_output, ",  Size=",         a_log_file_size, ", Rotations=",
             a_rotations,  ": ", exc.what());
    }
  }
} // End namespace IO
} // End namespace MAQUETTE
