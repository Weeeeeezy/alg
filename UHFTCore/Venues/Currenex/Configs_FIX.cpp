// vim:ts=2:et
//===========================================================================//
//                      "Venues/Currenex/Configs_FIX.cpp":                   //
//                 Session Configs for Currenex FIX Connector               //
//===========================================================================//
#include "Venues/Currenex/Configs_FIX.hpp"

using namespace std;

namespace MAQUETTE
{
namespace Currenex
{
  //=========================================================================//
  // "FIXSessionConfigs":                                                    //
  //=========================================================================//
  // NB: In the Configs below:
  // .m_UseTLS              = true
  // .m_HeartBeatSec        = 2
  // .m_LogOnTimeOutSec     = 5
  // .m_LogOffTimeOutSec    = 1
  // .m_ReConnectSec        = 5
  // .m_MaxGapMSec          = 500 (how long to wait for sync after ResendReq)
  // .m_IsOrdMgmt, .m_IsMktData:  depend on Env
  //
  map<FIXEnvT::type, FIXSessionConfig const> const FIXSessionConfigs
  {
    //=======================================================================//
    // Prod in NY6, LD4 and KVH (TK1):                                       //
    //=======================================================================//
    // XXX: CoLo configs are not available yet:
    //-----------------------------------------------------------------------//
    { FIXEnvT::Prod_NY6_INet_MKT,
    //-----------------------------------------------------------------------//
      { "fxtrades-fixstream.currenex.com",     "63.111.184.125", 443, true,
        "CNX", 2, 5, 1, 5, 500, false, true }
    },
    //-----------------------------------------------------------------------//
    { FIXEnvT::Prod_NY6_INet_ORD,
    //-----------------------------------------------------------------------//
      { "fxtrades-fixtrade.currenex.com",      "63.111.184.126", 443, true,
        "CNX", 2, 5, 1, 5, 500, true, false }
    },
    //-----------------------------------------------------------------------//
    { FIXEnvT::Prod_LD4_INet_MKT,
    //-----------------------------------------------------------------------//
      { "fxtrades-fixstream.ld4.currenex.com", "91.229.92.125", 443, true,
        "CNX", 2, 5, 1, 5, 500, false, true }
    },
    //-----------------------------------------------------------------------//
    { FIXEnvT::Prod_LD4_INet_ORD,
    //-----------------------------------------------------------------------//
      { "fxtrades-fixtrade.ld4.currenex.com",  "91.229.92.126", 443, true,
        "CNX", 2, 5, 1, 5, 500, true, false }
    },
    //-----------------------------------------------------------------------//
    { FIXEnvT::Prod_KVH_INet_MKT,
    //-----------------------------------------------------------------------//
      { "fxtrades-fixstream.tk1.currenex.com", "103.22.167.125", 443, true,
        "CNX", 2, 5, 1, 5, 500, false, true }
    },
    //-----------------------------------------------------------------------//
    { FIXEnvT::Prod_KVH_INet_ORD,
    //-----------------------------------------------------------------------//
      { "fxtrades-fixtrade.tk1.currenex.com",  "103.22.167.126", 443, true,
        "CNX", 2, 5, 1, 5, 500, true, false }
    },
  };
} // End namespace Currenex
} // End namespace MAQUETTE
