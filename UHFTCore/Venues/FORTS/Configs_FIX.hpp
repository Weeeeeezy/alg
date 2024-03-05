// vim:ts=2:et
//===========================================================================//
//                        "Venues/FORTS/Configs_FIX.hpp":                    //
//               Session and Account Configs for FORTS FIX Connector         //
//===========================================================================//
#pragma once

#include "Basis/BaseTypes.hpp"
#include "Connectors/FIX/Configs.hpp"
#include "Venues/FORTS/OMCEnvs.h"
#include "Venues/FORTS/SecDefs.h"
#include <utxx/config.h>
#include <utxx/error.hpp>
#include <boost/algorithm/string.hpp>
#include <string>
#include <map>
#include <vector>

namespace MAQUETTE
{
  namespace FORTS
  {
    //=======================================================================//
    // FIX Session and Account Configs for FORTS:                            //
    //=======================================================================//
    // For FORTS, same FIX account is valid for all Mkt Segments (Fut, Opt etc):
    //
    extern std::map<OMCEnvT::type, FIXSessionConfig const> const
      FIXSessionConfigs;

    // FIXME: Currently, "FIXAccountConfig"s are also compiled in, which is a
    // VERY BAD idea -- will eventually be replaced by an Erlang-based Config
    // Server:
    extern std::map<std::string,   FIXAccountConfig const> const
      FIXAccountConfigs;
  }
  // End namespace FORTS

  //=========================================================================//
  // "GetFIXSessionConfig<FORTS>":                                           //
  //=========================================================================//
  // Here "acct_key" must be of the following format:
  //
  // {AnyPrefix}-FIX-FORTS-{Env}
  //
  // where {Env} is "OMCEnvT" literal as above; extract both "FIXSessionConfig"
  // and "FIXAccountConfig" using such a key:
  //
  template<>
  inline FIXSessionConfig const& GetFIXSessionConfig<FIX::DialectT::FORTS>
    (std::string const& a_acct_key)
  {
    std::vector<std::string> keyParts;
    boost::split  (keyParts, a_acct_key, boost::is_any_of("-"));
    size_t keyN = keyParts.size();

    if (utxx::unlikely
       (keyN < 4 || keyParts[keyN-3] != "FIX" || keyParts[keyN-2] != "FORTS"))
      throw utxx::badarg_error
            ("GetFIXSessionConfig<FORTS>: Invalid AccountKey=", a_acct_key);

    std::string envStr = keyParts[keyN-1];    // Environment
    try
    {
      return FORTS::FIXSessionConfigs.at(FORTS::OMCEnvT::from_string(envStr));
    }
    catch (std::exception const& exc)
    {
      throw utxx::badarg_error
            ("GetFIXSessionConfig<FORTS>: Invalid AccountKey=", a_acct_key,
             ": ", exc.what());
    }
  }

  //=========================================================================//
  // "GetFIXAccountConfig<FORTS>":                                           //
  //=========================================================================//
  template<>
  inline FIXAccountConfig const& GetFIXAccountConfig<FIX::DialectT::FORTS>
    (std::string const& a_acct_key)
  {
    try   { return FORTS::FIXAccountConfigs.at(a_acct_key); }
    catch (...)
    {
      throw utxx::badarg_error
            ("GetFIXAccountConfig<FORTS>: Invalid AccountKey=", a_acct_key);
    }
  }

  //=========================================================================//
  // "GetFIXSrcSecDefs<FORTS>":                                              //
  //=========================================================================//
  // XXX: Return a combined list of FX and EQ instrs (not NOT including non-tra-
  // dable Indices):
  template<>
  inline std::vector<SecDefS> const&
    GetFIXSrcSecDefs<FIX::DialectT::FORTS>(MQTEnvT a_cenv)
    { return FORTS::GetSecDefs_All(a_cenv); }

} // End namespace MAQUETTE
