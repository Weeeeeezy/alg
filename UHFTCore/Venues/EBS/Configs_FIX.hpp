// vim:ts=2:et
//===========================================================================//
//                      "Venues/EBS/Configs_FIX.hpp":                        //
//              Server and Account Configs for EBS FIX Connector             //
//===========================================================================//
#pragma once

#include "Basis/BaseTypes.hpp"
#include "Connectors/FIX/Configs.hpp"
#include "Venues/EBS/SecDefs.h"
#include <utxx/config.h>
#include <utxx/error.hpp>
#include <utxx/enumv.hpp>
#include <boost/algorithm/string.hpp>
#include <string>
#include <map>
#include <vector>

namespace MAQUETTE
{
  namespace EBS
  {
    //=======================================================================//
    // EBS FIX Environments:                                                 //
    //=======================================================================//
    // Each of these Envs exists for both EQ and FX AssetClasses:
    //
    UTXX_ENUM(
    FIXEnvT, int,
      Test1SESSION
    );

    //=======================================================================//
    // FIX Session and Account Configs for EBS:                              //
    //=======================================================================//
    extern std::map<FIXEnvT::type, FIXSessionConfig const> const
      FIXSessionConfigs;

    // FIXME: Currently, "FIXAccountConfig"s are also compiled in,  which is a
    // VERY BAD idea -- will eventually be replaced by an Erlang-based  Config
    // Server:
    extern std::map<std::string,   FIXAccountConfig const> const
      FIXAccountConfigs;
  }
  // End namespace EBS

  //=========================================================================//
  // "GetFIXSessionConfig<EBS>":                                             //
  //=========================================================================//
  // For EBS, "acct_key" must be of the following format:
  //
  // {AnyPrefix}-FIX-EBS-{Env}:
  //
  // where {Env} is "FIXEnvT" literal as above; extract both "FIXSessionConfig"
  // and "FIXAccountConfig" using such a key:
  //
  template<>
  inline FIXSessionConfig const& GetFIXSessionConfig<FIX::DialectT::EBS>
    (std::string const& a_acct_key)
  {
    std::vector<std::string> keyParts;
    boost::split  (keyParts, a_acct_key, boost::is_any_of("-"));
    size_t keyN  = keyParts.size();

    if (utxx::unlikely
       (keyN < 4 || keyParts[keyN-3] != "FIX" || keyParts[keyN-2] != "EBS"))
      throw utxx::badarg_error
            ("GetFIXSessionConfig<EBS>: Invalid AccountKey=", a_acct_key);

    std::string envStr = keyParts[keyN-1];    // Environment
    try
    {
      EBS::FIXEnvT env = EBS::FIXEnvT::from_string(envStr);
      return EBS::FIXSessionConfigs.at(env);
    }
    catch (std::exception const& exc)
    {
      throw utxx::badarg_error
            ("GetFIXSessionConfig<EBS>: Invalid AccountKey=", a_acct_key, ": ",
             exc.what());
    }
  }

  //=========================================================================//
  // "GetFIXAccountConfig<EBS>":                                             //
  //=========================================================================//
  template<>
  inline FIXAccountConfig const& GetFIXAccountConfig<FIX::DialectT::EBS>
    (std::string const& a_acct_key)
  {
    try   { return EBS::FIXAccountConfigs.at(a_acct_key); }
    catch (...)
    {
      throw utxx::badarg_error
            ("GetFIXAccountConfig<EBS>: Invalid AccountKey=", a_acct_key);
    }
  }

  //=========================================================================//
  // "GetFIXSrcSecDefs":                                                     //
  //=========================================================================//
  template<>
  inline std::vector<SecDefS> const&
    GetFIXSrcSecDefs<FIX::DialectT::EBS>(MQTEnvT UNUSED_PARAM(a_cenv))
    { return EBS::SecDefs; }

} // End namespace MAQUETTE
