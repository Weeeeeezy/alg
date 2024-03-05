// vim:ts=2:et
//===========================================================================//
//                      "Connectors/FIX/EConnector_FIX.h":                   //
//               Generic FIX OrdMgmt/MktData Embedded Connector              //
//===========================================================================//
#pragma once

#include "Connectors/EConnector_OrdMgmt.h"
#include "Protocols/FIX/Features.h"
#include "Protocols/FIX/ProtoEngine.h"
#include "Protocols/FIX/SessData.hpp"
#include "Connectors/FIX/FIX_ConnectorSessMgr.h"
#include "Connectors/EConnector_MktData.h"

// XXX: Need to include all FIX Venue-Specific Features here:
#if (!CRYPTO_ONLY)
#include "Venues/AlfaECN/Features_FIX.h"
#include "Venues/AlfaFIX2/Features_FIX.h"
#include "Venues/EBS/Features_FIX.h"
#include "Venues/FORTS/Features_FIX.h"
#include "Venues/FXBA/Features_FIX.h"
#include "Venues/MICEX/Features_FIX.h"
#include "Venues/NTProgress/Features_FIX.h"
#include "Venues/HotSpotFX/Features_FIX.h"
#include "Venues/Currenex/Features_FIX.h"
#endif
#include "Venues/Cumberland/Features_FIX.h"
#include "Venues/LMAX/Features_FIX.h"
#include "Venues/TT/Features_FIX.h"

namespace MAQUETTE
{
  //=========================================================================//
  // "EConnector_FIX" Class:                                                 //
  //=========================================================================//
  // IMPORTANT:
  // (*) "EConnector_FIX" inherits from both "EConnector_{OrdMgmt,MktData}", so
  //     in principle, this class provides BOTH OMC and MDC functionality.
  // (*) However, particular Dialect "D" may or may not provide both functiona-
  //     lities; currently, all Dialects provide OMC, some of them also provide
  //     MDC functionality.
  // (*) Furthermore,even if the Dialect  in question provides both OMC and MDC,
  //     a particular INSTANCE of "EConnectr_FIX<D>" (operating on a given Con-
  //     fig) may provide OMC only, MDC only, or both OMC and MDC.
  // (*) XXX: Currently, at the end, a given instance of "EConnector_FIX" must
  //     provide at least one of OMC / MDC functionality; TODO:  Implement STP
  //     over FIX, which would not be  OMC or MDC:
  //
  template<FIX::DialectT::type D>
  class    EConnector_FIX final:
    public EConnector_OrdMgmt,
    public EConnector_MktData,
    public FIX_ConnectorSessMgr<D, EConnector_FIX<D>>
  {
  private:
    //=======================================================================//
    // Internal Types:                                                       //
    //=======================================================================//
    // SessMgr, ProtoEngine and Helers must be Friends:
    //
    using  SessMgr = FIX_ConnectorSessMgr<D, EConnector_FIX<D>>;
    friend class     FIX_ConnectorSessMgr<D, EConnector_FIX<D>>;

    // NB: IsServer=false in ProtoEngine:
    using  ProtoEngT =
      FIX::ProtoEngine
      <D, false, FIX_ConnectorSessMgr<D, EConnector_FIX<D>>, EConnector_FIX<D>>;
    friend class
      FIX::ProtoEngine
      <D, false, FIX_ConnectorSessMgr<D, EConnector_FIX<D>>, EConnector_FIX<D>>;

    //=======================================================================//
    // Flds:                                                                 //
    //=======================================================================//
    // User-supplied "transient" handler for SecurityDefintion and
    // SecurityList msgs:
    std::function<void(FIX::SecurityDefinition const&)> m_secDefHandler;
    std::function<void(FIX::SecurityList const&)>       m_secListHandler;

    // Streaming quote IDs:
    using ProtoQuoteIDsVec =
          boost::container::static_vector<ObjName, Limits::MaxInstrs>;
    ProtoQuoteIDsVec*                              m_quoteIDs;

  public:
    //=======================================================================//
    // External Types and Consts:                                            //
    //=======================================================================//
    // "QtyN" type and its template params are defined by the ProtoEngine:
    // It is obviously same as Qty<QT,QR>:
    //
    using                     QR   = typename ProtoEngT::QR;
    constexpr static QtyTypeT QT   = ProtoEngT::QT;
    using                     QtyN = typename ProtoEngT::QtyN;

    // Check the types consistency:
    static_assert
    (
      std::is_same_v<QtyN,    Qty<QT,QR>>                                     &&
      std::is_same_v<QtyN,    typename SessMgr::QtyN>                         &&
      std::is_same_v<QR,      typename SessMgr::QR>                           &&
      std::is_floating_point_v<QR> == FIX::ProtocolFeatures<D>::s_hasFracQtys &&
      QT == SessMgr::QT,
      "EConnector_FIX: Inconsistent Qty Types"
    );

    // We assume that "Cancel" is normally NOT free in FIX, ie if Throttling is
    // in place, "Cancel" counts as a normal req:
    constexpr static bool HasFreeCancel = false;

    // use sparse order book for TT, otherwise dense
    constexpr static bool IsSparse = (D == FIX::DialectT::TT);

    // NB: "OMC" is the type of OMC to be linked with this MDC; it will  be same
    // EConnector_FIX, XXX though for FIX, such a link is probably not very much
    // useful:
    using                     OMC  = EConnector_FIX;

    //=======================================================================//
    // Non-Default Ctor, Dtor:                                               //
    //=======================================================================//
    // NB: "a_params" must be a sub-tree where all relevant params are located
    // at Level 1:
    EConnector_FIX
    (
      EPollReactor*                      a_reactor,
      SecDefsMgr*                        a_sec_defs_mgr,
      std::vector<std::string>    const* a_only_symbols,         // NULL=All
      RiskMgr*                           a_risk_mgr,
      boost::property_tree::ptree const& a_params,
      EConnector_MktData*                a_primary_mdc = nullptr // Only if MDC!
    );

    // Default Ctor is deleted:
    EConnector_FIX() = delete;

    // Dtor is actually trivial -- but that of "FIX_ConnectorSessMgr" is not!
    // (Still, needs to be implemented in the .hpp):
    // Because this class is "final", no need to make the Dtor "virtual", but
    // it still "override"s the parent Dtors! XXX: The mechanism of automatic-
    // ally inserting calls to virtual base classes' virtual Dtor looks compl-
    // ex; but unlike Ctor, it can be done automatically because there are no
    // params in the Dtor!
    ~EConnector_FIX()  noexcept override;

    //=======================================================================//
    // "Start", "Stop", Properties, Subscription Mgmt:                       //
    //=======================================================================//
    void Start()          override;
    void Stop()           override;
    bool IsActive() const override;

    // Due to mulltiple inheritance, we need to provide explicit implementati-
    // ons of the Subscription Mgmt methods:
    //
    using EConnector_OrdMgmt::Subscribe;
    using EConnector_MktData::UnSubscribe;
    using EConnector_MktData::UnSubscribeAll;

    // "SubscribeMktData" is inherited from "EConnector_MktData" (no ambiguity)

    //=======================================================================//
    // "SubscribeStreamingQuote":                                            //
    //=======================================================================//
    // Subscribing a given Strategy to a Streaming Quote with an optional
    // quantity
    //
    template <QtyTypeT QtyType = QT>
    void SubscribeStreamingQuote
    (
      Strategy*                 a_strat,
      SecDefD const&            a_instr,
      bool                      a_reg_instr_risks,   // Interact with RiskMgr?
      Qty<QtyType, QR>          a_quantity = QtyN()  // Optional, def=0
    );

    //=======================================================================//
    // "CancelStreamingQuote":                                               //
    //=======================================================================//
    // Cancel a streaming quote
    //
    void CancelStreamingQuote
    (
      SecDefD const&            a_instr
    );

  private:
    // This class is final, so the following method must finally be implemented:
    void EnsureAbstract() const override {}

    //-----------------------------------------------------------------------//
    // MktData Subscription Mgmt:                                            //
    //-----------------------------------------------------------------------//
    // IsSubscr=true for Subscribe, false for UnSubscribe:
    //
    template<bool IsSubscr>
    void SendMktDataSubscrReqs();

    //=======================================================================//
    // Processing of Received Msgs of All Types:                             //
    //=======================================================================//
    // Temporal params (if applicable) are RecvTS, HandlTS:
    //-----------------------------------------------------------------------//
    // Processing End-of-data-Chunk:  No Msg here:                           //
    //-----------------------------------------------------------------------//
    void Process(FIX::SessData const*);

    //-----------------------------------------------------------------------//
    // Processing of Application-Level OrdMgm Msgs:                          //
    //-----------------------------------------------------------------------//
    void Process(FIX::OrderCancelReject                     const&,
                 FIX::SessData const*, utxx::time_val, utxx::time_val);

    void Process(FIX::BusinessMessageReject                 const&,
                 FIX::SessData const*, utxx::time_val, utxx::time_val);

    void Process(FIX::ExecutionReport<QtyN>                 const&,
                 FIX::SessData const*, utxx::time_val, utxx::time_val);

    //-----------------------------------------------------------------------//
    // Processing of Application-Level "Quote-Based" Msgs:                   //
    //-----------------------------------------------------------------------//
    // NB: Such msgs are typically received on the Server Side, or if the Cli-
    // ent provides Streaming Quotes. Temporal param: RecvTS:
    //
    void Process(FIX::NewOrderSingle<QtyN>             const&,
                 FIX::SessData const*, utxx::time_val, utxx::time_val);

    void Process(FIX::OrderCancelRequest               const&,
                 FIX::SessData const*, utxx::time_val, utxx::time_val);

    void Process(FIX::OrderCancelReplaceRequest<QtyN>  const&,
                 FIX::SessData const*, utxx::time_val, utxx::time_val);

    void Process(FIX::MarketDataRequest                const&,
                 FIX::SessData const*, utxx::time_val, utxx::time_val);

    void Process(FIX::QuoteRequestReject                    const&,
                 FIX::SessData const*, utxx::time_val, utxx::time_val);

    void Process(FIX::Quote<QtyN>                           const&,
                 FIX::SessData const*, utxx::time_val, utxx::time_val);

    void Process(FIX::QuoteStatusReport                     const&,
                 FIX::SessData const*, utxx::time_val, utxx::time_val);

    //-----------------------------------------------------------------------//
    // Processing of Application-Level MktData Msgs:                         //
    //-----------------------------------------------------------------------//
    // Temporal params: RecvTS, and (if present) HandlTS:
    //
    void Process(FIX::MarketDataRequestReject               const&,
                 FIX::SessData const*, utxx::time_val, utxx::time_val);

    void Process(FIX::SecurityDefinition                    const&,
                 FIX::SessData const*, utxx::time_val, utxx::time_val);

    void Process(FIX::SecurityStatus                        const&,
                 FIX::SessData const*, utxx::time_val, utxx::time_val);

    void Process(FIX::SecurityList                          const&,
                 FIX::SessData const*, utxx::time_val, utxx::time_val);

    void Process(FIX::MarketDataIncrementalRefresh<D, QtyN> const&,
                 FIX::SessData const*, utxx::time_val, utxx::time_val);

    void Process(FIX::MarketDataSnapShot<D, QtyN>           const&,
                 FIX::SessData const*, utxx::time_val, utxx::time_val);

  public:
    //=======================================================================//
    // Impls of Virtual "Send" Methods from "EConnector_OrdMgmt":            //
    //=======================================================================//
    // Re-using generic impls from the same base class; "SessMgr" is THIS cls:
    //-----------------------------------------------------------------------//
    // "NewOrder":                                                           //
    //-----------------------------------------------------------------------//
    AOS const* NewOrder
    (
      // The originating Stratergy:
      Strategy*           a_strategy,

      // Main order params:
      SecDefD const&      a_instr,
      FIX::OrderTypeT     a_ord_type,
      bool                a_is_buy,
      PriceT              a_px,
      bool                a_is_aggr,      // Aggressive order?
      QtyAny              a_qty,          // Full Qty

      // Temporal params of the triggering event:
      utxx::time_val      a_ts_md_exch    = utxx::time_val(),
      utxx::time_val      a_ts_md_conn    = utxx::time_val(),
      utxx::time_val      a_ts_md_strat   = utxx::time_val(),

      // Optional order params:
      // NB: TimeInForceT::UNDEFINED must automatically resolve to a Connector-
      // specific default:
      bool                a_batch_send    = false,
      FIX::TimeInForceT   a_time_in_force = FIX::TimeInForceT::UNDEFINED,
      int                 a_expire_date   = 0,        // Or YYYYMMDD
      QtyAny              a_qty_show      = QtyAny::PosInf(),
      QtyAny              a_qty_min       = QtyAny::Zero  (),
      bool                a_peg_side      = true,     // True: this, false: opp
      double              a_peg_offset    = NaN<double>
    )
    override;

   //-----------------------------------------------------------------------//
    // "NewStreaminQuoteOrder":                                              //
    //-----------------------------------------------------------------------//
    // Simplified form of "NewOrder", where an Order is sent in response to  a
    // Streaming Quote. XXX: Maybe pass a single "StreamingQuote" arg. This is
    // essentially a Market Order. Other params  (eg QuoteID)  are takem  from
    // the Connector state:
    //
    AOS const* NewStreamingQuoteOrder
    (
      // The originating Stratergy:
      Strategy*           a_strategy,

      // Main order params:
      SecDefD const&      a_instr,
      bool                a_is_buy,
      QtyAny              a_qty,

      // Temporal params of the triggering MktData event:
      utxx::time_val      a_ts_md_exch  = utxx::time_val(),
      utxx::time_val      a_ts_md_conn  = utxx::time_val(),
      utxx::time_val      a_ts_md_strat = utxx::time_val(),

      // Optional order params:
      // NB: TimeInForceT::UNDEFINED must automatically resolve to a Connector-
      // specific default:
      bool                a_batch_send  = false     // Recommendation Only!
    );

    //-----------------------------------------------------------------------//
    // "CancelOrder":                                                        //
    //-----------------------------------------------------------------------//
    bool CancelOrder
    (
      AOS const*          a_aos,          // Non-NULL
      utxx::time_val      a_ts_md_exch  = utxx::time_val(),
      utxx::time_val      a_ts_md_conn  = utxx::time_val(),
      utxx::time_val      a_ts_md_strat = utxx::time_val(),
      bool                a_batch_send  = false
    )
    override;

    //-----------------------------------------------------------------------//
    // "ModifyOrder":                                                        //
    //-----------------------------------------------------------------------//
    bool ModifyOrder
    (
      AOS const*          a_aos,            // Non-NULL
      PriceT              a_new_px,
      bool                a_is_aggr,        // Becoming Aggressive?
      QtyAny              a_new_qty,
      utxx::time_val      a_ts_md_exch   = utxx::time_val(),
      utxx::time_val      a_ts_md_conn   = utxx::time_val(),
      utxx::time_val      a_ts_md_strat  = utxx::time_val(),
      bool                a_batch_send   = false,
      QtyAny              a_new_qty_show = QtyAny::PosInf(),
      QtyAny              a_new_qty_min  = QtyAny::Zero  ()
    )
    override;

    //-----------------------------------------------------------------------//
    // "CancelAllOrders":                                                    //
    //-----------------------------------------------------------------------//
    void CancelAllOrders
    (
      unsigned long       a_strat_hash48 = 0,                     // All  Strats
      SecDefD const*      a_instr        = nullptr,               // All  Instrs
      FIX::SideT          a_side         = FIX::SideT::UNDEFINED, // Both Sides
      char const*         a_segm_id      = nullptr                // All  Segms
    )
    override;

    //-----------------------------------------------------------------------//
    // "FlushOrders":                                                        //
    //-----------------------------------------------------------------------//
    utxx::time_val FlushOrders() override;

    //=======================================================================//
    // "GetSecurityDefinition":                                              //
    //=======================================================================//
    // Performs a one-off request for SecurityDefintion; when the response
    // comes in, it is processed by the specified Handler:
    //
    void GetSecurityDefinition
    (
      std::function<void(FIX::SecurityDefinition const&)> a_handler,
      const std::string& a_sec_type,
      const std::string& a_exch_id
    );

    //=======================================================================//
    // "GetSecurityList":                                                    //
    //=======================================================================//
    // Performs a one-off request for SecurityList; when the response comes in,
    // it is processed by the specified Handler:
    //
    void GetSecurityList
      (std::function<void(FIX::SecurityList const&)> a_handler);
  };

  //=========================================================================//
  // External Instances of "EConnector_FIX":                                 //
  //=========================================================================//
# if (!CRYPTO_ONLY)
  using EConnector_FIX_AlfaECN        =
        EConnector_FIX<FIX::DialectT::AlfaECN>;

  using EConnector_FIX_AlfaFIX2       =
        EConnector_FIX<FIX::DialectT::AlfaFIX2>;

  using EConnector_FIX_Currenex       =
        EConnector_FIX<FIX::DialectT::Currenex>;

  using EConnector_FIX_EBS            =
        EConnector_FIX<FIX::DialectT::EBS>;

  using EConnector_FIX_FORTS          =
        EConnector_FIX<FIX::DialectT::FORTS>;

  using EConnector_FIX_FXBA           =
        EConnector_FIX<FIX::DialectT::FXBA>;

  using EConnector_FIX_HotSpotFX_Gen  =
        EConnector_FIX<FIX::DialectT::HotSpotFX_Gen>;

  using EConnector_FIX_HotSpotFX_Link =
        EConnector_FIX<FIX::DialectT::HotSpotFX_Link>;

  using EConnector_FIX_MICEX          =
        EConnector_FIX<FIX::DialectT::MICEX>;

  using EConnector_FIX_NTProgress     =
        EConnector_FIX<FIX::DialectT::NTProgress>;
# endif
  using EConnector_FIX_Cumberland     =
        EConnector_FIX<FIX::DialectT::Cumberland>;

  using EConnector_FIX_LMAX          =
        EConnector_FIX<FIX::DialectT::LMAX>;

  using EConnector_FIX_TT          =
        EConnector_FIX<FIX::DialectT::TT>;
}
// End namespace MAQUETTE
