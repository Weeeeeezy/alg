// vim:ts=2:et
//===========================================================================//
//                      "Venues/AlfaECN/Configs_FIX.hpp":                    //
//              Server and Account Configs for AlfaECN FIX Connector         //
//===========================================================================//
#pragma once

#include "Basis/BaseTypes.hpp"
#include "Connectors/FIX/Configs.hpp"
#include "Venues/AlfaECN/SecDefs.h"
#include <utxx/config.h>
#include <utxx/error.hpp>
#include <utxx/enumv.hpp>
#include <boost/algorithm/string.hpp>
#include <string>
#include <map>
#include <vector>

namespace MAQUETTE
{
  namespace AlfaECN
  {
    //=======================================================================//
    // AlfaECN FIX Environments:                                             //
    //=======================================================================//
    // Each of these Envs exists for both EQ and FX AssetClasses:
    //
    UTXX_ENUM(
    FIXEnvT, int,
      ProdORD,
      ProdMKT,

      UAT_ORD,
      UAT_MKT,

      Test1ORD,
      Test1MKT,

      Test2ORD,
      Test2MKT
    );

    //=======================================================================//
    // FIX Session and Account Configs for AlfaECN:                          //
    //=======================================================================//
    extern std::map<FIXEnvT::type, FIXSessionConfig const> const
      FIXSessionConfigs;

    // FIXME: Currently, "FIXAccountConfig"s are also compiled in,  which is a
    // VERY BAD idea -- will eventually be replaced by an Erlang-based  Config
    // Server:
    extern std::map<std::string,   FIXAccountConfig const> const
      FIXAccountConfigs;
  }
  // End namespace AlfaECN

  //=========================================================================//
  // "GetFIXSessionConfig<AlfaECN>":                                         //
  //=========================================================================//
  // For AlfaECN, "acct_key" must be of the following format:
  //
  // {AnyPrefix}-FIX-AlfaECN-{Env}:
  //
  // where {Env} is "FIXEnvT" literal as above; extract both "FIXSessionConfig"
  // and "FIXAccountConfig" using such a key:
  //
  template<>
  inline FIXSessionConfig const& GetFIXSessionConfig<FIX::DialectT::AlfaECN>
    (std::string const& a_acct_key)
  {
    std::vector<std::string> keyParts;
    boost::split  (keyParts, a_acct_key, boost::is_any_of("-"));
    size_t keyN  = keyParts.size();

    if (utxx::unlikely
       (keyN < 4 || keyParts[keyN-3] != "FIX" || keyParts[keyN-2] != "AlfaECN"))
      throw utxx::badarg_error
            ("GetFIXSessionConfig<AlfaECN>: Invalid AccountKey=", a_acct_key);

    std::string envStr = keyParts[keyN-1];    // Environment
    try
    {
      AlfaECN::FIXEnvT env = AlfaECN::FIXEnvT::from_string(envStr);
      return AlfaECN::FIXSessionConfigs.at(env);
    }
    catch (std::exception const& exc)
    {
      throw utxx::badarg_error
            ("GetFIXSessionConfig<AlfaECN>: Invalid AccountKey=", a_acct_key,
             ": ", exc.what());
    }
  }

  //=========================================================================//
  // "GetFIXAccountConfig<AlfaECN>":                                         //
  //=========================================================================//
  template<>
  inline FIXAccountConfig const& GetFIXAccountConfig<FIX::DialectT::AlfaECN>
    (std::string const& a_acct_key)
  {
    try   { return AlfaECN::FIXAccountConfigs.at(a_acct_key); }
    catch (...)
    {
      throw utxx::badarg_error
            ("GetFIXAccountConfig<ALfaECN>: Invalid AccountKey=", a_acct_key);
    }
  }

  //=========================================================================//
  // "GetFIXSrcSecDefs":                                                     //
  //=========================================================================//
  template<>
  inline std::vector<SecDefS> const&
    GetFIXSrcSecDefs<FIX::DialectT::AlfaECN>(MQTEnvT UNUSED_PARAM(a_cenv))
    { return AlfaECN::SecDefs; }

} // End namespace MAQUETTE
