// vim:ts=2:et
//===========================================================================//
//                      "Venues/LATOKEN/Configs_WS.h":                       //
//===========================================================================//
#pragma  once
#include "Connectors/H2WS/Configs.hpp"
#include <map>

namespace MAQUETTE
{
namespace LATOKEN
{
  //=========================================================================//
  // MDC and OMC Configs (STP currently uses OMC ones) and API Keys:         //
  //=========================================================================//
  extern std::map<MQTEnvT, H2WSConfig const> const Configs_WS_MDC;
  extern std::map<MQTEnvT, H2WSConfig const> const Configs_WS_OMC;

  // "APIKeysIntMM" is a map {UserID => APIKey}:
  // We currently maintain them for Internal MM Accounts only. They are exempt
  // from any trading fees:
  extern std::map<UserID, std::string const> const APIKeysIntMM;

  // Similarly, there is a list of External MM Accounts.   They are NOT exempt
  // from trading fees, but for some other reasons (eg LiqStats monitoring),
  // we should be aware of them:
  extern std::vector<UserID> const                 UserIDsExtMM;

  //-------------------------------------------------------------------------//
  // "GetAPIKey":                                                            //
  //-------------------------------------------------------------------------//
  // XXX: Currently same API Key space for OMC (incl STP) and MDC?
  //
  inline std::string const& GetAPIKey(UserID a_user_id)
  {
    // Try to extract the APIKey:
    auto cit = APIKeysIntMM.find(a_user_id);

    if (utxx::unlikely(cit == APIKeysIntMM.cend()))
      throw utxx::badarg_error
           ("LATOKEN::GetAPIKey: Missing UserID=", a_user_id);
    // If OK:
    return cit->second;
  }
} // End namespace LATOKEN
} // End namespace MAQUETTE
