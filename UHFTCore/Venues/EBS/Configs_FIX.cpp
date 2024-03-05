// vim:ts=2:et
//===========================================================================//
//                       "Venues/EBS/Configs_FIX.cpp":                       //
//                   Session Configs for EBS FIX Connector                   //
//===========================================================================//
#include "Venues/EBS/Configs_FIX.hpp"

using namespace std;

namespace MAQUETTE
{
namespace EBS
{
  //=========================================================================//
  // "FIXSessionConfigs":                                                    //
  //=========================================================================//
  // NB: In the Configs below:
  // .m_ServerName          = ""
  // .m_UseTLS              = false
  // .m_HeartBeatSec        = 1
  // .m_LogOnTimeOutSec     = 5
  // .m_LogOffTimeOutSec    = 1
  // .m_ReConnectSec        = 20
  // .m_MaxGapMSec          = 500 (how long to wait for sync after ResendReq)
  // .m_IsOrdMgmt, .m_IsMktData:  depend on Env
  //
  map<FIXEnvT::type, FIXSessionConfig const> const FIXSessionConfigs
  {
    //-----------------------------------------------------------------------//
    { FIXEnvT::Test1SESSION,
    //-----------------------------------------------------------------------//
      { "", "", 0, false, "ICAP_Ai_Server", 1, 5, 1, 20, 500, false, true }
    }
  };
} // End namespace EBS
} // End namespace MAQUETTE
