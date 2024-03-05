// vim:ts=2:et
//===========================================================================//
//               "Connectors/ITCH/EConnector_ITCH_HotSpotFX.cpp":            //
//    Instantiations of "EConnector_ITCH_HotSpotFX" for "Gen" and "Link"     //
//===========================================================================//
#include "Connectors/ITCH/EConnector_ITCH_HotSpotFX.hpp"

namespace MAQUETTE
{
  //=========================================================================//
  // Actial Instances of "EConnector_ITCH_HotSpotFX":                        //
  //=========================================================================//
  template class EConnector_ITCH_HotSpotFX<ITCH::DialectT::HotSpotFX_Gen>;
  template class EConnector_ITCH_HotSpotFX<ITCH::DialectT::HotSpotFX_Link>;
}
// End namespace MAQUETTE
