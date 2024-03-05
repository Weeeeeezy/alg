// vim:ts=2:et
//===========================================================================//
//                      "Venues/OPICS_Proxy/Features_FIX.h":                 //
//                  Protocol Features for OPICS_Proxy Connector              //
//===========================================================================//
#pragma once
#include "Protocols/FIX/Msgs.h"
#include "Venues/AlfaFIX2/SecDefs.h"

namespace MAQUETTE
{
namespace FIX
{
  //=========================================================================//
  // "ProtocolFeatures<OPICS_Proxy>":                                        //
  //=========================================================================//
  // XXX: Most of the features are de-configured: This is neither a MktData nor
  // an OrdMgmt "EConnector"; OPICS_Proxy is for very specific IntraStruct ses-
  // sions only. Yet it needs Session Mgmt:
  //
  template<>
  struct ProtocolFeatures<DialectT::OPICS_Proxy>
  {
    //=======================================================================//
    // Static Features:                                                      //
    //=======================================================================//
    constexpr static ApplVerIDT   s_fixVersionNum            =
      ApplVerIDT::FIX43;
    constexpr static char const*  s_exchange                 = "OPICS"; // ???
    constexpr static char const*  s_custApplVerID            = "";
    constexpr static bool         s_hasOrdMgmt               = false;
    constexpr static bool         s_hasMktData               = false;
    constexpr static bool         s_hasMktDataTrades         = false;

    constexpr static bool         s_hasReSend                = true;
    constexpr static bool         s_hasHandlInstr            = false;
    constexpr static bool         s_hasMktOrders             = false;
    constexpr static bool         s_hasStopOrders            = false;
    constexpr static bool         s_hasPeggedOrders          = false;
    constexpr static bool         s_hasCancel                = false;
    constexpr static bool         s_hasModify                = false;
    constexpr static bool         s_hasPipeLinedReqs         = false;
    constexpr static bool         s_hasPartFilledModify      = false;
    constexpr static bool         s_hasMassCancel            = false;
    constexpr static bool         s_hasSecondOrdID           = false;
    constexpr static bool         s_hasSecOrdIDInMassCancel  = false;
    constexpr static bool         s_hasAllSessnsInMassCancel = false;
    constexpr static bool         s_hasQtyShow               = false;
    constexpr static bool         s_hasQtyMin                = false;

    constexpr static bool         s_hasPasswdChange          = false;
    constexpr static bool         s_waitForTrSessStatus      = false;

    constexpr static bool         s_forceUseExecType         = false;
    constexpr static bool         s_useCFICode               = false;
    constexpr static bool         s_useTradingSessionID      = false;
    constexpr static bool         s_grpTradingSessionID      = false;
    constexpr static bool         s_useMktSegmentID          = false;
    constexpr static bool         s_useSettlDateAsSegmOrd    = false;
    constexpr static bool         s_useSettlDateAsSegmMkt    = false;
    constexpr static bool         s_useTenorInSecID          = false;
    constexpr static bool         s_useTenorAsSegmOrd        = false;
    constexpr static bool         s_useTenorAsSegmMkt        = false;
    constexpr static bool         s_useCurrDateInReqID       = false;

    constexpr static bool         s_useAccountInNew          = false;
    constexpr static bool         s_useAccountInCancel       = false;
    constexpr static bool         s_useAccountInModify       = false;
    constexpr static bool         s_useSubID                 = false;
    constexpr static bool         s_useAccountAsSecOrdID     = false;
    constexpr static bool         s_useOrderType             = false;
    constexpr static bool         s_useTimeInForce           = false;
    constexpr static TimeInForceT s_defaultTimeInForce       =
      TimeInForceT::UNDEFINED;

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

    constexpr static QtyTypeT     s_QT                       = QtyTypeT::QtyA;
    constexpr static bool         s_hasFracQtys              = false;
    constexpr static bool         s_hasFullAmount            = false;
    constexpr static bool         s_hasIncrUpdates           = false;
    constexpr static bool         s_useSegmInMDSubscr        = false;

    // In AlfaFIX2 OrderBook, Bid-Ask collisions are allowed!
    constexpr static bool         s_useOrdersLog             = false;
    constexpr static bool         s_useRelaxedOrderBook      = false;
    constexpr static bool         s_hasChangeAsPartFill      = false;
    constexpr static int          s_hasMktDepth              = false;

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
