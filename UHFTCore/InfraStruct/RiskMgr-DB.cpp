// vim:ts=2:et
//===========================================================================//
//                             "RiskMgr-DB.cpp":                             //
//                   Loading RiskMgr Data Structs from a DB                  //
//===========================================================================//
#include "Basis/TimeValUtils.hpp"
#include "InfraStruct/RiskMgr.h"
#include "Venues/LATOKEN/SecDefs.h"  // For LATOKEN-specific stuff
#include <utxx/compiler_hints.hpp>
#include <utility>
#include <exception>

#if WITH_PQXX
#include <pqxx/pqxx>
#endif

using namespace std;

namespace MAQUETTE
{
  //=========================================================================//
  // "Load":                                                                 //
  //=========================================================================//
  void RiskMgr::Load
  (
    boost::property_tree::ptree const& a_params,
#   if (WITH_PQXX && !CRYPTO_ONLY)
    SecDefsMgr                  const& a_sdm
#   else
    SecDefsMgr                  const&
#   endif
  )
  {
    try
    {
      // TODO: At the moment, only LATOKEN-specific Schema is supported:
      string dbSchm = a_params.get<string>("DBSchema", "");
      if (dbSchm.empty())
        return;       // Not doing anything

      if (dbSchm != "LATOKEN")
      {
        LOG_ERROR(2,
          "RiskMgr::Load: Only LATOKEN PostgreSQL DB schema is currently "
          "supported: NOT performing recovery")
        return;
      }
      // If OK: Run the LATOKEN-specific version if available:
#     if (WITH_PQXX && !CRYPTO_ONLY)
      LoadLATOKEN(a_params, a_sdm);
#     else
      throw utxx::runtime_error
            ("RiskMgr::LoadLATOKEN: UnImplemented: Requires PQXX");
#     endif
    }
    catch (exception const& exn)
    {
      LOG_CRIT(0, "RiskMgr::Load: Recovery Failed: EXCEPTION: {}", exn.what())
      // It is probably a good idea to re-throw the exception:
      throw;
    }
  }

  //=========================================================================//
  // "LoadLATOKEN" (Requires PQXX):                                          //
  //=========================================================================//
# if (WITH_PQXX && !CRYPTO_ONLY)
  void RiskMgr::LoadLATOKEN
  (
    boost::property_tree::ptree const& a_params,
    SecDefsMgr                  const& a_sdm
  )
  {
    //-----------------------------------------------------------------------//
    // Get the DB-related params:                                            //
    //-----------------------------------------------------------------------//
    string dbHost = a_params.get<string>("DBHost_PgSQL",     "");
    int    dbPort = a_params.get<int>   ("DBPort_PgSQL",     -1);
    string dbName = a_params.get<string>("DBName_PgSQL",     "");
    string dbUser = a_params.get<string>("DBUser_PgSQL",     "");
    string dbPass = a_params.get<string>("DBPass_PgSQL",     "");
    string tabIR  = a_params.get<string>("DBTable_IR_PgSQL", "");
    string tabAR  = a_params.get<string>("DBTable_AR_PgSQL", "");

    // If any of the above params is empty, "Load" cannot proceed:
    if (utxx::unlikely
       (dbHost.empty() || dbPort < 0     || dbName.empty() ||
        dbUser.empty() || dbPass.empty() || tabIR.empty()  || tabAR.empty()))
    {
      LOG_WARN(2,
        "RiskMgr::LoadLATOKEN: DB config empty/incomplete: NOT performing "
        "recovery")
      return;
    }
    //-----------------------------------------------------------------------//
    // Connect to the DB:                                                    //
    //-----------------------------------------------------------------------//
    pqxx::connection conn
      ("host="    + dbHost + " port=" + to_string(dbPort) +
       " dbname=" + dbName + " user=" + dbUser + " password=" + dbPass);
    pqxx::work   txn(conn);

    //-----------------------------------------------------------------------//
    // Read the "AssetRisks" in:                                             //
    //-----------------------------------------------------------------------//
    // This needs to be done first, as subsequently-read "InstrRisks" will dep-
    // end on the "AssetRisks". The latter should only contain UserID=0 yet:
    assert(m_assetRisks.size() == 1);
    pqxx::result resAR = txn.exec("SELECT * from " + tabAR);

    for (auto const& row: resAR)
    {
      // If the RFC does not match, this "row" is not applicable: This is NOT
      // an error:
      char const* rfc = row["c_rfc"].c_str();
      if (rfc == nullptr || strcmp(m_RFC.data(), rfc) != 0)
        continue;

      // Get the UserID:
      UserID userID = row["c_user_id"].as<UserID>();

      // Get or create the corresp InnerMap:
      auto it =  m_assetRisks.find(userID);
      if  (it == m_assetRisks.end())
        it = m_assetRisks.insert
             (make_pair
               (userID,
                ARsMapI(ARsMapILessT(),
                        ARsMapIAlloc(s_pm.GetSegm()->get_segment_manager()))
             )).first;
      assert(it != m_assetRisks.end() && it->first == userID);
      ARsMapI& mapI = it->second;     // Ref!

      // Get the Asset Name. Use the LATOKEN-specific AssocList. XXX: We do not
      // even create a map out of it, because "Load*" is a "one-off" operation:
      // Perform a linear search instead:
      unsigned    ccyID     = row["c_ccy_id"].as<unsigned>();
      char const* assetName = nullptr;
      for (auto const& qd: LATOKEN::Assets)
        if (utxx::unlikely(get<0>(qd) == ccyID))
        {
          assetName = get<2>(qd);
          break;
        }
      if (utxx::unlikely(assetName == nullptr))
      {
        LOG_ERROR(2, "RiskMgr::LoadLATOKEN: UnExpected CcyID={}", ccyID)
        // Skip it and continue:
        continue;
      }
      // Get the SettlDate: For LATOKEN, it must cirrently be 0:
      int settlDate  = row["c_settl_date"].as<int>();
      assert(settlDate == 0);

      // Now create a new "AssetRisks" obj:
      AssetRisks  ar(this, userID, MkSymKey(assetName), settlDate);

      // Insert it into the map:
      ARsMapIKeyT keyI   = make_pair(ar.m_asset, ar.m_settlDate);

      auto insRes = mapI.insert(make_pair(keyI,  ar));
      if (utxx::unlikely(!insRes.second))
      {
        LOG_ERROR(2, "RiskMgr::LoadLATOKEN: Duplicate Asset={}", assetName)
        continue;
      }
      // If OK: Install other ("mutable") Flds in-place. XXX: There is no value
      // range verification here:
      assert(insRes.first != mapI.end() && insRes.first->first  == keyI);
      AssetRisks* arp = &(insRes.first->second);

      arp->m_lastEvalRate    = row["c_asset_rfc_rate"    ].as<double>();
      arp->m_ts              =
        DateTimeToTimeValSQL  (row["c_exch_ts"           ].c_str());
      if (utxx::unlikely(arp->m_ts.empty()))
        arp->m_ts            =
        DateTimeToTimeValSQL  (row["c_ts"                ].c_str());
      arp->m_epoch           =
        DateTimeToTimeValSQL  (row["c_epoch"             ].c_str());
      arp->m_initPos         = row["c_initial"           ].as<double>();
      arp->m_trdDelta        = row["c_trd_delta"         ].as<double>();
      arp->m_cumTranss       = row["c_cum_trabsfers"     ].as<double>();
      arp->m_cumDeposs       = row["c_cum_deposits"      ].as<double>();
      arp->m_cumDebt         = row["c_cum_debt"          ].as<double>();
      arp->m_extTotalPos     = 0.0;   // Not in the DB
      arp->m_initRFC         = row["c_initial_rfc"       ].as<double>();
      arp->m_trdDeltaRFC     = row["c_trd_delta_rfc"     ].as<double>();
      arp->m_cumTranssRFC    = row["c_cum_transfers_rfc" ].as<double>();
      arp->m_cumDepossRFC    = row["c_cum_deposits_rfc"  ].as<double>();
      arp->m_cumDebtRFC      = row["c_cum_debt_rfc"      ].as<double>();
      arp->m_apprInitRFC     = row["c_appr_initial_rfc"  ].as<double>();
      arp->m_apprTrdDeltaRFC = row["c_appr_trd_delta_rfc"].as<double>();
      arp->m_apprTranssRFC   = row["c_appr_transfers_rfc"].as<double>();
      arp->m_apprDepossRFC   = row["c_appr_deposits_rfc" ].as<double>();
    }
    //-----------------------------------------------------------------------//
    // Read the "InstrRisks" in:                                             //
    //-----------------------------------------------------------------------//
    // They should only contain UserID==0 at the moment:
    assert(m_instrRisks.size() == 1);

    pqxx::result resIR = txn.exec("SELECT * from " + tabIR);

    for (auto const& row: resIR)
    {
      // If the RFC does not match, this "row" is not applicable: This is NOT
      // an error:
      char const* rfc = row["c_rfc"].c_str();
      if (rfc == nullptr || strcmp(m_RFC.data(), rfc) != 0)
        continue;

      // First, get the UserID:
      UserID userID = row["c_user_id"].as<UserID>();

      // Get or create the corresp InnerMap:
      auto it =  m_instrRisks.find(userID);
      if  (it == m_instrRisks.end())
        it = m_instrRisks.insert
             (make_pair
               (userID,
                IRsMapI(less<SecID>(),
                        IRsMapIAlloc(s_pm.GetSegm()->get_segment_manager()))
             )).first;
      assert(it != m_instrRisks.end() && it->first == userID);
      IRsMapI& mapI = it->second;     // Ref!

      // Get the SecDef:
      SecID          secID = row["c_instr_id"].as<SecID>();
      SecDefD const* instr = a_sdm.FindSecDefOpt (secID);

      if (utxx::unlikely(instr == nullptr))
      {
        // Log an error but continue:
        LOG_ERROR(2, "RiskMgr::LoadLATOKEN: SecID={} Not Found", secID)
        continue;
      }

      // Find the "AssetRisks" for A and B -- they must already be in:
      AssetRisks* arA =
        GetAssetRisksImpl
          (instr->m_AssetA, instr->m_SettlDate, userID, nullptr);
      if (utxx::unlikely(arA == nullptr))
      {
        LOG_ERROR(2,
          "RiskMgr::LoadLATOKEN: Asset={} (SettlDate={}), UserID={}: Not Found",
          instr->m_AssetA.data(),   instr->m_SettlDate, userID)
        continue;
      }

      AssetRisks* arB =
        GetAssetRisksImpl
          (MkSymKey(instr->m_QuoteCcy.data()), instr->m_SettlDate, userID,
           nullptr);
      if (utxx::unlikely(arB == nullptr))
      {
        LOG_ERROR(2,
          "RiskMgr::LoadLATOKEN: Asset={} (SettlDate={}), UserID={}: Not Found",
          instr->m_QuoteCcy.data(), instr->m_SettlDate, userID)
      }

      // We can now construct the InstrRisks obj. The OrderBook is NULL as yet:
      // XXX: the CallER will need to run "Register" again:
      //
      InstrRisks ir(this, userID, *instr, nullptr, arA, arB);

      // Install the "InstrRisks" in "mapI":
      auto insRes = mapI.insert(make_pair(secID, ir));
      if (utxx::unlikely(!insRes.second))
      {
        LOG_ERROR(2,
          "RiskMgr::LoadLATOKEN: Duplicate SecID={}, Instr={}",
          secID, instr->m_FullName.data())
        continue;
      }
      // If OK: Install other ("mutable") Flds in-place. XXX: There is no value
      // range verification here:
      assert(insRes.first != mapI.end() && insRes.first->first == secID);
      InstrRisks* irp = &(insRes.first->second);

      // XXX: The latest B/RFC rate is currently not stored in "InstrRisks",
      // but it is available from the corresp "AssetRisks". Do NOT use "GetVal-
      // uationRate" as it may still try to re-compute it -- extract the saved
      // rate directly:
      irp->m_lastRateB      = arB->m_lastEvalRate;
      irp->m_ts             = DateTimeToTimeValSQL(row["c_exch_ts"].c_str());
      if (utxx::unlikely(irp->m_ts.empty()))
        irp->m_ts           = DateTimeToTimeValSQL(row["c_ts"     ].c_str());

      irp->m_posA           = RMQtyA(row["c_pos_a"         ].as<double>());
      irp->m_avgPosPxAB     = PriceT(row["c_cost_px"       ].as<double>());
      irp->m_realisedPnLB   = RMQtyB(row["c_cum_rlsd_pnl_b"].as<double>());
      irp->m_unrPnLB        = RMQtyB(row["c_unrlsd_pnl_b"  ].as<double>());

      irp->m_realisedPnLRFC = row["c_cum_rlsd_pnl_rfc" ]    .as<double>();
      irp->m_apprRealPnLRFC = row["c_appr_rlsd_pnl_rfc"]    .as<double>();
      irp->m_unrPnLRFC      = row["c_unrlsd_pnl_rfc"   ]    .as<double>();

      // The remaining flds are always 0 for LATOKEN (as set out by Ctor)...
    }
    // All Done:
    LOG_INFO(1, "RiskMgr::LoadLATOKEN: Recovery Complete")
  }
# endif
}
// End namespace MAQUETTE
