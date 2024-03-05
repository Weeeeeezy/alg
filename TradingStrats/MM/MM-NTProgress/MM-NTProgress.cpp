// vim:ts=2:et
//===========================================================================//
//                "TradingStrats/MM-NTProgress/MM-NTProgress.cpp":            //
//===========================================================================//
#include "Basis/TimeValUtils.hpp"
#include "MM-NTProgress.h"
#include "InitRun.hpp"
#include "CallBacks.hpp"
#include "QuantAnalytics.hpp"
#include "Orders.hpp"

using namespace MAQUETTE;
using namespace std;

//===========================================================================//
// "main":                                                                   //
//===========================================================================//
int main(int argc, char* argv[])
{
  try
  {
    //-----------------------------------------------------------------------//
    // Global Init:                                                          //
    //-----------------------------------------------------------------------//
    IO::GlobalInit({SIGINT});

    //-----------------------------------------------------------------------//
    // Get the Config:                                                       //
    //-----------------------------------------------------------------------//
    if (argc != 2)
    {
      cerr << "PARAMETER: ConfigFile.ini" << endl;
      return 1;
    }
    string iniFile(argv[1]);

    // Get the Propert Tree:
    boost::property_tree::ptree params;
    boost::property_tree::ini_parser::read_ini(iniFile, params);

    // Create the Logger:
    shared_ptr<spdlog::logger> loggerShP =
      IO::MkLogger
      (
        "Main",
        params.get<string>("Exec.LogFile"),
        false,            // MT is NOT required!
        params.get<long>  ("Exec.LogFileSzM") * 1048576,
        params.get<int>   ("Exec.LogFileRotations")
      );
    spdlog::logger*  logger = loggerShP.get();
    assert(logger != nullptr);

    //-----------------------------------------------------------------------//
    // Install all SettlDates in MDC/OMC Configs:                            //
    //-----------------------------------------------------------------------//
    // This is to avoid manually specifying SettlDates for multiple Instrs, and
    // also to de-configure TOD Instrs  if they are not traded  due to European
    // or American holidays:
    //
    int TOD    = params.get<int>("Main.TOD");
    int TOM_AB = params.get<int>("Main.TOM_EUR/RUB");
    int TOM_CB = params.get<int>("Main.TOM_USD/RUB");
    int TOM_AC = params.get<int>("Main.TOM_EUR/USD");

    bool isEnabled_AB_TOD = params.get<bool>("Main.EUR/RUB_TOD_Enabled");
    bool isEnabled_AB_TOM = params.get<bool>("Main.EUR/RUB_TOM_Enabled");
    bool isEnabled_CB_TOD = params.get<bool>("Main.USD/RUB_TOD_Enabled");
    bool isEnabled_CB_TOM = params.get<bool>("Main.USD/RUB_TOM_Enabled");
    // NB:
    // EUR/USD (TOD,TOM) on MICEX are actually synthetic instrs derived from
    // EUR/RUB and USD/RUB with the same tenors (the standard EUR/USD is T+2),
    // so they are enabled iff both EUR/RUB and USD/RUB are enabed for  the
    // corresp tenor (XXX: this could result in false negatives if AB or CB
    // is disabled for non-market reasons -- but that's OK):
    //
    bool isEnabled_AC_TOD = isEnabled_AB_TOD && isEnabled_CB_TOD;
    bool isEnabled_AC_TOM = isEnabled_AB_TOM && isEnabled_CB_TOM;

    if (utxx::likely(isEnabled_AB_TOD))
    { // May or may not be available:
      params.put
        ("EConnector_FIX_NTProgress_ORD.EUR/RUB_TOD-NTProgress--TOD", TOD);
      params.put
        ("EConnector_FAST_MICEX_FX.EUR_RUB__TOD-MICEX-CETS-TOD",      TOD);
    }
    if (utxx::likely(isEnabled_AB_TOM))
    { // Usually available:
      params.put
        ("EConnector_FIX_NTProgress_ORD.EUR/RUB_TOM-NTProgress--TOM", TOM_AB);
      params.put
        ("EConnector_FAST_MICEX_FX.EUR_RUB__TOM-MICEX-CETS-TOM",      TOM_AB);
    }

    if (utxx::likely(isEnabled_CB_TOD))
    { // May or may not be available:
      params.put
        ("EConnector_FIX_NTProgress_ORD.USD/RUB_TOD-NTProgress--TOD", TOD);
      params.put
        ("EConnector_FAST_MICEX_FX.USD000000TOD-MICEX-CETS-TOD",      TOD);
    }
    if (utxx::likely(isEnabled_CB_TOM))
    {
      // Usually available:
      params.put
        ("EConnector_FIX_NTProgress_ORD.USD/RUB_TOM-NTProgress--TOM", TOM_CB);
      params.put
        ("EConnector_FAST_MICEX_FX.USD000UTSTOM-MICEX-CETS-TOM",      TOM_CB);
    }

    if (utxx::likely(isEnabled_AC_TOD))
      // May or may not be available:
      params.put
        ("EConnector_FAST_MICEX_FX.EURUSD000TOD-MICEX-CETS-TOD",      TOD);

    if (utxx::likely(isEnabled_AC_TOM))
      // Usially available:
      params.put
        ("EConnector_FAST_MICEX_FX.EURUSD000TOM-MICEX-CETS-TOM",      TOM_AC);

    //-----------------------------------------------------------------------//
    // Create the Reactor:                                                   //
    //-----------------------------------------------------------------------//
    EPollReactor reactor
    (
      logger,
      params.get<int> ("Main.DebugLevel"),
      params.get<bool>("Exec.UsePoll",      false),
      params.get<bool>("Exec.UseKernelTLS", true),
      128,            // MaxFDs
      params.get<int> ("Exec.StrategyCPU")
    );

    //-----------------------------------------------------------------------//
    // Create and Run the Strategy:                                          //
    //-----------------------------------------------------------------------//
    MM_NTProgress mmNTP(&reactor, logger, params);

    mmNTP.Run();
  }
  catch (exception const& exc)
  {
    cerr << "\nEXCEPTION: " << exc.what() << endl;
    return 1;
  }
  return 0;
}
