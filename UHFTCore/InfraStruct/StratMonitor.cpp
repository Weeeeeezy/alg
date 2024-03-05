// vim:ts=2:et
//===========================================================================//
//                       "InfraStruct/StratMonitor.cpp":                     //
//===========================================================================//
#include "InfraStruct/StratMonitor.h"
#include "Basis/TimeValUtils.hpp"
#include "Basis/ConfigUtils.hpp"
#include "Basis/IOUtils.hpp"
#include <utxx/compiler_hints.hpp>
#include <utxx/error.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <filesystem>
#include <unistd.h>
#include <signal.h>
#include <string>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <clocale>
#include <cassert>
#include <sys/types.h>
#include <sys/wait.h>
#include <Wt/WBootstrapTheme.h>
#include <Wt/WPanel.h>
#include <Wt/WLineEdit.h>
#include <Wt/WPushButton.h>

#define  MU std::make_unique
#define  MS std::make_shared

using namespace std;

namespace MAQUETTE
{
  //=========================================================================//
  // Static Data: (Common for All StratMonitor Sessions):                    //
  //=========================================================================//
  SecDefsMgr*                 StratMonitor::s_secDefsMgr   = nullptr;
  RiskMgr*                    StratMonitor::s_riskMgr      = nullptr;
  int                         StratMonitor::s_stratPID     = 0;
  int                         StratMonitor::s_stratStops   = 0;
  bool                        StratMonitor::s_stratExited  = false;
  int                         StratMonitor::s_stratRC      = 0;
  string                      StratMonitor::s_globalMsg;
  StratMonitor::StatusT       StratMonitor::s_globalStatus =
                              StratMonitor::StatusT::Info;
  vector<StratMonitor*>       StratMonitor::s_sessions;

  //=========================================================================//
  // Non-Default Ctor:                                                       //
  //=========================================================================//
  StratMonitor::StratMonitor(Wt::WEnvironment const& a_env)
  : Wt::WApplication  (a_env),
    m_userID          (0),
    m_title           (),
    m_stratINI        (),
    m_timer           (),
    m_wStack          (nullptr),
    m_paramsEditor    (nullptr),
    m_text0           (nullptr),
    m_runTimeCtls     (nullptr),
    m_text1           (nullptr),
    m_instrRisksTable (nullptr),
    m_assetRisksTable (nullptr),
    m_summaryTable    (nullptr),
    m_params          (),
    m_paramsBak       (),
    m_paramsSaved     (true),   // Because params are not updated yet
    m_paramsExt       (nullptr)
  {
    //-----------------------------------------------------------------------//
    // Top-Level Setup:                                                      //
    //-----------------------------------------------------------------------//
    // Set the locale for proper formatting of all numeric outputs:
    setlocale(LC_NUMERIC, "");

    // Attach this instance to "s_sessions" (should NOT be there yet):
    assert(find(s_sessions.cbegin(), s_sessions.cend(), this) ==
           s_sessions.cend());
    s_sessions.push_back(this);

    // Get the Application Properties:
    if (utxx::unlikely
       (!readConfigurationProperty("strategyINI", m_stratINI)))
      throw utxx::badarg_error
            ("StratMonitor::Ctor: strategyINI not set in XML config");

    // Attach the SIGCHLD Handler:
    struct sigaction sa;
    sa.sa_sigaction     = SigChldAction;
    sa.sa_flags         = SA_SIGINFO;

    if (utxx::unlikely(sigaction(SIGCHLD, &sa, nullptr) != 0))
      IO::SystemError(-1, "StratMonitor::Ctor: Cannot set up SIGCHLD handler");

    // Load the Params:
    boost::property_tree::ini_parser::read_ini(m_stratINI, m_params);
    m_paramsBak = m_params;

    // Set the UserID (XXX: Currently NOT editable at run time):
    m_userID    = m_params.get<unsigned>("Main.UserID", 0);

    // Set the Title:
    setTitle(m_params.get<string>("Main.Title"));

    // Set the BootStrap3 Theme:
    std::shared_ptr  <Wt::WBootstrapTheme>  theme = MS<Wt::WBootstrapTheme>();
    theme->setVersion(Wt::BootstrapVersion::v3);
    setTheme(theme);

    // Create a Stacked Widget which will be used to display either:
    // (*) the Config.INI    page when the Strategy is not running;
    // (*) the Run-Time Info page when the Stratrgy is     running:
    // It is directly attached to the root():
    //
    m_wStack = root()->addWidget(MU<Wt::WStackedWidget>());

    //-----------------------------------------------------------------------//
    // Widget #0: Editing the Config.ini file (when Strategy not running):   //
    //-----------------------------------------------------------------------//
    {
      // Attach Widget #0 to the Stack:
      // It is a "WContanerWidgets";  each section of the Config.ini file is a
      // CollapsiblePanel attached to this Container:
      m_paramsEditor = m_wStack->addWidget(MU<Wt::WContainerWidget>());

      // Put the "SaveConfig" and "START"/"STOP" buttons on top:
      Wt::WTable*      buttonsTable  =
        m_paramsEditor->addWidget(MU<Wt::WTable>());

      buttonsTable->rowAt(0)->setHeight(Wt::WLength("5ex"));
      buttonsTable->rowAt(1)->setHeight(Wt::WLength("5ex"));
      buttonsTable->rowAt(2)->setHeight(Wt::WLength("5ex"));

      Wt::WPushButton* startButton   =
        buttonsTable->elementAt(0,0)->addWidget(MU<Wt::WPushButton>
        ("START THE STRATEGY"));

      Wt::WPushButton* stopButton    =
        buttonsTable->elementAt(0,1)->addWidget(MU<Wt::WPushButton>
        ("STOP THE STRATEGY"));

      m_text0                        =
        buttonsTable->elementAt(0,2)->addWidget(MU<Wt::WText>());

      Wt::WPushButton* statusButton  =
        buttonsTable->elementAt(1,0)->addWidget(MU<Wt::WPushButton>
        ("Strategy Status"));

      Wt::WPushButton* saveButton    =
        buttonsTable->elementAt(2,0)->addWidget(MU<Wt::WPushButton>
        ("SAVE Config Changes"));

      Wt::WPushButton* discardButton =
        buttonsTable->elementAt(2,1)->addWidget(MU<Wt::WPushButton>
        ("DISCARD Config Changes"));

      //---------------------------------------------------------------------//
      // "START" and "STOP" Buttons:                                         //
      //---------------------------------------------------------------------//
      startButton->clicked().connect
        ([this]()->void { this->StartButtonPressed(); });

      stopButton->clicked().connect
        ([this]()->void { StopStrategy(); });

      //---------------------------------------------------------------------//
      // "Strategy Status" Button:                                           //
      //---------------------------------------------------------------------//
      // Switches to Widget #1 without starting the Strategy:
      //
      statusButton->clicked().connect
        ([this]()->void { this->m_wStack->setCurrentIndex(1); });

      //---------------------------------------------------------------------//
      // "Save" Button:                                                      //
      //---------------------------------------------------------------------//
      // Save the (possibly edited) "m_params" in the same file:
      saveButton->clicked().connect
      (
        [this]()->void
        {
          boost::property_tree::ini_parser::write_ini
            (this->m_stratINI, m_params);

          for (auto sess: s_sessions)
          {
            assert(sess != nullptr);
            if (sess == this)
              sess->ResetEditors(nullptr);   // (Re)set only Keys Colors
            else
              // Need to propagate "m_params" to that "sess"; but in order to
              // get it re-displayed, cannot call "ResetEditors" now; rather,
              // set a ptr for a future update at a Timer tick:
              sess->m_paramsExt = &m_params;
          }
          this->m_paramsBak   = m_params;
          this->m_paramsSaved = true;        // Because they are now saved
        }
      );

      //---------------------------------------------------------------------//
      // "Discard" Button:                                                   //
      //---------------------------------------------------------------------//
      discardButton->clicked().connect
      (
        [this]()->void
        {
          if (this->m_paramsSaved)
            return;                              // No updates -- nothing to do

          // We only need to reset THIS session:
          this->ResetEditors(&m_paramsBak);      // Reset Keys Colors, and Data
          this->m_paramsSaved = true;            // Because they are now reset
        }
      );

      //=====================================================================//
      // Go through all Params Sections, add them as Panels to Widget #0:    //
      //=====================================================================//
      for (auto& section: m_params)
      {
        string                const& sectionName = section.first;
        boost::property_tree::ptree& sectionTree = section.second;
        // NB: Non-Const Ref!

        // Create a CollapsiblePanel for this Section. All Panels except the
        // top one, are initially collapsed:
        Wt::WPanel* sectionPanel =  m_paramsEditor->addWidget(MU<Wt::WPanel>());
        sectionPanel->setTitle      (sectionName);
        sectionPanel->setCollapsible(true);

        // Initially, collapse all Panels except the "Main" one:
        bool isMain = (sectionName == "Main");
        if (!isMain)
          sectionPanel->collapse();

        // All entries are placed in a 2-column Table (for better alignment):
        std::unique_ptr<Wt::WTable>    sectionTable = MU<Wt::WTable>();
        sectionPanel->setCentralWidget(std::move(sectionTable));

        sectionTable->columnAt(0)->setWidth(Wt::WLength("18em"));
        int row = 0;
        for (auto& node: sectionTree)
        {
          string const& nodeName = node.first;
          string&       nodeData = node.second.data(); // Non-Const Ref!

          if (isMain && nodeName == "Title")
            // Skip it -- the Title has already been set:
            continue;

          // Insert a row into the "sectionTable". XXX: Currently, because the
          // contents of Boost Property Tree is untyped, we do not impose  any
          // input filters here -- all data are just strings; the user is res-
          // ponsible for the correct input:
          //
          Wt::WText* key =
            sectionTable->elementAt(row, 0)->addWidget(MU<Wt::WText>(nodeName));

          if (sectionName == "Main" && nodeName == "Env")
            // This params entry is NOT editable to avoid conflicts with the
            // RiskMgr setup:
            sectionTable->elementAt(row, 1)->addWidget(MU<Wt::WText>(nodeData));
          else
          {
            // Otherwise, the entry is editable:
            Wt::WLineEdit* editor =
              sectionTable->elementAt(row, 1)->addWidget(MU<Wt::WLineEdit>
              (nodeData));
            editor->setTextSize(32);

            // Text editing call-back:
            editor->changed().connect
            (
              [this,key,editor,&nodeName,&nodeData]()->void
              {
                // Install the updated text back into "m_params":
                nodeData = editor->text().toUTF8();

                // Mark this text as updated. XXX: We cannot mark the value
                // for technical reasons -- so mark the key:
                key->setText
                  ("<b><font color=\"red\">" + nodeName + "</font></b>");

                // Updated params are not saved yet:
                this->m_paramsSaved = false;
              }
            );
          }
          ++row;
        }
      }
      // If the Strategy is not currently running, make Widget #0 visible;
      // otherwise, Widget #1:
      bool isRunning = (s_stratPID > 0);
      m_wStack->setCurrentIndex(isRunning);
    }
    // End of Widget #0

    //=======================================================================//
    // Widget #1: Controlling the running Strategy:                          //
    //=======================================================================//
    {
      // Attach Widget #1 to the Stack:
      m_runTimeCtls = m_wStack->addWidget(MU<Wt::WContainerWidget>());

      // Put the "START/STOP" buttons on top:
      Wt::WTable* buttonsTable = m_runTimeCtls->addWidget(MU<Wt::WTable>());

      buttonsTable ->rowAt(0)->setHeight(Wt::WLength("5ex"));
      buttonsTable ->rowAt(1)->setHeight(Wt::WLength("5ex"));

      Wt::WPushButton* startButton =
        buttonsTable ->elementAt(0,0)->addWidget(MU<Wt::WPushButton>
        ("START THE STRATEGY"));

      Wt::WPushButton* stopButton  =
        buttonsTable ->elementAt(0,1)->addWidget(MU<Wt::WPushButton>
        ("STOP THE STRATEGY"));

      m_text1                      =
        buttonsTable ->elementAt(0,2)->addWidget(MU<Wt::WText>());

      Wt::WPushButton* confButton  =
        buttonsTable ->elementAt(1,0)->addWidget(MU<Wt::WPushButton>
        ("Strategy Config"));

      //---------------------------------------------------------------------//
      // "START" and "STOP" Buttons:                                         //
      //---------------------------------------------------------------------//
      startButton->clicked().connect
        ([this]()->void { this->StartButtonPressed(); });

      stopButton->clicked().connect
        ([this]()->void { StopStrategy(); });

      //---------------------------------------------------------------------//
      // "Strategy Config" Button:                                           //
      //---------------------------------------------------------------------//
      // Return to Widget #0 without stopping the Strategy:
      confButton->clicked().connect
        ([this]()->void { m_wStack->setCurrentIndex(0); });

      //---------------------------------------------------------------------//
      // InstrRisks and AssetRisks Tables:                                   //
      //---------------------------------------------------------------------//
      // They are empty as yet:
      m_instrRisksTable = m_runTimeCtls->addWidget(MU<Wt::WTable>());
      m_instrRisksTable->toggleStyleClass("table-bordered", true);
      m_instrRisksTable->toggleStyleClass("table-striped",  true);

      m_assetRisksTable = m_runTimeCtls->addWidget(MU<Wt::WTable>());
      m_assetRisksTable->toggleStyleClass("table-bordered", true);
      m_assetRisksTable->toggleStyleClass("table-striped",  true);

      //---------------------------------------------------------------------//
      // Summary (Risks and NAV) Table:                                      //
      //---------------------------------------------------------------------//
      m_summaryTable  = m_runTimeCtls->addWidget(MU<Wt::WTable>());
      m_summaryTable->setHeaderCount(0);

      m_summaryTable->elementAt(0,0)->addWidget
        (MU<Wt::WText>("Total Active Ords (RFC): "));
      m_summaryTable->elementAt(0,1)->addWidget(MU<Wt::WText>("0.00"));

      m_summaryTable->elementAt(1,0)->addWidget
        (MU<Wt::WText>("Total Asset Risks (RFC): "));
      m_summaryTable->elementAt(1,1)->addWidget(MU<Wt::WText>("0.00"));

      m_summaryTable->elementAt(2,0)->addWidget
        (MU<Wt::WText>("<b>Total NAV (RFC): </b>"));
      m_summaryTable->elementAt(2,1)->addWidget(MU<Wt::WText>("0.00"));
    }

    //=======================================================================//
    // Periodic Timer:                                                       //
    //=======================================================================//
    // Its primary purpose is to detect Strategy termination events (which are
    // only marked by the asynchronous signal handler -- no action can be invo-
    // ked from that handler, for safety reasons):
    // XXX: It looks like in WT 4.0, WTimer does not need to be attached to the
    // root() container:
    m_timer = MS<Wt::WTimer>  ();        // Attach Timer to the Root Widget
    m_timer->setInterval      (std::chrono::milliseconds(500));
    m_timer->setSingleShot    (false);
    m_timer->timeout().connect
    (
      [this]()->void
      {
        // Handle Strategy Terminations:
        if (utxx::unlikely(s_stratExited))
          StrategyTerminated();

        // Display Global Msg (if any):
        DisplayGlobalMsg();

        // Handle Strategy Risks etc:
        ObserveRisks();

        // If the Configs were externally-updated, re-display them:
        if (utxx::unlikely(this->m_paramsExt != nullptr))
        {
          this->ResetEditors   (this->m_paramsExt);
          this->m_paramsBak = *(this->m_paramsExt);
          this->m_paramsExt = nullptr;
        }
      }
    );
    m_timer->start();
  }

  //=========================================================================//
  // Dtor:                                                                   //
  //=========================================================================//
  StratMonitor::~StratMonitor() noexcept
  {
    // Do not allow any exceptions to propagate out of the Dtor:
    try
    {
      // XXX:   As the very least, we need to remove the curr instance from the
      // "s_sessions":
      // FIXME: Potential race cond here -- need a lock:
      //
      auto it = find(s_sessions.begin(), s_sessions.end(), this);
      if (utxx::likely  (it != s_sessions.end()))
        s_sessions.erase(it);

      // Also, it is probably a good idea to stop the Timer before it is destrd:
      m_timer->stop();
    }
    catch(...){}
  }

  //=========================================================================//
  // "StartButtonPressed":                                                   //
  //=========================================================================//
  void StratMonitor::StartButtonPressed()
  {
    if (!m_paramsSaved)
    {
      // Cannot start the Strategy if there are unsaved Config changes (msg
      // for the Originating Session only):
      DisplayMsg<0, StatusT::Error>
        ("You must Save or Discard config changes first");
      return;
    }

    // Check that the Startegy is not currently running (it should not, bec-
    // ause otherwise the "Start" button would not be visible -- but be on a
    // safe side!):
    if (utxx::unlikely(s_stratPID > 0))
    {
      // Msg is for the Originating Session only:
      DisplayMsg<0, StatusT::Error>
        ("Strategy is already running: PID=" + to_string(s_stratPID));
      return;
    }

    // If OK: Start the Strategy:
    StartStrategy();

    // Switch to Widget #1:
    this->m_wStack->setCurrentIndex(1);
  }

  //=========================================================================//
  // "ResetEditors":                                                         //
  //=========================================================================//
  inline void StratMonitor::ResetEditors
    (boost::property_tree::ptree const* a_new_params)
  {
    // NB: "a_new_params" may be NULL!
    // Clean up the previous text (if any):
    m_text0->setText("");

    // For all Config Panels and all Keys, remove coloring from the Keys
    // (which highlighted updated but not-yet-saved params) :
    // NB: Skip child #0 -- it's a ButtonsTable, not a Panel:
    // Traverse "m_params" in parallel with the Widgets -- we  only need
    // "m_params" structure (not data!) here:
    //
    int  nPanels   = m_paramsEditor->count();
    auto sectionIt = m_params.begin();

    for (int p = 1; p < nPanels; ++p, ++sectionIt)
    {
      // Get the Params Section:
      string const&                sectionName = sectionIt->first;
      boost::property_tree::ptree& sectionTree = sectionIt->second;

      // Get the Section Panel:
      Wt::WPanel* sectionPanel =
        dynamic_cast<Wt::WPanel*>(m_paramsEditor->widget(p));
      assert (sectionPanel != nullptr);

      // Get the contents as a Table:
      Wt::WTable* sectionTable =
        dynamic_cast<Wt::WTable*>(sectionPanel->centralWidget());
      assert (sectionTable != nullptr);

      // For all Rows of that Table:
      // (In parallel, for all Nodes in the corresp Section):
      int  nRows  = sectionTable->rowCount();
      auto nodeIt = sectionTree.begin();

      for (int r = 0; r < nRows; )
      {
        // The cell @ (r,0) is a Key (should contain only 1 Widget, which is
        // a WText):
        Wt::WText* key =
          dynamic_cast<Wt::WText*>(sectionTable->elementAt(r, 0)->widget(0));
        assert(key != nullptr);

        // Get the original "nodeName" and install it in "key" (but skip the
        // "Title"):
        string const& nodeName = nodeIt->first;

        if (sectionName == "Main" && nodeName == "Title")
        {
          // Skip this Node but not the Row -- it will be processed with the
          // next Node:
          ++nodeIt;
          continue;
        }

        // The following, in particular, clears red highlighting:
        if (utxx::unlikely(key->text() != nodeName))
          key->setText(nodeName);

        if (a_new_params != nullptr)
        {
          // Also install the value of this Param into both "m_params" (if
          // different from "a_new_params") and the Editor:
          Wt::WLineEdit* editor =
            dynamic_cast<Wt::WLineEdit*>
            (sectionTable->elementAt(r, 1)->widget(0));

          // NB: "editor" can be NULL if the corresp fld is read-only. Also,
          // ignore the "Title" mode:
          if (utxx::likely(editor != nullptr))
          {
            // Get the value from "a_new_params": NB: No ref here:
            string newData =
              a_new_params->get<string>(sectionName + "." + nodeName);

            // Put "newData" into "m_params":
            if (nodeIt->second.data() != newData)
              nodeIt->second.data() = newData;

            // Put "newData" into the Editor:
            if (editor->text() != newData)
              editor->setText(newData);
          }
        }
        // Increment the Row# (this is skipped if the "Title" was skipped):
        // Also go for the next Node:
        ++r;
        ++nodeIt;
      }
    }
  }

  //=========================================================================//
  // "StartStrategy":                                                        //
  //=========================================================================//
  void StratMonitor::StartStrategy()
  {
    //-----------------------------------------------------------------------//
    // Construct the CmdLine Params and Env for the Strategy Process:        //
    //-----------------------------------------------------------------------//
    vector<char const*> argv;
    vector<char const*> envp;

    // The command and params depend on whether we link against LibVMA at run-
    // time to achieve kernel by-pass. In the latter case, "RunVMA.sh" script
    // is used to launch the application:
    bool useLibVMA   =  m_params.get<bool>("Exec.UseLibVMA");
    string              topEXE;
    string              stratEXE = m_params.get<string>("Exec.StrategyEXE");

    // Path to the Startegy Executable (argv[0]) -- it may also be the script:
    if (useLibVMA)
    {
      topEXE = "/opt/bin/RunVMA.sh";
      argv.push_back(topEXE.data());
    }
    else
      topEXE = stratEXE;

    // Then in any case the Strategy Executable itself and the INI file come:
    argv.push_back(stratEXE.data());
    argv.push_back(m_stratINI.data());
    argv.push_back(nullptr);      // Terminating NULL

    //-----------------------------------------------------------------------//
    // "fork" and "exec":                                                    //
    //-----------------------------------------------------------------------//
    s_stratPID = fork();

    if (s_stratPID == 0)
    {
      // This is a child process:
      string statusFile = m_params.get<string>("Exec.StatusLine", "");
      if (!statusFile.empty())
      {
        // Re-direct "stdout" to "statusFile":
        (void) freopen(statusFile.data(), "w", stdout);

        // Point "stderr" to the same file descr as that of "stdout":
        (void) dup2(fileno(stdout), fileno(stderr));
      }
      // In any case, close "stdin" -- it will not be required:
      fclose (stdin);

      // Do "exec" -- launch the Strategy. XXX: For some reason, "execve" re-
      // quires non-const ptrs to "argv" and "envp" strings:
      int rc = execve
      (
        topEXE.data(),
        const_cast<char* const*>(argv.data()),
        const_cast<char* const*>(envp.data())
      );
      if (utxx::unlikely(rc < 0))
        // FIXME: Need an explicit error msg here!
        exit(errno);
    }
    else
    {
      // This is a parent (StratMonitor) process:
      // To be on a safe side, reset the flags:
      s_stratExited = false;
      s_stratRC     = 0;
      s_stratStops  = 0;

      // Display the start msg for all Sessions with the next Timer tick:
      s_globalMsg    = "Strategy started: PID=" + to_string(s_stratPID);
      s_globalStatus = StatusT::Info;
    }
  }

  //=========================================================================//
  // "StopStrategy":                                                         //
  //=========================================================================//
  void StratMonitor::StopStrategy()
  {
    // XXX: For now, graceful termination of a Strategy is achieved  by sending
    // a SIGINT to it. If, however, this is a repeated "Stop" request, then send
    // a sure SIGKILL signal:
    //
    if (utxx::likely(s_stratPID > 0))
    {
      // In the normal mode, send SIGINT to the Strategy process identified
      // by "s_stratPID";
      // but if this is not the 1st attempt, send SIGKILL to all child pro-
      // cesses:
      int sig  = (s_stratStops == 0) ? SIGINT : SIGKILL;

      int rc   = kill(s_stratPID, sig);
      if (utxx::unlikely(rc < 0 && errno == ESRCH))
      {
        // Unsuccessful signal:
        string msg =
          "Cannot stop the Strategy: PID=" +  to_string(s_stratPID)         +
          (errno == ESRCH ? " does not exist" : " insufficient permissions");

        // NB: "Insufficient permissions" case is impossible, and is conside-
        // red for the sake of completeness only!
        // This msg is displayed in the Originating Session only:
        DisplayMsg<0, StatusT::Error>(msg);
        DisplayMsg<1, StatusT::Error>(msg);

        // Reset "s_stratPID" because it is not valid anyway, and because of
        // this, reset "s_stratStops" as well:
        s_stratPID   = 0;
        s_stratStops = 0;
      }
      else
      {
        // Successful signal: Increment the "Stops Counter":
        ++s_stratStops;

        // This msg is displayed in ALL Sessions:
        s_globalMsg    =
          "Stopping: Signal=" + to_string(sig) + " sent to PID=" +
          to_string(s_stratPID);
        s_globalStatus = StatusT::Info;

        // Furthermore, if it was a SIGKILL, wait synchronously for terminat-
        // ion of the Strategy:
        if (sig == SIGKILL)
        {
          int dummy = 0;
          if (utxx::likely(waitpid(s_stratPID, &dummy, 0) == s_stratPID))
          {
            // Override the previously-saved global msg with the following:
            s_globalMsg    = "Strategy killed: PID=" + to_string(s_stratPID);
            s_globalStatus = StatusT::Warning;

            // After that, assume we can reset "s_strat{PID,Stops}":
            s_stratPID   = 0;
            s_stratStops = 0;
          }
        }
      }
    }
    else
    {
      // This msg is for the Originating Session only:
      string msg = "Strategy is not running, nothing to stop";
      DisplayMsg<0, StatusT::Error>(msg);
      DisplayMsg<1, StatusT::Error>(msg);
    }
  }

  //=========================================================================//
  // "SigChldAction":                                                        //
  //=========================================================================//
  // Invoked asynchronously (signal handler):
  //
  void StratMonitor::SigChldAction
  (
    int         a_signum,
    siginfo_t*  a_siginfo,
    void*       // unused
  )
  {
    assert(a_siginfo != nullptr);

    // This signal  *should*  be a SIGCHLD, and PID should be same as we have
    // started previously:
    if (utxx::unlikely
       (a_signum != SIGCHLD ||
       (s_stratPID != 0 && a_siginfo->si_pid != s_stratPID)))
      cerr
        << "StratMonitor::SigChldAction: WARNING: UnExpected Signal="
        << a_signum      << " and/or ChildPID=" << a_siginfo->si_pid
        << ": Expected=" << s_stratPID          << endl;

    // XXX: Currently also ignore extraneous signals and the cases when the
    // child did not exit but received a STOP or CONT signal (this is *very*
    // unlikely unless done manually):
    if (utxx::unlikely
       (a_signum != SIGCHLD || a_siginfo->si_code != CLD_EXITED))
      return;

    // Now: The Strategy has really exited: "wait" for it so it does not be-
    // come a "zomby" (it's OK to do so from within the Signal Handler):
    // NB: Use the real PID as reported by "siginfo":
    int unused = 0;
    (void) waitpid(a_siginfo->si_pid, &unused, WNOHANG);

    // Record the status and return to the main program:
    s_stratPID    = 0;
    s_stratExited = true;
    s_stratRC     = a_siginfo->si_status;
    // XXX: Check that "unused" status is equal to "s_stratRC"...
  }

  //=========================================================================//
  // "StrategyTerminated":                                                   //
  //=========================================================================//
  // Invoked synchronously after "SigChldAction" sets "s_stratExited".
  // NB: This flag CANNOT be reset by "StrategyTerminated", because it needs to
  // be consumed by ALL Web sessions; thus, multiple invocations of this method
  // (even within one session -- at subsequent timer ticks) can occur.  This is
  // in general OK:
  //
  void StratMonitor::StrategyTerminated()
  {
    // Reset the flag and the Stops Count:
    assert(s_stratPID == 0 && s_stratExited);
    s_stratExited = false;
    s_stratStops  = 0;

    // Display the StatusLine; if unsuccessful, display a generic msg. Do
    // this for ALL Sessions:
    s_globalMsg    = GetStatusLine();
    s_globalStatus = StatusT::Info;

    // Status depends on waht we got in the StatusLine:
    if (strstr(s_globalMsg.data(), "ERROR")     != nullptr ||
        strstr(s_globalMsg.data(), "Error")     != nullptr ||
        strstr(s_globalMsg.data(), "error")     != nullptr ||
        strstr(s_globalMsg.data(), "EXCEPTION") != nullptr ||
        strstr(s_globalMsg.data(), "Exception") != nullptr ||
        strstr(s_globalMsg.data(), "exception") != nullptr)
      s_globalStatus = StatusT::Error;
    else
    if (strstr(s_globalMsg.data(), "WARN" )     != nullptr ||
        strstr(s_globalMsg.data(), "Warn" )     != nullptr ||
        strstr(s_globalMsg.data(), "warn" )     != nullptr)
      s_globalStatus = StatusT::Warning;

    if (s_globalMsg.empty())
      s_globalMsg    =
        (s_stratRC == 0)
        ? string("Strategy Terminated Successfully")
        : "Strategy Terminated with RC=" + to_string(s_stratRC);

    // In any case (irresp to the Msg): If RC is not 0, it is an error:
    if (s_stratRC != 0)
      s_globalStatus = StatusT::Error;

    // We can now reset the StratRC as well:
    s_stratRC = 0;
  }

  //=========================================================================//
  // "StratMonitor::DisplayMsg":                                             //
  //=========================================================================//
  // W: Widget where the msg is to be displayed (currently 0 or 1)
  // S: Status (as below):
  //
  template<int W, StratMonitor::StatusT S>
  inline void StratMonitor::DisplayMsg(string const& a_msg)
  {
    (W ? m_text1 : m_text0)->setText
      (string("<b><font color=\"") +
      (S == StatusT::Info    ? "green\">"        :
       S == StatusT::Warning ? "orange\">WARNING: " : "red\">ERROR: ") +
      a_msg + "</font></b>");
  }

  //=========================================================================//
  // "DisplayGlobalMsg":                                                     //
  //=========================================================================//
  void StratMonitor::DisplayGlobalMsg()
  {
    switch (s_globalStatus)
    {
    case StatusT::Info:
      DisplayMsg<0, StatusT::Info>   (s_globalMsg);
      DisplayMsg<1, StatusT::Info>   (s_globalMsg);
      break;
    case StatusT::Warning:
      DisplayMsg<0, StatusT::Warning>(s_globalMsg);
      DisplayMsg<1, StatusT::Warning>(s_globalMsg);
      break;
    case StatusT::Error:
      DisplayMsg<0, StatusT::Error>  (s_globalMsg);
      DisplayMsg<1, StatusT::Error>  (s_globalMsg);
      break;
    default:
      __builtin_unreachable();
    }
  }

  //=========================================================================//
  // "GetStatusLine":                                                        //
  //=========================================================================//
  // XXX: This should be done in a better way rather than by reading the status
  // line from a file:
  // Returns "true" iff the StatusLine was indeed found and displayed:
  //
  string StratMonitor::GetStatusLine() const
  {
    // Get the FileName for StatusLine:
    string statusFile = m_params.get<string>("Exec.StatusLine", "");
    if (statusFile.empty())
      return string();

    // Open the file:
    FILE* f = fopen(statusFile.data(), "r");
    if (f == nullptr)
      return string();

    // Read in the contents (but no more than 1k):
    char   line[1024];
    size_t len = fread(line, 1, sizeof(line), f);
    fclose(f);

    return string(line, len);
  }

  //=========================================================================//
  // "ObserveRisks":                                                         //
  //=========================================================================//
  // Invoked by Timer.
  // FIXME: Multiple issues here incl race conditions wrt RiskMgr:
  //
  void StratMonitor::ObserveRisks()
  {
    //-----------------------------------------------------------------------//
    // If there is no SecDefsMgr instance attached yet, try to do it now:    //
    //-----------------------------------------------------------------------//
    // (Otherwise there would be dangling ptrs in the RiskMgr). NB: Because the
    // SecDefsMgr ShM objects are actually created by Strategies,   our attempt
    // may or may not be successful -- ignore any errors:
    //
    bool isProd = (m_params.get<string>("Main.Env") == "Prod");

    if (utxx::unlikely(s_secDefsMgr == nullptr))
      try
      {
        s_secDefsMgr = SecDefsMgr::GetPersistInstance(isProd, "");
        // If successful:
        assert(s_secDefsMgr != nullptr);
        cerr << "INFO: Successfully Attached to SecDefsMgr" << endl;
      }
      catch (exception const& exc)
      {
        // If still could not attach, cannot proceed in this invocation:
        assert(s_secDefsMgr == nullptr);
        cerr << "WARNING: Could not attach to SecDefsMgr: " << exc.what()
              << endl;
        return;
      }
    assert(s_secDefsMgr != nullptr);
    //-----------------------------------------------------------------------//
    // If there is no RiskMgr instance attached yet, try to do it now:       //
    //-----------------------------------------------------------------------//
    // Similar to SecDefsMgr above:
    if (utxx::unlikely(s_riskMgr == nullptr))
      try
      {
        s_riskMgr =
          // ObserverMode! ResetAll, Logger and DebugLevel are omitted:
          RiskMgr::GetPersistInstance
            (isProd, GetParamsSubTree(m_params, "RiskMgr"), *s_secDefsMgr,
             true);

        // If successful:
        assert(s_riskMgr != nullptr);
        cerr << "INFO: Successfully Attached to RiskMgr" << endl;
      }
      catch (exception const& exc)
      {
        // If still could not attach, cannot proceed in this invocation:
        assert(s_riskMgr == nullptr);
        cerr << "WARNING: Could not attach to RiskMgr: " << exc.what() << endl;
        return;
      }
    // Both SecDefsMgr and RiskMgr should now be available:
    assert(s_secDefsMgr != nullptr && s_riskMgr != nullptr);

    //-----------------------------------------------------------------------//
    // Output the Instr Positions:                                           //
    //-----------------------------------------------------------------------//
    char buff[64];
    auto const& irs = s_riskMgr->GetAllInstrRisks(m_userID);
    int I           = int(irs.size());

    if (utxx::unlikely(m_instrRisksTable->rowCount() != 1 + I))
    {
      // The table may be empty as yet, or may have been resized:
      if (m_instrRisksTable->rowCount() != 0)
        m_instrRisksTable->clear();

      // Install the Header:
      m_instrRisksTable->addStyleClass ("table form-inline");
      m_instrRisksTable->setHeaderCount(1);

      m_instrRisksTable->elementAt(0, 0)->addWidget
        (MU<Wt::WText>("Instrument"));
      m_instrRisksTable->elementAt(0, 1)->addWidget
        (MU<Wt::WText>("Position"  ));
      m_instrRisksTable->elementAt(0, 2)->addWidget
        (MU<Wt::WText>("Active Ords Size"));
      m_instrRisksTable->elementAt(0, 3)->addWidget
        (MU<Wt::WText>("Total Ords Count"));

      // Fill in the Instr Data:
      int o = 1;
      for (auto const& irp: irs)
      {
        // Src and Target:
        InstrRisks const& ir = irp.second;
        assert (o <= I);

        // Instrument:
        m_instrRisksTable->elementAt(o, 0)->addWidget
          (MU<Wt::WText>(ir.m_instr->m_FullName.data()));

        // Position:
        sprintf(buff, "%'.2f", double(ir.m_posA));
        m_instrRisksTable->elementAt(o, 1)->addWidget(MU<Wt::WText>(buff));

        // Size (in units of Asset A) of currently-active Orders:
        sprintf(buff, "%'.2f", double(ir.m_activeOrdsSzA));
        m_instrRisksTable->elementAt(o, 2)->addWidget(MU<Wt::WText>(buff));

        // Orders Count:
        sprintf(buff, "%'ld", ir.m_ordsCount);
        m_instrRisksTable->elementAt(o, 3)->addWidget(MU<Wt::WText>(buff));
        ++o;
      }
    }
    else
    {
      // The number of rows is unchanged, so we assume XXX the order of entries
      // is unchanged as well:
      // Fill in Instrs Risks:
      int o = 1;
      for (auto const& irp: irs)
      {
        // Src and Target:
        InstrRisks const& ir = irp.second;
        assert(o <= I);

        // Position:
        Wt::WText* posTxt  =
          dynamic_cast<Wt::WText*>
          (m_instrRisksTable->elementAt(o, 1)->widget(0));
        assert (posTxt != nullptr);
        sprintf(buff, "%'.2f", double(ir.m_posA));
        posTxt->setText(buff);

        // Active Orders Size:
        Wt::WText* actTxt =
          dynamic_cast<Wt::WText*>
          (m_instrRisksTable->elementAt(o, 2)->widget(0));
        assert (actTxt != nullptr);
        sprintf(buff, "%'.2f", double(ir.m_activeOrdsSzA));
        actTxt->setText(buff);

        // Orders Count:
        Wt::WText* ordsTxt =
          dynamic_cast<Wt::WText*>
          (m_instrRisksTable->elementAt(o, 3)->widget(0));
        assert (ordsTxt != nullptr);
        sprintf(buff, "%'ld", ir.m_ordsCount);
        ordsTxt->setText(buff);
        ++o;
      }
    }
    //-----------------------------------------------------------------------//
    // Output the Asset Positions:                                           //
    //-----------------------------------------------------------------------//
    auto const& ars = s_riskMgr->GetAllAssetRisks(m_userID);
    int A = int(ars.size());

    if (utxx::unlikely(m_assetRisksTable->rowCount() != 1 + A))
    {
      // The table may be empty as yet, or may have been resized:
      if (m_assetRisksTable->rowCount() != 0)
        m_assetRisksTable->clear();

      // Install the Header:
      m_assetRisksTable->addStyleClass ("table form-inline");
      m_assetRisksTable->setHeaderCount(1);

      m_assetRisksTable->elementAt(0, 0)->addWidget(MU<Wt::WText>("Asset"));
      m_assetRisksTable->elementAt(0, 1)->addWidget(MU<Wt::WText>("SettlDate"));
      m_assetRisksTable->elementAt(0, 2)->addWidget(MU<Wt::WText>("Asset Qty"));
      m_assetRisksTable->elementAt(0, 3)->addWidget(MU<Wt::WText>("RFC Val"));

      // Fill in the Asset Positions (per SettlDate):
      int o = 1;
      for (auto const& arp: ars)
      {
        AssetRisks const& ar = arp.second;
        assert (o <= A);

        m_assetRisksTable->elementAt(o, 0)->addWidget
          (MU<Wt::WText>(ar.m_asset.data()));

        if (ar.m_settlDate == 0 || ar.m_settlDate >= EarliestDate)
        {
          int year  =  ar.m_settlDate / 10000;
          int month = (ar.m_settlDate % 10000) / 100;
          int day   =  ar.m_settlDate % 100;
          sprintf(buff, "%04d-%02d-%02d", year, month, day);
        }
        else
          strcpy (buff, "unattributed");

        m_assetRisksTable->elementAt(o, 1)->addWidget(MU<Wt::WText>(buff));
        sprintf(buff, "%'.2f", ar.GetTrdPos());
        m_assetRisksTable->elementAt(o, 2)->addWidget(MU<Wt::WText>(buff));
        sprintf(buff, "%'.2f", ar.GetTrdRFC());
        m_assetRisksTable->elementAt(o, 3)->addWidget(MU<Wt::WText>(buff));
        ++o;
      }
    }
    else
    {
      // The number of rows is unchanged, so we assume XXX the order of entries
      // is unchanged as well:
      // Fill in the Asset Risks:
      int o = 1;
      for (auto const& arp: ars)
      {
        AssetRisks const& ar = arp.second;
        assert(o <= A);

        Wt::WText* assetTxt =
          dynamic_cast<Wt::WText*>
          (m_assetRisksTable->elementAt(o, 2)->widget(0));
        assert(assetTxt != nullptr);

        sprintf(buff, "%'.2f", ar.GetTrdPos());
        assetTxt->setText(buff);

        Wt::WText* usdTxt =
          dynamic_cast<Wt::WText*>
          (m_assetRisksTable->elementAt(o, 3)->widget(0));
        assert(usdTxt != nullptr);

        sprintf(buff, "%'.2f", ar.GetTrdRFC());
        usdTxt->setText(buff);
        ++o;
      }
    }
    //-----------------------------------------------------------------------//
    // Output RFC NAV, Risks and Active Orders:                              //
    //-----------------------------------------------------------------------//
    double   totalActOrdsRFC = s_riskMgr->GetActiveOrdersTotalSizeRFC(m_userID);
    double   totalRiskRFC    = s_riskMgr->GetTotalRiskRFC(m_userID);
    double   totalNAV_RFC    = s_riskMgr->GetTotalNAV_RFC(m_userID);

    // Output the totals into SummaryTable:
    Wt::WTableCell* actCell  = m_summaryTable->elementAt(0, 1);
    Wt::WTableCell* riskCell = m_summaryTable->elementAt(1, 1);
    Wt::WTableCell* nlvCell  = m_summaryTable->elementAt(2, 1);

    assert(actCell != nullptr    && riskCell != nullptr    &&
           nlvCell != nullptr    &&
           actCell->count() == 1 && riskCell->count() == 1 &&
           nlvCell->count() == 1);

    Wt::WText* actTxt  = dynamic_cast<Wt::WText*>(actCell ->widget(0));
    Wt::WText* riskTxt = dynamic_cast<Wt::WText*>(riskCell->widget(0));
    Wt::WText* nlvTxt  = dynamic_cast<Wt::WText*>(nlvCell ->widget(0));
    assert(actTxt != nullptr && riskTxt != nullptr && nlvTxt != nullptr);

    // Total Active Orders:
    sprintf(buff, "%'.2f", totalActOrdsRFC);
    actTxt ->setText(buff);

    // Total Risks: colored in Red if exceeding the max normal level:
    if (utxx::unlikely(totalRiskRFC > s_riskMgr->GetMaxNormalRiskRFC()))
      sprintf(buff, "<b><font color=\"red\">%'.2f</font></b>", totalRiskRFC);
    else
      sprintf(buff, "%'.2f", totalRiskRFC);
    riskTxt->setText(buff);

    // NAV is also colorised:
    if (utxx::likely(totalNAV_RFC > 0.0))
      sprintf(buff, "<b><font color=\"green\">%'.2f</font></b>", totalNAV_RFC);
    else
      sprintf(buff, "<b><font color=\"red\">%'.2f</font></b>",   totalNAV_RFC);
    nlvTxt->setText(buff);
  }
}
// End namespace MAQUETTE
