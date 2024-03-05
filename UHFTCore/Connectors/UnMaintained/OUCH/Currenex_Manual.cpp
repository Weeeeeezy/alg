// vim:ts=2
//===========================================================================//
//                            "Currenex_Manual.cpp":                         //
//===========================================================================//
#include <Infrastructure/Logger.h>
#include "Common/Maths.h"
#include "StrategyAdaptor/StratEnv.h"
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <cassert>
#include <ctime>
#include <sys/types.h>
#include <boost/process.hpp>
#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/algorithm/string/case_conv.hpp>

using namespace Arbalete;
using namespace MAQUETTE;
using namespace std;
namespace BP  = boost::process;
namespace BPI = boost::process::initializers;
namespace IOS = boost::iostreams;

//===========================================================================//
// "main":                                                                   //
//===========================================================================//
int main(int argc, char* argv[])
{
  //-------------------------------------------------------------------------//
  // Command-Line Args:                                                      //
  //-------------------------------------------------------------------------//
  if (argc < 5 || argc > 8)
  {
    cerr << "PARAMETERS: {Test|Prod} {C|K} Symbol Position [Px|Dyn|Mkt]"
            "[TPSpread] [SLSpread]" << endl;
    return 1;
  }
  string mode     = argv[1];
  bool   cancel   = (argv[2][0] == 'C');     // Cancel active orders on exit
  string symbol   = argv[3];
  long   pos      = atoi(argv[4]);
  double px       = (argc >= 6)
                    ? (strcmp(argv[5], "Mkt") == 0
                       ? NaN<double>         // MktOrder
                       : strcmp(argv[5], "Dyn") == 0
                       ? 0.0                 // To be determined
                       : atof(argv[5]))      // Use this limit px
                    : 0.0;                   // To be determined
  double tpSpread = (argc >= 7) ? atof(argv[6]) : 0.0;
  double slSpread = (argc >= 8) ? atof(argv[7]) : 0.0;

  if (pos == 0 || tpSpread < 0.0 || slSpread < 0.0)
  {
    cerr << "ERROR: Position must be non-0, Spreads must be >= 0.0" << endl;
    return 1;
  }
  long           qty  = abs(pos);
  SideT          side = (pos > 0) ? SideT::Buy : SideT::Sell;
  OrderTypeT ord_type = px == NaN<double>
                      ? OrderTypeT::Market : OrderTypeT::Limit;

  //-------------------------------------------------------------------------//
  // Create a Client:                                                        //
  //-------------------------------------------------------------------------//
  // Connectors:
  string MDC = "cnx-md-" + boost::to_lower_copy(mode);
  string OMC = "cnx-oe-" + boost::to_lower_copy(mode);

  vector<string> mdcs { MDC };
  vector<string> omcs { OMC };

  // "QSym":
  StratEnv::QSym qs(symbol.c_str(), mdcs.size()-1, omcs.size()-1);

  // NB: Currenex uses OUCH OMC which does no

  srand48(time(NULL));
  string q_name = "cnx-manual-" + boost::to_lower_copy(mode)
                + to_string(lrand48());

  StratEnv::ConnEventMsg msg;

  // Create "StratEnv":
  StratEnv env
  (
    "MANUAL",           // Type (eg "MM")
    "MANUAL",
    "cnx-manual",       // Strategy Name
    mode == "Prod" ? EnvType::PROD : EnvType::TEST, // Environment
    q_name,             // Strat MsgQ Name
    mdcs,
    omcs,               // OMC = conn
    std::make_pair(SecDefSrcT::DB, MDC),
    StratEnv::StartModeT::Clean,
    1,                  // DebugLevel
    MaxRTPrio,          // RT priority -- will be ineffective unless "root"
    5                   // CPU Affinity
  );

  //-------------------------------------------------------------------------//
  // Subscribe to the Symbol: omcN=0 always:                                 //
  //-------------------------------------------------------------------------//
  env.Subscribe(qs);

  //-------------------------------------------------------------------------//
  // Get the curr Mkt Price:                                                 //
  //-------------------------------------------------------------------------//
  // (Not required if we place a fixed Limit or a Mkt order -- only needed in
  // the "Dynamic" mode):
  bool   isDyn = (strcmp(argv[5], "Dyn") == 0);
  while (isDyn)
  {
    // Get the next event:
    env.GetNextEvent(&msg);
    Events::EventT tag = msg.GetTag();
    SLOG(INFO) << "Event:" <<  Events::ToString(tag);

    switch (tag)
    {
      case Events::EventT::StratExit:
        SLOG(INFO) << "EXIT REQUESTED" << endl;
        return 0; // XXX: Receive final events first?

      case Events::EventT::OrderBookUpdate:
        assert(msg.GetQSym() == qs);
        // Need Bid[0] or Ask[0] depending on the side. If it is not yet
        // available, catch the exception and wait:
        try
        {
          OrderBook::Entry const& best =
            (side == SideT::Sell) ? env.GetBestAsk(qs) : env.GetBestBid(qs);

          // OK, got it. XXX: Qty is currently disregarded, only px is used:
          if (px == 0.0)
            // NB: Do NOT do this if "px" is NaN, in which case a true MktOrder
            // will be used:
            px = best.m_price;
          goto PlaceOrder;
        }
        catch (...){}
        break;

      default: ;
    }
  }

  //-------------------------------------------------------------------------//
  // Place an Order or a Pair of Orders:                                     //
  //-------------------------------------------------------------------------//
PlaceOrder:;
  cerr << "TradingStatus: "
       << env.GetSecDef(qs).m_TradingStatus << "\nPx=" << px
       << endl;

  // NB: "accCrypt" is currently NOT used for Currenex, hence 0:
  AOSReq12* req = env.NewOrder(qs, ord_type, side, px, qty, 0);
  assert(req != NULL);

  cerr << "RootID=" << req->GetAOS()->RootID() << ", " << ToString(side)
       << ", Px="   << px  <<  ", Qty="  << qty  << endl;

  if (tpSpread > 0.0)
  {
    // Place a take-profit order at the specified distance:
    double     tpPx = side == SideT::Buy ? (px + tpSpread) : (px - tpSpread);
    auto other_side = OtherSide(side);
    AOSReq12* tpReq = env.NewOrder
                        (qs, OrderTypeT::Limit, other_side, tpPx, qty, 0);
    assert(tpReq != NULL);

    cerr << "Take-Profit OrderID=" << tpReq->GetAOS()->RootID()
         << ", Side=" << ToString(other_side)
         << ", Px=" << tpPx << ", Qty=" << qty << endl;
  }

  if (slSpread > 0.0)
  {
    // Place a stop-loss order at the same distance in the opposite direction:
    double slPx   = side == SideT::Buy ? (px - slSpread) : (px + slSpread);
    auto*  slReq  = env.NewOrder(qs, OrderTypeT::Limit, side, slPx, qty, 0);
    assert(slReq != NULL);

    cerr << "Stop-Loss RootID=" << slReq->GetAOS()->RootID() << ", Side="
         << ToString(side) <<  ", Px="   << slPx << ", Qty="   << qty << endl;
  }

  //-----------------------------------------------------------------------//
  // Event Loop (until Exit):                                              //
  //-----------------------------------------------------------------------//
  // NB: Actually, if removing of active orders is requested, then 2 SIGINTs
  // are required for an exit:
  //
  int sigCount = 0;
  while (true)
  {
    //-----------------------------------------------------------------------//
    // Get the next event with 1-second waiting:                             //
    //-----------------------------------------------------------------------//
    bool got = env.GetNextEvent(&msg, utxx::time_val(utxx::rel_time(1, 0)));
    if (!got)
      continue;

    Events::EventT tag = msg.GetTag();
    switch (tag)
    {
      //---------------------------------------------------------------------//
      case Events::EventT::StratExit:
      //---------------------------------------------------------------------//
      {
        ++sigCount;
        SLOG(INFO) << "EXIT REQUESTED (" << sigCount << ')' << endl;
        goto Exit;
      }

      //---------------------------------------------------------------------//
      case Events::EventT::TradeBookUpdate:
      case Events::EventT::SecDefUpdate:
      case Events::EventT::ConnectorStop:
      case Events::EventT::ConnectorStart:
      case Events::EventT::RequestError:
      //---------------------------------------------------------------------//
        // NB: On RequestError, log output is produced automatically
        if (tag == Events::EventT::ConnectorStop)
          SLOG(ERROR) << Events::ToString(tag) << endl;
        break;

      //---------------------------------------------------------------------//
      case Events::EventT::OrderFilled:
      //---------------------------------------------------------------------//
      {
        AOSReqFill* rf = msg.GetReqFillPtr();
        assert(rf     != NULL);
        AOS const* aos = rf->Req()->GetAOS();
        assert(aos != NULL);

        SLOG(INFO) << "FILLED: RootID="  << aos->RootID()
                  << ", Symbol="        << Events::ToCStr(aos->m_symbol)
                  << ToString(aos->Side()) << ", CumQty="
                  << aos->CumQty()      << ", AvgPx="  << aos->AvgPrice()
                  << endl;

        // List all our Trades:
        StratEnv::OurTradesMap const& ourTrades = env.GetOurTrades();
        SLOG(INFO) << "=== OurTrades: ===" << endl;

        for (StratEnv::OurTradesMap::const_iterator cit0 = ourTrades.begin();
             cit0 != ourTrades.end();  ++cit0)
          for (StratEnv::OurTradesVector::const_iterator cit1 =
              cit0->second.begin(); cit1 != cit0->second.end(); ++cit1)
          {
            double px =
              Finite(cit1->m_avgPx)
              ? cit1->m_avgPx
              : cit1->m_lastPx;
            SLOG(INFO) << '\t' << (cit1->m_isBuy ? "BUY " : "SELL ") << '\t'
                      << Events::ToString(cit1->m_qsym.m_symKey)    << '\t'
                      << px   <<  '\t'  << cit1->m_cumQty << endl;
          }
        break;
      }

      //---------------------------------------------------------------------//
      case Events::EventT::OrderPartFilled:
      //---------------------------------------------------------------------//
      {
        AOSReqFill* rf = msg.GetReqFillPtr();
        assert(rf     != NULL);
        AOS const* aos = rf->Req()->GetAOS();
        assert(aos != NULL);

        SLOG(INFO) << "PARTIALLY FILLED: RootID=" << aos->RootID()
                  << ", Symbol=" << Events::ToCStr(aos->m_symbol)
                  << ToString(aos->Side()) << ", CumQty="
                  << aos->CumQty() << ", LeavesQty=" << aos->LeavesQty()
                  << ", LastPx="   << rf->Price()    << endl;
        break;
      }

      //---------------------------------------------------------------------//
      case Events::EventT::OrderCancelled:
      //---------------------------------------------------------------------//
      {
        AOS* aos = msg.GetAOSPtr();
        assert(aos != NULL);

        SLOG(INFO) << "CANCELLED: RootID=" << aos->RootID()
                  << Events::ToString(aos->m_symbol) << endl;
        break;
      }

      //---------------------------------------------------------------------//
      case Events::EventT::MiscStatusChange:
      //---------------------------------------------------------------------//
      {
        AOSReq12* req = msg.GetReqPtr();
        assert(req   != NULL);
        AOS*   aos    =  req->GetAOS();
        assert(aos   != NULL);

        // TODO: more detailed info here...

        SLOG(INFO) << "STATUS CHANGE: RootID=" << aos->RootID() << endl;

        // List all AOSes:
        //
        SLOG(INFO) << "=== Active Orders: ==="<< endl;
        StratEnv::AOSMapQPV const& aoss = env.GetAOSes();

        for (StratEnv::AOSMapQPV::const_iterator cit0 = aoss.begin();
             cit0 != aoss.end(); cit0++)
          for (StratEnv::AOSPtrVec::const_iterator cit1 = cit0->second.begin();
               cit1 != cit0->second.end();  cit1++)
          {
            AOS*   aos  = *cit1;
            assert(aos != NULL && aos->m_symbol == cit0->first.m_symKey);
            SLOG(INFO) << "\t" << aos->RootID() << endl;
          }
        break;
      }

      //---------------------------------------------------------------------//
      case Events::EventT::MassCancelReport:
      //---------------------------------------------------------------------//
      {
        AOS* aos = msg.GetAOSPtr();
        assert(aos != NULL);

        SLOG(INFO) << "MASS CANCEL REPORT: RootID=" << aos->RootID() << endl;
        break;
      }
      //---------------------------------------------------------------------//
      default: ;
      //---------------------------------------------------------------------//
    }
  }
  //-------------------------------------------------------------------------//
  // At the End:                                                             //
  //-------------------------------------------------------------------------//
  Exit:;
  if (cancel)
  {
    // NB: No "accCrypt":
    env.CancelAllOrders(qs, 0);
    sleep(5);
  }
  return 0;
}
