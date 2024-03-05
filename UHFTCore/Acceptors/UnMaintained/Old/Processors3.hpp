// vim:ts=2:et
//===========================================================================//
//                             "FIX/Processors3.hpp":                        //
//                Processors of Application-Level Msgs (MktData)             //
//===========================================================================//
#pragma once

#include "FixEngine/Acceptor/EAcceptor_FIX_MktData.h"
#include "utxx/error.hpp"

namespace AlfaRobot
{
  //=========================================================================//
  // "ProcessMarketDataRequest":                                             //
  //=========================================================================//
  template<FIX::DialectT::type D, typename Conn>
  inline void EAcceptor_FIX_MktData<D, Conn>::ProcessMarketDataRequest
    (FIXSession* a_session)
  {
    assert(a_session != nullptr);
    FIX::MarketDataRequest const& tmsg = m_MarketDataRequest;

    // Request Type:
    Ecn::FixServer::LiquidityMarketDataRequestType rType =
      (tmsg.m_ReqType == FIX::SubscrReqTypeT::Subscribe)
      ? Ecn::FixServer::LiquidityMarketDataRequestType::Subscribe
      :
      (tmsg.m_ReqType == FIX::SubscrReqTypeT::UnSubscribe)
      ? Ecn::FixServer::LiquidityMarketDataRequestType::Unsubscribe
      : // Other ReqTypes (incl a one-off SnapShot) are currently unsupported:
        throw utxx::badarg_error("Invalid MarketDataRequestType");

    // Check the RequestID size:
    if (utxx::unlikely
       (strlen(tmsg.m_ReqID) >
        sizeof(Ecn::FixServer::LiquidityMarketDataRequest::TRequestId) - 1))
      throw utxx::badarg_error("MarketDataRequestID Too Long");

    // SettlDate (Tenor):
    // XXX: "LiquidityMarketDataRequest" currently only supports TOD/TOM/etc,
    // not dates (as YYYYMMDD):
    Ecn::Model::Tenor tenor =
      (strcasecmp(tmsg.m_SettlDate, "TOD")  == 0)
      ? Ecn::Model::Tenor::Tod
      :
      (strcasecmp(tmsg.m_SettlDate, "TOM")  == 0)
      ? Ecn::Model::Tenor::Tom
      :
      (strcasecmp(tmsg.m_SettlDate, "SPOT") == 0)
      ? Ecn::Model::Tenor::Spot
      :
      (tmsg.m_SettlDate[0] == '\0')
      ? Ecn::Model::Tenor::None
      : throw utxx::badarg_error("Invalid MarketDataRequestTenor");

    // Allocate a new "LiquidityMarketDataRequest" obj:
    // XXX: This involved copying of data from "tmsg", which is sub-optimal but
    // OK for now:
    Ecn::Basis::SPtr<Ecn::FixServer::LiquidityMarketDataRequest> mdReq =
      Ecn::Basis::MakeSPtr<Ecn::FixServer::LiquidityMarketDataRequest>
      (
        rType,
        Ecn::FixServer::LiquidityMarketDataRequest::TRequestId(tmsg.m_ReqID),
        a_session->m_ecnSession,
        Ecn::Model::Instrument::TFixSymbol(tmsg.m_Symbol.data()),
        tenor
      );

    // Send this obj to the ECN Core:
    this->m_conn->SendMarketDataRequest(mdReq);
  }

} // End namespace AlfaRobot
