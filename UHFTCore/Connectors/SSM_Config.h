// vim:ts=2:et
//===========================================================================//
//                         "Connectors/SSM_Config.h":                        //
//                     Simple Configs for "SSM_Channel"s                     //
//===========================================================================//
#pragma once
#include <string>

namespace MAQUETTE
{
  //=========================================================================//
  // "SSM_Config" Struct:                                                    //
  //=========================================================================//
  struct SSM_Config
  {
    //-----------------------------------------------------------------------//
    // Data Flds:                                                            //
    //-----------------------------------------------------------------------//
    std::string   m_groupIP;   // MultiCast Group IP
    std::string   m_sourceIP;  // SSM Source IP;  use "" for ordinary MCast
    std::string   m_localIP;   // Local IP   to bind to (may  be empty)
    int           m_localPort; // Local port (MUST be valid 

    //----------------------------------------------------------------------//
    // Non-Default Ctor: Normally, LocalIP is not given at this point:      //
    //----------------------------------------------------------------------//
    SSM_Config
    (
      std::string const& a_group_ip,
      std::string const& a_source_ip,
      int                a_local_port
    )
    : m_groupIP   (a_group_ip),
      m_sourceIP  (a_source_ip),
      m_localIP   (""),
      m_localPort (a_local_port)
    {}
  };
} // End namespace MAQUETTE
