// vim:ts=2:et
//===========================================================================//
//                    "TradingStrats/Pairs-MF/Pairs-MF.cpp":                 //
//===========================================================================//
#include "Pairs-MF.h"
#include "InitRun.hpp"
#include "CallBacks.hpp"
#include "Orders.hpp"
#include "Basis/TimeValUtils.hpp"

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
        false,            // MT is not required
        params.get<long>  ("Exec.LogFileSzM") * 1048576,
        params.get<int>   ("Exec.LogFileRotations")
      );
    spdlog::logger*  logger = loggerShP.get();
    assert(logger != nullptr);

    //-----------------------------------------------------------------------//
    // Install "SettlDates" in Connectors:                                   //
    //-----------------------------------------------------------------------//
    // They are currently in [Pair*] sections:
    //
    for (auto const& sect: params)
      if (strncmp(sect.first.data(), "Pair", 4) == 0)
      {
        // This is a "Pair*" section:
        auto const& ps = sect.second;

        // Get its Pass and Aggr MDCs, Symbols, Segments, Tenors, SettlDates:
        for (int aggr = 0; aggr < 2; ++aggr)
        {
          string pfx = aggr ? "Aggr" : "Pass";
          string mdc        = ps.get<string>(pfx + "MDC");
          string instrName  = ps.get<string>(pfx + "Instr");
          string path       = mdc + "." + instrName;
          int    settlDate  = ps.get<int>   (pfx + "SettlDate");

          // Put this info back into the corresp MDC section:
          params.put(path, settlDate);
        }
      }

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
    Pairs_MF pmf(&reactor, logger, params);

    //-----------------------------------------------------------------------//
    // Run the Strategy:                                                     //
    //-----------------------------------------------------------------------//
    pmf.Run();
  }
  catch (exception const& exc)
  {
    cerr << "\nEXCEPTION: " << exc.what() << endl;
    return 1;
  }
  return 0;
}
