// vim:ts=2:et
//===========================================================================//
//                        "Connectors/ITCH/Configs.h":                       //
//===========================================================================//
#pragma once

#include "Basis/SecDefs.h"
#include "Protocols/ITCH/Features.h"
#include <utxx/compiler_hints.hpp>
#include <utxx/error.hpp>
#include <string>
#include <vector>

namespace MAQUETTE
{
  //=========================================================================//
  // "TCP_ITCH_Config" Struct:                                               //
  //=========================================================================//
  // XXX: For TCP-based ITCH, we normally do NOT expect too many Accounts per
  // connection, so we store User Credentials and Session details in the same
  // config struct:
  //
  struct TCP_ITCH_Config
  {
    //-----------------------------------------------------------------------//
    // Data Flds:                                                            //
    //-----------------------------------------------------------------------//
    std::string         m_ServerIP;
    int                 m_ServerPort;
    std::string         m_UserName;
    std::string         m_Passwd;
    int                 m_HeartBeatSec;
    int                 m_LogOnTimeOutSec;
    int                 m_LogOffTimeOutSec;
    int                 m_ReConnSec;

    //-----------------------------------------------------------------------//
    // Non-Default Ctor:                                                     //
    //-----------------------------------------------------------------------//
    TCP_ITCH_Config
    (
      std::string       a_server_ip,
      int               a_server_port,
      std::string       a_user_name,
      std::string       a_passwd,
      int               a_heart_beat_sec,
      int               a_logon_timeout_sec,
      int               a_logoff_timeout_sec,
      int               a_reconn_sec
    )
    : m_ServerIP        (a_server_ip),
      m_ServerPort      (a_server_port),
      m_UserName        (a_user_name),
      m_Passwd          (a_passwd),
      m_HeartBeatSec    (a_heart_beat_sec),
      m_LogOnTimeOutSec (a_logon_timeout_sec),
      m_LogOffTimeOutSec(a_logoff_timeout_sec),
      m_ReConnSec       (a_reconn_sec)
    {
      CHECK_ONLY
      (
        if (utxx::unlikely
           (m_ServerIP.empty()  || m_ServerPort      <  0 ||
            m_HeartBeatSec <= 0 || m_LogOnTimeOutSec <= 0 ||
            m_ReConnSec    <= 0 ))
          throw utxx::badarg_error("TCP_ITCH_Config::Ctor: Invalid arg(s)");
      )
    }
  };

  //=========================================================================//
  // "Get_TCP_ITCH_Config":                                                  //
  //=========================================================================//
  // (Implemented by template specialisation for each Dialect, in "Venues"):
  //
  template<ITCH::DialectT::type D>
  TCP_ITCH_Config const& Get_TCP_ITCH_Config(std::string const& a_key);

  //=========================================================================//
  // Obtaining SecDefs and their Properties:                                 //
  //=========================================================================//
  // Again, implemented in "Venues" by template specialisation over Dialect:
  //
  template<ITCH::DialectT::type D>
  std::vector<SecDefS> const& Get_TCP_ITCH_SrcSecDefs(MQTEnvT a_cenv);

  template<ITCH::DialectT::type D>
  bool Get_TCP_ITCH_UseTenorsInSecIDs();

} // End namespace MAQUETTE
