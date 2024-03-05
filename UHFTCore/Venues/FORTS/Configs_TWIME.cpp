// vim:ts=2:et
//===========================================================================//
//                      "Venues/FORTS/Configs_TWIME.cpp":                    //
//                  Session Configs for FORTS TWIME Connector                //
//===========================================================================//
#include "Venues/FORTS/Configs_TWIME.hpp"

using namespace std;

namespace MAQUETTE
{
namespace FORTS
{
  //=========================================================================//
  // "TWIMESessionConfigs":                                                  //
  //=========================================================================//
  // NB: In all Configs below:
  // .m_HeartBeatSec        =    30
  // .m_LogOnTimeOutSec     =     5
  // .m_LogOffTimeOutSec    =     1
  // .m_ReConnectSec        =    10
  // .m_MaxGapMSec          =   500 (waiting for sync after ResendReq)
  //
  map<OMCEnvT::type, TWIMESessionConfig>
  const TWIMESessionConfigs
  {
    //-----------------------------------------------------------------------//
    // Test Configs:                                                         //
    //-----------------------------------------------------------------------//
    { OMCEnvT::TestL,
      { "91.203.252.38",  9000, "91.203.252.38",  9001, 30, 5, 1, 10, 500 }
    },
    { OMCEnvT::TestI,
      { "91.208.232.244", 9000, "91.208.232.244", 9001, 30, 5, 1, 10, 500 }
    },
    //-----------------------------------------------------------------------//
    // Production Configs:                                                   //
    //-----------------------------------------------------------------------//
    { OMCEnvT::Prod,
      { "91.203.252.32",  9000, "91.203.254.32",  9001, 30, 5, 1, 10, 500 }
    }
  };
} // End namespace FORTS
} // End namespace MAQUETTE
