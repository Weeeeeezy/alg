// vim:ts=2:et:sw=2
//===========================================================================//
//                                "ConnEmu.h":                               //
//  MDC/OMC Emulator for Strategies running in the Dry-Run (Emulation) Mode: //
//===========================================================================//
#ifndef MAQUETTE_STRATEGYADAPTOR_CONNEMU_H
#define MAQUETTE_STRATEGYADAPTOR_CONNEMU_H

#include "Common/DateTime.h"
#include "StrategyAdaptor/EventTypes.hpp"
#include "Infrastructure/SecDefTypes.h
#include "DB/SQLSequencer.h"

#include <memory>
#include <boost/interprocess/containers/deque.hpp>
// XXX: "boost::circular_buffer" in ShMem cannot be used with debugging:
#include <boost/circular_buffer.hpp>

namespace MAQUETTE
{
  //=========================================================================//
  // "ConnEmu" Class:                                                        //
  //=========================================================================//
  class ConnEmu
  {
  public:
    //-----------------------------------------------------------------------//
    // Ctors, Dtor...                                                        //
    //-----------------------------------------------------------------------//
    // Non-Default Ctor:
    ConnEmu
    (
      std::string const& strat_name,
      int                n_mdcs,
      int                debug_level
    );

    // Dtor (non-trivial):
    ~ConnEmu();

  private:
    //-----------------------------------------------------------------------//
    // Hidden / Deleted Stuff:                                               //
    //-----------------------------------------------------------------------//
    ConnEmu()                          = delete;
    ConnEmu(ConnEmu const&)            = delete;
    ConnEmu(ConnEmu&&)                 = delete;
    ConnEmu& operator=(ConnEmu const&) = delete;
    ConnEmu& operator=(ConnEmu&&)      = delete;

    bool operator==   (ConnEmu const&) const;
    bool operator!=   (ConnEmu const&) const;

    //-----------------------------------------------------------------------//
    // Data Types and Flds:                                                  //
    //-----------------------------------------------------------------------//
    int                                           m_nMDCs;     // For Ref calcs
    Arbalete::DateTime                            m_SimulateFrom;
    Arbalete::DateTime                            m_SimulateTo;
    MYSQL*                                        m_DB;
    int                                           m_DebugLevel;

    // For the SQL Sequencer:
    //
    MySQLDB::SQLSequencer::InstrsMap              m_InstrsMap;
    MySQLDB::SQLSequencer*                        m_SQLS;
    mutable MySQLDB::SQLSequencer::OrderBooksMap  m_NextOrderBooks;
    mutable TradeEntry                            m_NextTrade;

    // XXX: The following is a map which allows us to re-construct ClientRefs
    // when "event"s are received from a DB. HOWEVER, with the curr implemen-
    // tation, we only get the Symbol on a DB event, not the DB name!  So all
    // Symbols currently need to be different -- FIXME!
    //
    typedef
      std::map<Events::SymKey, std::pair<Events::ClientRef, Events::ObjName>>
      RefsMap;
    mutable RefsMap                               m_RefsMap;

    // Queue of msgs to be delivered to the Strategy: older msgs are still ava-
    // lable (like in case of real MktData delivery) until they are overwritten
    // by new ones:
    typedef
      boost::circular_buffer<Events::EventMsg>
      ToStratBuff;
    mutable ToStratBuff                           m_ToStratBuff;

    // Indications submitted by the Strategy (will result in Order Mgmt Events
    // being returned):
    mutable std::deque<Events::Indication>        m_Indications;

    //-----------------------------------------------------------------------//
    // If ShM Cache is used (to cache data from SQL Sequencer):              //
    //-----------------------------------------------------------------------//
    // The type of cached data:
    union CachedU
    {
      OrderBookSnapShot m_OB;
      TradeEntry        m_Tr;

      // Default Ctor:
      CachedU() { memset(this, '\0', sizeof(CachedU)); }
    };

    struct CachedS
    {
      MySQLDB::SQLSequencer::ResultT  m_tag;
      Events::SymKey                  m_symbol;
      Arbalete::DateTime              m_ts;
      CachedU                         m_U;

      // Default Ctor:
      CachedS()
      : m_tag   (MySQLDB::SQLSequencer::ResultT::Nothing),
        m_symbol(),
        m_ts    (),
        m_U     ()
      {}
    };

    typedef
      BIPC::allocator<CachedS,
                      BIPC::fixed_managed_shared_memory::segment_manager>
      CachedSAllocator;

    // NB: use "deque", not "vector", to avoid re-allocations as the size
    // grows:
    typedef
      BIPC::deque<CachedS, CachedSAllocator>
      CachedSDEQ;

    // The Cache objects (in ShM):
    //
    std::string                                        m_SegmentName;
    std::unique_ptr<BIPC::fixed_managed_shared_memory> m_Segment;
    bool                                               m_FromCache;
    std::unique_ptr<CachedSAllocator>                  m_CacheAlloc;
    CachedSDEQ*                                        m_Cache;
    mutable int                                        m_CacheIdx;

  public:
    //=======================================================================//
    // Methods:                                                              //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "GetSecDef":                                                          //
    //-----------------------------------------------------------------------//
    SecDef const* GetSecDef
    (
      Events::ObjName const& mdc_name,      // Same as DB Name
      Events::SymKey  const& symbol,
      SecDefBuff*            output
    )
    const;

    //-----------------------------------------------------------------------//
    // "Subscribe":                                                          //
    //-----------------------------------------------------------------------//
    void Subscribe
    (
      Events::ObjName const& mdc_name,      // Same as DB Name
      Events::SymKey  const& symbol,
      Events::ClientRef      ref            // Same as MDC#
    );

    //-----------------------------------------------------------------------//
    // "Unsubscribe":                                                        //
    //-----------------------------------------------------------------------//
    void Unsubscribe
    (
      Events::ObjName const& mdc_name,      // Same as DB Name
      Events::SymKey  const& symbol
    );

    //-----------------------------------------------------------------------//
    // "UnsubscribeAll":                                                     //
    //-----------------------------------------------------------------------//
    void UnsubscribeAll(Events::ObjName const& mdc_name);

    //-----------------------------------------------------------------------//
    // "GetNextEvent":                                                       //
    //-----------------------------------------------------------------------//
    // Without deadline:
    void GetNextEvent(Events::EventMsg* msg) const;

    // With deadline:
    bool GetNextEvent
    (
      Events::EventMsg*      msg,
      Arbalete::DateTime     deadline
    )
    const;

    //-----------------------------------------------------------------------//
    // "SendIndication":                                                     //
    //-----------------------------------------------------------------------//
    void SendIndication(Events::Indication const& ind) const;

  private:
    //=======================================================================//
    // Internal Methods:                                                     //
    //=======================================================================//
    void ReBuildSequencer();

    bool GetNextEventCommon
    (
      Events::EventMsg*      msg,
      Arbalete::DateTime*    currDT
    )
    const;

    bool GetNextEventFromCache
    (
      Arbalete::DateTime     deadline,
      CachedS*               cs
    )
    const;

    void ProcessMktDataEvent
    (
      MySQLDB::SQLSequencer::ResultT  resT,
      Events::SymKey          const&  symbol,
      Arbalete::DateTime              deadline,
      Events::EventMsg*               msg
    )
    const;

    void ReportFill
    (
      Events::Indication const&       ind,
      Events::ClientRef               ref,
      Arbalete::DateTime              now,
      double                          price
    )
    const;

    void ReportModify
    (
      Events::Indication const&       ind,
      Events::ClientRef               ref
    )
    const;

    void ReportCancel
    (
      Events::Indication const&       ind,
      Events::ClientRef               ref
    )
    const;

    void ReportCancelReject
    (
      Events::OrderID                 ord_id,
      bool                            is_buy,
      Events::ClientRef               ref
    )
    const;
  };
}
#endif  // MAQUETTE_STRATEGYADAPTOR_CONNEMU_H
