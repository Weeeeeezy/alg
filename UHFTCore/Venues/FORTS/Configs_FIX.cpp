// vim:ts=2:et
//===========================================================================//
//                       "Venues/FORTS/Configs_FIX.cpp":                     //
//                   Session Configs for FORTS FIX Connector                 //
//===========================================================================//
#include "Venues/FORTS/Configs_FIX.hpp"

using namespace std;

namespace MAQUETTE
{
namespace FORTS
{
  //=========================================================================//
  // "FIXSessionConfigs":                                                    //
  //=========================================================================//
  // NB: In all Configs below:
  // .m_ServerName          = "",
  // .m_UseTLS              = false,
  // .m_HeartBeatSec        = 30
  // .m_LogOnTimeOutSec     = 5
  // .m_LogOffTimeOutSec    = 1
  // .m_ReConnectSec        = 10
  // .m_MaxGapMSec          = 500   (waiting for sync  after ResendReq)
  // .m_IsOrdMgmt           = true
  // .m_IsMktData           = false (there is no Mktdata FIX for FORTS)
  //
  map<OMCEnvT::type, FIXSessionConfig const> const FIXSessionConfigs
  {
    //-----------------------------------------------------------------------//
    // Production Configs:                                                   //
    //-----------------------------------------------------------------------//
    { OMCEnvT::Prod,
      { "", "91.203.252.32", 6001, false, "FG", 30, 5, 1, 10, 500, true, false }
    }
    //-----------------------------------------------------------------------//
    // Test Configs:                                                         //
    //-----------------------------------------------------------------------//
    // TODO...
  };
} // End namespace FORTS
} // End namespace MAQUETTE
