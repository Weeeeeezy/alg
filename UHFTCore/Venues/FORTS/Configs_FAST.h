// vim:ts=2:et
//===========================================================================//
//                     "Venues/FORTS/Configs_FAST.h":                        //
//                      Configs for FORTS FAST Feeds                         //
//===========================================================================//
#pragma  once

#include "Connectors/SSM_Config.h"
#include "Venues/FORTS/SecDefs.h"   // For "FORTS::AssetClassT"
#include <utxx/enum.hpp>
#include <map>
#include <utility>

namespace MAQUETTE
{
namespace FAST
{
namespace FORTS
{
  using AssetClassT = ::MAQUETTE::FORTS::AssetClassT;

  //=========================================================================//
  // Enums Making the Feed Config Key:                                       //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // FORTS FAST Protocol Versions:                                           //
  //-------------------------------------------------------------------------//
  UTXX_ENUM(
  ProtoVerT, int,
    Curr,     // "Current" ("Production") FAST version
    Next      // "Next"    ("Testing")    FAST version
  );

  //-------------------------------------------------------------------------//
  // FORTS FAST Environments:                                                //
  //-------------------------------------------------------------------------//
  UTXX_ENUM(
  EnvT, int,
    ProdF,    // Production -- uses "Curr" version, "Fast" stream
    ProdS,    // Production -- uses "Curr" version, "Slow" stream
    TestI,    // Testing over the Internel (via PPTP VPN), uses "Next" version
    TestL     // Testing Local / Leased Lines,             uses "Next" version
  );

  // XXX: Currently, "EnvT::Next" is not used at all:
  constexpr  ProtoVerT::type ImpliedProtoVerT(EnvT::type UNUSED_PARAM(a_env))
    { return ProtoVerT::type::Curr; }

  // MQTEnvT inferred from FAST::EnvT:
  inline MQTEnvT GetMQTEnv(EnvT a_env)
  {
    switch (a_env)
    {
      case EnvT::ProdF: return MQTEnvT::Prod;
      case EnvT::ProdS: return MQTEnvT::Prod;
      case EnvT::TestI: return MQTEnvT::Test;
      case EnvT::TestL: return MQTEnvT::Test;
      default         : throw  utxx::badarg_error
                               ("FAST::FORTS::GetMQTEnv: Invalid Env=",
                                EnvT::to_string(a_env));
    }
  }
  //-------------------------------------------------------------------------//
  // "DataFeed":                                                             //
  //-------------------------------------------------------------------------//
  // (For each "AssetClass" and "ProtoVer", with some exceptions -- eg "Ind" and
  // "News" have fewer feeds):
  //
  UTXX_ENUM(
  DataFeedT, int,
    OrdersLogM_Incr,    // OrdersLog  (L1..L+oo) Incremental Updates, Main
    OrdersLogM_Snap,    // OrdersLog, (L1..L+oo) SnapShots,           Main

    OrdersLogB_Incr,    // OrdersLog  (L1..L+oo) Incremental Updates, Back-Up
    OrdersLogB_Snap,    // OrdersLog  (L1..L+oo) SnapShots,           Back-Up

    // XXX: Indices and News are provided as "Trades", whereas Trades by them-
    // selves are normally inferred from "OrdersLog",  though can also be pro-
    // vided separately for "Fut" and "Opt":
    Trades_Incr,        // Trades -- normal mode -- Incremental
    Trades_Snap,        // Trades -- SnapShots

    Infos_Replay,       // SecDefs: Base Static Defs
    Infos_Incr          // SecDefs: Dynamic Updates (eg InitMargs)
  );

  //=========================================================================//
  // Actual Configs:                                                         //
  //=========================================================================//
  extern
  std::map<std::pair<EnvT::type, AssetClassT::type>,
           std::map <DataFeedT::type,
                     std::pair<SSM_Config, SSM_Config> > >   // Pair: (A, B)
  const Configs;

} // End namespace FORTS
} // End namespace FAST
} // End namespace MAQUETTE
