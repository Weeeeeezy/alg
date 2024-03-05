// vim:ts=2:et
//===========================================================================//
//                        "InfraStruct/StratMonitor.h":                      //
// WebToolkit-based Server Class for Controlling and Monitoring Stratergies  //
//===========================================================================//
#pragma once

#include "InfraStruct/SecDefsMgr.h"
#include "InfraStruct/RiskMgr.h"
#include <Wt/WServer.h>
#include <Wt/WApplication.h>
#include <Wt/WStackedWidget.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WText.h>
#include <Wt/WTable.h>
#include <Wt/WSocketNotifier.h>
#include <Wt/WTimer.h>
#include <boost/property_tree/ptree.hpp>
#include <memory>

namespace MAQUETTE
{
  //=========================================================================//
  // "StratMonitor" Class:                                                   //
  //=========================================================================//
  // An instance of this class is created per each Client Connection; mapped to
  // a certain process or thread, depending  on the run-time model  used by the
  // Server:
  class StratMonitor: public Wt::WApplication
  {
  private:
    //-----------------------------------------------------------------------//
    // Data Flds:                                                            //
    //-----------------------------------------------------------------------//
    unsigned                                    m_userID;    // For RiskMgr
    std::string                                 m_title;
    // Paths to the Strategy INI file:
    std::string                                 m_stratINI;

    // The WTimer ptr is shared-owned, all raw ptrs are just aliases:
    std::shared_ptr<Wt::WTimer>                 m_timer;
    Wt::WStackedWidget*                         m_wStack;
    Wt::WContainerWidget*                       m_paramsEditor;
    Wt::WText*                                  m_text0;
    Wt::WContainerWidget*                       m_runTimeCtls;
    Wt::WText*                                  m_text1;

    // Tables for displaying USD Risks and NAV (also aliases):
    Wt::WTable*                                 m_instrRisksTable;
    Wt::WTable*                                 m_assetRisksTable;
    Wt::WTable*                                 m_summaryTable;

    // SecDefsMgr and the RiskMgr from which most of observable data come --
    // one for all sessions. XXX: If an instance of "SecDefsMgr" is not obt-
    // ained, there would be dangling pts coming from RiskMgr objs!
    static SecDefsMgr*                          s_secDefsMgr;
    static RiskMgr*                             s_riskMgr;

    // PID of the running Strategy (or 0 if not currently running):
    static int                                  s_stratPID;

    // Strategy termination:
    static int                                  s_stratStops;
    static bool                                 s_stratExited;
    static int                                  s_stratRC;

    // Msg to be displayed in all Sessions:
    enum class StatusT { Info, Warning, Error };
    static std::string                          s_globalMsg;
    static StatusT                              s_globalStatus;

    // All instances of this class (corresponding to Sessions with connected
    // clients):
    static std::vector<StratMonitor*>           s_sessions;

    // Strategy Control Params.
    // NB: The following restrictions apply:
    // (*) "m_params" is the main params instance; it is only allowed to change
    //     their string vals, but not the over-all layout or keys,  because the
    //     Keys and Vals are exported to Lambda call-backs as Refs;
    // (*) "m_paramsBak" is a back-up copy which can be completely over-written
    //     at any time (eg when "m_params" are saved); it is only used to rest-
    //     ore the orig params when the "Discard" button is pressed:
    //
    mutable boost::property_tree::ptree         m_params;
    mutable boost::property_tree::ptree         m_paramsBak;
    mutable bool                                m_paramsSaved;
    mutable boost::property_tree::ptree const*  m_paramsExt;

    // The Default Ctor is deleted:
    StratMonitor() = delete;

  public:
    //-----------------------------------------------------------------------//
    // Non-Default Ctor, Dtor:                                               //
    //-----------------------------------------------------------------------//
    StratMonitor(Wt::WEnvironment const& a_env);

    ~StratMonitor() noexcept override;

  private:
    //-----------------------------------------------------------------------//
    // Internal Utils:                                                       //
    //-----------------------------------------------------------------------//
    void StartButtonPressed();

    void ResetEditors(boost::property_tree::ptree const* a_new_params);
    bool DoMgmtSocket();

    void StartStrategy();
    void StopStrategy();

    // "SigChldAction":
    // Invoked asynchronously (signal handler):
    //
    static void SigChldAction
    (
      int         a_signum,
      siginfo_t*  a_siginfo,
      void*       // unused
    );

    // "StrategyTerminated":
    // Invoked synchronously after "SigChldAction" sets "s_stratExited":
    //
    void StrategyTerminated();

    // "DisplayMsg":
    // W: Widget where the msg is to be displayed (currently 0 or 1)
    // S: Status (as below):
    //
    template<int W, StatusT S>
    void DisplayMsg(std::string const& a_msg);

    void DisplayGlobalMsg();

    // "GetStatusLine":
    // Returns "true" iff the StatusLine was indeed found and displayed:
    //
    std::string GetStatusLine() const;

    // "ObserveRisks": Invoked by Timer:
    //
    void ObserveRisks();

//  void StratDataError(int a_ret, int a_errno);
  };
} // End namespace MAQUETTE
