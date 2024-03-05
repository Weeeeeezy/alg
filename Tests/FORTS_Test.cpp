// vim:ts=2:et
//===========================================================================//
//                            "Tests/FORTS_Test.cpp":                        //
//            Test for Receiving and Parsing FAST Msgs in FORTS              //
//===========================================================================//
#include "Basis/Maths.hpp"
#include "Basis/XXHash.hpp"
#include "Protocols/FAST/Msgs_FORTS.hpp"
#include "Protocols/FAST/Decoder_FORTS_Curr.h"
#include "Connectors/SSM_Channel.hpp"
#include "Venues/FORTS/SecDefs.h"
#include "Venues/FORTS/Configs_FAST.h"
#include <cstdlib>
#include <cassert>
#include <iostream>

using namespace std;

namespace MAQUETTE
{
namespace FAST
{
namespace FORTS
{
  using AssetClassT = ::MAQUETTE::FORTS::AssetClassT;

  //=========================================================================//
  // "AnyCh" Class:                                                          //
  //=========================================================================//
  // This class provides call-backs to "EPollReactor".
  // It is currently for stand-alone use only -- NOT intended to be part of
  // "EConnector_FAST_FORTS":
  template<ProtoVerT::type Ver>
  class AnyCh: public SSM_Channel<AnyCh<Ver>>
  {
  private:
    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    // Output data structs for parsing FAST msgs:
    mutable OrdersLogIncrRefresh<Ver>     m_incrRefr;
    mutable OrdersLogSnapShot<Ver>        m_snapShot;
    mutable SecurityDefinition<Ver>       m_secDef;
    mutable SecurityDefinitionUpdate<Ver> m_secDefUpd;
    mutable SecurityStatus<Ver>           m_secStat;
    mutable HeartBeat<Ver>                m_heartBeat;
    mutable SequenceReset<Ver>            m_seqReset;
    mutable TradingSessionStatus<Ver>     m_trSessStat;
    mutable News<Ver>                     m_news;

    // Default Ctor is deleted:
    AnyCh() = delete;

  public:
    //=======================================================================//
    // Non-Default Ctor:                                                     //
    //=======================================================================//
    AnyCh
    (
      EPollReactor*      a_reactor,
      EnvT::type         a_env,
      AssetClassT::type  a_ac,
      DataFeedT::type    a_df,
      spdlog::logger*    a_logger,
      string const&      a_iface_ip
    )
    : // Base Class Ctor:
      SSM_Channel<AnyCh<Ver>>
      (
        // Get the Configs for A and B feeds; XXX: currently, will only use A.
        // The following call will NOT through any exception if "Configs" are
        // properly populated:
        Configs.at(make_pair(a_env, a_ac)).at(a_df).first,
        a_iface_ip,
        "AnyCh-A",
        a_reactor,
        a_logger,
        1                // DebugLevel
      ),
      // Buffers for msgs decoding -- initially zeroed-out:
      m_incrRefr  (),
      m_snapShot  (),
      m_secDef    (),
      m_secDefUpd (),
      m_secStat   (),
      m_heartBeat (),
      m_seqReset  (),
      m_trSessStat(),
      m_news      ()
    {}

    //=======================================================================//
    // Dtor:                                                                 //
    //=======================================================================//
    ~AnyCh() noexcept {}

    //=======================================================================//
    // "RecvHandler":                                                        //
    //=======================================================================//
    bool RecvHandler
    (
      int            DEBUG_ONLY(a_fd),
      char const*    a_buff,
      int            a_size,
      utxx::time_val UNUSED_PARAM(a_ts_recv)
    )
    {
      assert(a_fd == this->m_fd);

      //---------------------------------------------------------------------//
      // Trivial Case (End-of-Chunk Event): Ignore it:                       //
      //---------------------------------------------------------------------//
      if (utxx::unlikely(a_buff == nullptr || a_size <= 0))
        return true;

      //---------------------------------------------------------------------//
      // Generic Case: Get the Msg Header:                                   //
      //---------------------------------------------------------------------//
      PMap pmap = 0;
      TID  tid  = 0;
      char const* end = a_buff + a_size;
      // NB: The initial 4 bytes are the SeqNum prefix which we do not use in
      // this case, so skip them:
      a_buff    = GetMsgHeader
                  (a_buff + 4, end, &pmap, &tid, this->m_shortName.data());

      // Parse the msg but ignore the result:
      switch (tid)
      {
      case OrdersLogIncrRefreshTID<Ver>():
        cout << "OrdersLogIncrRefresh" << endl;
        a_buff     = m_incrRefr  .Decode (a_buff, end, pmap);
        break;

      case OrdersLogSnapShotTID<Ver>():
        cout << "OrdersLogSnapShot" << endl;
        a_buff     = m_snapShot  .Decode (a_buff, end, pmap);
        break;

      case SecurityDefinitionTID<Ver>():
        cout << "SecurityDefinition" << endl;
        a_buff     = m_secDef    .Decode (a_buff, end, pmap);
        break;

      case SecurityDefinitionUpdateTID<Ver>():
        cout << "SecurityDefinitionUpdate" << endl;
        a_buff     = m_secDefUpd .Decode(a_buff, end, pmap);
        break;

      case SecurityStatusTID<Ver>():
        cout << "SecurityStatus" << endl;
        a_buff     = m_secStat   .Decode(a_buff, end, pmap);
        break;

      case HeartBeatTID<Ver>():
        cout << "HeartBeat" << endl;
        a_buff     = m_heartBeat .Decode(a_buff, end, pmap);
        break;

      case SequenceResetTID<Ver>():
        cout << "SequenceReset"  << endl;
        a_buff     = m_seqReset  .Decode(a_buff, end, pmap);
        break;

      case TradingSessionStatusTID<Ver>():
        cout << "TradingSessionStatus" << endl;
        a_buff     = m_trSessStat.Decode(a_buff, end, pmap);
        break;

      default:
        cout << "UNEXEPECTED TID=" << tid << endl;
        a_buff = end;
      }

      // In FORTS, there is only 1 msg per UDP pkt -- we must have arrived at
      // the "end":
      if (utxx::unlikely(a_buff != end))
      {
        this->m_logger->warn
          ("AnyCh::RecvHandler: Name={}: End-ptr wrong by {} bytes",
           this->m_longName, int(a_buff - end));
        // Still continue:
        return true;
      }
      return true;  // Continue
    }

    //=======================================================================//
    // "ErrHandler" (for low-level IO errors):                               //
    //=======================================================================//
    void ErrHandler
    (
      int         a_fd,
      int         a_err_code,
      uint32_t    a_events,
      char const* a_msg
    )
    {
      assert(a_fd == this->m_fd && a_msg != nullptr);

      // It is currently considered to be fatal:
      this->m_logger->critical
        ("AnyCh::ErrHandler: Name={}: IO Error: FD={}, ErrNo={}, Events={}: {}",
         this->m_longName,   a_fd,    a_err_code,
         this->m_reactor->EPollEventsToStr(a_events), a_msg);
      exit(1);
    }
  };
} // End namespace FORTS
} // End namespace FAST
} // End namespace MAQUETTE

//===========================================================================//
// "main":                                                                   //
//===========================================================================//
int main(int argc, char* argv[])
{
  using namespace std;
  using namespace MAQUETTE;
  using namespace FAST::FORTS;

  // Get the Command-Line Params:
  if (argc != 5)
  {
    cerr << "PARAMETERS: <EnvT> <AssetClassT> <DataFeedT> <IFaceIP>" << endl;
    return 1;
  }
  EnvT        env  = EnvT       ::from_string(argv[1]);
  AssetClassT ac   = AssetClassT::from_string(argv[2]);
  DataFeedT   df   = DataFeedT  ::from_string(argv[3]);
  string      ifIP =                          argv[4];
  ProtoVerT   ver  = ImpliedProtoVerT(env);

  // Initialise the Logger (simple -- synchronous logging on stderr):
  auto  loggerShP  = IO::MkLogger("FORTS_Test_Logger", "stderr");

  // Construct the Reactor (logging on "stderr") -- no RT settings:
  EPollReactor reactor(loggerShP.get(), 2);  // DebugLevel=2, others default

  // Then everything depends on the settings:
  if (ver == ProtoVerT::Curr)
  {
    AnyCh<ProtoVerT::Curr> channel
      (&reactor, env, ac, df, loggerShP.get(), ifIP);

    channel.Start();
    reactor.Run  (true);   // Exit on any unhandled exceptions
    channel.Stop ();
  }
  else
  {
    cerr << "ERROR: UnSupported: ProtoVer=" << ver.to_string() << endl;
    return 1;
  }

  // If we get here, the Channel has completed its operation and stopped itself:
  return 0;
}
