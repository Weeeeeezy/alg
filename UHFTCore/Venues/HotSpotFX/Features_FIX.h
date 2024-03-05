// vim:ts=2:et
//===========================================================================//
//                      "Venues/HotSpotFX/Features_FIX.h":                   //
//                 Protocol Features for HotSpotFX FIX_{Gen,Link}            //
//===========================================================================//
#pragma once
#include "Protocols/FIX/Msgs.h"
#include "Venues/HotSpotFX/SecDefs.h"

namespace MAQUETTE
{
namespace FIX
{
  //=========================================================================//
  // Common Part of HotSpotFX FIX_{Gen,Link}:                                //
  //=========================================================================//
  // XXX: HotSpotFX FIX impls are currently for OrdMgmt only; FIX MktData are
  // not (yet) supported -- use ITCH instead!
  //
  struct ProtocolFeatures_HotSpotFX_Common
  {
    //=======================================================================//
    // Static Features:                                                      //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // General:                                                              //
    //-----------------------------------------------------------------------//
    constexpr static ApplVerIDT   s_fixVersionNum            =
      ApplVerIDT::FIX42;
    constexpr static char const*  s_exchange                 = "HotSpotFX";
    constexpr static char const*  s_custApplVerID            = "";
    constexpr static int          s_hasReSend                = true;
    constexpr static bool         s_hasPasswdChange          = false;
    constexpr static bool         s_waitForTrSessStatus      = false;
    constexpr static bool         s_useSecIDInsteadOfSymbol  = false;

    // NB: s_QT, s_hasFracQtys and s_hasFullAmount are provided by derived
    // structs...
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
    constexpr static bool         s_hasPipeLinedReqs         = false;
    constexpr static bool         s_hasPartFilledModify      = true;
    constexpr static bool         s_hasMassCancel            = false;
    constexpr static bool         s_hasSecondOrdID           = false;
    constexpr static bool         s_hasSecOrdIDInMassCancel  = false;
    constexpr static bool         s_hasAllSessnsInMassCancel = false;
    constexpr static bool         s_hasQtyShow               = true;
    constexpr static bool         s_hasQtyMin                = false;

    // HotSpotFX FIX version is notionally 4.2, but uses with some "injections"
    // from 4.4: Tag 150 (ExecType) prevails over Tag 39 (OrdStatus):
    constexpr static bool         s_forceUseExecType         = true;
    constexpr static bool         s_useCFICode               = false;
    constexpr static bool         s_useTradingSessionID      = false;
    constexpr static bool         s_grpTradingSessionID      = false;
    constexpr static bool         s_useMktSegmentID          = false;
    constexpr static bool         s_useSettlDateAsSegmOrd    = false;
    constexpr static bool         s_useSettlDateAsSegmMkt    = false;
    constexpr static bool         s_useTenorInSecID          =
      ::MAQUETTE::HotSpotFX::UseTenorsInSecIDs;                     // false
    constexpr static bool         s_useTenorAsSegmOrd        = false;
    constexpr static bool         s_useTenorAsSegmMkt        = false;
    constexpr static bool         s_useCurrDateInReqID       = true;  // 24/5!

    constexpr static bool         s_useAccountInNew          = false;
    constexpr static bool         s_useAccountInCancel       = false;
    constexpr static bool         s_useAccountInModify       = false;
    constexpr static bool         s_useSubID                 = true;
    constexpr static bool         s_useAccountAsSecOrdID     = false;
    constexpr static bool         s_useOnBehalfOfCompIDInsteadOfClientID
                                                             = false;
    constexpr static bool         s_useOrderType             = true;
    constexpr static bool         s_useTimeInForce           = true;
    constexpr static TimeInForceT s_defaultTimeInForce       =
      TimeInForceT::Day;

    constexpr static bool         s_useNominalPxInMktOrders  = false;
    constexpr static bool         s_useIOCInMktOrders        = true;
    constexpr static bool         s_useOrderQtyInCancel      = false;
    constexpr static bool         s_useInstrInCancel         = true;
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
    constexpr static bool         s_hasMktDataTrades         = true;
    constexpr static bool         s_hasSepMktDataTrdSubscrs  = false;
    constexpr static bool         s_hasStreamingQuotes       = false;
    constexpr static bool         s_hasSecurityList          = false;
    constexpr static bool         s_hasIncrUpdates           = false;
    constexpr static bool         s_useSegmInMDSubscr        = false;
    constexpr static bool         s_useOrdersLog             = false;
    constexpr static bool         s_useExplOrdersLog         = false;
    constexpr static bool         s_useRelaxedOrderBook      = false;
    constexpr static bool         s_hasChangeIsPartFill      = true;
    constexpr static bool         s_hasNewEncdAsChange       = false;
    constexpr static bool         s_hasChangeEncdAsNew       = false;
    constexpr static int          s_hasMktDepth              = true;
    constexpr static EConnector_MktData::FindOrderBookBy  s_findOrderBooksBy =
                     EConnector_MktData::FindOrderBookBy::Symbol;

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

  //=========================================================================//
  // "ProtocolFeatures<HotSpotFX_Gen>":                                      //
  //=========================================================================//
  // Orders can contain Fractional Qtys, so no FullAmt:
  template<>
  struct ProtocolFeatures<DialectT::HotSpotFX_Gen>:
  public ProtocolFeatures_HotSpotFX_Common
  {
    constexpr static QtyTypeT     s_QT            = QtyTypeT::QtyA;
    constexpr static bool         s_hasFracQtys   = true;
    constexpr static bool         s_hasFullAmount = false;
  };

  //=========================================================================//
  // "ProtocolFeatures<HotSpotFX_Link>":                                     //
  //=========================================================================//
  // Uses FullAmt -- so no FracQtys:
  template<>
  struct ProtocolFeatures<DialectT::HotSpotFX_Link>:
  public ProtocolFeatures_HotSpotFX_Common
  {
    constexpr static QtyTypeT     s_QT            = QtyTypeT::QtyA;
    constexpr static bool         s_hasFracQtys   = false;
    constexpr static bool         s_hasFullAmount = true;
  };
} // End namespace FIX
} // End namespace MAQUETTE

