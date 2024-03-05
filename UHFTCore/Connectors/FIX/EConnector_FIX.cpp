// vim:ts=2:et
//===========================================================================//
//                     "Connectors/FIX/EConnector_FIX.cpp":                  //
//                       Instances of "EConnector_FIX"                       //
//===========================================================================//
#include "Connectors/EConnector_OrdMgmt.hpp"
#include "Connectors/EConnector_MktData.hpp"
#include "Protocols/FIX/ProtoEngine.hpp"
#include "Connectors/FIX/FIX_ConnectorSessMgr.hpp"
#include "Connectors/FIX/EConnector_FIX.hpp"

namespace MAQUETTE
{
# if (!CRYPTO_ONLY)
  template class EConnector_FIX<FIX::DialectT::AlfaECN>;
  template class EConnector_FIX<FIX::DialectT::AlfaFIX2>;
  template class EConnector_FIX<FIX::DialectT::EBS>;
  template class EConnector_FIX<FIX::DialectT::FORTS>;
  template class EConnector_FIX<FIX::DialectT::FXBA>;
  template class EConnector_FIX<FIX::DialectT::Currenex>;
  template class EConnector_FIX<FIX::DialectT::HotSpotFX_Gen>;
  template class EConnector_FIX<FIX::DialectT::HotSpotFX_Link>;
  template class EConnector_FIX<FIX::DialectT::MICEX>;
  template class EConnector_FIX<FIX::DialectT::NTProgress>;
# endif
  template class EConnector_FIX<FIX::DialectT::Cumberland>;
  template class EConnector_FIX<FIX::DialectT::LMAX>;
  template class EConnector_FIX<FIX::DialectT::TT>;

  // Currently streaming quotes are only used by Cumberland
  template void EConnector_FIX<FIX::DialectT::Cumberland>::
      SubscribeStreamingQuote(
        Strategy*, SecDefD const&, bool, Qty<QtyTypeT::QtyA, QR>);

  template void EConnector_FIX<FIX::DialectT::Cumberland>::
      SubscribeStreamingQuote(
        Strategy*, SecDefD const&,bool,Qty<QtyTypeT::QtyB, QR>);
}
// End namespace MAQUETTE
