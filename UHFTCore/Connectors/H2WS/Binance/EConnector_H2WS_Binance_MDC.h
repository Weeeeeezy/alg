// vim:ts=2:et
//===========================================================================//
//          "Connectors/H2WS/Binance/EConnector_WS_Binance_MDC.h":           //
//                         H2WS-Based MDC for Binance                        //
//===========================================================================//
#pragma once
#include "Connectors/H2WS/EConnector_H2WS_MDC.h"
#include "Connectors/H2WS/Binance/EConnector_H2WS_Binance_OMC.h"
#include "Venues/Binance/Configs_H2WS.h"
#include "Protocols/FIX/Msgs.h"
#include "string.h"
#include <type_traits>

namespace MAQUETTE
{
  //=========================================================================//
  // Struct "H2BinanceMDC": Extended "H2Connector" (IsOMC=false):            //
  //=========================================================================//
  template<typename PSM>
  struct H2BinanceMDC final: public H2Connector<false, PSM>
  {
    //-----------------------------------------------------------------------//
    // Data Flds:                                                            //
    //-----------------------------------------------------------------------//
    mutable std::map<SymKey, OrderID> m_snapStreamID;

    //-----------------------------------------------------------------------//
    // Non-Default Ctor, Dtor:                                               //
    //-----------------------------------------------------------------------//
    H2BinanceMDC
    (
      PSM*                                a_psm,        // Must be non-NULL
      boost::property_tree::ptree const&  a_params,
      uint32_t                            a_max_reqs,
      std::string                 const&  a_client_ip   = ""
    )
    : H2Connector<false, PSM>(a_psm, a_params, a_max_reqs, a_client_ip),
      m_snapStreamID()

      // XXX: NOT initialising StreamID Counters -- that is done dynamically by
      // "OnTurningActive"...
    {}

    // Dtor is auto-generated
  };

  //=========================================================================//
  // "EConnector_H2WS_Binance_MDC" Class:                                    //
  //=========================================================================//
  template <Binance::AssetClassT::type AC>
  class  EConnector_H2WS_Binance_MDC final:
  public EConnector_H2WS_MDC
        <EConnector_H2WS_Binance_MDC<AC>,
         H2BinanceMDC<EConnector_H2WS_Binance_MDC<AC>>>
  {
  public:
    //=======================================================================//
    // Types and Consts:                                                     //
    //=======================================================================//
    // The Native Qty type: Binance uses Fractional QtyA:
    constexpr static QtyTypeT QT   = QtyTypeT::QtyA;
    using                     QR   = double;
    using                     QtyN = Qty<QT,QR>;

    // "TCP_Connector", "EConnector_H2WS_MDC" and "H2WS::WSProtoEngine" must
    // have full access to this class:
    // IsOMC=false:
    friend class     EConnector_MktData;
    friend class     H2WS::WSProtoEngine<EConnector_H2WS_Binance_MDC<AC>>;
    friend class     TCP_Connector      <EConnector_H2WS_Binance_MDC<AC>>;
    friend struct    H2Connector <false, EConnector_H2WS_Binance_MDC<AC>>;
    friend struct    H2BinanceMDC       <EConnector_H2WS_Binance_MDC<AC>>;
    friend class     EConnector_WS_MDC  <EConnector_H2WS_Binance_MDC<AC>>;

    friend class     EConnector_H2WS_MDC
                    <EConnector_H2WS_Binance_MDC<AC>,
                     H2BinanceMDC       <EConnector_H2WS_Binance_MDC<AC>>>;

    using  MDCWS   = EConnector_WS_MDC  <EConnector_H2WS_Binance_MDC<AC>>;
    using  H2Conn  = H2BinanceMDC<EConnector_H2WS_Binance_MDC<AC>>;

    using  MDCH2WS = EConnector_H2WS_MDC
                    <EConnector_H2WS_Binance_MDC<AC>,
                     H2BinanceMDC       <EConnector_H2WS_Binance_MDC<AC>>>;

    // The associated OMC type:
    static_assert(AC == Binance::AssetClassT::FutT ||
                  AC == Binance::AssetClassT::FutC ||
                  AC == Binance::AssetClassT::Spt);

    using  OMC =
      std::conditional_t<AC == Binance::AssetClassT::Spt,
        EConnector_H2WS_Binance_OMC_Spt,
        std::conditional_t<AC == Binance::AssetClassT::FutC,
          EConnector_H2WS_Binance_OMC_FutC,
          EConnector_H2WS_Binance_OMC_FutT>
      >;

    //-----------------------------------------------------------------------//
    // Asset Class Mgmt:                                                     //
    //-----------------------------------------------------------------------//
    // Are we trading Spot?
    static constexpr bool IsSpt  = (AC == Binance::AssetClassT::Spt);
    static constexpr bool IsFutC = (AC == Binance::AssetClassT::FutC);

    // NB: This is a union of FutT and FutC:
    static constexpr bool IsFut  = (AC != Binance::AssetClassT::Spt);

    static constexpr char const* Name =
        IsSpt ? "H2WS-Binance-MDC-Spt"
              : IsFutC ? "H2WS-Binance-MDC-FutC"
                       : "H2WS-Binance-MDC-FutT";

    // Different queries for different AssetClassT
    static constexpr int      QueryPfxLen = IsSpt ? 8 : 9;
    static constexpr char const* QueryPfx = IsSpt ? "/api/v3/"
                                                  : IsFutC ? "/dapi/v1/"
                                                           : "/fapi/v1/";
    static constexpr size_t MaxSubscriptions = 1024;

    //-----------------------------------------------------------------------//
    // Flags required by "EConnector_H2WS_MDC":                              //
    //-----------------------------------------------------------------------//
    // MktDepth is conceprually unlimited:
    constexpr static int  MktDepth          = 0;

    // Sparse OrderBooks are not (yet) used for Binance:
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
      // "GetSymbol"
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
    // "EConnector_H2WS_Binance_MDC" Data Flds:                              //
    //=======================================================================//
    // Config parameter, realtime feed from exchange:
    bool const                        m_useBBA;
    const unsigned                    m_snapShotFreqSeq;
    mutable bool                      m_updateMissedNotify;
    mutable std::map<SymKey, OrderID> m_lastUpdateID;

    mutable MDMsg                     m_mdMsg; // Buff for curr SnapShot/Trade
    int                               m_ssTimerFD;

  public:
    //=======================================================================//
    // Non-Default Ctor, Dtor:                                               //
    //=======================================================================//
    EConnector_H2WS_Binance_MDC
    (
      EPollReactor*                       a_reactor,       // Non-NULL
      SecDefsMgr*                         a_sec_defs_mgr,  // Non-NULL
      std::vector<std::string>    const*  a_only_symbols,  // NULL=UseFullList
      RiskMgr*                            a_risk_mgr,      // Depends...
      boost::property_tree::ptree const&  a_params,
      EConnector_MktData*                 a_primary_mdc = nullptr
    );

    // The Dtor is trivial, and non-virtual (because this class is "final"):
    ~EConnector_H2WS_Binance_MDC() noexcept override;

    // This class is final, so:
    void EnsureAbstract() const override {}

    void Start() override;
    void Stop()  override;

    // "IsActive" is provided by "EConnector_H2WS_MDC"

    //=======================================================================//
    // Configuration:                                                        //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "Get{WS,H2}Config":                                                   //
    //-----------------------------------------------------------------------//
    // NB: There is no known "TestNet" for Binance (except DEX): the only supp-
    // orted Env is Prod:
    static H2WSConfig const& GetWSConfig(MQTEnvT a_env);
    static H2WSConfig const& GetH2Config(MQTEnvT a_env);

  private:
    //=======================================================================//
    // Msg Processing ("H2WS::WSProtoEngine" Call-Back):                     //
    //=======================================================================//
    //  "ProcessH2WSMsg" returns "true" to continue receiving msgs, "false" to
    //  stop:
    bool ProcessWSMsg
    (
      char const*     a_msg_body,
      int             a_msg_len,
      bool            a_last_msg,
      utxx::time_val  a_ts_recv,
      utxx::time_val  a_ts_handl
    );

    bool ProcessH2Msg
    (
      H2Conn*         a_h2conn,    // Non-NULL
      uint32_t        a_stream_id,
      char const*     a_msg_body,
      int             a_msg_len,
      bool            a_last_msg,
      utxx::time_val  a_ts_recv,
      utxx::time_val  a_ts_handl
    );

    bool ProcessTrade(char const*         a_curr,
                      char const*         a_msg,
                      int                 a_msg_len,
                      utxx::time_val      a_ts_recv,
                      utxx::time_val      a_ts_handl
    );

    bool ProcessAggrTrade(char const*     a_curr,
                          char const*     a_msg,
                          int             a_msg_len,
                          utxx::time_val  a_ts_recv,
                          utxx::time_val  a_ts_handl
    );

    bool ProcessUpdate(char const*        a_curr,
                       char const*        a_msg,
                       int                a_msg_len,
                       utxx::time_val     a_ts_recv,
                       utxx::time_val     a_ts_handl
    );

    bool ProcessSnapShot(char const*      a_curr,
                         char const*      a_msg,
                         int              a_msg_len,
                         utxx::time_val   a_ts_recv,
                         utxx::time_val   a_ts_handl
    );

    bool ProcessBBA(char const*    a_curr,
                    char const*      a_msg,
                    int              a_msg_len,
                    utxx::time_val   a_ts_recv,
                    utxx::time_val   a_ts_handl
    );

    //=======================================================================//
    // Session Mgmt:                                                         //
    //=======================================================================//
    //  "InitWSSession": Performs HTTP connection and WS upgrade:
    void InitWSSession();

    //  "InitWSLogOn" : For Binance, signals completion immediately:
    void InitWSLogOn ();

    // "OnTurningActive": When the WS and all H2Conns become Active:
    void OnTurningActive();

    void AddSnapShotTimer ();
    void RemoveSnapShotTimer ();
    void SnapShotTimerHandler(int a_fd);
    void SnapShotTimerErrHandler
         (int a_fd, int a_err_code, uint32_t a_events, char const* a_msg);

    void RequestSnapShots();

    //=======================================================================//
    // Processing and Sending Msgs:                                          //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // Msg Processors required by "H2Connector":                             //
    //-----------------------------------------------------------------------//
    void OnRstStream
    (
      H2Conn*        a_h2conn,   // Non-NULL
      uint32_t       a_stream_id,
      uint32_t       a_err_code,
      utxx::time_val a_ts_recv
    );

    // No-op here unless we need to react to specific headers:
    void OnHeaders
    (
      H2Conn*,
      uint32_t,
      char const*,
      char const*,
      utxx::time_val
    ) {}

    //-----------------------------------------------------------------------//
    // Sender (via H2):                                                      //
    //-----------------------------------------------------------------------//
    // Returns StreamID (TimeStamp is ignored):
    //
    uint32_t SendReq
    (
      char const* a_method,
      char const* a_path = nullptr
    );
  };

  //-------------------------------------------------------------------------//
  // Aliases:                                                                //
  //-------------------------------------------------------------------------//
  using EConnector_H2WS_Binance_MDC_Spt =
    EConnector_H2WS_Binance_MDC<Binance::AssetClassT::Spt>;

  using EConnector_H2WS_Binance_MDC_FutT =
    EConnector_H2WS_Binance_MDC<Binance::AssetClassT::FutT>;

  using EConnector_H2WS_Binance_MDC_FutC =
    EConnector_H2WS_Binance_MDC<Binance::AssetClassT::FutC>;
}
// End namespace MAQUETTE
