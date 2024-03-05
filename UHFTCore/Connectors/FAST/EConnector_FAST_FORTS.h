// vim:ts=2:et
//===========================================================================//
//                 "Connectors/FAST/EConnector_FAST_FORTS.h":                //
//  Embedded (Synchronous) FAST Protocol Connector for FORTS Futs and Opts   //
//===========================================================================//
// NB: In FORTS FullOrdersLog mode, we receive mkt data for BOTH Fut  and Opt
// Segments (there is no way of separating them);  so we will do both of them
// for SecDefs and SecDefUpdates, as well:
//
#pragma once

#include "Protocols/FAST/Decoder_FORTS_Curr.h"  // XXX: This is unfortunate!
#include "Connectors/SSM_Channel.hpp"
#include "Connectors/FAST/EConnector_FAST.hpp"
#include "Connectors/TWIME/EConnector_TWIME_FORTS.h"

namespace MAQUETTE
{
  //=========================================================================//
  // "EConnector_FAST_FORTS" Class:                                          //
  //=========================================================================//
  // Parameterised by FORTS FAST Protocol Version (they are always in flux,
  // hence a param!)
  //
  template<FAST::FORTS::ProtoVerT::type Ver>
  class    EConnector_FAST_FORTS   final:   // NB: Finally, it's "final"!
    public EConnector_FAST
    <
      // For OrderBooks:
      FAST::FORTS::OrdersLogSnapShot      <Ver>,
      FAST::FORTS::OrdersLogSnapShotTID   <Ver>(),
      FAST::FORTS::OrdersLogIncrRefresh   <Ver>,
      FAST::FORTS::OrdersLogIncrRefreshTID<Ver>(),
      // WithOrdersLog:
      true,
      // XXX: No separate Trade Msgs here: Trades are inferred from the Orders-
      // Log. So the following type param is a PLACEHOLDER ONLY; its TID=0:
      FAST::FORTS::OrdersLogIncrRefresh   <Ver>,
      0,
      // Others:
      FAST::FORTS::TradingSessionStatus   <Ver>,
      FAST::FORTS::TradingSessionStatusTID<Ver>(),
      FAST::FORTS::HeartBeatTID<Ver>(),
      // Qty Composition:
      FAST::FORTS::QT,
      FAST::FORTS::QR,
      // This Derived Class:
      EConnector_FAST_FORTS<Ver>,
      // The linked OMC Class:
      EConnector_TWIME_FORTS
    >
  {
  public:
    //=======================================================================//
    // Types:                                                                //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // Native Qty Type:                                                      //
    //-----------------------------------------------------------------------//
    using QtyN = Qty<FAST::FORTS::QT, FAST::FORTS::QR>;

  private:
    //-----------------------------------------------------------------------//
    // Internal Types: Parent Class for easy access:                         //
    //-----------------------------------------------------------------------//
    using ParentT =
      EConnector_FAST
      <
        FAST::FORTS::OrdersLogSnapShot      <Ver>,
        FAST::FORTS::OrdersLogSnapShotTID   <Ver>(),
        FAST::FORTS::OrdersLogIncrRefresh   <Ver>,
        FAST::FORTS::OrdersLogIncrRefreshTID<Ver>(),
        true,
        FAST::FORTS::OrdersLogIncrRefresh   <Ver>,
        0,
        FAST::FORTS::TradingSessionStatus   <Ver>,
        FAST::FORTS::TradingSessionStatusTID<Ver>(),
        FAST::FORTS::HeartBeatTID<Ver>(),
        FAST::FORTS::QT,
        FAST::FORTS::QR,
        EConnector_FAST_FORTS<Ver>,
        EConnector_TWIME_FORTS
      >;

    //=======================================================================//
    // "SecDefsUpdCh":                                                       //
    //=======================================================================//
    // Receiving "SecurityDefinitionUpdate" & "SecurityStatus" msgs. NB:  This
    // Channel currently uses only 1 feed ('A') -- data loss is tolerable (and
    // is very unlikely anyway):
    //
    class SecDefsUpdCh: public SSM_Channel<SecDefsUpdCh>
    {
    private:
      //---------------------------------------------------------------------//
      // Data Flds:                                                          //
      //---------------------------------------------------------------------//
      // "SecurityDefinition" msgs are parsed into the following:
      EConnector_FAST_FORTS*  const                       m_mdc;
      mutable FAST::FORTS::SecurityDefinitionUpdate<Ver>  m_secDefUpd;
      mutable FAST::FORTS::SecurityStatus<Ver>            m_secStat;

      // Default Ctor is deleted:
      SecDefsUpdCh() = delete;

      friend class EConnector_FAST_FORTS;

    public:
      //---------------------------------------------------------------------//
      // Non-Default Ctor:                                                   //
      //---------------------------------------------------------------------//
      SecDefsUpdCh
      (
        EConnector_FAST_FORTS*  a_mdc,
        FORTS::AssetClassT      a_ac,
        std::string const&      a_iface_ip
      );

      //---------------------------------------------------------------------//
      // "RecvHandler":                                                      //
      //---------------------------------------------------------------------//
      // "SecurityDefinitionUpdate" and "SecurityStatus" decoding and processing
      // occur HERE:
      bool RecvHandler
      (
        int                     a_fd,
        char const*             a_buff,
        int                     a_size,
        utxx::time_val          a_ts_recv
      );

      //---------------------------------------------------------------------//
      // "ErrHandler":                                                       //
      //---------------------------------------------------------------------//
      // Errors in this Channel are not critical. We simply restart the Channel
      // in case of any errors:
      void ErrHandler
      (
        int                     a_fd,
        int                     a_err_code,
        uint32_t                a_events,
        char const*             a_msg
      );
    };

    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    FAST::FORTS::EnvT::type    m_env;
    SecDefsUpdCh               m_secDefsUpdCh_Fut;
    SecDefsUpdCh               m_secDefsUpdCh_Opt;
    mutable bool               m_secDefsInited;

  public:
    //=======================================================================//
    // Ctors, Dtor, "Start", "Stop":                                         //
    //=======================================================================//
    // Default Ctor is deleted:
    EConnector_FAST_FORTS() = delete;

    // Dtor has nothing specific to do:
    ~EConnector_FAST_FORTS()  noexcept override {}

    // "Start", "Stop":
    void Start() override;
    void Stop()  override;

    //=======================================================================//
    // Non-Default Ctor:                                                     //
    //=======================================================================//
    EConnector_FAST_FORTS
    (
      FAST::FORTS::EnvT                   a_env,
      EPollReactor*                       a_reactor,
      SecDefsMgr*                         a_sec_defs_mgr,
      std::vector<std::string>    const*  a_only_symbols,  // NULL=UseFullList
      RiskMgr*                            a_risk_mgr,
      boost::property_tree::ptree const&  a_params,
      EConnector_MktData*                 a_primary_mdc = nullptr
    );

    // This class is final, so the following method must finally be implemented:
    void EnsureAbstract() const override {}

    //=======================================================================//
    // Connector Status:                                                     //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "IsFullyInited":                                                      //
    //-----------------------------------------------------------------------//
    // XXX: We are NOT waiting for receiving all SecDefUpdates -- they may not
    // be necessary (and so managed at the Strategy level if required):
    //
    bool IsFullyInited() const;

    //-----------------------------------------------------------------------//
    // "IsActive":                                                           //
    //-----------------------------------------------------------------------//
    bool IsActive() const override;

    //-----------------------------------------------------------------------//
    // "SubscribeMktData":                                                   //
    //-----------------------------------------------------------------------//
    // For FAST FORTS, we need to override the generic functionality implemented
    // in "EConnector_MktData":
    //
    void SubscribeMktData
    (
      Strategy*                 a_strat,
      SecDefD const&            a_instr,
      OrderBook::UpdateEffectT  a_min_cb_level,
      bool                      a_reg_instr_risks
    )
    override;

    //=======================================================================//
    // Decoders which take into account the FORTS-specific semantics:        //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "DecodeOBSnapShot": For OrderBooks:                                   //
    //-----------------------------------------------------------------------//
    static char const* DecodeOBSnapShot
    (
      char const*                             a_buff,
      char const*                             a_end,
      FAST::PMap                              a_pmap,
      FAST::FORTS::OrdersLogSnapShot<Ver>*    a_snap_shot
    );

    //-----------------------------------------------------------------------//
    // "DecodeIncr": For OrderBooks:                                         //
    //-----------------------------------------------------------------------//
    static char const* DecodeIncr
    (
      char const*                             a_buff,
      char const*                             a_end,
      FAST::PMap                              a_pmap,
      FAST::FORTS::OrdersLogIncrRefresh<Ver>* a_incr
    );
  };
  // End of "EConnector_FAST_FORTS" class decl

  //=========================================================================//
  // Alias:                                                                  //
  //=========================================================================//
  using EConnector_FAST_FORTS_FOL =
        EConnector_FAST_FORTS<FAST::FORTS::ProtoVerT::Curr>;
}
// End namespace MAQUETTE
