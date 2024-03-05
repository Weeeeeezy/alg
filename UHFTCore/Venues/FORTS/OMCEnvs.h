// vim:ts=2:et
//===========================================================================//
//                             "FORTS/OMCEnvs.h":                            //
//         Declaration of FIX, TWIME and PlazaII OMC Environments            //
//===========================================================================//
#pragma  once
#include <utxx/enum.hpp>

namespace MAQUETTE
{
namespace FORTS
{
  //=========================================================================//
  // "OMCEnvT" Enum:                                                         //
  //=========================================================================//
  // These envs are same for FIX and
  UTXX_ENUM(
    OMCEnvT, int,
      Prod,         // Production
      TestL,        // Test - Co-Location (Local)
      TestI         // Test - Internet
    );

} // End namespace FORTS
} // End namespace MAQUETTE
