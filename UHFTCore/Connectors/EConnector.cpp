// vim:ts=2:et
//===========================================================================//
//                          "Connectors/EConnector.cpp":                     //
//          Abstract common base class for all Embedded Connectors           //
//===========================================================================//
#include "Basis/IOUtils.h"
#include "Basis/ConfigUtils.hpp"
#include "Basis/TimeValUtils.hpp"
#include "Basis/XXHash.hpp"
#include "Basis/OrdMgmtTypes.hpp"
#include "Connectors/EConnector.h"
#include "InfraStruct/StaticLimits.h"
#include "InfraStruct/PersistMgr.h"
#include "InfraStruct/RiskMgr.h"
#include "InfraStruct/SecDefsMgr.h"
#include <utxx/error.hpp>
#include <boost/algorithm/string.hpp>
#include <spdlog/spdlog.h>
#include <cassert>

using namespace std;

namespace
{
  using namespace MAQUETTE;

  //=========================================================================//
  // Utils:                                                                  //
  //=========================================================================//
  //=========================================================================//
  // "GetShMMapSize":                                                        //
  //=========================================================================//
  // Computes the minimal required size of the ShM area for this Connector.
  // XXX: It has some semantic knowledge about OMCs in particular -- which
  // in theory MUST NOT be used here:
  //
  unsigned long GetShMMapSize
  (
    string const&                      CHECK_ONLY(a_name),
    boost::property_tree::ptree const& a_params,
    unsigned long                      a_extra_shm_size,
    unsigned long                      CHECK_ONLY(a_max_segm_size)
  )
  {
    // NB: "MaxAOSes" and "MaxReq12s" could even be present in the config of a
    // purely MktData Connector (eg if both MDC and OMC share some configs);
    // "MaxTrades" is for our own Trades in an OMC:
    //
    unsigned long const maxAOSes  = a_params.get<unsigned long>("MaxAOSes",  0);
    unsigned long const maxReq12s = a_params.get<unsigned long>("MaxReq12s", 0);
    unsigned long const maxTrades = a_params.get<unsigned long>("MaxTrades", 0);

    // Calculate the minimum size required to hold the above data. The minimum
    // size is 64k to allow for small allocations and memory manager overhead:
    unsigned long res =
      maxAOSes        * sizeof(AOS)   +
      maxReq12s       * sizeof(Req12) +
      maxTrades       * sizeof(Trade) +
      a_extra_shm_size                +
      65536;

    // Since we reserve "MaxShmSegmSz" of address space  to each mapped segm
    // (see "GetMapAddr"), the size must be limited by that value:
    CHECK_ONLY
    (
      if (utxx::unlikely(res > a_max_segm_size))
        throw utxx::runtime_error
              ("GetShMMapSize: ", a_name, ": Calculated Size Too Large: ", res);
    )
    // If OK:
    return res;
  }

  //=========================================================================//
  // "GetSecDefDData":                                                       //
  //=========================================================================//
  // Tries to extract (SettlDate, PassFeeRate, AggrFeeRate)
  // for some         (Symbol, Segm, Tenor, Tenor2)
  // from a given     (Key, Val) pair.
  // Returns "true" iff successful; in that case, fills in the output params.
  // Otherwise, their vals are undefined:
  //
  // The potentially-applicable entries are like:
  //
  // Key = Symbol-Exchange[-StaticSegment[-Tenor[-Tenor2]]]
  // Val = {YYYYMMDD|0}[,PassFeeRate[,AggrFeeRate]]
  //
  inline bool GetSecDefDData
  (
    string const& a_key,
    string const& a_val,
    SymKey*       a_symbol,
    SymKey*       a_exchange,
    SymKey*       a_segm,
    SymKey*       a_tenor,
    SymKey*       a_tenor2,
    int*          a_settl_date,
    double*       a_pass_fee_rate,
    double*       a_aggr_fee_rate
  )
  {
    assert(a_symbol        != nullptr && a_exchange      != nullptr &&
           a_segm          != nullptr && a_tenor         != nullptr &&
           a_tenor2        != nullptr && a_settl_date    != nullptr &&
           a_pass_fee_rate != nullptr && a_aggr_fee_rate != nullptr);

    if (utxx::unlikely(a_key.empty() || a_val.empty()))
      return false;

    // XXX: See also "SecDefsMgr::FindSecDefOpt(InstrName)" which implements
    // similar Key (FullInstrName) splitting logic:
    //
    vector<string> keyParts;
    boost::split  (keyParts, a_key, boost::is_any_of("-|"));
    // XXX: Do not use '_' or ':', as these chars may be part of the Symbol!

    int    n = int(keyParts.size());
    assert(n >= 1);
    if (utxx::unlikely(n < 2 || n > 5))
      return false;   // This is not a valid Key format: Just continue...

    // Now parse "a_val":
    vector<string> valParts;
    boost::split  (valParts, a_val, boost::is_any_of(",;"));

    int    m = int(valParts.size());
    assert(m >= 1);
    if (utxx::unlikely(m > 3))
      return false;   // This is not a valid Val format: Just continue...

    // Extract (Symbol, Exchange, StaticSegment, Tenor, Tenor2) from the Key:
    // NB:
    // (*) Exchange, StaticSegment, Tenor and Tenor2 may all be necessary for
    //     disambiguating the Symbol;
    // (*) StaticSegment, Tenor and Tenor2 may be empty (explcitly), or may be
    //     omitted, in which case  ANY  corresp SecDefS component would  match
    //     them. So:
    // Symbol:
    *a_symbol   = MkSymKey(keyParts[0]);
    if (utxx::unlikely(a_symbol->empty()))
      return false;   // This is VERY odd, but still no exception here...

    // Exchange/ECN/Aggregator/etc: For extra safety, it must NOT be omitted:
    *a_exchange = MkSymKey(keyParts[1]);
    if (utxx::unlikely(a_exchange->empty()))
      return false;

    // Segment / Sector:
    bool   anySegm   = (n <  3);
    *a_segm   = anySegm   ? EmptySymKey : MkSymKey(keyParts[2]);
    // Tenor:
    bool   anyTenor  = (n <  4);
    *a_tenor  = anyTenor  ? EmptySymKey : MkSymKey(keyParts[3]);
    // Tenor2 (for Swaps only):
    bool   anyTenor2 = (n != 5);
    *a_tenor2 = anyTenor2 ? EmptySymKey : MkSymKey(keyParts[4]);

    // Extract (SettlDate, PassFeeRate, AggrFeeRate) from the Val:
    *a_settl_date    = 0;
    *a_pass_fee_rate = NaN<double>;
    *a_aggr_fee_rate = NaN<double>;

    // SettlDate (FIXME: There is no SettlDate2 yet):
    if (!valParts[0].empty())
    {
      *a_settl_date = stoi(valParts[0]);
      if (utxx::unlikely
         (*a_settl_date != 0 && *a_settl_date < GetCurrDateInt()))
        return false; // This is not a valid SettlDate format: Just continue...
    }
    // PassFeeRate:
    if (m >= 2 && !valParts[1].empty())
      *a_pass_fee_rate = stod(valParts[1]);

    // AggrFeeRate:
    if (m == 3 && !valParts[2].empty())
      *a_aggr_fee_rate = stod(valParts[2]);

    // If only one FeeRate is provided, the other one is the same, so both are
    // either defined or undefined:
    if (IsFinite(*a_pass_fee_rate) && !IsFinite(*a_aggr_fee_rate))
      *a_aggr_fee_rate = *a_pass_fee_rate;

    if (IsFinite(*a_aggr_fee_rate) && !IsFinite(*a_pass_fee_rate))
      *a_pass_fee_rate = *a_aggr_fee_rate;

    assert(IsFinite(*a_pass_fee_rate) == IsFinite(*a_aggr_fee_rate));

    // We got something (the results are to be checked further by the Caller):
    return true;
  }

  //=========================================================================//
  // "MkSettlDateKey":                                                       //
  //=========================================================================//
  // Constructs the Key in the form of
  //   Symbol[:StaticSegment[:Tenor[:Tenor2]]]
  // which can be used for searching for explicitly-specified SettlDates in
  // Params. NB: This Key is NOT the same as SecDefD.m_FullName!
  //
  inline string MkSettlDateKey(SecDefS const& a_sec_def)
  {
    char key[256];

    // XXX: All components are strictly conditional on the existing of prev
    // ones -- empty parts (like "::") are not allowed:
    char* curr = stpcpy(key, a_sec_def.m_Symbol.data());

    if (!IsEmpty(a_sec_def.m_SessOrSegmID))
    {
      *curr = ':';
      curr  = stpcpy(curr + 1, a_sec_def.m_SessOrSegmID.data());

      if (!IsEmpty(a_sec_def.m_Tenor))
      {
        *curr = ':';
        curr  = stpcpy(curr + 1, a_sec_def.m_Tenor.data());

        if (IsEmpty(a_sec_def.m_Tenor2))
        {
          *curr = ':';
          curr  = stpcpy(curr + 1, a_sec_def.m_Tenor2.data());
        }
      }
    }
    *curr = '\0';

    // The buffer should always be of suffucient size:
    assert(int(curr - key) < int(sizeof(key)));
    return string(key);
  }

  //=========================================================================//
  // "SelectThisSymbol":                                                     //
  //=========================================================================//
  // Returns True iff the opt-in list is NULL/empty, or the Symbol of the given
  // SecDefS is on that list:
  //
  inline bool SelectThisSymbol
  (
    SecDefS const&       a_def,
    vector<string>const* a_only_symbols
  )
  {
    if (a_only_symbols == nullptr || a_only_symbols->empty())
      return true;  // No opt-in list provided, so always select this SecDefS

    // Otherwise: Check the Symbol againt the list. XXX: We allow both Symbols
    // and FullInstrNames to be on that list, so apply a prefix check:
    char const* symbol = a_def.m_Symbol.data();
    size_t      len    = strlen(symbol);
    assert(len != 0);

    return
      (find_if(a_only_symbols->cbegin(), a_only_symbols->cend(),
               [symbol, len]    (string const& a_symbol) -> bool
               { return (strncmp(symbol, a_symbol.data(), len) == 0); })
       != a_only_symbols->cend());
  }
}

namespace MAQUETTE
{
  //=========================================================================//
  // Non-Default Ctor:                                                       //
  //=========================================================================//
  // NB:
  // (*) Don't use the Reactor's Logger, otherwise msgs from all Connectors
  //     would be mixed together, which makes it more difficult  to analyse
  //     them; create a new specific Per-This-Connector Logger:
  // (*) RiskMgr is a single shared instance required by all Connectors (or
  //     NULL);
  // (*) SecDefsMgr must be non-NULL for a non-Hist Connectors, and NULL ot-
  //     herwise (it will then be created internally):
  // (*) If "a_name" is empty, a_params["AccountKey"] is used:
  //
  EConnector::EConnector
  (
    string const&                      a_name,         // See above
    string const&                      a_exchange,
    unsigned long                      a_extra_shm_size,
    EPollReactor*                      a_reactor,
    bool                               a_use_busy_wait,
    SecDefsMgr*                        a_sec_defs_mgr,
    vector<SecDefS>             const& a_all_sec_defs,
    vector<string>              const* a_only_symbols,
    bool                               a_expl_sds_only,
    bool                               a_use_tenors,   // UseTenorsInSecIDs
    RiskMgr*                           a_risk_mgr,
    boost::property_tree::ptree const& a_params,
    QtyTypeT                           a_qt,
    bool                               a_with_frac_qtys
  )
  : m_name         (a_name.empty()
                    ? a_params.get<string>("AccountKey")
                    : a_name),
    m_exchange     (a_exchange),
    m_cenv         (GetMQTEnv(m_name)),
    m_reactor      (!IsHistEnv() ? a_reactor      : nullptr),
    // NB: HistEnv is NOT a ProdEnv:
    m_secDefsMgr   (!IsHistEnv() ? a_sec_defs_mgr : new SecDefsMgr(false)),
    m_riskMgr      (!IsHistEnv() ? a_risk_mgr     : nullptr),

    // TODO: Rotation params in the Logger are currently not configurable:
    m_debugLevel   (a_params.get<int>("DebugLevel")),
    // m_loggerShP    (IO::MkLogger(m_name, a_params.get<string>("LogFile"),
    //                              a_params.get<string>("LogEmail", ""))),
    m_loggerShP    (IO::MkLogger(m_name, a_params.get<string>("LogFile"))),
    m_logger       (m_loggerShP.get()),

    // Initialize the PersistMgr (WithDate=true). Always reserve at least 64k
    // ShM for the Persistent Stats set out below. If the corresp ShM segment
    // already exists, "m_pm" will attach to it:
    m_pm           (a_name, &a_params,
                    GetShMMapSize(a_name, a_params, a_extra_shm_size,
                                  decltype(m_pm)::MaxSegmSz),
                    m_debugLevel, m_logger),
    m_qt           (a_qt),
    m_withFracQtys (a_with_frac_qtys),
    m_useBusyWait  (!IsHistEnv() ? a_use_busy_wait : false),
    m_usedSecDefs  (),
    m_allStrats    (),

    // Ptrs to Liveness Stats: Initialised later:
    m_ptrTotBytesTx(nullptr),
    m_ptrTotBytesRx(nullptr),
    m_ptrLastTxTS  (nullptr),
    m_ptrLastRxTS  (nullptr)
  {
    //-----------------------------------------------------------------------//
    // Some Checks:                                                          //
    //-----------------------------------------------------------------------//
    // "ConnectorName" and "Exchange" must be non-empty:
    if (utxx::unlikely(m_name.empty() || m_exchange.empty()))
      throw utxx::badarg_error("EConnector::Ctor: Empty Name/Exchange");

    // "ConnectorName" must actually include "Exvchange":
    if (utxx::unlikely
       (strstr(m_name.data(), m_exchange.data()) == nullptr))
      throw utxx::badarg_error
        ("EConnector::Ctor: Name=", m_name.data(),    " is incompatible with "
         "Exchange=",               m_exchange.data());

    // Also, "a_sec_defs_mgr" must be NULL for a Hist Connector, in which case
    // the SecDefsMgr has been created internally on the Heap:
    if (utxx::unlikely(IsHistEnv() != (a_sec_defs_mgr == nullptr)))
      throw utxx::badarg_error
        ("EConnector::Ctor: ", m_name, ": Missing or Improper SecDefsMgr");

    if (!IsHistEnv())
    {
      //---------------------------------------------------------------------//
      // Extra checks in non-Hist modes (Prod or Test):                      //
      //---------------------------------------------------------------------//
      // The following ptrs must all be non-NULL for a non-Hist Connector:
      if (utxx::unlikely
        (m_reactor == nullptr || m_logger  == nullptr))
        throw utxx::badarg_error
              ("EConnector::Ctor: ", m_name, ": Reactor, SecDefsMgr and "
               "Logger must all be non-NULL");

      // Check the "IsProdEnv" Consistency:
      if (utxx::unlikely
        ((m_riskMgr != nullptr   && m_riskMgr->IsProdEnv() != IsProdEnv()) ||
          m_secDefsMgr->IsProdEnv() != IsProdEnv()))
        throw utxx::badarg_error
              ("EConnector::Ctor: ", m_name, ": Inconsistent Prod/Test Envs");

      // Using RiskMgr is Highly Recommended:
      if (utxx::unlikely(m_riskMgr == nullptr))
        LOG_WARN(1,
          "EConnector::Ctor: {}: DANGER: Running w/o RiskMgr!", m_name)

      //---------------------------------------------------------------------//
      // Connector Persistence Mgmt:                                         //
      //---------------------------------------------------------------------//
      // Get the Segment:
      assert(!m_pm.IsEmpty());
      auto   segm  = m_pm.GetSegm();
      assert(segm != nullptr);

      // Install ptrs to Persistent Stats:
      m_ptrTotBytesTx =
        segm->find_or_construct<unsigned long> (TotBytesTxON())(0);
      m_ptrTotBytesRx =
        segm->find_or_construct<unsigned long> (TotBytesRxON())(0);
      m_ptrLastTxTS   =
        segm->find_or_construct<utxx::time_val>(LastTxTS_ON()) ();
      m_ptrLastRxTS   =
        segm->find_or_construct<utxx::time_val>(LastRxTS_ON()) ();
    }
    else
    {
      //---------------------------------------------------------------------//
      // Hist Connector:                                                     //
      //---------------------------------------------------------------------//
      // We also need to allocate the Stats, but they will be non-Persistent in
      // that case -- PersistMgr is not used:
      m_ptrTotBytesTx = new unsigned long (0);
      m_ptrTotBytesRx = new unsigned long (0);
      m_ptrLastTxTS   = new utxx::time_val();
      m_ptrLastRxTS   = new utxx::time_val();
    }
    // So the Stats Ptrs are always non-NULL:
    assert(m_ptrTotBytesTx != nullptr && m_ptrTotBytesRx != nullptr &&
           m_ptrLastTxTS   != nullptr && m_ptrLastRxTS   != nullptr);

    //-----------------------------------------------------------------------//
    // Install the "SecDefD"s from the given "SevDefS"s:                     //
    //-----------------------------------------------------------------------//
    // All "SecDefS"s must really belong to this Exchange:
    for (SecDefS const& defS: a_all_sec_defs)
      if (utxx::unlikely
         (strcmp(defS.m_Exchange.data(), m_exchange.data()) != 0))
        throw utxx::badarg_error
              ("EConnector::Ctor: Symbol=", defS.m_Symbol.data(),
               ": InAppropriate Exchange=", defS.m_Exchange.data());

    if (a_expl_sds_only)
      // Only explicit SettlDates are allowed, other SecDefs are not installed:
      InstallExplicitSecDefs
        (a_all_sec_defs, a_only_symbols, a_use_tenors, a_params);
    else
    {
      // Implicit SettlDates are allowed,  but in this case we probably  should
      // NOT use Tenors in SecIDs, as Tenors are irrelevant anyway for implicit
      // SettlDates:
      if (utxx::unlikely(a_use_tenors))
        throw utxx::badarg_error
              ("EConnector::Ctor: UseTenors and ImplicitSettlDates modes "
               "are currently incompatible");
      // If OK:
      InstallAllSecDefs(a_all_sec_defs, a_only_symbols, a_params);
    }
    // All Done!
  }

  //=========================================================================//
  // Dtor:                                                                   //
  //=========================================================================//
  EConnector::~EConnector() noexcept
  {
    // Prevent any possible exceptions from propagating out of the Dtor:
    try
    {
      // Flush the Logger (if exists):
      if (m_logger != nullptr)
        m_logger->flush();
      const_cast<spdlog::logger*(&)>(m_logger) = nullptr;

      // The only non-trivial action is to de-allocate  Stats and SecDefsMgr if
      // they are non-persistent (ie in Hist mode). Otherwise, the ShM segments
      // will be unmapped (but preserved!) automatically:
      if (IsHistEnv())
      {
        Delete0(m_secDefsMgr);
        Delete0(m_ptrTotBytesTx);
        Delete0(m_ptrTotBytesRx);
        Delete0(m_ptrLastTxTS);
        Delete0(m_ptrLastRxTS);
      }
    }
    catch(...){}
  }

  //=========================================================================//
  // "Subscribe":                                                            //
  //=========================================================================//
  // This is a common subscription method for all Connectors. It allows the
  // Clients to receive TradingStatus (ConnectorStatus) events. May be over-
  // riden by derived classes:
  //
  void EConnector::Subscribe(Strategy* a_strat)
  {
    assert(a_strat != nullptr);

    // Search for "a_strat" in the over-all "m_allStrats" vector, and append
    // it if it was not found; no error or warning  on duplicate "Subscribe"
    // attempt:
    if (utxx::likely
       (find(m_allStrats.cbegin(), m_allStrats.cend(), a_strat) ==
        m_allStrats.cend()))
      m_allStrats.push_back(a_strat);
  }

  //=========================================================================//
  // "UnSubscribe":                                                          //
  //=========================================================================//
  // Common unsubscription method for all Connectors. A Strategy is removed
  // from the TradingStatus notification list. May be overridden by derived
  // classes:
  //
  void EConnector::UnSubscribe(Strategy const* a_strat)
  {
    // Traverse   "m_allStrats", remove "a_strat" occurrences:
    for (auto it = m_allStrats.begin(); it != m_allStrats.end(); ++it)
      if (*it == a_strat)
      {
        m_allStrats.erase(it);
        return;
      }
    // NB: No error or warning if "a_strat" was not found
  }

  //=========================================================================//
  // "UnSubscribeAll":                                                       //
  //=========================================================================//
  // Similar to "Unsubscribe" above, but applies to ALL Strategies:
  //
  void EConnector::UnSubscribeAll() { m_allStrats.clear(); }

  //=========================================================================//
  // "ProcessStartStop":                                                     //
  //=========================================================================//
  void EConnector::ProcessStartStop
  (
    bool            a_is_on,
    char const*     a_sess_id,
    SecID           a_sec_id,
    utxx::time_val  a_ts_exch,
    utxx::time_val  a_ts_recv
  )
  const
  {
    //-----------------------------------------------------------------------//
    // Some Checks:                                                          //
    //-----------------------------------------------------------------------//
    if (utxx::likely(a_sec_id == 0))
    {
      // For all SecIDs:
      // NB: the "a_is_on" flag must refers to the trading status at the Exchan-
      // ge (in this case, that status applies to all Instrs), so it may not be
      // the same as the "IsActive()" status of the Connector. Still,  in  most
      // cases, this method is invoked after  the Connector has just become Ac-
      // tive or Inactive, to notify the Strategies of that. So if the Exchange
      // and Connector statuses do not match, produce a warning:
      CHECK_ONLY
      (
        bool isActive = IsActive();

        if (utxx::unlikely(a_is_on != isActive))
          LOG_WARN(2,
            "EConnector::ProcessStartStop: Inconsistency: On={} but IsActive="
            "{}", a_is_on, isActive)
      )
    }
    // One Session or All Sessions?
    bool allSessions = (a_sess_id == nullptr || *a_sess_id == '\0');

    SecDefD const* instr = nullptr;  // By default, event applies to ALL instrs

    //-----------------------------------------------------------------------//
    // Update the TradingStatus of the given Instrument (or All Instrs):     //
    //-----------------------------------------------------------------------//
    // XXX: It's OK to iterate over "m_usedSecDefs", since in most cases this
    // cal applies to all SecIDs anyway, and even if not, the number  of used
    // Instrs is typically small:
    //
    for (SecDefD const* used: m_usedSecDefs)
    {
      assert(used != nullptr);

      // NB: If it is a specific "a_sec_id", and a specific "a_sess_id" is also
      // given, they must be consistent:
      bool sessMatch =
        allSessions || (strcmp(used->m_SessOrSegmID.data(), a_sess_id) == 0);

      bool thisSecID = (used->m_SecID == a_sec_id);

      CHECK_ONLY
      (
        if (utxx::unlikely(thisSecID && !sessMatch))
          throw utxx::badarg_error
                ("EConnector::ProcessStartStop: ", m_name, ": SecID=", a_sec_id,
                 ", Instr=",  used->m_FullName.data(),
                 " are inconsistent with SessID=", a_sess_id);
      )
      // If OK:
      if (utxx::likely((a_sec_id == 0 || thisSecID) && sessMatch))
      {
        // NB: TradingStatus is mutable!
        used->m_TradingStatus =
          a_is_on
          ? SecTrStatusT::FullTrading
          : SecTrStatusT::NoTrading;
      }

      // If it was a specific SecID, it is unique -- do not continue:
      if (utxx::unlikely(thisSecID))
      {
        instr = used;
        break;
      }
    }
    //-----------------------------------------------------------------------//
    // Now notify all Strategies:                                            //
    //-----------------------------------------------------------------------//
    // NB: If the event is global (SecID=0), then really all Strategies are no-
    // tified.  Otherwise, only the Strategies  which  have subscribed to this
    // Instrument, get notified:
    //
    for (Strategy* strat: m_allStrats)
    {
      assert(strat != nullptr);
      if (a_sec_id == 0 || strat->IsSubscribed(a_sec_id))
      {
        // Yes, do the notification:
        // But be careful here -- the Strategy can propagate exceptions, so
        // catch them:
        SAFE_STRAT_CB( strat, OnTradingEvent,
                      (*this, a_is_on, a_sec_id, a_ts_exch, a_ts_recv) )
      }
    }
    LOG_INFO(1,
      "EConnector::ProcessStartStop: Connector/Trading is now {}, SessID={}, "
      "Instr={}",
      a_is_on ? "ACTIVE" : "INACTIVE",
      utxx::likely(a_sess_id == nullptr) ? "ALL" : a_sess_id,
      utxx::likely(instr     == nullptr) ? "ALL" : instr->m_FullName.data())
  }

  //=========================================================================//
  // "GetMQTEnv":                                                            //
  //=========================================================================//
  MQTEnvT EConnector::GetMQTEnv(string const& a_name)
  {
    char const* name = a_name.data();
    char const* dash = strrchr(name, '-');

    CHECK_ONLY
    (
      if (utxx::unlikely(dash == nullptr))
        throw utxx::badarg_error
              ("EConnector::GetMQTEnv: MalFormatted Connector Name: ", a_name);
    )
    return
      (strncmp("Prod", dash + 1, 4) == 0)
      ? MQTEnvT::Prod :
      (strncmp("Test", dash + 1, 4) == 0)
      ? MQTEnvT::Test :
      (strncmp("Hist", dash + 1, 4) == 0)
      ? MQTEnvT::Hist :
      throw utxx::badarg_error
            ("EConnector::GetMQTEnv: UnRecognised Connector Name: ", a_name);
  }

  //=========================================================================//
  // "InstallExplicitSecDefs":                                               //
  //=========================================================================//
  // Among the statically-configured "SeDefS"s, we will actually use (and inst-
  // all as "SecDefD"s) only those instrs for which "a_params" provide explicit
  // SettlDates, for the following reasons:
  // (*) There is currently no way of providing SettlDates to instrs apart from
  //     explicit specification in the Params File (unless they are set to all
  //     0s for "truly immediate settlement", as for Crypto-Assets);
  // (*) This will eliminate unnecessary (unused) instrs and their corresp Ord-
  //     erBooks, so both "SecDefD"s and "OrderBook"s   can be stored in short
  //     vectors, enabling fast search and optimal data locality:
  //
  inline void EConnector::InstallExplicitSecDefs
  (
    vector<SecDefS>             const&  a_all_sec_defs,
    vector<string>              const*  a_only_symbols,
    bool                                a_use_tenors,    // UseTenorsInSecIDs
    boost::property_tree::ptree const&  a_params
  )
  {
    //-----------------------------------------------------------------------//
    // Traverse "a_params" and process SettlDate entries:                    //
    //-----------------------------------------------------------------------//
    TraverseParamsSubTree
    (
      a_params,

      [this, &a_all_sec_defs, a_only_symbols, a_use_tenors]
      (string const&  a_key, string const& a_val)->bool
      {
        //-------------------------------------------------------------------//
        // Does this (Key,Val) pair contain SettlDate, etc for some Symbol?  //
        //-------------------------------------------------------------------//
        // Value format: {SettlDate|0}[,PassRelTrFee[,AggrRelTrFee]]
        //
        SymKey symbol, exchange, segm, tenor, tenor2;
        int    settlDate  = 0;
        int    settlDate2 = 0;
        double passFee    = NaN<double>;
        double aggrFee    = NaN<double>;
        bool   ok         =
          GetSecDefDData
            (a_key, a_val, &symbol, &exchange, &segm, &tenor, &tenor2,
             &settlDate,  &passFee, &aggrFee);

        if (!ok || strcmp(exchange.data(), m_exchange.data()) != 0)
          // This (Key,Val) pair does NOT contain a SettlDate for a Symbol, or
          // it belongs to another Exchange: Just continue traversing:
          return true;

        //-------------------------------------------------------------------//
        // Find the "SecDefS" matching (Symbol, Segm, Tenor, Tenor2):        //
        //-------------------------------------------------------------------//
        // (It must be unique):
        bool anySegm   = IsEmpty(segm);
        bool anyTenor  = IsEmpty(tenor);
        bool anyTenor2 = IsEmpty(tenor2);
        auto selector  =
          [&symbol, anySegm, &segm, anyTenor, &tenor, anyTenor2, &tenor2]
          (SecDefS  const& a_defs)->bool
          {
            return
              strcmp(symbol.data(), a_defs.m_Symbol.data())       == 0  &&
             (anySegm   ||
              strcmp(segm  .data(), a_defs.m_SessOrSegmID.data()) == 0) &&
             (anyTenor  ||
              strcmp(tenor .data(), a_defs.m_Tenor.data())        == 0) &&
             (anyTenor2 ||
              strcmp(tenor2.data(), a_defs.m_Tenor2.data())       == 0);
          };
        // Search for the 1st occurrence of out 4-ple:
        auto cit1 =
          find_if(a_all_sec_defs.cbegin(), a_all_sec_defs.cend(), selector);

        // Many Params entries could initially look like an Instrument -- so if
        // the required 4-ple was not found, it's completely normal  (TODO: use
        // a special XML sub-tree for Instruments!). Just continue:
        //
        if (utxx::likely(cit1 == a_all_sec_defs.cend()))
          return true;

        // If found, check for Uniqueness. If not unique (found twice), produce
        // an error:
        CHECK_ONLY
        (
          auto cit2 = find_if(next(cit1), a_all_sec_defs.cend(), selector);

          if (utxx::unlikely(cit2 != a_all_sec_defs.cend()))
            throw utxx::badarg_error
                  ("EConnector::Ctor: ", m_name,   ": Symbol=",
                   symbol.data(),     ", Exchange=", exchange.data(),
                   ", Segm=",   (anySegm   ? "ANY" : segm.data()),
                   ", Tenor=",  (anyTenor  ? "ANY" : tenor.data()),
                   ", Tenor2=", (anyTenor2 ? "ANY" : tenor2.data()),
                   ": Not Unique!");
        )
        // If Found and Uniq: Check this "defS" against the "opt-in" Symbols
        // list (if provided); if not selected, continue:
        SecDefS const& defS      = *cit1;
        if (!SelectThisSymbol(defS, a_only_symbols))
          return true;

        // FIXME:
        // (1) Calculate "settlDate2" from   "tenor2" if specified (for Swaps);
        // (2) Verify    "settlDate" against "tenor"...
        assert(defS.m_IsSwap == !IsEmpty(tenor2));

        if (utxx::unlikely(defS.m_IsSwap))
          LOG_ERROR(1, "EConnector::Ctor: No SettlDate2 for {}", symbol.data())

        // If Selected: Construct and install the SecDefD with the given Settl-
        // Date:
        SecDefD const& installed =
          m_secDefsMgr->Add
            (defS, a_use_tenors, settlDate, settlDate2, passFee, aggrFee);

        // Memoise the recently-"installed" (or updated)  "SecDefD" as "used" by
        // this Connector (unless already there). NB: below, "SecDefD"s are com-
        // pared by ptr equality:
        if (utxx::likely
           (find(m_usedSecDefs.begin(), m_usedSecDefs.end(),
                 &installed)        ==  m_usedSecDefs.end()))
          m_usedSecDefs.push_back(&installed);

        // NB: Even for filtered-in Symbols, here we only install  the corresp
        // SecDefDs; OrderBooks are NOT installed here. If the corresp Derived
        // Class is an MDC, it will create OrderBooks  for all configured Sec-
        // DefDs in its resp Ctor.
        // So Done:
        LOG_INFO(3,
          "EConnector::Ctor: SecDef Installed: {}, SecID={}",
          installed.m_FullName.data(), installed.m_SecID)
        return true;
      }
    );
    // All Done!
  }

  //=========================================================================//
  // "InstallAllSecDefs":                                                    //
  //=========================================================================//
  inline void EConnector::InstallAllSecDefs
  (
    vector<SecDefS>             const&  a_all_sec_defs,
    vector<string>              const*  a_only_symbols,
    boost::property_tree::ptree const&  a_params
  )
  {
    // Convert all "SecDefS"s into "SecDefD"s, using explicit SetllDates, or
    // Futures Tenors, or implicit SettlDates (0):
    //
    for (SecDefS const& defS: a_all_sec_defs)
    {
      // NB: The CallER has already checked that the Exchange is OK:
      assert(strcmp(defS.m_Exchange.data(), m_exchange.data()) == 0);

      // Is there an explicitly-given SettlDate in Params?
      // Construct the corresp Key and try to get the SettlDate if available
      // (or 0):
      int settlDate = a_params.get<int>(MkSettlDateKey(defS), 0);

      // And possibly use the ExpireDate  (which may still be 0):
      if (settlDate == 0)
        settlDate = defS.m_ExpireDate;

      // If there is a static expiry date, only proceed with this SecDef if it
      // has not expired yet:
      if ((defS.m_ExpireDate != 0) && (defS.m_ExpireDate < GetCurrDateInt()))
        continue;

      // If there is an opt-in list of Symbols, check "defS" again it:
      if (!SelectThisSymbol(defS, a_only_symbols))
        continue;

      // OK: Create a "SecDefD" and install it in ShM. In this case, we assume
      // that UseTenorsInSecIDs=false (because by default, all SettlDates are 0,
      // so Tenors do not really matter). Trading Fees not known here:
      //
      SecDefD const& installed =
        m_secDefsMgr->Add
          (defS, false, settlDate, 0, NaN<double>, NaN<double>);

      // Memoise the recently-"installed" (or updated)  "SecDefD" as "used" by
      // this Connector (unless already there). NB: below, "SecDefD"s are com-
      // pared by ptr equality (similar to "InstallExplicitSecDefs"):
      if (utxx::likely
         (find(m_usedSecDefs.begin(), m_usedSecDefs.end(),
               &installed)        ==  m_usedSecDefs.end()))
        m_usedSecDefs.push_back(&installed);

      // NB: Again, we only install "SecDefD"s here, NOT OrderBooks -- the lat-
      // ter are done by "EConnector_MktData" Ctor:
      LOG_INFO(3,
        "EConnector::Ctor: SecDef Installed: {}, SecID={}",
        installed.m_FullName.data(), installed.m_SecID)
    }
  }
} // End namespace MAQUETTE
