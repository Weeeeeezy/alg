// vim:ts=2:et
//===========================================================================//
//                     "Venues/HotSpotFX/Configs_FIX.cpp":                   //
//                 Session Configs for HotSpotFX FIX Connector               //
//===========================================================================//
#include "Venues/HotSpotFX/Configs_FIX.hpp"

using namespace std;

namespace MAQUETTE
{
namespace HotSpotFX
{
  //=========================================================================//
  // "FIXSessionConfigs_Gen":                                                //
  //=========================================================================//
  // NB: In the Configs below:
  // .m_ServerName          = ""
  // .m_UseTLS              = false
  // .m_HeartBeatSec        = 30
  // .m_LogOnTimeOutSec     = 5
  // .m_LogOffTimeOutSec    = 1
  // .m_ReConnectSec        = 5
  // .m_MaxGapMSec          = 500 (how long to wait for sync after ResendReq)
  // .m_IsOrdMgmt, .m_IsMktData: depend on Env
  //
  map<FIXEnvT_Gen::type, FIXSessionConfig const> const FIXSessionConfigs_Gen
  {
    //=======================================================================//
    // Test over the Internet:                                               //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    { FIXEnvT_Gen::Test_Int_Gen_MKT,
    //-----------------------------------------------------------------------//
      { "", "74.115.129.6",  17153, false, "HSFX-FIX-BRIDGE", 30, 5, 1, 5, 500,
        false, true }
    },
    //-----------------------------------------------------------------------//
    { FIXEnvT_Gen::Test_Int_Gen_ORD,
    //-----------------------------------------------------------------------//
      { "", "74.115.129.6",  16128, false, "HSFX-FIX-BRIDGE", 30, 5, 1, 5, 500,
        true, false }
    },

    //=======================================================================//
    // Prod over the Internet (actually coming from NY5):                    //
    //=======================================================================//
    // (XXX: Currently MKT only, no ORD):
    //-----------------------------------------------------------------------//
    { FIXEnvT_Gen::Prod_Int_Gen_MKT,
    //-----------------------------------------------------------------------//
      { "", "74.115.129.4",   7041, false, "HSFX-FIX-BRIDGE", 30, 5, 1, 5, 500,
        false, true }
    },

    //=======================================================================//
    // Prod in NY5 Co-Location: Depends on the Broker:                       //
    //=======================================================================//
    // XXX: Currently only ORD -- because MktData are coming over ITCH:
    //-----------------------------------------------------------------------//
    { FIXEnvT_Gen::Prod_NY5_INTL_ORD,
    //-----------------------------------------------------------------------//
      { "", "208.90.208.12",  6025, false, "HSFX-FIX-BRIDGE", 30, 5, 1, 5, 500,
        true, false }
    },
    //-----------------------------------------------------------------------//
    { FIXEnvT_Gen::Prod_NY5_UBS_ORD,
    //-----------------------------------------------------------------------//
      { "", "208.90.208.14",  6069, false, "HSFX-FIX-BRIDGE", 30, 5, 1, 5, 500,
        true, false }
    }
  };

  //=========================================================================//
  // "FIXSessionConfigs_Link":                                               //
  //=========================================================================//
  // Similar to "FIXSessionCobfigs_Gen" above:
  //
  map<FIXEnvT_Link::type, FIXSessionConfig const> const FIXSessionConfigs_Link
  {
    //=======================================================================//
    // Prod CCM Alpha Link over the Internet (actually coming from LD4):     //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    { FIXEnvT_Link::Prod_Int_CCMA_MKT,
    //-----------------------------------------------------------------------//
      { "", "195.93.197.194", 7100, false, "HSFX-FIX-BRIDGE", 30, 5, 1, 5, 500,
        false, true }
    },
    //-----------------------------------------------------------------------//
    { FIXEnvT_Link::Prod_Int_CCMA_ORD,
    //-----------------------------------------------------------------------//
      { "", "195.93.197.193", 6019, false, "HSFX-FIX-BRIDGE", 30, 5, 1, 5, 500,
        true, false }
    },

    //=======================================================================//
    // Prod CCM Alpha Link, LD4 Co-Location:                                 //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    { FIXEnvT_Link::Prod_LD4_CCMA_MKT,
    //-----------------------------------------------------------------------//
      { "", "195.3.208.38",  27380, false, "HSFX-FIX-BRIDGE", 30, 5, 1, 5, 500,
        false, true }
    },
    //-----------------------------------------------------------------------//
    { FIXEnvT_Link::Prod_LD4_CCMA_ORD,
    //-----------------------------------------------------------------------//
      { "", "195.3.208.5",    6016, false, "HSFX-FIX-BRIDGE", 30, 5, 1, 5, 500,
        true, false }
    }
  };
} // End namespace HotSpotFX
} // End namespace MAQUETTE