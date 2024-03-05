// vim:ts=2:et
//===========================================================================//
//                "QuantSupport/BT/HistOMC_InstaFill.h":                     //
//        Order Management Connector that instantly fills all orders         //
//===========================================================================//
#pragma once

#include <utxx/error.hpp>
#include <utxx/time_val.hpp>

#include "Basis/QtyConvs.hpp"
#include "Connectors/EConnector_OrdMgmt.h"
#include "Connectors/EConnector_OrdMgmt_Impl.hpp"
#include "InfraStruct/Strategy.hpp"

namespace MAQUETTE {
template <typename QR>
class HistOMC_InstaFill final : public EConnector_OrdMgmt {
public:
  constexpr static char Name[] = "HistOMC_InstaFill";
  constexpr static QtyTypeT QT = QtyTypeT::QtyA;
  using QtyN = Qty<QT, QR>;

  HistOMC_InstaFill(EPollReactor *a_reactor, SecDefsMgr *a_sec_defs_mgr,
                    std::vector<SecDefS> const *a_expl_sec_defs,
                    RiskMgr *a_risk_mgr,
                    boost::property_tree::ptree const &a_params)
      : EConnector(a_params.get<std::string>("AccountKey"),
                   a_params.get<std::string>("Exchange"),
                   0,       // No extra ShM
                   a_reactor,
                   false,   // No BusyWait
                   a_sec_defs_mgr,
                   a_expl_sec_defs != nullptr ? *a_expl_sec_defs
                                              : std::vector<SecDefS>(),
                   nullptr, // No restrictions as we pass ExplSecDefs
                   a_params.get<bool>("ExplSettlDatesOnly", false),
                   false,   // Presumably no Tenors in SecIDs
                   a_risk_mgr, a_params, QT, std::is_floating_point_v<QR>),
        EConnector_OrdMgmt(true, a_params, 1, 10000,
                           EConnector_OrdMgmt::PipeLineModeT::Wait0, false,
                           false, false, false, false, false) {}

  ~HistOMC_InstaFill() override {}

  void Start() override {}
  void Stop() override {}
  bool IsActive() const override { return true; }

  void EnsureAbstract() const override {}

  double GetPosition(SecDefD const * /*a_sd*/) const override { return 0.0; }

  void Subscribe(Strategy * /*a_strat*/) override {}

  void UnSubscribe(Strategy const * /*a_strat*/) override {}

  AOS *NewOrder(
      // The originating Stratergy:
      Strategy*           a_strategy,

      // Main order params:
      SecDefD const&      a_instr,
      FIX::OrderTypeT     a_ord_type,
      bool                a_is_buy,
      PriceT              a_px,
      bool                a_is_aggr,
      QtyAny              a_qty,

      // Temporal params of the triggering MktData event:
      utxx::time_val      a_ts_md_exch    = utxx::time_val(),
      utxx::time_val      a_ts_md_conn    = utxx::time_val(),
      utxx::time_val      a_ts_md_strat   = utxx::time_val(),

      // Optional order params:
      // NB: TimeInForceT::UNDEFINED must automatically resolve to a Connector-
      // specific default:
      bool              /*a_batch_send*/  = false,    // Recommendation Only!
      FIX::TimeInForceT   a_time_in_force = FIX::TimeInForceT::UNDEFINED,
      int                 a_expire_date   = 0,        // Or YYYYMMDD
      QtyAny              a_qty_show      = QtyAny::PosInf(),
      QtyAny              a_qty_min       = QtyAny::Zero  (),
                                                      //   reduces the position
      // This applies to FIX::OrderTypeT::Pegged only:
      bool                a_peg_side      = true,     // True: this, False: opp
      double              a_peg_offset    = NaN<double>  // Signed
    ) override {
    utxx::time_val createTS = utxx::now_utc();

    // XXX: We do NOT check that the OMC "IsActive"; even if not, we will store
    // an Indication! But it must be Enabled:
    CHECK_ONLY(
        if (utxx::unlikely(!m_isEnabled)) throw utxx::badarg_error(
            "EConnector_OrdMgmt::NewOrderGen: ", m_name, ": OMC Not Enabled");

        // NB: In some rare cases, Strategy may not be required, but we enforce
        // it being non-NULL in any case:
        if (utxx::unlikely(a_strategy == nullptr)) throw utxx::badarg_error(
            "EConnector_OrdMgmt::NewOrderGen: ", m_name,
            ": Strategy must not be NULL");

        // For a Limit or Stop Order, Px must always be Finite;  for a Mkt
        // Order, it must be otherwise:
        if (utxx::unlikely(((a_ord_type == FIX::OrderTypeT::Limit ||
                             a_ord_type == FIX::OrderTypeT::Stop) &&
                            !IsFinite(a_px)) ||
                           (a_ord_type == FIX::OrderTypeT::Market &&
                            IsFinite(a_px))))
          throw utxx::
            badarg_error("EConnector_OrdMgmt::NewOrderGen: ", m_name,
                         ": Inconsistency: OrderType=", char(a_ord_type),
                         ", Px=", a_px);)
    //-----------------------------------------------------------------------//
    // Adjust the Px:                                                        //
    //-----------------------------------------------------------------------//
    // Round "a_px" to a multiple of PxStep (so the CallER is not obliged to do
    // it on its side):
    a_px = Round(a_px, a_instr.m_PxStep);

    //-----------------------------------------------------------------------//
    // Convert, Normalise and Verify the Qtys:                               //
    //-----------------------------------------------------------------------//
    // Convert the user-level Qtys into the OMC-specific ones.  XXX: This is
    // rather expensive! And searching for a better way, will use "a_px" for
    // QtyA<->QtyB conversions (this is a RARE and in general ERROR-PRONE case):
    //
    Qty<QT, QR> qty     = a_qty     .ConvQty<QT, QR>(a_instr, a_px);
    Qty<QT, QR> qtyShow = a_qty_show.ConvQty<QT, QR>(a_instr, a_px);
    Qty<QT, QR> qtyMin  = a_qty_min .ConvQty<QT, QR>(a_instr, a_px);

    // Normalise them:
    qtyShow.MinWith(qty);
    qtyMin.MinWith (qty);

    assert(qtyShow >= 0 && (IsPosInf(qty) || qtyShow <= qty));
    bool isIceberg  = (qtyShow < qty);

    // Now check them:
    CHECK_ONLY(if (utxx::unlikely(qty <= 0 || qtyShow < 0 || qtyMin < 0 ||
                                  qtyShow > qty || qtyMin > qty)) throw utxx::
                   badarg_error("EConnector_OrdMgmt::NewOrderGen: ", m_name,
                                ": Invalid Params: Qty=", QR(qty), ", QtyShow=",
                                QR(qtyShow), ", QtyMin=", QR(qtyMin));)
    // Risk Checks: An exception will be thrown if the check below fails:
    if (m_riskMgr != nullptr)
      // IsReal=true, OldPx=0 (not NaN):
      m_riskMgr->OnOrder<true>(this, a_instr, a_is_buy, QT, a_px, double(qty),
                               PriceT(0.0), 0.0, a_ts_md_strat);

    //-----------------------------------------------------------------------//
    // Create the new "AOS" and "Req12":                                     //
    //-----------------------------------------------------------------------//
    DEBUG_ONLY(OrderID currReqN = *m_ReqN;)
    DEBUG_ONLY(OrderID currOrdN = *m_OrdN;)

    // Create a new AOS; this increments *m_OrdN:
    AOS *aos = NewAOS(a_strategy, &a_instr, a_is_buy, a_ord_type, isIceberg,
                      a_time_in_force, a_expire_date);
    assert(aos != nullptr && *m_OrdN == currOrdN + 1);

    // Create the Req12 with already-recorded time stamps; AttachToAOS=true:
    Req12 *req = NewReq12<QT, QR, true>(
        aos, 0, Req12::KindT::New, a_px, a_is_aggr, qty, qtyShow,
        qtyMin, a_peg_side, a_peg_offset, a_ts_md_exch, a_ts_md_conn,
        a_ts_md_strat, createTS);
    assert(req != nullptr && req->m_id == currReqN &&
           req->m_status == Req12::StatusT::Indicated &&
           *m_ReqN == currReqN + 1);

    // make call back to confirm and fill the Req12, we can't do it here,
    // because the strategy needs the aos pointer in order to properly handle
    // the confirm and fill events

    // TimerHandler:
    IO::FDInfo::TimerHandler fill_timer_handle
    (
      [this, req, aos, qty, a_strategy, a_ts_md_exch, a_ts_md_strat]
      (int a_fd) -> void
      {
        // immediately confirm and fill the order
        req->m_status = Req12::StatusT::Confirmed;
        a_strategy->OnConfirm(*req);

        req->m_leavesQty    = QtyU();
        aos->m_cumFilledQty = QtyU(aos->GetCumFilledQty<QT,QR>() + qty);

        req->m_status = Req12::StatusT::Filled;
        req->m_probFilled = true;
        aos->m_isInactive = true;

        Trade const *tr =
          NewTrade(nullptr, req, ExchOrdID(), req->m_px, qty, QtyN::Zero(),
                   aos->m_isBuy ? FIX::SideT::Buy : FIX::SideT::Sell,
                   a_ts_md_exch, a_ts_md_strat);

      a_strategy->OnOurTrade(*tr);

      // remove timer
      m_reactor->Remove(a_fd);
      close(a_fd);
    });

    // ErrorHandler:
    IO::FDInfo::ErrHandler fill_timer_error_handle(
        [](int /*a_fd*/, int a_err_code, uint32_t /*a_events*/,
               char const *a_msg) {
          throw utxx::runtime_error(
              "HistOMC_InstaFill::ConfirmAndFillCallback: Error ", a_err_code,
              ": ", a_msg);
        });
    // Create the m_access_token_timer_FD and add it to the Reactor:
    m_reactor->AddTimer("ConfirmAndFill",
                        1,          // Initial wait in ms
                        1000000000, // Period in ms
                        fill_timer_handle, fill_timer_error_handle);

    return aos;
  }

  bool CancelOrder(AOS const* /*a_aos*/, // Non-NULL
                   utxx::time_val /*a_ts_md_exch*/ = utxx::time_val(),
                   utxx::time_val /*a_ts_md_conn*/ = utxx::time_val(),
                   utxx::time_val /*a_ts_md_strat*/ = utxx::time_val(),
                   bool /*a_batch_send*/ = false // Recommendation!
                   ) override {
    throw utxx::runtime_error("HistOMC_InstaFill::CancelOrder: Not supported");
  }

  bool ModifyOrder(AOS const* /*a_aos*/, // Non-NULL
                   PriceT /*a_new_px*/, bool /*a_is_aggr*/,
                   QtyAny /*a_new_qty*/,
                   utxx::time_val /*a_ts_md_exch*/ = utxx::time_val(),
                   utxx::time_val /*a_ts_md_conn*/ = utxx::time_val(),
                   utxx::time_val /*a_ts_md_strat*/ = utxx::time_val(),
                   bool   /*a_batch_send*/   = false, // Recommendation!
                   QtyAny /*a_new_qty_show*/ = QtyAny::PosInf(),
                   QtyAny /*a_new_qty_min*/  = QtyAny::Zero  ()) override {
    throw utxx::runtime_error("HistOMC_InstaFill::ModifyOrder: Not supported");
  }

  void CancelAllOrders(
      unsigned long /*a_strat_hash48 = 0*/,                  // All  Strats
      SecDefD const * /*a_instr        = nullptr*/,          // All  Instrs
      FIX::SideT /*a_side         = FIX::SideT::UNDEFINED*/, // Both Sides
      char const * /*a_segm_id      = nullptr*/              // All  Segms
      ) override {
    throw utxx::runtime_error(
        "HistOMC_InstaFill::CancelAllOrders: Not supported");
  }

  utxx::time_val FlushOrders() override { return utxx::now_utc(); }
};
} // namespace MAQUETTE
