// vim:ts=2:et
//===========================================================================//
//                    "Venues/Cumberland/Configs_FIX.hpp":                   //
//          Server and Account Configs for Cumberland FIX Connectors         //
//===========================================================================//
#pragma once

#include "Basis/BaseTypes.hpp"
#include "Connectors/FIX/Configs.hpp"
#include "Venues/Cumberland/SecDefs.h"
#include <utxx/config.h>
#include <utxx/error.hpp>
#include <utxx/enumv.hpp>
#include <boost/algorithm/string.hpp>
#include <string>
#include <map>
#include <vector>

namespace MAQUETTE
{
  namespace Cumberland
  {
    //=======================================================================//
    // Cumberland FIX Environments:                                          //
    //=======================================================================//
    // XXX: Currently "Prod" only, in NY6, LD4 and KVH (Tokyo), with Internet
    // access or CoLocated access:
    //
    UTXX_ENUM(
    FIXEnvT, int,
      // Access via Internet:
      Test_INet,
      Prod_INet
    );

    // FIX Session Configs:
    extern std::map<FIXEnvT::type, FIXSessionConfig const> const
      FIXSessionConfigs;

    // FIX Account Configs:
    extern std::map<std::string,   FIXAccountConfig const> const
      FIXAccountConfigs;
  }
  // End namespace Cumberland

  //=========================================================================//
  // "GetFIXSessionConfig<Cumberland>":                                      //
  //=========================================================================//
  // For Cumberland, "AccountKey" must be of the following format:
  //
  // {AnyPrefix}-{Location}-FIX-Cumberland-{Prod|Test}
  //
  // where        Location = {INet|CoLo}
  //
  // We extract both "FIXSessionConfig" and "FIXAccountConfig" using such a key
  // (currently, only "Prod" is implemented):
  //
  template<>
  inline FIXSessionConfig const& GetFIXSessionConfig<FIX::DialectT::Cumberland>
    (std::string const& a_acct_key)
  {
    // Parse the AccountKey:
    std::vector<std::string> keyParts;
    boost::split  (keyParts, a_acct_key, boost::is_any_of("-"));
    size_t keyN  = keyParts.size();

    if (utxx::unlikely
       (keyN  != 5                                           ||
        keyParts[2] != "FIX"  || keyParts[3] != "Cumberland" ||
       (keyParts[4] != "Prod" && keyParts[4] != "Test")))
      throw utxx::badarg_error("GetFIXSessionConfig: Invalid Key=", a_acct_key);

    // Construct the "Cumberland::FIXEnvT" literal from "keyParts", as
    // {Prod|Test}_{Location}:
    std::string envLit = keyParts[4] + "_" + keyParts[1];
    try
    {
      Cumberland::FIXEnvT env = Cumberland::FIXEnvT::from_string(envLit);
      return Cumberland::FIXSessionConfigs.at(env);
    }
    catch (...)
    {
      throw utxx::badarg_error
            ("GetFIXSessionConfig<Cumberland>: Invalid AccountKey=", a_acct_key,
             ", FIXEnvT=", envLit);
    }
    // This point is not reached!
    __builtin_unreachable();
  }

  //=========================================================================//
  // "GetFIXAccountConfig<Cumberland>":                                      //
  //=========================================================================//
  template<>
  inline FIXAccountConfig const& GetFIXAccountConfig<FIX::DialectT::Cumberland>
    (std::string const& a_acct_key)
  {
    // Parse the AccountKey: the Location part will be dropped, as all Accounts
    // are valid across  all Locations:
    std::vector<std::string> keyParts;
    boost::split  (keyParts, a_acct_key, boost::is_any_of("-"));
    size_t keyN  = keyParts.size();

    if (utxx::unlikely
       (keyN  != 5                                           ||
        keyParts[2] != "FIX"  || keyParts[3] != "Cumberland" ||
       (keyParts[4] != "Prod" && keyParts[4] != "Test")))
      throw utxx::badarg_error("GetFIXSessionConfig: Invalid Key='", a_acct_key, "'");

    // Uniform (across all Locations) AccountKey:
    std::string uniAC =
      keyParts[0] + "-" + keyParts[2] + "-" + keyParts[3] + "-" + keyParts[4];

    try   { return Cumberland::FIXAccountConfigs.at(uniAC); }
    catch (...)
    {
      throw utxx::badarg_error
            ("GetFIXAccountConfig<Cumberland>: Invalid AccountKey=", a_acct_key,
             ", UniAC=", uniAC);
    }
    // This point is not reached!
    __builtin_unreachable();
  }

  //=========================================================================//
  // "GetFIXSrcSecDefs<Cumberland>":                                         //
  //=========================================================================//
  template<>
  inline std::vector<SecDefS> const&
  GetFIXSrcSecDefs<FIX::DialectT::Cumberland>(MQTEnvT UNUSED_PARAM(a_cenv))
    { return Cumberland::SecDefs; }

} // End namespace MAQUETTE
