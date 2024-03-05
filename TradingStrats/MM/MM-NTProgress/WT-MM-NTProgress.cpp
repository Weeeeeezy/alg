// vim:ts=2:et
//===========================================================================//
//             "TradingStrats/MM-NTProgress/WT-MM-NTProgress.cpp":           //
//    WebToolkit-based Server for Controlling the MM-NTProgress Stratergy    //
//===========================================================================//
#include "InfraStruct/StratMonitor.h"

using namespace MAQUETTE;
using namespace std;

namespace
{
  //=========================================================================//
  // "WT_MM_NTProgress" class:                                               //
  //=========================================================================//
  class WT_MM_NTProgress final: public StratMonitor
  {
  private:
    // Default Ctor is deleted:
    WT_MM_NTProgress() = delete;

  public:
    // Non-Default Ctor:
    WT_MM_NTProgress(Wt::WEnvironment const& a_env)
    : StratMonitor(a_env)
    {}
  };

  //=========================================================================//
  // "MkMonitor":                                                            //
  //=========================================================================//
  unique_ptr<Wt::WApplication> MkStratMonitor(Wt::WEnvironment const& a_env)
    { return make_unique<WT_MM_NTProgress>(a_env); }
}

//===========================================================================//
// "main":                                                                   //
//===========================================================================//
int main(int argc, char* argv[])
{
  try
  {
    Wt::WServer server(argc, argv, WTHTTP_CONFIGURATION);
    server.addEntryPoint(Wt::EntryPointType::Application, MkStratMonitor);
    // TODO: Configure Authentication!

    // At this point, we run as a Daemon if requested:
    // string daemon
    server.run();
  }
  catch (Wt::WServer::Exception const& e1)
  {
    cerr << "EXCEPTION in WServer: " << e1.what() << endl;
    return 1;
  }
  catch (exception const& e2)
  {
    cerr << "EXCEPTION: " << e2.what() << endl;
    return 1;
  }
  // All Done:
  return 0;
}
