// vim:ts=2:et
//===========================================================================//
//                         "InfraStruct/StaticLimits.h":                     //
//       Settings and Functions Common to Multiple MAQUETTE Components       //
//===========================================================================//
#pragma once

namespace MAQUETTE
{
namespace Limits
{
  //=========================================================================//
  // Misc Constants (Limits):                                                //
  //=========================================================================//
  // Maximum number of Strategies which can run in a single application (in
  // particular, which can be attached to a single "EConnector*" instance):
  constexpr int  MaxStrats  = 64;

  // Maximum number of different Instruments (across all Strategies). Must be
  // sufficiently large for use with Crypto-Asset STP etc:
  constexpr int  MaxInstrs  = 8192;

  // Maximum number of different Assets (Ccys ets) across all Strategies:
  constexpr int  MaxAssets  = 8192;

  // Maximum number of OrderMgmt Connectors which can run in a single applica-
  // tion:
  constexpr int  MaxOMCs    = 64;

} // End namespace Limits
} // End namespace MAQUETTE
