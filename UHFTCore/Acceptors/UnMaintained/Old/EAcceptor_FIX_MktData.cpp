// vim:ts=2:et
//===========================================================================//
//                          "EAcceptor_FIX_MktData.cpp":                     //
//       Dummy CPP to enforce checking of "EAcceptor_FIX_MktData.hpp"        //
//===========================================================================//
#include "FixEngine/Acceptor/EAcceptor_FIX_MktData.hpp"

//===========================================================================//
// XXX: For testing only: Will be removed in Prod!                           //
//===========================================================================//
namespace AlfaRobot
{
  using namespace Ecn::Basis;
  using namespace Ecn::FixServer;

  struct DummyConn_MktData
  {
    // On FIX Session State change (eg LogOn / Termination):
    void SendFixSessionState  (SPtr<LiquidityFixSessionState>)   {}

    // On "MarketDataRequest" received from Client:
    void SendMarketDataRequest(SPtr<LiquidityMarketDataRequest>) {}
  };
/*
  template class EAcceptor_FIX_MktData
                 <FIX::DialectT::AlfaFIX2, DummyConn_MktData>;

  template class EAcceptor_FIX_MktData
                 <FIX::DialectT::FXBA,     DummyConn_MktData>;
*/
}
