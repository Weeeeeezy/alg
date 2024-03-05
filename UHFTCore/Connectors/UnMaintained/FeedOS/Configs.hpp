// vim:ts=2:et
//===========================================================================//
//                      "Connectors/FeedOS/Configs.hpp":                     //
//    Configs for QuantHouse / S&P Capital IQ (aka FeedOS) MktData Feeds     //
//===========================================================================//
#pragma  once

#include <utxx/error.hpp>
#include <string>
#include <map>

namespace MAQUETTE
{
namespace FeedOS
{
  //=========================================================================//
  // "Config" Struct:                                                        //
  //=========================================================================//
  struct Config
  {
    std::string   m_ServerAddr;   // Server IP or Name
    int           m_ServerPort;   // Server Port
    std::string   m_UserName;
    std::string   m_Passwd;
  };

  //=========================================================================//
  // Actual Configs:                                                         //
  //=========================================================================//
  // Map:  Key => Config
  // where Key is like "PREFIX-FeedOS":
  //
  extern std::map<std::string, Config> const Configs;

  //=========================================================================//
  // "GetConfig":                                                            //
  //=========================================================================//
  inline Config const& GetConfig(std::string const& a_key)
  {
    // XXX: At the moment, we do not even check that "a_key" ends with "-FeedOS"
    // -- the key validity is ensured by "at" anyway:
    try
      { return Configs.at(a_key); }
    catch (...)
      { throw utxx::badarg_error("FeedOS::GetConfig: Invalid Key=", a_key); }
  }
} // End namespace FeedOS
} // End namespace MAQUETTE
