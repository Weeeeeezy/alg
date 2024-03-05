// vim:ts=2:et
//===========================================================================//
//                            "Acceptors/TCP_Acceptor.h":                    //
//                 Common Base of TCP-Based Acceptors (Servers)              //
//===========================================================================//
#pragma once

#include "Basis/IOUtils.h"
#include "InfraStruct/PersistMgr.h"
#include <spdlog/spdlog.h>
#include <boost/core/noncopyable.hpp>
#include <boost/property_tree/ptree.hpp>
#include <string>

namespace MAQUETTE
{
  class EPollReactor;

  //=========================================================================//
  // "TCP_Acceptor" Class:                                                   //
  //=========================================================================//
  template<typename Derived>
  class TCP_Acceptor: public PersistMgr<>
  {
  public:
    //=======================================================================//
    // Names of Persistent Objs:                                             //
    //=======================================================================//
    constexpr static char const* SessionsON()   { return "Sessions";   }
    constexpr static char const* TotBytesRxON() { return "TotBytesRx"; }
    constexpr static char const* TotBytesTxON() { return "TotBytesTx"; }
  protected:
    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    // General Params.
    // NB: Unlike  "TCP_Connector"  which is supposed  to be used within the
    // "EConnector" infrastructure, here we manage ShM directly in this class:
    //
    std::string      const                m_name;      // Acceptor inst name
    EPollReactor*    const                m_reactor;   // Ptr NOT owned
    spdlog::logger*  const                m_logger;    // Ptr NOT owned
    int              const                m_debugLevel;

    // Acceptor always contains a Listening Socket. We assume that there is only
    // 1 such socket per Acceptor, though in theory there could be several -- in
    // the Non-Blocking mode, this is entirely feasible:
    sockaddr_in                           m_accptAddr; // IP, Port to Listen on
    mutable int                           m_accptFD;
    mutable int                           m_timerFD;

    // Sessions: The actual Session type is defined by the Derived class.  For
    // persistence, the Session objs are allocated in ShM, and may contain some
    // past Sessions (this is why MaxSessions > MaxFDs: the latter is the larg-
    // est curr FD):
    constexpr static int                  MaxSess = 65521; // Larg.Prime < 65536
    constexpr static int                  MaxFDs  = 32768;

    // The number of currently-running Sessions (may be limited):
    int              const                m_maxCurrSess;
    mutable int                           m_currSess;
    typename Derived::SessionT*           m_sessions;      // Ptr to ShM Sess

    // HeartBeat interval applied to each Session:
    int              const                m_heartBeatSec;

    // In-bound buffers are per-Session (per-FD, and maintained by the Reactor);
    // the following buffer params are used (they also apply to out-bound "left-
    // over" buffers -- for data which cannot be sent out immediately):
    int              const                m_buffSz;
    int              const                m_buffLWM;

    // Statistics: Total number of bytes sent (Tx) and received (Rx). In parti-
    // cular, it can be used for liveness monitoring (counters are in ShM):
    unsigned long*                        m_totBytesTx;
    unsigned long*                        m_totBytesRx;

    // NB: Access control is delegated  to the Derived class
    // XXX: As yet, there is no connection rate control for Clients...

    // Default Ctor is deleted:
    TCP_Acceptor() = delete;

  public:
    //=======================================================================//
    // Non-default Ctor, Dtor:                                               //
    //=======================================================================//
    TCP_Acceptor
    (
      // General Params:
      std::string const&                  a_name,         // Non-empty
      EPollReactor  *                     a_reactor,      // Non-NULL
      spdlog::logger*                     a_logger,       // Non-NULL
      int                                 a_debug_level,
      // IO Params:
      std::string const&                  a_accpt_ip,
      int                                 a_accpt_port,
      int                                 a_buff_sz,
      int                                 a_buff_lwm,
      // Session Control:
      int                                 a_max_curr_sess,
      int                                 a_heart_beat_sec,
      // Misc Params (may be NULL or empty):
      boost::property_tree::ptree const*  a_params
    );

    ~TCP_Acceptor() noexcept

    //=======================================================================//
    // "Start", "Stop", Properties:                                          //
    //=======================================================================//
    void Start();
    void Stop ();

    // "GetName":
    char const*    GetName()        const { return m_name.data();  }

    // Data Volume Statistics:
    unsigned long  GetTotBytesTx()  const { return *m_totBytesTx;  }
    unsigned long  GetTotBytesRx()  const { return *m_totBytesRx;  }

    // Is this Acceptor currently Active?
    bool           IsActive()       const { return (m_accptFD >= 0); }

  protected:
    // "StopNow": Internal implementation of "Stop":
    template<bool Graceful>
    void StopNow(char const* a_where);

    //=======================================================================//
    // Handlers for EPollReactor (except ReadHandler):                       //
    //=======================================================================//
    // NB: ReadHandler is highly Protocol-specific, so it is provided by the
    // Derived class:
    //
    // "AcceptHandler":
    void AcceptHandler
    (
      int               a_acceptor_fd,
      int               a_client_fd,
      IO::IUAddr const* a_client_addr
    );

    // "TimerHandler":
    void TimerHandler();

    // "ErrHandler":
    // Call-back invoked on any IO error (via "m_errH" which is registered with
    // the Reactor):
    //
    void ErrHandler
      (int a_fd, int a_err_code, uint32_t a_events, char const* a_msg);

    //=======================================================================//
    // Internal Utils:                                                       //
    //=======================================================================//
    // "GetMapSize":
    // Calculate the size of the ShM Segment to be allocated:
    //
    static size_t GetMapSize();

    // "Close{Accpt,Timer}FD":
    // Close the AcceptorFD or TimerFD, and remove them from the Reactor:
    //
    void CloseAccptFD(char const* a_where) const;
    void CloseTimerFD(char const* a_where) const;

    // "GetSession":
    // Obtain the Session (in this case, by Client FD). Returns NULL of such a
    // Session was not found:
    //
    typename Derived::SessionT* GetSession(int a_fd) const;

    // "SendImpl":
    // Send the data out via the Reactor:
    //
    utxx::time_val SendImpl(char const* a_buff, int a_len) const;

    // "CheckFDConsistency":
    // Verify the consistency of Client FDs attached to the Reactor (and asso-
    // ciated with a Session Ptr), and the FD stored in the Session obj itself.
    // If the check fails, the Acceptor is stopped (but still gracefully), and
    // "false" is returned. Otherwise, "true" is returned:
    //
    bool CheckFDConsistency(int a_fd, typename Derived::SessionT* a_sess);
  };
} // End namespace MAQUETTE
