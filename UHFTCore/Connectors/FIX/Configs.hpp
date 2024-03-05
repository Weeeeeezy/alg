// vim:ts=2:et
//===========================================================================//
//                         "Connectors/FIX/Configs.hpp":                     //
//            Session and Account Config Types for the FIX Connector         //
//===========================================================================//
#pragma once

#include "Basis/SecDefs.h"
#include "Protocols/FIX/Features.h"
#include <utxx/error.hpp>
#include <boost/container/static_vector.hpp>
#include <string>
#include <utility>

namespace MAQUETTE
{
  //=========================================================================//
  // "FIXAccountConfig" Struct:                                              //
  //=========================================================================//
  // Contains User Credentials and User-specific settings.
  // NB: Using  "std::string"s here and in "FIXSessionConfig" (below) would be
  // sub-optimal at run-time;  however, all such flds are copied into   "FIX::
  // SessData" fixed-size strings, so there is no performance impact:
  //
  struct FIXAccountConfig
  {
    std::string   m_OurCompID;      // 49
    std::string   m_OurSubID;       // 50
    std::string   m_UserName;       // 553
    std::string   m_Passwd;         // 554
    std::string   m_Account;        // 1
    std::string   m_ClientID;       // For FIX <= 4.2 this is tag 109, for FIX
                                    // >= 4.3 this is OnBehalfOfCompID (tag 115)
    std::string   m_PartyID;        // 448 (empty for direct Exchange members)
    char          m_PartyIDSrc;     // 447 ; only meaningful if PartyID is set
    int           m_PartyRole;      // 452 ; only meaningful if PartyID is set
    int           m_ThrottlingPeriodSec;
    int           m_MaxReqsPerPeriod;
  };

  //=========================================================================//
  // "FIXSessionConfig" Struct:                                              //
  //=========================================================================//
  // Contains Exchange connectivity params:
  // (*) "FIXSessionConfig"s and "FIXAccountConfig"s are in fact NOT independ-
  //     ent -- eg a Test Account cannot be used with a Prod Sess,   nor other
  //     way round (in most cases); that's why they are grouped together   and
  //     can both be obtained via a single Key (see  "GetFIX{Account,Session}-
  //     Config" below);
  // (*) Multiple "ServerIP"s  in "SessionConfig" is  an  optional reliability
  //     feature (currently available for MICEX only); a Session can be re-con-
  //     nected to any of those IPs preserving all "SeqNum"s;  XXX:  but there
  //     is currently only 1 ServerPort;
  // (*) The session may provide any combination of OrdMgmt and MktData capabi-
  //     lities, incl both or none (eg in case of STP):
  //
  struct FIXSessionConfig
  {
    std::string   m_ServerName;
    std::string   m_ServerIP;
    int           m_ServerPort;
    bool          m_UseTLS;
    std::string   m_ServerCompID;   // 56
    int           m_HeartBeatSec;
    int           m_LogOnTimeOutSec;
    int           m_LogOffTimeOutSec;
    int           m_ReConnectSec;
    int           m_MaxGapMSec;
    bool          m_IsOrdMgmt;      // This Session provides OrdMgmt
    bool          m_IsMktData;      // This Session provides MktData
  };

  //=========================================================================//
  // "GetFIX{Account,Session}Config":                                        //
  //=========================================================================//
  // Actual implementations are by template specialisation; "key" format depends
  // on the Dialect:
  //
  template<FIX::DialectT::type D>
  FIXAccountConfig const& GetFIXAccountConfig(std::string const& a_key);

  template<FIX::DialectT::type D>
  FIXSessionConfig const& GetFIXSessionConfig(std::string const& a_key);

  //=========================================================================//
  // "GetFIXSrcSecDefs":                                                     //
  //=========================================================================//
  // Implemented by template specialisation. If  the Dialect  does not  support
  // MktData, it does not need a list of all SecDefs (the function would return
  // NULL):
  template<FIX::DialectT::type D>
  std::vector<SecDefS> const& GetFIXSrcSecDefs(MQTEnvT a_cenv);
}
// End namespace MAQUETTE
