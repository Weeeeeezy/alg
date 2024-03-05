// vim:ts=2:et
//===========================================================================//
//                        "Venues/FORTS/Features_FIX.h":                     //
//                        Protocol Features for FORTS FIX                    //
//===========================================================================//
#pragma once
#include "Protocols/FIX/Msgs.h"
#include "Venues/FORTS/SecDefs.h"

namespace MAQUETTE
{
namespace FIX
{
  //=========================================================================//
  // "ProtocolFeatures<FORTS>":                                              //
  //=========================================================================//
  // NB: "s_useCurrDateInReqID" is "true" because the Evening Session formally
  // belongs to the next trading day (normally starts at 19:00 MSK),  so if we
  // start re-using the ReqIDs the following morning, they could conflict with
  // those used in the prev Evening Session:
  //
  template<>
  struct ProtocolFeatures<DialectT::FORTS>
  {
    //=======================================================================//
    // Static Features:                                                      //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // General:                                                              //
    //-----------------------------------------------------------------------//
    constexpr static ApplVerIDT   s_fixVersionNum            =
      ApplVerIDT::FIX44;
    constexpr static char const*  s_exchange                 = "FORTS";
    constexpr static char const*  s_custApplVerID            = "";
    constexpr static QtyTypeT     s_QT                       = QtyTypeT::QtyA;
    constexpr static bool         s_hasFracQtys              = false;
    constexpr static bool         s_hasFullAmount            = false;
    constexpr static bool         s_hasPasswdChange          = false;
    constexpr static bool         s_waitForTrSessStatus      = false;
    constexpr static bool         s_useSecIDInsteadOfSymbol  = false;

    //-----------------------------------------------------------------------//
    // Ord Mgmt:                                                             //
    //-----------------------------------------------------------------------//
    constexpr static bool         s_hasOrdMgmt               = true;
    constexpr static int          s_hasReSend                = true;
    constexpr static bool         s_hasHandlInstr            = false;
    constexpr static bool         s_hasMktOrders             = false;
    constexpr static bool         s_hasStopOrders            = false;
    constexpr static bool         s_hasPeggedOrders          = false;
    constexpr static bool         s_hasCancel                = true;
    constexpr static bool         s_hasModify                = true;
    constexpr static bool         s_hasPipeLinedReqs         = false; // !!!
    constexpr static bool         s_hasPartFilledModify      = true;  // !!!
    constexpr static bool         s_hasMassCancel            = true;
    constexpr static bool         s_hasSecondOrdID           = true;
    constexpr static bool         s_hasSecOrdIDInMassCancel  = false;
    constexpr static bool         s_hasAllSessnsInMassCancel = false;
    constexpr static bool         s_hasQtyShow               = false;
    constexpr static bool         s_hasQtyMin                = false;

    constexpr static bool         s_forceUseExecType         = false;
    constexpr static bool         s_useCFICode               = true;
    constexpr static bool         s_useTradingSessionID      = false;
    constexpr static bool         s_grpTradingSessionID      = false;
    constexpr static bool         s_useMktSegmentID          = true;
    constexpr static bool         s_useSettlDateAsSegmOrd    = false;
    constexpr static bool         s_useSettlDateAsSegmMkt    = false;
    constexpr static bool         s_useTenorInSecID          =
      ::MAQUETTE::FORTS::UseTenorsInSecIDs;                         // false
    constexpr static bool         s_useTenorAsSegmOrd        = false;
    constexpr static bool         s_useTenorAsSegmMkt        = false;
    constexpr static bool         s_useCurrDateInReqID       = true;

    constexpr static bool         s_useAccountInNew          = true;
    constexpr static bool         s_useAccountInCancel       = false;
    constexpr static bool         s_useAccountInModify       = false;
    constexpr static bool         s_useSubID                 = false;
    constexpr static bool         s_useAccountAsSecOrdID     = false;
    constexpr static bool         s_useOnBehalfOfCompIDInsteadOfClientID
                                                             = false;
    constexpr static bool         s_useOrderType             = true;
    constexpr static bool         s_useTimeInForce           = true;
    constexpr static TimeInForceT s_defaultTimeInForce       =
      TimeInForceT::Day;

    constexpr static bool         s_useNominalPxInMktOrders  = false;
    constexpr static bool         s_useIOCInMktOrders        = false;
    constexpr static bool         s_useOrderQtyInCancel      = true;
    constexpr static bool         s_useInstrInCancel         = true;
    constexpr static bool         s_useInstrInModify         = true;

    constexpr static bool         s_useSegmEtcInNew          = true;
    constexpr static bool         s_useSegmEtcInCancel       = false;
    constexpr static bool         s_useSegmEtcInModify       = false;
    constexpr static bool         s_usePartiesInNew          = true;
    constexpr static bool         s_usePartiesInCancel       = true;
    constexpr static bool         s_usePartiesInModify       = true;
    constexpr static bool         s_useTimeInForceInModify   = false;
    constexpr static bool         s_hasQtyShowInModify       = false;

    //-----------------------------------------------------------------------//
    // Mkt Data: Not Supported:                                              //
    //-----------------------------------------------------------------------//
    constexpr static bool         s_hasMktData               = false;
    constexpr static bool         s_hasMktDataTrades         = false;
    constexpr static bool         s_hasSepMktDataTrdSubscrs  = false;
    constexpr static bool         s_hasStreamingQuotes       = false;
    constexpr static bool         s_hasSecurityList          = false;
    constexpr static bool         s_hasIncrUpdates           = false;
    constexpr static bool         s_useSegmInMDSubscr        = false;
    constexpr static bool         s_useOrdersLog             = false;
    constexpr static bool         s_useExplOrdersLog         = false;
    constexpr static bool         s_useRelaxedOrderBook      = false; // No OB!
    constexpr static bool         s_hasChangeIsPartFill      = false;
    constexpr static bool         s_hasNewEncdAsChange       = false;
    constexpr static bool         s_hasChangeEncdAsNew       = false;
    constexpr static int          s_hasMktDepth              = false;
    constexpr static EConnector_MktData::FindOrderBookBy  s_findOrderBooksBy =
                     EConnector_MktData::FindOrderBookBy::UNDEFINED;

    //=======================================================================//
    // Helpers: Nothing specific yet:                                        //
    //=======================================================================//
    constexpr static FuzzyBool FailedOrderWasFilled    (int, char const*)
      { return FuzzyBool::UNDEFINED; }

    constexpr static FuzzyBool FailedOrderWasPartFilled(int, char const*)
      { return FuzzyBool::UNDEFINED; }

    constexpr static FuzzyBool OrderCeasedToExist      (int, char const*)
      { return FuzzyBool::UNDEFINED; }
  };
} // End namespace FIX
} // End namespace MAQUETTE
