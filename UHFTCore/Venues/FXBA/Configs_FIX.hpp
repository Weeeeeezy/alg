// vim:ts=2:et
//===========================================================================//
//                        "Venues/FXBA/Configs_FIX.hpp":                     //
//       Server and Account Configs for FXBestAggregator FIX Connector       //
//===========================================================================//
#pragma once

#include "Basis/BaseTypes.hpp"
#include "Connectors/FIX/Configs.hpp"
#include "Venues/FXBA/SecDefs.h"
#include <utxx/config.h>
#include <utxx/error.hpp>
#include <utxx/enumv.hpp>
#include <boost/algorithm/string.hpp>
#include <string>
#include <map>
#include <vector>

namespace MAQUETTE
{
  namespace FXBA
  {
    //=======================================================================//
    // FXBA FIX Environments:                                                //
    //=======================================================================//
    // Each of these Envs exists for both EQ and FX AssetClasses:
    //
    UTXX_ENUM(
    FIXEnvT, int,
      ProdORD,  // Production -- OrdMgmt
      ProdMKT,  // Production -- MktData
      TestORD,  // Test       -- OrdMgmt
      TestMKT   // Test       -- MktData
    );

    //=======================================================================//
    // FIX Session and Account Configs for FXBA:                             //
    //=======================================================================//
    extern std::map<FIXEnvT::type, FIXSessionConfig const> const
      FIXSessionConfigs;

    // FIXME: Currently, "FIXAccountConfig"s are also compiled in, which is a
    // VERY BAD idea -- will eventually be replaced by an Erlang-based Config
    // Server:
    extern std::map<std::string,   FIXAccountConfig const> const
      FIXAccountConfigs;
  }
  // End namespace FXBA

  //=========================================================================//
  // "GetFIXSessionConfig<FXBA>":                                            //
  //=========================================================================//
  // For FXBA, "acct_key" must be of the following format:
  //
  // {AnyPrefix}-FIX-FXBA-{Env}:
  //
  // where {Env} is "FIXEnvT" literal as above; extract both "FIXSessionConfig"
  // and "FIXAccountConfig" using such a key:
  //
  template<>
  inline FIXSessionConfig const& GetFIXSessionConfig<FIX::DialectT::FXBA>
    (std::string const& a_acct_key)
  {
    std::vector<std::string> keyParts;
    boost::split  (keyParts, a_acct_key, boost::is_any_of("-"));
    size_t keyN  = keyParts.size();

    if (utxx::unlikely
       (keyN < 4 || keyParts[keyN-3] != "FIX" || keyParts[keyN-2] != "FXBA"))
      throw utxx::badarg_error
            ("GetFIXSessionConfig<FXBA>: Invalid AccountKey=", a_acct_key);

    std::string envStr = keyParts[keyN-1];    // Environment
    try
    {
      FXBA::FIXEnvT env = FXBA::FIXEnvT::from_string(envStr);
      return FXBA::FIXSessionConfigs.at(env);
    }
    catch (std::exception const& exc)
    {
      throw utxx::badarg_error
            ("GetFIXSessionConfig<FXBA>: Invalid AccountKey=", a_acct_key, ": ",
             exc.what());
    }
  }

  //=========================================================================//
  // "GetFIXAccountConfig<FXBA>":                                            //
  //=========================================================================//
  template<>
  inline FIXAccountConfig const& GetFIXAccountConfig<FIX::DialectT::FXBA>
    (std::string const& a_acct_key)
  {
    try   { return FXBA::FIXAccountConfigs.at(a_acct_key); }
    catch (...)
    {
      throw utxx::badarg_error
            ("GetFIXAccountConfig<FXBA>: Invalid AccountKey=", a_acct_key);
    }
  }

  //=========================================================================//
  // "GetFIXSrcSecDefs":                                                     //
  //=========================================================================//
  template<>
  inline std::vector<SecDefS> const&
    GetFIXSrcSecDefs<FIX::DialectT::FXBA>(MQTEnvT UNUSED_PARAM(a_cenv))
    { return FXBA::SecDefs; }

} // End namespace MAQUETTE
