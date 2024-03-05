// vim:ts=2:et
//===========================================================================//
//                    "Venues/Cumberland/Configs_FIX.cpp":                   //
//               Session Configs for Cumberland FIX Connector               //
//===========================================================================//
#include "Venues/Cumberland/Configs_FIX.hpp"

using namespace std;

namespace MAQUETTE
{
namespace Cumberland
{
  //=========================================================================//
  // "FIXSessionConfigs":                                                    //
  //=========================================================================//
  // NB: In the Configs below:
  // .m_UseTLS              = true
  // .m_HeartBeatSec        = 30
  // .m_LogOnTimeOutSec     = 5
  // .m_LogOffTimeOutSec    = 1
  // .m_ReConnectSec        = 5
  // .m_MaxGapMSec          = 500 (how long to wait for sync after ResendReq)
  // .m_IsOrdMgmt: depends on Env
  //
  map<FIXEnvT::type, FIXSessionConfig const> const FIXSessionConfigs
  {
    //=======================================================================//
    // Via Internet:                                                         //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    { FIXEnvT::Test_INet,
    //-----------------------------------------------------------------------//
      { "api-v00-cert.cumberlandmining.com",     "204.75.143.83", 20001, true,
        "cumberland", 30, 5, 1, 5, 500, true, true }
    },
    //-----------------------------------------------------------------------//
    { FIXEnvT::Prod_INet,
    //-----------------------------------------------------------------------//
      { "api-v00-prod.cumberland.drw.com",       "74.119.41.108", 20001, true,
        "cumberland", 30, 5, 1, 5, 500, true, true }
    },
  };
} // End namespace Cumberland
} // End namespace MAQUETTE
