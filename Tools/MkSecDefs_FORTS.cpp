// vim:ts=2:et
//===========================================================================//
//                        "Tools/MkSecDefs_FORTS.cpp":                       //
//       Receives and Outputs "SecDef"s for FORTS Fut and Opt Segments       //
//===========================================================================//
#include "Basis/IOUtils.h"
#include "Protocols/FAST/Msgs_FORTS.hpp"
#include "Protocols/FAST/Decoder_FORTS_Curr.h"
#include "Connectors/SSM_Channel.hpp"
#include "Venues/FORTS/SecDefs.h"
#include "Venues/FORTS/Configs_FAST.h"
#include <cstdlib>
#include <cassert>

using namespace std;

namespace MAQUETTE
{
namespace FAST
{
namespace FORTS
{
  using AssetClassT = ::MAQUETTE::FORTS::AssetClassT;

  //=========================================================================//
  // "SecDefsCh" Class:                                                      //
  //=========================================================================//
  // This class provides call-backs to "EPollReactor".
  // It is currently for stand-alone use only -- NOT intended to be part of
  // "EConnector_FAST_FORTS":
  template<ProtoVerT::type Ver>
  class SecDefsCh: public SSM_Channel<SecDefsCh<Ver>>
  {
  private:
    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    AssetClassT const                 m_ac;
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
      EPollReactor*      a_reactor,
      EnvT::type         a_env,
      AssetClassT::type  a_ac,
      spdlog::logger*    a_logger,
      string const&      a_iface_ip
    )
    : // Base Class Ctor:
      SSM_Channel<SecDefsCh<Ver>>
      (
        // Get the Configs for A and B feeds; XXX: currently, will only use A.
        // The following call will NOT through any exception if "Configs" are
        // properly populated:
        Configs.at(make_pair(a_env, a_ac))
               .at(DataFeedT::Infos_Replay).first,
        a_iface_ip,
        "SecDefsCh-A",
        a_reactor,
        a_logger,
        1                       // DebugLevel
      ),
      // Memoise the AssetClass;
      m_ac    (a_ac),

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
      if (utxx::unlikely(tid != SecurityDefinitionTID<Ver>()))
      {
        this->m_logger->info
          ("SecDefsCh::RecvHandler: Name={}: Got TID={}: Ignored",
           this->m_longName, tid);
        // Still continue:
        return true;
      }

      // Parse a "SecurityDefinition":
      a_buff = m_secDef.Decode(a_buff, end, pmap);

      // In FORTS, there is only 1 msg per UDP pkt -- we must have arrived at
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
      if (m_secDef.m_MsgSeqNum == 1)
      {
        // Starting a new Round; actual processing occurs in Round=1:
        ++m_round;
        if (m_round >= 2)
          // All done: Exit the Reactor:
          this->m_reactor->ExitImmediately("MkSecDefs_FORTS");
      }
      assert(m_round == 0 || m_round == 1);
      if (m_round != 1)
        return true;      // Round #0 -- continue

      //---------------------------------------------------------------------//
      // Process the "SecurityDefinition":                                   //
      //---------------------------------------------------------------------//
      SecurityDefinition<Ver> const& s = m_secDef;

      // XXX: Multi-Leg Instrs are currently EXCLUDED -- our "SecDef{S,D}"
      // structs are not made for them yet:
      if (s.m_NoLegs >= 2 || s.m_NoUnderlyings >= 2)
        return true;

      // TradingSessionOrSegmentID:
      // NB: "TradingSessionSubID" is not used:
      char const* sessID = s.m_MarketSegmentID;
      assert(sessID[0]  == s.m_CFICode[0]);

      // NB: "ContractMultiplier" is actually AssetQty! LotSize is always 1 --
      // we can always trade 1 Futures or Option contract:
      long assetAQty = long(Round(s.m_ContractMultiplier.m_val));

      // NB: Security Descriptions can contain ""s, need to escape them:
      char descr[256]; // Should still be enough
      int  const l   = int(strlen(s.m_SecurityDesc));
      int        out = 0;
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

      // SettlCcy Px of 1 Pricing Point: XXX: Here is a problem: the following
      // formula is correct in the generic case when the Settlccy is RUB;
      // otherwise, it would STILL give the PointPx expressed in RUB, whereas
      // the SettlCcy may also be USD or EUR. So in the latter cases, we need
      // to over-write the "pointPx" explicitly:
      //
      bool inRUB = (strcmp(s.m_Currency, "RUB") == 0);
      bool inUSD = (strcmp(s.m_Currency, "USD") == 0);
      bool inEUR = (strcmp(s.m_Currency, "EUR") == 0);
      bool inTRY = (strcmp(s.m_Currency, "TRY") == 0);
      bool inJPY = (strcmp(s.m_Currency, "JPY") == 0);
      bool inCAD = (strcmp(s.m_Currency, "CAD") == 0);
      bool inCHF = (strcmp(s.m_Currency, "CHF") == 0);
      bool inUAH = (strcmp(s.m_Currency, "UAH") == 0);

      // So: Nominal PointPx (not valid for non-RUB-denominated instrs):
      double pointPxNom =
        s.m_MinPriceIncrementAmount.m_val / s.m_MinPriceIncrement.m_val;

      double pointPx =
        utxx::likely(inRUB)
        ? pointPxNom :  // For RUB, always use the Nominal PointPx!
        (inUSD && (strncmp(s.m_Symbol, "GD", 2) == 0 ||   // Gold
                   strncmp(s.m_Symbol, "PT", 2) == 0 ||   // Platinum
                   strncmp(s.m_Symbol, "PD", 2) == 0 ))   // Palladium
        ? 1.0  :        // USD 1
        (inUSD &&  strncmp(s.m_Symbol, "VI", 2) == 0  )   // Russian Vol Idx
        ? 2.0  :        // USD 2 (XXX!)
        (inUSD && (strncmp(s.m_Symbol, "SV", 2) == 0 ||   // Silver
                   strncmp(s.m_Symbol, "BR", 2) == 0 ))   // Brent Crude Oil
        ? 10.0 :        // USD 10
        (inUSD && (strncmp(s.m_Symbol, "ED", 2) == 0 ||   // EUR/USD
                   strncmp(s.m_Symbol, "GU", 2) == 0 ||   // GBP/USD
                   strncmp(s.m_Symbol, "AU", 2) == 0 ))   // AUD/USD
        ? 1000.0 :      // USD 1000
        (inUSD &&  strncmp(s.m_Symbol, "4B", 2) == 0  )   // FTSE/JSE Top40
        ? 0.1    :      // USD 0.10
        (inUSD &&  strncmp(s.m_Symbol, "RI", 2) == 0  )   // RTS Idx
        ? 0.02   :      // USD 0.02
        (inEUR && (strncmp(s.m_Symbol, "BW", 2) == 0 ||   // European Equitties
                   strncmp(s.m_Symbol, "DM", 2) == 0 ||
                   strncmp(s.m_Symbol, "VM", 2) == 0 ||
                   strncmp(s.m_Symbol, "DB", 2) == 0 ||
                   strncmp(s.m_Symbol, "SM", 2) == 0 ))
        ? 10.0   :      // EUR 10
        (inTRY  && strncmp(s.m_Symbol, "TR", 2) == 0 )    // USD/TRY
        ? 1000.0 :      // TRY 1000
        (inJPY  && strncmp(s.m_Symbol, "JP", 2) == 0 )    // USD/JPY
        ? 1000.0 :      // JPY 1000
        (inCAD  && strncmp(s.m_Symbol, "CA", 2) == 0 )    // USD/CAD
        ? 1000.0 :      // CAD 1000
        (inCHF  && strncmp(s.m_Symbol, "CF", 2) == 0 )    // USD/CHF
        ? 1000.0 :      // CHF 1000
        (inUAH  && strncmp(s.m_Symbol, "UU", 2) == 0 )    // USD/UAH
        ? 1000.0 :      // UAH 1000
        throw utxx::runtime_error
              ("Unknown PointPx: Symbol=",      s.m_Symbol,  ", SettlCcy=",
               s.m_Currency,  ", NominalPtPx=", pointPxNom);

      SecID  underSecID =
        (s.m_NoUnderlyings == 1)
        ? s.m_Underlyings[0].m_UnderlyingSecurityID
        : 0;

      char const* underSym =
        (s.m_NoUnderlyings == 1)
        ? s.m_Underlyings[0].m_UnderlyingSymbol
        : "";

      // Asset: XXX: For the known FX Instrs,  it is the corresp Base(A) Ccy;
      // for others, use the Underlying Symbol (but NOT the Instr itself):
      // XXX: This should work for Futures, but how about Options?
      char const* assetA =
        (inUSD && strncmp(s.m_Symbol, "GD", 2) == 0)     // Gold
        ? "XAU" :
        (inUSD && strncmp(s.m_Symbol, "SV", 2) == 0)     // Silver
        ? "XAG" :
        (inUSD && strncmp(s.m_Symbol, "PT", 2) == 0)     // Platinum
        ? "XPT" :
        (inUSD && strncmp(s.m_Symbol, "PD", 2) == 0)     // Palladium
        ? "XPD" :
        (inUSD && strncmp(s.m_Symbol, "ED", 2) == 0)     // EUR/USD -> EUR
        ? "EUR" :
        (inUSD && strncmp(s.m_Symbol, "GU", 2) == 0)     // GBP/USD -> GBP
        ? "GBP" :
        (inUSD && strncmp(s.m_Symbol, "AU", 2) == 0)     // AUD/USD -> AUD
        ? "AUD" :
        (inTRY && strncmp(s.m_Symbol, "TR", 2) == 0) ||  // USD/TRY -> USD
        (inJPY && strncmp(s.m_Symbol, "JP", 2) == 0) ||  // USD/JPY -> USD
        (inCAD && strncmp(s.m_Symbol, "CA", 2) == 0) ||  // USD/CAD -> USD
        (inCHF && strncmp(s.m_Symbol, "CF", 2) == 0) ||  // USD/CHF -> USD
        (inRUB && strncmp(s.m_Symbol, "Si", 2) == 0) ||  // USD/RUB -> RUB
        (inUAH && strncmp(s.m_Symbol, "UU", 2) == 0)     // USD/UAH -> USD
        ? "USD" :
        (inRUB && strncmp(s.m_Symbol, "Eu", 2) == 0)     // EUR/RUB -> EUR
        ? "EUR" :
        underSym; // FIXME!

      // The MaturityTime given in FORTS FAST msgs is inexact (21:00:00.000
      // UTC). The actual time  is 18:45:00.000 MSK = 15:45:00.000 UTC:
      int const tenorTime = 15 * 3600 + 45 * 60;

      // For Futures only, extract the Tenor from Symbol
      // (XXX: For Options, we are not sure whether this would work correctly):
      char const* tenor = "";
      if (m_ac == AssetClassT::Fut)
      {
        assert(strlen(s.m_Symbol) == 4);
        tenor = s.m_Symbol + 2;
      }

      //---------------------------------------------------------------------//
      // Output:                                                             //
      //---------------------------------------------------------------------//
      cout
        << "{ // "<< s.m_MsgSeqNum       << '\n'          // SeqNum (comment)
        << "  "   << s.m_SecurityID      << ",\n"         // SecID
        << "  \"" << s.m_Symbol          << "\",\n"       // Symbol
        << "  \"" << s.m_SecurityAltID   << "\",\n"       // ISIN
        << "  \"" << descr               << "\",\n"       // SecurityDesc
        << "  \"" << s.m_CFICode         << "\",\n"       // CFICode
        << "  \"FORTS\", \"" << sessID   << "\",\n"       // Exchng, TradSessID
        << "  \"" << tenor               << "\",\n"       // Tenor
        << "  \"\",\n"                                    // Tenor2: None
        << "  \"" << assetA              << "\",\n"       // AssetA
        << "  \"" << s.m_Currency        << "\",\n"       // QuoteCcy
        << "  'A', "
                  << assetAQty           << ".0,\n"       // 'A', AssetAQty
        << "  1,  1,\n"                                   // LotSize=1,MinLots=1
        << "  "   << s.m_MinPriceIncrement.m_val << ",\n" // PxStep
        << "  'C', "
                  << pointPx             << ",\n"         // 'C', PointPx
        << "  "   << s.m_MaturityDate    << ",\n"         // TenorDate
        << "  "   << tenorTime           << ",\n"         // TenorTime
        << "  "   << s.m_StrikePrice.m_val       << ",\n" // Strike
        << "  "   << underSecID          << ",\n"         // UnderlyingSecID
        << "  \"" << underSym            << "\"\n"        // UnderlyingSymbol
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
        ("SecDefsCh_FORTS::ErrHandler: Name={}: IO Error: FD={}, ErrNo={}, "
         "Events={}: {}",  this->m_longName, a_fd, a_err_code,
         this->m_reactor->EPollEventsToStr(a_events),  a_msg);
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
  if (argc != 4)
  {
    cerr << "PARAMETERS: <EnvT (eg ProdF)> <Fut|Opt> <IFaceIP>" << endl;
    return 1;
  }

  // Go Ahead!
  EnvT        env       = EnvT       ::from_string(argv[1]);
  AssetClassT ac        = AssetClassT::from_string(argv[2]);
  ProtoVerT   ver       = ImpliedProtoVerT(env);
  string      ifaceIP   = argv[3];

  // Initialise the Logger (simple -- synchronous logging on stderr):
  auto        loggerShP = IO::MkLogger("Main", "stderr");

  // Construct the Reactor (logging on "stderr") -- no RT settings:
  EPollReactor reactor(loggerShP.get(), 2);  // DebugLevel=2, others default

  // Then everything depends on the settings:
  if (ver == ProtoVerT::Curr)
  {
    SecDefsCh<ProtoVerT::Curr> channel
    (
      &reactor,
      env,
      ac,
      loggerShP.get(),
      ifaceIP
    );
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
