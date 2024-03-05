// vim:ts=2:et
//===========================================================================//
//                    "QuantSupport/BT/OrdrMgmt.h":                          //
//===========================================================================//
#include "QuantSupport/BT/OrdMgmt.h"

#include "Basis/BaseTypes.hpp"
#include "Basis/PxsQtys.h"
#include "Basis/SecDefs.h"
#include "QuantSupport/BT/SecDefs.h"
#include "Connectors/EConnector_MktData.h"
#include "Connectors/EConnector_OrdMgmt_Impl.hpp"

#include <fstream>

namespace MAQUETTE
{
  Historical_OrdMgmt::Historical_OrdMgmt(
      EPollReactor*                      a_reactor,
      SecDefsMgr*                        a_sec_defs_mgr,
      std::vector<SecDefS> const*        a_expl_sec_defs,
      RiskMgr*                           a_risk_mgr,
      boost::property_tree::ptree const& a_params) :
    EConnector
    (
      a_params.get<std::string>("AccountKey"),
      a_params.get<std::string>("Exchange"),
      0,        // No extra ShM
      a_reactor,
      false,    // No BusyWait
      a_sec_defs_mgr,
      a_expl_sec_defs != nullptr ?
          *a_expl_sec_defs :
          History::SecDefs(a_params.get<std::string>("Exchange")),
      nullptr,  // No restrictions by Symbol here
      a_params.get<bool>("ExplSettlDatesOnly", false),
      false,    // Presumably no Tenors in SecIDs
      a_risk_mgr,
      a_params,
      QT,
      std::is_floating_point_v<QR>
    ),
    EConnector_OrdMgmt
    (
        true,  // IsEnabled!
        a_params,
        10,
        10000,
        EConnector_OrdMgmt::PipeLineModeT::Wait0,
        false,
        false,
        false,
        true,
        false,
        true),
    m_aos_f(m_aos),
    m_aos_t(m_aos + quotes_size),
    m_aos_c(m_aos_f),
    hdeals(),
    fee_maker(a_params.get<double>("fee_maker", 0.0)),
    fee_taker(a_params.get<double>("fee_taker", 0.0))
  {
    std::string deals = a_params.get<std::string>("Deals", "");
    if (!deals.empty())
    {
      hdeals = ::open(
          deals.c_str(),
          O_WRONLY | O_CREAT | O_APPEND | O_TRUNC,
          S_IWRITE | S_IREAD | S_IRGRP | S_IWGRP);
      if (hdeals < 0)
        throw utxx::runtime_error("open file error: ", deals);
    }
    name   = a_params.get<std::string>("Name", "");
    result = a_params.get<std::string>("Result");
  }

  void Historical_OrdMgmt::Start()
  {
  }

  void Historical_OrdMgmt::Stop()
  {
  }

  bool Historical_OrdMgmt::IsActive() const
  {
    return true;
  }

  void Historical_OrdMgmt::Subscribe(Strategy* a_strat)
  {
    if(m_strat)
      throw std::runtime_error("Historical_OrdMgmt::Subscribe() only one subscriber supported");
    m_strat = a_strat;
  }

  void Historical_OrdMgmt::UnSubscribe(Strategy const*)
  {
    m_strat = nullptr;
  }

  AOS const* Historical_OrdMgmt::NewOrder(
      Strategy* a_strat,
      SecDefD const& a_instr,
      FIX::OrderTypeT a_order_type,
      bool a_is_buy,
      PriceT a_px,
      bool,
      QtyAny a_qty,
      utxx::time_val,
      utxx::time_val,
      utxx::time_val,
      bool,
      FIX::TimeInForceT a_time_in_force,
      int a_expire_date,
      QtyAny,
      QtyAny,
      bool,
      double)
  {
      auto it = findStatistics(a_instr);

      auto f = [](AOS* c, AOS* t) -> AOS* {
          for (; c != t; ++c)
          {
              if (!c->m_instr || (c->m_isInactive))
              {
                  return c;
              }
          }
          return t;
      };

      m_aos_c = f(m_aos_c, m_aos_t);
      if (m_aos_c == m_aos_t)
          m_aos_c = f(m_aos_f, m_aos_t);

      if (utxx::unlikely(m_aos_c == m_aos_t))
          throw std::runtime_error(
              "Historical_OrdMgmt::NewOrder() active quotes_size reached");

      if (!m_aos_c->m_instr || m_aos_c->m_instr->m_SecID != a_instr.m_SecID)
      {
          new (m_aos_c)
              AOS(a_qty.m_qt,
                  IsFrac(a_qty),
                  a_strat,
                  ++lastReqId,
                  &a_instr,
                  this,
                  a_is_buy,
                  a_order_type,
                  false,  // NOT Iceberg!
                  a_time_in_force,
                  a_expire_date);
          const_cast<Req12*&>(m_aos_c->m_lastReq) = &m_req[m_aos_c - m_aos_f];
          const_cast<Req12*&>(m_aos_c->m_frstReq) = m_aos_c->m_lastReq;
          const_cast<AOS*&>  (m_aos_c->m_lastReq->m_aos) = m_aos_c;
      }
      else
      {
          const_cast<OrderID&>(m_aos_c->m_id)    = ++lastReqId;
          const_cast<bool&>   (m_aos_c->m_isBuy) = a_is_buy;
      }
      ++(it->new_orders);
      m_aos_c->m_isInactive = false;

      Req12* req = m_aos_c->m_lastReq;
      req->m_status = Req12::StatusT::New;
      const_cast<Req12::KindT&>(req->m_kind) = Req12::KindT::New;
      const_cast<PriceT&>      (req->m_px)   = a_px;
      const_cast<QtyU&>        (req->m_qty)  = a_qty.m_val;
      req->m_leavesQty = req->m_qty;
      const_cast<OrderID&>     (req->m_id)   = ++lastReqId;
      return m_aos_c++;
  }

  bool Historical_OrdMgmt::CancelOrder(
      AOS const* a_aos,
      utxx::time_val,
      utxx::time_val,
      utxx::time_val,
      bool)
  {
    auto it = findStatistics(*a_aos->m_instr);
    assert(it != m_statistics.end());

    const_cast<Req12::KindT&>(a_aos->m_lastReq->m_kind) = Req12::KindT::Cancel;
    ++(it->cancels);
    const_cast<OrderID&>     (a_aos->m_lastReq->m_id)   = ++lastReqId;
    return true;
  }

  bool Historical_OrdMgmt::ModifyOrder(
      AOS const*   a_aos,
      PriceT a_px,
      bool,
      QtyAny,
      utxx::time_val,
      utxx::time_val,
      utxx::time_val,
      bool,
      QtyAny,
      QtyAny)
  {
    Req12* req = a_aos->m_lastReq;
    if (utxx::unlikely(
            req->m_status != Req12::StatusT::Confirmed
            && req->m_status != Req12::StatusT::PartFilled))
      throw utxx::runtime_error(
          "inapropriate status for ModifyOrder(): ",
          int(req->m_status));

    auto it = findStatistics(*a_aos->m_instr);
    assert(it != m_statistics.end());

    const_cast<Req12::KindT&>(a_aos->m_lastReq->m_kind) = Req12::KindT::Modify;
    const_cast<PriceT&>      (req->m_px) = a_px;
    ++(it->modifies);
    const_cast<OrderID&>     (req->m_id) = ++lastReqId;
    return true;
  }

  void Historical_OrdMgmt::CancelAllOrders(
      unsigned long,
      const SecDefD*,
      FIX::SideT,
      const char*)
  {
  }

  Historical_OrdMgmt::~Historical_OrdMgmt()
  {
    if(hdeals)
      ::close(hdeals);

    std::ofstream of(result.c_str(), std::ios::app | std::ios::binary);
    for (auto stat: m_statistics)
    {
      if(stat.cur_pos != double())
      {
        double volume = Abs(stat.cur_pos) * m_lastPrices[stat.secID];
        stat.volume += volume;
        stat.fee_maker += volume * fee_maker;
        if(stat.cur_pos < double())
        {
          stat.delta -= volume;
        }
        else
        {
          stat.delta += volume;
        }
      }

      char buf[1024];
      char* c = buf;
      c = stpncpy(c, stat.symbol.c_str(), stat.symbol.size());
      *(c++) = ',';
      c = stpncpy(c, name.c_str(), name.size());
      c = stpcpy(c, ", delta:");
      c += utxx::ftoa_left(stat.delta, c, 16, 8);
      c = stpcpy(c, ", fee_maker:");
      c += utxx::ftoa_left(stat.fee_maker, c, 16, 8);
      c = stpcpy(c, ", fee_taker:");
      c += utxx::ftoa_left(stat.fee_taker, c, 16, 8);
      c = stpcpy(c, ", volume:");
      c += utxx::ftoa_left(stat.volume, c, 16, 8);
      c = stpcpy(c, ", min_pos:");
      c += utxx::ftoa_left(stat.min_pos, c, 16, 8);
      c = stpcpy(c, ", max_pos:");
      c += utxx::ftoa_left(stat.max_pos, c, 16, 8);
      c = stpcpy(c, ", new:");
      c = utxx::itoa_left<uint32_t, 8>(c, stat.new_orders);
      c = stpcpy(c, ", cancels:");
      c = utxx::itoa_left<uint32_t, 8>(c, stat.cancels);
      c = stpcpy(c, ", modifies:");
      c = utxx::itoa_left<uint32_t, 8>(c, stat.modifies);
      *(c++) = '\n';
      of.write(buf, c - buf);
    }
  }

  void Historical_OrdMgmt::SetTrade(
      const SecDefD& a_instr,
      utxx::time_val time,
      bool           is_buy,
      PriceT         price,
      QtyUD          count)
  {
    auto it = findStatistics(a_instr);
    assert(it != m_statistics.end());

    for(uint32_t i = 0; i != quotes_size; ++i)
    {
      AOS& a = m_aos[i];
      if (!a.m_instr || a.m_isInactive || a_instr.m_SecID != a.m_instr->m_SecID)
        continue;

      Req12& r = m_req[i];

      if (r.m_status == Req12::StatusT::New)
      {
        r.m_status     = Req12::StatusT::Confirmed;
        const_cast<utxx::time_val&>(r.m_ts_md_conn) = time;
        m_strat->OnConfirm(r);
      }

      if (r.m_kind == Req12::KindT::Cancel)
      {
        a.m_isInactive = true;
        r.m_status     = Req12::StatusT::Cancelled;
        m_strat->OnCancel(a, time, time);
        continue;
      }

      bool fill = false;
      if (a.m_isBuy && !is_buy && r.m_px > price)
        fill = true;
      if (!a.m_isBuy && is_buy && r.m_px < price)
        fill = true;

      if (fill)
      {
        QtyUD size = QtyUD(r.GetLeavesQty<QT,QR>());
        if (size > count)
        {
          r.m_leavesQty = QtyU(double(size) - double(count));
          size          = count;
          r.m_status    = Req12::StatusT::PartFilled;
        }
        else
        {
          r.m_leavesQty  = QtyU();
          r.m_status     = Req12::StatusT::Filled;
          a.m_isInactive = true;
        }

        if (a.m_isBuy)
        {
          it->cur_pos += double(size);
        }
        else
        {
          it->cur_pos -= double(size);
        }

        it->min_pos = std::min(it->min_pos, it->cur_pos);
        it->max_pos = std::max(it->max_pos, it->cur_pos);

        double volume = double(size) * double(r.m_px);
        it->volume += volume;
        it->fee_maker += volume * fee_maker;
        if (a.m_isBuy)
        {
          it->delta -= volume;
        }
        else
        {
          it->delta += volume;
        }

        Trade const* tr = NewTrade
        (
          nullptr,  &r,     ExchOrdID(), r.m_px, size, QtyUD(volume * fee_maker),
          a.m_isBuy ? FIX::SideT::Buy : FIX::SideT::Sell, time, time
        );

        m_strat->OnOurTrade(*tr);

        if (hdeals)
        {
          char  buf[256];
          char* c = buf;
          c       = utxx::itoa_left<long, 19>(c, time.nanoseconds());
          *(c++)  = ',';
          c       = stpncpy(c, it->symbol.c_str(), it->symbol.size());
          *(c++)  = ',';
          c       = utxx::itoa_left<int, 8>(c, a.m_isBuy ? 1 : -1);
          *(c++)  = ',';
          c += utxx::ftoa_left(double(tr->m_px), c, 16, 8);
          *(c++) = ',';
          c += utxx::ftoa_left(double(size), c, 16, 8);
          *(c++) = '\n';
          if (::write(hdeals, buf, uint32_t(c - buf)) != c - buf)
            throw std::runtime_error("writing deals.csv error");
        }
      }
    }
  }

  void Historical_OrdMgmt::SetBook(const SecDefD& a_instr, utxx::time_val time,
      uint16_t asks_size, History::quote* asks_quotes,
      uint16_t bids_size, History::quote* bids_quotes)
  {
    if(!asks_size || !bids_size)
      return;
    SetTrade(a_instr, time, false, asks_quotes->price, asks_quotes->count);
    SetTrade(a_instr, time, true, bids_quotes->price, bids_quotes->count);
    m_lastPrices[a_instr.m_SecID] =
        (double(asks_quotes->price) + double(bids_quotes->price)) / 2;
    last_time = time;
  }

  std::vector<Historical_OrdMgmt::Statistics>::iterator Historical_OrdMgmt::
      findStatistics(const SecDefD &a_instr, Strategy * a_strat)
  {
    auto res = std::find_if(
        m_statistics.begin(),
        m_statistics.end(),
        [a_instr](auto const& stat) {
          return stat.secID == a_instr.m_SecID;
        });

      if (res == m_statistics.end())
      {
        Statistics statistics;
        statistics.secID = a_instr.m_SecID;
        statistics.symbol = std::string(a_instr.m_Symbol.data()) + "@"
                            + a_instr.m_Exchange.data();
        if (a_strat != nullptr)
          SubscribeMktData(a_strat, this, a_instr, true);
        m_statistics.push_back(statistics);
        res = m_statistics.end() - 1;
      }
      return res;
  }

  utxx::time_val Historical_OrdMgmt::FlushOrders()
  {
    return last_time;
  }
}

