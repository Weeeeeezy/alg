// vim:ts=2:et
//===========================================================================//
//              "Connectors/H2WS/OKEX/EConnector_WS_OKEX_STP.cpp":           //
//                           WS-Based STP for OKEX                           //
//===========================================================================//
#include "Connectors/H2WS/OKEX/EConnector_WS_OKEX_STP.h"
#include "Venues/OKEX/SecDefs.h"
#include "Protocols/H2WS/WSProtoEngine.hpp"
#include <utxx/compiler_hints.hpp>
#include <utxx/error.hpp>

using namespace std;

namespace MAQUETTE
{
  //=========================================================================//
  // Non-Default Ctor:                                                       //
  //=========================================================================//
  EConnector_WS_OKEX_STP::EConnector_WS_OKEX_STP
  (
    EPollReactor*                       a_reactor,        // Non-NULL
    SecDefsMgr*                         a_sec_defs_mgr,   // Non-NULL
    vector<string> const*               a_only_symbols,   // NULL=UseFullList
    RiskMgr*                            a_risk_mgr,       // Non-NULL
    boost::property_tree::ptree const&  a_params
  )
  : //-----------------------------------------------------------------------//
    // "EConnector":                                                         //
    //-----------------------------------------------------------------------//
    // NB: From the "EConnector" Ctor perspective, IsOMC=false here, because
    // we do not need to allocate ShM space for AOSes, Req12s and Trades  :
    //
    // AccountKey would typically be {AccountPfx}-OKEX-STP-{Prod|Test} :
    //
    EConnector
    (
      a_params.get<string>("AccountKey"),
      "OKEX",
      false,           // Do NOT use Dates in ShM Segm Names
      ShMSz,           // Estimated ShM size
      a_reactor,
      false,           // No BusyWait
      a_sec_defs_mgr,
      OKEX::SecDefs,
      a_only_symbols,
      false,           // Explicit SettlDates are NOT required!
      false,           // No tenors in SecIDs
      a_risk_mgr,
      a_params,
      OKEX::QT,
      is_floating_point_v<OKEX::QR>
    )
  {}

  //=========================================================================//
  // "InitSession":                                                          //
  //=========================================================================//
  void EConnector_WS_OKEX_STP::InitSession() const
  {
    //-----------------------------------------------------------------------//
    // Construct the Path from the Instrs:                                   //
    //-----------------------------------------------------------------------//
    // NB: The number of subscribed instrs is potentially large, so use String
    // Stream -- it is OK as this method is for starting-up only:
    stringstream sp;
    sp << "/stream?streams=";

    bool frst  = true;
    for (SecDefD const* instr: m_usedSecDefs)
    {
      assert(instr != nullptr);
      if (frst)
        frst  = false;
      else
        sp << '/';

      // Put the Depth-20 SnapShot subscription into the Path. The Symbol needs
      // to be in LowerCase for some reason, so use AltSymbol:
      sp << instr->m_AltSymbol.data() << "@depth20";

      if (EConnector_MktData::HasTrades())
        // Subscribe to Aggregated (by Aggression) Trades:
        sp << '/' << instr->m_AltSymbol.data() << "@aggTrade";
    }
    //-----------------------------------------------------------------------//
    // Proceed with the HTTP/WS HandShake using the Path generated:          //
    //-----------------------------------------------------------------------//
    InitWSHandShake(sp.str().c_str());
  }

  //=========================================================================//
  // "InitLogOn":                                                            //
  //=========================================================================//
  void EConnector_WS_OKEX_STP::InitLogOn()
    { MDCWS::LogOnCompleted(utxx::now_utc()); }

  //=========================================================================//
  // "ProcessWSMsg":                                                         //
  //=========================================================================//
  // Returns "true" to continue receiving msgs, "false" to stop:
  //
  bool EConnector_WS_OKEX_STP::ProcessWSMsg
  (
    char const*     a_msg_body,
    int             a_msg_len,
    bool            UNUSED_PARAM(a_last_msg),
    utxx::time_val  a_ts_recv,
    utxx::time_val  a_ts_handl
  )
  {
    assert(a_msg_body != nullptr && a_msg_len > 0);
    try
    {
    }
    catch (std::exception const& exn)
    {
      //---------------------------------------------------------------------//
      // Any Exception:                                                      //
      //---------------------------------------------------------------------//
      // Log it but continue reading and processing (because all SnapShots and
      // Trades are independent from each other -- there would be no inconsist-
      // encies):
      LOG_ERROR(1,
        "EConnector_WS_OKEX_STP::ProcessWSMsg: {}: EXCEPTION: {}",
        string(a_msg_body, size_t(a_msg_len)), exn.what())
    }
    //-----------------------------------------------------------------------//
    // In any case: Continue:                                                //
    //-----------------------------------------------------------------------//
    return true;
  }
} // End namespace MAQUETTE
