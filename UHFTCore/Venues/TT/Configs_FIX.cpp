// vim:ts=2:et
//===========================================================================//
//                         "Venues/TT/Configs_FIX.cpp":                      //
//                    Session Configs for TT FIX Connector                   //
//===========================================================================//
#include "Venues/TT/Configs_FIX.hpp"

using namespace std;

namespace MAQUETTE
{
namespace TT
{
  //=========================================================================//
  // "FIXSessionConfigs":                                                    //
  //=========================================================================//
  // NB: In the Configs below:
  // .m_UseTLS              = true
  // .m_HeartBeatSec        = 30
  // .m_LogOnTimeOutSec     = 15
  // .m_LogOffTimeOutSec    = 5
  // .m_ReConnectSec        = 5
  // .m_MaxGapMSec          = 500 (how long to wait for sync after ResendReq)
  // .m_IsOrdMgmt, .m_IsMktData:  depend on Env
  //
  map<FIXEnvT::type, FIXSessionConfig const> const FIXSessionConfigs
  {
    //=======================================================================//
    // Prod over Internet in CH, NY, AU:                                     //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    { FIXEnvT::Prod_CH_INet_MKT,
    //-----------------------------------------------------------------------//
      { "fixsecurityinfo-ext-prod-live.trade.tt", "64.74.225.4", 11703,
        true, "TT", 30, 15, 5, 5, 500, false, true }
    },
    //-----------------------------------------------------------------------//
    { FIXEnvT::Prod_CH_INet_ORD,
    //-----------------------------------------------------------------------//
      { "fixorderrouting-ext-prod-live.trade.tt", "64.74.225.4", 11702,
        true, "TT", 30, 15, 5, 5, 500, true, false }
    },
    //-----------------------------------------------------------------------//
    { FIXEnvT::Prod_NY_INet_MKT,
    //-----------------------------------------------------------------------//
      { "fixsecurityinfo-ext-prod-live.trade.tt", "74.201.141.4", 11703,
        true, "TT", 30, 15, 5, 5, 500, false, true }
    },
    //-----------------------------------------------------------------------//
    { FIXEnvT::Prod_NY_INet_ORD,
    //-----------------------------------------------------------------------//
      { "fixorderrouting-ext-prod-live.trade.tt", "74.201.141.4", 11702,
        true, "TT", 30, 15, 5, 5, 500, true, false }
    },
    //-----------------------------------------------------------------------//
    { FIXEnvT::Prod_AU_INet_MKT,
    //-----------------------------------------------------------------------//
      { "fixsecurityinfo-ext-prod-live.trade.tt", "64.124.221.4", 11703,
        true, "TT", 30, 15, 5, 5, 500, false, true }
    },
    //-----------------------------------------------------------------------//
    { FIXEnvT::Prod_AU_INet_ORD,
    //-----------------------------------------------------------------------//
      { "fixorderrouting-ext-prod-live.trade.tt", "64.124.221.4", 11702,
        true, "TT", 30, 15, 5, 5, 500, true, false }
    },
    //=======================================================================//
    // Test over Internet in CH, NY, AU:                                     //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    { FIXEnvT::Test_CH_INet_MKT,
    //-----------------------------------------------------------------------//
      { "fixorderrouting-ext-uat-cert.trade.tt", "64.74.225.7", 11703,
        true, "TT", 30, 15, 5, 5, 500, false, true }
    },
    //-----------------------------------------------------------------------//
    { FIXEnvT::Test_CH_INet_ORD,
    //-----------------------------------------------------------------------//
      { "fixorderrouting-ext-uat-cert.trade.tt", "64.74.225.7", 11702,
        true, "TT", 30, 15, 5, 5, 500, true, false }
    },
    //-----------------------------------------------------------------------//
    { FIXEnvT::Test_NY_INet_MKT,
    //-----------------------------------------------------------------------//
      { "fixsecurityinfo-ext-uat-cert.trade.tt", "74.201.141.7", 11703,
        true, "TT", 30, 15, 5, 5, 500, false, true }
    },
    //-----------------------------------------------------------------------//
    { FIXEnvT::Test_NY_INet_ORD,
    //-----------------------------------------------------------------------//
      { "fixorderrouting-ext-uat-cert.trade.tt", "74.201.141.7", 11702,
        true, "TT", 30, 15, 5, 5, 500, true, false }
    },
    //-----------------------------------------------------------------------//
    { FIXEnvT::Test_AU_INet_MKT,
    //-----------------------------------------------------------------------//
      { "fixsecurityinfo-ext-uat-cert.trade.tt", "64.124.221.7", 11703,
        true, "TT", 30, 15, 5, 5, 500, false, true }
    },
    //-----------------------------------------------------------------------//
    { FIXEnvT::Test_AU_INet_ORD,
    //-----------------------------------------------------------------------//
      { "fixorderrouting-ext-uat-cert.trade.tt", "64.124.221.7", 11702,
        true, "TT", 30, 15, 5, 5, 500, true, false }
    },
  };
} // End namespace TT
} // End namespace MAQUETTE
