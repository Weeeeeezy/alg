// vim:ts=2:et
//===========================================================================//
//                        "Venues/FORTS/Configs_TWIME.hpp":                  //
//            Session and Account Configs for FORTS TWIME Connectors         //
//===========================================================================//
#pragma once

#include "Connectors/TWIME/Configs.h"
#include "Venues/FORTS/OMCEnvs.h"
#include "Venues/FORTS/SecDefs.h"
#include <utxx/error.hpp>
#include <boost/algorithm/string.hpp>
#include <map>
#include <utility>
#include <cstdlib>

namespace MAQUETTE
{
  // NB: TWIME is for FORTS only, so there is no need for a nested "FORTS" name-
  // space -- but we still provide it for the sake of uniformity (cf FIX):
  //
  namespace FORTS
  {
    //=======================================================================//
    // TWIME Session and Account Configs for FORTS:                          //
    //=======================================================================//
    // Same TWIME account is valid for all FORTS Mkt Segments (Fut, Opt, etc):
    //
    extern std::map<OMCEnvT::type, TWIMESessionConfig> const
      TWIMESessionConfigs;

    // FIXME: Currently, "TWIMEAccountConfig"s are also compiled in, which is a
    // VERY BAD idea  --  will eventually be replaced by an Erlang-based Config
    // Server:
    extern std::map<std::string, TWIMEAccountConfig> const TWIMEAccountConfigs;
  }
  // End namespace FORTS

  //=========================================================================//
  // "GetTWIMESessionConfig":                                                //
  //=========================================================================//
  // Here "acct_key" must be of the following format:
  //
  // {AnyPrefix}-TWIME-FORTS-{Env}
  //
  // where {Env} is an "OMCEnvT" literal as above; extract both "SessionConfig"
  // and "AccountConfig" using such a key:
  //
  inline TWIMESessionConfig const& GetTWIMESessionConfig
    (std::string const& a_acct_key)
  {
    std::vector<std::string> keyParts;
    boost::split  (keyParts, a_acct_key, boost::is_any_of("-"));
    size_t keyN = keyParts.size();

    if (utxx::unlikely
       (keyN < 4 || keyParts[keyN-3] != "TWIME" || keyParts[keyN-2] != "FORTS"))
      throw utxx::badarg_error
            ("GetTWIMESessionConfig: Invalid AccountKey=", a_acct_key);

    std::string envStr = keyParts[keyN-1];    // Environment
    try
    {
      return FORTS::TWIMESessionConfigs.at(FORTS::OMCEnvT::from_string(envStr));
    }
    catch (std::exception const& exc)
    {
      throw utxx::badarg_error
            ("GetTWIMESessionConfig: Invalid AccountKey=", a_acct_key, ": ",
             exc.what());
    }
  }

  //=========================================================================//
  // "GetTWIMEAccountConfig":                                                //
  //=========================================================================//
  inline TWIMEAccountConfig const& GetTWIMEAccountConfig
    (std::string const& a_acct_key)
  {
    try   { return FORTS::TWIMEAccountConfigs.at(a_acct_key); }
    catch (...)
    {
      throw utxx::badarg_error
            ("GetTWIMEAccountConfig: Invalid AccountKey=", a_acct_key);
    }
  }
} // End namespace MAQUETTE
