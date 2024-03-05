// vim:ts=2:et
//===========================================================================//
//                       "Venues/Currenex/Features_FIX.h":                   //
//                       Protocol Features for Currenex FIX                  //
//===========================================================================//
#pragma once
#include "Protocols/FIX/Msgs.h"
#include "Venues/Currenex/SecDefs.h"

namespace MAQUETTE
{
namespace FIX
{
  //=========================================================================//
  // "ProtocolFeatures<Currenex>":                                           //
  //=========================================================================//
  template<>
  struct ProtocolFeatures<DialectT::Currenex>
  {
    //=======================================================================//
    // Static Features:                                                      //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // General:                                                              //
    //-----------------------------------------------------------------------//
    constexpr static ApplVerIDT   s_fixVersionNum            =
      ApplVerIDT::FIX44;
    constexpr static char const*  s_exchange                 = "Currenex";
    constexpr static char const*  s_custApplVerID            = "";
    constexpr static QtyTypeT     s_QT                       = QtyTypeT::QtyA;
    constexpr static bool         s_hasFracQtys              = true;
    constexpr static bool         s_hasFullAmount            = false;
    constexpr static int          s_hasReSend                = true;
    constexpr static bool         s_hasPasswdChange          = false;
    constexpr static bool         s_waitForTrSessStatus      = true;
    constexpr static bool         s_useSecIDInsteadOfSymbol  = false;

    //-----------------------------------------------------------------------//
    // Ord Mgmt:                                                             //
    //-----------------------------------------------------------------------//
    constexpr static bool         s_hasOrdMgmt               = true;
    constexpr static bool         s_hasHandlInstr            = true;
    constexpr static bool         s_hasMktOrders             = true;
    constexpr static bool         s_hasStopOrders            = true;
    constexpr static bool         s_hasPeggedOrders          = true;
    constexpr static bool         s_hasCancel                = true;
    constexpr static bool         s_hasModify                = true;
    constexpr static bool         s_hasPipeLinedReqs         = true;  // TRY!
    constexpr static bool         s_hasPartFilledModify      = false;
    constexpr static bool         s_hasMassCancel            = false;
    constexpr static bool         s_hasSecondOrdID           = false;
    constexpr static bool         s_hasSecOrdIDInMassCancel  = false;
    constexpr static bool         s_hasAllSessnsInMassCancel = false;
    constexpr static bool         s_hasQtyShow               = true;
    constexpr static bool         s_hasQtyMin                = true;

    constexpr static bool         s_forceUseExecType         = true;  // ???
    constexpr static bool         s_useCFICode               = false;
    constexpr static bool         s_useTradingSessionID      = false;
    constexpr static bool         s_grpTradingSessionID      = false;
    constexpr static bool         s_useMktSegmentID          = false;
    constexpr static bool         s_useSettlDateAsSegmOrd    = false;
    constexpr static bool         s_useSettlDateAsSegmMkt    = false;
    constexpr static bool         s_useTenorInSecID          =
      ::MAQUETTE::Currenex::UseTenorsInSecIDs;              // false
    constexpr static bool         s_useTenorAsSegmOrd        = false;
    constexpr static bool         s_useTenorAsSegmMkt        = false;
    constexpr static bool         s_useCurrDateInReqID       = true;  // 24/5!

    // XXX: Accounts and SubIDs are not used for Currenex, though in general
    // they are supported there:
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
      TimeInForceT::Day;

    constexpr static bool         s_useNominalPxInMktOrders  = false;
    constexpr static bool         s_useIOCInMktOrders        = false;
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
    constexpr static bool         s_hasMktDataTrades         = true;
    constexpr static bool         s_hasSepMktDataTrdSubscrs  = true;  // NB!
    constexpr static bool         s_hasStreamingQuotes       = false;
    constexpr static bool         s_hasSecurityList          = true;
    constexpr static bool         s_hasIncrUpdates           = true;
    constexpr static bool         s_useSegmInMDSubscr        = false;
    constexpr static bool         s_useOrdersLog             = true;
    constexpr static bool         s_useExplOrdersLog         = true;
    constexpr static bool         s_useRelaxedOrderBook      = false;
    constexpr static bool         s_hasChangeIsPartFill      = false;
    constexpr static bool         s_hasNewEncdAsChange       = false;
    constexpr static bool         s_hasChangeEncdAsNew       = true;  // XXX!!!
    constexpr static int          s_hasMktDepth              = true;
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

