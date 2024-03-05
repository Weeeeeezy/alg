// vim:ts=2:et:sw=2
//===========================================================================//
//                               "StratEnv.h":                               //
//                     Strategy Environment and its API                      //
//===========================================================================//
#ifndef MAQUETTE_STRATEGYADAPTOR_STRATENV_H
#define MAQUETTE_STRATEGYADAPTOR_STRATENV_H

#include "StrategyAdaptor/StratMSEnvTypes.h"
#ifndef  ODB_COMPILER
#include "Common/Maths.h"
#include "Connectors/AOS.h"
#include "Connectors/OrderMgmtTypes.h"
#include "Connectors/XConnector.h"
#include "StrategyAdaptor/OrderBooksShM.h"
#include "StrategyAdaptor/TradeBooksShM.h"
#include "Infrastructure/SecDefTypes.h"

#include <utxx/compiler_hints.hpp>
#include <utxx/rate_throttler.hpp>
#include <utxx/time_val.hpp>
#include <Infrastructure/Logger.h>
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>
#include <atomic>

class Json;
#endif  // !ODB_COMPILER

namespace MAQUETTE
{
  class ConnEmu;

  //=========================================================================//
  // "MAQUETTE::StratEnv" Class:                                            //
  //=========================================================================//
  class StratEnv
  {
  public:
    //=======================================================================//
    // User-Visible Data Types:                                              //
    //=======================================================================//
    //=======================================================================//
    // "MAQUETTE::StratEnv::QSym": Key for All Maps:                        //
    //=======================================================================//
    typedef StratMSEnvTypes::QSym QSym;

    // It appears that Exchange / Broker Commission / Fee Rates   are often NOT
    // specified in the "SecDef" or "ExecReport" records (eg not for FORTS), so
    // so they need to be set here explicitly by the user (see "SetFeeRates").
    // The rates may be PerQty (eg for Futures or Options), PerValue (eg for EQ
    // or FX), or PerTrade (as with some brokers):
    //
    enum class CommRateT
    {
      PerQty   = 0,
      PerValue = 1
    };

  private:
    // If a given "QSym" is a derivative, the Symbol on which it is under-writ-
    // ten must also be subscribed for, and available as a "QSym", otherwise we
    // will not get MktData required for Risk Management.
    // The following map provids  Underlyings for all Options, plus XXX a work-
    // around for buggy SecDefs which may have empty Expiration Times. However,
    // "QSymInfo" below does not replace a full "SecDef"  (which would include
    // eg on-line Implied Vols). For consistency, Futures are also included in-
    // to the "QSymInfo" (mapped to themselves):
    // For convenience, this also provides further info on the Derivative  so
    // that the Application would not need to wait for the whole SecDef to arr-
    // ive:
    //
    struct QSymInfo
    {
      QSym                m_This;
      char                m_CFICode[7];
      Events::Ccy         m_Ccy;
      QSym                m_Underlying;
      utxx::time_val      m_Expir;
      double              m_Strike;     // 0 for non-Options
      double              m_ImplVol;    // 0 for non-Options -- "static" value
      double              m_IR;         // XXX: 0 if not available
      // Commission / Fee data:
      CommRateT           m_CommRateType;
      double              m_ExchCommRate;
      double              m_BrokerCommRate;

      QSymInfo()  { memset(this, '\0', sizeof(QSymInfo)); }
    };

    typedef std::map<QSym, QSymInfo>    QSymInfoMap;

  public:
    //=======================================================================//
    // "MAQUETTE::StratEnv::OurTrade":                                      //
    //=======================================================================//
    // Data are transferred from "AOS" to "OurTrade"s when an order is comple-
    // tely filled, or a partially-filled order has been cancelled. Partially-
    // filled orders which are still active are listed under AOSes    (because
    // they can still be amended). At the same time, partial fills are immedi-
    // ately reflected in Positions:
    //
    struct OurTrade
    {
      ExchOrdID           m_execID;
      QSym                m_qsym;
      Events::AccCrypt    m_accCrypt;
      bool                m_isBuy;
      long                m_cumQty;
      long                m_lastQty;
      double              m_avgPx;
      double              m_lastPx;
      double              m_exchFee;
      double              m_brokFee;
      utxx::time_val      m_ts;         // Corresponds to the last fill
      bool                m_isManual;   // Was it a manual order?
    };

#   ifndef ODB_COMPILER
    //=======================================================================//
    // "MAQUETTE::StratEnv::ConnEventMsg" Class:                             //
    //=======================================================================//
    // This is a local expended version of "Events::EventMsg". It contains:
    // (1) an incoming "EventMsg"; and
    // (2) "QSym" the Event refers to (for convenience);
    // In most cases, the user just needs to do GetTag() and then refer to the
    //     corresp data structure in the Local Books area:
    //
    class ConnEventMsg: public Events::EventMsg
    {
    private:
      friend class StratEnv;

      //---------------------------------------------------------------------//
      // Extra Data Fld:                                                     //
      //---------------------------------------------------------------------//
      QSym  m_QSym;

    public:
      //---------------------------------------------------------------------//
      // Default Ctor is just a PlaceHolder:                                 //
      //---------------------------------------------------------------------//
      // NB: Non-trivial objects of this class are constructed by "GetNextEvent"
      // methods of the "StratEnv" class:
      //
      ConnEventMsg()
      : Events::EventMsg(),
        m_QSym          ()
      {}

      //---------------------------------------------------------------------//
      // Accessors:                                                          //
      //---------------------------------------------------------------------//
      // Methods inherited from "Events::EventMsg":
      //
      // GetTag()
      // GetClientRef()
      // GetErrReqPtr()
      // GetOrderBookPtr()
      // GetTradePtr()
      // GetSecDefPtr()
      // GetAOSPtr()

      void SetTag(Events::EventT tag) { Events::EventMsg::SetTag(tag); }

      // Extracting the "QSym":
      QSym const& GetQSym() const     { return m_QSym; }
    };

    //=======================================================================//
    // "MAQUETTE::StratEnv::ConnDescr": Connector Descriptor (Internal):     //
    //=======================================================================//
    // XXX: At the moment, "ConnDescr"s are COPYABLE -- so use "shared_ptr"s
    // in them, not "unique_ptr"s:
    //
    class ConnDescr
    {
    private:
      // Default Ctor is hidden:
      ConnDescr() = delete;

    public:
      //---------------------------------------------------------------------//
      // Data Flds:                                                          //
      //---------------------------------------------------------------------//
      StratEnv*                                          m_owner;
      // Connector Name:
      std::string                                        m_connName;

      // Is it a "real" Connector (with a ShM segment) or a DB-Recording one?
      // (This only applies to MDs; OEs are always "real"):
      bool                                               m_is_real;

      /// Connector mode
      ConnModeT                                          m_conn_mode;
      /// Connector reference
      int                                                m_ref;

      // Within the OE segment, we may need ptrs to AOSMap and AOSMapMtx; NB:
      // AOSMap is read-only. These ptrs are NOT owned (aliases only), so raw
      // ptrs are OK:

      // Connector's ShMem segment: for both MDs (generational mkt data are
      // taken from there) and OEs (AOSes are allocated there):
      std::shared_ptr<ManagedShm>                        m_segment;
      AOS::AOSStorage                                    m_aos_storage;
      AOS::ReqStorage                                    m_req_storage;
      AOS::TradeStorage                                  m_trade_storage;
      AOS::ReqErrBuffer*                                 m_err_storage;

      // UpLink Queue   (OE only):
      std::shared_ptr<fixed_message_queue>               m_reqQ;

      // UDS and EventFD used to notify the corresp OE on Reqs submitted by
      // the Strategy; UDS is for receiving the EventFD (TODO: and also for
      // authetication); for OE only:
      int                                                m_uds;
      int                                                m_eventFD;

      // Do graceful Unsubscribe?
      bool                                               m_graceful;

      EnvType            Env()        const { return m_owner->m_envType;    }
      std::string const& StratName()  const { return m_owner->m_stratName;  }
      bool               DebugLevel() const { return m_owner->m_debugLevel; }
      std::string const& Name()       const { return m_connName;            }
      ConnModeT          Mode()       const { return m_conn_mode;           }
      bool               Real()       const { return m_is_real;             }
      int                Ref()        const { return m_ref;                 }

      //---------------------------------------------------------------------//
      // Ctors / Dtor:                                                       //
      //---------------------------------------------------------------------//
      // Non-Default Ctor:
      ConnDescr
      (
        StratEnv*       env,
        string   const& conn_name,
        ConnModeT       conn_mode,
        int             conn_ref,
        bool            is_real = true    // false - emulated mode
      );

      ConnDescr(ConnDescr&& a_rhs);
      ConnDescr(ConnDescr const& a_rhs)      = delete;
      void operator=(ConnDescr&& a_rhs);
      void operator=(ConnDescr const& a_rhs) = delete;

      // Dtor: close u_uds/m_eventFD
      ~ConnDescr();
    };

    //=======================================================================//
    // Data Types for Shared Memory Structures:                              //
    //=======================================================================//
    // The following Maps are allocated in Shared Memory, so require special
    // Allocators. All Symbols are qualified with the MD#  (from which they
    // originate),   so that the Strategy can operate with identical Symbols
    // attributed to different Exchanges:
    //-----------------------------------------------------------------------//
    // "StrVec", "QSymVec":                                                  //
    //-----------------------------------------------------------------------//
  private:
    typedef StratMSEnvTypes::ShMAllocator           ShMAllocator;
    typedef StratMSEnvTypes::StringAllocator        StringAllocator;
    typedef StratMSEnvTypes::QSymAllocator          QSymAllocator;
  public:
    typedef StratMSEnvTypes::ShMString              ShMString;
    typedef StratMSEnvTypes::StrVec                 StrVec;
    typedef StratMSEnvTypes::QSymVec                QSymVec;

    //-----------------------------------------------------------------------//
    // "PositionsMap":                                                       //
    //-----------------------------------------------------------------------//
    typedef StratMSEnvTypes::PositionInfo           PositionInfo;
    typedef StratMSEnvTypes::RiskLimitsPerSymbol    RiskLimitsPerSymbol;
  private:
    typedef StratMSEnvTypes::PositionsMapAllocator  PositionsMapAllocator;
  public:
    typedef StratMSEnvTypes::PositionsMap           PositionsMap;

    //-----------------------------------------------------------------------//
    // "GreeksMap" (by Underlying):                                          //
    //-----------------------------------------------------------------------//
    struct GreeksInfo
    {
      double        m_Delta;
      double        m_Gamma;
      double        m_Vega;

      // Default Ctor:
      GreeksInfo()  { Clear(); }
      void Clear()  { memset(this, '\0', sizeof(GreeksInfo)); }
    };

  private:
     typedef
       BIPC::allocator<std::pair<QSym const, GreeksInfo>,
                       BIPC::fixed_managed_shared_memory::segment_manager>
       GreeksMapAllocator;
  public:
    typedef
      BIPC::map<QSym, GreeksInfo, std::less<QSym>, GreeksMapAllocator>
      GreeksMap;

    //-----------------------------------------------------------------------//
    // "OurTradesMap":                                                       //
    //-----------------------------------------------------------------------//
    // Inner Vector:
  private:
    typedef
      BIPC::allocator<OurTrade,
                      BIPC::fixed_managed_shared_memory::segment_manager>
      OurTradeAllocator;
  public:
    typedef BIPC::vector<OurTrade, OurTradeAllocator>   OurTradesVector;

    // Outer Map:
  private:
    typedef
      BIPC::allocator<std::pair<QSym const, OurTradesVector>,
                      BIPC::fixed_managed_shared_memory::segment_manager>
      OurTradesMapAllocator;
  public:
    typedef
      BIPC::map<QSym, OurTradesVector, std::less<QSym>, OurTradesMapAllocator>
      OurTradesMap;

    //-----------------------------------------------------------------------//
    // "AOSMapQPV":                                                          //
    //-----------------------------------------------------------------------//
  private:
    typedef StratMSEnvTypes::AOSQPVAllocator            AOSQPVAllocator;
  public:
    typedef StratMSEnvTypes::AOSPtrVec                  AOSPtrVec;
    typedef StratMSEnvTypes::AOSMapQPV                  AOSMapQPV;

    //-----------------------------------------------------------------------//
    // "OrderBooksMap":                                                      //
    //-----------------------------------------------------------------------//
  private:
    typedef StratMSEnvTypes::OrderBookAllocator         OrderBookAllocator;
  public:
    typedef StratMSEnvTypes::OrderBooksMap              OrderBooksMap;

    //-----------------------------------------------------------------------//
    // "TradeBooksMap":                                                      //
    //-----------------------------------------------------------------------//
  private:
    typedef
      BIPC::allocator<std::pair<QSym const, TradeEntry const*>,
                      BIPC::fixed_managed_shared_memory::segment_manager>
      TradeAllocator;
  public:
    typedef
      BIPC::map<QSym, TradeEntry const*, std::less<QSym>, TradeAllocator>
      TradeBooksMap;

    //-----------------------------------------------------------------------//
    // "SecDefsMap":                                                         //
    //-----------------------------------------------------------------------//
  private:
    typedef
      BIPC::allocator<std::pair<QSym const, SecDef const*>,
                      BIPC::fixed_managed_shared_memory::segment_manager>
      SecDefAllocator;
  public:
    typedef
      BIPC::map<QSym, SecDef const*, std::less<QSym>,   SecDefAllocator>
      SecDefsMap;

    //-----------------------------------------------------------------------//
    // "CcyVec":                                                             //
    //-----------------------------------------------------------------------//
  private:
    typedef
      BIPC::allocator<Events::Ccy,
                      BIPC::fixed_managed_shared_memory::segment_manager>
      CcyAllocator;
  public:
    typedef BIPC::vector<Events::Ccy, CcyAllocator>     CcyVec;

    //-----------------------------------------------------------------------//
    // "CashMap":                                                            //
    //-----------------------------------------------------------------------//
    // Provides Cash demonimated in different Currencies (which are listed in a
    // "CcyVec"):
    //
    enum class CashInfoCompT
    {
      RealizedPnL = 1, // With  trans costs
      NLV         = 2, // Similar
      InitMargin  = 3,
      VarMargin   = 4,
      VaR         = 5
    };

    // Risk Limits Per Ccy:
    //
    struct RiskLimitsPerCcy
    {
      double m_stopLossNLV;        // NLV in this Ccy triggering StopLoss event
      double m_maxNLVLossRate;     // Per sec; assumed to be < 0

      // Default Ctor: NO LIMITS:
      RiskLimitsPerCcy()
      : m_stopLossNLV   (-Arbalete::Inf<double>),
        m_maxNLVLossRate(-Arbalete::Inf<double>)
      {}

      // Need to declare assignment explicitly:
      RiskLimitsPerCcy& operator=(RiskLimitsPerCcy const&) = default;
    };

    // "CashInfo": Provides RealizedPnL, NLV, IM, VM, VaR etc per Ccy (unlike
    // "PositionInfo" which provides RealizedPnL and VM per QSym -- then they
    // are aggregated and exported into per-Ccy "CashInfo"s):
    //
    struct CashInfo
    {
      double             m_realizedPnL;  // Corresp to last pos=0; for this
      double             m_NLV;          // Current NLV  value
      double             m_IM;           // Current value of Initial Margin
      double             m_VM;           // NB: CurrVM is sourced from "Posit-
      double             m_VaR;          // Current VaR  for this Ccy
      RiskLimitsPerCcy   m_RL;
      // TODO: Add NLVLossRateThrottle...

      // Default Ctor:
      CashInfo()
      : m_realizedPnL    (0.0),
        m_NLV            (0.0),
        m_IM             (0.0),
        m_VM             (0.0),
        m_VaR            (0.0),
        m_RL             ()
      {}
    };

  private:
    using CashMapAllocator =
      BIPC::allocator<std::pair<Events::Ccy const, CashInfo>,
                      BIPC::fixed_managed_shared_memory::segment_manager>;

    //=======================================================================//
    // "LoadSecDefs":                                                        //
    //=======================================================================//
    /// Load SecDef from a SecDef source (File, DB, etc) to local map
    ///
    /// Extracted SecDefs are cached in StratEnv's local memory (not ShM)
    //
    void LoadSecDefs(SecDefSrcT a_src, std::string const& a_name);

    /// Get SecDef from local map
    SecDef const* GetSecDef(SymKey const& symbol) const;

  public:
    typedef
      BIPC::map
        <Events::Ccy, CashInfo, std::less<Events::Ccy>, CashMapAllocator>
      CashMap;

    //-----------------------------------------------------------------------//
    // Mode of Operation:                                                    //
    //-----------------------------------------------------------------------//
    enum class StartModeT
    {
      Safe    = 0, // Preserve the ShM data if possible, process the incoming
                   // events, but do not send out any Requests
      Normal  = 1, // Preserve the ShM data if possible, then proceed normally
      Clean   = 2  // Clean-up the ShM data, do NOT use DB init either,  then
    };             // proceed normally

    static StartModeT StartModeOfStr(std::string const& str);

    static  bool                      s_ExitSignal;

  private:
    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // Connectors:                                                           //
    //-----------------------------------------------------------------------//
    EnvType                           m_envType;    // Environment
    std::string                       m_stratType;  // Type (eg "MM")
    std::string                       m_stratSubType;
    std::string                       m_stratName;  // Name of THIS Strategy
    ClientID                          m_strat_cid;
    SessionID                         m_strat_sid;
    std::string                       m_msgQName;   // Strat MsgQ Name
    StartModeT                        m_startMode;  // Start Mode, see above
    bool                              m_emulated;   // Emulated Mode?
    fixed_message_queue*              m_eventQ;     // EventQ  (Conn->Strat)
    std::vector<ConnDescr>            m_MDs;       // Mkt   Data Connectors
    std::vector<ConnDescr>            m_InternalOEs;// Order Mgmt Connectors...
    std::vector<ConnDescr>*           m_OEs;       // Order Mgmt Connectors...
    int                               m_debugLevel;

    // Maps of user-provided MD and OE numbers to internal ones (this allows
    // the user to specify repeated Connector Names, which are not allowed in-
    // ternally):
    std::vector<int>                  m_MDsMap;
    std::vector<int>                  m_OEsMap;
    QSymInfoMap                       m_QSymInfoMap;// As discussed above

    // Random Number Generator and the Random-Values-Generaling Lambda:
    mutable std::mt19937                                    m_RNG;
    mutable std::uniform_int_distribution<Events::OrderID>  m_Distr;

    // Exit Flag and the C3 Thread:
    mutable std::atomic<bool>         m_Exit;

    Arbalete::Thread*                 m_C3Thread;
    /// Number of seconds to wait for request timeout
    int                               m_req_timeout_sec;

    //-----------------------------------------------------------------------//
    // Positions, Order Statuses and PnL:                                    //
    //-----------------------------------------------------------------------//
    // NB: AOSes are classified first by QSym, and then by the original Order-
    // ID. These Maps are allocated in ShMem (in the "StratName data" segment)
    // so that they are visible to external observers,  BUT XXX:  they are NOT
    // protected by any locks,  so an observer must guard against a  potential
    // segfault while traversing any of the maps!
    //
    // The following vectors are non-updatable:
    StrVec*                           m_MDNames;       // "MDNames"
    StrVec*                           m_OENames;       // "OENames"

    // Subscriptions are dynamic:
    QSymVec*                          m_QSyms;          // "QSymsVec"
    CcyVec*                           m_Ccys;           // "CcyVec"

    int                               m_pos_rate_thr_win;
    PositionsMap*                     m_positions;      // "PositionsMap"
    GreeksMap*                        m_Greeks;         // "GreeksMap"
    OurTradesMap*                     m_ourTrades;      // "OurTradesMap"
    AOSMapQPV*                        m_aoss;           // "AOSMapQPV"
    OrderBooksMap*                    m_orderBooks;     // "OrderBooksMap"
    TradeBooksMap*                    m_tradeBooks;     // "TradeBooksMap"

    // NB: "m_secDefs" is a ShM map: { QSym => SecDef const* },   where the ptr
    // can point either to a a SecDef stored within the Connector's ShM, or (in
    // case of emulation or a DB Connector) here in "m_secDefsBuff":
    //
    SecDefsMap*                       m_secDefs;        // "SecDefsMap"
    SymbolsMap                        m_secdef_cache;   // Local!

    // Cash, NLV, InitMargin, VarMargin and VaR by Ccy:
    CashMap*                          m_cashInfo;       // "CashInfo"

    // Total Cash, Total NLV and Total VaR in a single Ccy (eg USD), if maint-
    // ained; so the following Map is either empty or singleton:
    CashMap*                          m_totalInfo;      // "TotalInfo"

    // Strategy Status Line:
    int const                         m_StatusLineSz  = 256;
    ShMString*                        m_statusLine;     // "StatusLine"

    // Whether the Strategy's ShM segments were cleanly unmounted at the end
    // of the previous run:
    bool*                             m_clean;          // "Clean"
    ConnEmu*                          m_connEmu;        // For Emulated Mode,
                                                        // otherwise NULL
    // Ptrs to dynamically-updatable variables (while the Strategy is running):
    std::map<std::string, double*>                      m_updatables;
    mutable std::map<std::string, double>               m_nextUpdate;

    //-----------------------------------------------------------------------//
    // ShM Segments and Allocators:                                          //
    //-----------------------------------------------------------------------//
    // Segment and Allocator for the Strategy's own ShM:
    std::unique_ptr<BIPC::fixed_managed_shared_memory>  m_segment;
    std::unique_ptr<ShMAllocator>                       m_shMAlloc;

    //-----------------------------------------------------------------------//
    // Deleted / Hidden Stuff:                                               //
    //-----------------------------------------------------------------------//
    // "StratEnv" objects are completely un-copyable, un-assignable and un-com-
    // parable:
    StratEnv()                = delete;
    StratEnv(StratEnv const&) = delete;
    StratEnv(StratEnv&&)      = delete;

    StratEnv& operator= (StratEnv const&);
    StratEnv& operator= (StratEnv&&);

    bool      operator==(StratEnv const&) const;
    bool      operator!=(StratEnv const&) const;

    /// Check if \a a_result is a successful reply from the server
    void CheckOK(const eterm& a_result, const char* a_where) const;
  public:
    //=======================================================================//
    // API for use by Strategies:                                            //
    //=======================================================================//
    //=======================================================================//
    // Non-Default Ctor, Dtor:                                               //
    //=======================================================================//
    // XXX: we can also endow the "StratEnv" with OrderMgmt Priority which will
    // be used to submit Requests to OEs, and to get Wake-Ups from MDs).
    // However, for the moment all StratEnvs are treated "equally"  by the Con-
    // nectors they are subscribed to.
    // NB: "emulate_from", "emulate_to" are for Strategies back-testing on his-
    //     torical data (from a DB); in that case, MDs must be the "*-DB" ones,
    //     OEs are ignored, all real-time and ShM params are ignored as well:
    //
    StratEnv
    (
      std::string              const& a_strat_type,    // Type (eg "MM")
      std::string              const& a_strat_subtype, // Eg "GatlingMM"
      std::string              const& a_strat_name,    // Eg "mm5"
      EnvType                         a_env_type,      // Eg Prod, QA, ...
      std::string              const& a_q_name,        // Strat MsgQ Name
      std::vector<std::string> const& a_mdcs,          // Names of MDs
      std::vector<std::string> const& a_omcs,          // Names of OEs
      SecDefSrcPair            const& a_secdef_src,    // SecDef source
      StartModeT                      a_start_mode,
      int                             a_debug_level         = 0,
      int                             a_rt_prio             = 0,
      int                             a_cpu_affinity        = -1,
      int                             a_shmem_sz_mb         = 16, // 16Mb
      int                             a_pos_rate_thr_window = 1   // 1 sec
    );

    // Dtor:
    ~StratEnv();

    //-----------------------------------------------------------------------//
    // "SetCommRates":                                                       //
    //-----------------------------------------------------------------------//
    // AFTER a "QSym" has been subscribed, the user can explicitly set the com-
    // mission rates (because they may not be accurately known otherwise):
    //
    void SetCommRates
    (
      QSym const& qsym,
      CommRateT   rate_type,
      double      exch_rate,
      double      broker_rate
    );

    //-----------------------------------------------------------------------//
    // Setting the Risk Limits:                                              //
    //-----------------------------------------------------------------------//
    // Per a given Symbol:
    void SetRiskLimits(QSym const& a_qsym,    RiskLimitsPerSymbol const& a_rl);

    // Per a given Ccy:
    void SetRiskLimits(Events::Ccy const& a_ccy, RiskLimitsPerCcy const& a_rl);

  public:
    /// Number of seconds to wait for a request timeout (default: 5)
    void ReqTimeoutSec(size_t a_sec) { m_req_timeout_sec = std::min(1ul,a_sec);}

    //=======================================================================//
    // Subscription Management:                                              //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "Subscribe":                                                          //
    //-----------------------------------------------------------------------//
    // Subscribes the Strategy to the given Symbol via the Connector (MD) ide-
    // ntified by its index (0-based) in the list of MDs, and similarly,  mark
    // the given OE as being used for sending orders related to this Symbol.
    // If an MD or OE list was empty, ie no corresp Connectors are to be used,
    // use (-1) for mdcN / omcN, resp.
    //
    // IMPORTANT: If 2 "QSym"s have same "Symbol" but different MDs, they must
    // have different OEs as well. That is, for a given Symbol,  OE  uniquely
    // defines MD -- so when a response with some "Symbol"  is received from a
    // known OE, we know the MD as well, and can re-construct the whole QSym.
    //
    // IMPORTANT: For Risk Management purposes, if a Derivative "QSym" is subs-
    // cribed, its Underlying QSym must be subscribed as well.  So the order of
    // subscriptions does matter: a uniquely-named Underlying must be subscribed
    // before its Derivatives (eg Futures first, then Options on those Futures):
    //
    void  Subscribe(QSym& qsym);

    //-----------------------------------------------------------------------//
    // "Unsubscribe":                                                        //
    //-----------------------------------------------------------------------//
    // The given "QSym" is unsubscribed from its MD (memoised internally,  so
    // not provided as an arg), and dis-associated with its OE (also memoised
    // internally):
    //
    void Unsubscribe(QSym const& qsym);

    //-----------------------------------------------------------------------//
    // "UnsubscribeAll":                                                     //
    //-----------------------------------------------------------------------//
    // NB: This is for MDs only; for OEs, "UnsubscribeAll" would be identical
    // to just "Unsubscribe" (which should be used instead) because  no Symbols
    // are involved anyway:
    //
    void UnsubscribeAll(ConnDescr const& a_conn);

    //=======================================================================//
    // "GetNextEvent":                                                       //
    //=======================================================================//
    // (*) If "deadline" is empty, the method will wait indefinitely for the
    //     next msg, and will return "true" (unless an exception occurs);
    // (*) Otherwise, it will wait until the "deadline" expiration for the
    //     Event to come, and return "true" iff it has been received (into
    //     "targ");
    // (*) returns "true" iff the mesg has been successfully received, or "fal-
    //     se" otherwise (incl time-outs or receiving / processing errors);
    // (*) this method does NOT propagate any exceptions;
    // (*) all internal state variables are updated automatically when a new
    //     Event is received:
    //
    bool GetNextEvent
    (
      ConnEventMsg*         targ,
      utxx::time_val        deadline = utxx::time_val()
    );

    //=======================================================================//
    // Meta-Data, Position and PnL Services:                                 //
    //=======================================================================//
    // Emulated Mode?
    bool InEmulatedMode() const { return m_emulated; }

    //-----------------------------------------------------------------------//
    // For all Symbols:                                                      //
    //-----------------------------------------------------------------------//
    QSymInfoMap   const& GetQSymInfoMap()     const { return  m_QSymInfoMap; }
    PositionsMap  const& GetPositions()       const { return *m_positions;   }
    OurTradesMap  const& GetOurTrades()       const { return *m_ourTrades;   }
    AOSMapQPV     const& GetAOSes()           const { return *m_aoss;        }

    OrderBooksMap const& GetOrderBooks()      const { return *m_orderBooks;  }
    TradeBooksMap const& GetTradeBooks()      const { return *m_tradeBooks;  }
    SecDefsMap    const& GetSecDefs()         const { return *m_secDefs;     }
    CashMap       const& GetCashInfos()       const { return *m_cashInfo;    }
    CashMap       const& GetTotalCashInfos()  const { return *m_totalInfo;   }

    //-----------------------------------------------------------------------//
    // For a given "QSym":                                                   //
    //-----------------------------------------------------------------------//
    // (Exception is thrown if the corresp "qsym" is not found):
    QSymInfo          const&  GetQSymInfo   (QSym const& qsym) const;
    long                      GetPosition   (QSym const& qsym) const;
    int                       GetPosAcqRate (QSym const& qsym) const;
    double                    GetRealizedPnL(QSym const& qsym) const;
    OurTradesVector   const&  GetOurTrades  (QSym const& qsym) const;
    AOSPtrVec         const&  GetAOSes      (QSym const& qsym) const;
    OrderBook::Entry  const&  GetBestBid    (QSym const& qsym) const;
    OrderBook::Entry  const&  GetBestAsk    (QSym const& qsym) const;
    SecDef  const&            GetSecDef     (QSym const& qsym) const;
    OrderBookSnapShot const&  GetOrderBook  (QSym const& qsym) const;
    double                    GetAvgPosPx   (QSym const& qsym,
                                             bool with_trans_costs)  const;
    //-----------------------------------------------------------------------//
    // TODO: Install RisksMap and VaRMap...                                  //
    //-----------------------------------------------------------------------//
  public:
    //=======================================================================//
    // Cash, NLV, InitMargin, VarMargin, VaR, ... Management:                //
    //=======================================================================//
    // Get the Ccy for a given QSym:
    Events::Ccy GetCcy (QSym const& qsym) const;

    // Computing the current RealizedPnL,  NLV, InitMargin, VarMargin and VaR
    // values (for a given "ccy" -- the Totals, if required, must be computed
    // by the Strategy itself, unless there is only 1 Ccy, in which case they
    // are propagated automatically).
    // The values computed are memoised internally in the corresp "CashInfo",
    // that's why these methods are NOT "const".
    // If the computed values need to be recorded in the DB, the "acc_crypt"
    // must be specified for accounting purposes (if it is 0, no DB recording
    // is performed)   (except for "GetCash" and "GetVarMargin" which do not
    // compute anything -- they return readily-available values):
    //
    CashInfo const& GetCashInfo (Events::Ccy const& ccy) const;

    // "GetTotalCashInfo" is somewhat different. It returns a pair
    // (Ccy, CashInfo) if the TotalCashInfo is available, or propagates an
    // exception otherwise:
    std::pair<Events::Ccy const, CashInfo> const& GetTotalCashInfo() const;

    double  GetRealizedPnL(Events::Ccy const& ccy, bool db_record = true);
    double  GetNLV        (Events::Ccy const& ccy, bool db_record = true);
    double  GetInitMargin (Events::Ccy const& ccy, bool db_record = true);
    double  GetVaR        (Events::Ccy const& ccy, bool db_record = true);

    EnvType Environment() const { return m_envType; }

    // "GetVarMargin": For each particular Instrument, and for the whole
    // Ccy (with optional DB recording):
    double  GetVarMargin (QSym const& qsym) const;
    double  GetVarMargin (Events::Ccy const& ccy, bool db_record = true);

    // "GetCashInfo": a "holistic" view of the above (but no DB recording --
    // this is an in-memory, read-only op):
    //
    CashMap const& GetCashInfos(Events::Ccy const& ccy) const
      { return *m_cashInfo; }

    // "GetGreeks":
    // Computes the Differential Risks, optionally stores them in the DB, and
    // returns a ref to the data structure holding them:
    //
    GreeksMap const& GetGreeks (bool db_record = true);

    // In general  (for multi-Ccy Strategies), "Total {RealizedPnL,NLV,IM,VM,
    // VaR}" cannot be computed by "StratEnv" itself, because we need to spe-
    // cify the FX rate(s) to be used; so this should be done by the Strategy
    // itself, and recorded by "StratEnv" (and in the DB if "db_record"  flag
    // is set):
    //
    void SetTotalRealizedPnL
         (Events::Ccy const& ccy, double rpnl,   bool db_record = true);

    void SetTotalNLV
         (Events::Ccy const& ccy, double nlv,    bool db_record = true);

    void SetTotalInitMargin
         (Events::Ccy const& ccy, double margin, bool db_record = true);

    void SetTotalVarMargin
         (Events::Ccy const& ccy, double margin, bool db_record = true);

    void SetTotalVaR
         (Events::Ccy const& ccy, double VaR,    bool db_record = true);

    // "GetTotal{RealizedPnL,NLV,IM,VM,VaR}" returns "false" if the Totals have
    // not been set yet; then the value of "ccy" (in which the Total is denomi-
    // nated) is undefined (""), and the "double" vals are 0.0:
    //
    bool GetTotalNLV        (Events::Ccy* ccy, double* nlv)    const;
    bool GetTotalInitMargin (Events::Ccy* ccy, double* margin) const;
    bool GetTotalVarMargin  (Events::Ccy* ccy, double* margin) const;
    bool GetTotalVaR        (Events::Ccy* ccy, double* VaR)    const;
    bool GetTotalRealizedPnL(Events::Ccy* ccy, double* rpnl)   const;

    //=======================================================================//
    // Order Management:                                                     //
    //=======================================================================//
  private:
    //-----------------------------------------------------------------------//
    // "SendReqPtr":                                                         //
    //-----------------------------------------------------------------------//
    // Sending up a ptr to a previously-constructed ShM Request, to the OE.
    // NB: "qsym" may be NULL if unknown (eg when cancelling or amending an or-
    // der based on RootID alone).
    // Returns "true" iff the send was successful:
    //
    bool SendReqPtr
    (
      AOSReq12*   req,
      int         omcN,
      QSym const* qsym,
      char const* where
    );

  public:
    //-----------------------------------------------------------------------//
    // "SendHeartBeat": For testing purposes only:                           //
    //-----------------------------------------------------------------------//
    void SendHeartBeat(int omcN = 0) const;

    //-----------------------------------------------------------------------//
    // "NewOrder":                                                           //
    //-----------------------------------------------------------------------//
    // NB: If "on_behalf" is not NULL, the order is sent  on behalf of another
    // Strategy (identified by that string); all responses will be received by
    // that Strategy, which must be subscribed  to the corresp instrument  for
    // correct operation.
    // If "user_data" is not NULL, the data are copied and sent along with the
    // order:
    // NB: pegged orders are supported; obviously, incorrect peg diff could re-
    // sult in an immediate fill.
    // Returns a ShM ptr to the new AOS created (DO NOT ATTEMPT to de-allocate
    // it), or NULL in case of error:
    //
    AOSReq12* NewOrder
    (
      QSym const&             qsym,
      OrderTypeT              ord_type,
      SideT                   side,
      double                  price,     // 0 for MktOrder, > 0 for LimitOrder
      long                    qty,
      Events::AccCrypt        acc_crypt,
      AOS::UserData const*    user_data         = NULL,
      FIX::TimeInForceT       time_in_force     = FIX::TimeInForceT::Day,
      utxx::time_val          expire_date       = utxx::time_val(),
      bool                    peg_to_this_side  = false,
      long                    show_qty          = LONG_MAX, // Show full qty
      long                    min_qty           = 0         // No minimum
    );

    //-----------------------------------------------------------------------//
    // "StreamingQuote":                                                     //
    //-----------------------------------------------------------------------//
    // Only available if the corresp OE supports Streaming Quotes.  Combines
    // the functionality of "NewOrderSingle", "CancelOrder" and "ModifyOrder",
    // and operates at both Bid and Ask sides simultaneously (see the "Strea-
    // mingQuote" FIX msg):
    // If no pair of StreamingQuotes with the given ID exists yet, this method
    // will post such a pair and return a pair of AOS Ptrs corresponding to the
    // Bid and Ask sides, resp.
    // Otherwise, the method will update or cancel 1 or 2 StreamingQuote sides
    // using the provided AOS Ptrs. Cancellation occurs if the corresp Px and
    // Qty are invalid:
    //
    void StreamingQuote
    (
      QSym const&           qsym,
      Events::AccCrypt      acc_crypt,
      AOS**                 bid_aos,    // *bid_aos == NULL if new quote
      double                bid_px,
      long                  bid_qty,
      AOS::UserData const*  bid_user_data,
      AOS**                 ask_aos,    // *ask_aos == NULL if new quote
      double                ask_px,
      long                  ask_qty,
      AOS::UserData const*  ask_user_data
    );

    //-----------------------------------------------------------------------//
    // "CancelOrder":                                                        //
    //-----------------------------------------------------------------------//
    // NB: "QSym" is not used by the OE (the OrderID is sufficient) but only
    // used to decide with OE is to use;  so if there is only 1 OE, can use
    // it wil qsym==NULL.
    // Returns "true" on success, "false" if any errors were encountered:
    //
    bool CancelOrder
    (
      AOS*                   aos,
      QSym const*            qsym,       // May be NULL (see above)
      bool                   is_manual = false
    );

    //-----------------------------------------------------------------------//
    // "ModifyOrder":                                                        //
    //-----------------------------------------------------------------------//
    // NB:
    // (*) Same comment on "QSym" as above:
    // (*) "new_show_qty" should be set to (-1) for no change, and to any value
    //     which is >= "new_qty" (eg MAX_LONG) to show full "new_qty", which is
    //     the default; using (-1) for no change may be erroneous if  "new_qty"
    //     is less than the original "qty";
    // (*) "new_min_qty"  should be set to (-1) for no change,  and to 0 for no
    //     minimum, which is the default; using (-1) for no change may be erro-
    //     neous if "new_qty" is less than the original "qty";
    // (*) returns "true" on success, "false" if any error was detected:
    //
    bool ModifyOrder
    (
      AOS*                    aos,
      QSym const*             qsym,                  // NULL OK (see above)
      double                  new_price,             // NaN  if not to change
      long                    new_qty,               // -1   if not to change
      AOS::UserData const*    user_data    = NULL,
      long                    new_show_qty = LONG_MAX,
      long                    new_min_qty  = 0,
      bool                    is_manual    = false
    );

    //-----------------------------------------------------------------------//
    // "CancelAllOrders":                                                    //
    //-----------------------------------------------------------------------//
    // NB: This method is intended to mass-cancel active orders created by THIS
    // STRATEGY ONLY, possibly with some further filtering (by Symbol and/or
    // Side).
    // Global over-all order cancellation is performed by the Connector itself
    // in case of communication errors:
    //
    AOSReq12* CancelAllOrders
    (
      QSym const&             qsym,
      Events::AccCrypt        acc_crypt,
      bool                    this_side_only = false,
      SideT                   side = SideT::Undefined // If "this_side_only" set
    );

    //-----------------------------------------------------------------------//
    // "CountActiveOrders":                                                  //
    //-----------------------------------------------------------------------//
    // NB: Orders which are still present in "AOSMapQPV" but are marked as In-
    // Active by the corresp Connector, are not counted.
    //
    // (1) for a particular "qsym":
    int CountActiveOrders(QSym const& qsym) const;

    // (2) a total count for all QSyms:
    //
    int CountActiveOrders() const;

    //=======================================================================//
    // Market Orders and Portfolio Liquidation:                              //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "GetWorst{Bid,Ask}":                                                  //
    //-----------------------------------------------------------------------//
    // Compute the MarketPrice on the resp Bid and Ask side for a given Qty:
    //
    double GetWorstBid(QSym const& qsym, long qty) const;
    double GetWorstAsk(QSym const& qsym, long qty) const;

    double GetArrivalPrice(QSym const& qsym) const;

    //-----------------------------------------------------------------------//
    // "LiquidatePortfolio":                                                 //
    //-----------------------------------------------------------------------//
    // Close all positions accumulated so far by the Strategy, @ market (aggres-
    // sive) prices, or arrival (mid-bid-ask) prices:
    //
    void LiquidatePortfolio(bool use_arrival_pxs = true);

    //=======================================================================//
    // Misc:                                                                 //
    //=======================================================================//
    // Status Line Output (must be 0-terminated):
    //
    void OutputStatusLine(char const* line);

    // Modification of Variables in Working Strategies:
    //
    // Modifyable vars must be of type "double", and must be used with EXTREME
    // CARE -- the life span of such vars must be that of the whole Strategy.
    // The following method allows the user to declare a dynamically-modifyable
    // variable:
    //
    void DeclareUpdatable
    (
      std::string const&  name,
      double*             ptr
    );

    // "GetConnectorStatus" (seldom required):
    XConnector::Status GetConnectorStatus(ConnDescr const& a_conn) const;

  private:
    //=======================================================================//
    // Internal Utils:                                                       //
    //=======================================================================//
    eterm Subscribe
      (ConnDescr const& a_conn, std::string const& a_sym, const char* where);

    // Check QSym / AOS validity and consistency:
    //
    bool CheckQSymAOS
    (
      QSym const& qsym,
      AOS  const* aos,
      char const* where
    )
    const;

    // Subscribed "QSym" look-ups:
    //
    QSym const& GetQSymByMD
    (
      Events::EventT        event_type,
      Events::SymKey const& sym_key,
      int                   mdcN
    )
    const;

    QSym const& GetQSymByOE
    (
      Events::EventT        event_type,
      Events::SymKey const& sym_key,
      int                   omcN
    )
    const;

    // ZeroMQ Dialogue:
    eterm SimpleReqResp
    (
      ConnDescr const& a_conn,
      eterm     const& a_request,
      char      const* a_where
    )
    const;

    // Updating internal data structures on Event arrival (before it is passed
    // to the application-level Stratergy):
    void ProcessEvent(ConnEventMsg* targ, utxx::time_val a_eventTime);

    // The Body of the internal C3 Thread:
    void C3Loop();

    // Updating various internal infos:
    void UpdateCash
    (
      Events::Ccy  const& ccy,
      double              cash_delta,
      Events::AccCrypt    acc_crypt
    );

    // Getting and setting "total" values (computed "manually" by the Startegy,
    // or automatically if there is only 1 Ccy):
    bool GetTotalVal
    (
      CashInfoCompT       type,
      Events::Ccy*        ccy,
      double*             val
    )
    const;

    void SetTotalVal
    (
      CashInfoCompT       type,
      Events::Ccy  const& ccy,
      double              val,
      bool                db_record
    );

    void VerifyAccCrypt(Events::AccCrypt acc_crypt) const;

    // Applying (synchronously, while in "GetNextEvent") dynamic updates to
    // Strategy Variables:
    void ApplyUpdates() const;

    // Running the Strategy in a Safe Mode:
    void SafeModeRun();

    // Common implementation of "ModifyOrder" and "CancelOrder". Return "true"
    // on success, "false" on failure:
    bool ModifyOrderImpl
    (
      AOS*                    aos,
      QSym const*             qsym,
      double                  new_price,
      long                    new_qty,
      AOS::UserData const*    user_data,
      long                    new_show_qty,
      long                    new_min_qty,
      bool                    is_manual,
      char const*             where
    );

    // Common implementation of "NewOrder" and "CancelAllOrders":
    AOSReq12* NewOrderImpl
    (
      QSym const&             qsym,
      OrderTypeT              ord_type,
      SideT                   side,
      double                  price,          // NaN if Mkt Order, peg_diff
      long                    qty,            //             if "is_pegged"
      Events::AccCrypt        acc_crypt,
      AOS::UserData const*    user_data,
      FIX::TimeInForceT       time_in_force,
      utxx::time_val          expire_date,
      bool                    this_side_only, // For Pegged or MassCancel orders
      long                    show_qty,
      long                    min_qty,
      utxx::time_val          ts,
      char const*             where
    );

    // Processing Fill and PartialFill Events:
    void OnTrade
    (
      AOSReqFill const*       rf,
      QSym const&             qsym,
      PositionInfo*           pi,
      std::string const&      mdc_name,
      std::string const&      omc_name,
      utxx::time_val          a_eventTime
    );
#   endif // !ODB_COMPILER
  };
}
#endif  // MAQUETTE_STRATEGYADAPTOR_STRATENV_H
