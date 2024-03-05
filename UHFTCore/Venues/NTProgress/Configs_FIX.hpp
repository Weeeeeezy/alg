// vim:ts=2:et
//===========================================================================//
//                      "Venues/NTProgress/Configs_FIX.hpp":                 //
//              Server and Account Configs for NTProgress FIX Connector      //
//===========================================================================//
#pragma once

#include "Basis/BaseTypes.hpp"
#include "Connectors/FIX/Configs.hpp"
#include "Venues/NTProgress/SecDefs.h"
#include <utxx/config.h>
#include <utxx/error.hpp>
#include <utxx/enumv.hpp>
#include <boost/algorithm/string.hpp>
#include <string>
#include <map>
#include <vector>

namespace MAQUETTE
{
  namespace NTProgress
  {
    //=======================================================================//
    // NTProgress FIX Environments:                                          //
    //=======================================================================//
    // Each of these Envs exists for both EQ and FX AssetClasses:
    //
    UTXX_ENUM(
    FIXEnvT, int,
      ProdORD,
      ProdMKT,
      TestORD,
      TestMKT
    );

    //=======================================================================//
    // FIX Session and Account Configs for NTProgress:                       //
    //=======================================================================//
    extern std::map<FIXEnvT::type, FIXSessionConfig const> const
      FIXSessionConfigs;

    // FIXME: Currently, "FIXAccountConfig"s are also compiled in,  which is a
    // VERY BAD idea -- will eventually be replaced by an Erlang-based  Config
    // Server:
    extern std::map<std::string,   FIXAccountConfig const> const
      FIXAccountConfigs;
  }
  // End namespace NTProgress

  //=========================================================================//
  // "GetFIXSessionConfig<NTProgress>":                                      //
  //=========================================================================//
  // For NTProgress, "acct_key" must be of the following format:
  //
  // {AnyPrefix}-FIX-NTProgress-{Env}:
  //
  // where {Env} is "FIXEnvT" literal as above; extract both "FIXSessionConfig"
  // and "FIXAccountConfig" using such a key:
  //
  template<>
  inline FIXSessionConfig  const& GetFIXSessionConfig<FIX::DialectT::NTProgress>
    (std::string const& a_acct_key)
  {
    std::vector<std::string> keyParts;
    boost::split  (keyParts, a_acct_key, boost::is_any_of("-_:"));
    size_t keyN  = keyParts.size();

    if (utxx::unlikely
       (keyN < 4         || keyParts[keyN-3] != "FIX" ||
        keyParts[keyN-2] != "NTProgress"))
      throw utxx::badarg_error
            ("GetFIXSessionConfig<NTProgress>: Invalid AccounKey=", a_acct_key);

    std::string envStr = keyParts[keyN-1];    // Environment
    try
    {
      NTProgress::FIXEnvT env = NTProgress::FIXEnvT::from_string(envStr);
      return NTProgress::FIXSessionConfigs.at(env);
    }
    catch (std::exception const& exc)
    {
      throw utxx::badarg_error
            ("GetFIXSessionConfig<NTProgress>: Invalid AccountKey=", a_acct_key,
             ": ", exc.what());
    }
  }

  //=========================================================================//
  // "GetFIXAccountConfig<NTProgress>":                                      //
  //=========================================================================//
  template<>
  inline FIXAccountConfig const& GetFIXAccountConfig<FIX::DialectT::NTProgress>
    (std::string const& a_acct_key)
  {
    try   { return NTProgress::FIXAccountConfigs.at(a_acct_key); }
    catch (...)
    {
      throw utxx::badarg_error
            ("GetFIXAccountConfig<NTProgress>: Invalid AccountKey=", a_acct_key);
    }
  }

  //=========================================================================//
  // "GetFIXSrcSecDefs":                                                     //
  //=========================================================================//
  template<>
  inline std::vector<SecDefS> const&
    GetFIXSrcSecDefs<FIX::DialectT::NTProgress>(MQTEnvT UNUSED_PARAM(a_cenv))
    { return NTProgress::SecDefs; }

} // End namespace MAQUETTE
