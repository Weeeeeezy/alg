// vim:ts=2
//===========================================================================//
//                          "Venues/Binance/SecDefs.h":                      //
//                 Statically-Configured SecDefs for Binance                 //
//===========================================================================//
#pragma once
#include "Basis/SecDefs.h"
#include <vector>

namespace MAQUETTE
{
namespace Binance
{
  UTXX_ENUM(
  AssetClassT, int,
    FutT,      // USDT-settled perpetual and quarterly futures
    FutC,      // Coin-settled perpetual and quarterly futures
    Spt        // Spot
  );

  //=========================================================================//
  // "SecDef"s for Binance:                                                  //
  //=========================================================================//
  extern std::vector<SecDefS> const SecDefs_FutT;
  extern std::vector<SecDefS> const SecDefs_FutC;
  extern std::vector<SecDefS> const SecDefs_Spt;

  // "GetSecDefs":
  // For a Given AssetClass and ConnEnv:
  //
  std::vector<SecDefS> const&  GetSecDefs
    (AssetClassT::type a_ascl);

  // "GetSecDefs_All":
  // For All Tradable Securities (Fut + Opt) and a Given ConnEnv:
  //
  std::vector<SecDefS> const& GetSecDefs_All(MQTEnvT a_cenv);
} // End namespace Binance
} // End namespace MAQUETTE
