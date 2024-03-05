// vim:ts=2:et
//===========================================================================//
//                    "Venues/HotSpotFX/Configs_ITCH.cpp":                   //
//===========================================================================//
#include "Venues/HotSpotFX/Configs_ITCH.hpp"
using namespace std;

namespace MAQUETTE
{
namespace HotSpotFX
{
  //=========================================================================//
  // "TCP_ITCH_Configs_Gen":                                                 //
  //=========================================================================//
  // NB: The Keys format is compatible with that of HotSpotFX FIX:
  //
  map<string, TCP_ITCH_Config_HotSpotFX> const TCP_ITCH_Configs_Gen
  {
    //-----------------------------------------------------------------------//
    // Prod Generic Envs:                                                    //
    //-----------------------------------------------------------------------//
    // Prod viewing account provides Indicative Generic pxs from NY5 HotSpot:
    // WithExtras=false:
    //
    { "ALFA-NY5_Gen_Gen-TCP_ITCH-HotSpotFX-Prod",
      { "195.3.208.37",  8211, "ltimochouk2view", "Hotspot1", 1, 5, 1, 10,
        false }
    },

    // As above, but with MinQtys and LotSzs (WithExtras=true):
    //
    { "ALFA-NY5_Ext_Gen-TCP_ITCH-HotSpotFX-Prod",
      { "195.3.208.37",  8212, "ltimochouk2view", "Hotspot1", 1, 5, 1, 10,
        true  }
    },

    // Prod account for RUB trading on Generic HotSpotFX in NY5, Generic  Pxs.
    // Broker is INTL FCStone: Same temporal params as above. XXX: BEWARE: it
    // is NOT so useful, because of our MM status there -- we can't see other
    // MMs' liquidity through this feed, -- only the quotes posted by partici-
    // pants without the MM status!
    // WithExtras=false:
    //
    { "ALFA-NY5_Gen_INTL-TCP_ITCH-HotSpotFX-Prod",
      { "195.3.208.37",  8211, "ife10100md",      "Hotspot1", 1, 5, 10, 1,
        false }
    },

    // As above, but with MinQtys and LotSzs (WithExtras=true):
    //
    { "ALFA-NY5_Ext_INTL-TCP_ITCH-HotSpotFX-Prod",
      { "195.3.208.37",  8212, "ife10100md",      "Hotspot1", 1, 5, 10, 1,
        true  }
    },

    // Prod account for G10/G20 taking from Generic HotSPotFX in NY5, broker is
    // UBS: WithExtras=false:
    //
    { "ALFA2-NY5_Gen_UBS-TCP_ITCH-HotSpotFX-Prod",
      { "195.3.208.37",  8211, "PBALFA2md",       "Hotspot",  1, 5, 10, 1,
        false }
    },

    // As above, but with MinQtys and LotSzs (WithExtras=true):
    //
    { "ALFA2-NY5_Ext_UBS-TCP_ITCH-HotSpotFX-Prod",
      { "195.3.208.37",  8212, "PBALFA2md",       "Hotspot",  1, 5, 10, 1,
        true  }
    },

    //-----------------------------------------------------------------------//
    // UAT Generic Envs:                                                     //
    //-----------------------------------------------------------------------//
    // Generic UAT: WithExtras=false:
    //
    { "ALFA-NY5_Gen_Gen-TCP_ITCH-HotSpotFX-UAT",
      { "74.115.129.6",   18276, "ny5alfa1itch",  "hotspot",  1, 5, 10, 1,
        false }
    },

    // As above, but with MinQtys and LotSzs (WithExtras=true):
    //
    { "ALFA-NY5_Ext_Gen-TCP_ITCH-HotSpotFX-UAT",
      { "74.115.129.6",   18277, "ny5alfa1itch",  "hotspot",  1, 5, 10, 1,
        true  }
    },

    // Generic UAT: WithExtras=false:
    //
    { "BLS-NY5_Gen_Gen-TCP_ITCH-HotSpotFX-Test",
      { "74.115.129.6",   18276, "ny5BLSpartanUATitch", "hotspot",  1, 5, 10, 1,
        false }
    },

    // As above, but with MinQtys and LotSzs (WithExtras=true):
    //
    { "BLS-NY5_Ext_Gen-TCP_ITCH-HotSpotFX-Test",
      { "74.115.129.6",   18277, "ny5BLSpartanUATitch", "hotspot",  1, 5, 10, 1,
        true  }
    }
  };

  //=========================================================================//
  // "TCP_ITCH_Configs_Link":                                                //
  //=========================================================================//
  map<string, TCP_ITCH_Config_HotSpotFX> const TCP_ITCH_Configs_Link
  {
    //-----------------------------------------------------------------------//
    // CCMA Prod Envs:                                                       //
    //-----------------------------------------------------------------------//
    // Prod account for CCM Alpha over HotSpot Link in LD4, PrimeBroker is UBS:
    // HeartBeat=1 sec, LogOn=5 sec, ReConn=10 sec, WithExtras=false:
    //
    { "ALFA-LD4_CCMA_UBS-TCP_ITCH-HotSpotFX-Prod",
      { "195.3.208.45",  8180, "ldnPBALFAitch",   "Hotspot",  1, 5, 10, 1,
        false }
    },

    //-----------------------------------------------------------------------//
    // CCMA UAT Envs:                                                        //
    //-----------------------------------------------------------------------//
    // UAT for CCM Alpha: WithRelaxedOBs=false:
    //
    { "ALFA-NY5_CCMA_Gen-TCP_ITCH_HotSpotFX-UAT",
      { "208.90.208.252", 18299, "ny5alfa1itch",  "hotspot",  1, 5, 10, 1,
        false }
    }
  };
} // End namespace HotSpotFX
} // End namespace MAQUETTE
