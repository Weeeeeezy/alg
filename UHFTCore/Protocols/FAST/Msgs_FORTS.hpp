// vim:ts=2:et
//===========================================================================//
//                       "Protocols/FAST/Msgs_FORTS.hpp"                     //
//          FORTS FAST Msg Types Parameterised by Protocol Version:          //
//===========================================================================//
#pragma once
#include "Basis/PxsQtys.h"
#include "Protocols/FAST/LowLevel.hpp"
#include "Venues/FORTS/Configs_FAST.h"

namespace MAQUETTE
{
namespace FAST
{
namespace FORTS
{
  //=========================================================================//
  // The Native Qty Type:                                                    //
  //=========================================================================//
  // In FAST FORTS, we always get an Integral number of Contracts.  XXX: This
  // type is NOT used internally in the following Msgs, but it is returned by
  // "GetEntrySize" calls to the MDC, so type safety is always ensured:
  //
  constexpr static QtyTypeT QT           = QtyTypeT::Contracts;
  constexpr static bool     WithFracQtys = false;
  using QR   = long;
  using QtyF = Qty<QT,QR>;

  //=========================================================================//
  // The following FAST Msg Types are supported by FORTS:                  //
  //=========================================================================//
  // NB: Actual Msg Types are given separately via template specialisations:
  //     TIDs are also version-specific!
  //
  template<ProtoVerT::type> struct    IncrementalRefresh;
  template<ProtoVerT::type> struct    SnapShot;
  template<ProtoVerT::type> struct    SecurityDefinition;
  template<ProtoVerT::type> struct    SecurityDefinitionUpdate;
  template<ProtoVerT::type> struct    SecurityStatus;
  template<ProtoVerT::type> struct    HeartBeat;
  template<ProtoVerT::type> struct    SequenceReset;
  template<ProtoVerT::type> struct    TradingSessionStatus;
  template<ProtoVerT::type> struct    News;
  template<ProtoVerT::type> struct    OrdersLogIncrRefresh;
  template<ProtoVerT::type> struct    OrdersLogSnapShot;

  //=========================================================================//
  // TIDs of FORTS FAST Msgs -- also version-dependent:                      //
  //=========================================================================//
  template<ProtoVerT::type> constexpr TID IncrementalRefreshTID();
  template<ProtoVerT::type> constexpr TID SnapShotTID();
  template<ProtoVerT::type> constexpr TID SecurityDefinitionTID();
  template<ProtoVerT::type> constexpr TID SecurityDefinitionUpdateTID();
  template<ProtoVerT::type> constexpr TID SecurityStatusTID();
  template<ProtoVerT::type> constexpr TID HeartBeatTID();
  template<ProtoVerT::type> constexpr TID SequenceResetTID();
  template<ProtoVerT::type> constexpr TID TradingSessionStatusTID();
  template<ProtoVerT::type> constexpr TID NewsTID();
  template<ProtoVerT::type> constexpr TID OrdersLogIncrRefreshTID();
  template<ProtoVerT::type> constexpr TID OrdersLogSnapShotTID();

} // End namespace FORTS
} // End namespace FAST
} // End namespace MAQUETTE
