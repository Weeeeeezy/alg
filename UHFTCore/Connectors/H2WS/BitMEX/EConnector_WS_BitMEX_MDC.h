// vim:ts=2:et
//===========================================================================//
//            "Connectors/H2WS/BitMEX/EConnector_WS_BitMEX_MDC.h":           //
//                          WS-Based MDC for BitMEX                          //
//===========================================================================//
#pragma once

#include "Connectors/H2WS/EConnector_WS_MDC.h"
#include "Connectors/H2WS/BitMEX/EConnector_H2WS_BitMEX_OMC.h"
#include "Protocols/FIX/Msgs.h"
#include <time.h>

namespace MAQUETTE
{
  //=========================================================================//
  // "EConnector_WS_BitMEX_MDC" Class:                                      //
  //=========================================================================//
  class EConnector_WS_BitMEX_MDC final
      : public EConnector_WS_MDC<EConnector_WS_BitMEX_MDC>
  {
  public:
    constexpr static char const Name[] = "WS-BitMEX-MDC";
    //=======================================================================//
    // Types and Consts:                                                     //
    //=======================================================================//
    // The Native Qty type: BitMEX uses Fractional QtyB, not QtyA!
    constexpr static QtyTypeT QT = QtyTypeT::Contracts;
    using QR                     = long;
    using QtyN                   = Qty<QT, QR>;

    static std::vector<SecDefS> const* GetSecDefs();

  private:
    //=======================================================================//
    // Configuration:                                                        //
    //=======================================================================//
    // "TCP_Connector", "EConnector_WS_MDC" and "H2WS::WSProtoEngine" must have
    // full access to this class:
    friend class   H2WS::WSProtoEngine<EConnector_WS_BitMEX_MDC>;
    friend class   TCP_Connector      <EConnector_WS_BitMEX_MDC>;
    friend class   EConnector_WS_MDC  <EConnector_WS_BitMEX_MDC>;
    using  MDCWS = EConnector_WS_MDC  <EConnector_WS_BitMEX_MDC>;

    // The associated OMC type:
    using  OMC   = EConnector_H2WS_BitMEX_OMC;

    //-----------------------------------------------------------------------//
    // Flags used by "EConnector_WS_MDC":                                    //
    //-----------------------------------------------------------------------//
    // The following flags are required for WSProtoEngine:
    constexpr static bool IsMultiCast     = false;
    constexpr static bool WithIncrUpdates = true;  // Not ONLY SnapShots
    constexpr static bool WithOrdersLog   = false;
    constexpr static bool WithRelaxedOBs  = false;

    // MktDepth is conceprually unlimited:
    constexpr static int  MktDepth        = 0;

    // Trades info is available (in general; need to be explicitly subscribed
    // for):
    constexpr static bool HasTrades       = true;

    // We do not get MktData SeqNums:
    constexpr static bool HasSeqNums      = false;

    // Do NOT use Sparse OrderBooks yet:
    constexpr static bool IsSparse        = true;
    constexpr static bool IsFullAmt       = false;

    //-----------------------------------------------------------------------//
    // "GetWSConfig":                                                        //
    //-----------------------------------------------------------------------//
    // NB: There is no known "TestNet" for BitMEX (except DEX): the only supp-
    // orted Env is Prod:
    static H2WSConfig const& GetWSConfig(MQTEnvT a_env);

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
      ExchOrdID         m_execID;     // For Trades only
      FIX::MDEntryTypeT m_entryType;  // Bid|Offer; for Trades, the Aggressor
      PriceT            m_px;
      QtyN              m_qty;

      //---------------------------------------------------------------------//
      // "MDEntryST" Default Ctor:                                           //
      //---------------------------------------------------------------------//
      MDEntryST()
      : m_execID   (0),
        m_entryType(FIX::MDEntryTypeT::UNDEFINED),
        m_px       (NaN<double>),
        m_qty      (0.0)
      {}

      //---------------------------------------------------------------------//
      // Accessors (CBs for EConnector_MktData::UpdateOrderBooks):           //
      //---------------------------------------------------------------------//
      // "GetEntryType" (returns Bid, Offer or Trade):
      FIX::MDEntryTypeT GetEntryType() const { return m_entryType; }

      // "GetUpdateAction": For SnapShot MDEs and Trades, it should always be
      // "New":
      FIX::MDUpdateActionT GetUpdateAction() const
        { return IsZero(m_qty) ? FIX::MDUpdateActionT::Delete
                               : FIX::MDUpdateActionT::New; }

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
      FIX::SideT GetTradeAggrSide() const
      {
        switch (m_entryType)
        {
          case FIX::MDEntryTypeT::Bid:   return FIX::SideT::Buy;
          case FIX::MDEntryTypeT::Offer: return FIX::SideT::Sell;
          default:  assert(false);       return FIX::SideT::UNDEFINED;
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
      constexpr int static MaxMDEs =  32768;
                                      // Just in case (trades come in batches)
      SymKey               m_symbol;
      utxx::time_val       m_exchTS;  // XXX: Currently for Trades only
      int                  m_nEntrs;  // Actual number of Entries, <= MaxMDEs
      MDEntry              m_entries[MaxMDEs];

      //---------------------------------------------------------------------//
      // "MDMsg" Default Ctor:                                               //
      //---------------------------------------------------------------------//
      MDMsg()
      : m_symbol (),
        m_exchTS (),
        m_nEntrs (0),
        m_entries()
      {}

      //---------------------------------------------------------------------//
      // Accessors (CBs for EConnector_MktData::UpdateOrderBooks):           //
      //---------------------------------------------------------------------//
      int GetNEntries() const
      {
        assert(0 <= m_nEntrs && m_nEntrs <= MaxMDEs);
        return m_nEntrs;
      }

      // SubscrReqIDs are not used:
      constexpr static OrderID GetMDSubscrID() { return 0; }

      MDEntry const& GetEntry(int a_i) const
      {
        assert(0 <= a_i && a_i < m_nEntrs);
        return m_entries[a_i];
      }

      // NB: Similar to FIX,  "IsLastFragment" should return "true" for TCP:
      constexpr static bool IsLastFragment() { return true; }

      //---------------------------------------------------------------------//
      // Accessors using both this "MDMsg" and "MDEntry":                    //
      //---------------------------------------------------------------------//
      // "GetSymbol":
      char const* GetSymbol(MDEntryST const&)   const
        { return m_symbol.data(); }

      // "GetSecID" : No SecIDs here:
      constexpr static SecID GetSecID  (MDEntryST const&) { return 0; }

      // "GetRptSeq": No RptSeqs here:
      constexpr static SeqNum GetRptSeq(MDEntryST const&) { return 0; }

      // "GetEventTS": Exchange-Side TimeStamps are currently only supported
      // for Trades; OrderBook updates are 1-sec SnapShots:
      utxx::time_val GetEventTS(MDEntryST const&) const { return m_exchTS; }
    };

    //=======================================================================//
    // "EConnector_WS_BitMEX_MDC" Data Flds:                                 //
    //=======================================================================//
    MDMsg m_mdMsg;  // Buffer for the curr SnapShot or Trade

    bool m_isInitialized;
    std::map<const SymKey, unsigned long> m_secID;// {{"XBTUSD", 88}};
    std::map<const SymKey, double> m_pxStp{{MkSymKey("XBTUSD"), 0.01},
                                           {MkSymKey("ETHUSD"), 0.05}};

  public:
    MAQUETTE::PriceT GetPxLevel(const SymKey& a_symbol, unsigned long a_id);

    //=======================================================================//
    // Non-Default Ctor, Dtor:                                               //
    //=======================================================================//
    // NB: If "a_only_symbols" is not NULL/empty, only the SecDefs with Symbols
    // from this list will be created. This may be useful eg to prevent constr-
    // uction of all "SecDefD"s/"OrderBook"s by default (because there are too
    // many of them). In any case, the list may be further restricted by Settl-
    // Dates configured in Params:
    //
    EConnector_WS_BitMEX_MDC
    (
      EPollReactor*                      a_reactor,        // Non-NULL
      SecDefsMgr*                        a_sec_defs_mgr,   // Non-NULL
      std::vector<std::string> const*    a_only_symbols,   // NULL=UseFullList
      RiskMgr*                           a_risk_mgr,       // Depends...
      boost::property_tree::ptree const& a_params,
      EConnector_MktData*                a_primary_mdc = nullptr
    );

    // The Dtor is trivial, and non-virtual (because this class is "final"):
    ~EConnector_WS_BitMEX_MDC() noexcept override {}

  private:
    // This class is final, so the following method must finally be implemented:
    void EnsureAbstract()   const override {}

    //=======================================================================//
    // Msg Processing ("H2WS::WSProtoEngine" Call-Back):                     //
    //=======================================================================//
    //  "ProcessWSMsg" returns "true" to continue receiving msgs, "false" to
    //  stop:
    bool ProcessWSMsg
    (
      char const*    a_msg_body,
      int            a_msg_len,
      bool           a_last_msg,
      utxx::time_val a_ts_recv,
      utxx::time_val a_ts_handl
    );

    template<bool IsSnapShot, bool IsDelete, bool IsInsert>
    bool ProcessUpdates
    (
      char const*    a_curr,
      char const*    a_end,
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
    void InitSession();

    //  "InitLogOn" : For BitMEX, signals completion immediately:
    void InitLogOn  ();
  };
}  // End namespace MAQUETTE
