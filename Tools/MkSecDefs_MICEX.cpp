// vim:ts=2:et
//===========================================================================//
//                        "Tools/MkSecDefs_MICEX.cpp":                       //
//       Receives and Outputs "SecDef"s for MICEX FX and EQ Segments         //
//===========================================================================//
#include "Basis/XXHash.hpp"
#include "Basis/IOUtils.h"
#include "Protocols/FAST/Msgs_MICEX.hpp"
#include "Protocols/FAST/Decoder_MICEX_Curr.h"
#include "Connectors/SSM_Channel.hpp"
#include "Venues/MICEX/SecDefs.h"
#include "Venues/MICEX/Configs_FAST.h"
#include <cstdlib>
#include <cassert>

using namespace std;

namespace
{
  // This flag, if set, reverses normal output:
  bool NonTrad = false;
}

namespace MAQUETTE
{
namespace FAST
{
namespace MICEX
{
  using AssetClassT = ::MAQUETTE::MICEX::AssetClassT;

  //=========================================================================//
  // "SecDefsCh" Class:                                                      //
  //=========================================================================//
  // This class provides call-backs to "EPollReactor".
  // It is currently for stand-alone use only -- NOT intended to be part of
  // "EConnector_FAST_MICEX":
  template
  <
    ProtoVerT::type   Ver,
    AssetClassT::type AC
  >
  class SecDefsCh: public SSM_Channel<SecDefsCh<Ver, AC>>
  {
  private:
    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    mutable SecurityDefinition<Ver>   m_secDef;     // FAST msgs parsed here!
    mutable int                       m_round;

    // Default Ctor is deleted:
    SecDefsCh() = delete;

  public:
    //=======================================================================//
    // Non-Default Ctor:                                                     //
    //=======================================================================//
    SecDefsCh
    (
      EPollReactor*   a_reactor,
      EnvT::type      a_env,
      spdlog::logger* a_logger,
      string const&   a_iface_ip
    )
    : // Base Class Ctor:
      SSM_Channel<SecDefsCh<Ver, AC>>
      (
        // Get the Configs for A and B feeds; XXX: currently, will only use A.
        // The following call will NOT through any exception if "Configs" are
        // properly populated:
        (Configs.at
          (make_tuple(Ver, a_env, AC)).at(DataFeedT::Instr_Defs).first),
        a_iface_ip,
        "SecDefsCh-A",
        a_reactor,
        a_logger,
        1                       // DebugLevel
      ),
      // Buffer for msgs decoding -- initially zeroed-out:
      m_secDef(),

      // Initially, we would generically be at Round=0 which corresponds to the
      // partially-complete "SecurityDefinition"s (not starting from SeqNum=1);
      // Round=1 is the first complete round starting from SeqNum=1; after that,
      // we exit, so there should be no Rounds >= 2:
      m_round(0)
    {}

    //=======================================================================//
    // Dtor:                                                                 //
    //=======================================================================//
    ~SecDefsCh() noexcept
    {
      // "m_secDef" has its own Dtor...
      m_round = 0;
    }

    //=======================================================================//
    // "RecvHandler":                                                        //
    //=======================================================================//
    // Actual "SecurityDefinition" decoding and processing occurs HERE:
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
      // Generically, we expect that "tid" is "SecDefTID":
      //
      constexpr int SecDefTID = GetMainTID(Ver, AC, DataFeedT::Instr_Defs);
      static_assert(SecDefTID != 0, "SecDefsCh::RecvHandler: Could not "
                                    "determine the Expected TID");
      PMap pmap = 0;
      TID  tid  = 0;
      char const* end = a_buff + a_size;
      // NB: The initial 4 bytes are the SeqNum prefix which we do not use in
      // this case, so skip them:
      a_buff    = GetMsgHeader
                  (a_buff + 4, end, &pmap, &tid, this->m_shortName.data());

      //---------------------------------------------------------------------//
      // Now REALLY PARSE the msg:                                           //
      //---------------------------------------------------------------------//
      // Any msgs apart from "SecurityDefinition" are ignored:
      //
      if (utxx::unlikely(tid != SecDefTID))
      {
        if (tid != HeartBeatTID)
          this->m_logger->warn
            ("SecDefsCh::RecvHandler: Name={}: UnExpected TID={}: Ignored",
             this->m_longName, tid);
        // Still continue:
        return true;
      }

      // Parse a "SecurityDefinition":
      a_buff = m_secDef.Decode(a_buff, end, pmap);

      // In MICEX, there is only 1 msg per UDP pkt -- we must have arrived at
      // the "end":
      if (utxx::unlikely(a_buff != end))
      {
        this->m_logger->warn
          ("SecDefsCh::RecvHandler: Name={}: End-ptr wrong by {} bytes",
           this->m_longName, int(a_buff - end));
        // Still continue:
        return true;
      }

      //---------------------------------------------------------------------//
      // Are we in Round=1?                                                  //
      //---------------------------------------------------------------------//
      assert(tid == SecDefTID);
      if (m_secDef.m_MsgSeqNum == 1)
      {
        // Starting a new Round; actual processing occurs in Round=1:
        ++m_round;
        if (m_round >= 2)
          // All done: Exit the Reactor:
          this->m_reactor->ExitImmediately("MkSecDefs_MICEX");
      }
      assert(m_round == 0 || m_round == 1);
      if (m_round != 1)
        return true;      // Round #0 -- continue

      //---------------------------------------------------------------------//
      // Process the "SecurityDefinition":                                   //
      //---------------------------------------------------------------------//
      SecurityDefinition<Ver> const& s = m_secDef;

      // If "MinPriceIncrement" is not known, this asset cannot be traded (as
      // for eg FX baskets or EQ indices):
      // For extra safety, we also require that the MarketCode must be "CURR"
      // for FX and "FNDT" (not "FOND"!) for EQ:
      // BUT if "NonTrad" is set, normal filtering is inverted:
      bool skip =
       (s.m_MinPriceIncrement.m_val <= 0.0                            ||
       (AC == AssetClassT::FX && strcmp(s.m_MarketCode, "CURR") != 0) ||
       (AC == AssetClassT::EQ && strcmp(s.m_MarketCode, "FNDT") != 0))
       ^ NonTrad;
      if (skip)
        return true;  // Continue

      // TradingSessionOrSegmentID:
      // NB: "TradingSessionSubID" is not used:
      char const* sessID =
        (!s.m_MarketSegments.empty() &&
         !s.m_MarketSegments[0].m_SessionRules.empty())
        ? s.m_MarketSegments[0].m_SessionRules[0].m_TradingSessionID
        : "";

      // (*) For the FX section, we are  interested only in SessionID="CETS" or
      //     "FUTS"; another one ("CNGD") is for negotiated deals -- ignored;
      // (*) for the EQ section, we are currently interested in "T+" instruments
      //     only (SessionID prefix "TQ"):
      if ((AC == AssetClassT::FX      &&
          strcmp(sessID, "CETS") != 0 && strcmp (sessID, "FUTS")  != 0) ||
          (AC == AssetClassT::EQ      && strncmp(sessID, "TQ", 2) != 0))
        return true;  // Continue

      // For LotSize (ContractMultiplier), we use "RoundLot":
      long lotSz =
        (!s.m_MarketSegments.empty())
        ? long(s.m_MarketSegments[0].m_RoundLot.m_val)
        : 1;

      // NB: Security Descriptions can contain ""s, need to escape them:
      char descr[256]; // Should still be enough
      int  const l = int(strlen(s.m_SecurityDesc));
      int  out = 0;
      for (int i = 0; i < l; ++i)
      {
        char c = s.m_SecurityDesc[i];
        if (utxx::likely(c != '"'))
          descr[out++] = c;
        else
        {
          descr[out++] = '\\';
          descr[out++] = '"';
        }
      }
      descr[out] = '\0';

      // Asset: For FX, it is Ccy(A); for EQ, the Equity itself:
      char const* asset =
        (AC == AssetClassT::FX)
        ? (strstr(descr, "GLD") != nullptr
           ? "XAU" :
           strstr(descr, "SLV") != nullptr
           ? "XAG" :
           strstr(descr, "HKD") != nullptr
           ? "HKD" :
           s.m_Currency)  // Generic FX case
        :  s.m_Symbol;    // Generic EQ cse

      if (utxx::unlikely(*asset == '\0'))
      {
        // XXX: It is still tolerable in the Non-Tradable case:
        if (utxx::likely(NonTrad))
          asset = s.m_Symbol;
        else
          throw utxx::runtime_error
                ("MkSecDefs_MICEX: Symbol=", s.m_Symbol,
                 ": Cannot determine the Asset");
      }

      // SettlCcy must be available in all cases. If it is not set, use "RUB"
      // by default:
      char const* settlCcy = s.m_SettlCurrency;
      if (utxx::unlikely(*settlCcy == '\0'))
        settlCcy = "RUB";

      // TenorTime:
      // For EQ, we use (notionally) 18:50 MSK  = 15:50 UTC = 57000 sec;
      // for FX, it depends on the Instrument (and TOD is rolled into TOM), but
      // we use 17:30 MSK as the latest for TOD = 14:30 UTC = 52200 sec:
      int const tenorTime =
        (AC == AssetClassT::EQ)
        ? (15 * 3600 + 50 * 60)
        : (14 * 3600 + 30 * 60);

      // MinSizeLots is 1 for Tradable Instruments, and 0 for Indices:
      int const minSizeLots =
        (lotSz > 0)
        ? 1     // Tradable
        : 0;    // Index

      cout
        << "{ // "<< s.m_MsgSeqNum       << '\n'         // SeqNum (comment)
        << "  0,\n"                                      // SecID: NOT HERE!
        << "  \"" << s.m_Symbol          << "\",\n"      // Symbol
        << "  \"\",\n"                                   // AltSymbol: None
        << "  \"" << descr               << "\",\n"      // SecurityDesc
        << "  \"" << s.m_CFICode         << "\",\n"      // CFICode
        << "  \"MICEX\", \"" << sessID   << "\",\n"      // Exchng, TradSessID
        << "  \"\",\n"                                   // Tenor:  Dynamic!
        << "  \"\",\n"                                   // Tenor2: Dynamic!
        << "  \"" << asset               << "\",\n"      // Asset
        << "  \"" << settlCcy            << "\",\n"      // SettlCcy
        << "  'A', 1.0,\n"                               // AssetQty: Always A1
        << "  "   << lotSz                               // LotSize
        << " ,"   << minSizeLots                 <<",\n" // MinSizeLots
        << "  "   << s.m_MinPriceIncrement.m_val <<",\n" // PxStep
        << "  'A', 1.0, 0, "             << tenorTime
        //    PtPx:A1   Tnr                 TnrTime
        << ", 0.0, 0, \"\"\n"                            // Not a Derivative!
        //    Strk UnID UnSym
        << "},"   << endl;

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
        ("SecDefsCh_MICEX::ErrHandler: Name={}: IO Error: FD={}, ErrNo={}, "
         "Events={}: {}", this->m_longName, a_fd,  a_err_code,
         this->m_reactor->EPollEventsToStr(a_events),  a_msg);
      exit(1);
    }
  };
} // End namespace MICEX
} // End namespace FAST
} // End namespace MAQUETTE

//===========================================================================//
// "main":                                                                   //
//===========================================================================//
int main(int argc, char* argv[])
{
  using namespace std;
  using namespace MAQUETTE;
  using namespace FAST::MICEX;

  // Get the Command-Line Params:
  if (argc != 5 && argc != 6)
  {
    cerr << "PARAMETERS: <ProtoVerT (Curr)>    <EnvT (Prod{12}|Test{IL})>\n"
            "            <AssetClassT (FX|EQ)> <IFaceIP> [--NonTradable]"
         << endl;
    return 1;
  }
  ProtoVerT   ver  = ProtoVerT  ::from_string(argv[1]);
  EnvT        env  = EnvT       ::from_string(argv[2]);
  AssetClassT ac   = AssetClassT::from_string(argv[3]);
  string      ifIP = argv[4];

  // Output non-tradable SecDefs (eg FX baskets or EQ indices) instead of
  // normal tradable ones?
  NonTrad  =  (argc == 6 && strcmp(argv[5], "--NonTradable") == 0);

  // Initialise the Logger (simple -- synchronous logging on stderr):
  auto loggerShP = IO::MkLogger("Main", "stderr");

  // Construct the Reactor (logging on "stderr") -- no RT settings:
  EPollReactor reactor(loggerShP.get(), 2);  // DebugLevel=2, others default

  // Then everything depends on the settings:
  if (ver ==  ProtoVerT::Curr && ac == AssetClassT::FX)
  {
    SecDefsCh<ProtoVerT::Curr, AssetClassT::FX> channel
    (
      &reactor,
      env,
      loggerShP.get(),
      ifIP
    );
    channel.Start();
    reactor.Run  (true);   // Exit on any unhandled exceptions
    channel.Stop ();
  }
  else
  if (ver ==  ProtoVerT::Curr && ac == AssetClassT::EQ)
  {
    SecDefsCh<ProtoVerT::Curr, AssetClassT::EQ> channel
    (
      &reactor,
      env,
      loggerShP.get(),
      ifIP
    );
    channel.Start();
    reactor.Run  (true);   // Exit on any unhandled exceptions
    channel.Stop ();
  }
  else
    // This should not happen:
    __builtin_unreachable();

  if (ac == AssetClassT::FX)
    cerr << "REMEMBER to install the FX Tenor manually!!!" << endl;

  // If we get here, the Channel has completed its operation and stopped itself:
  return 0;
}
