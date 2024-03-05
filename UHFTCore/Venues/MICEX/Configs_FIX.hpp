// vim:ts=2:et
//===========================================================================//
//                        "Venues/MICEX/Configs_FIX.hpp":                    //
//              Server and Account Configs for MICEX FIX Connector           //
//===========================================================================//
#pragma once

#include "Basis/BaseTypes.hpp"
#include "Connectors/FIX/Configs.hpp"
#include "Venues/MICEX/SecDefs.h"
#include <utxx/config.h>
#include <utxx/error.hpp>
#include <utxx/enumv.hpp>
#include <boost/algorithm/string.hpp>
#include <string>
#include <map>
#include <vector>
#include <utility>

namespace MAQUETTE
{
  namespace MICEX
  {
    //=======================================================================//
    // MICEX FIX Environments:                                               //
    //=======================================================================//
    // Each of these Envs exists for both EQ and FX AssetClasses:
    //
    UTXX_ENUM(
    FIXEnvT, int,
      Prod1,         // Production 1  (NOT RECOMMENDED)
      Prod20,        // Production 2x (PREFERRED)
      Prod21,        //
      Prod30,        // Production 3x (XXX: Currently for FX only)
      Prod31,        //
      UATL1,         // UAT  Local (co-location)
      UATL2,         //
      TestL1,        // Test Local (co-location)
      TestL2,        //
      UATI,          // UAT  over Internet
      TestI          // Test over Internet
    );

    //=======================================================================//
    // FIX Session and Account Configs for MICEX:                            //
    //=======================================================================//
    extern std::map<std::pair
                    <FIXEnvT::type, AssetClassT::type>, FIXSessionConfig const>
    const FIXSessionConfigs;

    // FIXME: Currently, "FIXAccountConfig"s are also compiled in, which is a
    // VERY BAD idea -- will eventually be replaced by an Erlang-based Config
    // Server:
    extern std::map<std::string, FIXAccountConfig const>
    const FIXAccountConfigs;
  }
  // End namespace MICEX

  //=========================================================================//
  // "GetFIXSessionConfig<MICEX>":                                           //
  //=========================================================================//
  // For MICEX, "acct_key" must be of the following format:
  //
  // {AnyPrefix}-FIX-MICEX-{FX|EQ}-{Env}
  //
  // where {Env} is "FIXEnvT" literal as above; extract both "FIXSessionConfig"
  // and "FIXAccountConfig" using such a key:
  //
  template<>
  inline FIXSessionConfig const& GetFIXSessionConfig<FIX::DialectT::MICEX>
    (std::string const& a_acct_key)
  {
    std::vector<std::string> keyParts;
    boost::split  (keyParts, a_acct_key, boost::is_any_of("-"));
    size_t keyN = keyParts.size();

    if (utxx::unlikely
       (keyN < 5 || keyParts[keyN-4] != "FIX" || keyParts[keyN-3] != "MICEX"))
      throw utxx::badarg_error
            ("GetFIXSessionConfig<MICEX>: Invalid AccountKey=", a_acct_key);

    // If parsing was OK:
    std::string acStr  = keyParts[keyN-2];    // Asset Class
    std::string envStr = keyParts[keyN-1];    // Environment
    try
    {
      MICEX::AssetClassT ac  = MICEX::AssetClassT::from_string(acStr);
      MICEX::FIXEnvT     env = MICEX::FIXEnvT    ::from_string(envStr);

      return MICEX::FIXSessionConfigs.at(std::make_pair(env, ac));
    }
    catch (std::exception const& exc)
    {
      throw utxx::badarg_error
            ("GetFIXSessionConfig<MICEX>: Invalid AccountKey=", a_acct_key,
             ": ", exc.what());
    }
  }

  //=========================================================================//
  // "GetFIXAccountConfig<MICEX>":                                           //
  //=========================================================================//
  template<>
  inline FIXAccountConfig const& GetFIXAccountConfig<FIX::DialectT::MICEX>
    (std::string const& a_acct_key)
  {
    try   { return MICEX::FIXAccountConfigs.at(a_acct_key); }
    catch (...)
    {
      throw utxx::badarg_error
            ("GetFIXAccountConfig<MICEX>: Invalid AccountKey=", a_acct_key);
    }
  }

  //=========================================================================//
  // "GetFIXSrcSecDefs":                                                     //
  //=========================================================================//
  // XXX: Return a combined list of FX and EQ instrs (not NOT including non-tra-
  // dable Baskets):
  template<>
  inline std::vector<SecDefS> const&
    GetFIXSrcSecDefs<FIX::DialectT::MICEX>(MQTEnvT UNUSED_PARAM(a_cenv))
    { return MICEX::GetSecDefs_All(); }

} // End namespace MAQUETTE
