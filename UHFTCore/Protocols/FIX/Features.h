// vim:ts=2:et
//===========================================================================//
//                          "Protocols/FIX/Features.h":                      //
//               Generic Formats for Most Common FIX Msg Types               //
//===========================================================================//
#pragma once
#include <utxx/enum.hpp>

namespace MAQUETTE
{
namespace FIX
{
  //=========================================================================//
  // Supported FIX Dialects for various Exchanges, ECNs and Brokers:         //
  //=========================================================================//
  UTXX_ENUM(
  DialectT, int,
    MICEX,          // Sub-version of FIX.4.4
    FORTS,          // Sub-version of FIX.4.4
    AlfaFIX2,       // Sub-version of FIX.4.2
    FXBA,           // Sub-version of FIX.4.4
    AlfaECN,        // Sub-version of FIX.4.4, close to FXBA but not identical
    NTProgress,     // Similar     to AlfaECN
    OPICS_Proxy,    // Sub-version of FIX.4.3
    EBS,            // Sub-version of FIX.5.0
    HotSpotFX_Gen,  // Sub-version of FIX.4.2
    HotSpotFX_Link, // Ditto
    Currenex,       // Sub-version of FIX.4.4 (for FX)
    Cumberland,     // Sub-version of FIX.4.4
    LMAX,           // Sub-version of FIX 4.2
    TT              // Sub-version of FIX 4.4
  );

  //=========================================================================//
  // Application-Level FIX Protocol Features, for each "Dialect":            //
  //=========================================================================//
  template<DialectT::type D>
  struct ProtocolFeatures;

} // End namespace FIX
} // End namespace MAQUETTE
