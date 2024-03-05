// vim:ts=2:et
//===========================================================================//
//                        "Venues/MICEX/Features_FIX.h":                     //
//                       Protocol Features for MICEX FIX                     //
//===========================================================================//
#pragma once
#include "Protocols/FIX/Msgs.h"
#include "Venues/MICEX/SecDefs.h"

namespace MAQUETTE
{
namespace FIX
{
  //=========================================================================//
  // "ProtocolFeatures<MICEX>":                                              //
  //=========================================================================//
  template<>
  struct ProtocolFeatures<DialectT::MICEX>
  {
    //=======================================================================//
    // Static Features:                                                      //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // General:                                                              //
    //-----------------------------------------------------------------------//
    constexpr static ApplVerIDT   s_fixVersionNum            =
      ApplVerIDT::FIX44;
    constexpr static char const*  s_exchange                 = "MICEX";
    constexpr static char const*  s_custApplVerID            = "";
    constexpr static QtyTypeT     s_QT                       = QtyTypeT::QtyA;
    constexpr static bool         s_hasFracQtys              = false;
    constexpr static bool         s_hasFullAmount            = false;
    constexpr static int          s_hasReSend                = true;
    constexpr static bool         s_hasPasswdChange          = true;
    constexpr static bool         s_waitForTrSessStatus      = false;
    constexpr static bool         s_useSecIDInsteadOfSymbol  = false;

    //-----------------------------------------------------------------------//
    // Ord Mgmt:                                                             //
    //-----------------------------------------------------------------------//
    constexpr static bool         s_hasOrdMgmt               = true;
    constexpr static bool         s_hasHandlInstr            = false;
    constexpr static bool         s_hasMktOrders             = true;
    constexpr static bool         s_hasStopOrders            = false;
    constexpr static bool         s_hasPeggedOrders          = false;
    constexpr static bool         s_hasCancel                = true;
    constexpr static bool         s_hasModify                = true;
    constexpr static bool         s_hasPipeLinedReqs         = true;
    constexpr static bool         s_hasPartFilledModify      = false;
    constexpr static bool         s_hasMassCancel            = true;
    constexpr static bool         s_hasSecondOrdID           = true;
    constexpr static bool         s_hasSecOrdIDInMassCancel  = true;
    constexpr static bool         s_hasAllSessnsInMassCancel = true;
    constexpr static bool         s_hasQtyShow               = true;
    constexpr static bool         s_hasQtyMin                = false;

    constexpr static bool         s_forceUseExecType         = false;
    constexpr static bool         s_useCFICode               = false;
    constexpr static bool         s_useTradingSessionID      = true;
    constexpr static bool         s_grpTradingSessionID      = true;
    constexpr static bool         s_useMktSegmentID          = false;
    constexpr static bool         s_useSettlDateAsSegmOrd    = false;
    constexpr static bool         s_useSettlDateAsSegmMkt    = false;
    constexpr static bool         s_useTenorInSecID          =
      ::MAQUETTE::MICEX::UseTenorsInSecIDs;                 // false
    constexpr static bool         s_useTenorAsSegmOrd        = false;
    constexpr static bool         s_useTenorAsSegmMkt        = false;
    constexpr static bool         s_useCurrDateInReqID       = false; // OK!

    constexpr static bool         s_useAccountInNew          = true;
    constexpr static bool         s_useAccountInCancel       = false;
    constexpr static bool         s_useAccountInModify       = true;
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
    constexpr static bool         s_useInstrInCancel         = false;
    constexpr static bool         s_useInstrInModify         = true;

    constexpr static bool         s_useSegmEtcInNew          = true;
    constexpr static bool         s_useSegmEtcInCancel       = false;
    constexpr static bool         s_useSegmEtcInModify       = true;
    constexpr static bool         s_usePartiesInNew          = true;
    constexpr static bool         s_usePartiesInCancel       = true;
    constexpr static bool         s_usePartiesInModify       = true;
    constexpr static bool         s_useTimeInForceInModify   = false;
    constexpr static bool         s_hasQtyShowInModify       = false;

    //-----------------------------------------------------------------------//
    // Mkt Data: Not Supported, use FAST instead:                            //
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
    // "FailedOrderWasFilled":                                               //
    //=======================================================================//
    // A helper function to determine whether an Order for which a Modify or
    // Cancel Req has Failed, had actually been Filled:
    //
    static FuzzyBool FailedOrderWasFilled
    (
      int         UNUSED_PARAM(a_err_code),
      char const* a_err_msg
    )
    {
      assert(a_err_msg != nullptr);
      // In the following case, we know that the order has been Filled:
      return
        (strncmp(a_err_msg, "ERROR: (915) Can't withdraw order. The order "
                            "already matched", 60) == 0)
        ? FuzzyBool::True
        : FuzzyBool::UNDEFINED;
    }

    //=======================================================================//
    // "FailedOrderWasPartFilled":                                           //
    //=======================================================================//
    // A helper function to determine whether an Order for which a Modify Req
    // has failed, has actually been Part-Filled:
    //
    static FuzzyBool FailedOrderWasPartFilled
    (
      int         UNUSED_PARAM(a_err_code),
      char const* a_err_msg
    )
    {
      assert(a_err_msg != nullptr);
      // In the following case, we know that the order has been Part-Filled:
      return
        (strncmp(a_err_msg, "ERROR: (900) Amend not allowed for partial "
                            "matched orders",  57) == 0)
        ? FuzzyBool::True
        : FuzzyBool::UNDEFINED;
    }

    //=======================================================================//
    // "OrderCeasedToExist":                                                 //
    //=======================================================================//
    // Helping the FIX OrderStateMgr to decide whether the Order still exists
    // upon receiving any Reject:
    //
    static FuzzyBool OrderCeasedToExist
    (
      int         a_err_code,
      char const* a_err_msg
    )
    {
      return
        (FailedOrderWasFilled    (a_err_code, a_err_msg) == FuzzyBool::True)
        ? FuzzyBool::True  :
        (FailedOrderWasPartFilled(a_err_code, a_err_msg) == FuzzyBool::True)
        ? FuzzyBool::False
        : FuzzyBool::UNDEFINED;
    }
  };
} // End namespace FIX
} // End namespace MAQUETTE
