// vim:ts=2:et:sw=2
//===========================================================================//
//                            "StratEnv_Analyt.cpp":                         //
//                    Analytics and Risk Management Services                 //
//===========================================================================//
#include "Common/Maths.h"
#include "Common/TradingCalendar.h"
#include "StrategyAdaptor/StratEnv.h"
#include "StrategyAdaptor/InitMargins.h"
#include <utxx/compiler_hints.hpp>
#include <Infrastructure/Logger.h>

namespace
{
  using namespace Arbalete;
  using namespace MAQUETTE;
  using namespace std;

  //=========================================================================//
  // "GetBSMGreeks":                                                         //
  //=========================================================================//
  // Simple Black-Scholes-Merto Greeks:
  //
  void GetBSMGreeks
  (
    double  CP,    double  S0,    double  K,
    double  sigma, double  tau,   double  r,
    double* price, double* delta, double* gamma, double* vega
  )
  {
    assert(Abs(CP) == 1.0 && S0 > 0.0 && K > 0.0 && sigma > 0.0 && tau > 0.0 &&
           price  != NULL && delta != NULL && gamma != NULL && vega != NULL);

    double rt    = r * tau;
    double x     = Log (S0 / K) + rt;

    double sqTau = SqRt(tau);
    double st    = sigma * sqTau;
    x /= st;

    double d1    = x  + 0.5 * st;
    double d2    = d1 - st;

    double N1    = InvSqRt2Pi<double>() * Exp(-0.5 * d1 * d1);

    *price = CP * (NCDF(CP * d1) - K * Exp(-rt) * NCDF(CP * d2));
    *delta = (CP > 0.0) ? NCDF(d1) : (NCDF(d1) - 1.0);
    *gamma = N1 / (S0 * st);
    *vega  = N1 *  S0 * sqTau;
  }
}

namespace MAQUETTE
{
  using namespace std;

  //=========================================================================//
  // Accessors by Symbol:                                                    //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "GetQSymInfo":                                                          //
  //-------------------------------------------------------------------------//
  StratEnv::QSymInfo const& StratEnv::GetQSymInfo(QSym const& qsym) const
  {
    CheckQSymAOS(qsym, NULL, "GetQSymInfo");

    QSymInfoMap::const_iterator cit = m_QSymInfoMap.find(qsym);

    if (utxx::unlikely(cit == m_QSymInfoMap.end()))
      throw runtime_error
            ("StratEnv::GetQSymInfo: Invalid QSym=" + qsym.ToString());
    return cit->second;
  }

  //-------------------------------------------------------------------------//
  // "GetPosition":                                                          //
  //-------------------------------------------------------------------------//
  long StratEnv::GetPosition(QSym const& qsym) const
  {
    CheckQSymAOS(qsym, NULL, "GetPosition");
    assert(m_positions != NULL);
    PositionsMap::const_iterator cit = m_positions->find(qsym);

    if (utxx::unlikely(cit == m_positions->end()))
      throw runtime_error
            ("StratEnv::GetPosition: QSym="+ qsym.ToString()
            +" is not subscribed");
    else
      return (cit->second).m_pos;
  }

  //-------------------------------------------------------------------------//
  // "GetPosAcqRate":                                                        //
  //-------------------------------------------------------------------------//
  // NB: Returns the ABSOLUTE rate (ie >= 0 for both long and short positions):
  //
  int StratEnv::GetPosAcqRate(QSym const& qsym) const
  {
    CheckQSymAOS(qsym, NULL, "GetPosAcqRate");
    assert(m_positions != NULL);
    PositionsMap::const_iterator cit = m_positions->find(qsym);

    if (utxx::unlikely(cit == m_positions->end()))
      throw runtime_error
            ("StratEnv::GetPosAcqRate: QSym="+ qsym.ToString()
            +" is not subscribed");
    else
      return (cit->second).m_posRateThrottler.running_sum();
  }

  //-------------------------------------------------------------------------//
  // "SetRiskLimits" (per Symbol):                                           //
  //-------------------------------------------------------------------------//
  void StratEnv::SetRiskLimits
    (QSym const& a_qsym, RiskLimitsPerSymbol const& a_rl)
  {
    CheckQSymAOS(a_qsym, NULL, "SetRiskLimits(Symbol)");
    assert(m_positions != NULL);
    PositionsMap::iterator it = m_positions->find(a_qsym);

    if (utxx::unlikely(it == m_positions->end()))
      throw runtime_error
            ("StratEnv::SetRiskLimits(Symbol): QSym="+ a_qsym.ToString()
            +" is not subscribed");

    (it->second).m_RL = a_rl;
  }

  //-------------------------------------------------------------------------//
  // "SetRiskLimits" (per Ccy):                                              //
  //-------------------------------------------------------------------------//
  void StratEnv::SetRiskLimits
    (Events::Ccy const& a_ccy, RiskLimitsPerCcy const& a_rl)
  {
    assert(m_cashInfo != NULL);
    CashMap::iterator it = m_cashInfo->find(a_ccy);

    if (utxx::unlikely(it == m_cashInfo->end()))
      throw runtime_error
            ("StratEnv::SetRiskLimits(Ccy): Ccy="+ Events::ToString(a_ccy)
            +" is not found");

    (it->second).m_RL = a_rl;
  }

  //-------------------------------------------------------------------------//
  // "GetAvgPosPx":                                                          //
  //-------------------------------------------------------------------------//
  double StratEnv::GetAvgPosPx(QSym const& qsym, bool with_trans_costs) const
  {
    CheckQSymAOS(qsym, NULL, "GetAvgPosPx");

    assert(m_positions != NULL);
    PositionsMap::const_iterator cit = m_positions->find(qsym);

    if (utxx::unlikely(cit == m_positions->end()))
      throw runtime_error
            ("StratEnv::GetAvgPosPx: QSym="+ qsym.ToString()
            +" is not subscribed");

    PositionInfo const& pi = cit->second;
    return pi.GetAvgPosPx(with_trans_costs);
  }

  //-------------------------------------------------------------------------//
  // "GetOurTrades":                                                         //
  //-------------------------------------------------------------------------//
  StratEnv::OurTradesVector const& StratEnv::GetOurTrades(QSym const& qsym)
  const
  {
    CheckQSymAOS(qsym, NULL, "GetOurTrades");
    assert(m_ourTrades != NULL);
    OurTradesMap::const_iterator cit = m_ourTrades->find(qsym);

    if (utxx::unlikely(cit == m_ourTrades->end()))
      throw runtime_error
            ("StratEnv::GetOurTrades: QSym="+ qsym.ToString()
            +" is not subscribed");
    else
      return cit->second;
  }

  //-------------------------------------------------------------------------//
  // "GetAOSes":                                                             //
  //-------------------------------------------------------------------------//
  StratEnv::AOSPtrVec const& StratEnv::GetAOSes(QSym const& qsym) const
  {
    CheckQSymAOS(qsym, NULL, "GetAOSes");
    assert(m_aoss != NULL);
    AOSMapQPV::const_iterator cit = m_aoss->find(qsym);

    if (utxx::unlikely(cit == m_aoss->end()))
      throw runtime_error
            ("StratEnv::GetAOSes: QSym="+ qsym.ToString() +
             " is not subscribed");
    else
      return cit->second;
  }

  //-------------------------------------------------------------------------//
  // "GetBestBid":                                                           //
  //-------------------------------------------------------------------------//
  OrderBook::Entry const& StratEnv::GetBestBid(QSym const& qsym) const
  {
    CheckQSymAOS(qsym, NULL, "GetBestBid");
    assert(m_orderBooks != NULL);
    OrderBooksMap::const_iterator cit = m_orderBooks->find(qsym);

    if (utxx::unlikely(cit == m_orderBooks->end()))
      throw runtime_error
            ("StratEnv::GetBestBid: QSym=" + qsym.ToString()
            +": No Order Book received yet (or wrong QSym)");

    if (utxx::unlikely(cit->second->m_currBids == 0))
      throw runtime_error
            ("StratEnv::GetBestBid: QSym=" + qsym.ToString() +": No Bids");
    return cit->second->m_bids[0];
  }

  //-------------------------------------------------------------------------//
  // "GetBestAsk":                                                           //
  //-------------------------------------------------------------------------//
  OrderBook::Entry const& StratEnv::GetBestAsk(QSym const& qsym) const
  {
    CheckQSymAOS(qsym, NULL, "GetBestAsk");
    assert(m_orderBooks != NULL);
    OrderBooksMap::const_iterator cit = m_orderBooks->find(qsym);

    if (utxx::unlikely(cit == m_orderBooks->end()))
      throw runtime_error
            ("StratEnv::GetBestAsk: QSym=" + qsym.ToString()
            +": No Order Book received yet (or wrong QSym)");

    if (utxx::unlikely(cit->second->m_currAsks == 0))
      throw runtime_error
            ("StratEnv::GetBestAsk: QSym=" + qsym.ToString() + ": No Asks");
    return cit->second->m_asks[0];
  }

  //-------------------------------------------------------------------------//
  // "GetSecDef":                                                            //
  //-------------------------------------------------------------------------//
  SecDef const& StratEnv::GetSecDef(QSym const& qsym) const
  {
    CheckQSymAOS(qsym, NULL, "GetSecDef");
    assert(m_secDefs != NULL);
    SecDefsMap::const_iterator cit = m_secDefs->find(qsym);

    if (utxx::unlikely(cit == m_secDefs->end()))
      throw runtime_error
            ("StratEnv::GetSecDef: QSym=" + qsym.ToString()
            +": No SecDefs received yet");

    SecDef const* def = cit->second;
    assert(def != NULL);
    return *def;
  }

  //-------------------------------------------------------------------------//
  // "GetOrderBook":                                                         //
  //-------------------------------------------------------------------------//
  OrderBookSnapShot const& StratEnv::GetOrderBook(QSym const& qsym) const
  {
    CheckQSymAOS(qsym, NULL, "GetOrderBook");
    assert(m_orderBooks != NULL);
    OrderBooksMap::const_iterator cit = m_orderBooks->find(qsym);

    if (utxx::unlikely(cit == m_orderBooks->end()))
      throw runtime_error
            ("StratEnv::GetOrderBook: QSym="+ qsym.ToString()
            +": No OrderBook SnapShots received yet (or wrong QSym)");

    OrderBookSnapShot const* ob = cit->second;
    assert(ob != NULL);
    return *ob;
  }

  //-------------------------------------------------------------------------//
  // "GetCashInfo":                                                          //
  //-------------------------------------------------------------------------//
  StratEnv::CashInfo const& StratEnv::GetCashInfo(Events::Ccy const& ccy) const
  {
    assert(m_cashInfo != NULL);
    CashMap::const_iterator cit = m_cashInfo->find(ccy);

    if (utxx::unlikely(cit == m_cashInfo->end()))
      throw runtime_error
            ("StratEnv::GetCashInfo: Invalid Ccy=" + Events::ToString(ccy));

    return cit->second;
  }

  //-------------------------------------------------------------------------//
  // "GetTotalCashInfo":                                                     //
  //-------------------------------------------------------------------------//
  pair<Events::Ccy const, StratEnv::CashInfo> const&
    StratEnv::GetTotalCashInfo() const
  {
    assert(m_totalInfo != NULL);

    if (utxx::unlikely(m_totalInfo->empty()))
      throw runtime_error("StratEnv::GetTotalCashInfo: Total is not set up");

    // Otherwise:
    assert  (m_totalInfo->size() == 1);
    return *(m_totalInfo->begin());
  }

  //=========================================================================//
  // Cash and NLV Management:                                                //
  //=========================================================================//
  //=========================================================================//
  // "GetCcy":                                                               //
  //=========================================================================//
  // XXX: Alternatively, the Ccy can be obtained from "QSymInfo":
  //
  Events::Ccy StratEnv::GetCcy(QSym const& qsym) const
  {
    CheckQSymAOS(qsym, NULL, "GetCcy");

    assert(m_secDefs != NULL);
    SecDefsMap::const_iterator cit = m_secDefs->find(qsym);

    if (utxx::unlikely(cit == m_secDefs->end()))
      throw invalid_argument
            ("StratEnv::GetCcy: Invalid QSym=" + qsym.ToString());

    // NB: Once again, use "SettlCcy" rather than just "Ccy":
    Events::Ccy ccy = cit->second->m_SettlCcy;
    return  ccy;
  }

  //=========================================================================//
  // "GetRealizedPnL":                                                       //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // (1) For a given "QSym":                                                 //
  //-------------------------------------------------------------------------//
  double StratEnv::GetRealizedPnL(QSym const& qsym) const
  {
    assert(m_positions != NULL);

    PositionsMap::const_iterator cit = m_positions->find(qsym);

    if (utxx::unlikely(cit == m_positions->end()))
      throw invalid_argument
            ("StratEnv::GetRealizedPnL: Invalid QSym: " + qsym.ToString());
    assert(cit->first == cit->second.m_qsym);

    return (cit->second).m_realizedPnL;
  }

  //-------------------------------------------------------------------------//
  // (2) By Ccy, for all suitable "QSym"s:                                   //
  //-------------------------------------------------------------------------//
  double StratEnv::GetRealizedPnL(Events::Ccy const& ccy, bool db_record)
  {
    assert (m_positions != NULL);

    // Get the Total Realized PnLs for this "ccy". FIXME: How can we decide if
    // these values did not change since the previous invocation:
    double rpnl = 0.0;
    for (PositionsMap::const_iterator  cit = m_positions->begin();
         cit != m_positions->end();  ++cit)
    {
      QSym const& qsym = cit->first;
      assert(qsym == cit->second.m_qsym);

      if (GetCcy(qsym) == ccy)
        rpnl += cit->second.m_realizedPnL;
    }

    // Store the result in the corresp "CashInfo":
    assert(m_cashInfo != NULL);
    CashInfo& ci = (*m_cashInfo)[ccy];
    ci.m_realizedPnL = rpnl;

    // If there is only 1 Ccy in this Strategy, also update the Total Realized
    // PNLs:
    assert(m_Ccys  != NULL);
    if (m_Ccys->size() == 1)
      SetTotalRealizedPnL(ccy, rpnl, db_record && !m_emulated);

    return rpnl;
  }

  //=========================================================================//
  // "GetVarMargin":                                                         //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // (1) For a given "QSym" (also provides ArrivalPx):                       //
  //-------------------------------------------------------------------------//
  double StratEnv::GetVarMargin(QSym const& qsym) const
  {
    assert(m_positions != NULL);
    PositionsMap::const_iterator cit = m_positions->find(qsym);

    if (utxx::unlikely(cit == m_positions->end()))
      throw invalid_argument
            ("StratEnv::GetVarMargin: Invalid QSym: "+ qsym.ToString());
    assert(cit->first == cit->second.m_qsym);

    PositionInfo const& pi = cit->second;
    return pi.m_VM;
  }

  //-------------------------------------------------------------------------//
  // (2) By Ccy, for all suitable "QSym"s:                                   //
  //-------------------------------------------------------------------------//
  double StratEnv::GetVarMargin(Events::Ccy const& ccy, bool db_record)
  {
    assert (m_positions != NULL);

    // Get the total VM for this "ccy". FIXME: how can we decide if this value
    // did not change since the previous invocation?
    double vm = 0.0;
    for (PositionsMap::const_iterator  cit = m_positions->begin();
         cit != m_positions->end();  ++cit)
    {
      QSym const& qsym = cit->first;
      assert(qsym == cit->second.m_qsym);

      if (GetCcy(qsym) == ccy)
        vm += cit->second.m_VM;
    }

    // Store the result in the corresp "CashInfo":
    assert(m_cashInfo != NULL);
    CashInfo& ci = (*m_cashInfo)[ccy];
    ci.m_VM  = vm;

    // If there is only 1 Ccy in this Strategy, also update the TotalVM:
    assert(m_Ccys  != NULL);
    if (m_Ccys->size() == 1)
      SetTotalVarMargin(ccy, vm, db_record && !m_emulated);

    return vm;
  }

  //=========================================================================//
  // "GetNLV":                                                               //
  //=========================================================================//
  double StratEnv::GetNLV(Events::Ccy const& ccy, bool db_record)
  {
    // First of all, there is a RealizedPnL component for this Ccy:
    double nlv = GetRealizedPnL(ccy, db_record);

    // Then iterate through the Portfolio:
    assert(m_positions != NULL);
    for (PositionsMap::const_iterator cit = m_positions->begin();
         cit != m_positions->end();   cit++)
    {
      QSym const& qsym =  cit->first;
      assert(qsym == cit->second.m_qsym);
      long pos         = (cit->second).m_pos; // Can be <=> 0

      // If the curr pos is 0, nothing to do here:
      if (pos == 0)
        continue;

      // Is this portfolio component denominated in the given "ccy"?
      if (GetCcy(qsym) != ccy)
        continue;

      // Will need the PointPx, so get a "SecDef":
      SecDef const& def = GetSecDef(qsym);

      // Otherwise:
      // Get the ptr to the corresp OrderBook (in ShMem):
      assert(m_orderBooks != NULL);
      OrderBooksMap::const_iterator cobit = m_orderBooks->find(qsym);
      if (utxx::unlikely(cobit == m_orderBooks->end()))
        // This should not normally happen (Position for this Symbol is present
        // but the Order Book is not):
        throw runtime_error
              ("StratEnv::NLV: No OrderBook for QSym="+ qsym.ToString());

      OrderBookSnapShot const* ob = cobit->second;
      assert(ob != NULL && ob->m_symbol == qsym.m_symKey);

      // Get the cash increment by traversing the OrderBook  and Buying/Selling
      // the qty of "pos". For NLV computation, we assume that WE bear the cost
      // of crossing the Bid-Ask spread while liquidating the position:
      //
      OrderBook::Entry const* levels =
        (pos < 0)
        ? ob->m_asks   // Need to buy,  ie look for Ask prices
        : ob->m_bids;  // Need to sell, ie look for Bid prices

      int depth = (pos < 0) ? ob->m_currAsks : ob->m_currBids;

      // Qty to be sold / bought:
      long qty  = abs(pos);

      // Go through the Order Book Levels:
      double value = 0.0;
      for (int  i = 0; qty > 0 && i < depth; ++i)
      {
        // Qty we get from this level. Each level's entry is (Price, Qty):
        // XXX: Don't forget "PointPx" -- especially  important for Index
        // Futures!
        long     lqty = min<long>(levels[i].m_qty, qty);
        qty   -= lqty;
        value += double(lqty) * levels[i].m_price * def.m_PointPx;
        assert(qty >= 0);
      }

      // Is there any remaining quantity unallocated? If yes, cannot value the
      // portfolio because it cannot be fully liquidated under the present mkt
      // conditions:
      if (utxx::unlikely(qty > 0))
        throw runtime_error
              ("StratEnv::NLV: QSym="+ qsym.ToString() +
               " cannot be fully liquidated: RemQty=" + to_string(qty));

      // Otherwise:
      if (pos < 0)
        nlv -= value;   // We have closed a short position
      else
        nlv += value;   // We have closed a long  position
    }

    // Save the computed "nlv" in ShMem. The corresp entry must already exist:
    assert(m_cashInfo != NULL && m_cashInfo->find(ccy) != m_cashInfo->end());

    CashInfo& ci = (*m_cashInfo)[ccy];
    ci.m_NLV = nlv;

    // If there is only 1 Ccy in this Strategy, also update the TotalNLV:
    assert(m_Ccys  != NULL);
    if (m_Ccys->size() == 1)
      SetTotalNLV(ccy, nlv, db_record && !m_emulated);

    return   nlv;
  }

  //=========================================================================//
  // "SetTotalVal":                                                          //
  //=========================================================================//
  // NB: Total{RealizedPnL,NLV,IM,VM,VaR} (in a single reference Ccy) can  on-
  // ly be computed by the Strategy itself; there is no universal way of doing
  // so, UNLESS there is actually only 1 Ccy (then the total is updated autom-
  // atically):
  //
  void StratEnv::SetTotalVal
  (
    CashInfoCompT       type,
    Events::Ccy  const& ccy,
    double              val,
    bool                db_record
  )
  {
    assert(m_totalInfo != NULL && m_Ccys != NULL);
    // But it may be a placeholder with empty Ccy...

    //-----------------------------------------------------------------------//
    // Create a new "CashInfo" or use an existing one:                       //
    //-----------------------------------------------------------------------//
    CashMap::iterator it = m_totalInfo->begin();

    CashInfo  cashInfo;     // Iitially, zeroed out
    CashInfo* targ = NULL;

    if (it->first != ccy)
    {
      // "ccy" is to be installed. Check that it is valid in the first place:
      if (utxx::unlikely
         (find(m_Ccys->begin(), m_Ccys->end(), ccy) == m_Ccys->end()))
        throw invalid_argument
              ("StratEnv::SetTotalVal: Invalid Ccy: "+ Events::ToString(ccy));

      // Then, the only valid case is when the curr Key is empty:
      if (utxx::unlikely(!Events::IsEmpty(it->first)))
        throw invalid_argument
              ("StratEnv::SetTotalVal: InConsistent Ccy: "+
               Events::ToString(ccy) + "; previous: "     +
               Events::ToString(it->first));

      // So it's an empty placeholder -- remove it:
      m_totalInfo->erase(it);
      assert(m_totalInfo->empty());

      // Point "targ" to the new "CashInfo" -- nothing exists yet:
      targ = &cashInfo;
    }
    else
      // Same Ccy indeed: Point "targ" to the existing record:
      targ = &(m_totalInfo->begin()->second);

    //-----------------------------------------------------------------------//
    // Update the value pointed to by "targ":                                //
    //-----------------------------------------------------------------------//
    switch (type)
    {
    case CashInfoCompT::RealizedPnL:
      targ->m_realizedPnL = val;
      break;

    case CashInfoCompT::NLV:
      targ->m_NLV = val;
      break;

    case CashInfoCompT::InitMargin:
      targ->m_IM  = val;
      break;

    case CashInfoCompT::VarMargin:
      targ->m_VM  = val;
      break;

    case CashInfoCompT::VaR:
      targ->m_VaR = val;
      break;

    default: ;
    }

    // If "cashInfo" is a new one, update the Map:
    if (m_totalInfo->empty())
    {
      assert(targ == &cashInfo);
      (*m_totalInfo)[ccy] = *targ;
    }
  }

  //=========================================================================//
  // "SetTotal{Cash,RealizedPnL,NLV,IM,VM,VaR}":                             //
  //=========================================================================//
  // NB: These are user API functions, they set "Current" (as opposed to "Peri-
  // od") values:
  //
  void StratEnv::SetTotalRealizedPnL
       (Events::Ccy const& ccy, double rpnl,   bool db_record)
    { SetTotalVal(CashInfoCompT::RealizedPnL, ccy, rpnl, db_record); }

  void StratEnv::SetTotalNLV
       (Events::Ccy const& ccy, double nlv,    bool db_record)
    { SetTotalVal(CashInfoCompT::NLV, ccy, nlv, db_record); }

  void StratEnv::SetTotalInitMargin
       (Events::Ccy const& ccy, double margin, bool db_record)
    { SetTotalVal(CashInfoCompT::InitMargin, ccy, margin, db_record); }

  void StratEnv::SetTotalVarMargin
       (Events::Ccy const& ccy, double margin, bool db_record)
    { SetTotalVal(CashInfoCompT::VarMargin,  ccy, margin, db_record); }

  void StratEnv::SetTotalVaR
       (Events::Ccy const& ccy, double VaR,    bool db_record)
    { SetTotalVal(CashInfoCompT::VaR, ccy, VaR, db_record); }

  //=========================================================================//
  // "GetTotalVal":                                                          //
  //=========================================================================//
  bool StratEnv::GetTotalVal
  (
    CashInfoCompT type,
    Events::Ccy*  ccy,
    double*       val
  )
  const
  {
    assert(m_totalInfo != NULL && ccy != NULL && val != NULL);

    if (m_totalInfo->empty())
    {
      *ccy  = Events::EmptyCcy(); // Empty value, all-0s
      *val  = 0.0;
      return  false;
    }

    // Otherwise: Generic Case:
    //
    assert(m_totalInfo->size() == 1);

    StratEnv::CashMap::const_iterator cit = m_totalInfo->begin();
    *ccy   = cit->first;
    CashInfo const& cashInfo = cit->second;

    switch (type)
    {
    case CashInfoCompT::RealizedPnL:
      *val = cashInfo.m_realizedPnL;
      break;

    case CashInfoCompT::NLV:
      *val = cashInfo.m_NLV;
      break;

    case CashInfoCompT::InitMargin:
      *val = cashInfo.m_IM;
      break;

    case CashInfoCompT::VarMargin:
      *val = cashInfo.m_VM;
      break;

    case CashInfoCompT::VaR:
      *val = cashInfo.m_VaR;
      break;

    default:
      *val = 0.0; assert(false);
    }
    return true;
  }

  //=========================================================================//
  // "GetTotal{RealizedPnL,NLV,IM,VM,VaR}":                                  //
  //=========================================================================//
  bool StratEnv::GetTotalNLV        (Events::Ccy*    ccy, double* nlv)  const
    { return GetTotalVal(CashInfoCompT::NLV,         ccy, nlv);  }

  bool StratEnv::GetTotalInitMargin (Events::Ccy*    ccy, double* im)   const
    { return GetTotalVal(CashInfoCompT::InitMargin,  ccy, im);   }

  bool StratEnv::GetTotalVarMargin  (Events::Ccy*    ccy, double* vm)   const
    { return GetTotalVal(CashInfoCompT::VarMargin,   ccy, vm);   }

  bool StratEnv::GetTotalVaR        (Events::Ccy*    ccy, double* var)  const
    { return GetTotalVal(CashInfoCompT::VaR,         ccy, var);  }

  bool StratEnv::GetTotalRealizedPnL(Events::Ccy*    ccy, double* rpnl) const
    { return GetTotalVal(CashInfoCompT::RealizedPnL, ccy, rpnl); }


  //=========================================================================//
  // "GetInitMargin":                                                        //
  //=========================================================================//
  double StratEnv::GetInitMargin(Events::Ccy const& ccy, bool db_record)
  {
    //-----------------------------------------------------------------------//
    // Classify Portfolio Instruments By OE Connector:                      //
    //-----------------------------------------------------------------------//
    // { ConnectorName => { Symbol, Position} }:
    //
    // If there are no OEs, then by definition, there is no Initial Margin:
    if (m_OEs->empty())
      return 0.0;

    // Otherwise: Generic Case:
    map<string, map<QSym, long>> ByConn;

    assert(m_positions != NULL);
    for (PositionsMap::const_iterator cit = m_positions->begin();
         cit != m_positions->end(); ++cit)
    {
      QSym const& qsym =  cit->first;
      long        pos  = (cit->second).m_pos;

      // If the  "pos" is 0, we currently skip it altogether.
      // Also skip it if it is denominated in another Ccy:
      if (pos == 0 || GetCcy(qsym) != ccy)
        continue;

      // Get the Symbol and its OE:
      int omcN         = qsym.m_omcN;
      assert(0 <= omcN && omcN < int(m_OEs->size()));

      string const& connName = (*m_OEs)[omcN].m_connName;

      // Install the map elements:
      ByConn[connName][qsym] = pos;
    }

    //-----------------------------------------------------------------------//
    // Go by Connector:                                                      //
    //-----------------------------------------------------------------------//
    double res = 0.0;

    for (map<string, map<StratEnv::QSym, long>>::iterator qit =
         ByConn.begin();  qit != ByConn.end();  ++qit)
    {
      string const& connName = qit->first;

      //---------------------------------------------------------------------//
      // FORTS (direct, via FIX):                                            //
      //---------------------------------------------------------------------//
      if (connName.substr(0, 15) == "Conn_FIX_FORTS_")
      {
        assert(m_secDefs != NULL);
        res += InitMargin_FORTS(&(qit->second), *m_secDefs);
      }
      //---------------------------------------------------------------------//
      // Other Exchanges: Not supported yet:                                 //
      //---------------------------------------------------------------------//
      // XXX: TODO...
    }

    //-----------------------------------------------------------------------//
    // Record the result in "CashInfo":                                      //
    //-----------------------------------------------------------------------//
    assert(m_cashInfo != NULL && m_cashInfo->find(ccy) != m_cashInfo->end());

    CashInfo& ci = (*m_cashInfo)[ccy];
    ci.m_IM      = res;

    // Furthermore, if there is only 1 Ccy for this Strategy, also update the
    // TotalMargin:
    if (m_Ccys->size() == 1)
      SetTotalInitMargin(ccy, res, db_record);
    return res;
  }

  //=========================================================================//
  // "GetGreeks":                                                            //
  //=========================================================================//
  StratEnv::GreeksMap const& StratEnv::GetGreeks(bool db_record)
  {
    //-----------------------------------------------------------------------//
    // Zero-out an existing Risk structure:                                  //
    //-----------------------------------------------------------------------//
    assert(m_Greeks != NULL);
    for (GreeksMap::iterator it = m_Greeks->begin(); it != m_Greeks->end();
         ++it)
      it->second.Clear();

    //-----------------------------------------------------------------------//
    // Go through the whole Portfolio:                                       //
    //-----------------------------------------------------------------------//
    // (Cash not included -- it does not affect any Greeks):
    //
    assert(m_positions != NULL && m_secDefs != NULL && m_Greeks != NULL);

    utxx::time_val now = utxx::now_utc();

    for (PositionsMap::const_iterator  cit = m_positions->begin();
         cit != m_positions->end();  ++cit)
    {
      long pos = (cit->second).m_pos;
      if (pos == 0)
        continue;   // In that case there is no Greeks contribution, obviously

      // Get the QSym and its SecDef:
      QSym        const& qsym    = cit->first;
      QSymInfo    const& qInfo   = GetQSymInfo(qsym); // Exception if not avail

      //---------------------------------------------------------------------//
      // Check the Instrument Type:                                          //
      //---------------------------------------------------------------------//
      char code = qInfo.m_CFICode[0];
      if (code == 'O')
      {
        //-------------------------------------------------------------------//
        // This is an Option, Call or Put:                                   //
        //-------------------------------------------------------------------//
        char   cpFlag = qInfo.m_CFICode[1];
        assert(cpFlag == 'C' || cpFlag == 'P');

        // Numerical Call/Put Coeff:
        double CP = (cpFlag == 'C') ? 1.0 : (-1.0);

        // Option Strike:
        double K  = qInfo.m_Strike;
        assert(K > 0.0);

        // XXX: We currently do not check the Option exrcise type (Europen or
        // American), this can produce imprecise results for Puts... FIXME!..
        //
        // Get the Option Underlying:
        QSym const& undQ  = qInfo.m_Underlying;

        // Get the MktData (arrival price) for the Underlying.  If it is not
        // (yet) available, we cannot compute Greeks:
        // NB:
        double S0 = 0.0;
        try
          { S0 = (GetBestBid(undQ).m_price + GetBestAsk(undQ).m_price) / 2.0; }
        catch (...) {}

        if (utxx::unlikely(!Finite(S0)))
          // Could not get the Underlying price:
          throw runtime_error
                ("StratEnv::GetGreeks: Option="    + qsym.ToString()      +
                 ", Underlying=" + undQ.ToString() + ": Could not get S0");

        // Year Fraction to Option Expiration Time:
        // TODO: convert to utxx::time_val
        double tau = YF<double>((qInfo.m_Expir - now).seconds());
        if (utxx::unlikely(tau <= 0.0))
          throw runtime_error
                ("StratEnv::GetGreeks: Option=" + qsym.ToString() + ": tau=" +
                 to_string(tau));

        // Get the Vol: Currently using Implied Vols (Marked-to-Market) from
        // the SecDef:
        double sigma = qInfo.m_ImplVol;
        assert(sigma > 0.0);

        //-------------------------------------------------------------------//
        // Ready to compute the Greeks:                                      //
        //-------------------------------------------------------------------//
        // (XXX: Currently no Interest Rate or Dividend data -- assuming rates
        // to be 0):
        //
        double price = 0.0; // Currently ignored -- not additive wrt Underl!
        double delta = 0.0;
        double gamma = 0.0;
        double vega  = 0.0;

        GetBSMGreeks
          (CP, S0, K, sigma, tau, 0.0, &price, &delta, &gamma, &vega);

        //-------------------------------------------------------------------//
        // Update by Risks for this Underlying:                              //
        //-------------------------------------------------------------------//
        GreeksMap::iterator git = m_Greeks->find(undQ);
        if (git != m_Greeks->end())
        {
          GreeksInfo& gi  = git->second;
          gi.m_Delta += double(pos) * delta;
          gi.m_Gamma += double(pos) * gamma;
          gi.m_Vega  += double(pos) * vega;
        }
        else
        {
          // Install this underlying:
          GreeksInfo& gi = (*m_Greeks)[undQ];   // Initially zeros

          gi.m_Delta  = double(pos) * delta;
          gi.m_Gamma  = double(pos) * gamma;
          gi.m_Vega   = double(pos) * vega;
        }
      }
      else
      {
        //-------------------------------------------------------------------//
        // This is an Equity or Futures:                                     //
        //-------------------------------------------------------------------//
        // We consider  this to be an Underlying by itself, even though a
        // Futures is formally a derivative over (eg) Equity or Index. Then:
        // Delta = 1, Gamma = 0, Vega = 0:
        //
        GreeksMap::iterator git = m_Greeks->find(qsym);

        if (git != m_Greeks->end())
          (git->second).m_Delta += double(pos);
        else
          // Install this underlying, with all risks being 0 except the new
          // Delta:
          (*m_Greeks)[qsym].m_Delta = double(pos);
      }
      // End of CFICode analysis
    }
    // End of Portfolio loop

    // All done:
    return *(m_Greeks);
  }

  //=========================================================================//
  // "GetVaR":                                                               //
  //=========================================================================//
  // XXX: TODO:
  double StratEnv::GetVaR(Events::Ccy const& ccy, bool db_record)
  {
    return 0.0;
  }

  //=========================================================================//
  // Order Book Analytics:                                                   //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "GetWorstBid":                                                          //
  //-------------------------------------------------------------------------//
  double StratEnv::GetWorstBid(QSym const& qsym, long qty) const
  {
    if (utxx::unlikely(qty <= 0))
      throw invalid_argument
            ("StratEnv::WorstBid: Invalid Qty=" + to_string(qty));

    // Get the OrderBook:
    OrderBookSnapShot const& ob = GetOrderBook(qsym);

    // Go through the Bids, trying to accommodate the required "qty":
    for (int i = 0; i < ob.m_currBids; ++i)
    {
      long lQty = ob.m_bids[i].m_qty;
      if (qty <= lQty)
        // Qty can be accommodated at this level: return Price:
        return ob.m_bids[i].m_price;

      // Otherwise: decrease Qty by what is available at this level, and go
      // further into the depth:
      qty -= lQty;
    }
    // If we got here: Could not accommodate the whole Qty:
    assert(qty > 0);
    throw runtime_error
          ("StratEnv::WorstBid: QSym="         + qsym.ToString() +
           ": Could not accommodate full Qty=" + to_string(qty)  +
           ": Remaining=" + to_string(qty));
  }

  //-------------------------------------------------------------------------//
  // "GetWorstAsk":                                                          //
  //-------------------------------------------------------------------------//
  double StratEnv::GetWorstAsk(QSym const& qsym, long qty) const
  {
    if (utxx::unlikely(qty <= 0))
      throw invalid_argument
            ("StratEnv::WorstAsk: Invalid Qty=" + to_string(qty));

    // Get the OrderBook:
    OrderBookSnapShot const& ob = GetOrderBook(qsym);

    // Go through the Asks, trying to accommodate the required "qty":
    for (int i = 0; i < ob.m_currAsks; ++i)
    {
      long lQty = ob.m_asks[i].m_qty;
      if (qty <= lQty)
        // Qty can be accommodated at this level: return Price:
        return ob.m_asks[i].m_price;

      // Otherwise: decrease Qty by what is available at this level, and go
      // further into the depth:
      qty -= lQty;
    }
    // If we got here: Could not accommodate the whole Qty:
    assert(qty > 0);
    throw runtime_error
          ("StratEnv::WorstAsk: QSym="         + qsym.ToString() +
           ": Could not accommodate full Qty=" + to_string(qty)  +
           ": Remaining=" + to_string(qty));
  }

  //-------------------------------------------------------------------------//
  // "GetArrivalPrice":                                                      //
  //-------------------------------------------------------------------------//
  double StratEnv::GetArrivalPrice(QSym const& qsym) const
  {
    // Get the OrderBook:
    OrderBookSnapShot const& ob = GetOrderBook(qsym);

    if (utxx::unlikely(ob.m_currBids == 0 || ob.m_currAsks == 0))
      throw runtime_error("StratEnv::GetArrivalPrice: Incomplete OrderBook");

    // NB: Bids are sorted in the descending order, Asks -- in ascending:
    return 0.5 * (ob.m_bids[0].m_price + ob.m_asks[0].m_price);
  }

  //-------------------------------------------------------------------------//
  // "LiquidatePortfolio":                                                   //
  //-------------------------------------------------------------------------//
  // XXX: No optimal execution strategies have been implemented as yet -- we
  // just produce market orders for the full quantities:
  //
  void StratEnv::LiquidatePortfolio(bool use_arrival_pxs)
  {
    // Go through all Positions:
    PositionsMap const& poss = GetPositions();

    for (PositionsMap::const_iterator cit = poss.begin(); cit != poss.end();
        ++cit)
    {
      QSym         const& qsym = cit->first;
      PositionInfo const& pi   = cit->second;
      assert(qsym == pi.m_qsym);

      double arrPx =
        use_arrival_pxs ? GetArrivalPrice(qsym) : NaN<double>;

      long pos = pi.m_pos;
      long qty = abs(pos);

      if (pos == 0)
        continue;   // Nothing to do here...

      // Otherwise: Generic Case:
      try
      {
        AOSReq12* req = nullptr;
        double    px  = 0.0;

        if (pos < 0)
        {
          // We hold a Short position; to close it, need to Buy at the worst
          // (highest) Ask price.  Use same Account on which the position is
          // held:
          px  = use_arrival_pxs
                ? arrPx
                : GetWorstAsk(qsym, qty);
          auto ord_type = Finite(px) ? OrderTypeT::Limit : OrderTypeT::Market;
          req = NewOrder(qsym, ord_type, SideT::Buy, px, qty, pi.m_accCrypt);
        }
        else
        {
          // We hold a Long position; to close it, we need to Sell at the
          // worst (lowest) Bid price. Again, use same Account:
          //
          px  = use_arrival_pxs
                ? arrPx
                : GetWorstBid(qsym, qty);
          auto ord_type = Finite(px) ? OrderTypeT::Limit : OrderTypeT::Market;
          req = NewOrder(qsym, ord_type, SideT::Sell, px, qty, pi.m_accCrypt);
        }
        assert(req != NULL);

        AOS* aos = req->GetAOS();

        LOG_INFO("StratEnv::LiquidatePortfolio: New RootID=%012lX: "
                 "%s %ld of %s @ %.5f\n",
                 aos->RootID(), (pos < 0) ? "Buy" : "Sell", qty,
                 qsym.m_symKey.data(), px);

        // Introduce a 50ms wait (in case if the server-side FIX engine uses
        // a flood control mechanism):
        struct timespec  ts = { 0, 50000000 };
        (void) nanosleep(&ts, NULL);
      }
      catch (exception const& exc)
      {
        // Most likely, could not accommodate the whole qty; should we continue
        // with other Positions? Currently we do, although this may create asy-
        // mmetric risk:
        LOG_ERROR("StratEnv::LiquidatePortfolio: Cannot completely liquidate: "
                  "%s\n", exc.what());
      }
    }
  }
}
