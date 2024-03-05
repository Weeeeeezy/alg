// vim:ts=2:et
//===========================================================================//
//                    "Venues/HotSpotFX/Configs_ITCH.hpp":                   //
//===========================================================================//
#pragma once
#include "Connectors/ITCH/Configs.h"
#include "Venues/HotSpotFX/SecDefs.h"
#include <utxx/error.hpp>
#include <boost/algorithm/string.hpp>
#include <vector>
#include <map>

namespace MAQUETTE
{
  //=========================================================================//
  // Extended TCP ITCH Config for HotSpotFX:                                 //
  //=========================================================================//
  struct TCP_ITCH_Config_HotSpotFX: public TCP_ITCH_Config
  {
    // Data Fld(s):
    bool m_WithExtras;   // With MinQtys and LotSzs

    // Non-Default Ctor:
    TCP_ITCH_Config_HotSpotFX
    (
      std::string     a_server_ip,
      int             a_server_port,
      std::string     a_user_name,
      std::string     a_passwd,
      int             a_heart_beat_sec,
      int             a_logon_timeout_sec,
      int             a_logoff_timeout_sec,
      int             a_reconn_sec,
      bool            a_with_extras
    )
    : TCP_ITCH_Config
      (
        a_server_ip,      a_server_port,       a_user_name,  a_passwd,
        a_heart_beat_sec, a_logon_timeout_sec, a_logoff_timeout_sec,
        a_reconn_sec
      ),
      m_WithExtras (a_with_extras)
    {}
  };

  namespace HotSpotFX
  {
    //=======================================================================//
    // "TCP_ITCH_Config"s for HotSpotFX_{Gen,Link}:                          //
    //=======================================================================//
    extern std::map<std::string, TCP_ITCH_Config_HotSpotFX> const
           TCP_ITCH_Configs_Gen;

    extern std::map<std::string, TCP_ITCH_Config_HotSpotFX> const
           TCP_ITCH_Configs_Link;
  }

  //=========================================================================//
  // "Get_TCP_ITCH_Config_HotSpotFX":                                        //
  //=========================================================================//
  // Expected format of the Key:
  //
  // {AnyPrefix}-{EnvDetail}-TCP_ITCH-HotSpotFX-{Prod|Test}:
  // where
  // {EnvDetail} is compatible with that of HotSpotFX FIX:
  //
  template<ITCH::DialectT::type D>
  inline TCP_ITCH_Config_HotSpotFX const& Get_TCP_ITCH_Config_HotSpotFX
    (std::string const& a_key)
  {
    // Here "D" could only be "HotSpotFX_{Gen,Link}":
    static_assert(D == ITCH::DialectT::HotSpotFX_Gen ||
                  D == ITCH::DialectT::HotSpotFX_Link,
                  "TCP_ITCH_Config_HotSpotFX: Dialect must be "
                  "HotSpotFX_{Gen,Link}");

    // Parse the Key:
    std::vector<std::string> keyParts;
    boost::split  (keyParts, a_key, boost::is_any_of("-"));
    size_t keyN = keyParts.size();

    // The last fld could only be "Prod" or "Test":
    if (utxx::unlikely
       (keyN < 5         ||
        keyParts[keyN-3] != "TCP_ITCH" || keyParts[keyN-2] != "HotSpotFX" ||
       (keyParts[keyN-1] != "Prod"     && keyParts[keyN-1] != "Test")))
      throw utxx::badarg_error
            ("Get_TCP_ITCH_Config<",   ITCH::DialectT::to_string(D),
             ">: Invalid Key=",        a_key);

    // If Key parsing was OK:
    try
    { return
        (D == ITCH::DialectT::HotSpotFX_Gen)
        ? HotSpotFX::TCP_ITCH_Configs_Gen .at(a_key)
        : HotSpotFX::TCP_ITCH_Configs_Link.at(a_key);
    }
    catch (...)
    {
      throw utxx::badarg_error
            ("Get_TCP_ITCH_Config<",   ITCH::DialectT::to_string(D),
             ">: Invalid Key=",        a_key);
    }
    // This point is not reached!
  }

  //=========================================================================//
  // "Get_TCP_ITCH_Config<HotSpotFX_{Gen,Link}":                             //
  //=========================================================================//
  // NB: These functions actually return "TCP_ITCH_Config_HotSpotFX", not just
  // "TCP_ITCH_Config"!
  //
  template<>
  inline TCP_ITCH_Config const& Get_TCP_ITCH_Config
    <ITCH::DialectT::HotSpotFX_Gen> (std::string const& a_key)
  {
    return Get_TCP_ITCH_Config_HotSpotFX<ITCH::DialectT::HotSpotFX_Gen> (a_key);
  }

  template<>
  inline TCP_ITCH_Config const& Get_TCP_ITCH_Config
    <ITCH::DialectT::HotSpotFX_Link>(std::string const& a_key)
  {
    return Get_TCP_ITCH_Config_HotSpotFX<ITCH::DialectT::HotSpotFX_Link>(a_key);
  }

  //=========================================================================//
  // Obtaining the SecDefs and their Properties for TCP ITCH:                //
  //=========================================================================//
  template<>
  inline std::vector<SecDefS> const& Get_TCP_ITCH_SrcSecDefs
  <ITCH::DialectT::HotSpotFX_Gen> (MQTEnvT UNUSED_PARAM(a_cenv))
    { return HotSpotFX::SecDefs; }

  template<>
  inline std::vector<SecDefS> const& Get_TCP_ITCH_SrcSecDefs
  <ITCH::DialectT::HotSpotFX_Link>(MQTEnvT UNUSED_PARAM(a_cenv))
    { return HotSpotFX::SecDefs; }

  template<>
  inline bool Get_TCP_ITCH_UseTenorsInSecIDs<ITCH::DialectT::HotSpotFX_Gen> ()
    { return HotSpotFX::UseTenorsInSecIDs; }

  template<>
  inline bool Get_TCP_ITCH_UseTenorsInSecIDs<ITCH::DialectT::HotSpotFX_Link>()
    { return HotSpotFX::UseTenorsInSecIDs; }

} // End namespace MAQUETTE
