// vim:ts=2:et
//===========================================================================//
//                           "Connectors/EConnector.h":                      //
//          Abstract common base class for all Embedded Connectors           //
//===========================================================================//
#pragma once

#include "Basis/BaseTypes.hpp"
#include "Basis/SecDefs.h"
#include "Basis/PxsQtys.h"
#include "Basis/TimeValUtils.hpp"
#include "InfraStruct/StaticLimits.h"
#include "InfraStruct/PersistMgr.h"
#include "InfraStruct/Strategy.hpp"
#include <spdlog/spdlog.h>
#include <boost/core/noncopyable.hpp>
#include <boost/container/static_vector.hpp>
#include <boost/property_tree/ptree.hpp>

namespace MAQUETTE
{
  class SecDefsMgr;
  class RiskMgr;
  class EPollReactor;

  //=========================================================================//
  // "EConnector" Class:                                                     //
  //=========================================================================//
  // Primary Purposes:
  // (*) Virtual base class for "Start", "Stop", "ReStart", "Subscribe" etc;
  // (*) SecDefs mgmt:
  //
  class EConnector: public boost::noncopyable
  {
  public:
    //=======================================================================//
    // Names of Persistent Objs:                                             //
    //=======================================================================//
    constexpr static char const* TotBytesRxON() { return "TotBytesRx"; }
    constexpr static char const* TotBytesTxON() { return "TotBytesTx"; }
    constexpr static char const* LastRxTS_ON () { return "LastRxTS";   }
    constexpr static char const* LastTxTS_ON () { return "LastTxTS";   }

  protected:
    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    std::string     const           m_name;       // Connector instance name
    std::string     const           m_exchange;   // Exchange name
    MQTEnvT         const           m_cenv;       // Environment Type
    EPollReactor*   const           m_reactor;    // Ptr NOT owned!
    SecDefsMgr*     const           m_secDefsMgr; // Ptr NOT owned!
    RiskMgr*        const           m_riskMgr;    // Ptr NOT owned!
    // Logging:
    int             const           m_debugLevel; // For extra logging
    std::shared_ptr<spdlog::logger> m_loggerShP;
    spdlog::logger* const           m_logger;     // Direct ptr to the above
    // Mgr for Conn's PersistData:
    PersistMgr<>                    m_pm;

    // Some General Connector Properties. In particular, WithFracQtys and QT
    // are used for dynamic checks of the Qtys used (applicable to both MDCs
    // and OMCs) in addition to static type checks:
    QtyTypeT        const           m_qt;
    bool            const           m_withFracQtys;
    bool            const           m_useBusyWait;

    // SecDefs used by this Connector (ptrs NOT owned -- actial SecDefs are in
    // the SecDefsMgr). The following vector is rarely used -- not time-criti-
    // cal:
    std::vector<SecDefD const*>     m_usedSecDefs;

    // All subscribed Strategies (up to "Limits::MaxStrats"):
    using   StratsVec =
            boost::container::static_vector<Strategy*, Limits::MaxStrats>;
    mutable StratsVec               m_allStrats;

  private:
    // Statistics: Total number of bytes sent (Tx) and received (Rx),  and the
    // corresp Last TimeStamps. In particular, can be used for liveness monito-
    // ring. These flds are located in a ShM segment. For  consistency,  those
    // stats are updated via "Update{T,R}xStats", hence "private" flds:
    //
    unsigned long *                 m_ptrTotBytesTx;
    unsigned long *                 m_ptrTotBytesRx;
    utxx::time_val*                 m_ptrLastTxTS;
    utxx::time_val*                 m_ptrLastRxTS;

  public:
    //=======================================================================//
    // Ctors, Dtor:                                                          //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // Non-Default Ctor:                                                     //
    //-----------------------------------------------------------------------//
    // NB:
    // (*) Don't use the Reactor's Logger, otherwise msgs from all Connectors
    //     would be mixed together, which makes it more difficult  to analyse
    //     them; create a new specific Per-This-Connector Logger:
    // (*) RiskMgr is a single shared instance required by all Connectors:
    // (*) "a_params" ptr can NOT be NULL in all real cases; nevertheless, it
    //     is a ptr, not a ref, because it can nominally be set to NULL if the
    //     "EConnector" invocation is eliminated due to virtual inheritance:
    // (*) the "SecDefD" construction functionality (the last 2 params) is ODD
    //     in "EConnector", and should probably be moved elsewhere -- but  XXX
    //     we do not know where to yet;
    // (*) "OnlySymbols", if non-NULL and non-empty,  selects a subset of All-
    //     SecDefs;
    // (*) "ExplDatesOnly" means that from "AllSecDefs" (possibly restricted to
    //     "OnlySymbols"), only those "SecDefD"s will be installed for which
    //     there are explicit SettlDates in Params (ie implicit 0s and Futures
    //     Tenors for SettlDates are NOT allowed); otherwise, ALL "SecDefD"s are
    //     installed, with explicit or implicit SettlDates";
    // (*) "UseTenorsInSecIDs" means that if SecIDs are not explicitly given
    //     and are thus auto-generated from Symbols and Segms, then Tenor and
    //     Tenor2 are also included; this is only necessary for Venues  where
    //     there are identical Symbols with several different Tenors. This is
    //     currently incompatible with use of implicit SettlDates,    ie with
    //     (!ExplDatesOnly):
    //
    EConnector
    (
      std::string const&                  a_name,
      std::string const&                  a_exchange,
      unsigned long                       a_extra_shm_size,
      EPollReactor*                       a_reactor,
      bool                                a_use_busy_wait,
      SecDefsMgr*                         a_sec_defs_mgr,
      std::vector<SecDefS>        const&  a_all_sec_defs,
      std::vector<std::string>    const*  a_only_symbols,    // NULL OK
      bool                                a_expl_dates_only, // See above
      bool                                a_use_tenors,      // See above
      RiskMgr*                            a_risk_mgr,
      boost::property_tree::ptree const&  a_params,
      QtyTypeT                            a_qt,
      bool                                a_with_frac_qtys
    );

    // Default Ctor is deleted:
    EConnector() = delete;

    //-----------------------------------------------------------------------//
    // Virtual Dtor:                                                         //
    //-----------------------------------------------------------------------//
    virtual ~EConnector() noexcept;

    // The followig virtual method is just to ensure that derived classes do
    // not become concrete prematurely:
    virtual  void EnsureAbstract() const = 0;

    //=======================================================================//
    // Accessors:                                                            //
    //=======================================================================//
    bool IsProdEnv()  const  { return (m_cenv == MQTEnvT::Prod); }
    bool IsHistEnv()  const  { return (m_cenv == MQTEnvT::Hist); }

    BT_VIRTUAL
    char const*       GetName()        const { return m_name.data();    }
    QtyTypeT          GetQtyTypeT()    const { return m_qt;             }
    bool              HasFracQtys()    const { return m_withFracQtys;   }
    RiskMgr    const* GetRiskMgr()     const { return m_riskMgr;        }
    SecDefsMgr const* GetSecDefsMgr()  const { return m_secDefsMgr;     }

    std::vector<SecDefD const*> const&
                      GetUsedSecDefs() const { return m_usedSecDefs;    }

    int               GetDebugLevel()  const { return m_debugLevel;     }
    spdlog::logger*   GetLogger()      const { return m_logger;         }
    bool              UseBusyWait()    const { return m_useBusyWait;    }

    // Liveness Statistics:
    unsigned long     GetTotBytesTx()  const { return *m_ptrTotBytesTx; }
    unsigned long     GetTotBytesRx()  const { return *m_ptrTotBytesRx; }
    utxx::time_val    GetLastTxTS  ()  const { return *m_ptrLastTxTS;   }
    utxx::time_val    GetLastRxTS  ()  const { return *m_ptrLastRxTS;   }

    //-----------------------------------------------------------------------//
    // Updating Tx and Rx Stats:                                             //
    //-----------------------------------------------------------------------//
    // The following methods are inlined for efficiency. TODO: move those stats
    // into ShM for external observability:
    //
    void UpdateTxStats(int a_len, utxx::time_val a_ts) const
    {
      assert(a_len >= 0);
      *m_ptrTotBytesTx += size_t(a_len);

      if (utxx::likely(!a_ts.empty()))
        *m_ptrLastTxTS  = a_ts;
    }

    void UpdateRxStats(int a_len, utxx::time_val a_ts) const
    {
      assert(a_len >= 0);
      *m_ptrTotBytesRx += size_t(a_len);

      if (utxx::likely(!a_ts.empty()))
        *m_ptrLastRxTS  = a_ts;
    }

    //=======================================================================//
    // Purely Virtual Methods:                                               //
    //=======================================================================//
    // Abstract "Start" and "Stop" methods:
    //
    virtual void Start()              = 0;
    virtual void Stop ()              = 0;

    // Is this Connector currently Active? NB: This only refers to the Connect-
    // or status itself; the corresp Exchange may suspend trading even if  the
    // Connector is active:
    virtual bool IsActive() const     = 0;

    //=======================================================================//
    // Strategies Subscription Mgmt:                                         //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "Subscribe":                                                          //
    //-----------------------------------------------------------------------//
    // This is a common subscription method for all Connectors. It allows the
    // Clients to receive TradingStatus (ConnectorStatus) events. May be over-
    // riden by derived classes -- but NB: this method is NOT purely-virtual!
    //
    virtual void Subscribe(Strategy* a_strat);

    //-----------------------------------------------------------------------//
    // "UnSubscribe":                                                        //
    //-----------------------------------------------------------------------//
    // Common unsubscription method for all Connectors. A Strategy is removed
    // from the TradingStatus notification list. May be overridden by derived
    // classes -- but NB: this method is NOT purely-virtual!
    //
    virtual void UnSubscribe(Strategy const* a_strat);

    //-----------------------------------------------------------------------//
    // "UnSubscribeAll":                                                     //
    //-----------------------------------------------------------------------//
    // Similar to "Unsubscribe" above, but applies to ALL Strategies. Again,
    // this method is NOT purely-virtual:
    //
    virtual void UnSubscribeAll();

  protected:
    //=======================================================================//
    // Utils:                                                                //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "ProcessStartStop":                                                   //
    //-----------------------------------------------------------------------//
    // It  notifies the subscribed Strategies  about the Connector  Start/Stop,
    // or (XXX: in the same way, which is not really good) about the suspension
    // or resumption of trading  at the  Exchange (for all Instrs, or just one
    // Instrument):
    void ProcessStartStop
    (
      bool            a_is_on,    // Start or Stop Flag
      char const*     a_sess_id,  // May be NULL
      SecID           a_sec_id,   // 0   : for all Instrs
      utxx::time_val  a_ts_exch,  // Timings of the triggering event
      utxx::time_val  a_ts_recv   //   (may be empty)
    )
    const;

    //-----------------------------------------------------------------------//
    // "GetMQTEnv":                                                          //
    //-----------------------------------------------------------------------//
    // MQTEnv inferred from the ConnectorName:
    //
    static MQTEnvT GetMQTEnv(std::string const& a_name);

  private:
    //=======================================================================//
    // SecDefs Mgmt in the Ctor:                                             //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "InstallExplicitSecDefs":                                             //
    //-----------------------------------------------------------------------//
    // Install (in SecDefsMgr) the "SecDefD"s derived from given "SecDefS"s for
    // which SettlDates are specified explicitly in the Params:
    //
    void InstallExplicitSecDefs
    (
      std::vector<SecDefS>        const&  a_all_sec_defs,
      std::vector<std::string>    const*  a_only_symbols,
      bool                                a_use_tenors,    // UseTenorsInSecIDs
      boost::property_tree::ptree const&  a_params
    );

    //-----------------------------------------------------------------------//
    // "InstallAllSecDefs":                                                  //
    //-----------------------------------------------------------------------//
    // As opposed to "InstallExplicitSecDefs", this method installs "SecDefD"s
    // derived from ALL available "SecDefS"s. If the corersp SettlDates are gi-
    // ven explicitly in the Params, they are used; otherwise, SettlDate=0  is
    // assumed, which means "truly immediate settlement" (as for Crypto-Assets).
    // In this case, it is assumed that Tenors are NOT used in SecIDs:
    //
    void InstallAllSecDefs
    (
      std::vector<SecDefS>        const&  a_all_sec_defs,
      std::vector<std::string>    const*  a_only_symbols,
      boost::property_tree::ptree const&  a_params
    );
  };
} // End namespace MAQUETTE
