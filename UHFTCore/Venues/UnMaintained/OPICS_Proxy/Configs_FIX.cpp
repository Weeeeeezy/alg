// vim:ts=2:et
//===========================================================================//
//                  "Venues/OPICS_Proxy/Configs_FIX.cpp":                    //
//            Provides both Server Configs and Client Accounts               //
//===========================================================================//
// NB: The params specified here must be consistent with the settings of the
// OPICS Proxy Server:
//
#include "Venues/UnMaintained/OPICS_Proxy/Configs_FIX.h"

using namespace std;

namespace MAQUETTE
{
namespace OPICS_Proxy
{
  //=========================================================================//
  // "FIXSessionConfigs":                                                    //
  //=========================================================================//
  // NB: In the Configs below:
  // .m_ServerName          = ""
  // .m_UseTLS              = false
  // .m_HeartBeatSec        = 30
  // .m_LogOnTimeOutSec     = 5
  // .m_LogOffTimeOutSec    = 1
  // .m_ReConnectSec        = 10
  // .m_MaxGapMSec          = 0 (ReSends are not allowed)
  // .m_IsOrdMgmt           = true (though in fact is is STP, not OMC)
  //
  map<bool, FIXSessionConfig> const FIXSessionConfigs
  {
    // Prod:     On "ARobot20" (London LD4):
    { true,
      { "", "172.19.16.137", 23501, false, "A3_OPICS_PROXY", 30, 5, 1, 10, 0,
        true }
    },
    // UAT/Test: On "ARobot17" (Moscow DSP):
    { false,
      { "", "172.19.16.199", 23501, false, "A3_OPICS_PROXY", 30, 5, 1, 10, 0,
        true }
    }
  };

  //=========================================================================//
  // "FIXAccountConfigs":                                                    //
  //=========================================================================//
  // NB: We allow, on average, no more than 1 req per sec, or more preciely,
  // 1000 reqs per 1000 sec:
  // XXX: Currently using idential Prod and UAT/Test account details:
  //
  map<bool, FIXAccountConfig> const FIXAccountConfigs
  {
    { true,
      { "A3_OPICS_CLIENT", "", "vonBismarck", "PrinceOtto",
        "", "", "", '\0', 0, 1000, 1000
    } },

    { false,
      { "A3_OPICS_CLIENT", "", "vonBismarck", "PrinceOtto",
        "", "", "", '\0', 0, 1000, 1000
    } }
  };
} // End namespace OPICS_Proxy
} // End namespace MAQUETTE
