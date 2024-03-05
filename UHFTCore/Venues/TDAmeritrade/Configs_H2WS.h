// vim:ts=2:et
//===========================================================================//
//                   "Venues/TDAmeritrade/Configs_WS.h":                     //
//===========================================================================//
// NB: TDAmeritrade only supports the Prod Env. WebSockets are only for MDC and
// STP; API Keys are NOT required for TDAmeritrade WebSockets per se:
//
#pragma once
#include "Connectors/H2WS/Configs.hpp"
#include "Venues/TDAmeritrade/SecDefs.h"
#include <map>

namespace MAQUETTE {
namespace TDAmeritrade {
using AssetClassT = ::MAQUETTE::TDAmeritrade::AssetClassT;

//=========================================================================//
// MDC/OMC Configs:                                                        //
//=========================================================================//
extern H2WSConfig const Config_WS;
extern H2WSConfig const Config_HTTP;

H2WSAccount const Dummy_Account;

} // End namespace TDAmeritrade
} // End namespace MAQUETTE
