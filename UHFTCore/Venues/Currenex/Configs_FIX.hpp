// vim:ts=2:et
//===========================================================================//
//                      "Venues/Currenex/Configs_FIX.hpp":                   //
//            Server and Account Configs for Currenex FIX Connectors         //
//===========================================================================//
#pragma once

#include "Basis/BaseTypes.hpp"
#include "Connectors/FIX/Configs.hpp"
#include "Venues/Currenex/SecDefs.h"
#include <utxx/config.h>
#include <utxx/error.hpp>
#include <utxx/enumv.hpp>
#include <boost/algorithm/string.hpp>
#include <string>
#include <map>
#include <vector>

namespace MAQUETTE
{
  namespace Currenex
  {
    //=======================================================================//
    // Currenex FIX Environments:                                            //
    //=======================================================================//
    // XXX: Currently "Prod" only, in NY6, LD4 and KVH (Tokyo), with Internet
    // access or CoLocated access:
    //
    UTXX_ENUM(
    FIXEnvT, int,
      // Access via Internet:
      Prod_NY6_INet_MKT,
      Prod_NY6_INet_ORD,

      Prod_LD4_INet_MKT,
      Prod_LD4_INet_ORD,

      Prod_KVH_INet_MKT,
      Prod_KVH_INet_ORD,

      // CoLocated (X-Connect) access: Currently NY6 only:
      Prod_NY6_CoLo_MKT,
      Prod_NY6_CoLo_ORD
    );

    // FIX Session Configs:
    extern std::map<FIXEnvT::type, FIXSessionConfig const> const
      FIXSessionConfigs;

    // FIX Account Configs:
    extern std::map<std::string,   FIXAccountConfig const> const
      FIXAccountConfigs;
  }
  // End namespace Currenex

  //=========================================================================//
  // "GetFIXSessionConfig<Currenex>":                                        //
  //=========================================================================//
  // For Currenex, "AccountKey" must be of the following format:
  //
  // {AnyPrefix}-{Location}-{MKT|ORD}-FIX-Currenex-{Prod|Test}
  //
  // where        Location ={NY6|LD4|KVH}_{INet|CoLo}
  //
  // We extract both "FIXSessionConfig" and "FIXAccountConfig" using such a key
  // (currently, only "Prod" is implemented):
  //
  template<>
  inline FIXSessionConfig const& GetFIXSessionConfig<FIX::DialectT::Currenex>
    (std::string const& a_acct_key)
  {
    // Parse the AccountKey:
    std::vector<std::string> keyParts;
    boost::split  (keyParts, a_acct_key, boost::is_any_of("-"));
    size_t keyN  = keyParts.size();

    if (utxx::unlikely
       (keyN  != 6                                         ||
       (keyParts[2] != "MKT"  && keyParts[2] != "ORD")     ||
        keyParts[3] != "FIX"  || keyParts[4] != "Currenex" ||
       (keyParts[5] != "Prod" && keyParts[5] != "Test")))
      throw utxx::badarg_error("GetFIXSessionConfig: Invalid Key=", a_acct_key);

    // Construct the "Currenex::FIXEnvT" literal from "keyParts", as
    // {Prod|Test}_{Location}_{MKT|ORD}:
    // [5]         [1]        [2]
    std::string envLit = keyParts[5] + "_" + keyParts[1] + "_" + keyParts[2];
    try
    {
      Currenex::FIXEnvT env = Currenex::FIXEnvT::from_string(envLit);
      return Currenex::FIXSessionConfigs.at (env);
    }
    catch (...)
    {
      throw utxx::badarg_error
            ("GetFIXSessionConfig<Currenex>: Invalid AccountKey=", a_acct_key,
             ", FIXEnvT=", envLit);
    }
    // This point is not reached!
    __builtin_unreachable();
  }

  //=========================================================================//
  // "GetFIXAccountConfig<Currenex>":                                        //
  //=========================================================================//
  template<>
  inline FIXAccountConfig const& GetFIXAccountConfig<FIX::DialectT::Currenex>
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
        keyParts[3] != "FIX"  || keyParts[4] != "Currenex" ||
       (keyParts[5] != "Prod" && keyParts[5] != "Test")))
      throw utxx::badarg_error("GetFIXSessionConfig: Invalid Key=", a_acct_key);

    // Uniform (across all Locations) AccountKey:
    std::string uniAC =
      keyParts[0] + "-" + keyParts[2] + "-" + keyParts[3] + "-" +
      keyParts[4] + "-" + keyParts[5];

    try   { return Currenex::FIXAccountConfigs.at(uniAC); }
    catch (...)
    {
      throw utxx::badarg_error
            ("GetFIXAccountConfig<Currenex>: Invalid AccountKey=", a_acct_key,
             ", UniAC=", uniAC);
    }
    // This point is not reached!
    __builtin_unreachable();
  }

  //=========================================================================//
  // "GetFIXSrcSecDefs<Currenex>":                                           //
  //=========================================================================//
  template<>
  inline std::vector<SecDefS> const&
  GetFIXSrcSecDefs<FIX::DialectT::Currenex>(MQTEnvT UNUSED_PARAM(a_cenv))
    { return Currenex::SecDefs; }

} // End namespace MAQUETTE
