// vim:ts=2:et
//===========================================================================//
//                        "Venues/LMAX/Configs_FIX.hpp":                     //
//              Server and Account Configs for LMAX FIX Connectors           //
//===========================================================================//
#pragma once

#include "Basis/BaseTypes.hpp"
#include "Connectors/FIX/Configs.hpp"
#include "Venues/LMAX/SecDefs.h"
#include <utxx/config.h>
#include <utxx/error.hpp>
#include <utxx/enumv.hpp>
#include <boost/algorithm/string.hpp>
#include <string>
#include <map>
#include <vector>

namespace MAQUETTE
{
  namespace LMAX
  {
    //=======================================================================//
    // LMAX FIX Environments:                                                //
    //=======================================================================//
    // XXX: Currently "Prod" only, in NY6, LD4 and KVH (Tokyo), with Internet
    // access or CoLocated access:
    //
    UTXX_ENUM(
    FIXEnvT, int,
      // Access via Internet:
      // No London market data over internet
      Prod_LD4_INet_ORD,

      Prod_NY4_INet_MKT,
      Prod_NY4_INet_ORD,

      Prod_TY3_INet_MKT,
      Prod_TY3_INet_ORD,

      // CoLocated (X-Connect) access:
      Prod_LD4_CoLo_MKT,
      Prod_LD4_CoLo_ORD,

      Prod_NY4_CoLo_MKT,
      Prod_NY4_CoLo_ORD,

      Prod_TY3_CoLo_MKT,
      Prod_TY3_CoLo_ORD,

      // Testing (all London)
      Test_LD4_INet_MKT,
      Test_LD4_INet_ORD,
      Test_LD4_CoLo_MKT,
      Test_LD4_CoLo_ORD
    );

    // FIX Session Configs:
    extern std::map<FIXEnvT::type, FIXSessionConfig const> const
      FIXSessionConfigs;

    // FIX Account Configs:
    extern std::map<std::string,   FIXAccountConfig const> const
      FIXAccountConfigs;
  }
  // End namespace LMAX

  //=========================================================================//
  // "GetFIXSessionConfig<LMAX>":                                            //
  //=========================================================================//
  // For LMAX, "AccountKey" must be of the following format:
  //
  // {AnyPrefix}-{Location}-{MKT|ORD}-FIX-LMAX-{Prod|Test}
  //
  // where        Location ={NY4|LD4|TY3}_{INet|CoLo}
  //
  // We extract both "FIXSessionConfig" and "FIXAccountConfig" using such a key:
  //
  template<>
  inline FIXSessionConfig const& GetFIXSessionConfig<FIX::DialectT::LMAX>
    (std::string const& a_acct_key)
  {
    // Parse the AccountKey:
    std::vector<std::string> keyParts;
    boost::split  (keyParts, a_acct_key, boost::is_any_of("-"));
    size_t keyN  = keyParts.size();

    if (utxx::unlikely
       (keyN  != 6                                         ||
       (keyParts[2] != "MKT"  && keyParts[2] != "ORD")     ||
        keyParts[3] != "FIX"  || keyParts[4] != "LMAX" ||
       (keyParts[5] != "Prod" && keyParts[5] != "Test")))
      throw utxx::badarg_error("GetFIXSessionConfig: Invalid Key=", a_acct_key);

    // Construct the "LMAX::FIXEnvT" literal from "keyParts", as
    // {Prod|Test}_{Location}_{MKT|ORD}:
    // [5]         [1]        [2]
    std::string envLit = keyParts[5] + "_" + keyParts[1] + "_" + keyParts[2];
    try
    {
      LMAX::FIXEnvT env = LMAX::FIXEnvT::from_string(envLit);
      return LMAX::FIXSessionConfigs.at(env);
    }
    catch (...)
    {
      throw utxx::badarg_error
            ("GetFIXSessionConfig<LMAX>: Invalid AccountKey=", a_acct_key,
             ", FIXEnvT=", envLit);
    }
    // This point is not reached!
    __builtin_unreachable();
  }

  //=========================================================================//
  // "GetFIXAccountConfig<LMAX>":                                            //
  //=========================================================================//
  template<>
  inline FIXAccountConfig const& GetFIXAccountConfig<FIX::DialectT::LMAX>
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
        keyParts[3] != "FIX"  || keyParts[4] != "LMAX" ||
       (keyParts[5] != "Prod" && keyParts[5] != "Test")))
      throw utxx::badarg_error
            ("GetFIXSessionConfig: Invalid AccountKey=", a_acct_key);

    // Uniform (across all Locations) AccountKey:
    std::string uniAC =
      keyParts[0] + "-" + keyParts[2] + "-" + keyParts[3] + "-" +
      keyParts[4] + "-" + keyParts[5];

    try   { return LMAX::FIXAccountConfigs.at(uniAC); }
    catch (...)
    {
      throw utxx::badarg_error
            ("GetFIXAccountConfig<LMAX>: Invalid AccountKey=", a_acct_key,
             ", UniAC=", uniAC);
    }
    // This point is not reached!
    __builtin_unreachable();
  }

  //=========================================================================//
  // "GetFIXSrcSecDefs<LMAX>":                                               //
  //=========================================================================//
  template<>
  inline std::vector<SecDefS> const&
  GetFIXSrcSecDefs<FIX::DialectT::LMAX>(MQTEnvT UNUSED_PARAM(a_cenv))
    { return LMAX::SecDefs; }

} // End namespace MAQUETTE
