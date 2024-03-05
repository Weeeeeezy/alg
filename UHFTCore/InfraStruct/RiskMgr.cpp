// vim:ts=2:et
//===========================================================================//
//                        "InfraStruct/RiskMgr.cpp":                         //
//                          RiskMgr Implementation                           //
//===========================================================================//
#include "Basis/QtyConvs.hpp"
#include "InfraStruct/PersistMgr.h"
#include "InfraStruct/RiskMgr.h"
#include "Connectors/EConnector_MktData.h"
#include "Connectors/EConnector_OrdMgmt.h"
#include <utxx/error.hpp>
#include <unistd.h>

using namespace std;

namespace MAQUETTE
{
  //=========================================================================//
  // "PersistMgr" Obj for the "RiskMgr":                                     //
  //=========================================================================//
  PersistMgr<> RiskMgr::s_pm;  // Initially empty

  //=========================================================================//
  // "RiskMgr" Non-Default Ctor:                                             //
  //=========================================================================//
  // NB:
  // (*) This Ctor is private: It can only be invoked from within the ShM segm
  //     mgr;
  // (*) The Limits set out here may or may not have effect at run time, depen-
  //     ding on the RMMode (see "Start"); nevertheless,  the  corresp "a_prms"
  //     entries are compulsory:
  //
  RiskMgr::RiskMgr
  (
    bool                               a_is_prod,
    boost::property_tree::ptree const& a_prms,
    spdlog::logger*                    a_logger, // May be NULL
    int                                a_debug_level
  )
  : m_isProd              (a_is_prod),
    m_RFC                 (MkCcy(a_prms.get<string>("RFC"))),
    m_isActive            (false),               // Initially
    m_mode                (RMModeT::Normal),     // Initially; set by "Start"
    m_mdUpdatesPeriodMSec (a_prms.get<int>   ("MDUpdatesPeriodMSec", 0)),
    m_nextMDUpdate        (),                    // Set by "Start"
    //
    // Main Risk and NAV Limits:
    //
    m_MaxTotalRiskRFC     (a_prms.get<double>("MaxTotalRisk_RFC")),
    m_MaxNormalRiskRFC    (a_prms.get<double>("MaxNormalRisk_RFC")),
    m_MinTotalNAV_RFC     (a_prms.get<double>("MinTotalNAV_RFC" )),
    m_MaxOrdSzRFC         (a_prms.get<double>("MaxOrderSize_RFC")),
    m_MinOrdSzRFC         (a_prms.get<double>("MinOrderSize_RFC")),
    m_MaxActiveOrdsTotalSzRFC
                          (a_prms.get<double>("MaxActiveOrdersTotalSize_RFC")),
    //
    // Settings for Order Volume Throttlers:
    //
    m_VlmThrottlPeriod1   (a_prms.get<int>   ("VlmThrottlPeriod1_Sec")),
    m_VlmLimitRFC1        (long(Round(a_prms.get<double>("VlmLimit1_RFC")))),

    m_VlmThrottlPeriod2   (a_prms.get<int>   ("VlmThrottlPeriod2_Sec")),
    m_VlmLimitRFC2        (long(Round(a_prms.get<double>("VlmLimit2_RFC")))),

    m_VlmThrottlPeriod3   (a_prms.get<int>   ("VlmThrottlPeriod3_Sec")),
    m_VlmLimitRFC3        (long(Round(a_prms.get<double>("VlmLimit3_RFC")))),
    //
    // Order Volume Throttlers themselves:
    //
    m_orderEntryThrottler1(m_VlmThrottlPeriod1),
    m_orderFillThrottler1 (m_VlmThrottlPeriod1),

    m_orderEntryThrottler2(m_VlmThrottlPeriod2),
    m_orderFillThrottler2 (m_VlmThrottlPeriod2),

    m_orderEntryThrottler3(m_VlmThrottlPeriod3),
    m_orderFillThrottler3 (m_VlmThrottlPeriod3),
    //
    // Other Settings:
    //
    m_omcs                (),
    m_logger              (a_logger),
    m_debugLevel          (a_debug_level),
    //
    // Call-Backs are initially empty (set separately):
    //
    m_irUpdtCB            (nullptr),
    m_arUpdtCB            (nullptr),
    //
    // "InstrRisks" and "AssetRisks" Maps:
    //
    m_userIDs             (std::less<UserID>(),
                           UserIDsAlloc(s_pm.GetSegm()->get_segment_manager())),
    m_affectedUserIDs     (new   set<UserID>),
    m_instrRisks          (std::less<UserID>(),
                           IRsMapAlloc (s_pm.GetSegm()->get_segment_manager())),
    m_assetRisks          (std::less<UserID>(),
                           ARsMapAlloc (s_pm.GetSegm()->get_segment_manager())),
    m_obirMap             (new OBIRMap),
    m_obarMap             (new OBARMap),
    //
    // Totals (for each UserID):
    //
    m_totalRiskRFC        (std::less<UserID>(),
                           TotalsMapAlloc
                             (s_pm.GetSegm()->get_segment_manager())),
    m_totalActiveOrdsSzRFC(std::less<UserID>(),
                           TotalsMapAlloc
                             (s_pm.GetSegm()->get_segment_manager())),
    m_totalNAV_RFC        (std::less<UserID>(),
                             (s_pm.GetSegm()->get_segment_manager()))
  {
    // Verify the Limits (Periods are verified by the utxx::rate_throttler ctor
    // automatically):
    if (utxx::unlikely
       (m_MaxTotalRiskRFC <= 0.0  || m_MinTotalNAV_RFC >= 0.0 ||
        m_MaxOrdSzRFC     <= 0.0  || m_MinOrdSzRFC     <= 0.0 ||
        m_MaxOrdSzRFC     <  m_MinOrdSzRFC                    ||
        m_MaxTotalRiskRFC <  m_MaxNormalRiskRFC               ||
        m_MaxActiveOrdsTotalSzRFC <= 0.0                      ||
        m_MaxActiveOrdsTotalSzRFC <  m_MaxOrdSzRFC            ||
        m_VlmLimitRFC1    <= 0    || m_VlmLimitRFC2    <= 0   ||
        m_VlmLimitRFC3    <= 0))
      throw utxx::badarg_error("RiskMgr::Ctor: Invalid Limit(s)");

    // The Logger is required:
    if (utxx::unlikely(m_logger == nullptr))
      throw utxx::badarg_error("RiskMgr::Ctor: Logger is required");

    // UserID=0 is always created (as a "prototype" and a "catch-all" one):
    m_userIDs.insert(0);

    // Install "IRsMapI" and "ARsMapI" for UserID=0, empty as yet:
    InstallEmptyMaps(0);

    // All Done!
  }

  //=========================================================================//
  // "InstallEmptyMaps":                                                     //
  //=========================================================================//
  // Install "IRsMapI" and "ARsMapI" for a given UserID, empty as yet:
  //
  void RiskMgr::InstallEmptyMaps(UserID a_user_id)
  {
    // XXX: For the moment, this method is only invoked for UserID=0:
    assert(a_user_id == 0);

    // "InstrRisks":
    auto insResIR =
      m_instrRisks.insert
        (make_pair
          (a_user_id,
           IRsMapI(less<SecID>(),
                   IRsMapIAlloc(s_pm.GetSegm()->get_segment_manager()))
        ));
    if (utxx::unlikely(!insResIR.second))
      throw utxx::logic_error
            ("RiskMgr::InstallEmptyMaps: UserID=", a_user_id,
             ": InstrRisks insert failed");

    // "AssetRisks":
    auto insResAR =
      m_assetRisks.insert
        (make_pair
          (a_user_id,
           ARsMapI(ARsMapILessT(),
                   ARsMapIAlloc(s_pm.GetSegm()->get_segment_manager()))
        ));
    if (utxx::unlikely(!insResAR.second))
      throw utxx::logic_error
            ("RiskMgr::InstallEmptyMaps: UserID=", a_user_id,
             ": AssetRisks insert failed");
  }

  //=========================================================================//
  // "GetPersistInstance": Static Factory:                                   //
  //=========================================================================//
  // Creates a "RiskMgr" object in ShM, or finds an existing one there:
  //
  RiskMgr* RiskMgr::GetPersistInstance
  (
    bool                               a_is_prod,
    boost::property_tree::ptree const& a_params,
    SecDefsMgr                  const& a_sdm,      // Only required for "Load"
    bool                               a_is_observer,
    spdlog::logger*                    a_logger,
    int                                a_debug_level
  )
  {
    //-----------------------------------------------------------------------//
    // RiskMgr ShM Obj Name -- depends on the Prod/Test mode:                //
    //-----------------------------------------------------------------------//
    string prefix   = a_params.get<string>("AccountPfx");
    string objName  =
      (prefix.empty() ? "RiskMgr-" : prefix + "-RiskMgr-");
    objName        += (a_is_prod ? "Prod"   : "Test");

    bool   resetAll = a_params.get<bool>  ("ResetAll",  false);
    size_t segmSz   = a_params.get<size_t>("ShMSegmSzMB") * (1UL << 20);

    // If "a_is_observer" is set, "resetAll" and "a_logger" must be empty. We
    // will NOT create a new "RiskMgr" obj  -- only attach to an existing one
    // in a Read-Only mode:
    if (utxx::unlikely(a_is_observer && (resetAll || a_logger != nullptr)))
      throw utxx::badarg_error
            ("RiskMgr::GetPersistInstance: ObserverMode and (ResetAll|Logger) "
             "must NOT be set together");

    //-----------------------------------------------------------------------//
    // Create the Static Objects (incl the PersistMgr):                      //
    //-----------------------------------------------------------------------//
    if (utxx::likely(s_pm.IsEmpty()))
    {
      if (a_is_observer)
        s_pm.Init(objName);
      else
        // CurrDate is NOT used in RiskMgr segment; the BaseMapAddr is the def-
        // ault one. Reserve a decent amount of space  for the RiskMgr  which
        // contains all static risks data structs:
        //
        s_pm.Init(objName, nullptr, segmSz);
    }
    assert(!s_pm.IsEmpty());

    //-----------------------------------------------------------------------//
    // Find or construct the "RiskMgr" obj inside the ShM Segment:           //
    //-----------------------------------------------------------------------//
    // NB: Here "res" would initially be NULL if the Segment has just been
    // created:
    RiskMgr* res = s_pm.GetSegm()->find<RiskMgr>(RiskMgrON()).first;

    if (a_is_observer)
    {
      if (utxx::unlikely(res == nullptr))
        // This should not normally happen, but still:
        throw utxx::runtime_error
              ("RiskMgr::GetPersistInstance: ObserverMode: Cannot find the "
               "RiskMgr instance in the ShM segment");
      else
        // All Done in the ObserverMode:
        return res;
    }
    //-----------------------------------------------------------------------//
    // Generic Case (NOT ObserverMode):                                      //
    //-----------------------------------------------------------------------//
    assert(!a_is_observer);

    // Here the Logger is required:
    if (utxx::unlikely(a_logger == nullptr))
      throw utxx::badarg_error
            ("RiskMgr::GetPersistInstance: Logger is required");

    if (res != nullptr)
    {
      //---------------------------------------------------------------------//
      // Got the "RiskMgr" object, but may need to update it:                //
      //---------------------------------------------------------------------//
      // NB: If any of the limits listed below are not valid, we simply do not
      // apply them -- the existing limits in the "res" obj would remain unch-
      // anged:
      int mdUpdatesPeriodMSec = a_params.get<int>   ("MDUpdatesPeriodMSec", 0);

      double maxTotalRiskRFC  = a_params.get<double>("MaxTotalRisk_RFC");
      double maxNormalRiskRFC = a_params.get<double>("MaxNormalRisk_RFC");
      double minTotalNAV_RFC  = a_params.get<double>("MinTotalNAV_RFC" );
      double maxOrdSzRFC      = a_params.get<double>("MaxOrderSize_RFC");
      double minOrdSzRFC      = a_params.get<double>("MinOrderSize_RFC");

      double maxActiveOrdsTotalSzRFC =
             a_params.get<double>("MaxActiveOrdersTotalSize_RFC");

      long   vlmLimitRFC1     = a_params.get<long>  ("VlmLimit1_RFC");
      long   vlmLimitRFC2     = a_params.get<long>  ("VlmLimit2_RFC");
      long   vlmLimitRFC3     = a_params.get<long>  ("VlmLimit3_RFC");

      //---------------------------------------------------------------------//
      // Modify an existing "RiskMgr" ShM object:                            //
      //---------------------------------------------------------------------//
      // FIXME: Currently, we need to remove all "EConnector_OrdMgmt" ptrs, as
      // they point to non-shared memory,  and may have become invalid by now.
      // But this does not allow us to use a single ShM-based "RiskMgr" obj by
      // multiple processes, so it must be changed in the future. Same applies
      // to OrderBook-keyed maps:
      res->m_omcs.clear();
      res->m_obirMap             = new OBIRMap;
      res->m_obarMap             = new OBARMap;
      res->m_affectedUserIDs     = new set<UserID>;

      // XXX: Make the RiskMgr inactive until Start() is invoked explicitly.
      // This only works correctly in the current fully-synchronous setup;
      // if the RiskMgr later becomes a stand-alone process  whose services
      // are shared between multiple dynamic Strategies,  this  logic  will
      // need to be changed:
      res->m_isActive            = false;

      // No need to reset the "NextMDUpdate" as "Start" would do it anyway...

      // We STILL need to install new params (if valid) in the RiskMgr object:
      // the limits may have changed, and the "m_logger" must now point to the
      // new logger in the Conventional Memory:
      //
      res->m_mdUpdatesPeriodMSec = mdUpdatesPeriodMSec;

      if (utxx::likely(maxTotalRiskRFC  > 0.0))
        res->m_MaxTotalRiskRFC   = maxTotalRiskRFC;

      if (utxx::likely(maxNormalRiskRFC > 0.0))
        res->m_MaxNormalRiskRFC  = maxNormalRiskRFC;

      if (utxx::likely(minTotalNAV_RFC  < 0.0))
        res->m_MinTotalNAV_RFC   = minTotalNAV_RFC;

      // NB: Order size(s) can only be changed together:
      if (utxx::likely(0.0 < minOrdSzRFC && minOrdSzRFC <= maxOrdSzRFC))
      {
        res->m_MaxOrdSzRFC       = maxOrdSzRFC;
        res->m_MinOrdSzRFC       = minOrdSzRFC;
      }
      if (utxx::likely(maxActiveOrdsTotalSzRFC > 0.0))
        res->m_MaxActiveOrdsTotalSzRFC = maxActiveOrdsTotalSzRFC;

      // XXX: Normally, the limits below correspond to increasing  Period lens,
      // so should be non-decreasing as well -- but we currently do not enforce
      // that:
      if (utxx::likely(vlmLimitRFC1 > 0))
        res->m_VlmLimitRFC1      = vlmLimitRFC1;

      if (utxx::likely(vlmLimitRFC2 > 0))
        res->m_VlmLimitRFC2      = vlmLimitRFC2;

      if (utxx::likely(vlmLimitRFC3 > 0))
        res->m_VlmLimitRFC3      = vlmLimitRFC3;

      // Invalidate the Call-Backs (they will need to be set again explicitly):
      res->m_irUpdtCB = nullptr;
      res->m_arUpdtCB = nullptr;

      // And finally, set the Logger and DebugLevel to curr vals:
      res->m_logger              = a_logger;
      res->m_debugLevel          = a_debug_level;

      // Still need some consistency checks:
      if (utxx::unlikely
         (res->m_MaxOrdSzRFC             < res->m_MinOrdSzRFC           ||
          res->m_MaxTotalRiskRFC         < res->m_MaxNormalRiskRFC      ||
          res->m_MaxActiveOrdsTotalSzRFC < res->m_MaxOrdSzRFC))
        throw utxx::badarg_error
              ("RiskMgr::GetPersistInstance: Invalid Limit(s)");

      if (utxx::unlikely(resetAll))
      {
        //-------------------------------------------------------------------//
        // ResetAll requested:                                               //
        //-------------------------------------------------------------------//
        // Then we clear all UserIDs and Risks, and reset the Totals:
        res->m_userIDs             .clear();
        res->m_instrRisks          .clear();
        res->m_assetRisks          .clear();

        res->m_totalRiskRFC        .clear();
        res->m_totalActiveOrdsSzRFC.clear();
        res->m_totalNAV_RFC        .clear();

        // Again, install UserID=0 and the corresp "IRsMapI" and "ARsMapI" (as
        // yet empty):
        res->m_userIDs.insert(0);
        res->InstallEmptyMaps(0);

        // Reset the Throttlers as well:
        res->m_orderEntryThrottler1.reset();
        res->m_orderFillThrottler1 .reset();

        res->m_orderEntryThrottler2.reset();
        res->m_orderFillThrottler2 .reset();

        res->m_orderEntryThrottler3.reset();
        res->m_orderFillThrottler3 .reset();
      }
      else
      {
        //-------------------------------------------------------------------//
        // No reset: Preseve all existing Risk accumulators:                 //
        //-------------------------------------------------------------------//
        // TODO:
        // (*) Implement removal of "InstrRisks" with expired SettlDates. NB: we
        //     cannot do it in a simple way, because that could give rise to un-
        //     balanced Asset positions and trigger various wrong actions -- in
        //     both the RiskMgr and the Strategies;
        // (*) So will need to implement, in particular,  rolling  of FX Instrs
        //     positions (eg TOD->TOM), similar rolling of Asset positions, etc;
        // (*) FOR NOW, it is imperative simply to Reset the RiskMgr at the beg-
        //     inning of each Trading Day:
        //
        // "InstrRisks": Remove dangling OrderBook, Logger etc ptrs, but keep
        // the actual data:
        for (IRsMapKVT&  irp:  res->m_instrRisks)
        for (IRsMapIKVT& irpI: irp.second)
        {
          InstrRisks& ir  = irpI.second;
          ir.ResetTransient(nullptr);  // NewOrderBook=NULL yet, it's safe
        }
        // "AssetRisks": Update the Logger and remove Asset->RFC Valuators
        // (because they are not ShM-placed, so the ptrs are now dangling):
        //
        for (ARsMapKVT&  arp:  res->m_assetRisks)
        for (ARsMapIKVT& arpI: arp.second)
        {
          AssetRisks& ar  =  arpI.second;
          ar.ResetValuator();          // DefaultValuator is installed
        }
        // XXX: Even if the limits have been changed above, we do NOT check here
        // whether the new limits are now violated (by any chance),  because RFC
        // values may be outdated -- so wait until routine updates / checks...
      }
    }
    else
    {
      //---------------------------------------------------------------------//
      // Need to create a new "RiskMgr" object in ShM:                       //
      //---------------------------------------------------------------------//
      res = s_pm.GetSegm()->construct<RiskMgr>
            (RiskMgrON())
            (a_is_prod, a_params, a_logger, a_debug_level);
      assert(res != nullptr);

      // If the DB settings are given in "a_params", load the RiskMgr data from
      // the DB:
      res->Load(a_params, a_sdm);
    }
    // All Done:
    assert(res != nullptr && !(res->m_userIDs.empty()));
    return res;
  }

  //=========================================================================//
  // Accessors:                                                              //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "SetCallBacks":                                                         //
  //-------------------------------------------------------------------------//
  void RiskMgr::SetCallBacks
  (
    InstrRisksUpdateCB const& a_ir_updt_cb,
    AssetRisksUpdateCB const& a_ar_updt_cb
  )
  {
    m_irUpdtCB  =  new InstrRisksUpdateCB(a_ir_updt_cb);
    m_arUpdtCB  =  new AssetRisksUpdateCB(a_ar_updt_cb);
  }

  //-------------------------------------------------------------------------//
  // "GetAllInstrRisks":                                                     //
  //-------------------------------------------------------------------------//
  RiskMgr::IRsMapI const& RiskMgr::GetAllInstrRisks(UserID a_user_id) const
  {
    auto cit = m_instrRisks.find(a_user_id);
    if (utxx::unlikely(cit == m_instrRisks.cend()))
      throw utxx::badarg_error
            ("RiskMgr::GetAllInstrRisks: UserID Not Found: ", a_user_id);
    // Generic Case:
    return cit->second;
  }

  //-------------------------------------------------------------------------//
  // "GetAllAssetRisks":                                                     //
  //-------------------------------------------------------------------------//
  RiskMgr::ARsMapI const& RiskMgr::GetAllAssetRisks(UserID a_user_id) const
  {
    auto cit = m_assetRisks.find(a_user_id);
    if (utxx::unlikely(cit == m_assetRisks.cend()))
      throw utxx::badarg_error
            ("RiskMgr::GetAllAssetRisks: UserID Not Found: ", a_user_id);
    // Generic Case:
    return cit->second;
  }

  //-------------------------------------------------------------------------//
  // "GetTotalNAV_RFC":                                                      //
  //-------------------------------------------------------------------------//
  double RiskMgr::GetTotalNAV_RFC(UserID a_user_id) const
  {
    auto cit = m_totalNAV_RFC.find(a_user_id);
    if (utxx::unlikely(cit == m_totalNAV_RFC.cend()))
      throw utxx::badarg_error
            ("RiskMgr::GetTotalNAV_RFC: UserID Not Found: ", a_user_id);
    // Generic Case:
    return cit->second;
  }

  //-------------------------------------------------------------------------//
  // "GetTotalRiskRFC":                                                      //
  //-------------------------------------------------------------------------//
  double RiskMgr::GetTotalRiskRFC(UserID a_user_id) const
  {
    auto cit = m_totalRiskRFC.find(a_user_id);
    if (utxx::unlikely(cit == m_totalRiskRFC.cend()))
      throw utxx::badarg_error
            ("RiskMgr::GetTotalRiskRFC: UserID Not Found: ", a_user_id);
    // Generic Case:
    return cit->second;
  }

  //-------------------------------------------------------------------------//
  // "GetActiveOrdersTotalSizeRFC":                                          //
  //-------------------------------------------------------------------------//
  double RiskMgr::GetActiveOrdersTotalSizeRFC(UserID a_user_id) const
  {
    auto cit = m_totalActiveOrdsSzRFC.find(a_user_id);
    if (utxx::unlikely(cit == m_totalActiveOrdsSzRFC.cend()))
      throw utxx::badarg_error
            ("RiskMgr::GetctiveOrdersTotalSizeRFC: UserID Not Found: ",
             a_user_id);
    // Generic Case:
    return cit->second;
  }

  //=========================================================================//
  // "InstallXValuators":                                                    //
  //=========================================================================//
  // Make sure that "InstrRisks" with RealisedPnL, UnRealisedPnL and/or Active-
  // OrdsSz, if expressed in the given Asset, get their RFC vals re-calculated
  // when the corresp Asset is RFC-re-valuated:
  //
  inline void RiskMgr::InstallXValuators(AssetRisks const& a_ar)
  {
    // If this "AssetRisks" is not OrderBook-evaled, there is nothing to do:
    OrderBookBase const* ob1 = a_ar.m_evalOB1;
    OrderBookBase const* ob2 = a_ar.m_evalOB2;

    if (ob1 == nullptr)
    {
      assert(ob2 == nullptr);
      return;
    }
    // Otherwise: Traverse all "InstrRisks" for the given UserID:
    auto cit = m_instrRisks.find(a_ar.m_userID);

    if (utxx::unlikely(cit == m_instrRisks.cend()))
    {
      // XXX: We got "AssetRisks", but not "InstrRisks", for some UserID. In
      // theory, this might happen if there were Transfers only  (no Trades)
      // in that Asset. Yet is is strange, so produce a warning:
      LOG_WARN(2,
        "RiskMgr::InstallXValuators: No InstrRisks found for UserID={}",
        a_ar.m_userID)
      return;
    }
    // Otherwise: Get to the "InstrRisks":
    assert(m_obirMap != nullptr && m_obarMap != nullptr);

    IRsMapI  const& mapI = cit->second;
    for (IRsMapIKVT const& irpI: mapI)
    {
      InstrRisks const& ir = irpI.second;

      // Check whether this "ir" has anything to do with this AssetRisks:
      if (ir.m_risksA == &a_ar || ir.m_risksB == &a_ar)
      {
        // Yes, they are related; so anytime when this Asset is re-evaled in
        // RFC, we MAY (or MAY NOT) need to update the corresp RFC vals in the
        // "ir". So install it in the map, but hwn it ticks, check carefully
        // whether it is applicable:
        //
        (*m_obirMap)  [ob1].insert(const_cast<InstrRisks*>(&ir));

        if (ob2 != nullptr)
          (*m_obirMap)[ob2].insert(const_cast<InstrRisks*>(&ir));
      }
    }
    // All Done!
  }

  //=========================================================================//
  // "Start":                                                                //
  //=========================================================================//
  void RiskMgr::Start(RMModeT a_mode)
  {
    //-----------------------------------------------------------------------//
    // "AssetRisks" Checks:                                                  //
    //-----------------------------------------------------------------------//
    for (ARsMapKVT const& arp: m_assetRisks)
    {
      UserID          userID = arp.first;
      ARsMapI const&  mapI   = arp.second;

      for (auto itI = mapI.cbegin(); itI != mapI.cend(); ++itI)
      {
        auto       const& key       = itI->first;
        SymKey     const& astName   = key.first;
        int               settlDate = key.second;
        AssetRisks const& ar        = itI->second;

        //-------------------------------------------------------------------//
        // (1) "AssetRisks" Validity:                                        //
        //-------------------------------------------------------------------//
        if (utxx::unlikely
           (MAQUETTE::IsEmpty(ar.m_asset)                                   ||
            ar.m_userID != userID                                           ||
            strcmp(astName.data(),   ar.m_asset.data())               != 0  ||
            ar.m_isRFC  !=   (strcmp(ar.m_asset.data(), m_RFC.data()) == 0) ||
            ar.m_settlDate   != settlDate                                   ||
           (settlDate        != 0    &&   settlDate < EarliestDate)
        ))
          throw utxx::logic_error
                ("RiskMgr::Start: AssetRisks(", ar.m_asset.data(), ", ",
                 ar.m_settlDate,  "), UserID=", userID, ": Invalid Entry");

        //-------------------------------------------------------------------//
        // (2) Validity of Valuators in "AssetRisks":                        //
        //-------------------------------------------------------------------//
        bool obsValid =
           ar.m_evalOB1 != nullptr   && IsFinite(ar.m_adj1[0])       &&
           IsFinite(ar.m_adj1[1])                                    &&
           (ar.m_evalOB2 != nullptr) == IsFinite(ar.m_adj2[0])       &&
           (ar.m_evalOB2 != nullptr) == IsFinite(ar.m_adj2[1])       &&
           (ar.m_evalOB2 != nullptr) == (!ar.m_rollOverTime.empty());

        // Also check that the non-NULL Valuator OrderBooks are contained in the
        // OBARMap:
        bool obarsOK =
          ((ar.m_evalOB1 == nullptr) ||
            IsInOBARMap(ar.m_evalOB1, const_cast<AssetRisks*>(&ar))) &&
          ((ar.m_evalOB2 == nullptr) ||
            IsInOBARMap(ar.m_evalOB2, const_cast<AssetRisks*>(&ar)));

        if (utxx::unlikely
           (!(ar.m_isRFC || ar.m_fixedEvalRate > 0.0  ||
              obsValid   || ar.m_lastEvalRate  > 0.0) ||
            !obarsOK))
          throw utxx::logic_error
                ("RiskMgr::Start: AssetRisks(", ar.m_asset.data(), ", ",
                 ar.m_settlDate,  "): UserID=", userID,  ": No Asset->",
                 m_RFC.data(),    " Valuator");

        // If "ar" only has a Trivial Valuator, produce a warning but allow us
        // to start:
        if (utxx::unlikely(ar.HasTrivialValuator()))
          LOG_WARN(1,
            "RiskMgr::Start: AssetRisks={} (SettlDate={}), UserID={}: Has only "
            "a Trivial Valuator, will use it anyway...",
            ar.m_asset.data(), ar.m_settlDate, ar.m_userID)

        //-------------------------------------------------------------------//
        // (3) "AssetRisks" Uniqueness (within this UserID):                 //
        //-------------------------------------------------------------------//
        // Travserse all subsequent "AssetRisks", check for duplicates of "ar":
        //
        for (auto itI1 = next(itI); itI1 != mapI.cend(); ++itI1)
        {
          AssetRisks const&  ar1  = itI1->second;

          if (utxx::unlikely
             (strcmp(ar1.m_asset.data(), ar.m_asset.data()) == 0 &&
              ar1.m_settlDate        ==  ar.m_settlDate  ))
            throw utxx::logic_error
                  ("RiskMgr::Start: AssetRisks(", ar.m_asset.data(), ", ",
                   ar.m_settlDate, ") occures more than once for UserID=",
                   userID);
        }
        //-------------------------------------------------------------------//
        // (4) Install X-Valuators (see the comments there):                 //
        //-------------------------------------------------------------------//
        InstallXValuators(ar);
      }
    }
    //-----------------------------------------------------------------------//
    // InstrRisks Checks:                                                    //
    //-----------------------------------------------------------------------//
    for (IRsMapKVT const& irp: m_instrRisks)
    {
      UserID          userID = irp.first;
      IRsMapI const&  mapI   = irp.second;

      for (auto itI = mapI.cbegin(); itI != mapI.cend(); ++itI)
      {
        SecID             secID = itI->first;
        InstrRisks const& ir    = itI->second;

        //-------------------------------------------------------------------//
        // (5) "InstrRisks" Validity:                                        //
        //-------------------------------------------------------------------//
        // Once again, we do NOT require here that
        //     &(ir.m_ob->GetInstr()) != ir.m_instr,
        //     but we require that those Instruments are consistent in Asset(A),
        //     QuoteCcy(B) and SettlDate:
        //
        SecDefD const* obInstr =
          utxx::likely(ir.m_ob != nullptr) ? &(ir.m_ob->GetInstr()) : nullptr;

        if (utxx::unlikely
           (ir.m_instr == nullptr         || ir.m_userID != userID ||
            ir.m_instr->m_SecID !=  secID ))
          throw utxx::logic_error
                ("RiskMgr::Start: InstrRisks: SecID=", secID, ", UserD=",
                 userID, ": Invalid Entry");

        //-------------------------------------------------------------------//
        // (6) Consistency of "InstrRisks", its OrderBook and "AssetRisks":  //
        //-------------------------------------------------------------------//
        // XXX: Unlike the "AssetRisks", here we DO allow "InstrRisks"  w/o a
        // valuation OrderBook, because the latter affects UnRealisedPnL only:
        //
        if (utxx::unlikely
           ((obInstr                  != nullptr                &&
             (ir.m_instr->m_AssetA    != obInstr->m_AssetA      ||
              ir.m_instr->m_QuoteCcy  != obInstr->m_QuoteCcy    ||
              ir.m_instr->m_SettlDate != obInstr->m_SettlDate)) ||
            ir.m_risksA               ==  nullptr               ||
            ir.m_risksA->m_userID     !=  userID                ||
            strcmp(ir.m_risksA->m_asset .data(),
                   ir.m_instr ->m_AssetA.data())  != 0          ||
            ir.m_risksB               ==  nullptr               ||
            ir.m_risksB->m_userID     !=  userID                ||
            strcmp(ir.m_risksB->m_asset.data(),
                   ir.m_instr ->m_QuoteCcy.data()) != 0         ||
            !(ir.m_ob == nullptr     ||
             IsInOBIRMap(ir.m_ob, const_cast<InstrRisks*>(&ir)))
           ))
        throw utxx::logic_error
              ("RiskMgr::Start: InstrRisks: ", ir.m_instr->m_FullName.data(),
               ", UserID=",     userID,
               ": Inconsistent with OrderBook and/or AssetRisks");

        //-------------------------------------------------------------------//
        // (7) "InstrRisks" Uniqueness (within this USerID):                 //
        //-------------------------------------------------------------------//
        // Travserse all subsequent "InstrRisks", check for duplicates of "ir":
        //
        for (auto itI1 = next(itI); itI1 != mapI.cend(); ++itI1)
        {
          InstrRisks const&  ir1  = itI1->second;

          // Compare them by "SecDefD" ptr:
          if (utxx::unlikely(ir1.m_instr == ir.m_instr))
            throw utxx::logic_error
                  ("RiskMgr::Start: InstrRisks(", ir.m_instr->m_FullName.data(),
                   ") occurs more than once for UserID=", userID);
        }
      }
    }
    //-----------------------------------------------------------------------//
    // Finally: All checks are OK:                                           //
    //-----------------------------------------------------------------------//
    m_isActive = true;
    m_mode     = a_mode;

    // Also set the nearest Next MktData Update Time (if required):
    m_nextMDUpdate =
      (m_mdUpdatesPeriodMSec > 0)
      ? (utxx::now_utc () + utxx::msecs(m_mdUpdatesPeriodMSec))
      :  utxx::time_val();
  }

  //=========================================================================//
  // "IsInOBIRMap", "IsInOBARMap":                                           //
  //=========================================================================//
  // Wheteher the corresp Map entries exist (used in "Start" checks):
  //-------------------------------------------------------------------------//
  // "IsInOBIRMap": For "InstrRisks":                                        //
  //-------------------------------------------------------------------------//
  inline bool RiskMgr::IsInOBIRMap
    (OrderBookBase const* a_ob, InstrRisks* a_ir) const
  {
    assert(a_ob != nullptr && a_ir != nullptr && m_obirMap != nullptr);
    auto   cit  =  m_obirMap->find(a_ob);

    if (utxx::unlikely(cit == m_obirMap->cend()))
      return false;    // No such OrderBook in Map Keys at all!

    // Now check if this "InstrRisks" is in the OrderBook's image:
    assert(cit->first == a_ob);
    set<InstrRisks*> const&   irs = cit->second;
    assert(!irs.empty());
    return (irs.find(a_ir) != irs.cend());
  }

  //-------------------------------------------------------------------------//
  // "IsInOBARMap": For "AssetRisks":                                        //
  //-------------------------------------------------------------------------//
  inline bool RiskMgr::IsInOBARMap
    (OrderBookBase const* a_ob, AssetRisks* a_ar) const
  {
    assert(a_ob != nullptr && a_ar != nullptr && m_obarMap != nullptr);
    auto   cit  =  m_obarMap->find(a_ob);

    if (utxx::unlikely(cit == m_obarMap->cend()))
      return false;    // No such OrderBook in Map Keys at all!

    // Now check if this "AssetRisks" is in the OrderBook's image:
    assert(cit->first == a_ob);
    set<AssetRisks*> const&   ars = cit->second;
    assert(!ars.empty());
    return (ars.find(a_ar) != ars.cend());
  }

  //=========================================================================//
  // "Register (Instrument, PossiblyAnotherOrderBook)":                      //
  //=========================================================================//
  // A Traded Instrument is registered with the Risk Manager:
  // (*) Double registration is OK (simply ignored);
  // (*) It creates the "prototype" InstrRisks and AssetRisks for UserID=0;
  // (*) This is a public method which should be invoked off-line (before
  //     "Start");  at "Start", the created configuration will be checked
  //     extensively;
  // (*) In the on-line mode (after "Start"), the prototype is used on arri-
  //     val of a new Trade or BalanceUpdate with a non-0 UserID:
  //
  void RiskMgr::Register
  (
    SecDefD       const& a_instr,
    OrderBookBase const* a_ob      // May be NULL, but NOT recommended!
  )
  {
    //-----------------------------------------------------------------------//
    // Verify the OrderBook / MDC:                                           //
    //-----------------------------------------------------------------------//
    // Extract the SecDef: XXX: Here we use SettlDate coming from the SecDef:
    assert(!IsEmpty(a_instr.m_QuoteCcy));

    if (utxx::likely(a_ob != nullptr))
    {
      // Check that the corresp MDC indeed uses this RiskMgr, and therefore,
      // will forward OrderBook updates to us. NB:
      // (*) It will  currently fwd updates (L1Px) on ALL OrderBooks instal-
      //     led in that MDC; it is the RiskMgr's task to figure out  which
      //     of them are applicable (for UnRPnL and/or RFC valuations;  see
      //     "OnMktDataUpdate").
      // (*) But it is also possible that the OrderBook does not belong  to
      //     any MDC (eg it's part of an STP), that is OK:
      //
      EConnector_MktData const* mdc = a_ob->GetMDCOpt();

      if (utxx::unlikely(mdc != nullptr && mdc->GetRiskMgr() != this))
        throw utxx::badarg_error
              ("RiskMgr::Register: ", a_instr.m_FullName.data(),
               ": MDC=", mdc->GetName(), " is using inconsistent RiskMgr");
    }
    //-----------------------------------------------------------------------//
    // Find or Create the "AssetRisks(A,B)":                                 //
    //-----------------------------------------------------------------------//
    // Install 2 "AssetRisks" obj(s): Asset(A) and QuoteCcy(B). If either of
    // them already exists, we get ptr(s) to those existing Asset(s).     As
    // UserID=0 here,  the ptrs  returned  by "GetAssetRisksImpl" are always
    // non-NULL, but they may point to newly-created empty objs:
    //
    AssetRisks* arA =
      GetAssetRisksImpl(a_instr.m_AssetA, a_instr.m_SettlDate, 0, nullptr);
    assert(arA != nullptr && !(arA->IsEmpty()));

    SymKey      assetB = MkSymKey(a_instr.m_QuoteCcy.data());
    AssetRisks* arB    =
      GetAssetRisksImpl(assetB, a_instr.m_SettlDate, 0, nullptr);
    assert(arB != nullptr && !(arB->IsEmpty()));

    // NB: The Valuators for those AssetRisks must be set separately via the
    // "InstallValuator" method(s)...
    //-----------------------------------------------------------------------//
    // Find or Create the "InstrRisks":                                      //
    //-----------------------------------------------------------------------//
    // Find the existing "InstrRisks" or create a new one. The following call
    // always returns a non-NULL ptr (because UserID=0),   but if the obj was
    // just created, it is Empty and needs to be initialised:
    //
    InstrRisks* ir0 = GetInstrRisksImpl(a_instr, 0, a_ob, arA, arB);
    assert(ir0 != nullptr && !(ir0->IsEmpty()));

    //-----------------------------------------------------------------------//
    // Install this OrderBook in similar "InstrRisks" with UserID != 0:      //
    //-----------------------------------------------------------------------//
    // (They may remain in ShM from prev invocations. This would be done dyna-
    // mically as well, but we better install it now):
    //
    for (auto& irp: m_instrRisks)
    {
      UserID userID = irp.first;
      if (utxx::unlikely(userID == 0))
        continue;   // Already done!

      IRsMapI& mapI = irp.second;

      // XXX: Do NOT use "GetInstrRisksImpl" here -- we do not want to create
      // a new "InstrRisks" for a UserID which may not need it at all.  Use a
      // passive search:
      auto it =  mapI.find(a_instr.m_SecID);
      if  (it == mapI.end())
        continue;

      // If found: That "InstrRisks" should in general be non-Empty,   but we
      // check it for extra safety:
      InstrRisks& ir = it->second;
      if (utxx::likely(!ir.IsEmpty()))
      {
        ir.m_ob = a_ob;
        // Do NOT forget to add "ir" to the map as well:
        (*m_obirMap)[ir.m_ob].insert(&ir);
      }
    }
    //-----------------------------------------------------------------------//
    // All Done:                                                             //
    //-----------------------------------------------------------------------//
    LOG_INFO(2,
      "RiskMgr::Register: {} Registered with OrderBook={}",
      ir0->m_instr->m_FullName.data(),
      (ir0->m_ob == nullptr)
      ? "NULL"   :
      (&(ir0->m_ob->GetInstr()) == &a_instr)
      ? "Native" :
      ir0->m_ob->GetInstr().m_FullName.data())
  }

  //=========================================================================//
  // "Register" (OMC):                                                       //
  //=========================================================================//
  void RiskMgr::Register(EConnector_OrdMgmt* a_omc)
  {
    // NULL is not allowed:
    if (utxx::unlikely(a_omc == nullptr))
      throw utxx::badarg_error("RiskMgr::Register(OMC): NULL is not allowed");

    // Repeated registrations are ignored:
    if (utxx::unlikely
       (find(m_omcs.cbegin(), m_omcs.cend(), a_omc) != m_omcs.cend()))
      return;

    // Otherwise, register it if we still can:
    if (utxx::unlikely(m_omcs.size() >= m_omcs.capacity()))
      throw utxx::badarg_error
            ("RiskMgr::Register(OMC): Too many registered OMCs: Max=",
             m_omcs.capacity());
    // If OK:
    m_omcs.push_back(a_omc);
  }

  //=========================================================================//
  // "InstallValuator(OrderBooks)":                                          //
  //=========================================================================//
  // Installs a single Asset->RFC Valuator for the given Asset, using the Order-
  // Book(s) and Adjustment(s).   Also allows us to overwrite the SettlDate (eg
  // set it to 0 for "unattributed" SettlDates). Must be invoked before "Start",
  // because the latter verifies the validity of Valuators for all Assets:
  //
  void RiskMgr::InstallValuator
  (
    char const*          a_asset,
    int                  a_settl_date,
    OrderBookBase const* a_ob1,
    double               a_bid_adj1,
    double               a_ask_adj1,
    utxx::time_val       a_roll_over_time,
    OrderBookBase const* a_ob2,
    double               a_bid_adj2,
    double               a_ask_adj2
  )
  {
    if (utxx::unlikely(a_asset == nullptr || *a_asset == '\0'))
      throw utxx::badarg_error
            ("RiskMgr::InstallValuator(OrderBooks): NULL/Empty Asset");

    // NB: Install it for *ALL* UserIDs for which this Asset exists, noy just
    // for UserID=0:
    SymKey asset = MkSymKey(a_asset);

    for (auto& arp: m_assetRisks)
    {
      UserID   userID = arp.first;
      ARsMapI& mapI   = arp.second;

      // Search for this (Asset,SettlDate) in "mapI":
      auto it = mapI.find(make_pair(asset, a_settl_date));

      if (it == mapI.end())
      {
        if (utxx::unlikely(userID == 0))
          // If UserID is 0, then we assume the AssetRisks is ought to be found,
          // so we are in an error cond:
          throw utxx::logic_error
                ("RiskMgr::InstallValuator(OrderBooks): AssetRisks for ",
                 a_asset, " (SettlDate=", a_settl_date, "): Not Found for "
                 "UserID=0");
        else
          // It is perfectly OK that this AssetRisk may not exist for some
          // UserID != 0:
          continue;
      }
      AssetRisks& ar = it->second;

      // NB: further checks are performed by  by "AssetRisks::InstallValuator":
      ar.InstallValuator(a_ob1, a_bid_adj1, a_ask_adj1, a_roll_over_time,
                         a_ob2, a_bid_adj2, a_ask_adj2);

      // Map the non-NULL OrderBooks to this "AssetRisks":
      assert(m_obarMap != nullptr);

      if (utxx::likely(ar.m_evalOB1 != nullptr))
        (*m_obarMap)  [ar.m_evalOB1].insert(&ar);

      if (ar.m_evalOB2 != nullptr)
        (*m_obarMap)  [ar.m_evalOB2].insert(&ar);
    }
    // All Done!
  }

  //=========================================================================//
  // "InstallValuator(Fixed Asset/RFC Rate)":                                //
  //=========================================================================//
  // Installs a single Asset->RFC Valuator given by a Fixed Asset/RFC Rate:
  //
  void RiskMgr::InstallValuator
  (
    char const* a_asset,
    int         a_settl_date,
    double      a_fixed_rate
  )
  {
    // Checks:
    if (utxx::unlikely(a_asset == nullptr || *a_asset == '\0'))
      throw utxx::badarg_error
            ("RiskMgr::InstallValuator(FixedRate): NULL/Empty Asset");

    // NB: Install it for all UserIDs for which this Asset exists:
    SymKey asset = MkSymKey(a_asset);

    for (auto& arp: m_assetRisks)
    {
      UserID   userID = arp.first;
      ARsMapI& mapI   = arp.second;

      // Search for this (Asset,SettlDate) in "mapI":
      auto it = mapI.find(make_pair(asset, a_settl_date));

      if (it == mapI.end())
      {
        if (utxx::unlikely(userID == 0))
          // If UserID is 0, then we assume the AssetRisks is ought to be found,
          // so we are in an error cond:
          throw utxx::logic_error
                ("RiskMgr::InstallValuator(FixedRate): AssetRisks for ",
                 a_asset, " (SettlDate=", a_settl_date, "): Not Found for "
                 "UserID=0");
        else
          // It is perfectly OK that this AssetRisk may not exist for some
          // UserID != 0:
          continue;
      }
      AssetRisks& ar = it->second;

      // NB: Futher checks are performed by "AssetRisks::InstallValuator":
      ar.InstallValuator(a_fixed_rate);
    }
    // All Done!
  }

  //=========================================================================//
  // "RefreshThrottlers":                                                    //
  //=========================================================================//
  // To be invoked as frequently as reasonably possible, to improved the accu-
  // racy and efficiency of Order Entry and Fill Throttlers:
  //
  inline void RiskMgr::RefreshThrottlers(utxx::time_val a_ts) const
  {
    if (utxx::likely(!a_ts.empty()))
    {
      m_orderEntryThrottler1.refresh(a_ts);
      m_orderFillThrottler1 .refresh(a_ts);

      m_orderEntryThrottler2.refresh(a_ts);
      m_orderFillThrottler2 .refresh(a_ts);

      m_orderEntryThrottler3.refresh(a_ts);
      m_orderFillThrottler3 .refresh(a_ts);
    }
  }

  //=========================================================================//
  // "GetQtyA":                                                              //
  //=========================================================================//
  inline RMQtyA RiskMgr::GetQtyA
  (
    double          a_qty_raw,
    QtyTypeT        a_qt,
    SecDefD const&  a_instr,
    PriceT          a_px_ab
  )
  {
    assert(a_qty_raw > 0.0);

    switch (a_qt)
    {
    // First, if it is already a QtyA, return it as is:
    case QtyTypeT::QtyA:
      return RMQtyA(a_qty_raw);

    // On the other hand, if it is QtyB (XXX: there is currently no further de-
    // talisation here, so it must REALLY be QtyB itself,  not eg  "QtyBLots"),
    // then use the Px to calculate QtyA  -- any other data  are irrelevant in
    // this case:
    case QtyTypeT::QtyB:
    {
      // In this case, Px=0 is not allowed:
      CHECK_ONLY
      (
        if (utxx::unlikely(!IsFinite(a_px_ab) || IsZero(a_px_ab)))
          throw utxx::badarg_error("RiskMgr::GetQtyA: Invalid Px with QtyB");
      )
      return RMQtyA(a_qty_raw / double(a_px_ab));
    }

    // The remaining 2 cases: Contracts and Lots:
    case QtyTypeT::Contracts:
      return QtyConverter<QtyTypeT::Contracts, QtyTypeT::QtyA>::
             Convert<double, double>
             (Qty<QtyTypeT::Contracts, double>(a_qty_raw), a_instr, PriceT());

    case QtyTypeT::Lots:
      return QtyConverter<QtyTypeT::Lots,      QtyTypeT::QtyA>::
             Convert<double, double>
             (Qty<QtyTypeT::Lots, double>     (a_qty_raw), a_instr, PriceT());

    default:
      assert(false);
    }
    // This point is UNREACHABLE:
    return RMQtyA();
  }

  //=========================================================================//
  // "GetQtyB":                                                              //
  //=========================================================================//
  inline RMQtyB RiskMgr::GetQtyB
  (
    double          a_qty_raw,
    QtyTypeT        a_qt,
    PriceT          a_px_ab
  )
  {
    // As this method also applies to Fees, "a_qty_raw" can be of any sign,
    // unlike the arg of "GetQtyA"!
    switch (a_qt)
    {
    case QtyTypeT::QtyB:
      // If it is already a QtyB, return it as is:
      return RMQtyB(a_qty_raw);

    // QtyA can be converted to QtyB, provided that the Px is valid:
    case QtyTypeT::QtyA:
      CHECK_ONLY
      (
        if (utxx::unlikely(!IsFinite(a_px_ab)))
          throw utxx::badarg_error("RiskMgr::GetQtyB: Invalid Px with QtyA");
      )
      return RMQtyB(a_qty_raw * double(a_px_ab));

    // Any other cases are extremely unlikely here, and are treated as errors:
    default:
      throw utxx::logic_error
            ("RMQtyB RiskMgr::GetQtyB: QT=", int(a_qt), ": UnSupported");
    }
    // This point is REALLY unreachable:
    __builtin_unreachable();
  }

  //=========================================================================//
  // "OnMktDataUpdate":                                                      //
  //=========================================================================//
  void RiskMgr::OnMktDataUpdate
  (
    OrderBookBase const& a_ob,
    utxx::time_val       a_ts   // ExchTS if known, LocalTS otherwise
  )
  {
    //-----------------------------------------------------------------------//
    // Check the Args:                                                       //
    //-----------------------------------------------------------------------//
    RefreshThrottlers(a_ts);

    // In the Relaxed mode, MktData updates are ignored altogether, because in
    // that case, we maintain only positions, but not PnL or UnRPnL:
    //
    if (utxx::unlikely(!m_isActive || m_mode == RMModeT::Relaxed))
      // It is safe to just ignore a MktData update in that case:
      return;

    //-----------------------------------------------------------------------//
    // Get the Bid and Ask Pxs:                                              //
    //-----------------------------------------------------------------------//
    PriceT rawBidPx = a_ob.GetBestBidPx();
    PriceT rawAskPx = a_ob.GetBestAskPx();

    // If for any reason the update is not valid, return now:
    if (utxx::unlikely(!(rawBidPx < rawAskPx)))
      return;

    // We will need to convert "raw{Bid|Ask}Px" into the "{bid|ask}PxAB", that
    // is, Px per 1 unit of A and expressed in units of B (rather than abstract
    // Pts as it may happen):
    //
    SecDefD const&   instr = a_ob.GetInstr();
    PriceT bidPxAB = instr.GetPxAB(rawBidPx);
    PriceT askPxAB = instr.GetPxAB(rawAskPx);

    //-----------------------------------------------------------------------//
    // Frequency Control (XXX: Uniform across all OrderBooks!)               //
    //-----------------------------------------------------------------------//
    // If Frequency Control of MktData Updates is enabled, and the next period
    // is not reached yet, return.  We perform this check not at the beginning
    // but when we are really in for an update:
    //
    if (!m_nextMDUpdate.empty() && m_mdUpdatesPeriodMSec > 0)
    {
      // NB: Do NOT use "a_ts" for MktDataUpdates periodicity, as they come from
      // DIFFERENT instrs, and may be non-monotonic. Use Real UTC instead:
      utxx::time_val now = utxx::now_utc();
      if (now < m_nextMDUpdate)
        return;
      // Otherwise, we can proceed with the update -- move the next period fwd
      // beyond the curr time:
      utxx::msecs mdUpdatesPeriod(m_mdUpdatesPeriodMSec);
      for (; m_nextMDUpdate <= now; m_nextMDUpdate += mdUpdatesPeriod);
    }
    // In the following, we memoise the affected UserIDs:
    m_affectedUserIDs->clear();

    //-----------------------------------------------------------------------//
    // Update "AssetRisks" affected by this OrderBook tick:                  //
    //-----------------------------------------------------------------------//
    // This is to be done first, as Asset->RFC rates computed here may subsequ-
    // ently be re-used in "InstrRisks" RFC valuations:
    //
    assert(m_obarMap != nullptr);
    auto itAR = m_obarMap->find(&a_ob);

    // NB: Bogus ticks CAN happen, and are in general harmless (except for some
    // minor inefficiencies):
    if (utxx::likely(itAR != m_obarMap->cend() && !(itAR->second.empty())))
    {
      // Generic Case: Found the set of "AssetRisks" affected by this OrderBook
      // as an Asset->RFC Valuator:
      set<AssetRisks*> const& ars = itAR->second;

      for (AssetRisks* ar: ars)
      {
        assert(ar != nullptr);
        // Again, for extra safety, guard against Empty "AssetRisks":
        if (utxx::likely(!(ar->IsEmpty())))
        {
          ar->OnValuatorTick       (a_ob,   a_ts);
          m_affectedUserIDs->insert(ar->m_userID);
        }
        else
          LOG_ERROR(1,
            "RiskMgr::OnMktDataUpdate: OrderBook={}: Target AssetRisks Empty",
            a_ob.GetInstr().m_FullName.data())
          // But still continue!
      }
    }
    //-----------------------------------------------------------------------//
    // Update "InstrRisks" affected by this OrderBook tick:                  //
    //-----------------------------------------------------------------------//
    assert(m_obirMap != nullptr);
    auto itIR = m_obirMap->find(&a_ob);

    // NB: Again, check for "bogus" ticks:
    if (utxx::likely(itIR != m_obirMap->cend() && !(itIR->second.empty())))
    {
      // Generic Case: Found the set of "InstrRisks" affected by this OrderBook
      // as UnRealisedPnL Valuator:
      set<InstrRisks*> const& irs = itIR->second;

      for (InstrRisks* ir: irs)
      {
        assert(ir != nullptr);
        // For extra safety, guard against Empty "InstrRisks":
        if (utxx::likely(!(ir->IsEmpty())))
        {
          ir->OnMktDataUpdate      (a_ob, bidPxAB, askPxAB, a_ts);
          m_affectedUserIDs->insert(ir->m_userID);
        }
        else
          LOG_ERROR(1,
            "RiskMgr::OnMktDataUpdate: OrderBook={}: Target InstrRisks Empty",
            a_ob.GetInstr().m_FullName.data())
          // But still continue!
      }
    }
    //-----------------------------------------------------------------------//
    // Now process the AffectedUserIDs:                                      //
    //-----------------------------------------------------------------------//
    if (m_mode != RMModeT::STP   && m_mode != RMModeT::Safe)
      for (UserID userID: *m_affectedUserIDs)
      {
        ReCalcRFCTotals(userID);

        //-------------------------------------------------------------------//
        // Any Limit(s) Exceeded?                                            //
        //-------------------------------------------------------------------//
        // FIXME: If a Limit is violated on some UserAccount, it should NOT im-
        // ply entering SafeMode over-all:
        if (utxx::unlikely
           (m_totalRiskRFC        [userID] > m_MaxTotalRiskRFC        ||
            m_totalActiveOrdsSzRFC[userID] > m_MaxActiveOrdsTotalSzRFC))
          EnterSafeMode("OnMktDataUpdate", "Risk Limit(s) violation");
      }
    // All Done!
  }

  //=========================================================================//
  // "ReCalcRFCTotals":                                                      //
  //=========================================================================//
  // Invoked after a Trade or Valuator Tick. Updates TotalRiskRFC, TotalNAV_RFC
  // and TotalActiveOrdsSzRFC for a given UserID.
  // XXX: All Totals are re-calculated by direct summation from 0. This is HUGE-
  // LY inefficient for NAV and ActiveIrdsSz (a differential update woule be
  // much better), but OK for Risk (Exposure) which is NON-Linear anyway. So we
  // curretly opt for full re-calculation of all Totals:
  //
  inline void RiskMgr::ReCalcRFCTotals(UserID a_user_id) const
  {
    //-----------------------------------------------------------------------//
    // Do nothing in the Relaxed Mode:                                       //
    //-----------------------------------------------------------------------//
    if (m_mode == RMModeT::Relaxed)
      return;

    //-----------------------------------------------------------------------//
    // TotalRiskRFC and TotalNAV_RFC from "AssetRisks":                      //
    //-----------------------------------------------------------------------//
    auto itAR = m_assetRisks.find(a_user_id);

    if (utxx::unlikely(itAR == m_assetRisks.cend()))
    {
      // This must NOT happen:
      LOG_ERROR(1,
        "RiskMgr::ReCalcRFCTotals: AssetRisks for UserID={} Not Found",
        a_user_id)
    }
    else
    {
      // Generic Case:
      double totalRiskRFC = 0.0;
      double totalNAV_RFC = 0.0;
      ARsMapI const& mapI = itAR->second;

      for (auto const& arp: mapI)
      {
        AssetRisks const& ar = arp.second;
        assert(ar.m_userID  == a_user_id);

        // Update the Total Pos and NAV RFC (algebraic summation is used here):
        double netTotalRFC = ar.GetNetTotalRFC();
        totalNAV_RFC += netTotalRFC;

        // Update the Total Risk (RFC), if this Asset is NOT RFC itself:  Here
        // we use the Abs value for the worst-case exposure: TODO: Correlation
        // Matrices!
        if (utxx::likely(!ar.m_isRFC))
          totalRiskRFC += Abs(netTotalRFC);
      }
      // NB: Risks are of course non-negative, but NAV can be any:
      assert(totalRiskRFC >= 0.0);

      // Store the computed Totals back:
      m_totalRiskRFC[a_user_id] = totalRiskRFC;
      m_totalNAV_RFC[a_user_id] = totalNAV_RFC;
    }
    //-----------------------------------------------------------------------//
    // TotaActiveOrdsSzRFC from "InstrRisks" (Non-STP only):                 //
    //-----------------------------------------------------------------------//
    if (m_mode != RMModeT::STP)
    {
      auto itIR = m_instrRisks.find(a_user_id);

      if (utxx::unlikely(itIR == m_instrRisks.cend()))
      {
        // This must NOT happen:
        LOG_ERROR(1,
          "RiskMgr::ReCalcRFCTotals: InstrRisks for UserID={} Not Found",
          a_user_id)
      }
      else
      {
        double totalActiveOrdsSzRFC = 0.0;
        IRsMapI const& mapI         = itIR->second;

        for (auto const& irp: mapI)
        {
          InstrRisks const& ir =  irp.second;
          assert(ir.m_userID   == a_user_id && ir.m_activeOrdsSzRFC >= 0.0);
          totalActiveOrdsSzRFC += ir.m_activeOrdsSzRFC;
        }
        // Store the computed Total back:
        assert(totalActiveOrdsSzRFC >= 0.0);
        m_totalActiveOrdsSzRFC[a_user_id] = totalActiveOrdsSzRFC;
      }
    }
    // All Done!
  }

  //=========================================================================//
  // "OnTrade":                                                              //
  //=========================================================================//
  void RiskMgr::OnTrade(Trade const& a_tr)
  {
    //-----------------------------------------------------------------------//
    // Check the Args:                                                       //
    //-----------------------------------------------------------------------//
    // The Time Stamp:
    utxx::time_val ts =
      (!a_tr.m_exchTS.empty()) ? a_tr.m_exchTS : a_tr.m_recvTS;
    assert(!ts.empty());

    assert(IsFinite(a_tr.m_px) && !ts.empty() && a_tr.m_instr != nullptr);
    RefreshThrottlers(ts);

    // The Instrument traded:
    SecDefD const& instr = *a_tr.m_instr;

    // UserID:
    UserID userID  = a_tr.m_accountID;

    //-----------------------------------------------------------------------//
    // Req12, AOS and OMC (Non-STP Modes Only):                              //
    //-----------------------------------------------------------------------//
    // Only known if it was OUR Trade, ie not in STP Mode:
    //
    AOS          const* aos = nullptr;
    EConnector_OrdMgmt* omc = nullptr;

    if (m_mode != RMModeT::STP)
    {
      Req12 const* req = a_tr.m_ourReq;
      assert(req != nullptr);

      aos =  req->m_aos;
      assert(aos != nullptr);

      // XXX: For our own Trades, UserID is assumed to be 0:
      if (utxx::unlikely(userID != 0))
        LOG_WARN(2, "RiskMgr::OnTrade: UnExpected UserID={}", userID)

      // The OMC: Should be registered with the RiskMgr:
      omc = aos->m_omc;
      assert(omc != nullptr);

      bool omcRegistered =
        find(m_omcs.cbegin(), m_omcs.cend(), omc) != m_omcs.cend();

      if (utxx::unlikely(!(m_isActive && omcRegistered)))
        // XXX: This is an unpleasant situation: The RiskMgr is not active  yet
        // (perhaps did not initialise the OrderBooks used in Asset->RFC Valuat-
        // ors...), or the OMC was not registered, but "SOMEHOW" we already got
        // a Trade event! Most likely, it is a delayed msg from a previous invo-
        // cation. We will have to handle it anyway to update our positions pro-
        // perly:
        LOG_ERROR(2,
          "RiskMgr::OnTrade: OMC={}{}{}: But have to proceed anyway...",
          omc->GetName(),
          m_isActive    ? "" : ": Not Active yet",
          omcRegistered ? "" : ": Not Registered")
    }
    //-----------------------------------------------------------------------//
    // Get the PxAB and QtyA for this Trade:                                 //
    //-----------------------------------------------------------------------//
    PriceT trPxAB = instr.GetPxAB(a_tr.m_px);

    // The TradedQty as an UNDEFINED "double" (with ConvReps=true); its actual
    // semantics is given by "a_tr.m_qt", ie it could be Contracts, Lots, QtyA
    // or QtyB:
    double trQty  = double(a_tr.GetQty<QtyTypeT::UNDEFINED,double,true>());

    // Now convert it dynamically into QtyA:
    RMQtyA qtyA   = GetQtyA(trQty, a_tr.m_qt, instr, trPxAB);

    // This "qtyA" should of course be positive. We refuse to process invalid
    // qtys:
    if (utxx::unlikely(!IsPos(qtyA)))
    {
      LOG_ERROR(1,
        "RiskMgr::OnTrade: TradeID={}, Instr={}, TrQty={}:  Got QtyA={}",
        a_tr.m_execID.ToString(), instr.m_FullName.data(),  trQty,
        double(qtyA))
      return;
    }
    // Similarly, convert the Fees/Commission into QtyB: NB: Fees are always
    // considered to be Fractional, with the semantics given by "a_tr.m_qf",
    // but for extra safety, we set ConvREps=true:
    //
    double fee    = double(a_tr.GetFee<QtyTypeT::UNDEFINED,double,true>());
    RMQtyB feeB   = GetQtyB(fee, a_tr.m_qf, trPxAB);

    //-----------------------------------------------------------------------//
    // Update the "InstrRisks":                                              //
    //-----------------------------------------------------------------------//
    InstrRisks* ir =
      GetInstrRisksImpl(instr, userID, nullptr, nullptr, nullptr);

    if (utxx::unlikely(ir == nullptr || ir->IsEmpty()))
    {
      // This is a VERY SERIOUS error condition  -- we got a trade for a non-
      // configured Instrument. (NB: a previously-unknown UserID is installed
      // automatically by "GetInstrRisksImpl").   This warrants an immediate
      // shut-down because we cannot update  our positions and risks anymore.
      // ("ir", if non-NULL, should not be Empty, but we ptrotect afainst that
      // case anyway):
      char    what[128];
      sprintf(what, "UnExpected Trade in %s: Missing/Empty InstrRisks",
              instr.m_FullName.data());

      // NB: If we are in STP or Safe mode already, this will also result in
      // logging the error:
      EnterSafeMode("OnTrade", what);
      return;
    }

    // Get the "AssetRisks":
    AssetRisks* arA = ir->m_risksA;
    AssetRisks* arB = ir->m_risksB;
    assert(arA != nullptr && arB != nullptr);

    // For extra safety, check that the "AssetRisks" are not Empty:
    if (utxx::unlikely(arA->IsEmpty() || arB->IsEmpty()))
    {
      char    what[256];
      sprintf(what, "TradeID=%s, InstrRisks=%s: Got Empty AssetRisks(A/B)",
              a_tr.m_execID.ToString(), instr.m_FullName.data());
      // NB: See above re STP or Safe mode already:
      EnterSafeMode("OnTrade", what);
      return;
    }

    // If any "AssetRisks" has the Epoch later than this Trade, skip this
    // update altogether -- this is quote normal:
    if (utxx::unlikely
       (!(a_tr.m_exchTS.empty()) && !(arA->m_epoch.empty()) &&
        !(arB->m_epoch .empty()) &&
         (a_tr.m_exchTS < arA->m_epoch || a_tr.m_exchTS < arB->m_epoch) ))
    {
      LOG_INFO(2,
        "RiskMgr::OnTrade: TradeID={}: Skipped: ExchTS={}.{}, Epoch({})={}.{}, "
        "Epoch({})={}.{}",
        a_tr.m_execID.ToString(), a_tr.m_exchTS.sec(), a_tr.m_exchTS.usec(),
        arA->m_asset.data(),      arA->m_epoch.sec(),  arA->m_epoch.usec(),
        arB->m_asset.data(),      arB->m_epoch.sec(),  arB->m_epoch.usec())
      return;
    }
    //-----------------------------------------------------------------------//
    // Generic Case: Get the Trade Side:                                     //
    //-----------------------------------------------------------------------//
    bool isBuy = false;
    if (aos != nullptr)
    {
      // Non-STP mode: We have AOS:
      assert(m_mode != RMModeT::STP);
      isBuy = aos->m_isBuy;
    }
    else
    {
      // Otherwise, the Side (from the User's point of view) must come in the
      // Trade:
      assert(m_mode == RMModeT::STP);
      if (utxx::unlikely
         (a_tr.m_accountID == 0 || a_tr.m_accSide == FIX::SideT::UNDEFINED))
      {
        // Again a VERY SERIOUS error condition -- cannot determine the Side of
        // the Trade. Log it and do not proceed (there is very little extra  we
        // can do in the STP Mode):
        LOG_ERROR(1,
          "RiskMgr::OnTrade: TrdID={}: Missing AccountID or TradeSide in STP "
          "Mode", a_tr.m_execID.ToString())
        return;
      }
      isBuy = (a_tr.m_accSide == FIX::SideT::Buy);
    }
    //-----------------------------------------------------------------------//
    // Update the "InstrRisks" and, transitively, both "AssetRisks":         //
    //-----------------------------------------------------------------------//
    // In non-Realxed, non-STP modes, this also reduces the size of currently-
    // Active Orders in this Instrument -- no need to invoke "OnOrder" separa-
    // tely; "tradeSzRFC" is the absolute size of this Trade in RFC:
    //
    double tradeSzRFC =
      ir->OnTrade(a_tr.m_execID, isBuy, trPxAB, qtyA, feeB, ts);

    // XXX: The rest is NOT performed in the Relaxed mode:
    if (m_mode == RMModeT::Relaxed)
      return;

    //-----------------------------------------------------------------------//
    // Re-calculate Total RFC NAV and TotalRiskRFC:                          //
    //-----------------------------------------------------------------------//
    // Only 2 Assets have been affected (arA and arB), and their RFCs were al-
    // ready re-calculated above,  so only the Totals need to be re-done, and
    // there is no ValuatorTick:
    //
    ReCalcRFCTotals(userID);

    //-----------------------------------------------------------------------//
    // Any Limits Exceeded?                                                  //
    //-----------------------------------------------------------------------//
    // FIXME: Again, "SafeMode" should apply to the misbehaving UserID only,
    // NOT to all of them:
    //
    if (m_mode != RMModeT::STP && m_mode != RMModeT::Safe)
    {
      // Update the Fill Rate and check for abnormal fill activity. XXX: It may
      // be safer to do it earlier -- immediately when "arB" is found:
      // XXX: The count in "add" is "int", so cannot add more than $2bn at once!
      //
      int trSzInt =
        (tradeSzRFC < double(INT_MAX)) ? int(Round(tradeSzRFC)) : INT_MAX;

      // Update the Order Fill Throttlers:
      m_orderFillThrottler1.add (ts, trSzInt);
      m_orderFillThrottler2.add (ts, trSzInt);
      m_orderFillThrottler3.add (ts, trSzInt);

      // Now check for possible abnormal activity:
      if (utxx::unlikely
         (m_orderFillThrottler1.running_sum() > m_VlmLimitRFC1 ||
          m_orderFillThrottler2.running_sum() > m_VlmLimitRFC2 ||
          m_orderFillThrottler3.running_sum() > m_VlmLimitRFC3))
        EnterSafeMode("OnTrade", "Abnormal Order Fill Rate Detected");

      // NB: Only use NAV2; NAV1 is Instrument-based (not Ccy-based), so it's
      // for information only:
      if (utxx::unlikely
         (m_totalRiskRFC[userID] > m_MaxTotalRiskRFC ||
          m_totalNAV_RFC[userID] < m_MinTotalNAV_RFC))
        EnterSafeMode("OnTrade", "Risk Limit(s) violation");
    }
    // All Done!
  }

  //=========================================================================//
  // "OnOrder":                                                              //
  //=========================================================================//
  // NB:
  // (*) Can actually be inoked BEFORE a New Order or Order Modification Req is
  //     sent to the Exchange (IsReal = true):
  // (*) Also invoked when an Order is Cancelled, Rejected, Filled or Part-Fil-
  //     led, to re-calculate the Active Orders Size (IsReal = false):
  // (*) "a_new_qty", "a_old_qty" are NOT positions -- they are Qtys of this
  //     Order (as given by the corresp AOS) across a New/Cancel/Modify/Unwind
  //     operation, with their common semantics given by "a_qt":
  // (*) "a_new_px" and "a_old_px" are only required in rare cases when QT=QTyB,
  //     as in that case, they would be used in computation of NewQtyA, OldQtyA:
  //
  template<bool IsReal>
  void RiskMgr::OnOrder
  (
    EConnector_OrdMgmt const* CHECK_ONLY(a_omc),
    SecDefD const&            a_instr,
    bool                      a_is_buy,
    QtyTypeT                  a_qt,
    PriceT                    a_new_px,     // 0.0  if Cancel, NaN for MktOrd
    double                    a_new_qty,    // 0.0  if Cancel;   mb ignored
    PriceT                    a_old_px,     // 0.0  if NewOrder
    double                    a_old_qty,    // 0.0  if NewOrder; mb ignored
    utxx::time_val            a_ts          // If empty,  it's a removal;
  )                                         // Otherwise, LocalTS
  {
    //-----------------------------------------------------------------------//
    // Check the Args:                                                       //
    //-----------------------------------------------------------------------//
    RefreshThrottlers(a_ts);

    // NB: Because it is OUR OWN ORDER, we assume userID=0:
    constexpr UserID userID = 0;

    CHECK_ONLY
    (
      // Check the OMC:
      assert(a_omc != nullptr);
      if (utxx::unlikely
         (find(m_omcs.cbegin(), m_omcs.cend(), a_omc) == m_omcs.cend()))
      {
        // This OMC was not registered. If it is an internal call, we will log
        // an error but still proceed:
        if (!IsReal)
        {
          LOG_ERROR(2,
            "RiskMgr::OnOrder(Intern): OMC={} was not registered, but have to "
            "proceed anyway...", a_omc->GetName())
        }
        else
          throw utxx::badarg_error
                ("RiskMgr::OnOrder: OMC=", a_omc->GetName(),
                 ": Not registered");
      }
      // If it is a "Real" order (not an internal call) and we are in the Safe
      // Mode, the order will NOT be allowed:
      if (IsReal && (m_mode == RMModeT::Safe || m_mode == RMModeT::STP))
        throw utxx::runtime_error
              ("RiskMgr::OnOrder: Not allowed in STP or Safe Mode");

      if (utxx::unlikely(IsReal && !m_isActive))
        // NB: Do not just return in this case -- if the RiskMgr is enabled but
        // not active yet, New Orders or Modifications are strictly disallowed!
        throw utxx::runtime_error
              ("RiskMgr::OnOrder: Not active yet, Orders Disallowed");

      // For Qtys, 0s may sometimes be allowed, but not negative vals -- once
      // again, those qtys are NOT positions!
      if (utxx::unlikely(a_old_qty < 0.0))
        throw utxx::badarg_error("RiskMgr::OnOrder: Invalid OldQty");
    )
    //-----------------------------------------------------------------------//
    // Get the InstrRisks and OrderBook:                                     //
    //-----------------------------------------------------------------------//
    // NB: An exception is thrown if "InstrRisks" is not found or is Empty (the
    // latter should not happen in any case, bit we guard against that anyway):
    //
    InstrRisks* ir =
      GetInstrRisksImpl(a_instr, userID, nullptr, nullptr, nullptr);

    if (utxx::unlikely(ir == nullptr || ir->IsEmpty()))
      throw utxx::badarg_error
            ("RiskMgr::OnOrder: Invalid Instr=", a_instr.m_FullName.data(),
             ", UserID=0: Missing/Empty InstrRisks");

    // XXX: If NewPx is NaN (as it would be for a Mkt Order), we still need some
    // value -- so will have to extract it from the OrderBook (which incurs ext-
    // ra latency, of course!)
    OrderBookBase const* ob = ir->m_ob;

    // NB: "ob" must NOT be NULL; in general, it is allowed to be NULL, but only
    // in the STP mode, in which case this method is not invoked at all:
    if (utxx::unlikely(ob == nullptr))
      throw utxx::badarg_error
            ("RiskMgr::OnOrder: ", a_instr.m_FullName.data(),
             ": OrderBook must NOT be NULL");

    //-----------------------------------------------------------------------//
    // Verify and Normalise the Pxs (only for QT=QtyB, otherwise NaN):       //
    //-----------------------------------------------------------------------//
    PriceT newPxAB; // NaN, unless modified below
    PriceT oldPxAB; // ditto

    if (a_qt == QtyTypeT::QtyB)
    {
      // NewPx may be NaN for a New Mkt Order,  but then (OldQty, OldPx)  must
      // be (0.0, 0.0).
      CHECK_ONLY
      (
        if (utxx::unlikely
           (!IsFinite(a_old_px) ||
           (!IsFinite(a_new_px) && (a_old_qty != 0 || double(a_old_px) != 0)) ))
          throw utxx::badarg_error("RiskMgr::OnOrder: Invalid Px(s)");
      )
      if (utxx::unlikely(!IsFinite(a_new_px)))
      {
        // This is a Mkt Order -- so will use aggressive pxs. XXX: Obviously,
        // they are not VWAP-adjusted, for efficiency:
        a_new_px = a_is_buy ? (ob->GetBestAskPx()) : (ob->GetBestBidPx());

        // If, still, have not got a valid price -- cannot proceed:
        if (utxx::unlikely(!IsFinite(a_new_px)))
          throw utxx::runtime_error
                ("RiskMgr::OnOrder: Cannot get the Order Px for ",
                 a_instr.m_FullName.data());
      }
      // TODO:
      // Verify "a_new_px" against the curr Mkt Pxs given by the "ob" -- orders
      // with seriously wrong Pxs, should NOT be allowed!
      //
      // Now Normalise the Pxs (to be A/B Pxs):
      newPxAB   = a_instr.GetPxAB(a_new_px);
      oldPxAB   = a_instr.GetPxAB(a_old_px);
    }
    //-----------------------------------------------------------------------//
    // Normalise the Qtys:                                                   //
    //-----------------------------------------------------------------------//
    // Old and New Qtys are to be expressed in A units:
    RMQtyA newQtyA = GetQtyA(a_new_qty, a_qt, a_instr, newPxAB);
    RMQtyA oldQtyA = GetQtyA(a_old_qty, a_qt, a_instr, oldPxAB);

    //-----------------------------------------------------------------------//
    // Get the AssetA Risk:                                                  //
    //-----------------------------------------------------------------------//
    // XXX: Here we require Readiness, not just non-Emptiness:
    AssetRisks* arA  = ir->m_risksA;
    assert(arA != nullptr);
    if (utxx::unlikely(arA->IsEmpty()))
      throw utxx::logic_error
            ("RiskMgr::OnOrder: InstrRisks=", a_instr.m_FullName.data(),
             ": Empty AssetA");

    // Delta (signed) of the Order Qtys: FullReCalc=false:
    RMQtyA diffSzA   = newQtyA - oldQtyA;
    double diffSzRFC = 0.0;
    arA->ToRFC<false>(double(diffSzA), a_ts, &diffSzRFC);

    //-----------------------------------------------------------------------//
    // If this is a Real Order (not Unwinding), check the Volume Limits:     //
    //-----------------------------------------------------------------------//
    // XXX: A limit violation here does not lead to an immediate SafeMode, be-
    // cause nothing has happened yet.  Rather, we propagate an exception back
    // to the Caller, and it will prevent the Order from being sent:
    //
    if (IsReal)
    {
      // In this case, we must have (at the very least) a valid NewQtyA, and a
      // valid TimeStamp (so that the Throttlers could be updated):
      //
      if (utxx::unlikely(IsZero(newQtyA) || a_ts.empty()))
        throw utxx::badarg_error("RiskMgr::OnOrder(Real): Empty NewQty or TS");
      assert(IsPos(newQtyA) && !IsNeg(oldQtyA));

      //---------------------------------------------------------------------//
      // Check the (Real) Target Order Size (for this Order alone):          //
      //---------------------------------------------------------------------//
      // XXX: It the Qtys are the same (eg only the Pxs have changed), we  do
      // NOT re-calculate the order size expressed in RFC, because the RFC val
      // of its qtyA does not change at this point:   rarher,  it changes when
      // "OnMktDataUpdate" is invoked:
      //
      if (newQtyA != oldQtyA)
      {
        double newSzRFC =  0.0;
        arA->ToRFC<false>(double(newQtyA), a_ts, &newSzRFC);
        assert(newSzRFC >= 0.0);     // Beware of Swaps!

        if (utxx::unlikely
          (newSzRFC >  m_MaxOrdSzRFC ||
          (newSzRFC != 0 && newSzRFC < m_MinOrdSzRFC)))
          throw utxx::runtime_error
                ("RiskMgr::OnOrder: OrderSize=$", newSzRFC,
                 " is outside Limits: [$",        m_MinOrdSzRFC, " .. $",
                 m_MaxOrdSzRFC, ']');
      }
      //---------------------------------------------------------------------//
      // Check the (Real) Order Entry Volume Rate:                           //
      //---------------------------------------------------------------------//
      // NB:
      // (*) It only applies to New Orders or Enlargements (expressed in RFC):
      // (*) The count in "add" in the Throttlers is an "int",  so cannot add
      //     more than $2bn!
      //
      int diffSzInt =
        (diffSzRFC > 0.0)
        ? (utxx::likely(diffSzRFC < double(INT_MAX))
          ? int(Round(diffSzRFC)) : INT_MAX)
        : 0;

      if (diffSzInt > 0)
      {
        // Update the Throttlers:
        m_orderEntryThrottler1.add (a_ts, diffSzInt);
        m_orderEntryThrottler2.add (a_ts, diffSzInt);
        m_orderEntryThrottler3.add (a_ts, diffSzInt);

        // Now check for signs of abnormal activity:
        if (utxx::unlikely
           (m_orderEntryThrottler1.running_sum() > m_VlmLimitRFC1 ||
            m_orderEntryThrottler2.running_sum() > m_VlmLimitRFC2 ||
            m_orderEntryThrottler3.running_sum() > m_VlmLimitRFC3))
        {
          LOG_ERROR(1,
            "RiskMgr::OnOrder: OrderEntryVolume Exceeded:"
            "\nPeriod1={} sec, Limit=${}, Actual=${}"
            "\nPeriod2={} sec, Limit=${}, Actual=${}"
            "\nPeriod3={} sec, Limit=${}, Actual=${}",
            m_VlmThrottlPeriod1, m_VlmLimitRFC1,
            m_orderEntryThrottler1.running_sum(),
            m_VlmThrottlPeriod2, m_VlmLimitRFC2,
            m_orderEntryThrottler2.running_sum(),
            m_VlmThrottlPeriod3, m_VlmLimitRFC3,
            m_orderEntryThrottler3.running_sum())

          throw utxx::runtime_error
                ("RiskMgr::OnOrder: OrderEntryVolume Exceeded");
        }
      }
      //---------------------------------------------------------------------//
      // If we got here:                                                     //
      //---------------------------------------------------------------------//
      // The Order is allowed to go ahead, so increment the counter in the
      // InstrRisks:
      (ir->m_ordsCount)++;
    }
    // End IsReal
    //-----------------------------------------------------------------------//
    // In all cases: Re-calculate the Active Orders Size:                    //
    //-----------------------------------------------------------------------//
    // XXX: We do NOT perform full re-calculation of "m_totalActiveOrdsSzRFC":
    // Differential update is sufficient here, because there was just 1 Order:
    //
    if (utxx::likely(diffSzA != 0 || diffSzRFC != 0.0))
    {
      ir->m_activeOrdsSzA     += diffSzA;
      if (utxx::unlikely(ir->m_activeOrdsSzA < 0))
        ir->m_activeOrdsSzA    = RMQtyA();  // 0

      ir->m_activeOrdsSzRFC   += diffSzRFC;
      if (utxx::unlikely(ir->m_activeOrdsSzRFC  < 0.0))
        ir->m_activeOrdsSzRFC  = 0.0;

      m_totalActiveOrdsSzRFC  [userID] += diffSzRFC;
      if (utxx::unlikely(m_totalActiveOrdsSzRFC[userID] < 0.0))
        m_totalActiveOrdsSzRFC[userID] =  0.0;
    }
    // All Done!
  }

  //=========================================================================//
  // "OnBalanceUpdate":                                                      //
  //=========================================================================//
  // XXX: This is a special method implemented to support the STP Mode. Process
  // an update from some external Balance src, typically  a Transfer  into  the
  // account given by UserID:
  //
  void RiskMgr::OnBalanceUpdate
  (
    char const*             a_asset,
    int                     a_settl_date,
    char const*             a_trans_id,
    UserID                  a_user_id,
    AssetRisks::AssetTransT a_trans_t,
    double                  a_new_total,
    utxx::time_val          a_ts_exch    // May be empty
  )
  {
    if (utxx::unlikely
       (a_asset    == nullptr || *a_asset    == '\0' ||
        a_trans_id == nullptr || *a_trans_id == '\0' || a_new_total < 0.0))
    {
      LOG_ERROR(1, "RiskMgr::OnBalanceUpdate: Empty/Invalid Arg(s)")
      return;
    }
    // Generic Case: Find this Asset (XXX: Thus involves string copying into
    // SymKey):
    SymKey asset = MkSymKey(a_asset);
    AssetRisks* ar =
      const_cast<RiskMgr*>(this)->GetAssetRisksImpl
        (asset, a_settl_date, a_user_id, nullptr);

    if (utxx::unlikely(ar == nullptr || ar->IsEmpty()))
    {
      LOG_ERROR(1,
        "RiskMgr::OnBalanceUpdate: Asset={} (SettlDate={}), UserID={}: Not "
        "Found or Empty", a_asset, a_settl_date, a_user_id)
      return;
    }
    // If OK: Proceed:
    ar->OnBalanceUpdate(a_trans_id, a_trans_t, a_new_total, a_ts_exch);
  }

  //=========================================================================//
  // "GetInstrRisks":                                                        //
  //=========================================================================//
  // Can be used to obtain Position, and (unless de-configured) AvgPosPx,  UnR-
  // PnL and RealisedPnL for a given Instrument. NB: A Strategy can obtain this
  // ref once, and then re-examine it periodically:
  //
  InstrRisks const& RiskMgr::GetInstrRisks
    (SecDefD const& a_instr, UserID a_user_id) const
  {
    // Use "GetInstrRisksImpl":
    InstrRisks const* ir =
      const_cast<RiskMgr*>(this)->
      GetInstrRisksImpl(a_instr, a_user_id, nullptr, nullptr, nullptr);

    if (utxx::unlikely(ir == nullptr || ir->IsEmpty()))
      throw utxx::badarg_error
            ("RiskMgr::GetInstrRisks: Missing/Empty: ",
             a_instr.m_FullName.data(), ", UserID=", a_user_id);
    // If OK:
    return *ir;
  }

  //=========================================================================//
  // "GetAssetRisks":                                                        //
  //=========================================================================//
  // Can be used to obtain a per-Asset position (exposure), etc; again, a Stra-
  // tegy can obtain  this ref once, and then re-examine it periodically. This
  // is an external (throwing) version:
  //
  AssetRisks const& RiskMgr::GetAssetRisks
    (char const* a_asset, int a_settl_date, UserID a_user_id) const
  {
    if (utxx::unlikely(a_asset == nullptr || *a_asset == '\0'))
      throw utxx::badarg_error("RiskMgr::GetAssetRisks: NULL/Empty Asset");

    // Use "GetAssetRisksImpl":
    SymKey asset = MkSymKey(a_asset);
    AssetRisks const* ar =
      const_cast<RiskMgr*>(this)->
      GetAssetRisksImpl(asset, a_settl_date, a_user_id,   nullptr);

    if (utxx::unlikely(ar == nullptr || ar->IsEmpty()))
      throw utxx::badarg_error
            ("RiskMgr::GetAssetRisks: Not Found or Empty: Asset=", a_asset,
             ", SettlDate=", a_settl_date, ", UserID=",   a_user_id);
    // If OK:
    return *ar;
  }

  //=========================================================================//
  // "GetInstrRisksImpl":                                                    //
  //=========================================================================//
  // For internal use only; non-throwing, returns NULL if "InstrRisks" was not
  // found and cannot be initialised:
  //
  InstrRisks* RiskMgr::GetInstrRisksImpl
  (
    SecDefD       const& a_instr,
    UserID               a_user_id,
    OrderBookBase const* a_ob,     // May be NULL
    AssetRisks*          a_ar_a,   // May be NULL (non-NULL only for UserID=0)
    AssetRisks*          a_ar_b    // May be NULL (non-NULL only for UserID=0)
  )
  {
    //-----------------------------------------------------------------------//
    // Check the args:                                                       //
    //-----------------------------------------------------------------------//
    // Non-NULL "AssetRisks" and "OrderBook" arg(s) are only valid for UserID=0:
    if (utxx::unlikely
       (a_user_id != 0  &&
       (a_ob != nullptr || a_ar_a != nullptr || a_ar_b != nullptr)))
      throw utxx::logic_error
            ("RiskMgr::GetInstrRisksImpl: Non-NULL arg(s) are incompatible "
             "with UserID=", a_user_id);

    //-----------------------------------------------------------------------//
    // Try to find it, or install an empty new one:                          //
    //-----------------------------------------------------------------------//
    // First, find the "IRsMapI" by UserID (or create a new empty one if this
    // UserID is not there yet):
    auto it = m_instrRisks.find(a_user_id);

    if (utxx::unlikely(it == m_instrRisks.end()))
    {
      auto insRes =
        m_instrRisks.insert
          (make_pair
            (a_user_id, IRsMapIAlloc(s_pm.GetSegm()->get_segment_manager())));
      it   = insRes.first;
      assert(insRes.second);
    }
    assert(it != m_instrRisks.end() && it->first == a_user_id);
    IRsMapI& mapI  = it->second; // Ref!

    // Similarly, find or create the "InstrRisks" obj for this SecID. In parti-
    // cular, new (Empty) "InstrRisks" are created HERE:
    InstrRisks& ir = mapI[a_instr.m_SecID];

    // Memoise the UserID:
    m_userIDs.insert(a_user_id);

    //-----------------------------------------------------------------------//
    // Dynamically initialise a new "InstrRisks"?                            //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely(ir.IsEmpty()))
    {
      //---------------------------------------------------------------------//
      // "ir" is entirely empty{                                             //
      //---------------------------------------------------------------------//
      assert(ir.m_initAttempts == 0);

      // If "ir" has just been created by the Default Ctor, try to run the Non-
      // Default Ctor on it; but for that, we need the corresp "AssetRisks(A/B),
      // and this case only applies to UserID=0:
      //
      if (a_user_id == 0)
      {
        // Then we MUST have both "AssetRisks" available (the "OrderBook" may
        // be NULL), because there is no prototype we can refer to:
        //
        if (utxx::likely(a_ar_a != nullptr && a_ar_b != nullptr))
        {
          // Run the Non-Default Ctor:
          new (&ir) InstrRisks(this, a_user_id, a_instr, a_ob, a_ar_a, a_ar_b);

          LOG_INFO(1,
            "RiskMgr::GetInstrRisksImpl: Installed InstrRisks for {}, "
            "UserID=0", a_instr.m_FullName.data())
        }
        else
        {
          // The "InstrRisks" obj is empty and cannot be fully constructed. We
          // XXX leave it in "mapI" but return NULL:
          LOG_ERROR(1,
            "RiskMgr::GetInstrRisksImpl: {}, UserID=0: Cannot construct the "
            "obj: No AssetRisks provided",   a_instr.m_FullName.data())
          return nullptr;
        }
      }
      else
      {
        // UserID is non-0, so try to get the prototype (with UserID=0); the
        // "AssetRisks" and OrderBook are not applicable:
        assert(a_ob == nullptr && a_ar_a == nullptr && a_ar_b == nullptr);

        InstrRisks const* ir0 =
          GetInstrRisksImpl(a_instr, 0, nullptr, nullptr, nullptr);

        if (utxx::unlikely(ir0 == nullptr || ir0->IsEmpty()))
        {
          // Prototype not found or empty, so cannot fully contruct the "ir":
          LOG_ERROR(1,
            "RiskMgr::GetInstrRisksImpl: {}, UserID={}: Cannot construct the "
            ": Prototype with UserID=0 is Missing/Empty",
            a_instr.m_FullName.data(), a_user_id)
          return nullptr;
        }
        // Otherwise: Prototype is valid:
        // Use its AssetRisks (with UserID=0) to find or construct those for
        // the given UserID:
        assert(ir0->m_risksA != nullptr && ir0->m_risksB != nullptr);

        // So the following are the corresp prototype "AssetRisks":
        AssetRisks const& arA0 = *(ir0->m_risksA);
        AssetRisks const& arB0 = *(ir0->m_risksB);

        // And the following are "AssetRisks" ptrs for this UserID:
        a_ar_a =
          GetAssetRisksImpl(arA0.m_asset, arA0.m_settlDate, a_user_id, &arA0);
        a_ar_b =
          GetAssetRisksImpl(arB0.m_asset, arB0.m_settlDate, a_user_id, &arB0);

        // Check them:
        if (utxx::unlikely(a_ar_a == nullptr || a_ar_a->IsEmpty() ||
                           a_ar_b == nullptr || a_ar_b->IsEmpty() ))
        {
          // Cannot complete the construction of "ir":
          LOG_ERROR(1,
            "RiskMgr::GetInstrRisksImpl: {}, UserID={}: Cannot construct the "
            "obj: AssetRisks Missing/Empty",
            a_instr.m_FullName.data(),  a_user_id)
          return nullptr;
        }
        // If we got here: Can run the Non-Default Ctor (also use the Order-
        // Book from the prototype, non-NULL or otherwise):
        new (&ir)
          InstrRisks(this, a_user_id, a_instr, ir0->m_ob, a_ar_a, a_ar_b);

        LOG_INFO(1,
          "RiskMgr::GetInstrRisksImpl: Installed InstrRisks for {}, UserID={}",
          a_instr.m_FullName.data(), a_user_id)
      }
      // List the new "InstrRisks" as updatable via its OrderBook:
      assert(m_obirMap != nullptr);
      if (utxx::likely(ir.m_ob != nullptr))
        (*m_obirMap)  [ir.m_ob].insert(&ir);
    }
    else
    {
      //---------------------------------------------------------------------//
      // "ir" is already non-empty:                                          //
      //---------------------------------------------------------------------//
      // If the AssetRisks and the OrderBook are given, they must be consistent
      // with what we find there:
      assert(ir.m_risksA != nullptr && ir.m_risksB != nullptr);

      if (utxx::unlikely
         ((a_ar_a != nullptr && a_ar_a  != ir.m_risksA) ||
          (a_ar_b != nullptr && a_ar_b  != ir.m_risksB) ||
          (a_ob   != nullptr && ir.m_ob != nullptr && a_ob != ir.m_ob)))
      {
        LOG_ERROR(1,
          "RiskMgr::GetInstrRisksImpl: {}, UserID={}: Args inconsistent with "
          "the stored ptrs", a_instr.m_FullName.data(),  a_user_id)
        return nullptr;
      }
      //---------------------------------------------------------------------//
      // We may STILL need to install the OrderBook (but only ONCE):         //
      //---------------------------------------------------------------------//
      if (ir.m_ob == nullptr && ir.m_initAttempts <= 1)
      {
        if (a_user_id != 0)
        {
          // UserID is non-0: There must be no explicit OrderBook:
          assert(a_ob == nullptr);

          // Again search for the prototype:
          InstrRisks const* ir0 =
            GetInstrRisksImpl(a_instr, 0, nullptr, nullptr, nullptr);

         // If not found, we produce a warning, but this is not an error:
          if (utxx::unlikely(ir0 == nullptr || ir0->IsEmpty()))
          {
            LOG_WARN(1,
              "RiskMgr::GetInstrRisksImpl: {}, UserID={}: Prototype with User"
              "ID=0 is Missing/Empty: Cannot install the OrderBook",
              a_instr.m_FullName.data(),  a_user_id)
          }
          else
            a_ob = ir0->m_ob;
        }
        // If the OrderBook is found (or was specified), install it:
        if (a_ob != nullptr)
        {
          ir.ResetTransient(a_ob);

          // List this "InstrRisks" as updatable via its OrderBook:
          assert(m_obirMap != nullptr  && ir.m_ob != nullptr);
          (*m_obirMap)[ir.m_ob].insert(&ir);
        }
        else
          LOG_WARN(1,
            "RiskMgr::GetInstrRisksImpl: {}, UserID={}: OrderBook=NULL still",
            a_instr.m_FullName.data(), a_user_id)
      }
    }
    //-----------------------------------------------------------------------//
    // If we got here, "ir" must be valid:                                   //
    //-----------------------------------------------------------------------//
    assert(!ir.IsEmpty()            && ir.m_instr == &a_instr &&
           ir.m_userID == a_user_id && ir.m_outer == this);
    ++ir.m_initAttempts;
    return &ir;
  }

  //=========================================================================//
  // "GetAssetRisksImpl":                                                    //
  //=========================================================================//
  // For internal use only; non-throwing, returns NULL if "AssetRisks" was not
  // found and could not be installed from a prototype:
  //
  AssetRisks* RiskMgr::GetAssetRisksImpl
  (
    SymKey     const& a_asset,
    int               a_settl_date,
    UserID            a_user_id,
    AssetRisks const* a_ar0      // Prototype, can be NULL
  )
  {
    //-----------------------------------------------------------------------//
    // Checks:                                                               //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely
       (IsEmpty(a_asset) || (a_settl_date != 0 && a_settl_date < EarliestDate)))
    {
      LOG_ERROR(1, "RiskMgr::GetAssetRisksImpl: Invalid arg(s)")
      return nullptr;
    }
    // If the Prototype is given, it must be non-Empty, consistent with the
    // above params, and must have UserID=0:
    if (utxx::unlikely(a_ar0 != nullptr))
    {
      if (utxx::unlikely
         (a_ar0->IsEmpty()   ||  a_ar0->m_userID != 0       ||
          a_ar0->m_settlDate  != a_settl_date               ||
          a_ar0->m_outer      != this                       ||
          strcmp(a_ar0->m_asset.data(), a_asset.data()) != 0))
      {
        LOG_ERROR(2,
          "RiskMgr::GetAssetRisksImpl: Invalid/Empty Prototype for {}",
          a_asset.data())

        // Mark the Prototype as unusable but still proceed:
        a_ar0 = nullptr;
      }
      else
      // If, on the other hand, the Prototype is valid and the requested UserID
      // is 0 by chance, return the Prototype itself!
      if (utxx::unlikely(a_user_id == 0))
      {
        assert(a_ar0 != nullptr);
        return const_cast<AssetRisks*>(a_ar0);
      }
    }
    //-----------------------------------------------------------------------//
    // Generic Case: Try to find "AssetRisks", or install an empty new one:  //
    //-----------------------------------------------------------------------//
    // First, find the "IRsMapI" by UserID (or create a new empty one if this
    // UserID is not there yet):
    auto it = m_assetRisks.find(a_user_id);

    if (utxx::unlikely(it == m_assetRisks.end()))
    {
      auto insRes =
        m_assetRisks.insert
          (make_pair
            (a_user_id, ARsMapIAlloc(s_pm.GetSegm()->get_segment_manager())));
      it   = insRes.first;
      assert(insRes.second);
    }
    assert(it != m_assetRisks.end() && it->first == a_user_id);
    ARsMapI& mapI  = it->second;  // Ref!

    // Similarly, find or create the "AssetRisks" for (AssetName, SettlDate):
    // In particular, new (Empty) "AssetRisks" are created HERE:
    AssetRisks& ar = mapI[make_pair(a_asset, a_settl_date)];

    // Memoise the UserID:
    m_userIDs.insert(a_user_id);

    //-----------------------------------------------------------------------//
    // Dynamically initialise a new "AssetRisks"?                            //
    //-----------------------------------------------------------------------//
    // If "ar" is Empty (just created), run the Non-Default Ctor on it. This
    // will install the Default Valuator in it (for now):
    //
    if (utxx::unlikely(ar.IsEmpty()))
    {
      assert(ar.m_initAttempts == 0);
      new (&ar) AssetRisks(this, a_user_id, a_asset, a_settl_date);

      assert(!ar.IsEmpty());
      LOG_INFO(2,
        "RiskMgr::GetAssetRisksImpl: Installed AssetRisks for {}, SettlDate="
        "{}, UserID={}", ar.m_asset.data(), ar.m_settlDate, ar.m_userID)
    }
    // If "ar" only has a Trivial Valuator (which is the Default Valuator for
    // a non-RFC asset),  and UserID != 0, try to find a prototype with UserID
    // = 0 and (or use the given prototype) and copy the Valuator over.   But
    // only do it ONCE:
    //
    if (ar.HasTrivialValuator() && ar.m_userID != 0 && ar.m_initAttempts <= 1)
    {
      if (a_ar0 == nullptr)
        a_ar0 = GetAssetRisksImpl(a_asset, a_settl_date, 0, nullptr);

      // NB: The prototype may still be unavailable. However, if found, it must
      // not be empty -- but we do a full check for extra safety:
      if (utxx::unlikely(a_ar0 == nullptr))
      {
        LOG_WARN(1,
          "RiskMgr::GetAssetRisksImpl: Missing prototype with UserID=0 for {} "
          "(SettlDate={}), UserID={}: Will use the Default Valuator",
          a_asset.data(), a_settl_date, a_user_id)
      }
      else
      {
        // Yes, we finally got a prototype:
        assert(!(a_ar0->IsEmpty())  && a_ar0->m_outer        == this &&
               strcmp(a_ar0->m_asset.data(), a_asset.data()) == 0    &&
               a_ar0->m_userID == 0 && a_ar0->m_settlDate    == a_settl_date);

        // Copy the Valuator from it:
        ar.InstallValuator(*a_ar0);
      }
    }
    //-----------------------------------------------------------------------//
    // Map the non-NULL OrderBooks to this "AssetRisks" (again, only ONCE):  //
    //-----------------------------------------------------------------------//
    if (ar.m_initAttempts <= 1)
    {
      assert(m_obarMap != nullptr);
      if (utxx::likely(ar.m_evalOB1 != nullptr))
        (*m_obarMap)  [ar.m_evalOB1].insert(&ar);

      if (ar.m_evalOB2 != nullptr)
        (*m_obarMap)  [ar.m_evalOB2].insert(&ar);

      // Also possibly map them to related "InstrRisks":
      InstallXValuators(ar);
    }
    //-----------------------------------------------------------------------//
    // If we got here: "ar" is valid:                                        //
    //-----------------------------------------------------------------------//
    assert(!ar.IsEmpty()          && ar.m_userID    == a_user_id    &&
           ar.m_asset == a_asset  && ar.m_settlDate == a_settl_date &&
           ar.m_outer == this);
    ++ar.m_initAttempts;
    return &ar;
  }

  //=========================================================================//
  // "EnterSafeMode": Action taken on limit(s) violation:                    //
  //=========================================================================//
  // TODO: Implement Per-User-ID SafeMode:
  //
  void RiskMgr::EnterSafeMode
  (
    char const* a_where,
    char const* a_what
  )
  const
  {
    // If, for any reason, we are in STP Mode (or already in Safe Mode), then
    // do nothing:
    if (m_mode == RMModeT::STP || m_mode == RMModeT::Safe)
      return;

    // Otherwise: Log the risks and limits (XXX: it may be increasing latency
    // of subsequent clean-up that we do it first):
    assert(a_where != nullptr && a_what != nullptr && m_logger != nullptr);

    // XXX: It is also assumed that there is only UserID=0 in this case:
    constexpr UserID userID = 0;

    LOG_CRIT(0,
        "RiskMgr::EnterSafeMode: {}: {}"
        "\n\tTotalNAV= ${} (Limit=${})"
        "\n\tTotalRisk=${} (Limit=${})"
        "\n\tActiveOrdersTotalSize=${} (Limit=${})"
        "\n\tOrderEntryVlm1=${}\n\tOrderFillVlm1= ${}\n\t\t(Limit1=${})"
        "\n\tOrderEntryVlm2=${}\n\tOrderFillVlm2= ${}\n\t\t(Limit2=${})"
        "\n\tOrderEntryVlm3=${}\n\tOrderFillVlm3= ${}\n\t\t(Limit3=${})",
        a_where,                                  a_what,
        m_totalNAV_RFC        [userID],           m_MinTotalNAV_RFC,
        m_totalRiskRFC        [userID],           m_MaxTotalRiskRFC,
        m_totalActiveOrdsSzRFC[userID],           m_MaxActiveOrdsTotalSzRFC,
        m_orderEntryThrottler1.running_sum(),
        m_orderFillThrottler1 .running_sum(),     m_VlmLimitRFC1,
        m_orderEntryThrottler2.running_sum(),
        m_orderFillThrottler2 .running_sum(),     m_VlmLimitRFC2,
        m_orderEntryThrottler3.running_sum(),
        m_orderFillThrottler3 .running_sum(),     m_VlmLimitRFC3)
    m_logger->flush();

    // Cancel all Active Orders in all Registered Connectors:
    for (EConnector_OrdMgmt* omc: m_omcs)
    {
      assert(omc != nullptr);
      omc->CancelAllOrders();
    }
    // Yes, we are now in the Safe Mode:
    m_mode = RMModeT::Safe;

    // TODO: Invoke a special call-back in all Strategies notifying them that
    // we have entered the Safe Mode...
  }

  //=========================================================================//
  // "OutputAssetTradingPositions":                                          //
  //=========================================================================//
  // Logs Trading Positions, Risks, PnL etc. Non-Trading Positions (Init, Trans-
  // fers, Deposits, Debt) are NOT considered here:
  //
  void RiskMgr::OutputAssetPositions
    (char const* a_heading, UserID a_user_id) const
  {
    char* curr = m_outBuff;
    if (a_heading != nullptr && *a_heading != '\0')
    {
      curr = stpcpy(curr, a_heading);
      curr = stpcpy(curr, ": Asset Trading Positions: ");
    }
    else
      curr = stpcpy(curr,   "Asset Trading Positions: ");

    // Traverse all "AssetRisks" with SettlDates (there should be at least 1):
    auto cit = m_assetRisks.find(a_user_id);

    if (utxx::unlikely(cit == m_assetRisks.cend()))
      return;  // No such UserID, so nothing to do

    ARsMapI const& mapI  =   cit->second;

    for (ARsMapIKVT const&   arp: mapI)
    {
      AssetRisks const& ar = arp.second;

      // Entries without a valid SettlDate AND without a position, will be
      // omitted from output:
      if (ar.m_settlDate == 0 && ar.m_trdDelta == 0.0)
        continue;

      // Otherwise: Asset_SettlDate=Position, ...
      curr  = stpcpy(curr, ar.m_asset.data());
      *curr = '_';
      curr  = utxx::itoa_left<int, 8>(curr + 1, ar.m_settlDate);
      *curr = '=';
      curr += utxx::ftoa_left(ar.m_trdDelta, curr + 1, 16, 2);
      curr  = stpcpy(curr, ", ");
    }

    // Finally, Total RFC NAV, Risk and PnL (with Asset Netting):
    curr  = stpcpy(curr,     "TotalRisks=");
    curr  = stpcpy(curr + 1, m_RFC.data());
    *curr = ' ';
    curr += utxx::ftoa_left(m_totalRiskRFC[a_user_id], curr + 1, 16, 2);
    curr  = stpcpy(curr,     ", NAV=");
    curr  = stpcpy(curr + 1, m_RFC.data());
    *curr = ' ';
    curr += utxx::ftoa_left(m_totalNAV_RFC[a_user_id], curr + 1, 16, 2);

    // NB: "m_outBuff" is guaranteed to be long enough:
    assert(curr < m_outBuff + sizeof(m_outBuff));

    // And finally, perform the actual output (XXX: Irresp to the DebugLevel):
    m_logger->info(m_outBuff);
  }

  //=========================================================================//
  // Dynamic Control of MktData Updates:                                     //
  //=========================================================================//
  void RiskMgr::DisableMktDataUpdates() const
  {
    // Simply set the NextMDUpdate far in the future (1 Tropical Year from now),
    // without affecting the Period:
    m_nextMDUpdate = utxx::now_utc() + utxx::secs(31556952);
    LOG_INFO(2,
      "RiskMgr::DisableMktDataUpdates: MDUpdates Temporarily Disabled")
  }

  void RiskMgr::EnableMktDataUpdates()  const
  {
    // Set the NextMDUpdate to Now, again without affecting the Period:
    m_nextMDUpdate = utxx::now_utc();
    LOG_INFO(2,
      "RiskMgr::EnableMktDataUpdates: MDUpdates Enabled, Period={} msec",
      m_mdUpdatesPeriodMSec)
  }

  //=========================================================================//
  // Explicit Instances of Template Methods:                                 //
  //=========================================================================//
  template
  void RiskMgr::OnOrder<false>
  (
    EConnector_OrdMgmt const* a_omc,
    SecDefD const&            a_instr,
    bool                      a_is_buy,
    QtyTypeT                  a_qt,
    PriceT                    a_new_px,
    double                    a_new_qty,
    PriceT                    a_old_px,
    double                    a_old_qty,
    utxx::time_val            a_ts
  );

  template
  void RiskMgr::OnOrder<true>
  (
    EConnector_OrdMgmt const* a_omc,
    SecDefD const&            a_instr,
    bool                      a_is_buy,
    QtyTypeT                  a_qt,
    PriceT                    a_new_px,
    double                    a_new_qty,
    PriceT                    a_old_px,
    double                    a_old_qty,
    utxx::time_val            a_ts
  );
} // End namespace MAQUETTE
