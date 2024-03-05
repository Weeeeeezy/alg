// vim:ts=2:et
//===========================================================================//
//            "Connectors/P2CGate/EConnector_P2CGate_FORTS_MDC.h":           //
//              PlazaII--CGate MktData-Only Connector for FORTS              //
//===========================================================================//
// NB: This is a MktData-only Connector; OrdMgmt is NOT supported, it can much
// better be done using TWIME on FORTS!
// Only the FullOrdersLog mode (infinite-depth synthetic OrderBooks) is suppor-
// ted, incl receiving anonymous Trades:
//
#pragma  once
#include "Connectors/EConnector_MktData.h"
#include "Protocols/P2CGate/OrdBook.h"
#include "Protocols/P2CGate/OrdLog.h"

struct cg_conn_t;
struct cg_listener_t;
struct cg_msg_t;

namespace MAQUETTE
{
  //=========================================================================//
  // "EConnector_P2CGate_FORTS_MDC" Class:                                   //
  //=========================================================================//
  // Similar to "EConnector_FAST_FORTS" (also in FullOrdersLog mode), here we
  // support both FORTS Segments (Futures and Options) within one Connector:
  //
  class EConnector_P2CGate_FORTS_MDC final: public EConnector_MktData
  {
  public:
    //=======================================================================//
    // Native Qty Type:                                                      //
    //=======================================================================//
    // Simiar to FAST and TWIME @ FORTS, all Qtys are Integral Contracts:
    //
    constexpr static QtyTypeT QT   = QtyTypeT::Contracts;
    using                     QR   = long;
    using                     QtyN = Qty<QT,QR>;

  private:
    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    static  bool    s_isEnvInited;
    cg_conn_t*      m_conn;
    cg_listener_t*  m_lsnOBs;
    cg_listener_t*  m_lsnTrs;
    mutable bool    m_isActive;
    mutable bool    m_isOnLineOBs;        // NB: Trades are always "OnLine"

  public:
    //=======================================================================//
    // Non-Default Ctor:                                                     //
    //=======================================================================//
    EConnector_P2CGate_FORTS_MDC
    (
      EPollReactor*                      a_reactor,
      SecDefsMgr*                        a_sec_defs_mgr,
      std::vector<std::string>    const* a_only_symbols, // NULL=UseFullList
      RiskMgr*                           a_risk_mgr,
      boost::property_tree::ptree const& a_params,
      EConnector_MktData*                a_primary_mdc = nullptr
    );

    //=======================================================================//
    // Dtor:                                                                 //
    //=======================================================================//
    // IMPORTANT: The "EConnector_P2CGate_FORTS_MDC" obj must always be properly
    // destructed at the application exit, otherwise the P2MQRouter may be left
    // in an inconsistent state, and would require a restart,  before  it could
    // accept a new connection from us:
    //
    ~EConnector_P2CGate_FORTS_MDC() noexcept override;

    //=======================================================================//
    // Start / Stop / Properties:                                            //
    //=======================================================================//
    // XXX: "Start" (and possibly "Stop") are blocking, so it is recommended
    // that "EConnector_P2CGate_FORTS_MDC" is "Start"ed BEFORE   all "purely
    // non-blocking" Connectors, and "Stop"ped in the reverse order:
    //
    void Start   () override;
    void Stop    () override;
    bool IsActive() const override { return m_isActive; }

  private:
    // This class is final, so the following method must finally be implemented:
    void EnsureAbstract()   const override {}

    //=======================================================================//
    // Listener Call-Backs:                                                  //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // Static Methods (Ptrs to them are passed to the P2CGate API):          //
    //-----------------------------------------------------------------------//
    static unsigned OrderBooksLsnCB
    (
      cg_conn_t*     a_conn,
      cg_listener_t* a_lsn,
      cg_msg_t*      a_msg,
      void*          a_obj
    )
    {
      assert(a_obj != nullptr);
      return reinterpret_cast<EConnector_P2CGate_FORTS_MDC*>(a_obj)->
             OrderBooksLsnCB(a_conn, a_lsn, a_msg);
    }

    static unsigned TradesLsnCB
    (
      cg_conn_t*     a_conn,
      cg_listener_t* a_lsn,
      cg_msg_t*      a_msg,
      void*          a_obj
    )
    {
      assert(a_obj != nullptr);
      return reinterpret_cast<EConnector_P2CGate_FORTS_MDC*>(a_obj)->
             TradesLsnCB(a_conn, a_lsn, a_msg);
    }

    //-----------------------------------------------------------------------//
    // Actual Implementations: Non-Static Class Methods:                     //
    //-----------------------------------------------------------------------//
    unsigned OrderBooksLsnCB
      (cg_conn_t* a_conn, cg_listener_t* a_lsn, cg_msg_t* a_msg);

    unsigned TradesLsnCB
      (cg_conn_t* a_conn, cg_listener_t* a_lsn, cg_msg_t* a_msg);

    //-----------------------------------------------------------------------//
    // Impl of the "GenericHandler" of the "EPollReactor":                   //
    //-----------------------------------------------------------------------//
    void ProcessCGEvents();

    //=======================================================================//
    // Application-Level Processing of P2CGate Msgs:                         //
    //=======================================================================//
    // The following methods return "true" iff successful:
    //
    bool Process(P2CGate::OrdBook::orders    const& a_tmsg, size_t a_len,
    long                               a_owner_id,
    unsigned                           a_msg_id,
    long                               a_rev,
    uint8_t const*                     a_nulls,
    size_t                             a_nn,
    uint64_t                           a_user_id);

    bool Process(P2CGate::OrdLog::orders_log const& a_tmsg, size_t a_len);
  };
} // End namespace MAQUETTE
