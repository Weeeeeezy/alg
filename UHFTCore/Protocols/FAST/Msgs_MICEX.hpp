// vim:ts=2:et
//===========================================================================//
//                       "Protocols/FAST/Msgs_MICEX.hpp"                     //
//          MICEX FAST Msg Types Parameterised by Protocol Version:          //
//===========================================================================//
#pragma once
#include "Basis/PxsQtys.h"
#include "Protocols/FAST/LowLevel.hpp"
#include "Venues/MICEX/Configs_FAST.h"

namespace MAQUETTE
{
namespace FAST
{
namespace MICEX
{
  //=========================================================================//
  // The Native Qty Type:                                                    //
  //=========================================================================//
  // In FAST MICEX, we receive an Integral number of Lots (NOT  Contracts  or
  // QtyA!). XXX: This is different from MICEX OMCs which still use Contracts!
  // The following "QtyF" type is NOT used by raw FAST MICEX msgs,  but it is
  // always returned by "GetEntrySize" calls to the MDC, so type safety is en-
  // sured:
  constexpr static QtyTypeT QT           = QtyTypeT::Lots;
  constexpr static bool     WithFracQtys = false;
  using QR   = long;
  using QtyF = Qty<QT,QR>;

  //=========================================================================//
  // The following 6 FAST msg types are supported by MICEX:                  //
  //=========================================================================//
  using AssetClassT = ::MAQUETTE::MICEX::AssetClassT;

  // NB: Actual Msg Types are given separately via template specialisations:
  //
  template<ProtoVerT::type> struct IncrementalRefresh;
  template<ProtoVerT::type> struct TradesIncrement;
  template<ProtoVerT::type> struct SnapShot;
  template<ProtoVerT::type> struct SecurityDefinition;
  template<ProtoVerT::type> struct SecurityStatus;
  template<ProtoVerT::type> struct TradingSessionStatus;
  template<ProtoVerT::type> struct HeartBeat;

  //=========================================================================//
  // FAST Template IDs of "Main Msgs" for all "Logical Channles":            //
  //=========================================================================//
  // NB: Each Logical Channel is identified by a Triple:
  //     (ProtoVerT, MICEX::AssetClassT, DataFeedT);
  //     a "Technical Channel", in addition, is also characterised  by "EnvT",
  //     and consists of 2 "Technical Feeds" ("A" and "B") -- but that is imp-
  //     ortant for UDP Multi-Cast configuration, not here.
  // NB: The following array does NOT include entries for "UNDEFINED" / "_END_"
  //     enum elements:
  //
  //=========================================================================//
  // "MainTIDs":                                                             //
  //=========================================================================//
  constexpr TID MainTIDs[int(ProtoVerT  ::_END_)-1]
                        [int(AssetClassT::_END_)-1]
                        [int(DataFeedT  ::_END_)-1]
  {
    //-----------------------------------------------------------------------//
    // "ProtoVerT::Curr":                                                    //
    //-----------------------------------------------------------------------//
    // NB: Here TID=2106 really physically exists; but still, "Stats_Snap" does
    //     not logically exist!
    {
      // "AssetClassT::FX":
      // MSR  OLR   OLS   TLR   IDF   ISF
      { 3613, 3610, 3600, 3611, 2115, 2106 },

      // "AssetClassT::EQ":
      // MSR  OLR   OLS   TLR   IDF   ISF
      { 2523, 2520, 2510, 2521, 2115, 2106 }
    }
  };

  //=========================================================================//
  // "GetMainTID":                                                           //
  //=========================================================================//
  // A Logical Channel identified by (Ver, AC, DF) transmits some "Main FAST
  // Msg Type"; the following function returns the TID of that msg type:
  //
  constexpr TID GetMainTID
  (
    ProtoVerT::type   Ver,
    AssetClassT::type AC,
    DataFeedT::type   DF
  )
  {
    return
      (int(Ver) <= 0  || int(Ver) >= int(ProtoVerT  ::_END_) ||
       int(AC)  <= 0  || int(AC)  >= int(AssetClassT::_END_) ||
       int(DF)  <= 0  || int(DF)  >= int(DataFeedT  ::_END_))
      ? 0
      : MainTIDs[int(Ver)-1][int(AC)-1][int(DF)-1];
  }

  //=========================================================================//
  // Other TIDs:                                                             //
  //=========================================================================//
  // In addition, "TradingSessionStatus" (ID=2107) and "HeartBeat" (ID=2108)
  // can be sent throughout all channels (???)
  //
  constexpr TID TradingSessionStatusTID = 2107;
  constexpr TID HeartBeatTID            = 2108;

} // End namespace MICEX
} // End namespace FAST
} // End namespace MAQUETTE
