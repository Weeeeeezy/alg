// vim:ts=2:et
//===========================================================================//
//                         "Venues/TT/Configs_FIX.hpp":                      //
//               Server and Account Configs for TT FIX Connectors            //
//===========================================================================//
#pragma once

#include "Basis/BaseTypes.hpp"
#include "Connectors/FIX/Configs.hpp"
#include "Venues/TT/SecDefs.h"
#include <utxx/config.h>
#include <utxx/error.hpp>
#include <utxx/enumv.hpp>
#include <boost/algorithm/string.hpp>
#include <string>
#include <map>
#include <vector>

namespace MAQUETTE
{
  namespace TT
  {
    //=======================================================================//
    // TT FIX Environments:                                                  //
    //=======================================================================//
    // XXX: Currently "Prod" only, in NY6, LD4 and KVH (Tokyo), with Internet
    // access or CoLocated access:
    //
    UTXX_ENUM(
    FIXEnvT, int,
      // Access via Internet:
      // Chicago
      Prod_CH_INet_MKT,
      Prod_CH_INet_ORD,

      // New York
      Prod_NY_INet_MKT,
      Prod_NY_INet_ORD,

      // Aurora
      Prod_AU_INet_MKT,
      Prod_AU_INet_ORD,

      // Test
      // Chicago
      Test_CH_INet_MKT,
      Test_CH_INet_ORD,

      // New York
      Test_NY_INet_MKT,
      Test_NY_INet_ORD,

      // Aurora
      Test_AU_INet_MKT,
      Test_AU_INet_ORD
    );

    // FIX Session Configs:
    extern std::map<FIXEnvT::type, FIXSessionConfig const> const
      FIXSessionConfigs;

    // FIX Account Configs:
    extern std::map<std::string,   FIXAccountConfig const> const
      FIXAccountConfigs;
  }
  // End namespace TT

  //=========================================================================//
  // "GetFIXSessionConfig<TT>":                                              //
  //=========================================================================//
  // For TT, "AccountKey" must be of the following format:
  //
  // {AnyPrefix}-{Location}-{MKT|ORD}-FIX-TT-{Prod|Test}
  //
  // where        Location ={CH|NY|AU}_{INet|CoLo}
  //
  // We extract both "FIXSessionConfig" and "FIXAccountConfig" using such a key:
  //
  template<>
  inline FIXSessionConfig const& GetFIXSessionConfig<FIX::DialectT::TT>
    (std::string const& a_acct_key)
  {
    // Parse the AccountKey:
    std::vector<std::string> keyParts;
    boost::split  (keyParts, a_acct_key, boost::is_any_of("-"));
    size_t keyN  = keyParts.size();

    if (utxx::unlikely
       (keyN  != 6                                         ||
       (keyParts[2] != "MKT"  && keyParts[2] != "ORD")     ||
        keyParts[3] != "FIX"  || keyParts[4] != "TT" ||
       (keyParts[5] != "Prod" && keyParts[5] != "Test")))
      throw utxx::badarg_error("GetFIXSessionConfig: Invalid Key=", a_acct_key);

    // Construct the "TT::FIXEnvT" literal from "keyParts", as
    // {Prod|Test}_{Location}_{MKT|ORD}:
    // [5]         [1]        [2]
    std::string envLit = keyParts[5] + "_" + keyParts[1] + "_" + keyParts[2];
    try
    {
      TT::FIXEnvT env = TT::FIXEnvT::from_string(envLit);
      return TT::FIXSessionConfigs.at(env);
    }
    catch (...)
    {
      throw utxx::badarg_error
            ("GetFIXSessionConfig<TT>: Invalid AccountKey=", a_acct_key,
             ", FIXEnvT=", envLit);
    }
    // This point is not reached!
    __builtin_unreachable();
  }

  //=========================================================================//
  // "GetFIXAccountConfig<TT>":                                              //
  //=========================================================================//
  template<>
  inline FIXAccountConfig const& GetFIXAccountConfig<FIX::DialectT::TT>
    (std::string const& a_acct_key)
  {
    // Parse the AccountKey: the Location part will be dropped, as all Accounts
    // are valid across  all Locations:
    std::vector<std::string> keyParts;
    boost::split  (keyParts, a_acct_key, boost::is_any_of("-"));
    size_t keyN  = keyParts.size();

    if (utxx::unlikely
       (keyN  != 6                                         ||
       (keyParts[2] != "MKT"  && keyParts[2] != "ORD")     ||
        keyParts[3] != "FIX"  || keyParts[4] != "TT" ||
       (keyParts[5] != "Prod" && keyParts[5] != "Test")))
      throw utxx::badarg_error("GetFIXSessionConfig: Invalid Key=", a_acct_key);

    // Uniform (across all Locations) AccountKey:
    std::string uniAC =
      keyParts[0] + "-" + keyParts[2] + "-" + keyParts[3] + "-" +
      keyParts[4] + "-" + keyParts[5];

    try   { return TT::FIXAccountConfigs.at(uniAC); }
    catch (...)
    {
      throw utxx::badarg_error
            ("GetFIXAccountConfig<TT>: Invalid AccountKey=", a_acct_key,
             ", UniAC=", uniAC);
    }
    // This point is not reached!
    __builtin_unreachable();
  }

  //=========================================================================//
  // "GetFIXSrcSecDefs<TT>":                                               //
  //=========================================================================//
  template<>
  inline std::vector<SecDefS> const&
  GetFIXSrcSecDefs<FIX::DialectT::TT>(MQTEnvT UNUSED_PARAM(a_cenv))
    { return TT::SecDefs; }

} // End namespace MAQUETTE
