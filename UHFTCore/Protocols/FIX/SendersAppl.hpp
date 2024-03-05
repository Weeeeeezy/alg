// vim:ts=2:et
//===========================================================================//
//                       "Protocols/FIX/SendersAppl.hpp":                    //
//           FIX Protocol Engine Sender of Application-Level Msgs            //
//===========================================================================//
#pragma once

#include "Basis/Maths.hpp"
#include "Basis/SecDefs.h"
#include "Basis/OrdMgmtTypes.hpp"
#include "Connectors/FIX/EConnector_FIX.h"
#include "InfraStruct/Strategy.hpp"
#include "Protocols/FIX/Features.h"
#include "Protocols/FIX/SessData.hpp"
#include "Protocols/FIX/ProtoEngine.h"
#include "Protocols/FIX/UtilsMacros.hpp"

namespace MAQUETTE
{
namespace FIX
{
  //=========================================================================//
  // Currenex-Specific Order Type Encoding:                                  //
  //=========================================================================//
  // XXX: Currenex uses a highly-non-standard encoding for Order Types:
  //
  inline char MkCurrenexOrdType(OrderTypeT a_ord_type, bool a_is_iceberg)
  {
    // XXX: Currenex uses highly non-standard OrdTypes. In particular, an
    // Iceberg order requires its own type:
    return
      (a_ord_type == OrderTypeT::Limit)
      ? 'F' :
      (a_ord_type == OrderTypeT::Market)
      ? 'C' :
      a_is_iceberg
      ? 'Z' :             // Special type for Icebergs
      char(a_ord_type);   // In all other cases, return the orig rep
  }

  //=========================================================================//
  // "NewOrderImpl":                                                         //
  //=========================================================================//
  // Generates and sends a "NewOrderSingle"; returns MsgSendTS:
  template
  <
    DialectT::type    D,
    bool              IsServer,
    typename          SessMgr,
    typename          Processor
  >
  void ProtoEngine<D, IsServer, SessMgr, Processor>::NewOrderImpl
  (
    SessData*         a_sess,
    Req12*            a_new_req,
    bool              a_batch_send   // To send several msgs at once
  )
  const
  {
    //-----------------------------------------------------------------------//
    // Checks:                                                               //
    //-----------------------------------------------------------------------//
    // The following invars mus be checked by the Caller (also, Qty invars are
    // asserted below):
    assert(a_sess != nullptr   && a_new_req != nullptr     &&
           a_new_req->m_kind   == Req12::KindT::New        &&
           a_new_req->m_status == Req12::StatusT::Indicated);

    // At the moment, "NewOrderImpl" can only be issued by a Client with OrdMgmt
    // capabilities:
    CHECK_ONLY
    (
      if (utxx::unlikely(IsServer || !(m_sessMgr->IsOrdMgmt())))
        throw utxx::runtime_error
              ("FIX::ProtoEngine::NewOrderImpl: Not Supported");
    )
    //-----------------------------------------------------------------------//
    // Order Params:                                                         //
    //-----------------------------------------------------------------------//
    AOS const*      aos         = a_new_req->m_aos;
    assert(aos != nullptr);

    OrderID newReqID            = a_new_req->m_id;
    assert( newReqID != 0);

    utxx::time_val  createTS    = a_new_req->m_ts_created;
    assert(!createTS.empty());

    Strategy*       strategy    = aos->m_strategy;
    assert(strategy != nullptr);

    SecDefD const*  instr       = aos->m_instr;
    assert(instr    != nullptr);

    bool            isBuy       = aos->m_isBuy;
    OrderTypeT      ordType     = aos->m_orderType;
    TimeInForceT    timeInForce = aos->m_timeInForce;
    int             expireDate  = aos->m_expireDate;
    PriceT          px          = a_new_req->m_px;
    bool            pegSide     = a_new_req->m_pegSide;
    double          pegOffset   = a_new_req->m_pegOffset;

    QtyN            qty         = a_new_req->GetQty    <QT,QR>();
    QtyN            qtyShow     = a_new_req->GetQtyShow<QT,QR>();
    QtyN            qtyMin      = a_new_req->GetQtyMin <QT,QR>();

    assert(IsPos(qty) && !IsNeg(qtyShow) && (IsPosInf(qty) || qtyShow <= qty)
                      && !IsNeg(qtyMin)  && qtyMin <= qty);

    // XXX: Currenex and Cumberland break the FIX standard substantially, so we
    // have to check for those Dialects explicitly:
    //
    constexpr bool IsCurrenex   = (D == DialectT::Currenex);
    constexpr bool IsCumberland = (D == DialectT::Cumberland);

    //-----------------------------------------------------------------------//
    // Preamble and Account:                                                 //
    //-----------------------------------------------------------------------//
    auto   preamble = MkPreamble<MsgT::NewOrderSingle>(a_sess, createTS);

    char*  curr     = std::get<0>(preamble);
    char*  msgBody  = std::get<1>(preamble);
    assert(std::get<2>(preamble) == createTS);
    SeqNum txSN     = std::get<3>(preamble);

    // Memoise the TimeStamp string ptr (len=22 incl SOH):
    [[maybe_unused]] char const* ts = curr - 22;
    curr = MkReqID(a_sess, curr, "11=", newReqID);            // ClOrdID

    // Account and SecOrdID:
    if constexpr (ProtocolFeatures<D>::s_useAccountInNew)
    {
      if constexpr (ProtocolFeatures<D>::s_useAccountAsSecOrdID)
        // This  is rare: Under Account tag, install the SecondaryOrderID value,
        // which is the 48-bit StratHashCode:
        HEX12_FLD("1=", strategy->GetHash48())               // Account
      else
      {
        // Generic case: User real Account for tag 1, and StratHashCode for tag
        // 526 (SecondaryOrderID):
        char const* acctStr = a_sess->m_account.data();
        CHECK_ONLY
        (
          if (utxx::unlikely(*acctStr == '\0'))
            throw utxx::runtime_error
                  ("FIX::ProtoEngine::NewOrderImpl: Empty Account");
        )
        STR_FLD("1=", acctStr)                                // Account
      }
    }
    // If SecOrdID is enabled, install StratHash here (even if it was already
    // installed in the Account fld):
    if constexpr (ProtocolFeatures<D>::s_hasSecondOrdID)
      HEX12_FLD("526=", strategy->GetHash48())                // SecClOrdID

    // For backwards compatibility, only add this if
    // s_useOnBehalfOfCompIDInsteadOfClientID is false, otherwise we're adding
    // the OnBehalfOfCompID in the message prefix
    if constexpr (!ProtocolFeatures<D>::s_useOnBehalfOfCompIDInsteadOfClientID)
    {
      // Use ClientID as well if it is non-empty (normally for FIX.4.2):
      if (!IsEmpty(a_sess->m_clientID))
        STR_FLD("109=", a_sess->m_clientID.data())            // ClientID
    }

    // Parties:
    if constexpr (ProtocolFeatures<D>::s_usePartiesInNew)
      MK_PARTIES

    // Handling Instructions: XXX: If enabled, use 1 for Fully-Automated
    // execution:
    if constexpr (ProtocolFeatures<D>::s_hasHandlInstr)
      EXPL_FLD("21=1")

    //-----------------------------------------------------------------------//
    // Instrument Spec:                                                      //
    //-----------------------------------------------------------------------//
    if constexpr (!IsCumberland)
      MK_SYMBOL(instr)                                        // Symbol

    // XXX: For Currenex, we also need the CcyA:
    if constexpr (IsCurrenex)
    {
      STR_FLD("15=", instr->m_AssetA.data())                  // CcyA
      EXPL_FLD("460=4")                                       // Product=Ccy
    }
    // Install the CFI Code:
    if constexpr (ProtocolFeatures<D>::s_useCFICode)          // CFICode
      STR_FLD("461=", instr->m_CFICode)

    // Install Market Segment or similar fld, if required:
    if constexpr (ProtocolFeatures<D>::s_useSegmEtcInNew)
      curr = MkSegmSessDest(curr, *instr);

    //-----------------------------------------------------------------------//
    // OrderType and Px:                                                     //
    //-----------------------------------------------------------------------//
    if constexpr (ProtocolFeatures<D>::s_useOrderType)
    {
      // OrderType (throw exception of the requested type is not supported):
      // Only in very degenerate cases (eg AlfaFIX2), OrderType is omitted.
      // For FullAmt Orders, OrderType needs to be modified. NB: But do NOT
      // alter the "ordType" param -- it is used below for Px output:
      //
      if constexpr (ProtocolFeatures<D>::s_hasFullAmount)
        EXPL_FLD("40=Q")                                      // CounterOrdSel
      else
      if constexpr (IsCurrenex)
        CHAR_FLD("40=", MkCurrenexOrdType(ordType, aos->m_isIceberg))
      else
        // Generic Case:
        CHAR_FLD("40=", ordType)                              // OrderType
    }

    switch (ordType)
    {
    //----------------------------//
    case OrderTypeT::Stop:
    //----------------------------//
      if constexpr (!ProtocolFeatures<D>::s_hasStopOrders)
        throw utxx::badarg_error
              ("FIX::ProtoEngine::NewOrderImpl: StopOrders Not Supported");
      // Then price must be Finite:
      assert(IsFinite(px));

      // XXX: Curreex has a special fld for Stop pxs:
      if constexpr (IsCurrenex)
        DEC_FLD("99=", double(px))                            // LimitPrice
      else
        DEC_FLD("44=", double(px))                            // Price
      break;

    //----------------------------//
    case OrderTypeT::Limit:
    //----------------------------//
      // Again, the price must be Finite:
      assert(IsFinite(px));
      DEC_FLD( "44=", double(px))                             // Price
      break;

    //----------------------------//
    case OrderTypeT::Market:
    //----------------------------//
      if constexpr (!ProtocolFeatures<D>::s_hasMktOrders)
        throw utxx::badarg_error
          ("FIX::ProtoEngine::NewOrderImpl: MktOrders Not Supported");
      // Otherwise: Mkt Order price must NOT be finite:
      assert(!IsFinite(px));

      // Nevertheless, a nominal 0 px may simetimes need to be installed:
      if constexpr (ProtocolFeatures<D>::s_useNominalPxInMktOrders)
        EXPL_FLD("44=0.0")                                    // Nominal Price
      break;

    //----------------------------//
    case OrderTypeT::Pegged:
    //----------------------------//
      if constexpr (!ProtocolFeatures<D>::s_hasPeggedOrders)
        throw utxx::badarg_error
          ("FIX::ProtoEngine::NewOrderImpl: PeggedOrders Not Supported");

      CHECK_ONLY
      (
        if (utxx::unlikely(!IsFinite(pegOffset)))
          throw utxx::badarg_error
                ("FIX::ProtoEngine::NewOrderImpl: Invalid PegOffset");
      )
      // XXX: Pegged Order currently implies that the TimeInForce must be Day:
      timeInForce = TimeInForceT::Day;

      if constexpr (!IsCurrenex)
      {
        // Generic Pegged Order Support:
        // The price may or may not be available; if provided, it limits the
        // Pegged px (XXX: hight limit for Bid, low limit for Ask?). If not,
        // we STILL need a 0 nominal px here (currently, HotSPotFX only):
        //
        if (utxx::unlikely(IsFinite(px)))
          DEC_FLD( "44=", double(px))                           // Price
        else
          EXPL_FLD("44=0.0")

        // Now the Peg Params themselves:
        CHAR_FLD("18=", (pegSide ? 'R' : 'P'))
        DEC_FLD("211=",  pegOffset)
      }
      else
      {
        // Currenex has its own support for Pegged Orders:
        if (qtyShow < qty)
          EXPL_FLD("7578=Z")                                    // Pegged Ice'g
        else
          EXPL_FLD("7578=F")                                    // Pegged Limit

        // Side to which the Pegged Order is attached. XXX: Pegging to MidPoint
        // is not supported yet:
        bool pegBid = (pegSide == isBuy);
        if  (pegBid)
          EXPL_FLD("7579=1")
        else
          EXPL_FLD("7579=2")

        // FIXME: Pegging offset semantics is currently unclear. We assume that
        // 1 point (or pip) is 10 * PxStep, and the offset is specified in such
        // points:
        double offSteps = Round(pegOffset / instr->m_PxStep);
        double offPts   = offSteps / 10.0;
        DEC_FLD("7580=",  offPts)
      }
      break;

    //----------------------------//
    default:
    //----------------------------//
      throw utxx::badarg_error
            ("FIX::ProtoEngine::NewOrderImpl: UnSupported OrderType=",
             char(ordType));
    }
    //-----------------------------------------------------------------------//
    // Side and Qty:                                                         //
    //-----------------------------------------------------------------------//
    SideT sd = isBuy ? SideT::Buy : SideT::Sell;
    CHAR_FLD("54=", sd)                                      // Side

    if constexpr (IsCumberland)
    {
      if (!IsPosInf(qty))
      {
        QTY_FLD("38=", qty)                                  // OrderQty
        STR_FLD("15=", instr->m_AssetA.data())               // CcyA
      }
    }
    else
      QTY_FLD ("38=", qty)                                   // OrderQty

    // Iceberg (incl a completely invisible one, with qtyShow==0)?
    if (utxx::unlikely(qtyShow != qty))
    {
      assert(!IsNeg(qtyShow) && qtyShow < qty);
      if constexpr (!ProtocolFeatures<D>::s_hasQtyShow)
        throw utxx::badarg_error
              ("FIX::ProtoEngine::NewOrderImpl: Icebergs Not Supported: "
               "Qty=", QR(qty), ", QtyShow=", QR(qtyShow));
      // If supported:
      QTY_FLD("210=", qtyShow)                               // QtyShow
    }

    if (utxx::unlikely(!IsZero(qtyMin)))                     // QtyMin
    {
      assert(IsPos(qtyMin) && qtyMin <= qty);
      if constexpr (!ProtocolFeatures<D>::s_hasQtyMin)
        throw utxx::badarg_error
              ("FIX::ProtoEngine::NewOrderImpl: QtyMin Not Supported");
      QTY_FLD("110=", qtyMin)
    }

    //-----------------------------------------------------------------------//
    // Other Flds:                                                           //
    //-----------------------------------------------------------------------//
    // XXX: Some Dialects may ONLY have "IoC"s, so in that case "TimeInForce"
    // is NOT provided explicitly:
    if constexpr (ProtocolFeatures<D>::s_useTimeInForce)
    {
      if (ordType == OrderTypeT::Market)
      {
        // For the Market orders, "TimeInForce" must be "IoC", but if UseIoCIn-
        // MktOrders flag is NOT set, we reset it to the default:
        assert(timeInForce ==  TimeInForceT::ImmedOrCancel);
        if constexpr  (!ProtocolFeatures<D>::s_useIOCInMktOrders)
          timeInForce = ProtocolFeatures<D>::s_defaultTimeInForce;
      }
      else
      if (utxx::unlikely(ProtocolFeatures<D>::s_hasFullAmount))
        // Also, over-write "TimeInForce" FullAmt Orders -- must be "IoC":
        timeInForce = TimeInForceT::ImmedOrCancel;
      else
        // In all other cases, since TimeInForce is required, set it to the
        // default:
        timeInForce = ProtocolFeatures<D>::s_defaultTimeInForce;

      CHAR_FLD("59=", char(timeInForce))                     // TimeInForce

      // "ExpireDate" is conditional on "TimeInForce". In any case, there is no
      // point in sumitting orders with past ExpireDates:
      if (utxx::unlikely(timeInForce == TimeInForceT::GoodTillDate))
      {
        CHECK_ONLY
        (
          if (utxx::unlikely(expireDate < GetCurrDateInt()))
            throw utxx::badarg_error
                  ("FIX::ProtoEngine::NewOrderImpl: Invalid ExpireDate=",
                   expireDate);
        )
        INT_FLD("432=", expireDate)                          // ExpireDate
      }
    }

    if constexpr (!IsCumberland)
    {
      // The SendingTime is already in the Preamble, get it from there:
      curr = stpcpy (curr, "60=" );                          // TransactTime
      curr = stpncpy(curr, ts, 22); // Incl SOH
    }
    else
    {
      // For Cumberland we need to include the quote ID and quote version:
      // We need access to the EConnector_FIX, this is ugly :(
      EConnector_FIX<D>* conn =
         const_cast <EConnector_FIX<D>*>
        (static_cast<EConnector_FIX<D> const*>(this));

      // First find the order book
      auto   ob =  conn->FindOrderBook(instr->m_SecID);
      assert(ob != nullptr);

      int ob_id = int(ob - conn->m_orderBooks->data());
      assert(ob_id >= 0 && ob_id < int(conn->m_orderBooks->size()));

      // Cumberland-specific flds:
      STR_FLD("117=",   conn->m_quoteIDs->data()[ob_id].data())
      INT_FLD("20000=", ob->GetLastUpdateSeqNum())
    }

    //-----------------------------------------------------------------------//
    // Go!                                                                   //
    //-----------------------------------------------------------------------//
    utxx::time_val sendTime =
      CompleteSendLog<MsgT::NewOrderSingle>
        (a_sess, msgBody, curr, a_batch_send);

    //-----------------------------------------------------------------------//
    // Update the "NewReq":                                                  //
    //-----------------------------------------------------------------------//
    a_new_req->m_status  = Req12::StatusT::New;
    a_new_req->m_ts_sent = sendTime;
    a_new_req->m_seqNum  = txSN;
  }

  //=========================================================================//
  // "CancelOrderImpl":                                                      //
  //=========================================================================//
  // Generates and sends an "OrderCancelRequest". Returns MsgSendTS:
  template
  <
    DialectT::type  D,
    bool            IsServer,
    typename        SessMgr,
    typename        Processor
  >
  void ProtoEngine<D, IsServer, SessMgr, Processor>::CancelOrderImpl
  (
    SessData*       a_sess,
    Req12*          a_clx_req,
    Req12 const*    a_orig_req,
    bool            a_batch_send   // To send multiple msgs at once
  )
  const
  {
    //-----------------------------------------------------------------------//
    // Checks:                                                               //
    //-----------------------------------------------------------------------//
    // It is assumed that "a_orig_req" has already been verified by the Caller:
    assert(a_sess     != nullptr && a_clx_req != nullptr      &&
           a_orig_req != nullptr &&
           a_clx_req ->m_kind    == Req12::KindT::Cancel      &&
           a_clx_req ->m_status  == Req12::StatusT::Indicated &&
           a_orig_req->m_status  >= Req12::StatusT::New);

    // At the moment, "CancelOrderImpl" can only be issues by a Client with Ord-
    // Mgmt and Cancel capabilities:
    CHECK_ONLY
    (
      if (utxx::unlikely (IsServer || !(m_sessMgr->IsOrdMgmt()) ||
                          !ProtocolFeatures<D>::s_hasCancel))
      throw utxx::runtime_error
            ("FIX::ProtoEngine::CancelOrderImpl: Not Supported");
    )
    // NB: No need to notify the Risk Manager now -- "Cancel" is always allowed;
    // it will be notified  when the order is actually cancelled!

    //-----------------------------------------------------------------------//
    // Cxl Params:                                                           //
    //-----------------------------------------------------------------------//
    AOS const*     aos     = a_clx_req->m_aos;
    assert(aos != nullptr && aos == a_orig_req->m_aos);

    OrderID        clReqID = a_clx_req->m_id;
    assert(clReqID != 0   && a_orig_req->m_id != 0 &&
           clReqID >         a_orig_req->m_id);

    utxx::time_val createTS = a_clx_req->m_ts_created;
    assert(!createTS.empty());

    //-----------------------------------------------------------------------//
    // Preamble, Account and Parties:                                        //
    //-----------------------------------------------------------------------//
    auto   preamble = MkPreamble<MsgT::OrderCancelRequest>(a_sess, createTS);

    char*  curr     = std::get<0>(preamble);
    char*  msgBody  = std::get<1>(preamble);
    assert(std::get<2>(preamble) == createTS);
    SeqNum txSN     = std::get<3>(preamble);

    // Memoise the TimeStamp string ptr (len=22 incl SOH):
    char const* ts = curr - 22;

    // Account -- may or may not be required, but if required, must be valid.
    // NB: It may actually be a RootID if Account is used as a SecondaryClOrd-
    // ID:
    if constexpr (ProtocolFeatures<D>::s_useAccountInCancel)
    {
      if constexpr (ProtocolFeatures<D>::s_useAccountAsSecOrdID)
        // This  is rare: Under Account tag, install the SecondaryOrderID value,
        // which is the 48-bit StratHashCode:
        HEX12_FLD("1=", aos->m_stratHash48)                 // Account
      else
      {
        // This is a "normal" case -- use the configured Account:
        char const* acctStr = a_sess->m_account.data();
        CHECK_ONLY
        (
          if (utxx::unlikely(*acctStr == '\0'))
            throw utxx::runtime_error
                  ("FIX::ProtoEngine::CancelOrderImpl: Empty Account");
        )
        STR_FLD("1=", acctStr)                                // Account
      }
    }
    // Parties:
    if constexpr (ProtocolFeatures<D>::s_usePartiesInCancel)
      MK_PARTIES

    // Handling Instructions: XXX: If enabled, use 1 for Fully-Automated
    // execution:
    if constexpr (ProtocolFeatures<D>::s_hasHandlInstr)
      EXPL_FLD("21=1")

    //-----------------------------------------------------------------------//
    // "CancelOrderImpl"-Specific Flds:                                      //
    //-----------------------------------------------------------------------//
    // OrderIDs:
    assert(clReqID > a_orig_req->m_id);                       // Caller-checked
    curr = MkReqID(a_sess, curr, "11=", clReqID);             // ClOrdID
    curr = MkReqID(a_sess, curr, "41=", a_orig_req->m_id);    // OrigClOrdID

    // Symbol etc -- may or may not be required:
    if constexpr (ProtocolFeatures<D>::s_useInstrInCancel)
    {
      SecDefD const* instr = aos->m_instr;
      assert(instr != nullptr);
      MK_SYMBOL(instr)                                        // Symbol

      if constexpr (ProtocolFeatures<D>::s_useCFICode)
        STR_FLD("461=", instr->m_CFICode)

      if constexpr (ProtocolFeatures<D>::s_useSegmEtcInCancel)
        curr = MkSegmSessDest(curr, *instr);
    }
    SideT sd = (aos->m_isBuy) ? SideT::Buy : SideT::Sell;
    CHAR_FLD("54=", sd)                                       // Side

    // The SendingTime is already in the Preamble, get it from there:
    curr = stpcpy (curr, "60=" );                             // TransactTime
    curr = stpncpy(curr, ts, 22); // Incl SOH

    if constexpr (ProtocolFeatures<D>::s_useOrderQtyInCancel)
    {
      QtyN origQty = a_orig_req->GetQty<QT,QR>();
      QTY_FLD("38=", origQty)                                 // OrderQty
    }
    //-----------------------------------------------------------------------//
    // Go!                                                                   //
    //-----------------------------------------------------------------------//
    utxx::time_val sendTime =
      CompleteSendLog<MsgT::OrderCancelRequest>
        (a_sess, msgBody, curr, a_batch_send);

    //-----------------------------------------------------------------------//
    // Update the "CxlReq":                                                  //
    //-----------------------------------------------------------------------//
    a_clx_req->m_status  = Req12::StatusT::New;
    a_clx_req->m_ts_sent = sendTime;
    a_clx_req->m_seqNum  = txSN;
  }

  //=========================================================================//
  // "ModifyOrderImpl":                                                      //
  //=========================================================================//
  // Generates and sends an "OrderCancelReplaceRequest"; returns MsgSendTS:
  template
  <
    DialectT::type  D,
    bool            IsServer,
    typename        SessMgr,
    typename        Processor
  >
  void ProtoEngine<D, IsServer, SessMgr, Processor>::ModifyOrderImpl
  (
    SessData*       a_sess,                    // Non-NULL
    Req12*          DEBUG_ONLY(a_mod_req0),    // NULL
    Req12*          a_mod_req1,                // Non-NULL
    Req12 const*    a_orig_req,                // Non-NULL
    bool            a_batch_send               // To send multiple msgs at once
  )
  const
  {
    //-----------------------------------------------------------------------//
    // Checks:                                                               //
    //-----------------------------------------------------------------------//
    // XXX: Assumtions:
    // (*) At the moment, "ModifyOrderImpl" can only be issues by a Client with
    //     OrdMgmt and Modify capabilities. In that case, "a_mod_req0" must al-
    //     ways be NULL;
    // (*) "a_orig_req" has aleady been verified by the Caller:
    //
    assert(a_sess != nullptr     && a_mod_req0 == nullptr     &&
           a_mod_req1 != nullptr && a_orig_req != nullptr     &&
           a_mod_req1->m_kind    == Req12::KindT::Modify      &&
           a_mod_req1->m_status  == Req12::StatusT::Indicated &&
           a_orig_req->m_status  >= Req12::StatusT::New);
    CHECK_ONLY
    (
      if (utxx::unlikely(IsServer || !(m_sessMgr->IsOrdMgmt()) ||
                         !ProtocolFeatures<D>::s_hasModify))
        throw utxx::runtime_error
              ("FIX::ProtoEngine::ModifyOrderImpl: Not Supported");
    )
    //-----------------------------------------------------------------------//
    // Modification Params:                                                  //
    //-----------------------------------------------------------------------//
    AOS const*      aos      = a_mod_req1->m_aos;
    assert(aos != nullptr && aos == a_orig_req->m_aos);

    OrderID         modReqID = a_mod_req1->m_id;
    assert(modReqID != 0  &&   a_orig_req->m_id != 0 &&
           modReqID >          a_orig_req->m_id);

    utxx::time_val  createTS = a_mod_req1->m_ts_created;
    assert(!createTS.empty());

    PriceT          newPx    = a_mod_req1->m_px;
    assert(IsFinite(newPx));

    QtyN newQty              = a_mod_req1->GetQty    <QT,QR>();
    assert(IsPos(newQty));

    QtyN newQtyShow          = a_mod_req1->GetQtyShow<QT,QR>();
    assert(!IsNeg(newQtyShow) && newQtyShow <= newQty);

    QtyN newQtyMin           = a_mod_req1->GetQtyMin <QT,QR>();
    assert(!IsNeg(newQtyMin)  && newQtyMin  <= newQty);

    //-----------------------------------------------------------------------//
    // Preamble, Account and Parties:                                        //
    //-----------------------------------------------------------------------//
    auto   preamble =
      MkPreamble<MsgT::OrderCancelReplaceRequest>(a_sess, createTS);

    char*  curr     = std::get<0>(preamble);
    char*  msgBody  = std::get<1>(preamble);
    assert(std::get<2>(preamble) == createTS);
    SeqNum txSN     = std::get<3>(preamble);

    // Memoise the TimeStamp string ptr (len=22 incl SOH):
    char const* ts = curr - 22;

    if constexpr (ProtocolFeatures<D>::s_useAccountInModify)
    {
      if constexpr (ProtocolFeatures<D>::s_useAccountAsSecOrdID)
        // This  is rare: Under Account tag, install the SecondaryOrderID value,
        // which is the 48-bit StratHashCode:
        HEX12_FLD("1=", aos->m_stratHash48)                   // Account
      else
      {
        // This is a "normal" case -- use a configired Account:
        char const* acctStr = a_sess->m_account.data();
        CHECK_ONLY
        (
          if (utxx::unlikely(*acctStr == '\0'))
            throw utxx::runtime_error
                  ("FIX::ProtoEngine::ModifyOrderImpl: Empty Account");
        )
        STR_FLD("1=", acctStr)                                // Account
      }
    }
    // Parties:
    if constexpr (ProtocolFeatures<D>::s_usePartiesInModify)
      MK_PARTIES

    // Handling Instructions: XXX: If enabled, use 1 for Fully-Automated
    // execution:
    if constexpr (ProtocolFeatures<D>::s_hasHandlInstr)
      EXPL_FLD("21=1")

    //-----------------------------------------------------------------------//
    // "ModifyOrderImpl"-Specific Flds:                                      //
    //-----------------------------------------------------------------------//
    // OrderIDs:
    assert(modReqID > a_orig_req->m_id);                      // Caller-checked
    curr = MkReqID(a_sess, curr, "11=", modReqID);            // ClOrdID
    curr = MkReqID(a_sess, curr, "41=", a_orig_req->m_id);    // OrigClOrdID

    // Symbol etc -- may or may not be required:
    if constexpr (ProtocolFeatures<D>::s_useInstrInModify)
    {
      SecDefD const* instr = aos->m_instr;
      assert(instr != nullptr);

      MK_SYMBOL(instr)                                        // Symbol

      if constexpr (ProtocolFeatures<D>::s_useCFICode)
        STR_FLD("461=", instr->m_CFICode)

      if constexpr (ProtocolFeatures<D>::s_useSegmEtcInModify)
        curr = MkSegmSessDest(curr, *instr);
    }
    SideT sd = (aos->m_isBuy) ? SideT::Buy : SideT::Sell;
    CHAR_FLD("54=", sd)                                       // Side

    // The SendingTime is already in the Preamble, get it from there:
    curr = stpcpy (curr, "60=" );                             // TransactTime
    curr = stpncpy(curr, ts, 22); // Incl SOH

    //-----------------------------------------------------------------------//
    // Amended Price and/or Qty(s):                                          //
    //-----------------------------------------------------------------------//
    // OrdType: XXX: Currently, only Limit Orders can be Modified (this is enf-
    // orced by the  Caller). BEWARE that Currenex has its own non-stadard Ord-
    // Type encoding:
    OrderTypeT origType =  aos->m_orderType;
    assert    (origType == OrderTypeT::Limit);

    if constexpr (D == DialectT::Currenex)
      CHAR_FLD("40=", MkCurrenexOrdType(origType, aos->m_isIceberg))
    else
      // Generic Case:
      CHAR_FLD("40=", origType)                               // OrderType

    // XXX: For safety, NewPx and NewQty are always sent as part of FIX msg,
    // even if they do not change:
    QTY_FLD( "38=", newQty)                                   // OrderQty
    DEC_FLD( "44=", double(newPx))                            // Price

    // But QtyShow and QtyMin are only sent if they have changed:
    // Is  QtyShow to be changed (if supported)?
    QtyN origQty     = a_orig_req->GetQty    <QT,QR>();
    QtyN origQtyShow = a_orig_req->GetQtyShow<QT,QR>();
    QtyN origQtyMin  = a_orig_req->GetQtyMin <QT,QR>();

    if (utxx::unlikely(aos->m_isIceberg))
    {
      // So it is a Iceberg, or it was an Iceberg, and its QtyShow has changed.
      // XXX: It might be that Icebergs are supported in "New" but not in "Mod-
      // ify":
      if constexpr (!(ProtocolFeatures<D>::s_hasQtyShow        &&
                      ProtocolFeatures<D>::s_hasQtyShowInModify))
        throw utxx::badarg_error
              ("FIX::ProtoEngine::ModifyOrderImpl: Icebergs Not Supported: "
               "OrigQty=",        QR(origQty),
               ", OrigQtyShow=",  QR(origQtyShow),
               ", NewQty=",       QR(newQty),
               ", NewQtyShow=",   QR(newQtyShow));
      // If supported:
      QTY_FLD("210=", newQtyShow)                             // QtyShow
    }

    // Is QtyMin to be changed (if supported)? XXX: The problem is: The original
    // "QtyMin" could be non-0, and it is now being switched to 0 (ie no min) --
    // would this be supported by the Exchange? Still try:
    //
    if (newQtyMin != origQtyMin)
    {
      if constexpr (!ProtocolFeatures<D>::s_hasQtyMin)
        throw utxx::badarg_error
              ("FIX::ProtoEngine::ModifyOrderImpl: QtyMin Not Supported");
      QTY_FLD("110=", newQtyMin)                              // QtyMin
    }

    // NB: TimeInForce may be reqiured in Modify -- not clear why. XXX: It is
    // currently not modified -- we just re-iterate the existing value:
    //
    if constexpr (ProtocolFeatures<D>::s_useTimeInForce      &&
                  ProtocolFeatures<D>::s_useTimeInForceInModify)
    {
      TimeInForceT timeInForce = aos->m_timeInForce;

      if (utxx::unlikely(timeInForce == TimeInForceT::UNDEFINED))
        timeInForce = ProtocolFeatures<D>::s_defaultTimeInForce;

      CHAR_FLD("59=", char(timeInForce))                      // TimeInForce

      // NB: "expireDate" is conditional on "timeInForce" -- we simply re-iter-
      // ate the existing value:
      INT_FLD("432=", aos->m_expireDate)
    }

    //-----------------------------------------------------------------------//
    // Go!                                                                   //
    //-----------------------------------------------------------------------//
    utxx::time_val sendTime =
      CompleteSendLog<MsgT::OrderCancelReplaceRequest>
        (a_sess, msgBody, curr, a_batch_send);

    //-----------------------------------------------------------------------//
    // Update "ModReq1":                                                     //
    //-----------------------------------------------------------------------//
    a_mod_req1->m_status  = Req12::StatusT::New;
    a_mod_req1->m_ts_sent = sendTime;
    a_mod_req1->m_seqNum  = txSN;
  }

  //=========================================================================//
  // "CancelAllOrdersImpl":                                                  //
  //=========================================================================//
  // Generates and sends an "OrderMassCancelRequest":
  template
  <
    DialectT::type D,
    bool           IsServer,
    typename       SessMgr,
    typename       Processor
  >
  void
  ProtoEngine<D, IsServer, SessMgr, Processor>::CancelAllOrdersImpl
  (
    SessData*      a_sess,
    unsigned long  a_strat_hash48,
    SecDefD const* a_instr,    // May be NULL
    SideT          a_side,     // May be UNDEFINED
    char const*    a_segm_id   // Segment or Session ID; may be NULL or empty
  )
  const
  {
    assert(a_sess != nullptr);
    //-----------------------------------------------------------------------//
    // "CancelAllOrdersImpl" Checks:                                         //
    //-----------------------------------------------------------------------//
    // At the moment, "CancelAllOrdersImpl" can only be issued by a Client with
    // OrdMgmt and MassCancel capabilities:
    CHECK_ONLY
    (
      if (utxx::unlikely(IsServer || !(m_sessMgr->IsOrdMgmt())  ||
                         !ProtocolFeatures<D>::s_hasMassCancel))
        throw utxx::runtime_error
              ("FIX::ProtoEngine::CancelAllOrdersImpl: Not Supported");
    )
    // Generate and send an "OrderMassCancelRequst" FIX msg. NB: In that case,
    // unlike "EmulateMassCancel", we will NOT install Cancel "Req12"s in the
    // subject AOSes:
    //-----------------------------------------------------------------------//
    // "CancelAllOrdersImpl": Preamble and Account:                          //
    //-----------------------------------------------------------------------//
    auto  preamble = MkPreamble<MsgT::OrderMassCancelRequest>(a_sess);
    char*          curr     = std::get<0>(preamble);
    char*          msgBody  = std::get<1>(preamble);
    utxx::time_val createTS = std::get<2>(preamble);

    // Memoise the TimeStamp string ptr (len=22 incl SOH):
    char const* ts = curr - 22;

    // "Account":
    // XXX: In rare cases when "s_useAccountAsSecOrdID" is set and (the reverse
    // is also true) we install the SecondaryOrderID value (ie StratHash48)  in
    // the Account fld, we will NOT install it here because:
    // (*) "CancelAllOrdersIMpl" may be invoked by the Connector itself, not by
    //     a particular Strategy;
    // (*) it has no AOS associated with it, and we never explicitly notify the
    //     Strategy of its completion (but of course, we DO provide notificati-
    //     ons on the orders actually cancelled):
    // So:
    if constexpr (!ProtocolFeatures<D>::s_useAccountAsSecOrdID)
    {
      // This is a "normal" case -- use a configured Account:
      char const* acctStr = a_sess->m_account.data();
      CHECK_ONLY
      (
        if (utxx::unlikely(*acctStr == '\0'))
          throw utxx::runtime_error
                ("FIX::ProtoEngine::CancelAllOrdersImpl: Empty Account");
      )
      STR_FLD("1=", acctStr)                                  // Account
    }

    //-----------------------------------------------------------------------//
    // "CancelAllOrdersImpl": Specific Flds:                                 //
    //-----------------------------------------------------------------------//
    // ClOrdID of the "MassCancel" itself:
    OrderID reqID = MkTmpReqID(createTS);
    curr =  MkReqID(a_sess, curr, "11=", reqID);              // ClOrdID

    // Filtering by Strategy?
    if (utxx::likely(a_strat_hash48 != 0))
    {
      if constexpr (!ProtocolFeatures<D>::s_hasSecOrdIDInMassCancel)
        throw utxx::badarg_error
              ("FIX::ProtoEngine::CancelAllOrdersImpl: Filtering by Strategy "
               "Not Supported");
      HEX12_FLD("526=", a_strat_hash48)                       // SecClOrdID
    }

    // Filtering by Side?
    if (utxx::unlikely(a_side  != SideT::UNDEFINED))
      CHAR_FLD("54=",  a_side)                                // Side

    // Filtering by Symbol?
    if (utxx::unlikely(a_instr != nullptr))
    {
      // Cancel Orders for a particular Symbol only, possibly with further fil-
      // tering (eg by Side):
      MK_SYMBOL(a_instr)                                      // Symbol

      // Possibly install the Session or MktSegm ID which comes with this Symb:
      curr = MkSegmSessDest(curr, *a_instr);

      // If CFI Code is required:
      if constexpr (ProtocolFeatures<D>::s_useCFICode)
        STR_FLD("461=", a_instr->m_CFICode)

      // In this case, the Mass Cancellation Type will be "ByInstr":
      CHAR_FLD("530=", MassCancelReqT::ByInstr)
    }
    else
    if (a_segm_id != nullptr && *a_segm_id != '\0')
    {
      // There is no filtering by Symbol, but there is filtering by the whole
      // MktSegm:
      curr = MkSegmSessDest(curr, a_segm_id);
      CHAR_FLD("530=", MassCancelReqT::ForThisMkt)
    }
    else
      // In all other cases: MassCancel applies to all Sessions/MktSegms:
      CHAR_FLD("530=", MassCancelReqT::ForAllMkts)

    // The SendingTime is already in the Preamble, get it from there:
    curr = stpcpy (curr, "60=" );                             // TransactTime
    curr = stpncpy(curr, ts, 22); // Incl SOH

    // "Parties" are only required if we do filtering by them -- currently XXX
    // we do not...

    //-----------------------------------------------------------------------//
    // "CancelAllOrdersImpl": Go!                                            //
    //-----------------------------------------------------------------------//
    (void) CompleteSendLog<MsgT::OrderMassCancelRequest>
           (a_sess, msgBody, curr, false);   // NB: Not Delayed!
  }
} // End namespace FIX
} // End namespace MAQUETTE
