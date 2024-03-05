// vim:ts=2:et
//===========================================================================//
//                  "TradingStrats/3Arb/TriArbC0/TriArbC0.cpp":              //
//===========================================================================//
#include "Basis/TimeValUtils.hpp"
#include "TradingStrats/3Arb/TriArbC0/TriArbC0.h"
#include "TradingStrats/3Arb/TriArbC0/QuantLib.hpp"
#include "TradingStrats/3Arb/TriArbC0/Orders.hpp"
#include "TradingStrats/3Arb/TriArbC0/CallBacks.hpp"
#include "TradingStrats/3Arb/TriArbC0/InitRun.hpp"

namespace MAQUETTE
{
  //=========================================================================//
  // "TriArbC0" Instances:                                                   //
  //=========================================================================//
  template class TriArbC0<ExchC0T::BinanceSpot>;  // TriArbC0_BinanceSpot
  template class TriArbC0<ExchC0T::BitMEX>;       // TriArbC0_BitMEX
}

//===========================================================================//
// "main":                                                                   //
//===========================================================================//
int main(int argc, char* argv[])
{
  using namespace MAQUETTE;
  using namespace std;

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
    // Create and Run the Strategy Instance:                                 //
    //-----------------------------------------------------------------------//
    string exch = params.get<string>("Main.Exchange");

    if (exch == "BinanceSpot")
    {
      TriArbC0_BinanceSpot strat(&reactor, logger, &params);
      strat.Run();
    }
    else
    if (exch == "BitMEX")
    {
      TriArbC0_BitMEX      start(&reactor, logger, &params);
      start.Run();
    }
    else
      throw utxx::badarg_error("Invalid Exchange: ", exch);
  }
  catch (exception const& exc)
  {
    cerr << "\nEXCEPTION: " << exc.what() << endl;
    return 1;
  }
  return 0;
}
