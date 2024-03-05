// vim:ts=2:et
//===========================================================================//
//        "Connectors/H2WS/KrakenSpot/EConnector_WS_KrakenSpot_MDC.h":       //
//                         WS-Based MDC for KrakenSpot                       //
//===========================================================================//
#pragma once
#include <string>
#include <type_traits>

#include "Connectors/H2WS/EConnector_WS_MDC.h"
#include "Connectors/H2WS/EConnector_WS_MDC.hpp"
#include "Connectors/H2WS/KrakenSpot/EConnector_WS_KrakenSpot_OMC.h"
#include "Venues/KrakenSpot/Configs_WS.h"
#include "Protocols/FIX/Msgs.h"

namespace MAQUETTE
{
  //=========================================================================//
  // "EConnector_WS_KrakenSpot_MDC" Class:                                    //
  //=========================================================================//
  class  EConnector_WS_KrakenSpot_MDC final:
  public EConnector_WS_MDC<EConnector_WS_KrakenSpot_MDC>
  {
  public:
    //=======================================================================//
    // Types and Consts:                                                     //
    //=======================================================================//
    // The Native Qty type: KrakenSpot uses Fractional QtyA:
    constexpr static QtyTypeT QT   = QtyTypeT::QtyA;
    using                     QR   = double;
    using                     QtyN = Qty<QT,QR>;

    // "TCP_Connector", "EConnector_WS_MDC" and "H2WS::WSProtoEngine" must
    // have full access to this class:
    // IsOMC=false:
    friend class     EConnector_MktData;
    friend class     H2WS::WSProtoEngine<EConnector_WS_KrakenSpot_MDC>;
    friend class     TCP_Connector      <EConnector_WS_KrakenSpot_MDC>;
    friend class     EConnector_WS_MDC  <EConnector_WS_KrakenSpot_MDC>;

    using  MDCWS   = EConnector_WS_MDC  <EConnector_WS_KrakenSpot_MDC>;
    using    OMC   = EConnector_WS_KrakenSpot_OMC;

    static constexpr char const* Name = "WS-KrakenSpot-MDC";

    //-----------------------------------------------------------------------//
    // Flags required by "EConnector_WS_MDC":                                //
    //-----------------------------------------------------------------------//
    // MktDepth is conceprually unlimited:
    constexpr static int  MktDepth          = 0;

    // Sparse OrderBooks are not (yet) used for KrakenSpot:
    constexpr static bool IsSparse          = true;

    // Trades info is available (in general; need to be explicitly subscribed
    // for):
    constexpr static bool HasTrades         = true;

    constexpr static bool HasSeqNums        = false;

    //-----------------------------------------------------------------------//
    // Flags for "EConnector_MktData" templated methods:                     //
    //-----------------------------------------------------------------------//
    constexpr static bool IsMultiCast       = false;
    constexpr static bool WithIncrUpdates   = true;
    constexpr static bool WithOrdersLog     = false;
    constexpr static bool WithRelaxedOBs    = false;
    constexpr static bool ChangeIsPartFill  = false;  // SnapShots only...
    constexpr static bool NewEncdAsChange   = false;
    constexpr static bool ChangeEncdAsNew   = false;
    constexpr static bool IsFullAmt         = false;
    constexpr static int  SnapDepth         = 100;

    static std::vector<SecDefS> const* GetSecDefs();

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

      // "IsValidUpdate": Whether this MDEntry is usable: Yes of course:
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
    //=======================================================================//
    struct MDMsg
    {
      using MDEntry = MDEntryST;
      //---------------------------------------------------------------------//
      // "MDMsg" Data Flds:                                                  //
      //---------------------------------------------------------------------//
      constexpr int static MaxMDEs = 20000;
      SymKey          m_symbol;
      utxx::time_val  m_exchTS;  // XXX: Currently for Trades only
      int             m_nEntrs;  // Actual number of Entries, <= MaxMDEs
      MDEntry         m_entries[MaxMDEs];

      //---------------------------------------------------------------------//
      // "MDMsg" Default Ctor:                                               //
      //---------------------------------------------------------------------//
      MDMsg()
      : m_symbol(),
        m_exchTS   (),
        m_nEntrs   (0),
        m_entries  ()
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
      // "GetSymbol" (actually, returns the AltSymbol):
      char const*      GetSymbol(MDEntryST const&) const
        { return m_symbol.data(); }

      // "GetSecID" : No SecIDs here:
      constexpr static SecID          GetSecID (MDEntryST const&) { return 0; }

      // "GetRptSeq": No RptSeqs here:
      constexpr static SeqNum         GetRptSeq(MDEntryST const&) { return 0; }

      // "GetEventTS": Exchange-Side TimeStamps are currently only supported
      // for Trades; OrderBook updates are 1-sec SnapShots:
      utxx::time_val GetEventTS(MDEntryST const&)   const
        { return m_exchTS; }
    };

    //=======================================================================//
    // "EConnector_WS_KrakenSpot_MDC" Data Flds:                              //
    //=======================================================================//
    // Config parameter, realtime feed from exchange:
    bool const                        m_useBBA;
    mutable MDMsg                     m_mdMsg; // Buff for curr SnapShot/Trade

    // Map: { ChannelID => OrderBook* }:
    mutable std::unordered_map<int, OrderBook*> m_channels;
    mutable std::unordered_map<int, SymKey> m_symbols;

  public:
    //=======================================================================//
    // Non-Default Ctor, Dtor:                                               //
    //=======================================================================//
    EConnector_WS_KrakenSpot_MDC
    (
      EPollReactor*                       a_reactor,       // Non-NULL
      SecDefsMgr*                         a_sec_defs_mgr,  // Non-NULL
      std::vector<std::string>    const*  a_only_symbols,  // NULL=UseFullList
      RiskMgr*                            a_risk_mgr,      // Depends...
      boost::property_tree::ptree const&  a_params,
      EConnector_MktData*                 a_primary_mdc = nullptr
    );

    // The Dtor is trivial, and non-virtual (because this class is "final"):
    ~EConnector_WS_KrakenSpot_MDC() noexcept override;

    // This class is final, so:
    void EnsureAbstract() const override {}

    void Start() override;
    void Stop()  override;

    // "IsActive" is provided by "EConnector_WS_MDC"

    //=======================================================================//
    // Configuration:                                                        //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "GetWSConfig":                                                        //
    //-----------------------------------------------------------------------//
    static H2WSConfig const& GetWSConfig(MQTEnvT a_env);

  private:
    //=======================================================================//
    // Msg Processing ("H2WS::WSProtoEngine" Call-Back):                     //
    //=======================================================================//
    //  "ProcessWSMsg" returns "true" to continue receiving msgs, "false" to
    //  stop:
    bool ProcessWSMsg(char const*     a_msg_body,
                      int             a_msg_len,
                      bool            a_last_msg,
                      utxx::time_val  a_ts_recv,
                      utxx::time_val  a_ts_handl
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

    template<bool IsSnapShot>
    bool ProcessBook(char const*    a_curr,
                     char const*    a_msg,
                     int            a_msg_len,
                     OrderBook*     a_ob,
                     utxx::time_val a_ts_recv,
                     utxx::time_val a_ts_handl
    );

    //=======================================================================//
    // Session Mgmt:                                                         //
    //=======================================================================//
    void ChannelMapReset();

    //  "InitWSSession": Performs HTTP connection and WS upgrade:
    void InitWSSession();

    //  "InitWSLogOn" : For KrakenSpot, signals completion immediately:
    void InitWSLogOn ();
  };
}
// End namespace MAQUETTE
