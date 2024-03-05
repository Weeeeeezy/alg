// vim:ts=2:et
//===========================================================================//
//                     "Tools/MkHistDataBin_HotSpotFX.cpp":                  //
//    Generation of HostSpotFX OrderBooks & Trades from TKS and TRD Files    //
//===========================================================================//
#include "QuantSupport/MDReader_HotSpotFX.h"
#include "QuantSupport/MDReader.hpp"
#include "QuantSupport/MDSaver.hpp"
#include "Basis/IOUtils.hpp"
#include <boost/algorithm/string.hpp>
#include <memory>
#include <iostream>
#include <stdexcept>
#include <filesystem>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cassert>

using namespace MAQUETTE;
using namespace MAQUETTE::QuantSupport;
using namespace std;

namespace
{
  //-------------------------------------------------------------------------//
  // "Usage":                                                                //
  //-------------------------------------------------------------------------//
  inline void Usage(char const* msg)
  {
    if (msg != nullptr)
      cerr << msg << endl;

    cerr << "PARAMETERS:\n"
            "\t[-h]     (Help)\n"
            "\t -s Symbol\n"
            "\t -o OutputDataFile\n"
            "\t[-l LogFile]\n"
            "\t[-d DebugLevel]\n"
            "\t[-m MatchesClusteringSeparatorMSec]\n"
            "\t[-x]     (Fix Bid-Ask Collisions)\n"
            "\t[-O MaxOrders]          (default is usually OK)\n"
            "\t[-L MaxOrderBookLevels] (default is usually OK)\n"
            "\tMktDataDir"
            "\n\nNB: If -m is not specified, produce Non-Aggr OrdersBook!"
         << endl;
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
    // Get the CmdL Params:                                                  //
    //-----------------------------------------------------------------------//
    string      symbol;
    string      mdTopDir;
    string      outFile;
    string      logFile;
    long        maxOrders       = 0;      // Use the built-in default
    int         maxOBLevels     = 0;      // ditto
    bool        fixCollisions   = false;
    int         debugLevel      = -1;     // ditto
    int         clusteringMSec  = -1;

    while (true)
    {
      int c = getopt(argc, argv, "s:o:O:L:l:d:m:xh");
      if (c < 0)
        break;
      switch (c)
      {
      case 's':
        symbol          = optarg;
        break;
      case 'o':
        outFile         = optarg;
        break;
      case 'O':
        maxOrders       = atol(optarg);
        break;
      case 'L':
        maxOBLevels     = atoi(optarg);
        break;
      case 'l':
        logFile         = optarg;
        break;
      case 'd':
        debugLevel      = atoi(optarg);
        break;
      case 'x':
        fixCollisions   = true;
        break;
      case 'm':
        clusteringMSec  = atoi(optarg);
        break;
      case 'h':
        Usage("");
        return 0;
      default:
        Usage("ERROR: Invalid option");
        return 1;
      }
    }
    // At this point, exactly 1 arg must remain for MktDataDir:
    if (optind != argc-1)
    {
      Usage("ERROR: MktDataDir is missing");
      return 1;
    }
    mdTopDir = argv[optind];

    // Symbol, OutputFile and MDTopDir are compulsory:
    if (symbol.empty() || outFile.empty() || mdTopDir.empty())
    {
      Usage("ERROR: Symbol/OutputFile/MDTopDir is missing");
      return 1;
    }

    // NB:
    // (*) Iff ClusteringMSec is specified,  we get and cluster the Trades, and
    //     eventually produce synchronised aggregated "MDAggrRecord"s;
    // (*) Otherwise, we only read OrderBook Updates,  and produce orders-based
    //     "MDOrdersBook"s:
    // (*) So "withAggr" below stands for both "WithAggregations" and "WithAgg-
    //     ressions"!
    bool withAggr = (clusteringMSec > 0);

    //-----------------------------------------------------------------------//
    // Get the list of all DataDirss and TimeStamp:                          //
    //-----------------------------------------------------------------------//
    std::vector<std::string>           pathComps;
    vector<MDReader_HotSpotFX::FileTS> dataDirs;

    filesystem::directory_iterator rit(mdTopDir);
    for (auto const& de: rit)
    {
      // The entries should be DIRECTORIES for the resp Trading Dates, or SYM-
      // LINKS to such directories, rather than individual files:
      filesystem::path path(de);
      if (de.is_symlink())
        path = filesystem::read_symlink(path);
      if (!filesystem::is_directory(path))
        continue;

      // Parse the Path: It should be: SomePrefix/YYYY-MM-DD
      string strPath(path);
      pathComps.clear();
      boost::split  (pathComps, strPath, boost::is_any_of("/"));
      string const& dateStr = pathComps.back();

      int year = -1, month = -1, day = -1;
      int ok   = sscanf(dateStr.c_str(), "%04d-%02d-%02d", &year, &month, &day);
      if (utxx::unlikely
         (ok    != 3 || year < MinYear || year > MaxYear  || month < 1   ||
          month > 12 || day  < 1       || day  > DaysInMonth(year, month)))
      {
        cerr << "WARNING: Invalid Dir: " <<  strPath << endl;
        continue;
      }
      // Generate the DateStart TimeStamp (XXX: assuming it's UTC) -- Hour and
      // Min are not set yet:
      int decDate = year * 10000 + month * 100 + day;
      dataDirs.push_back
        (MDReader_HotSpotFX::FileTS
          (strPath, DateToTimeValFAST<true>(decDate), decDate, 0, 0));
    }
    //-----------------------------------------------------------------------//
    // Create the "MDReader" and the Output File:                            //
    //-----------------------------------------------------------------------//
    // NB: We need "w+" (O_RDWR) here for correct MMap operation on this FD:
    FILE* of = fopen(outFile.c_str(), "w+");
    if (utxx::unlikely(of == nullptr))
    {
      cerr << "ERROR: Cannot open the OutputFile: " << outFile << ": "
           << strerror(errno) << endl;
      return 1;
    }
    vector<string> symbols {symbol};

    unique_ptr<MDReader_HotSpotFX> mdReader
      (MDReader_HotSpotFX::New
        (symbols,        dataDirs, maxOrders, maxOBLevels, fixCollisions,
         clusteringMSec, logFile,  debugLevel)
      );

    //=======================================================================//
    // Aggregations and Aggressions:                                         //
    //=======================================================================//
    if (withAggr)
    {
      //---------------------------------------------------------------------//
      // In this case, synchronised "MDAggrRecord"s are stored:              //
      //---------------------------------------------------------------------//
      constexpr int   LDepth = DefaultLDepth;
      using     MDR = MDAggrRecord<LDepth>;
      MDR       rec;

      // For installing Aggressions, we need to MMap the "outFile", but we can
      // only do that after the OrderBook Updates have been written into  that
      // file (otherwise we don't have its size yet):
      // So create the following "unique_ptr", empty yet (IsRW=true):
      //
      IO::MMapedFile<MDR, true> mmf;

      // Trigger used to (re-)map "mmf" when starting processing new Aggressi-
      // ons:
      bool mmfTrigger = true;

      //---------------------------------------------------------------------//
      // On OrderBook Update:                                                //
      //---------------------------------------------------------------------//
      MDReader_HotSpotFX::OnOBUpdateCBT onOBUpdate
      (
        [of, &rec, &mmfTrigger]
        (OrderBook const& a_ob, long, utxx::time_val a_ts) -> void
        {
          // Serialize the OrderBook data up to the given Depth:
          SaveAggrOrderBook(a_ob, a_ts, &rec);

          // Write the data into a file:
          size_t wrt = fwrite(&rec, sizeof(rec), 1, of);
          if (utxx::unlikely(wrt != 1))
            throw utxx::runtime_error
                  ("AggrOrderBook Saving FAILED: Write Failed: ",
                   strerror(errno));

          // Get ready to process Aggressions when OrderBook Updates are done
          // for the curr Wk:
          mmfTrigger = true;
        }
      );
      //---------------------------------------------------------------------//
      // On Aggression:                                                      //
      //---------------------------------------------------------------------//
      // Install Aggression in the synchronised OrderBook Update entry:
      //
      MDReader_HotSpotFX::OnAggressionCBT onAggr
      (
        [&mmf, &mmfTrigger, &outFile, of]
        (MDAggression const& a_aggr, long a_upd_id, utxx::time_val a_ts)
        -> void
        {
          assert(a_upd_id       >= 0 && !a_ts.empty() && a_aggr.IsValid() &&
                 a_aggr.m_avgPx >  0.0);
          // If the file has not been MMaped yet, or needs  to be  Re-Mapped
          // (after its size has grown with recent OrderBook Updates), do it
          // now, with existing FD:
          if (utxx::unlikely(mmfTrigger))
          {
            // Fully synchronize the previous writes before MMaping the file.
            // We need to flush and sync "of" first:
            (void) fflush(of);
            int    ofd = fileno(of);
            (void) fsync (ofd);

            // Remove the prev Map (if exists) and create a new one:
            mmf.UnMap();
            mmf.Map  (outFile.c_str(), ofd);

            // Re-set the Trigger for processing subsequent Aggressions:
            mmfTrigger = false;
          }
          assert(!(mmfTrigger || mmf.IsEmpty()));

          // Use "a_upd_id" to find the record to be updated. It must have same
          // TimeStamp as specified:
          if (utxx::unlikely(a_upd_id >= mmf.GetNRecs()))
          {
            cerr << "WARNING: Invalid UpdateID: " << a_upd_id << ": NRecs="
                 << mmf.GetNRecs() << endl;
            return;
          }
          // If the UpdID is OK, use it to get the offset in the MMaped array:
          MDR* base = mmf.GetPtr();
          assert(base != nullptr);
          MDR* mdr  = base + a_upd_id;

          // The TimeStamps must match:
          if (utxx::unlikely(mdr->m_ts != a_ts))
          {
            cerr << "WARNING: UpdateID=" << a_upd_id << ": Expected TS="
                 << (mdr->m_ts).milliseconds()       << ", GotTS="
                 << a_ts.milliseconds()              << endl;
            return;
          }
          // If OK: Save the Aggression:
          mdr->m_aggr = a_aggr;
        }
      );
      //---------------------------------------------------------------------//
      // PRESTO! Run the "MDReader":                                         //
      //---------------------------------------------------------------------//
      mdReader->Run(onOBUpdate, onAggr);
    }
    else
    //=======================================================================//
    // OrdersBook (no Trades, but with Orders):                              //
    //=======================================================================//
    {
      constexpr int   ODepth = DefaultODepth;
      using     MDO = MDOrdersBook<ODepth>;
      MDO       rec;

      //---------------------------------------------------------------------//
      // On OrderBook Update:                                                //
      //---------------------------------------------------------------------//
      MDReader_HotSpotFX::OnOBUpdateCBT onOBUpdate
      (
        [of, &rec]
        (OrderBook const& a_ob, long, utxx::time_val a_ts) -> void
        {
          // Serialize the OrdersBook data up to the given Depth:
          SaveOrdersBook(a_ob, a_ts, &rec);

          // Write the data into a file:
          size_t wrt = fwrite(&rec, sizeof(rec), 1, of);
          if (utxx::unlikely(wrt != 1))
            throw utxx::runtime_error
                  ("OrdersBook Saving FAILED: Write Failed: ",
                    strerror(errno));
        }
      );
      //---------------------------------------------------------------------//
      // Run the "MDReader":                                                 //
      //---------------------------------------------------------------------//
      mdReader->Run(onOBUpdate);
    }
    // All Done:
    return 0;
  }
  catch (std::exception const& exn)
  {
    cerr << "EXCEPTION: " << exn.what() << endl;
    return 1;
  }
}
