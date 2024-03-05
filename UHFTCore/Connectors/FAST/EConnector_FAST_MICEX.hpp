// vim:ts=2:et
//===========================================================================//
//               "Connectors/FAST/EConnector_FAST_MICEX.hpp":                //
//    Embedded (Synchronous) FAST Protocol Connector for MICEX FX and EQ     //
//===========================================================================//
// NB: Unlike "EConnector_FAST_FORTS", this class is relatively simple, so we
// don't need separate ".h" and ".hpp" files:
//
#pragma once

#include "Protocols/FAST/Msgs_MICEX.hpp"
#include "Protocols/FAST/Decoder_MICEX_Curr.h"
#include "Connectors/FAST/EConnector_FAST.hpp"
#include "Venues/MICEX/Configs_FAST.h"

// XXX:   Need FIX implementations here (for linked OMC). So FIXME: Create a
// separate "EConnector_FAST_MICEX.h" file to reduce compilation time:
#include "Connectors/FIX/EConnector_FIX.h"
#include "Connectors/FIX/Configs.hpp"
#include "Connectors/EConnector_OrdMgmt.hpp"
#include "Connectors/EConnector_OrdMgmt_Impl.hpp"
#include "Connectors/FIX/FIX_ConnectorSessMgr.hpp"
#include "Connectors/FIX/EConnector_FIX.hpp"
#include "Protocols/FIX/ProtoEngine.hpp"
#include "Venues/MICEX/Configs_FIX.hpp"

namespace MAQUETTE
{
  //=========================================================================//
  // "EConnector_FAST_MICEX" Class:                                          //
  //=========================================================================//
  // Parameterised by
  // (*) MICEX FAST Protocol Version (they are always in flux, hence a param!)
  // (*) Assset Class (FX or EQ)
  //
  template
  <
    FAST::MICEX::ProtoVerT::type Ver,
    MICEX::AssetClassT::type      AC
  >
  class EConnector_FAST_MICEX   final:        // NB: Finally, it's "final"!
    public EConnector_FAST
    <
      // For OrderBooks:
      FAST::MICEX::SnapShot<Ver>,
      FAST::MICEX::GetMainTID(Ver, AC, FAST::MICEX::DataFeedT::Orders_Snap),
      FAST::MICEX::IncrementalRefresh<Ver>,
      FAST::MICEX::GetMainTID(Ver, AC, FAST::MICEX::DataFeedT::Orders_Incr),
      // WithOrdersLog:
      true,
      // Still, in MICEX, Trades are NOT inferred from the OrdersLog, so need
      // the following:
      FAST::MICEX::TradesIncrement<Ver>,
      FAST::MICEX::GetMainTID(Ver, AC, FAST::MICEX::DataFeedT::Trades_Incr),
      // Others:
      FAST::MICEX::TradingSessionStatus<Ver>,
      FAST::MICEX::TradingSessionStatusTID,
      FAST::MICEX::HeartBeatTID,
      // Qty Composition:
      FAST::MICEX::QT,
      FAST::MICEX::QR,
      // This Derived class:
      EConnector_FAST_MICEX<Ver, AC>,
      // The linked OMC class:
      EConnector_FIX_MICEX
    >
  {
  public:
    //=======================================================================//
    // Types:                                                                //
    //=======================================================================//
    // Native Qty Type:
    using QtyN = Qty<FAST::MICEX::QT, FAST::MICEX::QR>;

    //=======================================================================//
    // Ctors, Dtor:                                                          //
    //=======================================================================//
    // Default Ctor is deleted:
    EConnector_FAST_MICEX() = delete;

    // Dtor has nothing specific to do:
    ~EConnector_FAST_MICEX()  noexcept override {}

    //-----------------------------------------------------------------------//
    // Non-Default Ctor:                                                     //
    //-----------------------------------------------------------------------//
    EConnector_FAST_MICEX
    (
      FAST::MICEX::EnvT                   a_env,
      EPollReactor*                       a_reactor,
      SecDefsMgr*                         a_sec_defs_mgr,
      std::vector<std::string>    const*  a_only_symbols, // NULL=UseFullList
      RiskMgr*                            a_risk_mgr,
      boost::property_tree::ptree const&  a_params,
      EConnector_MktData*                 a_primary_mdc = nullptr
    )
    : //---------------------------------------------------------------------//
      // "EConnector": Virtual Base:                                         //
      //---------------------------------------------------------------------//
      EConnector
      (
        // Generate the ConnectorName (aka "AccountKey") from "AccountPfx":
        a_params.get<std::string>("AccountPfx") + "-FAST-MICEX-" +
                    MICEX::AssetClassT::to_string(AC)      + "-" +
                    FAST::MICEX::ProtoVerT::to_string(Ver) + "-" +
                    FAST::MICEX::EnvT::to_string(a_env),
        "MICEX",
        0,                          // XXX: No extra ShM data at the moment
        a_reactor,
        false,                      // No BusyWait in FAST -- using UDP MCast
        a_sec_defs_mgr,
        MICEX::GetSecDefs(AC, true),
        a_only_symbols,
        a_params.get<bool>("ExplSettlDatesOnly",   true),
        MICEX::UseTenorsInSecIDs,   // (false -- no need for Tenors)
        a_risk_mgr,
        a_params,
        FAST::MICEX::QT,
        FAST::MICEX::WithFracQtys   // (false)
      ),
      //---------------------------------------------------------------------//
      // "EConnector_FAST:                                                   //
      //---------------------------------------------------------------------//
      EConnector_FAST
      <
        FAST::MICEX::SnapShot<Ver>,
        FAST::MICEX::GetMainTID(Ver, AC, FAST::MICEX::DataFeedT::Orders_Snap),
        FAST::MICEX::IncrementalRefresh<Ver>,
        FAST::MICEX::GetMainTID(Ver, AC, FAST::MICEX::DataFeedT::Orders_Incr),
        true,
        FAST::MICEX::TradesIncrement<Ver>,
        FAST::MICEX::GetMainTID(Ver, AC, FAST::MICEX::DataFeedT::Trades_Incr),
        FAST::MICEX::TradingSessionStatus<Ver>,
        FAST::MICEX::TradingSessionStatusTID,
        FAST::MICEX::HeartBeatTID,
        FAST::MICEX::QT,
        FAST::MICEX::QR,
        EConnector_FAST_MICEX,
        EConnector_FIX_MICEX
      >
      (
        a_primary_mdc,
        a_params,
        true,                       // Yes, MICEX RptSeqs are  Strict
        0,                          // MktDepth=+oo (enc as 0) with OrdersLog

        // NB: In MICEX, FullOrdersLog-inferred Trades are unreliable, so "With-
        // Trades" is equivalent to having Explicitly-Subscribed Trades:
        a_params.get<bool>("WithTrades"),
        false,

        // Config  for Orders SnapShots  ('A' only):
        FAST::MICEX::Configs
          .at(std::make_tuple(Ver, a_env, AC))
          .at(FAST::MICEX::DataFeedT::Orders_Snap)
          .first,

        // Configs for Orders Increments ('A' and 'B'):
        FAST::MICEX::Configs
          .at(std::make_tuple(Ver, a_env, AC))
          .at(FAST::MICEX::DataFeedT::Orders_Incr),

        // As explained above, we still need a separate Channel for Trades here,
        // if the Trades are enabled:
        a_params.get<bool>("WithTrades")
        ? &FAST::MICEX::Configs
           .at(std::make_tuple(Ver, a_env, AC))
           .at(FAST::MICEX::DataFeedT::Trades_Incr)
        : nullptr
      )
    {}

    // NB: "Start" and "Stop" are inherited from "EConnector_FAST"

    // This class is final, so the following method must finally be implemented:
    void EnsureAbstract()   const override {}

    //=======================================================================//
    // Decoders which take into account the MICEX-specific semantics:        //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "DecodeOBSnapShot": Use "Decode<AC>" on "SnapShot<Ver>":              //
    //-----------------------------------------------------------------------//
    // (Because for MICEX, we use OrdersLog-based OrderBooks):
    //
    static char const* DecodeOBSnapShot
    (
      char const*                  a_buff,
      char const*                  a_end,
      FAST::PMap                   a_pmap,
      FAST::MICEX::SnapShot<Ver>*  a_snap_shot
    )
    {
      assert(a_snap_shot != nullptr);
      return a_snap_shot->template Decode<AC>(a_buff, a_end, a_pmap);
    }

    //-----------------------------------------------------------------------//
    // "DecodeIncr": For OrderBooks or Trades:                               //
    //-----------------------------------------------------------------------//
    template<typename  Incr>
    static char const* DecodeIncr
    (
      char const* a_buff,
      char const* a_end,
      FAST::PMap  a_pmap,
      Incr*       a_incr
    )
    {
      assert(a_incr != nullptr);
      return a_incr->template Decode<AC>(a_buff, a_end, a_pmap);
    }

    //-----------------------------------------------------------------------//
    // "IsFullyInited":                                                      //
    //-----------------------------------------------------------------------//
    // The MICEX FAST Connector does not require any specific initialisations
    // beyond those (for OrderBooks) performed by "EConnector_FAST", so:
    //
    bool IsFullyInited() const { return this->m_orderBooksInited; }

    //-----------------------------------------------------------------------//
    // "IsActive":                                                           //
    //-----------------------------------------------------------------------//
    bool IsActive() const override
    {
      // NB: If it IsFullyInited, then it is Active, and not Starting anymore:
      bool   res = IsFullyInited();
      assert(!(res && this->m_isStarting));
      return res;
    }
  };
  // End of "EConnector_FAST_MICEX" class decl

  //=========================================================================//
  // Aliases:                                                                //
  //=========================================================================//
  using EConnector_FAST_MICEX_FX = EConnector_FAST_MICEX
    <FAST::MICEX::ProtoVerT::Curr, MICEX::AssetClassT::FX>;

  using EConnector_FAST_MICEX_EQ = EConnector_FAST_MICEX
    <FAST::MICEX::ProtoVerT::Curr, MICEX::AssetClassT::EQ>;
}
// End namespace MAQUETTE
