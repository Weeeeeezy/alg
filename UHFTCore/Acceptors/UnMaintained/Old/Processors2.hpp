// vim:ts=2:et
//===========================================================================//
//                             "FIX/Processors2.hpp":                        //
//                Processors of Application-Level Msgs (OrdMgmt)             //
//===========================================================================//
#pragma once

#include "FixEngine/Acceptor/EAcceptor_FIX_OrdMgmt.h"
#include "FixEngine/Basis/Maths.hpp"
#include "utxx/error.hpp"

namespace
{
  using namespace AlfaRobot;

  //=========================================================================//
  // "MkDateTime":                                                           //
  //=========================================================================//
  inline Ecn::Basis::DateTime MkDateTime(utxx::time_val a_tv)
  {
    // XXX: The following conversion may not be the most efficient one, AND it
    // is accurate up to 1 sec only:
    return Ecn::Basis::DateTime
          (std::chrono::system_clock::from_time_t(a_tv.sec())

          // TODO YYY added milliseconds precision
          + std::chrono::milliseconds(a_tv.milliseconds() % 1000));
  }
}

namespace AlfaRobot
{
  //=========================================================================//
  // "ProcessNewOrderSingle":                                                //
  //=========================================================================//
  template<FIX::DialectT::type D, typename Conn>
  inline void EAcceptor_FIX_OrdMgmt<D, Conn>::ProcessNewOrderSingle
    (FIXSession* a_session)
  {
    assert(a_session != nullptr);
    FIX::NewOrderSingle const& tmsg = m_NewOrderSingle;

    // Verify the Side:
    Ecn::Model::Side side =
      (tmsg.m_Side == FIX::SideT::Buy)
      ? Ecn::Model::Side::Buy
      :
      (tmsg.m_Side == FIX::SideT::Sell)
      ? Ecn::Model::Side::Sell
      : throw utxx::runtime_error("NewOrderSingle: Invalid Side");

    // Verify TimeInForce:
    Ecn::Model::TimeInForce tif =
      (tmsg.m_TmInForce == FIX::TimeInForceT::GoodTillCancel)
      ? Ecn::Model::TimeInForce::GTC
      :
      (tmsg.m_TmInForce == FIX::TimeInForceT::ImmedOrCancel)
      ? Ecn::Model::TimeInForce::IOC
      : throw utxx::runtime_error("NewOrderSingle: Invalid TimeInForce");

    // Verify OrderType:
    Ecn::Model::OrderType ordType;  // XXX: Uninitialised!
    switch (tmsg.m_OrdType)
    {
    case FIX::OrderTypeT::Limit:
      ordType = Ecn::Model::OrderType::Limit;
      break;
    case FIX::OrderTypeT::Market:
      ordType = Ecn::Model::OrderType::Market;
      break;
    case FIX::OrderTypeT::MarketWithLeftOver:
      ordType = Ecn::Model::OrderType::MarketLimit;
      break;
    case FIX::OrderTypeT::Stop:
      ordType = Ecn::Model::OrderType::StopLoss;
      break;
    default:
      throw utxx::runtime_error("NewOrderSingle: Invalid OrderType");
    }

    // TODO YYY (decide how to check that all mandatory fields are present???)
    if (tmsg.m_ClOrdID[0] == 0)
    {
      throw utxx::runtime_error("NewOrderSingle: ClOrdID is missed.");
    }

    // Allocate a new "LiquidityOrderNewRequest" obj:
    // XXX: Currently, the data are just copied over from "tmsg":
    //
    Ecn::Basis::SPtr<Ecn::FixServer::LiquidityOrderNewRequest> newReq =
      Ecn::Basis::MakeSPtr<Ecn::FixServer::LiquidityOrderNewRequest>
      (
        a_session->m_ecnSession,
        Ecn::Model::OrderFixInfo::TClientOrderId(tmsg.m_ClOrdID),
        Ecn::Model::Instrument::TFixSymbol      (tmsg.m_Symbol.data()),
        side,
        MkDateTime(tmsg.m_TransactTime),
        tmsg.m_Qty,
        IsFinite(tmsg.m_Px) ? boost::optional<double>(tmsg.m_Px) : boost::none,
        tif,
        ordType
      );
    // Send it to the ECN Core:
    this->m_conn->SendOrderNewRequest(newReq);
  }

  //=========================================================================//
  // "ProcessOrderCancelRequest":                                            //
  //=========================================================================//
  template<FIX::DialectT::type D, typename Conn>
  inline void EAcceptor_FIX_OrdMgmt<D, Conn>::ProcessOrderCancelRequest
    (FIXSession* a_session)
  {
    assert(a_session != nullptr);
    FIX::OrderCancelRequest const& tmsg = m_OrderCancelRequest;

    // Allocate a new "LiquidityOrderCancelRequest":
    Ecn::Basis::SPtr<Ecn::FixServer::LiquidityOrderCancelRequest> cancelReq =
      Ecn::Basis::MakeSPtr<Ecn::FixServer::LiquidityOrderCancelRequest>
      (
        a_session->m_ecnSession,
        Ecn::Model::OrderFixInfo::TClientOrderId(tmsg.m_ClOrdID),
        Ecn::Model::OrderFixInfo::TClientOrderId(tmsg.m_OrigClOrdID),
        MkDateTime(tmsg.m_TransactTime)
      );
    // Send it to the ECN Core:
    this->m_conn->SendOrderCancelRequest(cancelReq);
  }

  //=========================================================================//
  // "ProcessOrderCancelReplaceRequest":                                     //
  //=========================================================================//
  template<FIX::DialectT::type D, typename Conn>
  inline void EAcceptor_FIX_OrdMgmt<D, Conn>::ProcessOrderCancelReplaceRequest
    (FIXSession* a_session)
  {
    assert(a_session != nullptr);
    FIX::OrderCancelReplaceRequest const& tmsg = m_OrderCancelReplaceRequest;

    // Allocate a new "LiquidityOrderReplaceRequest":
    Ecn::Basis::SPtr<Ecn::FixServer::LiquidityOrderReplaceRequest> modifyReq =
      Ecn::Basis::MakeSPtr<Ecn::FixServer::LiquidityOrderReplaceRequest>
      (
        a_session->m_ecnSession,
        Ecn::Model::OrderFixInfo::TClientOrderId(tmsg.m_ClOrdID),
        Ecn::Model::OrderFixInfo::TClientOrderId(tmsg.m_OrigClOrdID),
        MkDateTime(tmsg.m_TransactTime),
        tmsg.m_Qty,
        tmsg.m_Px
      );
    // Send it to the ECN Core:
    this->m_conn->SendOrderReplaceRequest(modifyReq);
  }
} // End namespace AlfaRobot
