// vim:ts=2:et
//===========================================================================//
//                       "Venues/MICEX/Configs_FIX.cpp":                     //
//                   Session Configs for MICEX FIX Connector                 //
//===========================================================================//
#include "Venues/MICEX/Configs_FIX.hpp"

using namespace std;

namespace MAQUETTE
{
namespace MICEX
{
  //=========================================================================//
  // "FIXSessionConfigs":                                                    //
  //=========================================================================//
  // NB: In all Configs below:
  // .m_ServerName          = ""
  // .m_UseTLS              = false
  // .m_HeartBeatSec        = 30
  // .m_LogOnTimeOutSec     = 5
  // .m_LogOffTimeOutSec    = 1
  // .m_ReConnectSec        = 10
  // .m_MaxGapMSec          = 500   (waiting for sync  after ResendReq)
  // .m_IsOrdMgmt           = true
  // .m_IsMktData           = false (there is no MktData FIX for MICEX)
  //
  map<pair<FIXEnvT::type, AssetClassT::type>, FIXSessionConfig const> const
  FIXSessionConfigs
  {
    //-----------------------------------------------------------------------//
    // Production Configs:                                                   //
    //-----------------------------------------------------------------------//
    // XXX: "Prod1" is NOT recommended at any time:
    { make_pair(FIXEnvT::Prod1,  AssetClassT::EQ),
      { "", "91.203.253.30",  9120, false, "GWMFIX1EQ",       30, 5, 1, 10, 500,
        true, false }
    },
    { make_pair(FIXEnvT::Prod1,  AssetClassT::FX),
      { "", "91.203.252.29",  9212, false, "GWMFIX1CUR",      30, 5, 1, 10, 500,
        true, false }
    },
    //
    // Prod2{0-1} are the top-most configurations for both EQ and FX:
    // EQ:
    { make_pair(FIXEnvT::Prod20, AssetClassT::EQ),
      { "", "91.203.252.25",  9120, false, "DFIX1-EQ",        30, 5, 1, 10, 500,
        true, false }
    },
    { make_pair(FIXEnvT::Prod21, AssetClassT::EQ),
      { "", "91.203.253.25",  9120, false, "DFIX1-EQ",        30, 5, 1, 10, 500,
        true, false }
    },
    // FX:
    { make_pair(FIXEnvT::Prod20, AssetClassT::FX),
      { "", "91.203.252.26",  9212, false, "DFIX1-CUR",       30, 5, 1, 10, 500,
        true, false }
    },
    { make_pair(FIXEnvT::Prod21, AssetClassT::FX),
      { "", "91.203.253.26",  9212, false, "DFIX1-CUR",       30, 5, 1, 10, 500,
        true, false }
    },
    //
    // Prod3{0-1} is currently for FX only:
    //
    { make_pair(FIXEnvT::Prod30, AssetClassT::FX),
      { "", "91.203.252.19",  9212, false, "DFIX2-CUR",       30, 5, 1, 10, 500,
        true, false }
    },
    { make_pair(FIXEnvT::Prod31, AssetClassT::FX),
      { "", "91.203.253.19",  9212, false, "DFIX2-CUR",       30, 5, 1, 10, 500,
        true, false }
    },

    //-----------------------------------------------------------------------//
    // UAT and Test Configs (Local):                                         //
    //-----------------------------------------------------------------------//
    // UATL:
    { make_pair(FIXEnvT::UATL1,   AssetClassT::EQ),
      { "", "91.203.253.239", 39125, false, "MFIX-EQ-UAT",    30, 5, 1, 10, 500,
        true, false }
    },
    { make_pair(FIXEnvT::UATL2,   AssetClassT::EQ),
      { "", "91.203.255.238", 39125, false, "MFIX-EQ-UAT",    30, 5, 1, 10, 500,
        true, false }
    },
    { make_pair(FIXEnvT::UATL1,   AssetClassT::FX),
      { "", "91.203.253.239", 39215, false, "MFIX-CUR-UAT",   30, 5, 1, 10, 500,
        true, false }
    },
    { make_pair(FIXEnvT::UATL2,   AssetClassT::FX),
      { "", "91.203.255.238", 39215, false, "MFIX-CUR-UAT",   30, 5, 1, 10, 500,
        true, false }
    },

    // TestL:
    { make_pair(FIXEnvT::TestL1,  AssetClassT::EQ),
      { "", "91.203.253.239", 39120,  false, "MFIXTradeID",   30, 5, 1, 10, 500,
        true, false }
    },
    { make_pair(FIXEnvT::TestL2,  AssetClassT::EQ),
      { "", "91.203.255.238", 39120,  false, "MFIXTradeID",   30, 5, 1, 10, 500,
        true, false }
    },
    { make_pair(FIXEnvT::TestL1,  AssetClassT::FX),
      { "", "91.203.253.239", 39212, false, "MFIXTradeIDCurr",
        30, 5, 1, 10, 500, true, false }
    },
    { make_pair(FIXEnvT::TestL2,  AssetClassT::FX),  // XXX: May not work!
      { "", "91.203.255.238", 39212, false, "MFIXTradeIDCurr",
        30, 5, 1, 10, 500, true, false }
    },
    //-----------------------------------------------------------------------//
    // UAT and Test Configs (Internet):                                      //
    //-----------------------------------------------------------------------//
    { make_pair(FIXEnvT::UATI,   AssetClassT::EQ),
      { "", "91.208.232.200", 9130, false, "IFIX-EQ-UAT",     30, 5, 1, 10, 500,
        true, false }
    },
    { make_pair(FIXEnvT::UATI,   AssetClassT::FX),
      { "", "91.208.232.200", 9222, false, "IFIX-CUR-UAT",    30, 5, 1, 10, 500,
        true, false }
    },
    { make_pair(FIXEnvT::TestI,  AssetClassT::EQ),
      { "", "91.208.232.200", 9120, false, "MFIXTradeID",     30, 5, 1, 10, 500,
        true, false }
    },
    { make_pair(FIXEnvT::TestI,  AssetClassT::FX),
      { "", "91.208.232.200", 9212, false, "MFIXTradeIDCurr", 30, 5, 1, 10, 500,
        true, false }
    }
  };
} // End namespace MICEX
} // End namespace MAQUETTE
