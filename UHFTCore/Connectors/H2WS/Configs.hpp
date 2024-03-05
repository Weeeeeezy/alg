// vim:ts=2:et
//===========================================================================//
//                        "Connectors/H2WS/Configs.hpp":                     //
//                    Configs for WebSocket-Based MDC/STP/OMC                //
//===========================================================================//
#pragma  once

#include "Basis/Macros.h"
#include "Basis/IOUtils.h"
#include "Basis/BaseTypes.hpp"
#include <boost/property_tree/ptree.hpp>
#include <string>
#include <vector>
#include <sys/random.h>

namespace MAQUETTE
{
  //=========================================================================//
  // "H2WSConfig":                                                           //
  //=========================================================================//
  // Crypto-exchanges providing a WebSocket API often have multiple IP address-
  // es with quite different access latency, hence the following config type:
  //
  struct H2WSConfig
  {
    //-----------------------------------------------------------------------//
    // Data Flds:                                                            //
    //-----------------------------------------------------------------------//
    char const*                m_HostName;
    int                        m_Port;
    std::vector<char const*>   m_IPs;

    //-----------------------------------------------------------------------//
    // Default Ctor:                                                         //
    //-----------------------------------------------------------------------//
    H2WSConfig()
    : m_HostName(nullptr),
      m_Port    (-1),
      m_IPs     ()
    {}

    //-----------------------------------------------------------------------//
    // Non-Default Ctor:                                                     //
    //-----------------------------------------------------------------------//
    H2WSConfig
    (
      char const*              a_host_name,
      int                      a_port,
      std::vector<char const*> a_ips
    )
    : m_HostName(a_host_name),
      m_Port    (a_port),
      m_IPs     (a_ips)
    {
      assert(m_HostName != nullptr && *m_HostName != '\0' &&
             m_Port     >= 0       && m_Port      <= 65535);

      // "m_IPs" may be empty, but if present, they must be valid (at least
      // superficially):
      DEBUG_ONLY
      (
        for (auto const& ip: m_IPs)
          assert(ip != nullptr && *ip != '\0');
      )
    }
  };

  //=========================================================================//
  // "GetH2WSIP":                                                            //
  //=========================================================================//
  inline std::string GetH2WSIP
  (
    H2WSConfig const&                   a_config,
    boost::property_tree::ptree const&  a_params
  )
  {
    // If the ServerIP address is explicitly given in "Params", return that
    // one:
    std::string const& ipE = a_params.get<std::string>("ServerIP", "");
    if (!ipE.empty())
      return ipE;

    // Otherwise, get the pre-configured list of IPs from "Config":
    std::vector<char const*> const& ips = a_config.m_IPs;
    if (!ips.empty())
    {
      // Select a random IP from the list:
      size_t rnd = 0;
      DEBUG_ONLY(long len =) getrandom(&rnd, sizeof(rnd), 0);
      assert(len == sizeof(rnd));

      size_t i  = rnd % ips.size();
      char const* ipR = ips[i];
      assert(ipR != nullptr && *ipR != '\0');
      return std::string(ipR);
    }
    // Finally, perform IP look-up for the HostName (XXX: a potentially-blocking
    // operation!):
    return IO::GetIP(a_config.m_HostName);
  }

  //=========================================================================//
  // "H2WSAccount":                                                          //
  //=========================================================================//
  // Contains API Key and API Secret typically used for HTTP/2 and WS authenti-
  // cation:
  struct H2WSAccount
  {
    // Data Flds:
    std::string   m_APIKey;
    std::string   m_APISecret;

    // Default Ctor:
    H2WSAccount()
    : m_APIKey   (),
      m_APISecret()
    {}

    // Non-Default Ctor:
    H2WSAccount(std::string const& a_api_key, std::string const& a_api_secret)
    : m_APIKey   (a_api_key),
      m_APISecret(a_api_secret)
    {}

    // Similarly, extracting the APIKey and APISecret from Params:
    H2WSAccount(boost::property_tree::ptree const&  a_params)
    : m_APIKey   (a_params.get<std::string>("APIKey",    "")),
      m_APISecret(a_params.get<std::string>("APISecret", ""))
    {}
  };
} // End namespace MAQUETTE
