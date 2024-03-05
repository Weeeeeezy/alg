// vim:ts=2:et
//===========================================================================//
//                         "Connectors/TWIME/Configs.h":                     //
//          Session and Account Config Types for the TWIME Connector         //
//===========================================================================//
#pragma once

#include "Basis/Macros.h"
#include "Protocols/FIX/Features.h"
#include <utxx/error.hpp>
#include <boost/container/static_vector.hpp>
#include <string>
#include <utility>

namespace MAQUETTE
{
  //=========================================================================//
  // "TWIMEAccountConfig" Struct:                                            //
  //=========================================================================//
  // Contains User Credentials and User-specific settings:
  struct TWIMEAccountConfig
  {
    //-----------------------------------------------------------------------//
    // Data Flds:                                                            //
    //-----------------------------------------------------------------------//
    std::string      m_ClientID;      // Used in LogOn ("Establish")
    std::string      m_Account;       // Used in Order Placement Msgs
    int              m_MaxReqsPerSec; // Depends on the Account settings...

    // Default Ctor is deleted:
    TWIMEAccountConfig() = delete;

    //-----------------------------------------------------------------------//
    // Non-Default Ctor:                                                     //
    //-----------------------------------------------------------------------//
    TWIMEAccountConfig
    (
      std::string const&  a_client_id,
      std::string const&  a_account,
      int                 a_max_reqs_per_sec
    )
    : m_ClientID          (a_client_id),
      m_Account           (a_account),
      m_MaxReqsPerSec     (a_max_reqs_per_sec)
    {
      CHECK_ONLY
      (
        if (utxx::unlikely
           (m_ClientID.empty() || m_Account.empty() || m_MaxReqsPerSec <= 0))
          throw utxx::badarg_error
                ("TWIMEAccountConfig::Ctor: Invalid param(s)");
      )
    }
  };

  //=========================================================================//
  // "TWIMESessionConfig" Struct:                                            //
  //=========================================================================//
  struct TWIMESessionConfig
  {
    //-----------------------------------------------------------------------//
    // Data Flds:                                                            //
    //-----------------------------------------------------------------------//
    std::string      m_MainIP;
    int              m_MainPort;
    std::string      m_RecoveryIP;
    int              m_RecoveryPort;
    int              m_HeartBeatSec;
    int              m_LogOnTimeOutSec;
    int              m_LogOffTimeOutSec;
    int              m_ReConnectSec;
    int              m_MaxGapMSec;

    // Default Ctor is deleted:
    TWIMESessionConfig() = delete;

    //-----------------------------------------------------------------------//
    // Non-Default Ctor:                                                     //
    //-----------------------------------------------------------------------//
    TWIMESessionConfig
    (
      std::string const&  a_main_ip,
      int                 a_main_port,
      std::string const&  a_recovery_ip,
      int                 a_recovery_port,
      int                 a_heart_beat_sec,
      int                 a_logon_timeout_sec,
      int                 a_logoff_timeout_sec,
      int                 a_reconnect_sec,
      int                 a_max_gap_msec
    )
    : m_MainIP            (a_main_ip),
      m_MainPort          (a_main_port),
      m_RecoveryIP        (a_recovery_ip),
      m_RecoveryPort      (a_recovery_port),
      m_HeartBeatSec      (a_heart_beat_sec),
      m_LogOnTimeOutSec   (a_logon_timeout_sec),
      m_LogOffTimeOutSec  (a_logoff_timeout_sec),
      m_ReConnectSec      (a_reconnect_sec),
      m_MaxGapMSec        (a_max_gap_msec)
    {
      CHECK_ONLY
      (
        if (utxx::unlikely
           (m_MainIP.empty()         || m_MainPort        <  0 ||
            m_RecoveryIP.empty()     || m_RecoveryPort    <  0 ||
            m_HeartBeatSec     <= 0  || m_ReConnectSec    <= 0 ||
            m_MaxGapMSec       <= 0  || m_LogOnTimeOutSec <= 0 ||
            m_LogOffTimeOutSec <= 0))
          throw utxx::badarg_error("TWIMESessionConfig: Invalid param(s)");
      )
    }
  };

  //=========================================================================//
  // "GetTWIME{Account,Session}Config":                                      //
  //=========================================================================//
  TWIMEAccountConfig const& GetTWIMEAccountConfig(std::string const& a_key);
  TWIMESessionConfig const& GetTWIMESessionConfig(std::string const& a_key);

} // End namespace MAQUETTE
