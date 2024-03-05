// vim:ts=2:et
//===========================================================================//
//                       "Venues/NTProgress/Configs_FIX.cpp":                //
//                   Session Configs for NTProgress FIX Connector            //
//===========================================================================//
#include "Venues/NTProgress/Configs_FIX.hpp"

using namespace std;

namespace MAQUETTE
{
namespace NTProgress
{
  //=========================================================================//
  // "FIXSessionConfigs":                                                    //
  //=========================================================================//
  // NB: In the Configs below:
  // .m_ServerName          = ""
  // .m_UseTLS              = false
  // .m_HeartBeatSec        = 2 (XXX why so short???)
  // .m_LogOnTimeOutSec     = 5
  // .m_LogOffTimeOutSec    = 1
  // .m_ReConnectSec        = 5
  // .m_MaxGapMSec          = 500 (how long to wait for sync after ResendReq)
  // .m_IsOrdMgmt, .m_IsMktDaya:  depend on Env
  //
  map<FIXEnvT::type, FIXSessionConfig const> const FIXSessionConfigs
  {
    // Test MDC:
    //-----------------------------------------------------------------------//
    { FIXEnvT::TestMKT,
    //-----------------------------------------------------------------------//
      { "", "10.144.50.20", 20001, false, "NTPROUAT", 2, 5, 1, 5, 500,
        false, true }
    },
    // Test OMC:
    //-----------------------------------------------------------------------//
    { FIXEnvT::TestORD,
    //-----------------------------------------------------------------------//
      { "", "10.144.50.20", 20002, false, "NTPROUAT", 2, 5, 1, 5, 500,
        true, false }
    },

    // Prod MDC:
    //-----------------------------------------------------------------------//
    { FIXEnvT::ProdMKT,
    //-----------------------------------------------------------------------//
      { "", "10.144.50.30", 20001, false, "NTPRO",    2, 5, 1, 5, 500,
        false, true }
    },
    // Prod OMC:
    //-----------------------------------------------------------------------//
    { FIXEnvT::ProdORD,
    //-----------------------------------------------------------------------//
      { "", "10.144.50.30", 20002, false, "NTPRO",    2, 5, 1, 5, 500,
       true, false  }
    }
  };
} // End namespace NTProgress
} // End namespace MAQUETTE
