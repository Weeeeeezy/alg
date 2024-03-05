// vim:ts=2:et
//===========================================================================//
//                     "Venues/Cumberland/Features_FIX.h":                   //
//                     Protocol Features for Cumberland FIX                  //
//===========================================================================//
#pragma once
#include "Protocols/FIX/Msgs.h"
#include "Venues/Cumberland/SecDefs.h"

namespace MAQUETTE
{
namespace FIX
{
  //=========================================================================//
  // "ProtocolFeatures<Cumberland>":                                         //
  //=========================================================================//
  template<>
  struct ProtocolFeatures<DialectT::Cumberland>
  {
    //=======================================================================//
    // Static Features:                                                      //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // General:                                                              //
    //-----------------------------------------------------------------------//
    constexpr static ApplVerIDT   s_fixVersionNum            =
      ApplVerIDT::FIX44;
    constexpr static char const*  s_exchange                 = "Cumberland";
    constexpr static char const*  s_custApplVerID            = "";
    constexpr static QtyTypeT     s_QT                       = QtyTypeT::QtyA;
    constexpr static bool         s_hasFracQtys              = true;
    constexpr static bool         s_hasFullAmount            = false;
    constexpr static int          s_hasReSend                = true;
    constexpr static bool         s_hasPasswdChange          = false;
    constexpr static bool         s_waitForTrSessStatus      = false;
    constexpr static bool         s_useSecIDInsteadOfSymbol  = false;

    //-----------------------------------------------------------------------//
    // Ord Mgmt:                                                             //
    //-----------------------------------------------------------------------//
    constexpr static bool         s_hasOrdMgmt               = true;
    constexpr static bool         s_hasHandlInstr            = false;
    constexpr static bool         s_hasMktOrders             = true;   // ONLY
    constexpr static bool         s_hasStopOrders            = false;
    constexpr static bool         s_hasPeggedOrders          = false;
    constexpr static bool         s_hasCancel                = false;
    constexpr static bool         s_hasModify                = false;
    constexpr static bool         s_hasPipeLinedReqs         = false;  // ???
    constexpr static bool         s_hasPartFilledModify      = false;
    constexpr static bool         s_hasMassCancel            = false;
    constexpr static bool         s_hasSecondOrdID           = false;
    constexpr static bool         s_hasSecOrdIDInMassCancel  = false;
    constexpr static bool         s_hasAllSessnsInMassCancel = false;
    constexpr static bool         s_hasQtyShow               = false;
    constexpr static bool         s_hasQtyMin                = false;

    constexpr static bool         s_forceUseExecType         = true;  // ???
    constexpr static bool         s_useCFICode               = false;
    constexpr static bool         s_useTradingSessionID      = false;
    constexpr static bool         s_grpTradingSessionID      = false;
    constexpr static bool         s_useMktSegmentID          = false;
    constexpr static bool         s_useSettlDateAsSegmOrd    = false;
    constexpr static bool         s_useSettlDateAsSegmMkt    = false;
    constexpr static bool         s_useTenorInSecID          = false;
    constexpr static bool         s_useTenorAsSegmOrd        = false;
    constexpr static bool         s_useTenorAsSegmMkt        = false;
    constexpr static bool         s_useCurrDateInReqID       = true;  // 24/5!

    constexpr static bool         s_useAccountInNew          = false;
    constexpr static bool         s_useAccountInCancel       = false;
    constexpr static bool         s_useAccountInModify       = false;
    constexpr static bool         s_useSubID                 = true;
    constexpr static bool         s_useAccountAsSecOrdID     = false;
    constexpr static bool         s_useOnBehalfOfCompIDInsteadOfClientID
                                                             = true;
    constexpr static bool         s_useOrderType             = false;
    constexpr static bool         s_useTimeInForce           = false;
    constexpr static TimeInForceT s_defaultTimeInForce       =
      TimeInForceT::ImmedOrCancel;

    constexpr static bool         s_useNominalPxInMktOrders  = false;
    constexpr static bool         s_useIOCInMktOrders        = false;
    constexpr static bool         s_useOrderQtyInCancel      = false;
    constexpr static bool         s_useInstrInCancel         = false;
    constexpr static bool         s_useInstrInModify         = false;

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
    constexpr static bool         s_hasStreamingQuotes       = true;   // NB!
    constexpr static bool         s_hasSecurityList          = true;
    constexpr static bool         s_hasIncrUpdates           = false;
    constexpr static bool         s_useSegmInMDSubscr        = false;
    constexpr static bool         s_useOrdersLog             = false;
    constexpr static bool         s_useExplOrdersLog         = false;
    constexpr static bool         s_useRelaxedOrderBook      = false;
    constexpr static bool         s_hasChangeIsPartFill      = false;
    constexpr static bool         s_hasNewEncdAsChange       = false;
    constexpr static bool         s_hasChangeEncdAsNew       = false;
    constexpr static int          s_hasMktDepth              = false;
    constexpr static EConnector_MktData::FindOrderBookBy  s_findOrderBooksBy =
                     EConnector_MktData::FindOrderBookBy::SubscrReqID;

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

