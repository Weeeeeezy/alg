// vim:ts=2:et
//===========================================================================//
//                           "InfraStruct/RiskMgr.h":                        //
//   Position, PnL, UnR PnL and Risks Manager (ShM-Based Singleton Class)    //
//===========================================================================//
#pragma once

#include "InfraStruct/PersistMgr.h"
#include "InfraStruct/SecDefsMgr.h"
#include "InfraStruct/RiskMgrTypes.h"
#include "InfraStruct/StaticLimits.h"
#include <utxx/rate_throttler.hpp>
#include <boost/container/static_vector.hpp>
#include <boost/interprocess/allocators/private_node_allocator.hpp>
#include <algorithm>
#include <map>
#include <set>

namespace MAQUETTE
{
  //=========================================================================//
  // "RiskMgr" Class:                                                        //
  //=========================================================================//
  // Its single instance lives in ShM, and is accessed via the static "s_pm":
  //
  class  EConnector_OrdMgmt;
  struct Trade;

  class RiskMgr: public boost::noncopyable
  {
  public:
    //=======================================================================//
    // Names of Persistent Objs:                                             //
    //=======================================================================//
    constexpr static char const* RiskMgrON() { return "RiskMgr"; }

    //=======================================================================//
    // Public Types (with Private Impls):                                    //
    //=======================================================================//
    // NB: Implementation details (such as ShM Allocators)  are made "private":
    // XXX: Using "private_node_allocator" (as opposed to the generic one) may
    // result in slightly higher ShM footprint, but  is preferred  because  it
    // does not use any locking (which could otherwise result in ShM data being
    // locked on a crash):
    //-----------------------------------------------------------------------//
    // "IRsMapI": Inner ShM Map: {SecID  => InstrRisks}:                     //
    //-----------------------------------------------------------------------//
    // FIXME: Need a global space of "SecID"s! For now, there is no guratantee
    // that SecIDs from different exchanges would not accidentially clash!
  private:
    using IRsMapIKVT   = std::pair<SecID const, InstrRisks>;
    using IRsMapIAlloc =
          boost::interprocess::private_node_allocator
          <IRsMapIKVT, FixedShM::segment_manager>;
  public:
    using IRsMapI      =
          std::map<SecID,  InstrRisks, std::less<SecID>,  IRsMapIAlloc>;

    //-----------------------------------------------------------------------//
    // "IRsMap" : Outer ShM Map: {UserID => IRsMapI}:                        //
    //-----------------------------------------------------------------------//
  private:
    using IRsMapKVT    = std::pair<UserID const, IRsMapI>;
    using IRsMapAlloc  =
          boost::interprocess::private_node_allocator
          <IRsMapKVT,  FixedShM::segment_manager>;
  public:
    using IRsMap       =
          std::map<UserID, IRsMapI,    std::less<UserID>, IRsMapAlloc>;

    //-----------------------------------------------------------------------//
    // "ARsMapI": Inner ShM Map: {(SymKey, SettlDate) => AssetRisks}:        //
    //-----------------------------------------------------------------------//
    // FIXME: Again, we need a global registry of AssetIDs to be used instead
    // of SymKeys for identifying Assets:
  private:
    using ARsMapIKeyT  = std::pair<SymKey,     int>;   // (Symbol, SettlDate)
    using ARsMapIKVT   = std::pair<ARsMapIKeyT const,  AssetRisks>;
    using ARsMapIAlloc =
          boost::interprocess::private_node_allocator
          <ARsMapIKVT, FixedShM::segment_manager>;

    // Comparator of (Symbol, SettlDate) Pairs:
    struct ARsMapILessT
    {
      bool operator()(ARsMapIKeyT const& a_left, ARsMapIKeyT const& a_right)
      const
      {
        int     c = strcmp(a_left.first.data(), a_right.first.data());
        return (c < 0) || (c == 0 && a_left.second < a_right.second);
      }
    };
  public:
    using ARsMapI      =
          std::map<ARsMapIKeyT, AssetRisks, ARsMapILessT,      ARsMapIAlloc>;

    //-----------------------------------------------------------------------//
    // "ARsMap": Outer ShM Map: {UserID => ARsMapI}:                         //
    //-----------------------------------------------------------------------//
  private:
    using ARsMapKVT    = std::pair<UserID const, ARsMapI>;
    using ARsMapAlloc  =
          boost::interprocess::private_node_allocator
          <ARsMapKVT,  FixedShM::segment_manager>;
  public:
    using ARsMap       =
          std::map<UserID,      ARsMapI,    std::less<UserID>, ARsMapAlloc>;

    //-----------------------------------------------------------------------//
    // "UserIDsSet": ShM Set: {UserID}:                                      //
    //-----------------------------------------------------------------------//
  private:
    using UserIDsAlloc =
          boost::interprocess::private_node_allocator
          <UserID,     FixedShM::segment_manager>;
  public:
    using UserIDsSet   = std::set<UserID, std::less<UserID>,  UserIDsAlloc>;

  private:
    //=======================================================================//
    // Private Types:                                                        //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "OBIRMap"  (NOT ShM): { OrderBook => {InstrRisks} }:                  //
    //-----------------------------------------------------------------------//
    // Used to quickly find the InstrRisks affected by an OrderBook tick (UnRe-
    // alised PnL). Not placed in ShM because OrderBooks themselves are not in
    // ShM either:
    using OBIRMap = std::map<OrderBookBase const*, std::set<InstrRisks*>>;

    //-----------------------------------------------------------------------//
    // "OBARMap" (NOT ShM): { OrderBook => {AssetRisks} }:                   //
    //-----------------------------------------------------------------------//
    // (Again, not in ShM): Used to quickly find the AssetRisks affected by an
    // OrderBook tick (as an Asset->RFC Valuator):
    using OBARMap = std::map<OrderBookBase const*, std::set<AssetRisks*>>;

    //-----------------------------------------------------------------------//
    // "TotalsMap": ShM Map: {UserID => double}:                             //
    //-----------------------------------------------------------------------//
    using TotalsMapKVT   = std::pair<UserID const, double>;
    using TotalsMapAlloc =
          boost::interprocess::private_node_allocator
          <TotalsMapKVT, FixedShM::segment_manager>;
    using TotalsMap      =
          std::map <UserID, double,    std::less<UserID>, TotalsMapAlloc>;

    //-----------------------------------------------------------------------//
    // "OMCsVec": StaticVector is sufficient here:                           //
    //-----------------------------------------------------------------------//
    using OMCsVec          =
          boost::container::static_vector<EConnector_OrdMgmt*, Limits::MaxOMCs>;

    //=======================================================================//
    // Consts:                                                               //
    //=======================================================================//
    // Window sizes (in sec) for Order (Entry & Fill) Volume Throttlers:
    // NB: It's OK that they are not configurable -- those consts are only used
    // for Rate Buckets construction, not for actual rates computation:
    // TODO: This setup is suitable for UHFT but not for longer horizons.  Need
    // to make it general enough:
    //
    constexpr static int VlmThrottlWinSec1  =   10; // Rate is eg per 1 sec
    constexpr static int VlmThrottlBkts1    =   10; // ...with 10 buckets / sec

    constexpr static int VlmThrottlWinSec2  =  120; // Rate is eg per 1 min
    constexpr static int VlmThrottlBkts2    =    2; // ... with 2 buckets / sec

    constexpr static int VlmThrottlWinSec3  = 3600; // Rate is eg per 1 hour
    constexpr static int VlmThrottlBkts3    =    1; // ... with 1 bucket  / sec

    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    // STATIC "PersistMgr" used to access the ShM-based "RiskMgr" object:
    static PersistMgr<>               s_pm;

    bool    const                     m_isProd;     // Prod or Test Env?
    Ccy     const                     m_RFC;        // Risk-Free Ccy
    mutable bool                      m_isActive;
    mutable RMModeT                   m_mode;

    // Frequency Control of MktData Updates (otherwise, too much recalculations
    // could occur). (This can also be used to disable MD Updates while eg read-
    // ing a backlog of Trades). If the Period is 0 and/or NextUpdate is unspec-
    // ified, all Updates are carried out:
    int                               m_mdUpdatesPeriodMSec;   // msec!
    mutable utxx::time_val            m_nextMDUpdate;

    // Exposure, NAV and Active Orders Limits:
    // NB:
    // (*) The following params are not marked as "const" because they may act-
    //     ually be modified when the "RiskMgr" object (in ShM) is re-attached;
    // (*) Exceeding the "MaxTotalRiskRFC" limit causes a shut-down,   whereas
    //     "MaxNormalRiskRFC" might only produce a warning in the GUI;
    // (*) XXX: Currently, these Limits are Global, ie NOT per UserID, because
    //     the Multi-User env is typically the STP one where those Limits  are
    //     not really used:
    double                            m_MaxTotalRiskRFC;
    double                            m_MaxNormalRiskRFC;
    double                            m_MinTotalNAV_RFC;
    double                            m_MaxOrdSzRFC;
    double                            m_MinOrdSzRFC;
    double                            m_MaxActiveOrdsTotalSzRFC;

    // Operational limits on the over-all frequency of requests issued:  Separa-
    // tely for Passive (Quotes) and Aggressive Orders, with 3 different periods
    // (eg: 1 sec, 1 min, 1 hour or so); all periods are in sec:
    // NB:
    // (*) The limits are "long"s, not "double"s, because the RateThrottler only
    //     supports integrals at the moment;
    // (*) As Throttlers below, these settings are Global:
    //
    int const                         m_VlmThrottlPeriod1;
    long                              m_VlmLimitRFC1;
    int const                         m_VlmThrottlPeriod2;
    long                              m_VlmLimitRFC2;
    int const                         m_VlmThrottlPeriod3;
    long                              m_VlmLimitRFC3;

    // Throttlers for the RFC Volume rate of Entered and Filled Order. Again,
    // the Throttlers are currently Global:
    //
    mutable utxx::basic_rate_throttler<VlmThrottlWinSec1, VlmThrottlBkts1>
                                      m_orderEntryThrottler1;
    mutable utxx::basic_rate_throttler<VlmThrottlWinSec1, VlmThrottlBkts1>
                                      m_orderFillThrottler1;

    mutable utxx::basic_rate_throttler<VlmThrottlWinSec2, VlmThrottlBkts2>
                                      m_orderEntryThrottler2;
    mutable utxx::basic_rate_throttler<VlmThrottlWinSec2, VlmThrottlBkts2>
                                      m_orderFillThrottler2;

    mutable utxx::basic_rate_throttler<VlmThrottlWinSec3, VlmThrottlBkts3>
                                      m_orderEntryThrottler3;
    mutable utxx::basic_rate_throttler<VlmThrottlWinSec3, VlmThrottlBkts3>
                                      m_orderFillThrottler3;

    // The list of all OrderMgmt Connectors using this RiskMgr; in case we enter
    // the SafeMode, all active orders will be automatically cancelled: Ptrs  to
    // OMCs are NOT owned, of course:
    OMCsVec                           m_omcs;

    // Logger (XXX: ptr NOT owned -- we currently use an external Logger, eg
    // same as for the Reactor etc; typically points back from ShM to conven-
    // tional memory):
    spdlog::logger*                   m_logger;
    int                               m_debugLevel;

    // CALL-BACKS: XXX: Do NOT install "std::function"s here by themselves, as
    // they are not plain-vanilla objs, and their size  may vary  even between
    // different compilations of MAQUETTE software!  Install ptrs to functions
    // instead; they are pointing into ordinary (non-shared) memory,   so need
    // to be reset in beetween invocations:
    InstrRisksUpdateCB const*         m_irUpdtCB;
    AssetRisksUpdateCB const*         m_arUpdtCB;

    // All UserIDs, and UserIDs affected by a particular Update (the latter is
    // NOT in ShM -- XXX BAD, a ptr would be much safer!):
    mutable UserIDsSet                m_userIDs;
    mutable std::set<UserID>*         m_affectedUserIDs;

    // All "InstrsRisks" and "AssetRisks" for all Users:
    mutable IRsMap                    m_instrRisks;
    mutable ARsMap                    m_assetRisks;

    // Indices for easy access to the above Maps on OrderBook updates (built in
    // Non-Shared Memory, as their Keys are transient OrderBook ptrs anyway):
    mutable OBIRMap*                  m_obirMap;
    mutable OBARMap*                  m_obarMap;

    // Total risk exposure in RFC -- RFC cash is NOT counted, by UserID:
    mutable TotalsMap                 m_totalRiskRFC;

    // As above, but for potential risks due to currently active orders:
    mutable TotalsMap                 m_totalActiveOrdsSzRFC;

    // Total NAV (across all Assets) in RFC, incl RFC position itself, computed
    // by summing up across all netted Assets (in particular Ccys) and SettlDa-
    // tes:
    mutable TotalsMap                 m_totalNAV_RFC;

    // AT THE END: Output buffer for logging the Positions and Risks: Must be
    // large enough:
    mutable char                      m_outBuff[65536];

    // Default Ctor is deleted:
    RiskMgr() = delete;

    //=======================================================================//
    // Non-Default Ctor:                                                     //
    //=======================================================================//
    // The Ctor is private: It can only be invoked from within the ShM segment
    // mgr;  "a_params" must contain the "ShMSegmSzMB" entry which is the size
    // (in MegaBytes) of that ShM segment which must be sufficient to allocate
    // the "RiskMgr" obj itself and all of its dynamic ShM data structs:
    RiskMgr
    (
      bool                               a_is_prod,
      boost::property_tree::ptree const& a_params,
      spdlog::logger*                    a_logger,
      int                                a_debug_level
    );

    // The following is required in order to construct  "RiskMgr" objs in ShM,
    // because the above Ctor is private (to prevent on-stack and on-heap ctor
    // invocations):
    template<class T, bool IsIterator, class ...Args>
    friend struct  BIPC::ipcdetail::CtorArgN;

    // "InstrRisks" and "AssetRisks" may need to have access to RiskMgr's Flds:
    friend struct InstrRisks;
    friend struct AssetRisks;

  public:
    //=======================================================================//
    // Configuration/Start/Stop/Accessor Methods:                            //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "SetCallBacks":                                                       //
    //-----------------------------------------------------------------------//
    // Similar to "SetUserIDs" above: to be invoked before "Start". Unused CBs
    // must be passed as empty functions:
    //
    void SetCallBacks
    (
      InstrRisksUpdateCB const& a_ir_updt_cb,
      AssetRisksUpdateCB const& a_ar_updt_cb
    );

    //-----------------------------------------------------------------------//
    // "Start":                                                              //
    //-----------------------------------------------------------------------//
    // The RiskMgr is NOT started immediately upon construction  by default, eg
    // because some OrderBook data needed for Asset->RFC valuation  may  yet be
    // unavailable. Use "Start" to avoid such race conditions;  enable the Risk-
    // Mgr ONLY when all data srcs are fully initialised:
    // HOWEVER, for safety reasons, once started, the RiskMgr CANNOT be stopped;
    // its Operating Mode cannot be relaxed (eg it is impossible to go from the
    // Normal to the Relaxed Mode), but it can be strngthened either automatic-
    // ally or manually (eg going into Safe Mode):
    //
    void Start(RMModeT a_mode = RMModeT::Normal);

    //-----------------------------------------------------------------------//
    // Accessors:                                                            //
    //-----------------------------------------------------------------------//
    Ccy     GetRFC   () const { return m_RFC;      }
    bool    IsProdEnv() const { return m_isProd;   }
    bool    IsActive () const { return m_isActive; }
    RMModeT GetMode  () const { return m_mode;     }

    //-----------------------------------------------------------------------//
    // "GetPersistInstance": Static Factory:                                 //
    //-----------------------------------------------------------------------//
    // Creates a "RiskMgr" object in ShM, or finds an existing one there.   If
    // "a_observer" is set, tries to  open  an existing RiskMgr in a Read-Only
    // Only (Observer) mode; in that case, "a_reset_all", "a_logger" and "a_de-
    // bug_level" must NOT be set.
    // If the ShM object was not found and is created from scratch, it may be
    // populated from a DB (currently PostGreSQL is supported)   according to
    // the settings in "a_params":
    //
    static RiskMgr* GetPersistInstance
    (
      bool                               a_is_prod,
      boost::property_tree::ptree const& a_params,
      SecDefsMgr                  const& a_sdm,    // Only required for "Load*"
      bool                               a_is_observer,
      spdlog::logger*                    a_logger      = nullptr,
      int                                a_debug_level = 0
    );

    //-----------------------------------------------------------------------//
    // "Register" (Instrument):                                              //
    //-----------------------------------------------------------------------//
    // A Traded Instrument is registered with the Risk Manager; double regist-
    // ration is OK (simply ignored).
    // IMPORTANT:
    // (*) It is NOT required to that &(a_ob.GetInstr())==&a_instr, ie it can
    //     be a "foreign" OrderBook for a "compatible" instr;
    // (*) But it is required that the explicit "a_instr" and the instr behind
    //     "a_ob" must be consistent in terms of Asset(A), SettlCcy(B)  and
    //     SettlDate;
    // (*) This allows us to use some "more liquid" OrderBook for evaluation of
    //     "a_instr" risks than its "native" one;
    // (*) It is also possible to Register an Instrument w/o OrderBook, but it
    //     is highly undesirable (UnRealised PnL will be missing in that case):
    // (*) Even if a certain OrderBook was Registered with the RiskMgr, it does
    //     NOT mean that the corresp OrderBook Updates will be sourced by the
    //     RiskMgr automatically. Instead, the corresp MDC must do it  as part
    //     of its notification routine. Thus, this method checks that the corr
    //     MDC indeed uses this RiskMgr:
    //
    void Register
    (
      SecDefD       const& a_instr,
      OrderBookBase const* a_ob
    );

    //-----------------------------------------------------------------------//
    // "Register" (OMC):                                                     //
    //-----------------------------------------------------------------------//
    // Only Registered OrderMgmt Connectors will be allowed to place orders, so
    // that all active orders could be cancelled if the RiskMgr enters SafeMode:
    //
    void Register(EConnector_OrdMgmt* a_omc); // Mist be non-NULL

    //-----------------------------------------------------------------------//
    // "InstallValuator":                                                    //
    //-----------------------------------------------------------------------//
    // NB: "InstallValuator" methods assume User=0; for other UserIDs, the Val-
    // uators are propagated dynamically:
    //
    // "InstallValuator(OrderBooks)": Uses 1 or 2 OrderBooks (in the latter ca-
    // se, depending on the TimeOfDay with the specified RollOverTime) providing
    // an Asset/RFC or RFC/CcyAsset valuation rate,  with possible  adjustments
    // (if different SettlDates) coming from eg Swap Rates:
    //
    void InstallValuator
    (
      char          const* a_asset,           // Must be non-NULL, non-empty
      int                  a_settl_date,      // May be 0
      OrderBookBase const* a_ob1,             // Must be non-NULL
      double               a_bid_adj1       = 0.0,
      double               a_ask_adj1       = 0.0,
      utxx::time_val       a_roll_over_time = utxx::time_val(),
      OrderBookBase const* a_ob2            = nullptr,
      double               a_bid_adj2       = 0.0,
      double               a_ask_adj2       = 0.0
    );

    // "InstallValuator(Fixed Asset/RFC Valuation Rate)":
    // Installs an Asset->RFC Valuator given by a Fixed Asset/RFC Rate:
    //
    void InstallValuator
    (
      char const*      a_asset,               // Non-NULL, non-empty
      int              a_settl_date,
      double           a_rate
    );

    //=======================================================================//
    // Principal Event Processors:                                           //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "OnMktDataUpdate":                                                    //
    //-----------------------------------------------------------------------//
    // XXX: If it is an update of a "Valuator" (Asset->RFC) instrument, we will
    // NOT automatically re-calculate all RFC  values  -- rather,  we will use
    // the new conversion rates later on when such re-calculation is activated;
    //
    // "a_ts" is any suitable TimeStamp (only used to update the Throttlers):
    // ExchTS if known, LocalTS otherwise:
    //
    void OnMktDataUpdate(OrderBookBase const& a_ob, utxx::time_val a_ts);

    //-----------------------------------------------------------------------//
    // "OnTrade":                                                            //
    //-----------------------------------------------------------------------//
    void OnTrade(Trade const& a_tr);

    //-----------------------------------------------------------------------//
    // "OnBalanceUpdate":                                                    //
    //-----------------------------------------------------------------------//
    // Should be called on an external (Non-Trading) Balance Update for some
    // Asset:
    void  OnBalanceUpdate
    (
      char const*               a_asset,         // Non-NULL
      int                       a_settl_date,
      char const*               a_trans_id,      // Non-NULL
      UserID                    a_user_id,
      AssetRisks::AssetTransT   a_trans_t,
      double                    a_new_total,
      utxx::time_val            a_ts_exch        // May be empty
    );

    //-----------------------------------------------------------------------//
    // "OnOrder":                                                            //
    //-----------------------------------------------------------------------//
    // NB: This method is invoked:
    // (*) on New / Modification Orders (IsReal=true)
    // (*) when an existing order has been Filled, Cancelled, or Rejected
    //                                  (IsReal=false);
    // (*) Here all Order/Trade Qtys are always "double"s for uniformity,
    //     becuase the RiskMgr is a singleton component;
    // (*) The semantics of both NewQty and OldQty is given by QT param;
    // (*) The semantics of both NewPx  and OldPx  is the REAL TRADE/QUOTE Px,
    //     NOT the normalised A/B Px:
    //
    template<bool IsReal>
    void OnOrder
    (
      EConnector_OrdMgmt const* a_omc,           // Non-NULL
      SecDefD const&            a_instr,
      bool                      a_is_buy,
      QtyTypeT                  a_qt,            // To interpret Qtys below
      PriceT                    a_new_px,        // 0.0 if Cancel, NaN for Mkt
      double                    a_new_qty,
      PriceT                    a_old_px,        // Use 0   if NewOrder
      double                    a_old_qty,
      utxx::time_val            a_ts             // LocalTS or empty (Removal)
    );

    //=======================================================================//
    // Getting Risk-Related Data:                                            //
    //=======================================================================//
    // XXX: None of the menthods below check the UNIQUENESS of the obj found --
    // this is done by "Start" at the last check-point:
    // External versions: Throw an exception of the object was not found:
    //
    InstrRisks const& GetInstrRisks    (SecDefD const&       a_instr,
                                        UserID               a_user_id) const;

    AssetRisks const& GetAssetRisks    (char    const*       a_asset,
                                        int                  a_settl_date,
                                        UserID               a_user_id) const;
  private:
    // Internal non-throwing versions: Return NULL of object not found. NB:
    // The ptrs may be NULL (and *must* be NULL for UserID != 0):
    InstrRisks*       GetInstrRisksImpl(SecDefD const&       a_instr,
                                        UserID               a_user_id,
                                        OrderBookBase const* a_ob,
                                        AssetRisks*          a_ar_a,
                                        AssetRisks*          a_ar_b);

    AssetRisks*       GetAssetRisksImpl(SymKey  const&       a_asset,
                                        int                  a_settl_date,
                                        UserID               a_user_id,
                                        AssetRisks const*    a_ar0);  // Proto!
    // "RefreshThrottlers":
    // Invoked with any of the "On*" methods, simply to update the curr time-
    // stamp in the Throttlers:
    void RefreshThrottlers(utxx::time_val a_ts) const;

    //-----------------------------------------------------------------------//
    // "EnterSafeMode": Action taken on limit(s) violation:                  //
    //-----------------------------------------------------------------------//
    void EnterSafeMode(char const* a_where,  char const* a_what) const;

    //-----------------------------------------------------------------------//
    // "GetQtyA": Convert a given QT-based TradeQty into QtyA:               //
    //-----------------------------------------------------------------------//
    // NB: This is an alternative to "ConvQty"; we deliberately use an alterna-
    // tive implementation in RiskMg, for extra safety (2-version programming):
    static RMQtyA GetQtyA
    (
      double           a_qty_raw,  // Semantics of QtyRaw given by QT below
      QtyTypeT         a_qt,
      SecDefD const&   a_instr,
      PriceT           a_px_ab     // Must be true A/B Px
    );

    //-----------------------------------------------------------------------//
    // "GetQtyB": Similar:                                                   //
    //-----------------------------------------------------------------------//
    // NB: This is an alternative to "ConvQty"; we deliberately use an alterna-
    // tive implementation in RiskMg, for extra safety (2-version programming):
    static RMQtyB GetQtyB
    (
      double           a_qty_raw,  // Semantics of QtyRaw given by QT below
      QtyTypeT         a_qt,       // Only QtyA or QtyB currently
      PriceT           a_px_ab     // Must be true A/B Px
    );

    //-----------------------------------------------------------------------//
    // Utils:                                                                //
    //-----------------------------------------------------------------------//
    // "ReCalcRFCTotas":
    // Re-calculate Total Risks, NAV and ActiveOrdsSz in RFC for a given UserID
    // after a Trade or MktData tick:
    //
    void ReCalcRFCTotals(UserID a_user_id)   const;

    //  "IsInOBIRMap", "IsInOBARMap" (for use in "Start" checks):
    bool IsInOBIRMap(OrderBookBase    const* a_ob, InstrRisks* a_ir) const;
    bool IsInOBARMap(OrderBookBase    const* a_ob, AssetRisks* a_ar) const;

    // "InstallXValuators" (see the impl for details):
    void InstallXValuators(AssetRisks const& a_ar);

    // "InstallEmptyMaps": Install empty "IRsMapI" and "ARsMapI" for a given
    // UserID (typically 0):
    void InstallEmptyMaps (UserID a_user_id);

    //-----------------------------------------------------------------------//
    // Loading the data from a DB (when ShM segment is re-created):          //
    //-----------------------------------------------------------------------//
    // The following methods require PQXX:
    //
    // Generic Wrapper:
    void Load
    (
      boost::property_tree::ptree const& a_params,
      SecDefsMgr                  const& a_sdm
    );

    // XXX: Specific version for PostgreSQL and LATOKEN-specific DB schema:
    void LoadLATOKEN
    (
      boost::property_tree::ptree const& a_params,
      SecDefsMgr                  const& a_sdm
    );

  public:
    //=======================================================================//
    // Accessors:                                                            //
    //=======================================================================//
    UserIDsSet const& GetAllUserIDs()    const   { return m_userIDs; }

    IRsMapI    const& GetAllInstrRisks   (UserID   a_user_id) const;
    ARsMapI    const& GetAllAssetRisks   (UserID   a_user_id) const;

    // The following method returns both NAVs -- they do not need to be identi-
    // cal but should be reasonable close to each other:
    //
    double GetTotalNAV_RFC               (UserID   a_user_id) const;
    double GetTotalRiskRFC               (UserID   a_user_id) const;
    double GetActiveOrdersTotalSizeRFC   (UserID   a_user_id) const;

    // Access to the Limits (XXX: Volume Throttling params are currenly NOT
    // exported, because there is little point  for the client in examining
    // them):
    double GetMaxTotalRiskRFC () const   { return  m_MaxTotalRiskRFC;  }
    double GetMaxNormalRiskRFC() const   { return  m_MaxNormalRiskRFC; }
    double GetMinTotalNAV_RFC () const   { return  m_MinTotalNAV_RFC;  }
    double GetMaxOrderSizeRFC () const   { return  m_MaxOrdSzRFC;      }
    double GetMinOrderSizeRFC () const   { return  m_MinOrdSzRFC;      }

    double GetMaxActiveOrdsTotalSzRFC() const
      { return m_MaxActiveOrdsTotalSzRFC; }

    //=======================================================================//
    // Dynamic Control of MktData Updates:                                   //
    //=======================================================================//
    void  EnableMktDataUpdates() const;
    void DisableMktDataUpdates() const;

    //=======================================================================//
    // "OutputAssetPositions":                                               //
    //=======================================================================//
    // Logs Positions, Risks, PnL etc:
    //
    void OutputAssetPositions(char const* a_heading, UserID a_user_id) const;
  };
} // End namespace MAQUETTE
