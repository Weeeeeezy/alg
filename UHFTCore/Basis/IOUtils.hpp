// vim:ts=2:et
//===========================================================================//
//                            "Basis/IOUtils.hpp":                           //
//                Socket and File IO Utils, and Misc OS Stuff                //
//===========================================================================//
#pragma  once

#include "Basis/IOUtils.h"
#include "Basis/Macros.h"
#include "Basis/Maths.hpp"
#include "Basis/BaseTypes.hpp"
#include <utxx/compiler_hints.hpp>
#include <utxx/buffer.hpp>
#include <utxx/time_val.hpp>
#include <utxx/error.hpp>
#include <utxx/string.hpp>
#include <stdexcept>
#include <system_error>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <csignal>
#include <type_traits>
#include <fcntl.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sched.h>
#include <sys/mman.h>
#include <linux/tls.h>

namespace MAQUETTE
{
namespace IO
{
  //=========================================================================//
  // Global Initialisation Flag:                                             //
  //=========================================================================//
  extern bool Inited;

  //=========================================================================//
  // "FDInfo" Dtor:                                                          //
  //=========================================================================//
  inline FDInfo::~FDInfo() noexcept
  {
    // Do not allow any exceptions to come out from the Dtor:
    try { Clear(); } catch(...){}
  }

  //=========================================================================//
  // Error Propagation:                                                      //
  //=========================================================================//
  //=========================================================================//
  // "SystemError":                                                          //
  //=========================================================================//
  // NB: Use a_err=-1 if not known exactly, then errno will actually be used:
  //
  template<typename... ErrMsgArgs>
  [[noreturn]] inline void SystemError
    (int a_err, ErrMsgArgs const&... a_emas)
  {
    // If "a_err" is valid, it is used instead of the current "errno":
    int    error = (a_err >= 0) ? a_err : errno;
    throw std::system_error
         (std::error_code(error, std::generic_category()),
          utxx::to_string(a_emas...));
  }

  //=========================================================================//
  // "GetSocketError":                                                       //
  //=========================================================================//
  inline int GetSocketError(int a_fd)
  {
    // Try to extract the error code explicitly rather  than use the previous
    // "errno"; but if "getsockopt" fails or returns 0, the save "errno" (NOT
    // the one from "getsockopt" invocation!) will be used:
    int saved_errno  = errno;
    int       error  = -1;
    socklen_t errlen = sizeof(error);

    int       res    =
      (getsockopt(a_fd, SOL_SOCKET, SO_ERROR, &error, &errlen) == 0
       && error > 0)
      ? error
      : saved_errno;
    assert(res >= 0);
    return res;
  }

  //=========================================================================//
  // "SocketError":                                                          //
  //=========================================================================//
  // (*) If the 1st arg (error code) is >= 0, it will be used; otherwise, the
  //     error code is determined by "GetSocketError";
  // (*) "DoClose" is only set for locally-created sockets (eg those returned
  //     by "accept"); it must NOT be applied to those sockets whose life-time
  //     is managed elsewhere;
  // (*) XXX: "a_fd" is a non-const REF, which is normally avoided in our API:
  //
  template<bool DoClose, typename... ErrMsgArgs>
  [[noreturn]] inline void SocketError
    (int a_err, int& a_fd, ErrMsgArgs const&... a_emas)
  {
    // Get the error code:
    int error = (a_err >= 0) ? a_err : GetSocketError(a_fd);

    if (DoClose)
    {
      (void) close(a_fd);
      a_fd = -1;
    }
    // Throw an exception:
    auto   error_code = std::error_code(error, std::generic_category());
    throw  std::system_error(error_code, utxx::to_string(a_emas...));
  }

  //=========================================================================//
  // Non-Blocking Socket IO:                                                 //
  //=========================================================================//
  //=========================================================================//
  // "GetRXTime":                                                            //
  //=========================================================================//
  inline utxx::time_val GetRXTime(msghdr* a_mh)
  {
    assert(a_mh->msg_control != nullptr && a_mh->msg_controllen > 0);

    // XXX: CMSG_FIRSTHDR casts 0 into nullptr: suppress the CLang warning:
#   ifdef  __clang__
#   pragma clang diagnostic push
#   pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"
#   endif
    for (cmsghdr* cm = CMSG_FIRSTHDR(a_mh); cm != nullptr;
                  cm = CMSG_NXTHDR(  a_mh,  cm))
#   ifdef  __clang__
#   pragma clang diagnostic pop
#   endif
      // NB: We assume that TimeStamps were requested with nano-second resoltn:
      if (cm->cmsg_level == SOL_SOCKET && cm->cmsg_type == SO_TIMESTAMPNS)
      {
        timespec const* ts = reinterpret_cast<timespec const*>(CMSG_DATA(cm));
        assert(ts != nullptr);
        return utxx::time_val(*ts);
      }
    // XXX: If we got here: The nano-second TimeStamp was not found, so we will
    // have to get the curr time instead -- this is HIGHLY UNDESIRABLE:
    return utxx::now_utc();
  }

  //=========================================================================//
  // "ReadUntilEAgain":                                                      //
  //=========================================================================//
  // NB: This version is for STREAM (TCP-like) sockets and special FDs (eg Tim-
  // erFD, SignalFD, EventFD); "IsSocket" param tells whether "a_fd" is really
  // a socket.
  // Fill in a buffer from a non-blocking FD (eg a socket) while data are avai-
  // lable, and invoke a user-provided generic action which consumes that data:
  //
  // Action:  (char const*    data,
  //           int            data_sz,
  //           utxx::time_val read_ts) -> int
  // Action Return Value:     n_bytes_consumed
  //                          (>= 0 to continue, < 0 to stop immediately)
  //
  // OnError: (int ret, int errno) -> void
  //                          (in most cases, it should NOT throw exceptions)
  //
  // NB: Action can scan the buffer, but does not update it; "read" (actually
  // meaning "pop") and "crunch" on the buffer,  are performed by "ReadUntil-
  // EAgain" itself:
  // XXX: Unlike "RecvUntilEAgain" (see below),  this  function currently does
  // NOT invoke the Action with (NULL,0) args when EAgain is encountered. This
  // is because for Stream sockets, the chunk boundaries normally do not coin-
  // cide with logical msg boundaries, so EAgain event carries no useful info:
  //
  template<bool IsSocket, TLSTypeT TLST, typename Action, typename ErrHandler>
  inline void ReadUntilEAgain
  (
    FDInfo*           a_info,
    Action     const& a_action,
    ErrHandler const& a_on_error,
    char       const* a_where
  )
  {
    //-----------------------------------------------------------------------//
    // Checks:                                                               //
    //-----------------------------------------------------------------------//
    // If TLS is in use, it must be a Socket of course:
    static_assert(TLST == TLSTypeT::None || IsSocket,
                  "ReadUntilEAgain: TLS is for Sockets only");

    assert(a_info != nullptr && a_where != nullptr);

    constexpr bool           UseGNUTLS = (TLST == TLSTypeT::GNUTLS);
    int                      fd        = a_info->m_fd;
    DEBUG_ONLY(uint64_t      iid       = a_info->m_inst_id;)
    utxx::dynamic_io_buffer* rdBuff    = a_info->m_rd_buff;
    gnutls_session_t         tlsSess   = a_info->m_tlsSess;

    // Initially, all params must be valid:
    assert(fd >= 0 && iid != 0 && rdBuff != nullptr &&
          (TLST == TLSTypeT::GNUTLS) == (tlsSess != nullptr));

    // IOVec is only required for KernelTLS mode:
    iovec* iov =
      (TLST == TLSTypeT::KernelTLS)
      ? a_info->m_msghdr.msg_iov
      : nullptr;

    //-----------------------------------------------------------------------//
    // The RX Loop:                                                          //
    //-----------------------------------------------------------------------//
    int ns = 0;  // Total bytes read in this loop

    while (true)
    {
      size_t space = rdBuff->capacity();    // This is a remaining capacity!

      if (utxx::unlikely(space == 0))
        throw utxx::runtime_error
              ("ReadUntilEAgain(", a_where, "): BufferOverFlow1: FD=", fd);

      char* into = rdBuff->wr_ptr();
      int   n    = 0;

      //---------------------------------------------------------------------//
      // Inner Reading Loop:                                                 //
      //---------------------------------------------------------------------//
      // Skip EINTR events -- we must continue until EAGAIN is encountered, not
      // EINTR, or we will lose an Edge-Triggered event. In the GNUTLS mode, use
      // a similar GNUTLS_E_INTERRUPTED flag:
      // XXX:
      // (*) Should we use "recv" or "read" here? We use the latter because it
      //     is applicable to both sockets, files (but then  "ReadUntilEAgain"
      //     is not really usable -- we would only get EAGAIN at EOF) and also
      //     for TimeFDs/SignalFDs/EventFDs;  but does it incur  any  overhead
      //     compared to "recv"?
      // (*) For KernelTLS, we have to use "recvmsg" to handle CtlMsgs;
      // (*) For Stream sockets, kernel time-stamping is not available -- so we
      //     conservatively record the time BEFORE going into "read":
      //
      bool tryAgain = false;
      do
      {
        switch (TLST)
        {
        case TLSTypeT::None:
          n        = int(read(fd, into, space));
          tryAgain = (n < 0 && errno == EINTR);
          break;

        case TLSTypeT::GNUTLS:
          n        = int(gnutls_record_recv(tlsSess, into, space));
          tryAgain = (n == GNUTLS_E_INTERRUPTED);
          break;

        case TLSTypeT::KernelTLS:
          // Point "iov" to the current (into, space) and do "recvmsg":
          assert(iov != nullptr);
          iov->iov_base = into;
          iov->iov_len  = space;
          // And set ControlLen to the max possible size again:
          a_info->m_msghdr.msg_controllen = sizeof(a_info->m_cmsg);

          n        = int(recvmsg(fd, &(a_info->m_msghdr), 0));
          tryAgain = (n < 0 && errno == EINTR);

          // Check whether we got a ControlMsg (or just ApplData); we assume
          // that no more than 1 ControlMsg can be received at once:
          {
            cmsghdr const* cmsg =  CMSG_FIRSTHDR(&(a_info->m_msghdr));

            if (cmsg != nullptr && cmsg->cmsg_level == SOL_TLS &&
                cmsg->cmsg_type == TLS_GET_RECORD_TYPE)
            {
              // If this is a CtlMsg, not ApplData, we try again, overwriting
              // the former:
              unsigned char rtype  = *(CMSG_DATA(cmsg));
              tryAgain            |= (rtype != 23);
            }
          }
          break;

        default:
          assert(false);
        }
      }
      while (utxx::unlikely(tryAgain));

      //---------------------------------------------------------------------//
      // OK, got some data (or an error):                                    //
      //---------------------------------------------------------------------//
      if (!UseGNUTLS ? (n < 0 && errno == EAGAIN) : (n == GNUTLS_E_AGAIN))
        // No more data are available -- this is a normal end of reading in the
        // non-blocking mode:
        break;
      else
      if (utxx::unlikely(n <= 0))
      {
        // Any other error; in particular, n==0 means that there is no error in
        // the strict sense, but the other side had disconnected. Still, we re-
        // gard this as an error condition: Invoke the ErrHandler; in the GNUTLS
        // mode, the error code is "n" itself:
        //
        int ec = !UseGNUTLS ? (IsSocket ? GetSocketError(fd) : errno) : n;
        a_on_error(n, ec);

        // No point in going further:
        break;
      }
      else
      if (utxx::unlikely(n == int(space)))
        // We have reached the end of the buffer space, so most likely, not all
        // available data have been read in. XXX: This is currently an unrecov-
        // erable error, as it would confuse the non-blocking read mechanism:
        throw utxx::runtime_error
              ("ReadUntilEAgain(", a_where, "): BufferOverFlow2: FD=", fd);

      //---------------------------------------------------------------------//
      // GENERIC CASE: Successful Read: Put the data into the Buffer:        //
      //---------------------------------------------------------------------//
      // Advance the buffer internal counter by "n":
      //
      assert(0 < n && n < int(space));
      rdBuff->commit(n);
      ns += n;

      // And try more reads, until EAGAIN is encountered!
    }
    //-----------------------------------------------------------------------//
    // RX Done:                                                              //
    //-----------------------------------------------------------------------//
    // If no data has been committed to the buffer, there is nothing to do in
    // this invocation (even if the buffer contains some data from prev invo-
    // cations: since they are unconsumed yet, they are incomplete):
    //
    if (utxx::unlikely(ns == 0))
      return;

    // Otherwise, invoke the user action on the cumulative data located in the
    // buffer: Because this is a Stream socket, the user call-back must receive
    // the whole buffered data, but may consume any part of it:
    // XXX: Any exceptions thrown by "a_action" are NOT handled here, at least
    // because in that case we don't know the state of the Buffer (ie the val-
    // ue of "consumed" and whether "rdBuff" would still exist after returning
    // from the "a_on_error" handler):
    //
    int consumed =
      a_action(rdBuff->rd_ptr(), int(rdBuff->size()), utxx::now_utc());

    // NB:
    // (*) consumed==0 is also perfectly legitimate, but there is nothing to
    //     do in that case;
    // (*) as a result of "Action", the FDInfo may have been destroyed  (via
    //     "Remove");  but in that case, "Action" must return (-1), so check
    //     the FDInfo validity (XXX: using FD and InstID flds only):
    //
    assert(consumed < 0 || (a_info->m_fd == fd && a_info->m_inst_id == iid));

    if (utxx::unlikely(consumed < 0))
      // This is an indication that an immediate exit is required; XXX:   in
      // that case, we do not even care about the state of the buffer -- and
      // BEWARE, it may have already been de-allocated by "a_action", so  do
      // NOT try to "reset" it!
      return;
    else
    if (utxx::likely(consumed > 0))
      // Generic Case:
      // May need to move the rest of data to the buffer beginning. So if the
      // buffer was empty before this function is invoked  (as would normally
      // be the case for DataGram type), crunching the buffer has no effect --
      // it will be empty again:
      rdBuff->read_and_crunch(consumed);

    // All Done!
  }

  //=========================================================================//
  // "RecvUntilEAgain":                                                      //
  //=========================================================================//
  // NB: This function is for DataGram (UDP) sockets:
  //
  // Args are similar to those for "ReadUntilEAgain", except that:
  // Action (NB: also invoked with empty args on EAgain -- the caller may want
  //         to process end-of-physical-chunk events):
  //
  //     (char const*    data,      // NULL  on EAgain
  //      int            data_sz,   // 0     on EAgain
  //      utxx::time_val recv_ts,   // Empty on EAgain
  //      IUAddr const*  from_addr  // NULL  on EAgain
  //     ) -> bool                  // (true: continue; false: stop now)
  //
  template<typename Action, typename ErrHandler>
  inline void RecvUntilEAgain
  (
    FDInfo     const& a_info,
    UDPRX*            a_udprxs,
    int               a_nu,         // #(UDPRXs)
    Action     const& a_action,
    ErrHandler const& a_on_error,
    char       const* a_where
  )
  {
    //-----------------------------------------------------------------------//
    // Checks:                                                               //
    //-----------------------------------------------------------------------//
    // We will be using "recvmsg", rather than "recv" or "recvfrom", primarily
    // for the purpose of extracting the TimeStamps of incoming UDP DataGrams:
    assert(a_where != nullptr && a_udprxs != nullptr && a_nu > 0);

    int                 fd  = a_info.m_fd;
    DEBUG_ONLY(uint64_t iid = a_info.m_inst_id;)
    assert(fd >= 0 && iid != 0);

    //-----------------------------------------------------------------------//
    // The RX Loop:                                                          //
    //-----------------------------------------------------------------------//
    // XXX: There is a limit on the max number of UDP msgs we can receive at
    // once,  due to a limited number of "UDPRX" buffers:
    //
    int nrx = 0;
    for (; nrx < a_nu; ++nrx)
    {
      // The structures used to receive the next UDP msg:
      UDPRX*  udprx = a_udprxs + nrx;
      msghdr* mh    = &(udprx->m_mh);

      assert(mh->msg_iov     != nullptr && mh->msg_iovlen == 1  &&
             mh->msg_control != nullptr && mh->msg_controllen > 0);

      iovec& iov    = mh->msg_iov[0];
      int    buffSz = int(iov.iov_len);
      assert(buffSz > 0);

      udprx->m_len  = 0;
      udprx->m_ts   = utxx::time_val(); // Initially empty

      // Skip EINTR events -- we must continue until EAGAIN is encountered, not
      // EINTR, otherwise we will lose an Edge-Triggered event:
      int n = 0;
      do
        n = int(recvmsg(fd, mh, 0));
      while
        (utxx::unlikely(n < 0 && errno == EINTR));

      if (n < 0 && errno == EAGAIN)
        // No more data are available -- this is a normal end of reading in the
        // non-blocking mode:
        break;
      else
      if (utxx::unlikely(n <= 0))
      {
        // Any other error; in particular, ec==0 means that there is no  error
        // in the strict sense, but the other side had disconnected. Still, we
        // regard this as an error condition: Invoke the ErrHandler:
        int  ec = GetSocketError(fd);
        a_on_error(n, ec);

        // No point in going further:
        break;
      }
      // Otherwise: Nominally-successful "recv":
      assert(0 < n && n <= buffSz);

      // Since it is a DataGram socket, if the buffer is now full, it is likely
      // that the last datagram was truncated -- this is an error:
      //
      if (utxx::unlikely(n == buffSz))
        throw utxx::runtime_error
              ("RecvUntilEAgain(", a_where, "): Buffer OverFlow: FD=", fd);

      // OK, now we know that the recv was truly successful. Extract the Time
      // Stamp from the control msg and record the actual data size:
      udprx->m_len = n;
      udprx->m_ts  = GetRXTime(mh);

      // Increment "nrx" and continue until EAGAIN is encountered!
    }
    //-----------------------------------------------------------------------//
    // RX Done: Process the data read:                                       //
    //-----------------------------------------------------------------------//
    // "nrx" is the number of datagrams read.  However, if (nrx==a_nu), we have
    // exited the RX loop by reaching the limit, so reading may not be complete:
    //
    if (utxx::unlikely(nrx == a_nu))
      throw utxx::runtime_error("RecvUntilEAgain: End of UDPRX Buffers!");

    // Process what we got (or do nothing if no UDP msgs have been received):
    for (int i = 0; i < nrx; ++i)
    {
      UDPRX*  udprx     = a_udprxs + nrx;
      msghdr* mh        = &(udprx->m_mh);
      iovec&  iov       = mh->msg_iov[0];
      char*   buff      = static_cast<char*>(iov.iov_base);
      int     n         = udprx->m_len;
      utxx::time_val ts = udprx->m_ts;
      DEBUG_ONLY(int buffSz  = int(iov.iov_len);)
      assert(buff != nullptr && buffSz > 0 && n > 0 && n <= buffSz);

      // NB: "fromAddr" may or may not be obtained depending on the settings in
      // "mh". Normally we always get it at the socket level, because it is re-
      // turned as a "by-product" of "recvmsg", and the overhead of that is ab-
      // solutely negligible:
      IUAddr const* fromAddr  = static_cast<IUAddr const*>(mh->msg_name);
      assert(mh->msg_namelen <= sizeof(IUAddr));

      // Now invoke the user Action. NB: Similar to "ReadUntilEAgain",  excepti-
      // ons thrown by "a_action" are NOT handled here, because "cont" would not
      // be known then:
      bool cont = a_action(buff, n, ts, fromAddr);

      // NB: If  "a_action" has destroyed the FDInfo (via "Remove" on the FD in-
      // ternally known to it), it must have returned False:
      assert(!cont || (a_info.m_fd == fd && a_info.m_inst_id == iid));

      if (utxx::unlikely(!cont))
        return;
    }
    // If we got here, signal EndOfDataChunk:
    (void) a_action(nullptr, 0, utxx::time_val(), nullptr);

    // All Done!
  }

  //=========================================================================//
  // "SendUntilEAgain":                                                      //
  //=========================================================================//
  // Tries to send the data straight through the socket  (must be non-blocking)
  // until EAGAIN is encountered;  after that, places the rest of the user data
  // (if any data remains unsent) into the buffer provided, from which they can
  // be picked up and actually sent out by the "Reactor" when the socket Write-
  // ability is detected:
  // NB:
  // (*) unlike "RecvUntilEAgain", there is no generic user call-back here (but
  //     there is still an Error Handler);
  // (*) if the last 2 (optional) args are omitted, this function uses "send"
  //     (not "sendto"); it can still be used for UDP-like sockets but  they
  //     need to be connected first;
  //
  // OnError: (int fd, int ret, int errno)     -> bool
  //          (returns "true" iff need to throw an exception)
  //
  // "SendUntilEAgain" returns sending time (after completing socket output) if
  // the whole "a_len" bytes were sent, or empty value if there was an error:
  //
  template<bool UseGNUTLS, typename ErrHandler>
  inline utxx::time_val SendUntilEAgain
  (
    char const*       a_data,
    int               a_len,
    FDInfo     const& a_info,
    ErrHandler const& a_on_error,
    IUAddr const*     a_dest_addr     = nullptr,
    int               a_dest_addr_len = 0
  )
  {
    //-----------------------------------------------------------------------//
    // Check the params:                                                     //
    //-----------------------------------------------------------------------//
    // NB:
    // (*) "a_data" may be NULL (equivalent to "a_len<=0") if we only want to
    //     send the data which are already in "a_buff" (if any);
    // (*) "a_buff" may also be NULL, but in that case, if any data cannot be
    //     sent directly, an exception is thrown, so this mode is not recomm-
    //     ended;
    // (*) UseGNUTLS is equivalent to a_tls_sess != NULL:
    //
    int                      fd      = a_info.m_fd;
    utxx::dynamic_io_buffer* wrBuff  = a_info.m_wr_buff;
    gnutls_session_t         tlsSess = a_info.m_tlsSess;

    CHECK_ONLY
    (
      assert(((a_data == nullptr) == (a_len == 0)) && fd >= 0 &&
             (UseGNUTLS  == (tlsSess != nullptr))  &&
             (UseGNUTLS  == (a_info.m_tlsType  ==  TLSTypeT::GNUTLS)));

      if (utxx::unlikely(fd < 0 || (a_data == nullptr) != (a_len <= 0)))
        throw utxx::badarg_error
              ("SendUntilEAgain: Invalid Params: Data",
              (a_data == nullptr) ? '=' : '!', "=NULL, Len=", a_len, ", FD=",
               fd);
    )
    // The last 2 args must both be empty, or both non-empty. Also, DestAddr is
    // for DataGrams only, so currently incompatible with both GNUTLS and Kern-
    // elTLS:
    bool useSendTo = (a_dest_addr != nullptr);
    CHECK_ONLY
    (
      bool useTLS  = UseGNUTLS || (a_info.m_tlsType == TLSTypeT::KernelTLS);
      if (utxx::unlikely
         (useSendTo != (a_dest_addr_len > 0) || (useSendTo && useTLS)))
        throw utxx::badarg_error("SendUntilEAgain: DestAddr/TLS Inconsistency");
    )
    // Normally, there would be no data already present in "wrBuff";  but if
    // there are, the buffered data need to be sent out first. XXX: The easi-
    // est way of doing that is by copying "a_data" into "wrBuff" first; bec-
    // ause such situations are very rare, and the overhead of copying is ty-
    // pically small, we resort to that simplest method:
    bool fromBuffer = false;
    if (utxx::unlikely(wrBuff != nullptr && !(wrBuff->empty())))
    {
      if (a_len > 0)
      {
        assert(a_data != nullptr);
        wrBuff->write(a_data, size_t(a_len));
      }
      fromBuffer = true;
      a_data     = wrBuff->rd_ptr();
      a_len      = int(wrBuff->size());
      assert(a_len > 0);   // Because the buffer was not empty in this case
    }

    // So: Is there anything to send?
    if (utxx::unlikely(a_len <= 0))
      return utxx::time_val();

    //-----------------------------------------------------------------------//
    // Generic Case: Socket or TLS Sending Loop:                             //
    //-----------------------------------------------------------------------//
    assert(a_data != nullptr && a_len > 0);
    int         rem  = a_len;   // How many bytes remain to be sent
    char const* from = a_data;
    DEBUG_ONLY( char const* end  = a_data + a_len; )

    while (rem > 0)
    {
      assert(rem <= a_len);

      // Perform the actual "send" or "sendto". XXX: It is slightly sub-optimal
      // to use the dynamic "useSendTo" flag, but we avoid a static  param  for
      // the sake of simplicity.
      // NB: Ignore EINTR errors -- try over and over again until we succeed
      // (at least partially), or get EAGAIN, or a real error occurs:
      int n = 0;
      do  n =
        int(useSendTo
            ? sendto(fd, from, size_t(rem), MSG_NOSIGNAL,
                     reinterpret_cast<sockaddr const*>(a_dest_addr),
                     socklen_t(a_dest_addr_len))
            : !UseGNUTLS
            ? send  (fd, from, size_t(rem), MSG_NOSIGNAL)
            : gnutls_record_send(tlsSess,   from, size_t(rem)) );
      while
        (utxx::unlikely
        (!UseGNUTLS ? (n < 0 && errno == EINTR) : (n == GNUTLS_E_INTERRUPTED)));

      // To avoid accidential overwriting, save "errno" (for TLS, it is "n"
      // itself):
      int savedErr = !UseGNUTLS ? errno : n;

      // Normal EGAIAN case: we have sent as much data as we could. Unsent data
      // ("rem" bytes, > 0) are buffered (unless the data src is the buffer it-
      // self):
      if (utxx::unlikely
         (!UseGNUTLS ? (n < 0 && savedErr == EAGAIN) : (n == GNUTLS_E_AGAIN)))
      {
        assert(rem > 0);
        if (utxx::likely(!fromBuffer))
        {
          // Data are NOT from the buffer, so put the put the remaining data in-
          // to the buffer if it exists, otherwise XXX just throw an exception:
          if (utxx::likely(wrBuff != nullptr))
            wrBuff->write (from, size_t(rem));
          else
            throw utxx::runtime_error
                  ("SendUntilEAgain: ", rem, " bytes unsent, and no Buffer");
        }
        // Because not all intended data are sent yet (rem > 0), we cannot ret-
        // urn a completion time stamp:
        return utxx::time_val();
      }

      // Any other error case: The unsent data are NOT buffered because the er-
      // ror is likely to persist. In particular, EPIPE or ENOTCONN can be gen-
      // erated of the peer has closed connection.  The error will be reported
      // via ErrHanldr:
      if (utxx::unlikely(n <= 0))
      {
        // Otherwise: Got a real socket error; n==0 can occur for TCP sockets if
        // the receiving side had stopped reading data from their socket. So get
        // the error code:
        int ec = !UseGNUTLS ? GetSocketError(fd) : n;
        if (utxx::unlikely(ec >= 0))
          ec = savedErr;

        // Invoke the ErrHandler. NB: "wrBuff" is not automatically reset -- it
        // still contains all the unsent data; "on_error" must handle this:
        //
        a_on_error(fd, n, ec);

        // No point in going further, return an empty TimeStamp:
        return utxx::time_val();
      }

      // Finally, the Generic Case: Successful send of "n" bytes:
      // Advance the "from" ptr and decrement the remaining number of bytes:
      assert (0 < n && n <= rem);
      from += n;
      rem  -= n;
      assert(from <= end && rem >= 0);

      // If the data are being sent from the buffer, crunch the buffer for "n"
      // bytes (this produces a very minimal overhead):
      if (utxx::unlikely(fromBuffer))
      {
        assert(wrBuff != nullptr);
        (void) wrBuff->read_and_crunch(n);
      }
    }
    // If we got here: The data have actually been sent (not buffered -- other-
    // wise "rem" would not have been decremented to 0). So  "send_ts" CAN  be
    // determined (though it's time to the socket, not to the wire):
    assert(rem == 0);
    return utxx::now_utc();
  }

  //=========================================================================//
  // "MMap"ing Files:                                                        //
  //=========================================================================//
  // It is assumed that the file contains records of type "RecT". If "IsRW" is
  // set, the file is opened and "mmap"ed for Read-Write, otherwise Read-Only.
  // We follow the RAII paradigm: file is opened and mapped in  when an obj is
  // constructed, closed and unmapped when destructed:
  //
  template<typename RecT,  bool IsRW>
  class MMapedFile: public boost::noncopyable
  {
  private:
    //-----------------------------------------------------------------------//
    // Data Flds:                                                            //
    //-----------------------------------------------------------------------//
    int     m_fd;       // FileDescr  (iff the file was opened by us)
    void*   m_mapAddr;  // Address returned by "mmap" (no fixed-addr maps yet!)
    size_t  m_mapLen;   // FileLength (in bytes) when it was mapped
    long    m_nRecs;    // Number of "RecT" records in the map

  public:
    //-----------------------------------------------------------------------//
    // Ctors, Dtor:                                                          //
    //-----------------------------------------------------------------------//
    // Deafult Ctor:
    // Creates an empty obj (yet to be actually Mapped):
    //
    MMapedFile()
    : m_fd     (-1),
      m_mapAddr(nullptr),
      m_mapLen (0),
      m_nRecs  (0)
    {}

    // Non-Default Ctor: Creates are real Map:
    //
    MMapedFile(char const* a_file_name, int a_fd = -1)
    : m_fd     (-1),
      m_mapAddr(nullptr),
      m_mapLen (0),
      m_nRecs  (0)
    { Map(a_file_name, a_fd); }

    // Dtor: Invokes UnMap:
    ~MMapedFile() noexcept
    {
      // Do not allow any exceptions to come out of the Dtor:
      try { UnMap(); } catch(...){}
    }

    //-----------------------------------------------------------------------//
    // "Map":                                                                //
    //-----------------------------------------------------------------------//
    // The actual Map is constructed here:
    //
    void Map(char const* a_file_name, int a_fd = -1)
    {
      assert(a_file_name != nullptr);

      // An empty "MMapedFile" obj can actually be Mapped, but it can only be
      // unmapped by the Dtor:
      CHECK_ONLY
      (
        if (utxx::unlikely(!IsEmpty()))
          throw utxx::badarg_error("MMapFile: Already Mapped");
      )
      // Open the file (the mode depends on "IsRW"), unless a valid "a_fd" was
      // passed by the Caller:
      int fd = a_fd;
      if (fd < 0)
      {
        // Open the file by ourselves:
        fd = open(a_file_name, IsRW ? O_RDWR : O_RDONLY);
        CHECK_ONLY
        (
          if (utxx::unlikely(fd < 0))
            SystemError(-1, "MMapFile: Cannot open: ", a_file_name);
        )
      }
      assert(fd >= 0);

      // Get the file size (length of the MMap to be created):
      struct stat statBuff;
      (void) fstat(fd,   &statBuff);
      size_t len = size_t(statBuff.st_size);

      // The "len" must be a multiple of "MDR" size:
      CHECK_ONLY
      (
        if (utxx::unlikely(len % sizeof(RecT) != 0))
        {
          (void) close(fd);
          throw utxx::badarg_error
                ("MMapFile: Invalid size of ", a_file_name, ": ", len,
                 ": Must be a multiple of ",   sizeof(RecT));
        }
      )
      // Now the actual "mmap":
      // It is sufficient to have it "private" for reading; but need "shared"
      // for writing:
      void* addr =
        mmap
        (
          nullptr,    // Map addr is not fixed
          len,
          IsRW  ? (PROT_WRITE | PROT_READ)   : PROT_READ,
          (IsRW ?  MAP_SHARED : MAP_PRIVATE) | MAP_NORESERVE,
          fd,
          0           // Map is started at offset=0
        );
      if (utxx::unlikely(addr == MAP_FAILED || addr == nullptr))
      {
        int    err = errno;
        (void) close(fd);
        SystemError(err, "MMapFile: Cannot mmap: ", a_file_name);
      }

      // Map successful: Fininally initialize the obj;  NB: "fd" is memoised
      // ONLY if the file was opened by us (in that case, it will need to be
      // closed by the Dtor):
      m_fd      = (a_fd < 0) ? fd : -1;
      m_mapAddr = addr;
      m_mapLen  = len;
      m_nRecs   = long(len / sizeof(RecT));
    }

    //-----------------------------------------------------------------------//
    // "UnMap":                                                              //
    //-----------------------------------------------------------------------//
    // XXX: It is safe to invoke the Dtor multiple times (ie explicitly):
    //
    void UnMap()
    {
      if (m_fd >= 0)
        (void) close(m_fd);

      if (m_mapAddr != nullptr)
        (void) munmap(m_mapAddr, m_mapLen);

      m_fd      = -1;
      m_mapAddr = nullptr;
      m_mapLen  = 0;
      m_nRecs   = 0;
    }

    //-----------------------------------------------------------------------//
    // Accessors:                                                            //
    //-----------------------------------------------------------------------//
    // "GetPtr": Ptr to the MMaped array of "RecT" objs:
    std::conditional_t<IsRW, RecT*, RecT const*> GetPtr() const
    {
      return
        static_cast<std::conditional_t<IsRW, RecT*, RecT const*>>(m_mapAddr);
    }

    // Number of MMaped "RecT" objs:
    long GetNRecs() const { return m_nRecs; }

    // "IsEmpty": Returns "true" when the obj is not really Mapped:
    bool IsEmpty() const
    {
      return m_fd    == -1 && m_mapAddr == nullptr && m_mapLen == 0 &&
             m_nRecs == 0;
    }
  };
} // End namespace IO
} // End namespace MAQUETTE
