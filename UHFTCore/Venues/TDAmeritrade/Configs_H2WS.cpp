// vim:ts=2:et
//===========================================================================//
//                   "Venues/TDAmeritrade/Configs_WS.cpp":                   //
//===========================================================================//
#include "Venues/TDAmeritrade/Configs_H2WS.h"
#include "Venues/TDAmeritrade/SecDefs.h"

using namespace std;

namespace MAQUETTE {
namespace TDAmeritrade {
using AssetClassT = ::MAQUETTE::TDAmeritrade::AssetClassT;

//=========================================================================//
// WS End-Points (both MDC and OMC):                                       //
//=========================================================================//
// XXX: Located in AWS NorthEast-1? Here explicit IP addrs could be appropri-
// ate, but still, the list appears to be dependent on the Client IP addr, so
// we better use purely-dynamic resolution:
//-------------------------------------------------------------------------//
// MDC/OMC Configs                                                         //
//-------------------------------------------------------------------------//
H2WSConfig const Config_WS{"streamer-ws.tdameritrade.com", 443, {}};
H2WSConfig const Config_HTTP{"api.tdameritrade.com", 443, {}};

} // End namespace TDAmeritrade
} // End namespace MAQUETTE
