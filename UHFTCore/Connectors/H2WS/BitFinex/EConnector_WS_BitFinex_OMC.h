// vim:ts=2:et
//===========================================================================//
//          "Connectors/H2WS/BitFinex/EConnector_WS_BitFinex_OMC.h":         //
//                          WS-Based OMC for BitFinex                        //
//===========================================================================//
#pragma once

#include "Basis/Maths.hpp"
#include "Connectors/H2WS/EConnector_WS_OMC.h"
#include "Connectors/H2WS/EConnector_WS_OMC.hpp"
#include "Protocols/FIX/Msgs.h"

namespace MAQUETTE
{
  //=========================================================================//
  // "EConnector_WS_BitFinex_OMC" Class:                                     //
  //=========================================================================//
  class EConnector_WS_BitFinex_OMC final
      : public EConnector_WS_OMC<EConnector_WS_BitFinex_OMC>
  {
  public:
    constexpr static char const Name[] = "WS-BitFinex-OMC";
    //=======================================================================//
    // Types:                                                                //
    //=======================================================================//
    // The Native Qty type: BitFinex uses Fractional QtyA:
    constexpr static QtyTypeT QT = QtyTypeT::QtyA;
    using QR                     = double;
    using QtyN                   = Qty<QT, QR>;

  private:
    // "TCP_Connector", "EConnector_WS_OMC" and "H2WS::WSProtoEngine" must have
    // full access to this class:
    friend class   TCP_Connector      <EConnector_WS_BitFinex_OMC>;
    friend class   EConnector_WS_OMC  <EConnector_WS_BitFinex_OMC>;
    friend class   H2WS::WSProtoEngine<EConnector_WS_BitFinex_OMC>;
    friend class   EConnector_OrdMgmt;
    friend class   SecDefsMgr;
    using  OMCWS = EConnector_WS_OMC  <EConnector_WS_BitFinex_OMC>;

    //=======================================================================//
    // Configuration:                                                        //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // Flags required by "EConnector_WS_OMC":  CHECK!!!                      //
    //-----------------------------------------------------------------------//
    // NB: UseExchOrdIDsMap is False even if ExchOrdIDs may be used in "Modify"
    // or "Cancel" msgs;  what is important is that in BitFinex, ExchOrdIDs do
    // NOT identify Req12s, because they are constant per-AOS:
    //
    constexpr static int  ThrottlingPeriodSec   = 60;     // 1 min
    constexpr static int  MaxReqsPerPeriod      = 100000; // Or even less!
    constexpr static PipeLineModeT PipeLineMode = PipeLineModeT::Wait0;
    constexpr static bool SendExchOrdIDs        = true;
    constexpr static bool UseExchOrdIDsMap      = false;
    constexpr static bool HasAtomicModify       = true;   // This is normal
    constexpr static bool HasPartFilledModify   = true;
    constexpr static bool HasExecIDs            = false;  // Peculiar!
    constexpr static bool HasMktOrders          = false;  // Not implemented
    constexpr static bool HasFreeCancel         = false;  // Cancels DO count

    //-----------------------------------------------------------------------//
    // "GetWSConfig":                                                        //
    //-----------------------------------------------------------------------//
    // XXX: The only supported Env is Prod:
    static H2WSConfig           const& GetWSConfig(MQTEnvT a_env);
    static std::vector<SecDefS> const* GetSecDefs();

    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    // CurrDate (used in the wire protocol; the OMC is to be restarted when the
    // CurrDate changes in UTC):
    mutable utxx::time_val  m_currDate;
    mutable char            m_currDateStr[12];
    mutable char            m_tmpBuff    [100];

  public:
    //=======================================================================//
    // Non-Default Ctor, Dtor:                                               //
    //=======================================================================//
    EConnector_WS_BitFinex_OMC
    (
      EPollReactor*                      a_reactor,        // Non-NULL
      SecDefsMgr*                        a_sec_defs_mgr,   // Non-NULL
      std::vector<std::string>    const* a_only_symbols,   // NULL=UseFullList
      RiskMgr*                           a_risk_mgr,       // Depends...
      boost::property_tree::ptree const& a_params
    );

    // The Dtor is trivial, and non-virtual (because this class is "final"):
    ~EConnector_WS_BitFinex_OMC() noexcept override;

  private:
    // This class is final, so the following method must finally be implemented:
    void EnsureAbstract()   const override {}

    //=======================================================================//
    // Msg Processing:                                                       //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    //  "ProcessMsg": "H2WS::WSProtoEngine" Call-Back):                      //
    //-----------------------------------------------------------------------//
    // Returns "true" to continue receiving msgs, "false" to stop:
    bool ProcessWSMsg
    (
      char const*    a_msg_body,
      int            a_msg_len,
      bool           a_last_msg,
      utxx::time_val a_ts_recv,
      utxx::time_val a_ts_handl
    );

    //=======================================================================//
    // Session-Level Methods:                                                //
    //=======================================================================//
    //  "InitWSSession": Performs HTTP connection and WS upgrade:
    void InitWSSession() const;

    //  "InitWSLogOn" :  Send the Authentication data up:
    void InitWSLogOn()   const;

    // "MkWSPrefix": Internal Helper:
    int MkWSPrefix() const;

  public:
    //=======================================================================//
    // "FlushOrders*":                                                       //
    //=======================================================================//
    // Externally-visible method:
    utxx::time_val FlushOrders() override;

  private:
    //-----------------------------------------------------------------------//
    // "FlushOrdersImpl":                                                    //
    //-----------------------------------------------------------------------//
    // "EConnector_OrdMgmt" call-back:
    utxx::time_val FlushOrdersImpl(EConnector_WS_BitFinex_OMC* a_dummy) const;

  public:
    //=======================================================================//
    // "NewOrder":                                                           //
    //=======================================================================//
    AOS const* NewOrder
    (
      // The originating Stratergy:
      Strategy*         a_strategy,

      // Main order params:
      SecDefD const&    a_instr,
      FIX::OrderTypeT   a_ord_type,
      bool              a_is_buy,
      PriceT            a_px,
      bool              a_is_aggr,                // Aggressive Order?
      QtyAny            a_qty,                    // Full Qty

      // Temporal params of the triggering MktData event:
      utxx::time_val    a_ts_md_exch    = utxx::time_val(),
      utxx::time_val    a_ts_md_conn    = utxx::time_val(),
      utxx::time_val    a_ts_md_strat   = utxx::time_val(),

      // Optional order params:
      // NB: TimeInForceT::UNDEFINED must automatically resolve to a
      // Connector- specific default:
      bool              a_batch_send    = false,  // Ignored: Never Delayed!
      FIX::TimeInForceT a_time_in_force = FIX::TimeInForceT::Day,
      int               a_expire_date   = 0,      // Or YYYYMMDD
      QtyAny            a_qty_show      = QtyAny::PosInf(),
      QtyAny            a_qty_min       = QtyAny::Zero  (),
      // The following params are irrelevant -- Pegged Orders are NOT
      // supported in BitFinex:
      bool              a_peg_side      = true,
      double            a_peg_offset    = NaN<double>
    )
    override;

  private:
    //-----------------------------------------------------------------------//
    // "NewOrderImpl" -- Call-Back from "EConnector_OrdMgmt":                //
    //-----------------------------------------------------------------------//
    void NewOrderImpl
    (
      EConnector_WS_BitFinex_OMC* a_dummy,
      Req12*                      a_new_req,       // Non-NULL
      bool                        a_batch_send     // Always false
    );

  public:
    //=======================================================================//
    // "CancelOrder":                                                        //
    //=======================================================================//
    bool CancelOrder
    (
      AOS const*                  a_aos,           // May be NULL...
      utxx::time_val              a_ts_md_exch  = utxx::time_val(),
      utxx::time_val              a_ts_md_conn  = utxx::time_val(),
      utxx::time_val              a_ts_md_strat = utxx::time_val(),
      bool                        a_batch_send  = false
    )                                              // Ignored: Never delayed!
    override;

  private:
    //-----------------------------------------------------------------------//
    // "CancelOrderImpl" -- Call-Back from "EConnector_OrdMgmt":             //
    //-----------------------------------------------------------------------//
    void CancelOrderImpl
    (
      EConnector_WS_BitFinex_OMC* a_dummy,
      Req12*                      a_clx_req,       // Non-NULL
      Req12 const*                a_orig_req,      // Non-NULL
      bool                        a_batch_send     // Always false
    );

  public:
    //=======================================================================//
    // "ModifyOrder":                                                        //
    //=======================================================================//
    bool ModifyOrder
    (
      AOS const*                  a_aos,                   // Non-NULL
      PriceT                      a_new_px,
      bool                        a_is_aggr,               // Becoming Aggr?
      QtyAny                      a_new_qty,
      utxx::time_val              a_ts_md_exch   = utxx::time_val(),
      utxx::time_val              a_ts_md_conn   = utxx::time_val(),
      utxx::time_val              a_ts_md_strat  = utxx::time_val(),
      bool                        a_batch_send   = false,  // Ignored: False
      QtyAny                      a_new_qty_show = QtyAny::PosInf(),
      QtyAny                      a_new_qty_min  = QtyAny::Zero  ()
    )
    override;

  private:
    //-----------------------------------------------------------------------//
    // "ModifyOrderImpl" -- Call-Back from "EConnector_OrdMgmt":             //
    //-----------------------------------------------------------------------//
    void ModifyOrderImpl
    (
      EConnector_WS_BitFinex_OMC* a_dummy,
      Req12*                      a_mod_req0,              // NULL in BitFinex
      Req12*                      a_mod_req1,              // Non-NULL
      Req12 const*                a_orig_req,              // Non-NULL
      bool                        a_batch_send = false     // Ignored: False
    );

  public:
    //=======================================================================//
    // "CancelAllOrders": Only the externally-visible method:                //
    //=======================================================================//
    void CancelAllOrders
    (
      unsigned long               a_strat_hash48 = 0,             // All  Strats
      SecDefD const*              a_instr        = nullptr,       // All  Instrs
      FIX::SideT                  a_side = FIX::SideT::UNDEFINED, // Both Sides
      char const*                 a_segm_id = nullptr             // All  Segms
    )
    override;

  private:
    //=======================================================================//
    // Helpers / Utils:                                                      //
    //=======================================================================//
    // XXX: BitFinex does not provide Per-Req ClOrdIDs; ie ClOrdID is constant
    // per AOS, not Req12; for every Update of an existing Order, we must sub-
    // mit the same (UnChanged) ClOrdID and/or UnChanged OrdID, and same value
    // is then reported back to us by the BitFinex protocol.
    // However, this logic is inconsistent with "EConnector_OrdMgmt" common fu-
    // nctionality, so we need to emulate Per-Req ClOrdIDs somehow. Currently,
    // they are embedded into TinInForce flds:
    //
    char* EncodeReqIDInTIF(char* a_buff, OrderID a_clord_id);
  };
} // End namespace MAQUETTE
