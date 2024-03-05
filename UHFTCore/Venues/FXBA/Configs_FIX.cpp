// vim:ts=2:et
//===========================================================================//
//                        "Venues/FXBA/Configs_FIX.cpp":                     //
//              Session Configs for FXBestAggregator FIX Connector           //
//===========================================================================//
#include "Venues/FXBA/Configs_FIX.hpp"

using namespace std;

namespace MAQUETTE
{
namespace FXBA
{
  //=========================================================================//
  // "FIXSessionConfigs":                                                    //
  //=========================================================================//
  // NB: In the Configs below:
  // .m_ServerName          = ""
  // .m_UseTLS              = false
  // .m_HeartBeatSec        = 20
  // .m_LogOnTimeOutSec     = 5
  // .m_LogOffTimeOutSec    = 1
  // .m_ReConnectSec        = 20
  // .m_MaxGapMSec          = 500 (how long to wait for sync after ResendReq)
  // .m_IsOrdMgmt, .m_IsMktData:  depend on Env
  //
  map<FIXEnvT::type, FIXSessionConfig const> const FIXSessionConfigs
  {
    //-----------------------------------------------------------------------//
    { FIXEnvT::TestMKT,
    //-----------------------------------------------------------------------//
      // "ARobotPhyAP.moscow.alfaintra.net":
      { "", "172.25.144.71", 31233, false, "FXBA2-MD-DEV", 20, 5, 1, 20, 500,
        false, true }
    },
    //-----------------------------------------------------------------------//
    { FIXEnvT::TestORD,
    //-----------------------------------------------------------------------//
      // "ARobotPhyAP.moscow.alfaintra.net":
      { "", "172.25.144.71", 31234, false, "FXBA2-OM-DEV", 20, 5, 1, 20, 500,
        true, false }
    }
  };
} // End namespace FXBA
} // End namespace MAQUETTE
