// vim:ts=2:et
//===========================================================================//
//      "Connectors/H2WS/TDAmeritrade/EConnector_H1WS_TDAmeritrade.h":       //
//             WS and HTTP/1.1 Based MDC/OMC for TDAmeritrade                //
//===========================================================================//
#pragma once

#include <string_view>
#include <utility>

#include "Connectors/EConnector_MktData.h"
#include "Connectors/EConnector_MktData.hpp"
#include "Connectors/EConnector_OrdMgmt.h"
#include "Connectors/EConnector_OrdMgmt.hpp"
#include "Connectors/H2WS/Configs.hpp"
#include "Connectors/H2WS/TDAmeritrade/TD_H1WS_Connector.hpp"

// TODO:
// process fills
// get initial positions, balances
// process trades
// process level 2 data

namespace MAQUETTE {
//=========================================================================//
// "EConnector_H1WS_TDAmeritrade" Class:                                   //
//=========================================================================//
class EConnector_H1WS_TDAmeritrade final
    : public EConnector_MktData,
      public EConnector_OrdMgmt,
      public TD_H1WS_Connector<EConnector_H1WS_TDAmeritrade> {
public:
  constexpr static char const Name[] = "H1WS-TDAmeritrade";
  //=======================================================================//
  // Types and Consts:                                                     //
  //=======================================================================//
  // This is both an MDC and an OMC:
  constexpr static bool IsOMC = true;
  constexpr static bool IsMDC = true;

  constexpr static QtyTypeT QT = QtyTypeT::QtyA;
  using QR = long;
  using QtyN = Qty<QT, QR>;

  // This class must be accessible by the parent classes:
  friend class EConnector_MktData;
  friend class EConnector_OrdMgmt;
  friend class TCP_Connector<EConnector_H1WS_TDAmeritrade>;
  friend class H2WS::WSProtoEngine<EConnector_H1WS_TDAmeritrade>;
  friend class TD_H1WS_Connector<EConnector_H1WS_TDAmeritrade>;
  using TDH1WS = TD_H1WS_Connector<EConnector_H1WS_TDAmeritrade>;

  //-----------------------------------------------------------------------//
  // Flags required by "EConnector_MDC":                                   //
  //-----------------------------------------------------------------------//
  // MktDepth is conceptually unlimited:
  constexpr static int MktDepth = 0;

  // Sparse OrderBooks are not (yet) used for TDAmeritrade:
  constexpr static bool IsSparse = true;
  constexpr static bool HasTrades = true;
  constexpr static bool HasSeqNums = false;

  //-----------------------------------------------------------------------//
  // Flags for "EConnector_MktData" templated methods:                     //
  //-----------------------------------------------------------------------//
  constexpr static bool IsMultiCast = false;
  constexpr static bool WithIncrUpdates = false;
  constexpr static bool WithOrdersLog = false;
  constexpr static bool WithRelaxedOBs = false;
  constexpr static bool ChangeIsPartFill = false; // SnapShots only...
  constexpr static bool NewEncdAsChange = false;
  constexpr static bool ChangeEncdAsNew = false;
  constexpr static bool IsFullAmt = false;

  //-----------------------------------------------------------------------//
  // Features required by "EConnector_OMC":                                //
  //-----------------------------------------------------------------------//
  // Throttling period is 1 min for TDAmeritrade, with 60 Reqs/Period allowed:
  constexpr static int ThrottlingPeriodSec = 60;
  constexpr static int MaxReqsPerPeriod = 120;
  // Other Features (XXX: CHECK!):
  constexpr static PipeLineModeT PipeLineMode = PipeLineModeT::Wait1;
  constexpr static bool SendExchOrdIDs = true;
  constexpr static bool UseExchOrdIDsMap = true;
  constexpr static bool HasAtomicModify = true;
  constexpr static bool HasPartFilledModify = true;
  constexpr static bool HasExecIDs = false;
  constexpr static bool HasMktOrders = true;
  constexpr static bool HasFreeCancel = false; // Cancels DO count
  constexpr static bool HasNativeMassCancel = false;

  //-----------------------------------------------------------------------//
  // "Get[WS|HTTP]Config":                                                 //
  //-----------------------------------------------------------------------//
  // XXX: The only supported Env is Prod:
  static H2WSConfig const &GetWSConfig(MQTEnvT a_env);
  static H2WSConfig const &GetHTTPConfig(MQTEnvT a_env);
  static std::vector<SecDefS> const *GetSecDefs();

private:
  //=======================================================================//
  // Synthetic "MDEntryST" and "MDMsg" Types:                              //
  //=======================================================================//
  // XXX: They are not strictly necessary,  as we construct OrderBook Snap-
  // Shots and Trades directly from the data  read from incoming JSON objs.
  // However, in order to re-use EConnector_MktData::UpdateOrderBooks<>, we
  // need to use these structs -- the copying overhead is minimal:
  //
  //=======================================================================//
  // "MDEntryST" (SnapShot/Trade) Struct:                                  //
  //=======================================================================//
  // NB: Bids and Offers are stored in the same array of MDEntries, distingu-
  // ished by the MDEntryType:
  //
  struct MDEntryST {
    //---------------------------------------------------------------------//
    // "MDEntryST" Data Flds:                                              //
    //---------------------------------------------------------------------//
    OrderID m_execID;              // For Trades only
    FIX::MDEntryTypeT m_entryType; // Bid|Offer; for Trades, the Aggressor
    PriceT m_px;
    QtyN m_qty;

    //---------------------------------------------------------------------//
    // "MDEntryST" Default Ctor:                                           //
    //---------------------------------------------------------------------//
    MDEntryST()
        : m_execID(0), m_entryType(FIX::MDEntryTypeT::UNDEFINED),
          m_px(NaN<double>), m_qty(0.0) {}

    //---------------------------------------------------------------------//
    // Accessors (CBs for EConnector_MktData::UpdateOrderBooks):           //
    //---------------------------------------------------------------------//
    // "GetEntryType" (returns Bid, Offer or Trade):
    FIX::MDEntryTypeT GetEntryType() const { return m_entryType; }

    // "GetUpdateAction": For SnapShot MDEs and Trades, it should always be
    // "New":
    FIX::MDUpdateActionT GetUpdateAction() const {
      return IsZero(m_qty) ? FIX::MDUpdateActionT::Delete
                           : FIX::MDUpdateActionT::New;
    }

    // "GetOrderID": Not available, and not required (currently used for FOL
    // only):
    constexpr static OrderID GetMDEntryID() { return 0; }

    // "GetEntryRefID": Not applicable to SnapShots or Trades:
    constexpr static OrderID GetEntryRefID() { return 0; }

    // Size (Qty) and Px are supported, of course:
    QtyN GetEntrySize() const { return m_qty; }
    PriceT GetEntryPx() const { return m_px; }

    // "WasTradedFOL" : Not applicable since this is not FOL mode:
    constexpr static bool WasTradedFOL() { return false; }

    // "IsValidUpdate": Whether this MDEntry is usable: Yes of course:
    constexpr static bool IsValidUpdate() { return true; }

    // "GetTradeAggrSide":
    // We encode the TradeAggrSide in MDEntryType:
    FIX::SideT GetTradeAggrSide() const {
      switch (m_entryType) {
      case FIX::MDEntryTypeT::Bid:
        return FIX::SideT::Buy;
      case FIX::MDEntryTypeT::Offer:
        return FIX::SideT::Sell;
      default:
        return FIX::SideT::UNDEFINED;
      }
    }

    // "GetTradeExecID":
    ExchOrdID GetTradeExecID() const { return ExchOrdID(m_execID); }

    // "GetTradeSettlDate":
    // We can assume that crypto-trades are always settled @ T+0, but can ac-
    // tually ingore this request:
    constexpr static int GetTradeSettlDate() { return 0; }
  };

  //=======================================================================//
  // "MDMsg" Struct: Top-Level for SnapShots and Trade Batches:            //
  //=======================================================================//
  struct MDMsg {
    using MDEntry = MDEntryST;
    //---------------------------------------------------------------------//
    // "MDMsg" Data Flds:                                                  //
    //---------------------------------------------------------------------//
    constexpr int static MaxMDEs = 200;
    SymKey m_Symbol;
    utxx::time_val m_exchTS; // XXX: Currently for Trades only
    int m_nEntrs;            // Actual number of Entries, <= MaxMDEs
    MDEntry m_entries[MaxMDEs];

    //---------------------------------------------------------------------//
    // "MDMsg" Default Ctor:                                               //
    //---------------------------------------------------------------------//
    MDMsg() : m_Symbol(), m_exchTS(), m_nEntrs(0), m_entries() {}

    //---------------------------------------------------------------------//
    // Accessors (CBs for EConnector_MktData::UpdateOrderBooks):           //
    //---------------------------------------------------------------------//
    int GetNEntries() const {
      assert(0 <= m_nEntrs && m_nEntrs <= MaxMDEs);
      return m_nEntrs;
    }

    // SubscrReqIDs are not used:
    constexpr static OrderID GetMDSubscrID() { return 0; }

    MDEntry const &GetEntry(int a_i) const {
      assert(0 <= a_i && a_i < m_nEntrs);
      return m_entries[a_i];
    }

    // NB: Similar to FIX,  "IsLastFragment" should return "true" for TCP:
    constexpr static bool IsLastFragment() { return true; }

    //---------------------------------------------------------------------//
    // Accessors using both this "MDMsg" and "MDEntry":                    //
    //---------------------------------------------------------------------//
    // "GetSymbol" (actually, returns the AltSymbol):
    char const *GetSymbol(MDEntryST const &) const { return m_Symbol.data(); }

    // "GetSecID" : No SecIDs here:
    constexpr static SecID GetSecID(MDEntryST const &) { return 0; }

    // "GetRptSeq": No RptSeqs here:
    constexpr static SeqNum GetRptSeq(MDEntryST const &) { return 0; }

    // "GetEventTS": Exchange-Side TimeStamps are currently only supported
    // for Trades; OrderBook updates are 1-sec SnapShots:
    utxx::time_val GetEventTS(MDEntryST const &) const { return m_exchTS; }
  };

  //=======================================================================//
  // "EConnector_H1WS_TDAmeritrade" Data Flds:                             //
  //=======================================================================//
  // Config parameter, realtime feed from exchange:
  bool const    m_useBBA;
  bool const    m_trades_only;
  mutable MDMsg m_mdMsg; // Buff for curr SnapShot/Trade

public:
  //=======================================================================//
  // Non-Default Ctor, Dtor:                                               //
  //=======================================================================//
  EConnector_H1WS_TDAmeritrade
  (
    EPollReactor*                     a_reactor,
    SecDefsMgr*                       a_sec_defs_mgr,
    std::vector<std::string> const*   a_only_symbols, // NULL=UseFullList
    RiskMgr*                          a_risk_mgr,
    boost::property_tree::ptree const &a_params,
    EConnector_MktData*                a_primary_mdc = nullptr
  );

  // (*) Dtor is non-virtual (this class is "final"), and is auto-generated

  //=======================================================================//
  // "Start", "Stop", Properties, Subscription Mgmt:                       //
  //=======================================================================//
  void Start() override { TDH1WS::Start(); }
  void Stop() override { TDH1WS::Stop(); }
  bool IsActive() const override { return TDH1WS::IsActive(); }

  // Due to mulltiple inheritance, we need to provide explicit implementati-
  // ons of the Subscription Mgmt methods:
  //
  using EConnector_MktData::UnSubscribe;
  using EConnector_MktData::UnSubscribeAll;
  using EConnector_OrdMgmt::Subscribe;

private:
  // This class is final, so the following method must finally be implemented:
  void EnsureAbstract() const override {}

  // On TDAmeritrade WS login
  void OnTDWSLogOn();

  // Return true to continue, false to stop
  bool ProcessWSData(const char *a_service, const char *a_command,
                     const char *a_content, utxx::time_val a_ts_exch,
                     utxx::time_val a_ts_recv, utxx::time_val a_ts_handl);

  bool ProcessAcctActivity(char const *a_msg_body, int a_msg_len,
                           utxx::time_val a_ts_recv, utxx::time_val a_ts_exch);

  bool ProcessTrade(char const *a_msg_body, int a_msg_len,
                    utxx::time_val a_ts_recv, utxx::time_val a_ts_handl);

  bool ProcessLevel1(char const *a_msg_body, int a_msg_len,
                     utxx::time_val a_ts_recv, utxx::time_val a_ts_handl);

  bool ProcessLevel2(char const *a_msg_body, int a_msg_len,
                     utxx::time_val a_ts_recv, utxx::time_val a_ts_handl);

  char const *ProcessLevels(char const *a_msg_body, int a_msg_len,
                            FIX::MDEntryTypeT a_type);

  //=======================================================================//
  // Implementation of virtual methods from "EConnector_OrdMgmt":          //
  //=======================================================================//
  //-----------------------------------------------------------------------//
  // "NewOrder":                                                           //
  //-----------------------------------------------------------------------//
  // Write the order data to the HTTP buffer, used by NewOrderImpl and
  // ModifyOrderImpl
  const char *WriteOutOrder(char *a_chunk, SecDefD const *a_instr,
                            FIX::OrderTypeT a_ord_type, bool a_is_buy,
                            PriceT a_px,
                            QtyN a_qty, // Full Qty
                            FIX::TimeInForceT a_time_in_force,
                            int a_expire_date // Or YYYYMMDD
  );

public:
  //-----------------------------------------------------------------------//
  // "NewOrder":                                                           //
  //-----------------------------------------------------------------------//
  AOS const* NewOrder
  (
      // The originating Stratergy:
      Strategy *a_strategy,

      // Main order params:
      SecDefD const &a_instr, FIX::OrderTypeT a_ord_type, bool a_is_buy,
      PriceT a_px,
      bool a_is_aggr, // Aggressive Order?
      QtyAny a_qty,   // Full Qty

      // Temporal params of the triggering MktData event:
      utxx::time_val a_ts_md_exch = utxx::time_val(),
      utxx::time_val a_ts_md_conn = utxx::time_val(),
      utxx::time_val a_ts_md_strat = utxx::time_val(),

      // Optional order params:
      // NB: TimeInForceT::UNDEFINED must automatically resolve to a
      // Connector- specific default:
      bool a_batch_send = false, // Ignored: Never Delayed!
      FIX::TimeInForceT a_time_in_force = FIX::TimeInForceT::Day,
      int a_expire_date = 0,    // Or YYYYMMDD
      QtyAny a_qty_show = QtyAny::PosInf(),
      QtyAny a_qty_min  = QtyAny::Zero  (),
      // The following params are irrelevant -- Pegged Orders are NOT
      // supported in BitFinex:
      bool a_peg_side = true, double a_peg_offset = NaN<double>) override;

private:
  //-----------------------------------------------------------------------//
  // "NewOrderImpl": "EConnector_OrdMgmt" Call-Back (Wire Protocol):       //
  //-----------------------------------------------------------------------//
  void NewOrderImpl(EConnector_H1WS_TDAmeritrade *a_dummy,
                    Req12 *a_new_req,   // Non-NULL
                    bool a_batch_send   // Always false
  );

public:
  //-----------------------------------------------------------------------//
  // "CancelOrder":                                                        //
  //-----------------------------------------------------------------------//
  bool CancelOrder(AOS const* a_aos,    // May be NULL...
                   utxx::time_val a_ts_md_exch = utxx::time_val(),
                   utxx::time_val a_ts_md_conn = utxx::time_val(),
                   utxx::time_val a_ts_md_strat = utxx::time_val(),
                   bool a_batch_send = false) // Ignored: Never delayed!
      override;

private:
  //-----------------------------------------------------------------------//
  // "CancelOrderImpl": "EConnector_OrdMgmt" Call-Back (Wire Protocol):    //
  //-----------------------------------------------------------------------//
  void CancelOrderImpl(EConnector_H1WS_TDAmeritrade *a_dummy,
                       Req12 *a_clx_req,        // Non-NULL
                       Req12 const *a_orig_req, // Non-NULL
                       bool a_batch_send        // Always false
  );

public:
  //-----------------------------------------------------------------------//
  // "ModifyOrder":                                                        //
  //-----------------------------------------------------------------------//
  bool ModifyOrder(AOS const* a_aos,            // Non-NULL
                   PriceT a_new_px,
                   bool a_is_aggr,              // Becoming Aggr?
                   QtyAny a_new_qty,
                   utxx::time_val a_ts_md_exch = utxx::time_val(),
                   utxx::time_val a_ts_md_conn = utxx::time_val(),
                   utxx::time_val a_ts_md_strat = utxx::time_val(),
                   bool a_batch_send = false,    // Ignored: False
                   QtyAny a_new_qty_show = QtyAny::PosInf(),
                   QtyAny a_new_qty_min  = QtyAny::Zero()) override;

private:
  //-----------------------------------------------------------------------//
  // "ModifyOrderImpl": "EConnector_OrdMgmt" Call-Back (Wire Protocol):    //
  //-----------------------------------------------------------------------//
  void ModifyOrderImpl(EConnector_H1WS_TDAmeritrade *a_dummy,
                       Req12 *a_mod_req0,       // NULL in TDAmeritrade
                       Req12 *a_mod_req1,       // Non-NULL
                       Req12 const *a_orig_req, // Non-NULL
                       bool a_batch_send        // Always false
  );

public:
  //=======================================================================//
  // "CancelAllOrders": Only the externally-visible method:                //
  //=======================================================================//
  void CancelAllOrders(unsigned long a_strat_hash48 = 0,          // All  Strats
                       SecDefD const *a_instr = nullptr,          // All  Instrs
                       FIX::SideT a_side = FIX::SideT::UNDEFINED, // Both Sides
                       char const *a_segm_id = nullptr            // All  Segms
                       ) override;

public:
  //=======================================================================//
  // "FlushOrders*":                                                       //
  //=======================================================================//
  // Externally-visible method:
  utxx::time_val FlushOrders() override;

private:
  //-----------------------------------------------------------------------//
  // "FlushOrdersImpl":                                                    //
  //-----------------------------------------------------------------------//
  // "EConnector_OrdMgmt" call-back:
  utxx::time_val FlushOrdersImpl(EConnector_H1WS_TDAmeritrade *a_dummy) const;
};
} // namespace MAQUETTE
// End namespace MAQUETTE
