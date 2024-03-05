// vim:ts=2:et
//===========================================================================//
//                     "Venues/MICEX/Configs_FAST.h":                        //
//                      Configs for MICEX FAST Feeds                         //
//===========================================================================//
#pragma  once

#include "Connectors/SSM_Config.h"
#include "Venues/MICEX/SecDefs.h" // For "MICEX::AssetClassT"
#include <utxx/enum.hpp>
#include <map>
#include <utility>
#include <tuple>

namespace MAQUETTE
{
namespace FAST
{
namespace MICEX
{
  using AssetClassT = ::MAQUETTE::MICEX::AssetClassT;

  //=========================================================================//
  // Enums Making the Feed Config Key:                                       //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // MICEX FAST Protocol Versions:                                           //
  //-------------------------------------------------------------------------//
  // NB: In addition to Feed Configs, MICEX msg types are parameterised by this
  // enum:
  UTXX_ENUM(
  ProtoVerT, int,
    Curr,    // Always available
    Next     // XXX: May or may not be actually available
  );

  //-------------------------------------------------------------------------//
  // MICEX FAST Environments:                                                //
  //-------------------------------------------------------------------------//
  // NB: "Testing/UAT" disambiguation: The  term  "UAT"  applies to the "Curr"
  // "ProtoVerT", "Testing" -- to the "Next" one; if, at the moment,  there is
  // only one "ProtoVerT" available,  "Testing" and "UAT"  should be the same:
  //
  UTXX_ENUM(
  EnvT, int,
    Prod1,   // Production 1, for Curr only
    Prod2,   // Production 2, for Curr only
    TestI,   // Testing/UAT over the Internel (via PPTP VPN)
    TestL    // Testing/UAT,            Local (Co-Location)
  );

  // MQTEnvT inferred from MICEX::EnvT:
  inline MQTEnvT GetMQTEnv(EnvT a_env)
  {
    switch (a_env)
    {
      case EnvT::Prod1: return MQTEnvT::Prod;
      case EnvT::Prod2: return MQTEnvT::Prod;
      case EnvT::TestI: return MQTEnvT::Test;
      case EnvT::TestL: return MQTEnvT::Test;
      default:          throw  utxx::badarg_error
                               ("FAST::MICEX::GetMQTEnv: Invalid Env=",
                                EnvT::to_string(a_env));
    }
  }
  //-------------------------------------------------------------------------//
  // "DataFeed":                                                             //
  //-------------------------------------------------------------------------//
  UTXX_ENUM(
  DataFeedT, int,
    Stats_Incr,               // MSR
    Orders_Incr,              // OLR
    Orders_Snap,              // OLS
    Trades_Incr,              // TLR
    Instr_Defs,               // IDF
    Instr_Status              // ISF
  );

  //=========================================================================//
  // Actual Configs:                                                         //
  //=========================================================================//
  extern
  std::map<std::tuple<ProtoVerT::type, EnvT::type, AssetClassT::type>,
           std::map  <DataFeedT::type,
                      std::pair<SSM_Config, SSM_Config> > >  // Pair: (A, B)
  const Configs;

} // End namespace MICEX
} // End namespace FAST
} // End namespace MAQUETTE
