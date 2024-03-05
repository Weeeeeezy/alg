// vim:ts=2:et
//===========================================================================//
//            "Connectors/H2WS/Huobi/Traits.h"                               //
//===========================================================================//
// Defines different settings for Assets
#pragma once

#include "Venues/Huobi/SecDefs.h"

namespace MAQUETTE
{
namespace Huobi
{
  template <Huobi::AssetClassT::type AC>
  struct Traits;

  template <>
  struct Traits<AssetClassT::Spt>
  {
    constexpr static char MdcName[] = "WS-Huobi-MDC-Spt";
    constexpr static char OmcName[] = "H2WS-Huobi-OMC-Spt";
    constexpr static char MdcEndpoint[] = "/ws";
    constexpr static char OmcEndpoint[] = "/ws/v1";
  };

  template <>
  struct Traits<AssetClassT::Fut>
  {
    constexpr static char MdcName[] = "WS-Huobi-MDC-Fut";
    constexpr static char OmcName[] = "H2WS-Huobi-OMC-Fut";
    constexpr static char MdcEndpoint[] = "/ws";
    static constexpr char OmcEndpoint[] = "/notification";
  };

  template <>
  struct Traits<AssetClassT::CSwp>
  {
    constexpr static char MdcName[] = "WS-Huobi-MDC-CoinSwap";
    constexpr static char OmcName[] = "H2WS-Huobi-OMC-CoinSwap";
    constexpr static char MdcEndpoint[] = "/swap-ws";
    static constexpr char OmcEndpoint[] = "/swap-notification";
  };

  template <>
  struct Traits<AssetClassT::USwp>
  {
    constexpr static char MdcName[] = "WS-Huobi-MDC-USDTSwap";
    constexpr static char OmcName[] = "H2WS-Huobi-OMC-USDTSwap";
    constexpr static char MdcEndpoint[] = "/linear-swap-ws";
    static constexpr char OmcEndpoint[] = "/linear-swap-notification";
  };

} // namespace Huobi
} // namespace MAQUETTE
