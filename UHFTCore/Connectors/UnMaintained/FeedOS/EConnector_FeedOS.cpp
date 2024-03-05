// vim:ts=2:et
//===========================================================================//
//                 "Connectors/FeedOS/EConnector_FeedOS.cpp":                //
//      MktData using the QuantHouse / S&P Capital IQ API (aka FeedOS)       //
//===========================================================================//
#include "Connectors/FeedOS/EConnector_FeedOS.h"
#include "Venues/FeedOS/SecDefs.h"
#include "Connectors/EConnector_MktData.hpp"
#include <utxx/error.hpp>
#include <utxx/compiler_hints.hpp>

using namespace std;

namespace MAQUETTE
{
  //=========================================================================//
  // Non-Default Ctor:                                                       //
  //=========================================================================//
  EConnector_FeedOS::EConnector_FeedOS
  (
    EPollReactor*                       a_reactor,
    SecDefsMgr*                         a_sec_defs_mgr,
    RiskMgr*                            a_risk_mgr,
    boost::property_tree::ptree const&  a_params
  )
  : //-----------------------------------------------------------------------//
    // Parent Classes:                                                       //
    //-----------------------------------------------------------------------//
    // NB: Explicit invocation of the "EConnector" ctor is required due to virt-
    // ual inheritance of "EConnector_MktData". XXX: We currently  assume  that
    // only the "Prod" env exists for FeedOS MktData:
    //
    EConnector_MktData
    (
      a_params.get<std::string>("AccountKey"),
      "FeedOS",
      0,                    // XXX: No extra ShM data at the moment...
      a_reactor,
      false,                // No BusyWait: the FD is still exposed to EPoll
      a_sec_defs_mgr,
      // XXX: The actual set of supported securities depends on FeedOS setup.
      // It should NOT be configured statically -- but for now, we do it that
      // way:
      FeedOS::SecDefs,
      a_params.get<bool>    ("ExplSettlDatesOnly", true),
      false,                // Tenors are NOT used in FeedOS SecIDs
      a_risk_mgr,
      a_params,
      QtyTypeT::Contracts,  // XXX ???
      false,                // XXX No fractional Qtys???
      nullptr,              // No Primary MDC
      false,                // No FullAmt
      false,                // No Sparse OrderBooks
      false,                // No SeqNums in OrderBooks
      false,                // No RptSeqs either
      false,                // So their Strictness is irrelevant
      0,                    // FIXME: MktDepth for FeedOS???
      false,                // Trades are currently not provided (XXX?)
      false,                // And in any case, Trades are not inferred from OB
      false                 // No DynInit
    ),
    //-----------------------------------------------------------------------//
    // "EConnector_FeedOS" itself:                                           //
    //-----------------------------------------------------------------------//
    // Get the Config, but do not connect yet:
    //
    m_config     (FeedOS::GetConfig(EConnector::m_name)),
    m_conn       (),
    m_maxRestarts(a_params.get<int>("MaxConnectAttempts", 5)),
    m_fd         (-1),
    m_errFlag    (false)
  {
    //-----------------------------------------------------------------------//
    // Verify the Config:                                                    //
    //-----------------------------------------------------------------------//
    CHECK_ONLY
    (
      if (utxx::unlikely
         (m_config.m_ServerAddr.empty() || m_config.m_ServerPort < 0   ||
          m_config.m_ServerPort > 65535 || m_config.m_UserName.empty() ||
          m_config.m_Passwd.empty()     || m_maxRestarts <= 0))
        throw utxx::badarg_error
              ("EConnector_FeedOS::Ctor: ", m_name, ": Invalid Params");
    )
  }

  //=========================================================================//
  // Dtor:                                                                   //
  //=========================================================================//
  EConnector_FeedOS::~EConnector_FeedOS() noexcept
  {
    // Stop the Connector, but prevent any exceptions from propagating:
    try   { Stop(); }
    catch (...)    {}
  }

  //=========================================================================//
  // "Start":                                                                //
  //=========================================================================//
  // XXX: It currently uses Synchronous Connect to the FeedOS server. For  this
  // reason, if that TCP connection is terminated, there is currently no way of
  // re-starting it while continuing to run the Main Event Loop, because  that
  // would block other events from arriving:
  //
  void EConnector_FeedOS::Start()
  {
    //-----------------------------------------------------------------------//
    // Are we not connected yet?                                             //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely(m_fd >= 0))
    {
      CHECK_ONLY
      (
        // The following should not happen, of course:
        if (utxx::unlikely(m_fd != m_conn.get_socket_handle()))
          throw utxx::logic_error
                ("EConnector_FeedOS::Start: ",      m_name,
                 ": SocketFD inconsistency: m_fd=", m_fd, ", ConnHandle=",
                 m_conn.get_socket_handle());
      )
      // Socket already exists, so presumably, we are connected?
      if (m_reactor->IsConnected(m_fd, nullptr, nullptr))
      {
        // Yes -- already connected, nothing to do:
        LOG_WARN(2,
          "EConnector_FeedOS::Start: Already connected (FD={})", m_fd)
        return;
      }
      // Otherwise, it is a strange condition: Close the socket, and will
      // connect again:
      m_reactor->Remove(m_fd);
      (void) close     (m_fd);
      m_fd = -1;
    }

    //-----------------------------------------------------------------------//
    // So: Try to connect to the Server:                                     //
    //-----------------------------------------------------------------------//
    assert(m_fd < 0);

    for (int i = 0; i < m_maxRestarts; ++i)
    {
      auto rc = m_conn.sync_connect
      (
        m_config.m_ServerAddr,
        unsigned(m_config.m_ServerPort),
        m_config.m_UserName,
        m_config.m_Passwd
      );
      if (utxx::likely(rc == ::FeedOS::RC_OK))
        // Successfully connected:
        break;

      // Otherwise: Connection attempt failed:
      LOG_WARN(1,
        "EConnector_FeedOS::Start: Attempt #{}: Could not connect to {}:{}: "
        "{}", i, m_config.m_ServerAddr, m_config.m_ServerPort,
        ::FeedOS::error_string(rc))

      // If all connection attempts failed:
      if (i == m_maxRestarts - 1)
        LOG_ERROR(1,
          "EConnector_FeedOS::Start: ", m_name, ": Failed to connect to ",
          m_config.m_ServerAddr,   ':', m_config.m_ServerPort)

      // Otherwise: Wait for 5 sec before trying again:
      ::FeedOS::API::sleep(5);
    }

    //-----------------------------------------------------------------------//
    // If we got here: Successfully Connected:                               //
    //-----------------------------------------------------------------------//
    // Extract the SocketFD:
    m_fd = m_conn.get_socket_handle();

    CHECK_ONLY
    (
      if (utxx::unlikely(m_fd < 0))
        throw utxx::runtime_error
              ("EConnector_FeedOS::Start: ", m_name,
               ": Could NOT get the underlying SocketFD");
    )
    // SocketFD is OK, Connection Confirmed:
    LOG_INFO(1,
      "EConnector_FeedOS::Start: Successfully Connected to {}:{}: FD={}",
      m_config.m_ServerAddr, m_config.m_ServerPort, m_fd)

    //-----------------------------------------------------------------------//
    // Set up the Handlers and Add this SocketFD to the Reactor:             //
    //-----------------------------------------------------------------------//
    // RawInputHandler:
    IO::FDInfo::RawInputHandler rawInH
      ([this](int a_fd)->bool { return RawInputHandler(a_fd); });

    // ErrHandler:
    IO::FDInfo::ErrHandler      errH
    (
      [this](int a_fd, int a_err_code, uint32_t a_events, char const* a_msg)
      -> void
      { ErrHandler(a_fd,  a_err_code, a_events, a_msg); }
    );

    // NB: The FeedOS socket was originally intended to work with "select", so
    // it must be Level-Triggered (ie EdgeTriggered = false):
    //
    m_reactor->AddRawInput
      (EConnector::m_name.data(), m_fd, false, rawInH, errH);

    // XXX: Do we need to request Meta-Data here?

/*
FeedOS::Types::ListOfQuotationTagNumber otherValues;
  FeedOS::Types::QuotationContentMask contentMask =
FeedOS::Types::QuotationContentMask_EVERYTHING;
  bool ignoreInvalidCodes=false;

  // start the subscription. Use sync_start() for the synchronous version
  my_L1_subscription->start (connection, my_list_of_instr_codes, otherValues,
contentMask, ignoreInvalidCodes)
*/

    LOG_INFO(1,
      "EConnector_FeedOS::Stop: Name={}, FD={}: Connector STARTED",
      EConnector::m_name, m_fd)
  }

  //=========================================================================//
  // "Stop":                                                                 //
  //=========================================================================//
  void EConnector_FeedOS::Stop()
  {
    if (utxx::unlikely(m_fd < 0))
    {
      // Nothing to do, presumably:
      LOG_WARN(2, "EConnector_FeedOS::Stop: Already disconnected")
      return;
    }

    // Otherwise: Check the SocketFDs consistency:
    CHECK_ONLY
    (
      if (utxx::unlikely(m_fd != m_conn.get_socket_handle()))
        throw utxx::logic_error
              ("EConnector_FeedOS::Stop: ",       m_name,
               ": SocketFD inconsistency: m_fd=", m_fd, ", ConnHandle=",
               m_conn.get_socket_handle());
    )
    // Disconnect, detach the Socket FD, close the Socket:
    m_conn.disconnect();
    m_reactor->Remove(m_fd);
    (void)      close(m_fd);

    LOG_INFO(1,
      "EConnector_FeedOS::Stop: Name={}, FD={}: Connector STOPPED",
      EConnector::m_name, m_fd)

    m_fd = -1;
  }

  //=========================================================================//
  // "IsActive":                                                             //
  //=========================================================================//
  bool EConnector_FeedOS::IsActive() const
    { return m_fd >= 0; }

  //=========================================================================//
  // "RawInputHandler":                                                      //
  //=========================================================================//
  inline bool EConnector_FeedOS::RawInputHandler(int a_fd)
  {
    // Verify the SocketFD: If it fails, it's a critical error:
    CHECK_ONLY
    (
      if (utxx::unlikely
         (a_fd < 0 || a_fd != m_fd || a_fd != m_conn.get_socket_handle()))
      {
        LOG_CRIT(1,
          "EConnector_FeedOS::RawInputHandler: Inconsistency: ArgFD={}, m_fd={}"
          ", m_conn.handle={}", a_fd, m_fd, m_conn.get_socket_handle())

        // Terminate the application:
        m_reactor->ExitImmediately("EConnector_FeedOS::RawInputHandler");
      }
    )
    // FeedOS will read the data from the socket:
    int rc = m_conn.read_socket();

    // Reset the ErrFlag before the pending msgs (if any) are processed:
    m_errFlag = false;

    if (utxx::likely(rc != 0))
      // This is NOT an error by itself -- this means that some msgs have been
      // read, and will be forwarded to our Second-Level Handler.   The latter
      // may potentially set the ErrFlag:
      //
      (void) m_conn.dispatch_pending_events();

    // Finally:
    return !m_errFlag;
  }

  //=========================================================================//
  // "ErrHandler":                                                           //
  //=========================================================================//
  inline void EConnector_FeedOS::ErrHandler
  (
    int               a_fd,
    int               a_err_code,
    uint32_t          a_events,
    char const*       a_msg
  )
  {
    assert(a_msg != nullptr);

    // Verify the SocketFD: If it fails, it's a critical error:
    CHECK_ONLY
    (
      if (utxx::unlikely
         (a_fd < 0 || a_fd != m_fd || a_fd != m_conn.get_socket_handle()))
      {
        LOG_CRIT(1,
          "EConnector_FeedOS::ErrHandler: Inconsistency: ArgFD={}, m_fd={}, "
          "m_conn.handle={}", a_fd, m_fd, m_conn.get_socket_handle())

        m_reactor->ExitImmediately("EConnector_FeedOS::ErrHandler");
        __builtin_unreachable();  // Not reached!
      }
    )
    // Log this error condition:
    LOG_ERROR(1,
      "EConnector_FeedOS::ErrHandler: IO Error: FD={}, ErrNo={}, Events={}: "
      "{}", a_fd, a_err_code, m_reactor->EPollEventsToStr(a_events), a_msg)

    // Stop the Connector (actually, the Reactor itself would also detach the
    // Socket in this case -- but not close it, so close it here):
    Stop();
  }
}
// End namesapce MAQUETTE
