// vim:ts=2:et
//===========================================================================//
//                 "TradingStrats/3Arb/TriArb2S/TriArb2S.cpp":               //
//            2-Sided Triangle Arbitrage (using MICEX and AlfaFIX2)          //
//===========================================================================//
#include "Basis/TimeValUtils.hpp"
#include "TradingStrats/3Arb/TriArb2S/TriArb2S.h"
#include "TradingStrats/3Arb/TriArb2S/InitRun.hpp"
#include "TradingStrats/3Arb/TriArb2S/CallBacks.hpp"
#include "TradingStrats/3Arb/TriArb2S/Orders.hpp"

using namespace MAQUETTE;
using namespace std;

//===========================================================================//
// Utils:                                                                    //
//===========================================================================//
namespace
{
  //-------------------------------------------------------------------------//
  // "InstallSettlDate":                                                     //
  //-------------------------------------------------------------------------//
  inline void InstallSettlDate
  (
    boost::property_tree::ptree* a_params,      // XXX: In-out arg (via ptr)
    char const*                  a_path,
    int                          a_settl_date
  )
  {
    assert(a_params != nullptr);
    // Does this node already exist?
    auto  mbNode = a_params->get_child_optional(a_path);
    if  (!mbNode)
      // No, it does not exist yet -- so always install the SettlDate:
      a_params->put(a_path, a_settl_date);
  }
}

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

    //-----------------------------------------------------------------------//
    // Create the Logger:                                                    //
    //-----------------------------------------------------------------------//
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
    // Install all Instrs in Config:                                         //
    //-----------------------------------------------------------------------//
    // This is to avoid manually specifying SettlDates for multiple Instrs, and
    // also to de-configure TOD Instrs after the cut-off time:
    //
    int TOD    = params.get<int>("Main.TOD");
    int TOM_ER = params.get<int>("Main.TOM_EUR/RUB");
    int TOM_UR = params.get<int>("Main.TOM_USD/RUB");
    int SPOT   = params.get<int>("Main.SPOT");

    // Install TOD Instrs in both MDC Connectors:
    //
    InstallSettlDate
      (&params, "EConnector_FAST_MICEX_FX.EUR_RUB__TOD-MICEX-CETS-TOD", TOD);
    InstallSettlDate
      (&params, "EConnector_FAST_MICEX_FX.USD000000TOD-MICEX-CETS-TOD", TOD);

    InstallSettlDate
      (&params, "EConnector_FIX_AlfaFIX2_MKT.EUR/RUB-AlfaFIX2--TOD",    TOD);
    InstallSettlDate
      (&params, "EConnector_FIX_AlfaFIX2_MKT.USD/RUB-AlfaFIX2--TOD",    TOD);

    // Install TOM Instrs in both MDC Connectors:
    InstallSettlDate
      (&params, "EConnector_FAST_MICEX_FX.EUR_RUB__TOM-MICEX-CETS-TOM", TOM_ER);
    InstallSettlDate
      (&params, "EConnector_FAST_MICEX_FX.USD000UTSTOM-MICEX-CETS-TOM", TOM_UR);

    InstallSettlDate
      (&params, "EConnector_FIX_AlfaFIX2_MKT.EUR/RUB-AlfaFIX2--TOM",    TOM_ER);
    InstallSettlDate
      (&params, "EConnector_FIX_AlfaFIX2_MKT.USD/RUB-AlfaFIX2--TOM",    TOM_UR);

    // Install SPOT for 1 Instr:
    InstallSettlDate
      (&params, "EConnector_FIX_AlfaFIX2_MKT.EUR/USD-AlfaFIX2--SPOT",   SPOT);

    //-----------------------------------------------------------------------//
    // Create the Reactor and the Strategy Instance:                         //
    //-----------------------------------------------------------------------//
    // EPollReactor (MaxUDPSz has the default value):
    EPollReactor reactor
    (
      logger,
      params.get<int> ("Main.DebugLevel"),
      params.get<bool>("Exec.UsePoll",      false),
      params.get<bool>("Exec.UseKernelTLS", true),
      128,            // MaxFDs
      params.get<int> ("Exec.StrategyCPU")
    );

    // Strategy Instance:
    TriArb2S tra2s(&reactor, logger, &params);

    //-----------------------------------------------------------------------//
    // Run the Strategy:                                                     //
    //-----------------------------------------------------------------------//
    tra2s.Run();
  }
  catch (exception const& exc)
  {
    cerr << "\nEXCEPTION: " << exc.what() << endl;
    return 1;
  }
  return 0;
}
