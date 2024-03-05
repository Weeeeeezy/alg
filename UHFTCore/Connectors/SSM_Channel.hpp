// vim:ts=2:et
//===========================================================================//
//                          "Connectors/SSM_Channel.hpp":                    //
//     Base class for UDP SSM Channels (to be attached to "EPollReactor")    //
//===========================================================================//
// This class provides call-backs to "EPollReactor" -- core functionality only;
// the rest is provided by the corresp Derived class (template param):
//
#pragma once

#include "Basis/EPollReactor.h"
#include "Basis/IOUtils.h"
#include "Connectors/SSM_Config.h"
#include <utxx/compiler_hints.hpp>
#include <utxx/time_val.hpp>
#include <utxx/error.hpp>
#include <boost/core/noncopyable.hpp>
#include <cstdlib>
#include <string>

namespace MAQUETTE
{
  //=========================================================================//
  // "SSM_Channel" Class:                                                    //
  //=========================================================================//
  template<typename Derived>
  class SSM_Channel: public boost::noncopyable
  {
  protected:
    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    SSM_Config    const       m_config;
    std::string   const       m_ifaceIP;
    std::string   const       m_shortName;  // Names of this Channel
    std::string   const       m_longName;   //
    EPollReactor*             m_reactor;    // Ptr NOT owned
    spdlog::logger*           m_logger;     // Ptr NOT owned
    int                       m_fd;
    int           const       m_debugLevel;

    // Default Ctor is deleted -- it would make no sense:
    SSM_Channel() = delete;

  public:
    //=======================================================================//
    // Non-Default Ctor:                                                     //
    //=======================================================================//
    SSM_Channel
    (
      SSM_Config  const&      a_config,
      std::string const&      a_iface_ip,
      std::string const&      a_name,
      EPollReactor*           a_reactor,
      spdlog::logger*         a_logger,     // Use the Logger from enclosing
      int                     a_debug_level //   Conn, not from the Reactor!
    )
    : // Store the Config Params:
      m_config   (a_config),
      m_ifaceIP  (a_iface_ip),
      m_shortName(a_name),
      m_longName (a_name              + ":" + m_config.m_groupIP + ":" +
                  m_config.m_sourceIP + ":" + m_config.m_localIP + ":" +
                  std::to_string(m_config.m_localPort)),
      m_reactor  (a_reactor),
      m_logger   (a_logger),

      // Should we open the UDP socket here? -- No, will do it in "Start";
      // it's more reliable from the error mgmt point of view:
      m_fd(-1),

      // DebugLevel:
      m_debugLevel(a_debug_level)
    {
      // Validate the settings:
      assert(m_reactor != nullptr);
      CHECK_ONLY
      (
        if (utxx::unlikely
           (m_logger == nullptr        ||
            m_config.m_groupIP.empty() || m_config.m_sourceIP.empty() ||
            m_config.m_localPort < 0))
          throw utxx::badarg_error("SSM_Channel::Ctor");
      )
    }

    //=======================================================================//
    // Dtor:                                                                 //
    //=======================================================================//
    ~SSM_Channel() noexcept
    {
      // Make sure exceptions do not propagate out of the Dtor:
      try
      {
        Stop();
        m_reactor = nullptr;
        m_logger  = nullptr;
      }
      catch(...){}
    }

    //=======================================================================//
    // "Start":                                                              //
    //=======================================================================//
    void Start()
    {
      if (utxx::unlikely(m_fd >= 0))
      {
        LOG_WARN(3,
          "SSM_Channel::Start: Name={}, FD={}: Already Started",
          m_longName, m_fd)
        return;
      }
      // Construct the Handlers:
      // NB: We have to use Lambdas for "recvH" and "errH" below,  rather than
      // to implement corresp "operator()"s  in "SSM_Channel" itself  and pass
      // "*this" to "std::function" ctors,  because in the former  case,  only
      // "this" ptr needs to be captured; in the latter case, the whole object
      // would need to be copied into "std::function" -- and  we can't do that,
      // obviously:
      //
      // Recv  Handler function -- a wrapper around "RecvHandler" method to be
      // provided by the "Derived" class; "FromAddr" is ignored at this point:
      //
      IO::FDInfo::RecvHandler recvH
      (
        [this](int a_fd,     char const* a_buff,    int a_size,
               utxx::time_val a_ts_recv, IO::IUAddr const*) ->bool
        {
          return (static_cast<Derived*>(this))->
                 RecvHandler(a_fd, a_buff, a_size, a_ts_recv);
        }
      );
      // Error Handler function -- a wrapper around "ErrHandler" method to be
      // provided by the "Derived" class:
      IO::FDInfo::ErrHandler  errH
      (
        [this](int a_fd, int a_err_code, uint32_t a_events, char const* a_msg)
        -> void
        {
          (static_cast<Derived*>(this))->
            ErrHandler(a_fd, a_err_code, a_events, a_msg);
        }
      );

      // Open and bind the UDP socket, and attach it to the Reactor:
      // Because this is UDP, we don't need a large input buffer size  -- the
      // buffer is emptied every time:
      // IsServer =true (multicast listening is supposed to be a SERVER mode;
      //
      m_fd = m_reactor->AddDataGram
      (
        m_shortName.data(),
        m_config.m_localIP.data(),  // Empty OK
        m_config.m_localPort,       // Already checked: must be valid
        recvH,                      // Long lifespan     -- safe
        errH,                       // Long lifespan     -- safe
        0,       // No writing into UDP SSM socket, so WriteBuffSz  = 0
        0,       //                                and WriteBuffLWM = 0
        nullptr, // No remote IP/Port to connect to (no sending)
        -1
      );
      assert(m_fd >= 0);

      // Subscribe to the MCast. If it fails for any reason, we remove the sock
      // and re-throw the exception:
      try
      {
        IO::MCastManage<true>
          (m_fd,    m_config.m_groupIP.data(), m_ifaceIP.data(),
          m_config.m_sourceIP.data());
      }
      catch (...)
      {
        m_reactor->Remove(m_fd);
        m_fd = -1;
        throw;
      }
      // All Done:
      LOG_INFO(1,
        "SSM_Channel::Start: Name={}, FD={}: Started", m_longName, m_fd)
    }

    //=======================================================================//
    // "Stop":                                                               //
    //=======================================================================//
    void Stop()
    {
      if (utxx::unlikely(m_fd < 0))
      {
        LOG_WARN(3,
          "SSM_Channel::Stop: Name={}: Already Stopped", m_longName)
        return;
      }
      // Generic Case: Unsubscribe from MCast:
      IO::MCastManage<false>
        (m_fd,    m_config.m_groupIP.data(), m_ifaceIP.data(),
         m_config.m_sourceIP.data());

      // And finally remove the UDP socket:
      m_reactor->Remove(m_fd);

      LOG_INFO(1,
        "SSM_Channel::Stop: Name={}, FD={}: Stopped", m_longName, m_fd)
      m_fd = -1;
    }
  };
} // End namespace MAQUETTE
