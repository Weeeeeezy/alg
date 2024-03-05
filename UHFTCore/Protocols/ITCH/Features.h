// vim:ts=2:et
//===========================================================================//
//                         "Protocols/ITCH/Features.h":                      //
//                     Top-Level ITCH Dialects and Features                  //
//===========================================================================//
#pragma once

#include <utxx/config.h>
#include <utxx/enumv.hpp>

namespace MAQUETTE
{
namespace ITCH
{
  //=========================================================================//
  // Supported Dialects of the ITCH Protocol:                                //
  //=========================================================================//
  UTXX_ENUMV(DialectT, (int, 0),
    (HotSpotFX_Gen,  1)     // Generic HotSpotFX
    (HotSpotFX_Link, 2)     // Single LPs over HotSpotFX (eg CCM Alpha)
    (FastMatch,      3)     // TODO: also provide Gen and Single LP versions
    (Currenex,       4)
  );

  //=========================================================================//
  // Statically-Configured ITCH Protocol Features:                           //
  //=========================================================================//
  // Implemented by template specialisation:
  //
  template<DialectT::type D>
  struct ProtocolFeatures;

} // End namespace ITCH
} // End namespace MAQUETTE
