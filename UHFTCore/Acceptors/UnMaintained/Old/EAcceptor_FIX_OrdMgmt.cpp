// vim:ts=2:et
//===========================================================================//
//                          "EAcceptor_FIX_OrdMgmt.cpp":                     //
//       Dummy CPP to enforce checking of "EAcceptor_FIX_OrdMgmt.hpp"        //
//===========================================================================//
#include "FixEngine/Acceptor/EAcceptor_FIX_OrdMgmt.hpp"

//===========================================================================//
// XXX: For testing only: Will be removed in Prod!                           //
//===========================================================================//
namespace AlfaRobot
{
  using namespace Ecn::Basis;
  using namespace Ecn::FixServer;

  struct DummyConn_OrdMgmt
  {
    // On FIX Session State change (eg LogOn / Termination):
    void SendFixSessionState    (SPtr<LiquidityFixSessionState>)     {}

    // On Connector State change      received from Client:
    void SendConnectorState     (SPtr<LiquidityConnectorState>)      {}

    // On "NewOrderSingle"            received from Client:
    void SendOrderNewRequest    (SPtr<LiquidityOrderNewRequest>)     {}

    // On "OrderCancelRequest"        received from Client:
    void SendOrderCancelRequest (SPtr<LiquidityOrderCancelRequest>)  {}

    // On "OrderCancelReplaceRequest" received from Client:
    void SendOrderReplaceRequest(SPtr<LiquidityOrderReplaceRequest>) {}
  };

  template class EAcceptor_FIX_OrdMgmt
                <FIX::DialectT::AlfaFIX2, DummyConn_OrdMgmt>;

  template class EAcceptor_FIX_OrdMgmt
                <FIX::DialectT::FXBA,     DummyConn_OrdMgmt>;

  template class EAcceptor_FIX_OrdMgmt
                <FIX::DialectT::MICEX,    DummyConn_OrdMgmt>;
}
