// vim:ts=2:et
//===========================================================================//
//                              "LARisKa.cpp":                               //
//                       LATOKEN Risk Manager on Kafka                       //
//===========================================================================//
#include "Basis/IOUtils.hpp"
#include "Basis/ConfigUtils.hpp"
#include "Connectors/Kafka/EConnector_LATOKEN_STP.h"
#include "Connectors/H2WS/Binance/EConnector_H2WS_Binance_MDC.h"
#include "Connectors/H2WS/EConnector_WS_MDC.hpp"
#include "Protocols/H2WS/WSProtoEngine.hpp"
#include "InfraStruct/SecDefsMgr.h"
#include "InfraStruct/RiskMgr.h"
#include <utxx/compiler_hints.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <iostream>
#include <csignal>
#include <cassert>

using namespace MAQUETTE;
using namespace std;


//===========================================================================//
// "main":                                                                   //
//===========================================================================//
int main(int argc, char* argv[])
{
  //-------------------------------------------------------------------------//
  // Global Init:                                                            //
  //-------------------------------------------------------------------------//
  IO::GlobalInit({SIGINT});

  shared_ptr<spdlog::logger> loggerShP;         // Empty as yet
  spdlog::logger*            logger = nullptr;  //

  try
  {
    //-----------------------------------------------------------------------//
    // Get the Config:                                                       //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely(argc != 2))
    {
      cerr << "PARAMETER: ConfigFile.ini" << endl;
      return 1;
    }
    string iniFile(argv[1]);

    // Get the Propert Tree:
    boost::property_tree::ptree params;
    boost::property_tree::ini_parser::read_ini(iniFile, params);

    // Propagate some settings from the "Common" section to other Sections:
    string const& env        =  params.get<string>("Common.Env");
    string const& logFile    =  params.get<string>("Common.LogFile");
    int           debugLevel =  params.get<int>   ("Common.DebugLevel");
    bool          isProd     =  (env == "Prod");

    // NB: AccountPfx is extended with RFC tag for safety (FIXME: currently
    // disabled):
    string const& accountPfx =
      params.get<string>("Common.AccountPfx") + "-" +
      params.get<string>("RiskMgr.RFC");

    for (auto it  = params.ordered_begin(); it != params.not_found(); ++it)
    {
      if (it->first == "Common")
        continue;
      // NB: For MDCs, we always use the "Prod" env, as others may be unavail-
      // able or unreliable:
      it->second.put("Env",                  env);
      it->second.put("AccountPfx",           accountPfx);
      it->second.put("LogFile",              logFile);
      it->second.put("DebugLevel",           debugLevel);
      it->second.put("UseDateInShMSegmName", false);

      // Special settings for Connectors:
      if (it->first == "STP")
        it->second.put("AccountKey", accountPfx + "-LATOKEN-STP-" + env);
      else
      if (it->first == "MDC-Binance")
        it->second.put("AccountKey", accountPfx + "-Binance-MDC-" + env);
    }
    //-----------------------------------------------------------------------//
    // Create the Logger:                                                    //
    //-----------------------------------------------------------------------//
    loggerShP = IO::MkLogger
    (
      "Main",
      logFile,
      true      // Multi-threaded (required for async Kafka logging)
    );
    logger = loggerShP.get();
    assert(logger != nullptr);

    //-----------------------------------------------------------------------//
    // Create the Reactor:                                                   //
    //-----------------------------------------------------------------------//
    EPollReactor reactor
    (
      logger,
      debugLevel,
      params.get<bool>("Exec.UsePoll",      false),
      params.get<bool>("Exec.UseKernelTLS", true),
      128,            // MaxFDs
      params.get<int> ("Exec.CPU",          -1)
    );

    //-----------------------------------------------------------------------//
    // Graceful Termination on SIGINT:                                       //
    //-----------------------------------------------------------------------//
    // NB: This by itself may not work w/o the global "SigIntBlocker", because
    // the sygnal could be caught by other threads (created by 3rd-party libs)
    // instead of being processed synchronously:
    reactor.AddSignals
    (
      (accountPfx + "-SignalFD").data(),
      { SIGINT },
      [&reactor](int, signalfd_siginfo const&)->void
        { reactor.ExitImmediately("SIGINT Received"); },
      IO::FDInfo::ErrHandler()
    );

    //-----------------------------------------------------------------------//
    // Create the SecDefsMgr and RiskMgr in ShM:                             //
    //-----------------------------------------------------------------------//
    SecDefsMgr* secDefsMgr = SecDefsMgr::GetPersistInstance(isProd, accountPfx);
    assert(secDefsMgr != nullptr);

    RiskMgr*  riskMgr =
      RiskMgr::GetPersistInstance
      (
        isProd,
        GetParamsSubTree(params, "RiskMgr"),
        *secDefsMgr,
        false,          // NOT a R/O Observer
        logger,
        debugLevel
      );
      assert(riskMgr != nullptr);

    // NB: At this moment, SecDefsMgr and RiskMgr are empty -- they do not
    // have any Instruments / Assets data...

    //-----------------------------------------------------------------------//
    // Create the LATOKEN STP Connector:                                     //
    //-----------------------------------------------------------------------//
    EConnector_LATOKEN_STP stpLA
    (
      &reactor,
      secDefsMgr,
      riskMgr,
      params     // NB: ALL "params" are required here!
    );

    //-----------------------------------------------------------------------//
    // Start the RiskMgr and STP, and Run the Reactor Loop:                  //
    //-----------------------------------------------------------------------//
    // NB:
    // (*) RiskMgr is started in STP Mode;
    // (*) Do NOT start the MDCs -- it will be done by the STP after the Dyn-
    //     Init phase is over, otherwise MDCs can be starved while Kafka msgs
    //     being read from the log:
    //
    riskMgr->Start(RMModeT::STP);
    stpLA   .Start();
    reactor .Run  (false);  // Do not exit on any exceptions

    // Before Exiting: Stop the STP gracefully (this also stops the MDCs):
    stpLA   .Stop();
  }
  //-------------------------------------------------------------------------//
  // Outer Exception Handling:                                               //
  //-------------------------------------------------------------------------//
  catch (exception const& exn)
  {
    if (utxx::likely(logger != nullptr))
      logger->flush();

    cerr << "\nEXCEPTION: " << exn.what() << endl;
    // Prevent any further exceptions eg in Dtors -- exit immediately:
    _exit(1);
  }
  // If we got here:
  return 0;
}
