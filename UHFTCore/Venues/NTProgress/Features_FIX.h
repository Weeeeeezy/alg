// vim:ts=2:et
//===========================================================================//
//                     "Venues/NTProgress/Features_FIX.h":                   //
//                     Protocol Features for NTProgress FIX                  //
//===========================================================================//
#pragma once
#include "Venues/NTProgress/SecDefs.h"
#include "Protocols/FIX/Msgs.h"
#include <cstring>

namespace MAQUETTE
{
namespace FIX
{
  //=========================================================================//
  // "ProtocolFeatures<NTProgress>":                                         //
  //=========================================================================//
  template<>
  struct ProtocolFeatures<DialectT::NTProgress>
  {
    //=======================================================================//
    // Static Features:                                                      //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // General:                                                              //
    //-----------------------------------------------------------------------//
    constexpr static ApplVerIDT   s_fixVersionNum            =
      ApplVerIDT::FIX44;
    constexpr static char const*  s_exchange                 = "NTProgress";
    constexpr static char const*  s_custApplVerID            = "";
    constexpr static QtyTypeT     s_QT                       = QtyTypeT::QtyA;
    constexpr static bool         s_hasFracQtys              = false;
    constexpr static bool         s_hasFullAmount            = false;
    constexpr static int          s_hasReSend                = true;
    constexpr static bool         s_hasPasswdChange          = false;
    constexpr static bool         s_waitForTrSessStatus      = true;
    constexpr static bool         s_useSecIDInsteadOfSymbol  = false;

    //-----------------------------------------------------------------------//
    // Ord Mgmt:                                                             //
    //-----------------------------------------------------------------------//
    constexpr static bool         s_hasOrdMgmt               = true;
    constexpr static bool         s_hasHandlInstr            = false;
    constexpr static bool         s_hasMktOrders             = true;
    constexpr static bool         s_hasStopOrders            = true;
    constexpr static bool         s_hasPeggedOrders          = false;
    constexpr static bool         s_hasCancel                = true;
    constexpr static bool         s_hasModify                = true;
    constexpr static bool         s_hasPipeLinedReqs         = true;
    constexpr static bool         s_hasPartFilledModify      = false;
    constexpr static bool         s_hasMassCancel            = false;
    constexpr static bool         s_hasSecondOrdID           = false;
    constexpr static bool         s_hasSecOrdIDInMassCancel  = false;
    constexpr static bool         s_hasAllSessnsInMassCancel = false;
    constexpr static bool         s_hasQtyShow               = true;
    constexpr static bool         s_hasQtyMin                = false;

    constexpr static bool         s_forceUseExecType         = false;
    constexpr static bool         s_useCFICode               = false;
    constexpr static bool         s_useTradingSessionID      = false;
    constexpr static bool         s_grpTradingSessionID      = false;
    constexpr static bool         s_useMktSegmentID          = false;
    constexpr static bool         s_useSettlDateAsSegmOrd    = false;
    constexpr static bool         s_useSettlDateAsSegmMkt    = false;
    constexpr static bool         s_useTenorInSecID          =
      ::MAQUETTE::NTProgress::UseTenorsInSecIDs;                    // false
    constexpr static bool         s_useTenorAsSegmOrd        = false;
    constexpr static bool         s_useTenorAsSegmMkt        = false;
    constexpr static bool         s_useCurrDateInReqID       = true;  // 24/5!

    constexpr static bool         s_useAccountInNew          = false;
    constexpr static bool         s_useAccountInCancel       = false;
    constexpr static bool         s_useAccountInModify       = false;
    constexpr static bool         s_useSubID                 = false;
    constexpr static bool         s_useAccountAsSecOrdID     = false;
    constexpr static bool         s_useOnBehalfOfCompIDInsteadOfClientID
                                                             = false;
    constexpr static bool         s_useOrderType             = true;
    constexpr static bool         s_useTimeInForce           = true;
    constexpr static TimeInForceT s_defaultTimeInForce       =
      TimeInForceT::GoodTillCancel;

    constexpr static bool         s_useNominalPxInMktOrders  = true;
    constexpr static bool         s_useIOCInMktOrders        = true;
    constexpr static bool         s_useOrderQtyInCancel      = false;
    constexpr static bool         s_useInstrInCancel         = true;
    constexpr static bool         s_useInstrInModify         = true;

    constexpr static bool         s_useSegmEtcInNew          = false;
    constexpr static bool         s_useSegmEtcInCancel       = false;
    constexpr static bool         s_useSegmEtcInModify       = false;
    constexpr static bool         s_usePartiesInNew          = false;
    constexpr static bool         s_usePartiesInCancel       = false;
    constexpr static bool         s_usePartiesInModify       = false;
    constexpr static bool         s_useTimeInForceInModify   = false;
    constexpr static bool         s_hasQtyShowInModify       = false;

    //-----------------------------------------------------------------------//
    // Mkt Data:                                                             //
    //-----------------------------------------------------------------------//
    constexpr static bool         s_hasMktData               = true;
    constexpr static bool         s_hasMktDataTrades         = false;
    constexpr static bool         s_hasSepMktDataTrdSubscrs  = false;
    constexpr static bool         s_hasStreamingQuotes       = false;
    constexpr static bool         s_hasSecurityList          = false;
    constexpr static bool         s_hasIncrUpdates           = false;
    constexpr static bool         s_useSegmInMDSubscr        = true;
    // In NTProgress OrderBook, Bid-Ask collisions are NOT allowed:
    constexpr static bool         s_useOrdersLog             = false;
    constexpr static bool         s_useExplOrdersLog         = false;
    constexpr static bool         s_useRelaxedOrderBook      = false;
    constexpr static bool         s_hasChangeIsPartFill      = false;
    constexpr static bool         s_hasNewEncdAsChange       = false;
    constexpr static bool         s_hasChangeEncdAsNew       = false;
    constexpr static int          s_hasMktDepth              = true;
    constexpr static EConnector_MktData::FindOrderBookBy  s_findOrderBooksBy =
                     EConnector_MktData::FindOrderBookBy::SubscrReqID;

    //=======================================================================//
    // "FailedOrderWas[Part]Filled": Nothing specific:                       //
    //=======================================================================//
    constexpr static FuzzyBool FailedOrderWasFilled    (int, char const*)
      { return FuzzyBool::UNDEFINED; }

    constexpr static FuzzyBool FailedOrderWasPartFilled(int, char const*)
      { return FuzzyBool::UNDEFINED; }

    //=======================================================================//
    // "OrderCeasedToExist":                                                 //
    //=======================================================================//
    // Helping the FIX OrderStateMgr to decide whether the Order still exists
    // upon receiving any Reject:
    //
    static FuzzyBool OrderCeasedToExist
    (
      int         UNUSED_PARAM(a_err_code),
      char const* a_err_msg
    )
    {
      assert(a_err_msg != nullptr);
      // XXX: Currently, we recognise the following cond -- it can mean any obs-
      // cure failure, nit necessarily wrong order price:
      return
        (strcmp(a_err_msg, "wrong price") == 0)
        ? FuzzyBool::True
        : FuzzyBool::UNDEFINED;
    }
  };
} // End namespace FIX
} // End namespace MAQUETTE
