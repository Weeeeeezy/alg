// vim:ts=2:et
//===========================================================================//
//                       "Venues/AlfaECN/Configs_FIX.cpp":                   //
//                   Session Configs for AlfaECN FIX Connector               //
//===========================================================================//
#include "Venues/AlfaECN/Configs_FIX.hpp"

using namespace std;

namespace MAQUETTE
{
namespace AlfaECN
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
    { FIXEnvT::Test1MKT,
    //-----------------------------------------------------------------------//
      // "ARobot11.moscow.alfaintra.net":
      { "", "172.19.16.143",  21001, false, "EcnMD", 20, 5, 1, 20, 500,
        false, true }
    },
    //-----------------------------------------------------------------------//
    { FIXEnvT::Test1ORD,
    //-----------------------------------------------------------------------//
      // "ARobot11.moscow.alfaintra.net":
      { "", "172.19.16.143",  21002, false, "EcnOD", 20, 5, 1, 20, 500,
        true, false }
    },
    //-----------------------------------------------------------------------//
    { FIXEnvT::Test2MKT,
    //-----------------------------------------------------------------------//
      // "ARobot12.moscow.alfaintra.net":
      { "", "172.19.16.144",  21001, false, "EcnMD", 20, 5, 1, 20, 500,
        false, true }
    },
    //-----------------------------------------------------------------------//
    { FIXEnvT::Test2ORD,
    //-----------------------------------------------------------------------//
      // "ARobot12.moscow.alfaintra.net":
      { "", "172.19.16.144",  21002, false, "EcnOD", 20, 5, 1, 20, 500,
        true, false }
    },
    //-----------------------------------------------------------------------//
    { FIXEnvT::ProdMKT,
    //-----------------------------------------------------------------------//
      // "ARobot14.moscow.alfaintra.net":
      { "", "172.19.16.206",  21001, false, "EcnMD", 20, 5, 1, 20, 500,
        false, true }
    },
    //-----------------------------------------------------------------------//
    { FIXEnvT::ProdORD,
    //-----------------------------------------------------------------------//
      // "ARobot14.moscow.alfaintra.net":
      { "", "172.19.16.206",  21002, false, "EcnOD", 20, 5, 1, 20, 500,
        true, false }
    }
  };
} // End namespace AlfaECN
} // End namespace MAQUETTE
