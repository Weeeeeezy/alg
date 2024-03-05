// vim:ts=2:et
//===========================================================================//
//                "Connectors/FAST/EConnector_FAST_MICEX.cpp":               //
//    Embedded (Synchronous)  FAST Protocol Connector for MICEX FX and EQ    //
//===========================================================================//
#include "Connectors/FAST/EConnector_FAST_MICEX.hpp"
#include "Connectors/EConnector_MktData.hpp"

namespace MAQUETTE
{
  //=========================================================================//
  // Pre-Compiled Instances of "EConnector_FAST_MICEX":                      //
  //=========================================================================//
  template class EConnector_FAST_MICEX
    <FAST::MICEX::ProtoVerT::Curr, MICEX::AssetClassT::FX>;

  template class EConnector_FAST_MICEX
    <FAST::MICEX::ProtoVerT::Curr, MICEX::AssetClassT::EQ>;
}
// End namespace MAQUETTE

