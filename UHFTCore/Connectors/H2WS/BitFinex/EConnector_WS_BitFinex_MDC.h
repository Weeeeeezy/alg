// vim:ts=2:et
//===========================================================================//
//          "Connectors/H2WS/BitFinex/EConnector_WS_BitFinex_MDC.h":         //
//                          WS-Based MDC for BitFinex                        //
//===========================================================================//
#pragma once

#include "Basis/Maths.hpp"
#include "Connectors/H2WS/EConnector_WS_MDC.h"
#include "Connectors/H2WS/BitFinex/EConnector_WS_BitFinex_OMC.h"
#include "Protocols/FIX/Msgs.h"

namespace MAQUETTE
{
  //=========================================================================//
  // "EConnector_WS_BitFinex_MDC" Class:                                     //
  //=========================================================================//
  class EConnector_WS_BitFinex_MDC final
      : public EConnector_WS_MDC<EConnector_WS_BitFinex_MDC>
  {
  public:
    constexpr static char const Name[] = "WS-BitFinex-MDC";
    //=======================================================================//
    // Types and Consts:                                                     //
    //=======================================================================//
    // The Native Qty type: BitFinex uses Fractional QtyA:
    constexpr static QtyTypeT QT = QtyTypeT::QtyA;
    using QR                     = double;
    using QtyN                   = Qty<QT, QR>;

    // The associated OMC type: XXX: Not sure if we need it, or it could be
    // "void":
    using OMC                    = EConnector_WS_BitFinex_OMC;

  private:
    //=======================================================================//
    // Configuration:                                                        //
    //=======================================================================//
    using  TCPWS = TCP_Connector      <EConnector_WS_BitFinex_MDC>;
    using  MDCWS = EConnector_WS_MDC  <EConnector_WS_BitFinex_MDC>;
    using  Proto = H2WS::WSProtoEngine<EConnector_WS_BitFinex_MDC>;

    // "TCP_Connector", "EConnector_WS_MDC" and "H2WS::WSProtoEngine" must have
    // full access to this class:
    friend class   TCP_Connector      <EConnector_WS_BitFinex_MDC>;
    friend class   EConnector_WS_MDC  <EConnector_WS_BitFinex_MDC>;
    friend class   H2WS::WSProtoEngine<EConnector_WS_BitFinex_MDC>;
    friend class   SecDefsMgr;

  private:
    constexpr static bool RawBooks        = false;
    //-----------------------------------------------------------------------//
    // Flags required by "EConnector_WS_MDC":                                //
    //-----------------------------------------------------------------------//
    // MktDepth is conceprually unlimited (for BitFinex "RawBooks"):
    constexpr static int MktDepth          = 0;

    // Use Sparse OrderBooks for BitFinex:
    constexpr static bool IsSparse         = true;

    // Trades info is available (in general; need to be explicitly subscribed
    // for):
    constexpr static bool HasTrades        = true;

    // No SeqNums in MktData:
    constexpr static bool HasSeqNums       = false;

    //-----------------------------------------------------------------------//
    // Flags for "EConnector_MktData" templated methods:                     //
    //-----------------------------------------------------------------------//
    constexpr static bool IsMultiCast      = false;    // It's TCP/WS
    constexpr static bool WithIncrUpdates  = true;     // Not only SnapShots
    constexpr static bool WithOrdersLog    = RawBooks; // WSMDA is FOB/FOL
    constexpr static bool WithRelaxedOBs   = false;    // Be strict?
    constexpr static bool StaticInitMode   = false;    // Appl to MultiCast only
    constexpr static bool IsFullAmt        = false;
    constexpr static bool ChangeIsPartFill = false;    // ANY Changes happen!
    constexpr static bool ChangeEncdAsNew  = false;

    //-----------------------------------------------------------------------//
    // "GetWSConfig":                                                        //
    //-----------------------------------------------------------------------//
    // NB: There is no known "TestNet" for BitFinex (except DEX): the only supp-
    // orted Env is Prod:
    static H2WSConfig const& GetWSConfig(MQTEnvT a_env);
    static std::vector<SecDefS> const* GetSecDefs();

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
    struct MDEntryST
    {
      //---------------------------------------------------------------------//
      // "MDEntryST" Data Flds:                                              //
      //---------------------------------------------------------------------//
      OrderID           m_execID;     // For Trades only
      FIX::MDEntryTypeT m_entryType;  // Bid|Offer; for Trades, the Aggressor
      PriceT            m_px;
      QtyN              m_qty;

      //---------------------------------------------------------------------//
      // "MDEntryST" Default Ctor:                                           //
      //---------------------------------------------------------------------//
      MDEntryST() :
          m_execID(0), m_entryType(FIX::MDEntryTypeT::UNDEFINED),
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
      QtyN   GetEntrySize() const { return m_qty; }
      PriceT GetEntryPx() const { return m_px; }

      // "WasTradedFOL" : Not applicable since this is not FOL mode:
      constexpr static bool WasTradedFOL() { return false; }

      // "IsValidUpdate": Whether this MDEntry is usable: Yes of course:
      constexpr static bool IsValidUpdate() { return true; }

      // "GetTradeAggrSide":
      // We encode the TradeAggrSide in MDEntryType:
      FIX::SideT GetTradeAggrSide() const {
        switch (m_entryType) {
          case FIX::MDEntryTypeT::Bid: return FIX::SideT::Buy;
          case FIX::MDEntryTypeT::Offer: return FIX::SideT::Sell;
          default: assert(false); return FIX::SideT::UNDEFINED;
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
    struct MDMsg
    {
      using MDEntry = MDEntryST;

      //---------------------------------------------------------------------//
      // "MDMsg" Data Flds:                                                  //
      //---------------------------------------------------------------------//
      // MaxMDEs would be 50 for an OrderBook SnapShot; for Trades, we process
      // each Aggression (an Aggregated Trade) individually,  so MaxMDEs is 50
      // anyway:
      constexpr int static MaxMDEs = 1000;
      // Just in case (trades come in batches)
      SymKey         m_symbol;
      utxx::time_val m_exchTS;  // XXX: Currently for Trades only
      int            m_nEntrs;  // Actual number of Entries, <= MaxMDEs
      MDEntry        m_entries[MaxMDEs];

      //---------------------------------------------------------------------//
      // "MDMsg" Default Ctor:                                               //
      //---------------------------------------------------------------------//
      MDMsg(): m_symbol(), m_exchTS(), m_nEntrs(0), m_entries() {}

      //---------------------------------------------------------------------//
      // Accessors (CBs for EConnector_MktData::UpdateOrderBooks):           //
      //---------------------------------------------------------------------//
      int GetNEntries() const {
        assert(0 <= m_nEntrs && m_nEntrs <= MaxMDEs);
        return m_nEntrs;
      }

      // SubscrReqIDs are not used:
      constexpr static OrderID GetMDSubscrID() { return 0; }

      MDEntry const& GetEntry(int a_i) const {
        assert(0 <= a_i && a_i < m_nEntrs);
        return m_entries[a_i];
      }

      // NB: Similar to FIX,  "IsLastFragment" should return "true" for TCP:
      constexpr static bool IsLastFragment() { return true; }

      //---------------------------------------------------------------------//
      // Accessors using both this "MDMsg" and "MDEntry":                    //
      //---------------------------------------------------------------------//
      // "GetSymbol":
      char const* GetSymbol(MDEntryST const&) const { return m_symbol.data(); }

      // "GetSecID" : No SecIDs here:
      constexpr static SecID GetSecID(MDEntryST const&) { return 0; }

      // "GetRptSeq": No RptSeqs here:
      constexpr static SeqNum GetRptSeq(MDEntryST const&) { return 0; }

      // "GetEventTS": Exchange-Side TimeStamps are currently only supported
      // for Trades; OrderBook updates are 1-sec SnapShots:
      utxx::time_val GetEventTS(MDEntryST const&) const { return m_exchTS; }
    };

    //=======================================================================//
    // "EConnector_WS_BitFinex_MDC" Data Flds:                               //
    //=======================================================================//
    mutable MDMsg m_mdMsg;  // Buffer for the curr SnapShot or Trade

    // Map: { ChannelID => OrderBook* }:
    mutable std::unordered_map<unsigned, OrderBook*>  m_channels;
    mutable std::unordered_map<unsigned, std::string> m_symbols;

  public:
    //=======================================================================//
    // Non-Default Ctor, Dtor:                                               //
    //=======================================================================//
    EConnector_WS_BitFinex_MDC
    (
      EPollReactor*                      a_reactor,        // Non-NULL
      SecDefsMgr*                        a_sec_defs_mgr,   // Non-NULL
      std::vector<std::string> const*    a_only_symbols,   // NULL=UseFullList
      RiskMgr*                           a_risk_mgr,       // Depends...
      boost::property_tree::ptree const& a_params,
      EConnector_MktData*                a_primary_mdc = nullptr
    );

    // The Dtor is trivial, and non-virtual (because this class is "final"):
    ~EConnector_WS_BitFinex_MDC() noexcept override;

  private:
    // This class is final, so the following method must finally be implemented:
    void EnsureAbstract()   const override {}

    //=======================================================================//
    // Msg Processing:                                                       //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    //  "ProcessWSMsg": "H2WS::WSProtoEngine" Call-Back):                    //
    //-----------------------------------------------------------------------//
    // Returns "true" to continue receiving msgs, "false" to stop:
    bool ProcessWSMsg
    (
      char const*    a_msg_body,
      int            a_msg_len,
      bool           a_last_msg,
      utxx::time_val a_ts_recv,
      utxx::time_val a_ts_handl
    );

    //-----------------------------------------------------------------------//
    // "Processtrade":                                                       //
    //-----------------------------------------------------------------------//
    // Returns "true" on success, "false" otherwise:
    //
    bool ProcessSingleTrade
    (
      char const*    a_data,
      char const*    a_end,
      OrderBook*     a_ob,
      utxx::time_val a_ts_recv,
      utxx::time_val a_ts_handl
    );

    //-----------------------------------------------------------------------//
    // "ProcessUpdate": Initialisation:                                      //
    //-----------------------------------------------------------------------//
    // Returns "true" on success, "false" otherwise:
    //
    template <bool IsSnapShot>
    char const* ProcessUpdate
    (
      char const*    a_data,
      char const*    a_end,
      OrderBook*     a_ob,
      int            a_offset,
      utxx::time_val a_ts_recv,
      utxx::time_val a_ts_handl
    );

    //-----------------------------------------------------------------------//
    // "ProcessSnapShot": Initialisation:                                    //
    //-----------------------------------------------------------------------//
    // Returns "true" on success, "false" otherwise:
    //
    bool ProcessSnapShot
    (
      char const*    a_data,
      char const*    a_end,
      OrderBook*     a_ob,
      utxx::time_val a_ts_recv,
      utxx::time_val a_ts_handl
    );

    //-----------------------------------------------------------------------//
    // "ProcessOrder": Initialisation or Steady Mode:                        //
    //-----------------------------------------------------------------------//
    // Returns the "after" ptr on success, NULL otherwise:
    //
    template <bool IsSnapShot>
    char const* ProcessOrder
    (
      char const*    a_data,
      char const*    a_end,
      OrderBook*     a_ob,
      utxx::time_val a_ts_recv,
      utxx::time_val a_ts_handl
    );

    //=======================================================================//
    // "TCP_Connector" Call-Backs (others are in "EConnector_WS_MDC"):       //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // Session-Level Methods:                                                //
    //-----------------------------------------------------------------------//
    //  "InitSession": Performs HTTP connection and WS upgrade:
    void InitSession() const;

    //  "InitLogOn" : For BitFinex, signals completion immediately:
    void InitLogOn  ();
  };
}  // End namespace MAQUETTE
