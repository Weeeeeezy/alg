// vim:ts=2:et
//===========================================================================//
//                    "Venues/HotSpotFX/Features_ITCH.h":                    //
//          ITCH Protocol Static Features for HotSpotFX_{Gen,Link}           //
//===========================================================================//
#pragma once
#include "Protocols/ITCH/Features.h"

namespace MAQUETTE
{
namespace ITCH
{
  //=========================================================================//
  // "ProtocolFeatures<HotSpotFX_Gen>":                                      //
  //=========================================================================//
  // Generic HotSpotFX MktData Feed:
  template<>
  struct ProtocolFeatures<DialectT::HotSpotFX_Gen>
  {
    constexpr static bool s_hasFullAmount = false;
    constexpr static bool s_hasFracQtys   = true;
    constexpr static bool s_hasRelaxedOBs = true;  // Bid-Ask Collisions OK!
  };

  //=========================================================================//
  // "Features<HotSpotFX_Link>":                                             //
  //=========================================================================//
  // For Single-LP MktData Feeds over HotSpotFX Link, eg CCM Alpha:
  //
  template<>
  struct ProtocolFeatures<DialectT::HotSpotFX_Link>
  {
    constexpr static bool s_hasFullAmount = true;
    constexpr static bool s_hasFracQtys   = false;
    constexpr static bool s_hasRelaxedOBs = false;
  };
} // End namespace ITCH
} // End namespace MAQUETTE
