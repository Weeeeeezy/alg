// vim:ts=2:et
//===========================================================================//
//                       "Venues/AlfaFIX2/Configs_FIX.hpp":                  //
//               Session and Account Configs for AlfaFIX2 Connector          //
//===========================================================================//
#pragma once

#include "Basis/BaseTypes.hpp"
#include "Connectors/FIX/Configs.hpp"
#include "Venues/AlfaFIX2/SecDefs.h"
#include <utxx/config.h>
#include <utxx/error.hpp>
#include <utxx/enumv.hpp>
#include <boost/algorithm/string.hpp>
#include <string>
#include <map>
#include <vector>

namespace MAQUETTE
{
  namespace AlfaFIX2
  {
    //=======================================================================//
    // AlfaFIX2 FIX Environments:                                            //
    //=======================================================================//
    // Each of these Envs exists for both EQ and FX AssetClasses:
    //
    UTXX_ENUM(
    FIXEnvT, int,
      ProdMKT1,  // Production -- MktData1
      ProdORD1,  // Production -- OrdMgmt1
      ProdMKT2,  // Production -- MktData2
      ProdORD2   // Production -- OrdMgmt2: XXX: Currently Non-Functional!
    );

    //=======================================================================//
    // Session and Account Configs for AlfaFIX2:                             //
    //=======================================================================//
    extern std::map<FIXEnvT::type, FIXSessionConfig const> const
      FIXSessionConfigs;

    // FIXME: Currently, "AccountConfig"s are also compiled in, which is a VERY
    // BAD idea -- will eventually be replaced by an Erlang-based Config Server:
    //
    extern std::map<std::string,   FIXAccountConfig const> const
      FIXAccountConfigs;
  }
  // End namespace AlfaFIX2

  //=========================================================================//
  // "GetFIXSessConfig<AlfaFIX2>":                                           //
  //=========================================================================//
  // For AlfaFIX2, "acct_key" must be of the following format:
  //
  // ALFA{N}-FIX-AlfaFIX2-{Env}:
  //
  // where {Env} is a "FIXEnvT" literal as above, but w/o the trailing {N}
  // (which is taken from the Prefix instead!);
  // extract both "FIXSessionConfig" and "FIXAccountConfig" using such a key:
  //
  template<>
  inline FIXSessionConfig const& GetFIXSessionConfig<FIX::DialectT::AlfaFIX2>
    (std::string const& a_acct_key)
  {
    std::vector<std::string> keyParts;
    boost::split  (keyParts, a_acct_key, boost::is_any_of("-"));
    size_t keyN  = keyParts.size();

    // XXX: Here the Prefix DOES matter -- we derive the Env Postfix from it!
    if (utxx::unlikely
       (keyN != 4 || keyParts[0].substr(0, 4) != "ALFA"  ||
                     keyParts[1]  != "FIX" || keyParts[2] != "AlfaFIX2"))
      throw utxx::badarg_error
            ("GetFIXSessionConfig<AlfaFIX2>: Invalid AccountKey=",
             a_acct_key);

    // Environment Literal, constructed by appending the EnvVer:
    std::string envVer   = keyParts[0].substr(4);
    std::string envLit   = keyParts[3] + envVer;
    try
    {
      AlfaFIX2::FIXEnvT env = AlfaFIX2::FIXEnvT::from_string(envLit);
      return   AlfaFIX2::FIXSessionConfigs.at(env);
    }
    catch (std::exception const& exc)
    {
      throw utxx::badarg_error
            ("GetFIXSessionConfig<AlfaFIX2>: Invalid AccountKey=", a_acct_key,
             ", Env=", envLit, ": ", exc.what());
    }
  }

  //=========================================================================//
  // "GetFIXAccountConfig<AlfaFIX2>":                                        //
  //=========================================================================//
  template<>
  inline FIXAccountConfig const& GetFIXAccountConfig<FIX::DialectT::AlfaFIX2>
    (std::string const& a_acct_key)
  {
    try   { return AlfaFIX2::FIXAccountConfigs.at(a_acct_key); }
    catch (...)
    {
      throw utxx::badarg_error
            ("GetFIXAccountConfig<AlfaFIX2>: Invalid AccountKey=", a_acct_key);
    }
  }

  //=========================================================================//
  // "GetFIXSrcSecDefs":                                                     //
  //=========================================================================//
  template<>
  inline std::vector<SecDefS> const&
    GetFIXSrcSecDefs<FIX::DialectT::AlfaFIX2>(MQTEnvT UNUSED_PARAM(a_cenv))
    { return AlfaFIX2::SecDefs; }

} // End namespace MAQUETTE
