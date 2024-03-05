#include "Basis/ConfigUtils.hpp"
#include "Basis/IOUtils.h"
#include "Connectors/EConnector_MktData.h"
#include "Connectors/EConnector_OrdMgmt.h"
#include "QuantSupport/BT/HistOMC_InstaFill.h"

#include "Connectors/FIX/EConnector_FIX.h"
#include "Connectors/H2WS/Binance/EConnector_H2WS_Binance_MDC.h"
#include "Connectors/H2WS/Binance/EConnector_H2WS_Binance_OMC.h"
#include "Connectors/H2WS/TDAmeritrade/EConnector_H1WS_TDAmeritrade.h"
#include "TradingStrats/BLS/Common/BLSStrategy.hpp"
#include <memory>

using namespace MAQUETTE;
using namespace BLS;

static constexpr bool BACKTEST = false;
static constexpr bool EMULATE_TS = false;

template <typename MDC, typename OMC, bool USE_FRAC_QTY, bool SEP_ORD_OPN_CLOSE>
void AddStrategy(std::vector<std::unique_ptr<Strategy>> &a_strats,
                 EPollReactor *a_reactor, spdlog::logger *a_logger,
                 const boost::property_tree::ptree &a_params, MDC *a_mdc,
                 OMC *a_omc, uint64_t a_bls_idx) {
  a_strats.emplace_back(
      std::make_unique<BLSStrategy<MDC, OMC, USE_FRAC_QTY, SEP_ORD_OPN_CLOSE,
                                   BACKTEST, EMULATE_TS>>(
          a_reactor, a_logger, a_params, a_mdc, a_omc, a_bls_idx));
}

template <typename MDC, typename OMC, bool USE_FRAC_QTY>
void AddStrategy(std::vector<std::unique_ptr<Strategy>> &a_strats,
                 bool a_separate_open_close_orders, EPollReactor *a_reactor,
                 spdlog::logger *a_logger,
                 const boost::property_tree::ptree &a_params, MDC *a_mdc,
                 OMC *a_omc, uint64_t a_bls_idx) {
  if (a_separate_open_close_orders)
    AddStrategy<MDC, OMC, USE_FRAC_QTY, true>(
        a_strats, a_reactor, a_logger, a_params, a_mdc, a_omc, a_bls_idx);
  else
    AddStrategy<MDC, OMC, USE_FRAC_QTY, false>(
        a_strats, a_reactor, a_logger, a_params, a_mdc, a_omc, a_bls_idx);
}

template <typename MDC, typename OMC>
void AddStrategy(std::vector<std::unique_ptr<Strategy>> &a_strats,
                 bool a_fractional_qty, bool a_separate_open_close_orders,
                 EPollReactor *a_reactor, spdlog::logger *a_logger,
                 const boost::property_tree::ptree &a_params, MDC *a_mdc,
                 OMC *a_omc, uint64_t a_bls_idx) {
  if (a_fractional_qty)
    AddStrategy<MDC, OMC, true>(a_strats, a_separate_open_close_orders,
                                a_reactor, a_logger, a_params, a_mdc, a_omc,
                                a_bls_idx);
  else
    AddStrategy<MDC, OMC, false>(a_strats, a_separate_open_close_orders,
                                 a_reactor, a_logger, a_params, a_mdc, a_omc,
                                 a_bls_idx);
}

template <typename MDC, typename OMC>
void AddAllStrategies(const boost::property_tree::ptree &a_pt,
                      const std::vector<std::string> &a_strategy_names,
                      std::vector<std::unique_ptr<Strategy>> &a_strats,
                      bool a_fractional_qty, bool a_separate_open_close_orders,
                      EPollReactor *a_reactor, spdlog::logger *a_logger,
                      EConnector_MktData *a_mdc, EConnector_OrdMgmt *a_omc) {
  for (uint64_t i = 0; i < a_strategy_names.size(); ++i) {
    auto params = GetParamsSubTree(a_pt, a_strategy_names[i]);
    AddStrategy<MDC, OMC>(a_strats, a_fractional_qty,
                          a_separate_open_close_orders, a_reactor, a_logger,
                          params, dynamic_cast<MDC *>(a_mdc),
                          dynamic_cast<OMC *>(a_omc), i);
  }
}

//===========================================================================//
// "main": //
//===========================================================================//
int main(int argc, char *argv[]) {
  try {
    IO::GlobalInit({SIGINT});

    if (argc != 2) {
      std::cerr << "PARAMETER: ConfigFile.ini" << std::endl;
      return 1;
    }
    std::string iniFile(argv[1]);

    boost::property_tree::ptree pt;
    boost::property_tree::ini_parser::read_ini(iniFile, pt);

    std::shared_ptr<spdlog::logger> loggerShP =
        IO::MkLogger("Main", pt.get<std::string>("Main.LogFile"));
    spdlog::logger *logger = loggerShP.get();
    assert(logger != nullptr);

    EPollReactor reactor(logger, pt.get<int>("Main.DebugLevel"), false, false);

    auto exch = pt.get<std::string>("Main.Exchange");
    SecDefsMgr *secDefsMgr = SecDefsMgr::GetPersistInstance(true, exch);

    auto strat_params = pt.get<std::string>("Main.Strategies");
    std::vector<std::string> strategy_names;
    boost::split(strategy_names, strat_params, boost::is_any_of(","));

    int num_strats = int(strategy_names.size());
    BLS_CreatePool(num_strats);

    // Create MDC and OMC
    EConnector_MktData *mdc = nullptr;
    EConnector_OrdMgmt *omc = nullptr;
    std::vector<std::unique_ptr<Strategy>> strats;
    bool fractional_qty, use_seprate_open_close_orders;

#define ADD_STRATS()                                                           \
  AddAllStrategies<MDC, OMC>(pt, strategy_names, strats, fractional_qty,       \
                             use_seprate_open_close_orders, &reactor, logger,  \
                             mdc, omc)

    if (exch == "TDAmeritrade") {
      using MDC = EConnector_H1WS_TDAmeritrade;
      using OMC = EConnector_H1WS_TDAmeritrade;
      mdc = new MDC(&reactor, secDefsMgr, nullptr, nullptr,
                    GetParamsSubTree(pt, "EConnector_H1WS_TDAmeritrade"),
                    nullptr);
      omc = dynamic_cast<OMC*>(mdc);

      fractional_qty = false;
      use_seprate_open_close_orders = true;
      ADD_STRATS();
    } else if (exch == "Binance_Fut") {
      using MDC = EConnector_H2WS_Binance_MDC<Binance::AssetClassT::FutT>;
      using OMC = EConnector_H2WS_Binance_OMC<Binance::AssetClassT::FutT>;
      mdc = new MDC(&reactor, secDefsMgr, nullptr, nullptr,
                    GetParamsSubTree(pt, "EConnector_H2WS_Binance_MDC_Fut"),
                    nullptr);
      omc = new OMC(&reactor, secDefsMgr, nullptr, nullptr,
                    GetParamsSubTree(pt, "EConnector_H2WS_Binance_OMC_Fut"));

      fractional_qty = true;
      use_seprate_open_close_orders = false;
      ADD_STRATS();
    } else if (exch == "TTPaper") {
      using MDC = EConnector_FIX_TT;
      using OMC = HistOMC_InstaFill<long>;
      mdc = new MDC(&reactor, secDefsMgr, nullptr, nullptr,
                    GetParamsSubTree(pt, "EConnector_FIX_TT"), nullptr);
      omc = new OMC(&reactor, secDefsMgr, nullptr, nullptr,
                    GetParamsSubTree(pt, "HistOMC_InstaFill"));

      fractional_qty = false;
      use_seprate_open_close_orders = false;
      ADD_STRATS();
    } else {
      throw std::runtime_error("Unknown exchange: " + exch);
    }

    bool shared_mdc_omc =
      (static_cast<void*>(omc) == static_cast<void*>(mdc));

    mdc->Start();
    if (!shared_mdc_omc)
      omc->Start();

    // Run it! Exit on any unhandled exceptions:
    bool busyWait = pt.get<bool>("Main.BusyWaitInEPoll");
    reactor.Run(true, busyWait);

    // Termination:
    mdc->Stop();
    if (!shared_mdc_omc)
      omc->Stop();

    // destroy strategies
    for (size_t i = 0; i < strats.size(); ++i) {
      strats[i].reset();
    }

    delete mdc;
    if (!shared_mdc_omc)
      delete omc;

    BLS_DestroyPool();
  } catch (std::exception const &exc) {
    std::cerr << "\nEXCEPTION: " << exc.what() << std::endl;
    return 1;
  }

  return 0;
}
