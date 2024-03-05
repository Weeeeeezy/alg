// vim:ts=2:et
//===========================================================================//
//                              "FIX/Senders3.hpp":                          //
//                Sender of Application-Level FIX Msgs (MktData)             //
//===========================================================================//
#pragma once

#include "FixEngine/Basis/TimeValUtils.hpp"
#include "FixEngine/Acceptor/EAcceptor_FIX_MktData.h"
#include "FixEngine/Acceptor/UtilsMacros.hpp"
#include <cstring>

namespace AlfaRobot
{
  //=========================================================================//
  // "ProcessMarketDataIncrement":                                           //
  //=========================================================================//
  // Ie: process a "LiquidityMarketDataIncrement" msg coming from the ECN Core;
  //     send    a "MarketDataIncrementalRefresh" msg to the Client:
  //
  template<FIX::DialectT::type D, typename Conn>
  inline void EAcceptor_FIX_MktData<D, Conn>::ProcessMarketDataIncrement
    (Ecn::Basis::SPtr<Ecn::FixServer::LiquidityMarketDataIncrement> a_incr)
  const
  {
    //-----------------------------------------------------------------------//
    // Preamble:                                                             //
    //-----------------------------------------------------------------------//
    namespace EFS = Ecn::FixServer;
    namespace EM  = Ecn::Model;

    assert(a_incr.HasValue());
    Ecn::FixServer::LiquidityMarketDataIncrement const& incr = *a_incr;

    // Get the Session (must be available):
    FIXSession* sess = this->GetFIXSession(*incr.FixSession);
    assert(sess != nullptr);

    // Preamble:
    auto  preamble =
      this->template MkPreamble<FIX::MsgT::MarketDataIncrementalRefresh>(sess);
    char* curr     = std::get<0>(preamble);
    char* msgBody  = std::get<1>(preamble);

    //-----------------------------------------------------------------------//
    // Specific "MarketDataIncrementalRefresh" flds:                         //
    //-----------------------------------------------------------------------//
    // RequestID:
    STR_FLD("262=", incr.RequestId.c_str())

    // Now MDEntries:
    Ecn::Basis::Vector<EFS::LiquidityMarketDataIncrement::Entry> const& mdes =
      *incr.Entries;
    INT_FLD("268=", mdes.size());

    for (EFS::LiquidityMarketDataIncrement::Entry const& mde: mdes)
    {
      // "MDUpdateAction" (must be the 1st fld in the MDEntry):
      FIX::MDUpdateActionT action = FIX::MDUpdateActionT::UNDEFINED;

      switch (mde.UpdateAction)
      {
      case EFS::LiquidityMarketDataIncrement::MDUpdateAction::New:
        action = FIX::MDUpdateActionT::New;
        break;
      case EFS::LiquidityMarketDataIncrement::MDUpdateAction::Changed:
        action = FIX::MDUpdateActionT::Change;
        break;
      case EFS::LiquidityMarketDataIncrement::MDUpdateAction::Deleted:
        action = FIX::MDUpdateActionT::Delete;
        break;
      default:
        __builtin_unreachable();
      }
      assert(action != FIX::MDUpdateActionT::UNDEFINED);
      CHAR_FLD("279=", char(action))

      // "MDEntryType":
      // XXX: Currently, only "Bid" and "Offer" types are supported:
      assert
      (mde.EntryType == EFS::LiquidityMarketDataIncrement::MDEntryType::Bid ||
       mde.EntryType == EFS::LiquidityMarketDataIncrement::MDEntryType::Offer);

      FIX::MDEntryTypeT type =
        (mde.EntryType == EFS::LiquidityMarketDataIncrement::MDEntryType::Bid)
        ? FIX::MDEntryTypeT::Bid
        : FIX::MDEntryTypeT::Offer;

      CHAR_FLD("269=", char(type))

      // "MDEntryID":
      STR_FLD( "278=", mde.EntryId.c_str())

      // {Prev|Ref}EntryID": Needed because "Change" is implemented via a double
      // Delete / New action, each of them requiring its own EntryID:
      STR_FLD( "280=", mde.PrevEntryId.c_str())

      // Symbol:
      STR_FLD( "55=",  mde.Symbol.c_str())

      // MDEntryPx (quoted with up to 5 decimal points):
      DEC_FLD("270=",  mde.Price)

      // MDEntrySize (Qty):
      INT_FLD("271=",  mde.Amount)

      // XXX: No TimeStamps from the MatchingEngine are currently provided...
    }
    // End of "mdes" loop
    //-----------------------------------------------------------------------//
    // Go:                                                                   //
    //-----------------------------------------------------------------------//
    (void) this->template CompleteSendLog
      <FIX::MsgT::MarketDataIncrementalRefresh>(sess, msgBody, curr);
  }

  //=========================================================================//
  // "ProcessMarketDataSnapshot":                                            //
  //=========================================================================//
  // Ie: process a "LiquidityMarketDataSnapshot" msg coming from the ECN Core;
  //     send    a "MarketDataSnapShot"          msg to the Client:
  //
  template<FIX::DialectT::type D, typename Conn>
  inline void EAcceptor_FIX_MktData<D, Conn>::ProcessMarketDataSnapshot
       (Ecn::Basis::SPtr<Ecn::FixServer::LiquidityMarketDataSnapshot> a_snshot)
  const
  {
    //-----------------------------------------------------------------------//
    // Preamble:                                                             //
    //-----------------------------------------------------------------------//
    namespace EFS = Ecn::FixServer;
    namespace EM  = Ecn::Model;

    assert(a_snshot.HasValue());
    Ecn::FixServer::LiquidityMarketDataSnapshot const& snshot = *a_snshot;

    // Get the Session (must be available):
    FIXSession* sess = this->GetFIXSession(*snshot.FixSession);
    assert(sess != nullptr);

    // Preamble:
    auto  preamble =
      this->template MkPreamble<FIX::MsgT::MarketDataSnapShot>(sess);
    char* curr     = std::get<0>(preamble);
    char* msgBody  = std::get<1>(preamble);

    //-----------------------------------------------------------------------//
    // Specific "MarketDataSnapShot" flds:                                   //
    //-----------------------------------------------------------------------//
    // RequestID:
    STR_FLD("262=", snshot.RequestId.c_str())

    // Symbol:
    STR_FLD("55=",  snshot.Symbol.c_str())
    // XXX: Tenor (SettlDate) is NOT provided by "LiquidityMarketDataSnapshot"

    // Now MDEntries:
    Ecn::Basis::Vector<EFS::LiquidityMarketDataSnapshot::Entry> const& mdes =
      *snshot.Entries;
    INT_FLD("268=", mdes.size());

    for (EFS::LiquidityMarketDataSnapshot::Entry const& mde: mdes)
    {
      // MDEntryType (must be the 1st fld in the MDEntry):
      // XXX: Currently, only "Bid" and "Offer" types are supported:
      assert
      (mde.EntryType == EFS::LiquidityMarketDataSnapshot::MDEntryType::Bid ||
       mde.EntryType == EFS::LiquidityMarketDataSnapshot::MDEntryType::Offer);

      FIX::MDEntryTypeT type =
        (mde.EntryType == EFS::LiquidityMarketDataSnapshot::MDEntryType::Bid)
        ? FIX::MDEntryTypeT::Bid
        : FIX::MDEntryTypeT::Offer;

      CHAR_FLD("269=", char(type))

      // MDEntryPx (quoted with up to 5 decimail points):
      DEC_FLD( "270=", mde.Price)

      // MDEntrySize (Qty):
      INT_FLD( "271=", mde.Amount)

      // QuoteEntryID:
      STR_FLD( "299=", mde.EntryId.c_str())
    }
    // End of "mdes" loop
    //-----------------------------------------------------------------------//
    // Go:                                                                   //
    //-----------------------------------------------------------------------//
    (void) this->template CompleteSendLog<FIX::MsgT::MarketDataSnapShot>
      (sess, msgBody, curr);
  }

  //=========================================================================//
  // "ProcessMarketDataRequestReject":                                       //
  //=========================================================================//
  // Ie: process a "LiquidityMarketDataRequestReject" msg from the ECN Core;
  //     send    a "MarketDataRequestReject"          msg to the Client:
  //
  template<FIX::DialectT::type D, typename Conn>
  inline void EAcceptor_FIX_MktData<D, Conn>::ProcessMarketDataRequestReject
    (Ecn::Basis::SPtr<Ecn::FixServer::LiquidityMarketDataRequestReject>
     a_mkdrej)
  const
  {
    //-----------------------------------------------------------------------//
    // Preamble:                                                             //
    //-----------------------------------------------------------------------//
    namespace EFS = Ecn::FixServer;
    namespace EM  = Ecn::Model;

    assert(a_mkdrej.HasValue());
    Ecn::FixServer::LiquidityMarketDataRequestReject const& mkdrej = *a_mkdrej;

    // Get the original Request (must be available):
    EFS::LiquidityMarketDataRequest const& req = *mkdrej.Request;

    // Get the Session (must be available):
    FIXSession* sess = this->GetFIXSession(*req.FixSession);
    assert(sess != nullptr);

    // Preamble:
    auto  preamble =
      this->template MkPreamble<FIX::MsgT::MarketDataRequestReject>(sess);
    char* curr     = std::get<0>(preamble);
    char* msgBody  = std::get<1>(preamble);

    //-----------------------------------------------------------------------//
    // Specific "MarketDataRequestReject" flds:                              //
    //-----------------------------------------------------------------------//
    // ReqID:
    STR_FLD( "262=",  req.RequestId.c_str())

    // FIXME: FIX Protocol requires that we send a Reject Reason (Fld 281), but
    // it is not provided by "LiquidityMarketDataRequestReject" -- so will send
    // 'X' (other):
    // TODO YYY 281=X ---> 281=0
    EXPL_FLD("281=0")

    // The actual reason is sent as a Text:
    STR_FLD( "58=",   mkdrej.Reason.c_str())

    // NB: Other Request params, such as Symbol, Tenor, RequestType etc, are
    // NOT copied into "MarketDataRequestReject" msg...
    //-----------------------------------------------------------------------//
    // Go:                                                                   //
    //-----------------------------------------------------------------------//
    (void) this->template CompleteSendLog<FIX::MsgT::MarketDataRequestReject>
      (sess, msgBody, curr);
  }

  // TODO YYY
  template<FIX::DialectT::type D, typename Conn>
  inline void EAcceptor_FIX_MktData<D, Conn>::ProcessTradingSessionStatus
      (Ecn::Basis::SPtr<Ecn::FixServer::LiquidityFixSession> aSession)
  const
  {
    //-----------------------------------------------------------------------//
    // Preamble:                                                             //
    //-----------------------------------------------------------------------//
    namespace EFS = Ecn::FixServer;
    namespace EM  = Ecn::Model;

    // Get the Session (must be available):
    FIXSession* sess = this->GetFIXSession(*aSession);
    assert(sess != nullptr);

    // Preamble:
    auto  preamble =
        this->template MkPreamble<FIX::MsgT::TradingSessionStatus>(sess);
    char* curr     = std::get<0>(preamble);
    char* msgBody  = std::get<1>(preamble);

    EXPL_FLD("336=XXX");
    EXPL_FLD("340=2");
    EXPL_FLD("58=Opened");


    this->template CompleteSendLog<FIX::MsgT::TradingSessionStatus>
        (sess, msgBody, curr);
  }
} // End namespace AlfaRobot
