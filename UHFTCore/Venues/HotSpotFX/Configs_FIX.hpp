// vim:ts=2:et
//===========================================================================//
//                     "Venues/HotSpotFX/Configs_FIX.hpp":                   //
//           Server and Account Configs for HotSpotFX FIX Connectors         //
//===========================================================================//
#pragma once

#include "Basis/BaseTypes.hpp"
#include "Connectors/FIX/Configs.hpp"
#include "Venues/HotSpotFX/SecDefs.h"
#include <utxx/config.h>
#include <utxx/error.hpp>
#include <utxx/enumv.hpp>
#include <boost/algorithm/string.hpp>
#include <string>
#include <map>
#include <vector>

namespace MAQUETTE
{
  namespace HotSpotFX
  {
    //=======================================================================//
    // HotSpotFX_Gen FIX Environments:                                       //
    //=======================================================================//
    UTXX_ENUM(
    FIXEnvT_Gen, int,
      Test_Int_Gen_MKT,  // Test (MktData)   Generic over Internet
      Test_Int_Gen_ORD,  // Test (OrdMgmt)   Generic over Internet

      Prod_Int_Gen_MKT,  // Prod (MktData),  Generic over Internet

      Prod_NY5_INTL_ORD, // Prod (OrdMgmt),  INTL in NY5
      Prod_NY5_UBS_ORD   // Prod (OrdMgmt),  UBS  in NY5
    );

    // FIX Session Configs:
    extern std::map<FIXEnvT_Gen::type,  FIXSessionConfig const> const
           FIXSessionConfigs_Gen;

    // FIX Account Congigs:
    extern std::map<std::string,        FIXAccountConfig const> const
           FIXAccountConfigs_Gen;

    //=======================================================================//
    // HotSpotFX_Link FIX Environments:                                      //
    //=======================================================================//
    // XXX: Currently for CCM Alpha only:
    UTXX_ENUM(
    FIXEnvT_Link, int,
      Prod_Int_CCMA_MKT, // Prod (MktData) from CCMAlpha/UBS  over Internet
      Prod_Int_CCMA_ORD, // Prod (OrdMgmt) for  CCMAlpha/UBS  over Internet

      Prod_LD4_CCMA_MKT, // Prod (MktData) from CCMAlpha/UBS  in   LD4
      Prod_LD4_CCMA_ORD  // Prod (OrdMgmt) for  CCMAlpha/UBS  in   LD4
    );

    // FIX Session Configs:
    extern std::map<FIXEnvT_Link::type, FIXSessionConfig const> const
           FIXSessionConfigs_Link;

    // FIX Account Configs:
    extern std::map<std::string,        FIXAccountConfig const> const
           FIXAccountConfigs_Link;
  }
  // End namespace HotSpotFX

  //=========================================================================//
  // "GetFIXSessionConfig_HotSpotFX":                                        //
  //=========================================================================//
  // For HotSpotFX, "AccountKey" must be of the following format (compatible
  // with that of the TCP_ITCH connector):
  //
  // {AnyPrefix}-{EnvDetail}-FIX-HotSpotFX-{Prod|Test}
  //
  // Extract both "FIXSessionConfig" and "FIXAccountConfig" using such a key,
  // where "FIXEnvT" becomes   {EnvDetail}_{Prod|Test}:
  //
  template<FIX::DialectT::type D>
  inline FIXSessionConfig const& GetFIXSessionConfig_HotSpotFX
    (std::string const& a_acct_key)
  {
    // Here "D" could only be "HotSpot_{Gen,Link}":
    static_assert(D == FIX::DialectT::HotSpotFX_Gen ||
                  D == FIX::DialectT::HotSpotFX_Link,
                  "GetFIXSessionConfig_HotSpotFX: Dialect must be "
                  "HotSpotFX_{Gen,Link}");

    // Parse the Key:
    std::vector<std::string> keyParts;
    boost::split  (keyParts, a_acct_key, boost::is_any_of("-"));
    size_t keyN  = keyParts.size();

    // Construct the "HotSpotFX::FIXEnvT_Gen" literal from "keyParts" (XXX: how
    // could keyN be greater than 5?):
    if (utxx::unlikely
       (keyN < 5 ||
        keyParts[keyN-3] != "FIX"  || keyParts[keyN-2] != "HotSpotFX" ||
       (keyParts[keyN-1] != "Prod" && keyParts[keyN-1] != "Test")))
      throw utxx::badarg_error
            ("GetFIXSessionConfig<",  FIX::DialectT::to_string(D),
             ">: Invalid Key=",       a_acct_key);

    // If OK: Construct the Environment Literal:
    std::string envLit = keyParts[keyN-1] + "_" + keyParts[keyN-4];
    try
    {
      if (D == FIX::DialectT::HotSpotFX_Gen)
      {
        HotSpotFX::FIXEnvT_Gen  env =
        HotSpotFX::FIXEnvT_Gen::from_string(envLit);
        return HotSpotFX::FIXSessionConfigs_Gen.at (env);
      }
      else
      {
        HotSpotFX::FIXEnvT_Link env =
        HotSpotFX::FIXEnvT_Link::from_string(envLit);
        return HotSpotFX::FIXSessionConfigs_Link.at(env);
      }
    }
    catch (...)
    {
      throw utxx::badarg_error
            ("GetFIXSessionConfig<",  FIX::DialectT::to_string(D),
             ">: Invalid Key=",       a_acct_key);
    }
    // This point is not reached!
    __builtin_unreachable();
  }

  //=========================================================================//
  // "GetFIXSessionConfig<HotSpotFX_{Gen,Link}>":                            //
  //=========================================================================//
  template<>
  inline FIXSessionConfig const&
  GetFIXSessionConfig<FIX::DialectT::HotSpotFX_Gen>
    (std::string const& a_acct_key)
  {
    return GetFIXSessionConfig_HotSpotFX<FIX::DialectT::HotSpotFX_Gen>
           (a_acct_key);
  }

  template<>
  inline FIXSessionConfig const&
  GetFIXSessionConfig<FIX::DialectT::HotSpotFX_Link>
    (std::string const& a_acct_key)
  {
    return GetFIXSessionConfig_HotSpotFX<FIX::DialectT::HotSpotFX_Link>
           (a_acct_key);
  }

  //=========================================================================//
  // "GetFIXAccountConfig_HotSpotFX":                                        //
  //=========================================================================//
  template<FIX::DialectT::type D>
  inline FIXAccountConfig const& GetFIXAccountConfig_HotSpotFX
    (std::string const& a_acct_key)
  {
    static_assert(D == FIX::DialectT::HotSpotFX_Gen ||
                  D == FIX::DialectT::HotSpotFX_Link,
                  "GetFIXAccountConfig_HotSpotFX: Dialect must be "
                  "HotSpotFX_{Gen,Link}");
    try
    { return
        (D == FIX::DialectT::HotSpotFX_Gen)
        ? HotSpotFX::FIXAccountConfigs_Gen .at(a_acct_key)
        : HotSpotFX::FIXAccountConfigs_Link.at(a_acct_key);
    }
    catch (...)
    {
      throw utxx::badarg_error
            ("GetFIXAccountConfig<",   FIX::DialectT::to_string(D),
             ">: Invalid AccountKey=", a_acct_key);
    }
    // This point is not reached!
    __builtin_unreachable();
  }

  //=========================================================================//
  // "GetFIXAccountConfig<HotSpotFX_{Gen,Link}>":                            //
  //=========================================================================//
  template<>
  inline FIXAccountConfig const&
  GetFIXAccountConfig<FIX::DialectT::HotSpotFX_Gen>
    (std::string const& a_acct_key)
  {
    return GetFIXAccountConfig_HotSpotFX<FIX::DialectT::HotSpotFX_Gen>
           (a_acct_key);
  }

  template<>
  inline FIXAccountConfig const&
  GetFIXAccountConfig<FIX::DialectT::HotSpotFX_Link>
    (std::string const& a_acct_key)
  {
    return GetFIXAccountConfig_HotSpotFX<FIX::DialectT::HotSpotFX_Link>
           (a_acct_key);
  }

  //=========================================================================//
  // "GetFIXSrcSecDefs<HotSpotFX_{Gen,Link}>":                               //
  //=========================================================================//
  template<>
  inline std::vector<SecDefS> const&
  GetFIXSrcSecDefs<FIX::DialectT::HotSpotFX_Gen> (MQTEnvT UNUSED_PARAM(a_cenv))
    { return HotSpotFX::SecDefs; }

  template<>
  inline std::vector<SecDefS> const&
  GetFIXSrcSecDefs<FIX::DialectT::HotSpotFX_Link>(MQTEnvT UNUSED_PARAM(a_cenv))
    { return HotSpotFX::SecDefs; }

} // End namespace MAQUETTE
