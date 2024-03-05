// vim:ts=2:et
//===========================================================================//
//            "Connectors/H2WS/Huobi/EConnector_H2WS_Huobi_OMC.h"            //
//===========================================================================//
#pragma once

#include "Connectors/H2WS/EConnector_WS_MDC.h"
#include "Connectors/H2WS/Huobi/Traits.h"
#include "Connectors/H2WS/Huobi/EConnector_H2WS_Huobi_OMC.h"

namespace MAQUETTE
{
namespace Huobi
{
  struct zlib;
}

  // MDC connector to HUOBI market
  template <Huobi::AssetClassT::type AC>
  class EConnector_WS_Huobi_MDC final :
  public EConnector_WS_MDC<EConnector_WS_Huobi_MDC<AC>>
  {
  public:
    constexpr static char const* Name = Huobi::Traits<AC>::MdcName;

    constexpr static QtyTypeT QT = QtyTypeT::QtyA;
    using QR                     = double;
    using QtyN                   = Qty<QT, QR>;

    static constexpr bool IsSpt = (AC == Huobi::AssetClassT::Spt);
    static constexpr bool IsFut = (AC == Huobi::AssetClassT::Fut);
    static constexpr bool IsSwp = (AC == Huobi::AssetClassT::CSwp ||
                                   AC == Huobi::AssetClassT::USwp);

    // Declare classes requires full access to class internals
    friend class    EConnector_MktData;
    friend class    H2WS::WSProtoEngine<EConnector_WS_Huobi_MDC<AC>>;
    friend class    TCP_Connector      <EConnector_WS_Huobi_MDC<AC>>;
    friend class    EConnector_WS_MDC  <EConnector_WS_Huobi_MDC<AC>>;

    using MDCWS   = EConnector_WS_MDC  <EConnector_WS_Huobi_MDC<AC>>;
    using   OMC   = EConnector_H2WS_Huobi_OMC<AC>;

    //-----------------------------------------------------------------------//
    // Flags required by "EConnector_H2WS_MDC":                              //
    //-----------------------------------------------------------------------//
    constexpr static int  MktDepth         = 0;
    constexpr static bool IsSparse         = true;
    constexpr static bool HasTrades        = true;
    // Spot has seq number while futures has not
    constexpr static bool HasSeqNums       = false;

    //-----------------------------------------------------------------------//
    // Flags for "EConnector_MktData" templated methods:                     //
    //-----------------------------------------------------------------------//
    constexpr static bool IsMultiCast      = false;
    constexpr static bool WithIncrUpdates  = true;
    constexpr static bool WithOrdersLog    = false;
    constexpr static bool WithRelaxedOBs   = false;
    constexpr static bool StaticInitMode   = false;
    constexpr static bool IsFullAmt        = false;
    constexpr static bool ChangeIsPartFill = false;
    constexpr static bool NewEncdAsChange  = true;
    constexpr static bool ChangeEncdAsNew  = false;

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
    struct MDEntryST
    {
      //---------------------------------------------------------------------//
      // "MDEntryST" Data Flds:                                              //
      //---------------------------------------------------------------------//
      OrderID            m_execID;     // For Trades only
      FIX::MDEntryTypeT  m_entryType;  // Bid|Offer; for Trades, the Aggressor
      PriceT             m_px;
      QtyN               m_qty;

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
      FIX::MDEntryTypeT GetEntryType() const   { return m_entryType; }

      // "GetUpdateAction": For SnapShot MDEs and Trades, it should always be
      // "New":
      FIX::MDUpdateActionT  GetUpdateAction() const
        { return IsZero(m_qty) ? FIX::MDUpdateActionT::Delete
                               : FIX::MDUpdateActionT::New; }

      // "GetOrderID": Not available, and not required (currently used for FOL
      // only):
      constexpr static OrderID GetMDEntryID()  { return 0; }

      // "GetEntryRefID": Not applicable to SnapShots or Trades:
      constexpr static OrderID GetEntryRefID() { return 0; }

      // Size (Qty) and Px are supported, of course:
      QtyN   GetEntrySize() const              { return m_qty; }
      PriceT GetEntryPx  () const              { return m_px;  }

      // "WasTradedFOL" : Not applicable since this is not FOL mode:
      constexpr static bool WasTradedFOL()     { return false; }

      // "IsValidUpdate": Whether this MDEntry is usable
      constexpr static bool IsValidUpdate()    { return true;  }

      // "GetTradeAggrSide":
      // We encode the TradeAggrSide in MDEntryType:
      FIX::SideT            GetTradeAggrSide() const
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
      constexpr static int  GetTradeSettlDate()          { return 0; }
    };

    //=======================================================================//
    // "MDMsg" Struct: Top-Level for SnapShots and Trade Batches:            //
    // Template parameters:                                                  //
    // QtyN type - should match connector QtyN                               //
    // FOB - MDMsg symbol (or altSymbol) should match paramater we pass to   //
    // updateOrderBook method                                                //
    // MAX_MD - maximum number of messages                                   //
    //=======================================================================//
    struct MDMsg
    {
      using MDEntry = MDEntryST;
      //---------------------------------------------------------------------//
      // "MDMsg" Data Flds:                                                  //
      //---------------------------------------------------------------------//
      constexpr int static MaxMDEs = 20000;
      SymKey          m_symbol;  // It can be symbol or altSymbol
      utxx::time_val  m_exchTS;  // XXX: Currently for Trades only
      int             m_nEntrs;  // Actual number of Entries, <= MaxMDEs
      MDEntry         m_entries[MaxMDEs];

      //---------------------------------------------------------------------//
      // "MDMsg" Default Ctor:                                               //
      //---------------------------------------------------------------------//
      MDMsg() : m_symbol(), m_exchTS(), m_nEntrs(0), m_entries()
      {}

      //---------------------------------------------------------------------//
      // Accessors (CBs for EConnector_MktData::UpdateOrderBooks):           //
      //---------------------------------------------------------------------//
      int GetNEntries () const
      {
        assert(0 <= m_nEntrs && m_nEntrs <= MaxMDEs);
        return m_nEntrs;
      }

      // SubscrReqIDs are not used:
      constexpr static OrderID GetMDSubscrID()  { return  0; }

      MDEntry const& GetEntry  (int a_i) const
      {
        assert(0 <= a_i && a_i < m_nEntrs);
        return m_entries[a_i];
      }

      // NB: Similar to FIX,  "IsLastFragment" should return "true" for TCP:
      constexpr static bool    IsLastFragment() { return true; }

      //---------------------------------------------------------------------//
      // Accessors using both this "MDMsg" and "MDEntry":                    //
      //---------------------------------------------------------------------//
      // "GetSymbol" (actually, Symbol or AltSymbol):
      char const*      GetSymbol(MDEntry const&) const
        { return m_symbol.data(); }

      // "GetSecID" : No SecIDs here:
      constexpr static SecID          GetSecID (MDEntry const&) { return 0; }

      // "GetRptSeq": No RptSeqs here:
      constexpr static SeqNum         GetRptSeq(MDEntry const&) { return 0; }

      // "GetEventTS": Exchange-Side TimeStamps are currently only supported
      // for Trades; OrderBook updates are 1-sec SnapShots:
      utxx::time_val GetEventTS(MDEntry const&)   const
        { return m_exchTS; }
    };

    //=======================================================================//
    // "EConnector_WS_Huobi_MDC" Data Flds:                                  //
    //=======================================================================//
    // Config parameter, realtime feed from exchange:
    bool const                        m_useBBA;
    mutable MDMsg                     m_mdMsg; // Buff for curr SnapShot/Trade

    std::unique_ptr<Huobi::zlib> m_zlib;
    mutable std::unordered_map<std::string, OrderBook*> m_channels;

  public:
    EConnector_WS_Huobi_MDC
    (
      EPollReactor*                      a_reactor,
      SecDefsMgr*                        a_sec_defs_mgr,
      std::vector<std::string>    const* a_only_symbols,   // NULL=UseFullList
      RiskMgr*                           a_risk_mgr,
      boost::property_tree::ptree const& a_params,
      EConnector_MktData*                a_primary_mdc = nullptr
    );

    ~EConnector_WS_Huobi_MDC() noexcept override;

    // This class is final, so the following method must finally be implemented:
    void EnsureAbstract()   const override {}

    //=======================================================================//
    // Configuration accessor:                                               //
    //=======================================================================//
    static H2WSConfig const& GetWSConfig(MQTEnvT a_env);

  private:
    //=======================================================================//
    // Msg Processing ("H2WS::WSProtoEngine" Call-Back):                     //
    //=======================================================================//
    //  "ProcessH2WSMsg" returns "true" to continue receiving msgs, "false" to
    //  stop:
    bool ProcessWSMsg(char const*    a_msg_body,
                      int            a_msg_len,
                      bool           a_last_msg,
                      utxx::time_val a_ts_recv,
                      utxx::time_val a_ts_handl
    );

    bool ProcessTrade(char const*     a_curr,
                      char const*     a_msg,
                      int             a_msg_len,
                      OrderBook*      a_ob,
                      utxx::time_val  a_ts_recv,
                      utxx::time_val  a_ts_handl
    );

    bool ProcessBBA(char const*     a_curr,
                    char const*     a_msg,
                    int             a_msg_len,
                    OrderBook*      a_ob,
                    utxx::time_val  a_ts_recv,
                    utxx::time_val  a_ts_handl
    );

    bool ProcessBook(char const*     a_curr,
                     char const*     a_msg,
                     int             a_msg_len,
                     OrderBook*      a_ob,
                     utxx::time_val  a_ts_recv,
                     utxx::time_val  a_ts_handl
    );

    void InitSession() const;
    void InitLogOn();
  };

  //-------------------------------------------------------------------------//
  // Aliases:                                                                //
  //-------------------------------------------------------------------------//
  using EConnector_WS_Huobi_MDC_Spt =
    EConnector_WS_Huobi_MDC<Huobi::AssetClassT::Spt>;

  using EConnector_WS_Huobi_MDC_Fut =
    EConnector_WS_Huobi_MDC<Huobi::AssetClassT::Fut>;

  using EConnector_WS_Huobi_MDC_CSwp =
    EConnector_WS_Huobi_MDC<Huobi::AssetClassT::CSwp>;

  using EConnector_WS_Huobi_MDC_USwp =
    EConnector_WS_Huobi_MDC<Huobi::AssetClassT::USwp>;

} // namespace MAQUETTE
