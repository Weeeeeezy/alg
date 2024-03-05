// vim:ts=2:et
//===========================================================================//
//                             "FIX/Senders2.hpp":                           //
//              Sender of Application-Level FIX Msgs (OrdMgmt)               //
//===========================================================================//
#pragma once

#include "FixEngine/Basis/TimeValUtils.hpp"
#include "FixEngine/Acceptor/EAcceptor_FIX_OrdMgmt.h"
#include "FixEngine/Acceptor/UtilsMacros.hpp"
#include <cstring>

namespace AlfaRobot
{
  //=========================================================================//
  // "OutputDateFld"     (as "Tag=YYYYMMDD|"):                               //
  //=========================================================================//
  template<FIX::DialectT::type D, typename Conn, typename Derived>
  inline char* EAcceptor_FIX<D, Conn, Derived>::OutputDateFld
  (
    char const*                 a_tag,
    Ecn::Basis::Date const&     a_date,
    char*                       a_curr
  )
  {
    assert(a_tag != nullptr  && a_curr != nullptr);

    int year  = a_date.GetYear ();
    int month = a_date.GetMonth();
    int day   = a_date.GetDay  ();

    return
      (utxx::likely(year > 0 && month > 0 && day > 0))
      ? // XXX: No more validity checks:  This only guarantees that "a_date" is
        // non-empty:
        FIX::OutputDateFld(a_tag, year, month, day, a_curr)
      : a_curr;
  }

  //=========================================================================//
  // "OutputDateTimeFld" (as "Tag=YYYYMMDD-hh:mm:ss.sss|"):                  //
  //=========================================================================//
  template<FIX::DialectT::type D, typename Conn, typename Derived>
  inline char* EAcceptor_FIX<D, Conn, Derived>::OutputDateTimeFld
  (
    char const*                 a_tag,
    Ecn::Basis::DateTime const& a_dt,
    char*                       a_curr
  )
  {
    assert(a_tag != nullptr  && a_curr != nullptr);

    if (utxx::likely(a_dt.HasValue()))
    {
      // XXX: Unfortunately, we currently have to unpack "a_dt" into "DateTime-
      // Fields" -- this is somewhat inefficient:
      Ecn::Basis::DateTimeFields dtfs = a_dt.GetFields();

      // Tag:
      a_curr  = stpcpy(a_curr, a_tag);
      // Date:
      a_curr  = FIX::OutputDate
                (dtfs.GetYear(),  dtfs.GetMonth(),   dtfs.GetDay(), a_curr);
      // Separator:
      *a_curr = '-';
      // Time:
      a_curr  = FIX::OutputTime
                (dtfs.GetHours(), dtfs.GetMinutes(), dtfs.GetSeconds(),
                 dtfs.GetMilliseconds(),             a_curr + 1);
      // SOH:
      *a_curr = FIX::soh;
      ++a_curr;
    }
    return a_curr;
  }

  //=========================================================================//
  // "OutputOrdStatusFld":                                                   //
  //=========================================================================//
  template<FIX::DialectT::type D, typename Conn, typename Derived>
  inline char* EAcceptor_FIX<D, Conn, Derived>::OutputOrdStatusFld
  (
    Ecn::FixServer::LiquidityOrderStatus const& a_status,
    char*                                       a_curr
  )
  {
    assert(a_curr != nullptr);
    namespace EFS = Ecn::FixServer;

    FIX::OrdStatusT ordStatus = FIX::OrdStatusT::UNDEFINED;
    char* curr = a_curr;

    switch (a_status)
    {
    case EFS::LiquidityOrderStatus::New:
      ordStatus = FIX::OrdStatusT::New;
      break;
    case EFS::LiquidityOrderStatus::Canceled:
      ordStatus = FIX::OrdStatusT::Cancelled;
      break;
    case EFS::LiquidityOrderStatus::Rejected:
      ordStatus = FIX::OrdStatusT::Rejected;
      break;
    case EFS::LiquidityOrderStatus::PartialFill:
      ordStatus = FIX::OrdStatusT::PartiallyFilled;
      break;
    case EFS::LiquidityOrderStatus::Fill:
      ordStatus = FIX::OrdStatusT::Filled;
      break;
    default:
      __builtin_unreachable();  // This cannot happen
    }
    assert(ordStatus != FIX::OrdStatusT::UNDEFINED);

    // NB: The Tag is always 39:
    CHAR_FLD("39=", char(ordStatus))
    return curr;
  }

  //=========================================================================//
  // "ProcessOrderExecutionReport":                                          //
  //=========================================================================//
  template<FIX::DialectT::type D, typename Conn>
  inline void EAcceptor_FIX_OrdMgmt<D, Conn>::ProcessOrderExecutionReport
    (Ecn::Basis::SPtr<Ecn::FixServer::LiquidityOrderExecutionReport> a_report)
  const
  {
    //-----------------------------------------------------------------------//
    // Preamble:                                                             //
    //-----------------------------------------------------------------------//
    namespace EFS = Ecn::FixServer;
    namespace EM  = Ecn::Model;

    assert(a_report.HasValue());
    Ecn::FixServer::LiquidityOrderExecutionReport const& report = *a_report;

    // Get the Session (must be available):
    FIXSession* sess = this->GetFIXSession(*report.FixSession);
    assert(sess != nullptr);

    // Preamble:
    auto  preamble =
      this->template MkPreamble<FIX::MsgT::ExecutionReport>(sess);
    char* curr     = std::get<0>(preamble);
    char* msgBody  = std::get<1>(preamble);

    //-----------------------------------------------------------------------//
    // Specific "ExecutionReport" flds:                                      //
    //-----------------------------------------------------------------------//
    // FIXME: "OrderId" as assigned by the ECN (FIX Fld 37) is currently  NOT
    // sent to the Client, because it is a 16-byte UUID  which is problematic
    // to serialise (would be inefficient, and too long anyway!)

    // TODO YYY inefficient convert of uuid
    std::string orderId = report.OrderId.ToString();
    STR_FLD("37=", orderId.c_str());

    //
    // ClOrdID and OrigClOrdID: In general, they are Strings:
    STR_FLD("11=", report.ClientOrderId.c_str())

    // TODO YYY
    if (!report.OriginalClientOrderId.empty())
    {
      STR_FLD("41=", report.OriginalClientOrderId.c_str())
    }

    // ExecID: also a String:
    STR_FLD("17=", report.ExecId.c_str());

    // FIXME: "Account" (FIX Fld 1) is apparently not available in "Liquidity-
    // OrderExecutionReport"!

    // ExecType: XXX: Need to be careful here -- the exact values sent, depend
    // on the FIX protocol version:
    FIX::ExecTypeT execType = FIX::ExecTypeT::UNDEFINED;

    // XXX: Do not use table-driven conversions, they are error-prone if the
    // layout of EFS::LiquidityOrderExecType enum changes!
    switch (report.ExecType)
    {
    case EFS::LiquidityOrderExecType::New:
      // OrderStatus must be "New" as well:
      assert(report.OrderStatus == EFS::LiquidityOrderStatus::New);
      execType = FIX::ExecTypeT::New;
      break;
    case EFS::LiquidityOrderExecType::Canceled:
      assert(report.OrderStatus == EFS::LiquidityOrderStatus::Canceled);
      execType = FIX::ExecTypeT::Cancelled;
      break;
    case EFS::LiquidityOrderExecType::Replaced:
      // NB: This ExecType does NOT in general imply that  the corresp  Order-
      // Status is "Replaced" as well -- the status could (in some rare cases)
      // immediately become "Filled" or "PartFilled" as a result of a Replace-
      // ment event! (Plus, "Replaced" value is not provided by  EFS::Liquidi-
      // tyOrderStatus: FIXME: this might be wrong, actually):
      execType = FIX::ExecTypeT::Replaced;
      break;
    case EFS::LiquidityOrderExecType::Rejected:
      assert(report.OrderStatus == EFS::LiquidityOrderStatus::Rejected);
      execType = FIX::ExecTypeT::Rejected;
      break;
    case EFS::LiquidityOrderExecType::Trade:
      // Here the protocol version makes a difference:
      if (FIX::ProtocolFeatures<D>::s_fixVersionNum >= 44)
        // FIX.4.4 and above: use "Trade" as it is:
        execType = FIX::ExecTypeT::Trade;
      else
      { // Before FIX.4.4 (in particular, FIX.4.2): Use "Fill" or "PartialFill"
        // instead of "Trade" -- so need to examine "OrderStatus" as well:
        assert(report.OrderStatus == EFS::LiquidityOrderStatus::Fill       ||
               report.OrderStatus == EFS::LiquidityOrderStatus::PartialFill);
        execType =
              (report.OrderStatus == EFS::LiquidityOrderStatus::Fill)
              ? FIX::ExecTypeT::Fill
              : FIX::ExecTypeT::PartialFill;
      }
      break;
    default:
      __builtin_unreachable();  // This cannot happen
    }
    // Output the ExecType:
    assert  (execType != FIX::ExecTypeT::UNDEFINED);
    CHAR_FLD("150=", char(execType))

    // Now "OrderStatus" (XXX: as mentioned above, it is highly incomplete).
    // And while in most cases, "ExecType" implies a particulat "OrdStatus",
    // the converse may not be true: in general, we don't know what was the
    // last Exec event if we are now in a given Status (eg the Event could
    // be "Traded" and the status "Canceled", if it was an IoC order!):
    //
    curr = this->OutputOrdStatusFld(report.OrderStatus, curr);

    // Symbol:
    STR_FLD( "55=", report.Symbol.c_str())

    // Side:
    FIX::SideT side =
      (report.Side == EM::Side::Buy)
      ? FIX::SideT::Buy
      : FIX::SideT::Sell;
    CHAR_FLD("54=", char(side))

    // Initial order Qty (obviously, must be positive):
    assert (report.Amount > 0);
    INT_FLD( "38=",  report.Amount)

    // Initial order Px  (if available):
    if (utxx::likely(report.Price.is_initialized()))
      DEC_FLD("44=", report.Price.get())

    // LastQty (if available, ie if there was indeed a trade):
    assert(report.LastAmount >= 0);
    if (utxx::likely(report.LastAmount > 0))
      INT_FLD("32=", report.LastAmount)

    // LastPx: XXX: should be available or unavailable in sync with LastQty,
    // but we do not check that:
    if (utxx::likely(report.LastPrice.is_initialized()))
      DEC_FLD("31=", report.LastPrice.get())

    // LeavesQty: always available, reported even if it is 0:
    assert(report.LeavesAmount  >=  0);
    INT_FLD("151=",  report.LeavesAmount)

    // CumQty already filled: always available, reported even if 0:
    assert(report.CumulativeAmount >= 0);
    INT_FLD("14=",   report.CumulativeAmount)

    // AvgPx (if available); XXX: again, should be available in sync with the
    // LastQty, but we do not check that:
    if (utxx::likely(report.AveragePrice.is_initialized()))
    {
      DEC_FLD("6=", report.AveragePrice.get())
    }
    else
    {
      // TODO YYY AvgPx tag is mandatory
      EXPL_FLD("6=0")
    }

    // Order TimeInForce. XXX: Currently, only "GTC" and "IOC" are supported,
    // not even "Day" for some reason:
    FIX::TimeInForceT tif = FIX::TimeInForceT::UNDEFINED;
    switch (report.TimeInForce)
    {
    case EM::TimeInForce::GTC:
      tif = FIX::TimeInForceT::GoodTillCancel;
      break;
    case EM::TimeInForce::IOC:
      tif = FIX::TimeInForceT::ImmedOrCancel;
      break;
    default:
      __builtin_unreachable();
    }
    assert(tif != FIX::TimeInForceT::UNDEFINED);
    CHAR_FLD("59=", char(tif))

    // OrdType:
    FIX::OrderTypeT ordType = FIX::OrderTypeT::UNDEFINED;
    switch (report.OrderType)
    {
    case EM::OrderType::Limit:
      ordType = FIX::OrderTypeT::Limit;
      break;
    case EM::OrderType::Market:
      ordType = FIX::OrderTypeT::Market;
      break;
    case EM::OrderType::MarketLimit:
      ordType = FIX::OrderTypeT::MarketWithLeftOver;
      break;
    case EM::OrderType::Unconditional:
      // This is an internal OrderType, not supported by FIX:
      throw utxx::badarg_error
            ("EConnector_FIX::SendExecReport: UnSupported OrderType="
             "Unconditional");
    case EM::OrderType::StopLoss:
      ordType = FIX::OrderTypeT::Stop; // NB: NOT StopLimit!
      break;
    default:
      __builtin_unreachable();
    }
    assert(ordType != FIX::OrderTypeT::UNDEFINED);
    CHAR_FLD("40=", char(ordType))

    // SettlDate:    as YYYYMMDD:
    if (utxx::likely(bool(report.SettlementDate)))
      curr = this->OutputDateFld
             ("64=", report.SettlementDate.get(),  curr);

    // TransactTime: as YYYYMMDD-hh:mm:ss.sss:
    if (utxx::likely(bool(report.TransactionTime)))
      curr = this->OutputDateTimeFld
             ("60=", report.TransactionTime.get(), curr);

    // Finally: A Text Msg:
    char const* text = report.Text.c_str();
    if (*text != '\0')
      STR_FLD("58=", text);

    //-----------------------------------------------------------------------//
    // Go:                                                                   //
    //-----------------------------------------------------------------------//
    (void) this->template CompleteSendLog<FIX::MsgT::ExecutionReport>
      (sess, msgBody, curr);
  }

  //=========================================================================//
  // "ProcessOrderCancelReject":                                             //
  //=========================================================================//
  template<FIX::DialectT::type D, typename Conn>
  inline void EAcceptor_FIX_OrdMgmt<D, Conn>::ProcessOrderCancelReject
       (Ecn::Basis::SPtr<Ecn::FixServer::LiquidityOrderCancelReject> a_crej)
  const
  {
    //-----------------------------------------------------------------------//
    // Preamble:                                                             //
    //-----------------------------------------------------------------------//
    namespace EFS = Ecn::FixServer;
    namespace EM  = Ecn::Model;

    assert(a_crej.HasValue());
    Ecn::FixServer::LiquidityOrderCancelReject const& crej = *a_crej;

    // Get the Session (must be available):
    FIXSession* sess = this->GetFIXSession(*crej.FixSession);
    assert(sess != nullptr);

    // Preamble:
    auto  preamble =
      this->template MkPreamble<FIX::MsgT::OrderCancelReject>(sess);
    char* curr     = std::get<0>(preamble);
    char* msgBody  = std::get<1>(preamble);

    //-----------------------------------------------------------------------//
    // Specific "OrderCancelReject" flds:                                    //
    //-----------------------------------------------------------------------//
    // "ClOrdID" (ie the CancelReqID which has failed):
    STR_FLD("11=", crej.ClientOrderId.c_str())

    // "OrigClOrdID" (ID of the target Order which was supposed to be cancelled,
    // but was not):
    STR_FLD("41=", crej.OriginalClientOrderId.c_str())

    // TODO YYY inefficient convert of uuid
    std::string orderId = crej.OrderId.ToString();
    STR_FLD("37=", orderId.c_str());

    // OrderStatus --- XXX: of which order? Of the original one, or of the Can-
    // cellation Request? (Probably the FORMER): Same as for "ExecutionReport":
    curr = this->OutputOrdStatusFld(crej.OrderStatus, curr);

    // "RequestType" which has failed: Translated into "CxlRejRespT". Obviously,
    // a "New" request cannot result in this rejection -- only "Cancel" or "Re-
    // place" can:
    assert(crej.RequestType == EM::OrderRequestType::Cancel ||
           crej.RequestType == EM::OrderRequestType::Replace);

    FIX::CxlRejRespT reqType =
      (crej.RequestType == EM::OrderRequestType::Cancel)
      ? FIX::CxlRejRespT::Cancel
      : FIX::CxlRejRespT::CancelReplace;

    CHAR_FLD("434=", char(reqType));

    // XXX: "CxlRejReason" code is NOT available from "LiquidityOrderCancel-
    // Reject"; only a textual err msg is provided:
    STR_FLD("58=", crej.Text.c_str())

    //-----------------------------------------------------------------------//
    // Go:                                                                   //
    //-----------------------------------------------------------------------//
    (void) this->template CompleteSendLog<FIX::MsgT::OrderCancelReject>
      (sess, msgBody, curr);
  }

  //=========================================================================//
  // "ProcessTradingSessionStatus":                                          //
  //=========================================================================//
  template<FIX::DialectT::type D, typename Conn>
  inline void EAcceptor_FIX_OrdMgmt<D, Conn>::ProcessTradingSessionStatus
       (Ecn::Basis::SPtr<Ecn::FixServer::LiquidityFixSession> a_status)
  const
  {
    //-----------------------------------------------------------------------//
    // Preamble:                                                             //
    //-----------------------------------------------------------------------//
    namespace EFS = Ecn::FixServer;
    namespace EM  = Ecn::Model;

    // Get the Session (must be available):
    assert(a_status.HasValue());
    Ecn::FixServer::LiquidityFixSession const& status = *a_status;

    FIXSession* sess = this->GetFIXSession(status);
    assert(sess != nullptr);

    // FIXME: "LiquidityFixSession" does not provide the SessionStatus, actual-
    // ly!  We assume that this msg is sent when the Session is ACTIVATED  (be-
    // cause if it is terminated, the TCP connection is dropped, and the Status
    // cannot be sent anyway) -- so:
    // Re-use the "SendSessionStatus" method:
    //
    (void) this->SendSessionStatus(sess, FIX::TradSesStatusT::Open);
  }
} // End namespace AlfaRobot
