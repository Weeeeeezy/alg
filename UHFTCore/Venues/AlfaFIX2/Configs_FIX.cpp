// vim:ts=2:et
//===========================================================================//
//                      "Venues/AlfaFIX2/Configs_FIX.cpp":                   //
//                    Session Configs for AlfaFIX2 Connector                 //
//===========================================================================//
#include "Venues/AlfaFIX2/Configs_FIX.hpp"

using namespace std;

namespace MAQUETTE
{
namespace AlfaFIX2
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
  // .m_MaxGapMSec          = 500 (waiting for sync after ResendReq)
  // .m_IsOrdMgmt, .m_IsMktData:  depend on Env
  //
  map<FIXEnvT::type, FIXSessionConfig const> const FIXSessionConfigs
  {
    //-----------------------------------------------------------------------//
    { FIXEnvT::ProdMKT1,
    //-----------------------------------------------------------------------//
      { "", "172.19.16.203", 35203, false, "SRV_MQT_MD",  20, 5, 1, 20, 500,
        false, true }
    },
    //-----------------------------------------------------------------------//
    { FIXEnvT::ProdORD1,
    //-----------------------------------------------------------------------//
      { "", "172.19.16.203", 35204, false, "SRV_MQT_OD",  20, 5, 1, 20, 500,
        true, false }
    },
    //-----------------------------------------------------------------------//
    { FIXEnvT::ProdMKT2,
    //-----------------------------------------------------------------------//
      { "", "172.19.16.203", 35213, false, "SRV_MQT2_MD", 20, 5, 1, 20, 500,
        false, true }
    },
    //-----------------------------------------------------------------------//
    { FIXEnvT::ProdORD2,    // XXX: Currently Non-Functional!
    //-----------------------------------------------------------------------//
      { "", "172.19.16.203", 35214, false, "SRV_MQT2_OD", 20, 5, 1, 20, 500,
        true, false }
    }
  };
} // End namespace AlfaFIX2
} // End namespace MAQUETTE
