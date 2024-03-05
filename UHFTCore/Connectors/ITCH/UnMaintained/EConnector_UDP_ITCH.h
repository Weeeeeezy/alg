// vim:ts=2:et
//===========================================================================//
//                   "Connectors/ITCH/EConnector_UDP_ITCH.h":                //
//     MktData Connector Implementing a Dialect of ITCH Protocol over UDP    //
//===========================================================================//
#pragma once

#include "Protocols/ITCH/Features.h"
#include "Connectors/ITCH/Configs.h"
#include "Connectors/EConnector_MktData.h"

namespace MAQUETTE
{
  //=========================================================================//
  // "EConnector_UDP_ITCH" Class:                                            //
  //=========================================================================//
  template<ITCH::DialectT::type D>
  class EConnector_UDP_ITCH final: public EConnector_MktData
    {};
}
