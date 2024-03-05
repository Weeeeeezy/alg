// vim:ts=2:et
//===========================================================================//
//                           "MDReader_HotSpotFX.cpp":                       //
//             Reader for Historical (CSV) MktData for HotSpotFX             //
//===========================================================================//
#include "QuantSupport/MDReader_HotSpotFX.h"
#include "Basis/TimeValUtils.hpp"
#include "InfraStruct/SecDefsMgr.h"
#include "Venues/HotSpotFX/SecDefs.h"
#include <cstdlib>

using namespace std;

namespace MAQUETTE
{
namespace QuantSupport
{
  //=========================================================================//
  // Non-Default Ctor:                                                       //
  //=========================================================================//
  MDReader_HotSpotFX::MDReader_HotSpotFX
  (
    vector<string>              const& a_instr_names,
    vector<FileTS>              const& a_data_files,
    boost::property_tree::ptree const& a_params
  )
  : //-----------------------------------------------------------------------//
    // "EConnector": Virtual Base:                                           //
    //-----------------------------------------------------------------------//
    EConnector
    (
      MDRNameHist,
      "HotSpotFX",
      0,              // No extra ShM data
      nullptr,        // No Reactor
      false,          // No BusyWait (actually irrelevant)
      nullptr,        // No SecDefsMgr
      HotSpotFX::SecDefs,
      nullptr,        // No restrictions by symbol
      false,          // Explicit SettlDates are NOT required
      false,          // UseTenors: Irrelevant
      nullptr,        // No RiskMgr
      a_params,
      QT,
      WithFracQtys
    ),
    //-----------------------------------------------------------------------//
    // "MDReader":                                                           //
    //-----------------------------------------------------------------------//
    // NB: Real Ctor invocation comes for the "final" class in the inheritance
    // hierarchy:
    MDReader
    (
      a_instr_names,
      a_data_files,
      a_params
    ),
    //-----------------------------------------------------------------------//
    // "MDReader_HotSpotFX":                                                 //
    //-----------------------------------------------------------------------//
    m_instr1    (nullptr),  // Initialised below
    m_ob1       (nullptr),  // ditto
    m_clustMSec (a_params.get<int>("MatchesClusteringMSec")),
    m_lineBuff  (),
    m_wkMatches (),
    m_wkAggrs   (),
    m_wkPeriods (),
    m_wkUpdates (),
    m_baseUpdID (0),
    // The following histograms provide time distributions in msec:
    m_sepHist   ("Matches Separation, msec", 0.0, 200.0),
    m_discrHist ("(Trade TimeStamp) Minus (OrderBook Update TimeStamp), msec",
                 -250.0, 4750.0)
  {
    // For HotSpotFX, the MktData files format is such that only 1 InstrName
    // can be used:
    if (utxx::unlikely(m_instrNames.size() != 1))
      throw utxx::badarg_error
            ("MDReader_HotSpotFX::Ctor: Exactly 1 InstrName is required");

    // If OK: Get the SecDefD and OrderBook for that InstrName:
    string const& instrName = m_instrNames.back();

    assert    (m_secDefsMgr != nullptr);
    m_instr1 = m_secDefsMgr->FindSecDefOpt(instrName.data());

    if (utxx::unlikely(m_instr1 == nullptr))
      throw utxx::badarg_error
          ("MDReader_HotSpotFX::Ctor: Missing SecDef for ", instrName);

    m_ob1 = FindOrderBook(*m_instr1);
    assert(m_ob1 != nullptr);

    // Reserve space for a typical number of Matches and Aggressions per
    // trading week:
    m_wkMatches.reserve(1'500'000);
    m_wkAggrs  .reserve(  150'000);
    m_wkPeriods.reserve(  150'000);
    // XXX: No "reserve" is available for the "m_wkUpdates" map!
  }

  //=========================================================================//
  // "Run" (with Synchronisation of Trades and OrderBook Updates):           //
  //=========================================================================//
  // HotSpotFX MktData (for a single instrument, eg EUR/USD)  are recorded as
  // multiple files: One file per minute for each trading day. Separate files
  // are provided for OrderBook Updates (".tks") and Trades (".trd"). Each Or-
  // derBook file begins with SnapShot initialisation, followed by  a FullOrd-
  // erLog (of New and Removed/Cancelled Orders):
  //
  void MDReader_HotSpotFX::Run
  (
    OnOBUpdateCBT   const& a_ob_cb,
    OnAggressionCBT const& a_aggr_cb
  )
  {
    //-----------------------------------------------------------------------//
    // Group Data Dirs by Weeks:                                             //
    //-----------------------------------------------------------------------//
    // NB: "m_dataFiles" are actually Data Dirs, one per each Trading Day. We
    // group them by Trading Weeks which are completely independent from each
    // other (because HotSpotFX operates in 24/5 format), so that we can reset
    // the state (eg Aggressions, which are LARGE) between the Weeks:
    //
    if (utxx::unlikely(m_dataFiles.empty()))
      return;

    // In this mode, we need a correct temporal separation for aggregating Mat-
    // ches into Aggressions:
    if (utxx::unlikely(m_clustMSec < 0))
      throw utxx::badarg_error
            ("MDReader_HotSpotFX::Ctor: Invalid Matches Clustering Separator");

    vector<FileTS> currWk;     // Daily DirNames for the Curr Week
    utxx::time_val prevDateTS;

    for (FileTS const& d: m_dataFiles)
    {
      // TimeStamp of the beginning of this TradingDay: Must be monotonic:
      utxx::time_val   dateTS = d.m_mdTS;
      assert(dateTS >= prevDateTS + utxx::secs(86400));

      // If the separation between "prevDateTS" and "dateTS" is >= 3 days, the
      // prev week is presumably over. XXX: But we do not reset the week if it
      // is a 1-day gap (ie separation of 2d, eg a mid-week Independence Day);
      // is it correct???
      if ((dateTS - prevDateTS).seconds() >= (86400 * 3) && !currWk.empty())
      {
        // Yes, it is a NewWk starting. Process the accumulated TradingDays in
        // the prev-"currWk":
        RunWk(currWk, a_ob_cb, &a_aggr_cb);
        currWk.clear();
      }
      prevDateTS = dateTS;

      // Save the curr entry in the "currWk":
      currWk.push_back(d);
    }
    // There must be a remaining non-empty "currWk", so run it as well:
    assert(!currWk.empty());
    RunWk (currWk, a_ob_cb, &a_aggr_cb);

    // Output the Matches Separation Histogram:
    if (m_debugLevel >= 1)
      cout << m_sepHist << "\n\n" << m_discrHist;
  }

  //=========================================================================//
  // "Run" (for OrderBook Updates only):                                     //
  //=========================================================================//
  void MDReader_HotSpotFX::Run(OnOBUpdateCBT const& a_ob_cb)
  {
    // In this case, we process all available Dates at once, without  breaking
    // then into Wks; Aggressions are not processed at all, so there should be
    // NO temporal separator for Matches:
    if (utxx::unlikely(m_clustMSec >= 0))
      throw utxx::badarg_error
            ("MDReader_HotSpotFX::Ctor: Extraneous Matches Clustering "
             "Separator");

    // If OK: Run all data as just "one week":
    RunWk(m_dataFiles, a_ob_cb, nullptr);
  }

  //=========================================================================//
  // "RunWk":                                                                //
  //=========================================================================//
  void MDReader_HotSpotFX::RunWk
  (
    vector<FileTS>  const& a_curr_wk,
    OnOBUpdateCBT   const& a_ob_cb,
    OnAggressionCBT const* a_aggr_cb
  )
  {
    assert(!a_curr_wk.empty());
    //-----------------------------------------------------------------------//
    // Generate BaseFileNames within the CurrWk:                             //
    //-----------------------------------------------------------------------//
    // Each DataDir in "CurrWk"  contains per-Minute ".tks" (OrderBook Updates)
    // and ".trd" (Trades) files. Generate the BaseNames for them (w/o the suf-
    // fixes) and the corresp intra-day TimeStamps:
    //
    vector<FileTS> baseFiles;

    for (FileTS const& d: a_curr_wk)
    {
      string const&  dateDir = d.m_fileName;
      utxx::time_val dateTS  = d.m_mdTS;

      //---------------------------------------------------------------------//
      // Generate FileNames for each (Hour, Min):                            //
      //---------------------------------------------------------------------//
      // NB: Times from 17:00 actually belong to the Prev Calendar Date (which
      // is the beginning of this Trading Date!), so we must start there to en-
      // sure monotonic TimeStamps. Thus, start  at (-7) hours of the CurrDate:
      //
      for (int hour = -7; hour <= 16; ++hour)
      for (int min  =  0; min  <= 59; ++min)
      {
        // TimeStamp for the beginning of the Minute corresp to this File:
        // FIXME: The data files contain intra-day TimeStamps in LocalTime
        // (EST/EDT), not UTC, but we will treat them as UTC to avoid dealing
        // with TimeZones. For single-ECN data processing, this is acceptable:
        //
        utxx::time_val minTS = dateTS + utxx::secs(hour * 3600 + min * 60);

        // Generate the BaseFileName for this Hour and Minute:
        char    hhmm[6];
        int     hh       = (hour < 0) ? (hour + 24) : hour;
        sprintf(hhmm, "/%02d%02d",   hh, min);
        string  baseName = dateDir + hhmm;

        // Save the "baseName" and the corresp intra-day TimeStamp.  NB: Save
        // the valid-range "hh" rather than possibly-negative "hour", it will
        // only be used for verifying TimeStamps when reading data files:
        //
        baseFiles.push_back(FileTS(baseName, minTS, d.m_mdDate, hh, min));
      }
    }
    //-----------------------------------------------------------------------//
    // Read in all Trades (Matches) within the CurrWk:                       //
    //-----------------------------------------------------------------------//
    // Just for extra safety, clear all "wk" buffers once again before proceed-
    // ing:
    m_wkMatches.clear();
    m_wkAggrs  .clear();
    m_wkPeriods.clear();
    m_wkUpdates.clear();

    // But if there is no AggrCB, we do not process Matches/Aggressions at all,
    // so skip this step:
    if (a_aggr_cb != nullptr)
    {
      // Will aggregate Matches into Aggressions -- clustering is performed by
      // time and aggressive direction. The result is in "m_wkAggrs".
      ReadWkTrades(baseFiles);

      // Calculate continuos, non-overlapping, monotonic Periods for WkAggrs:
      MkPeriods();
      if (m_debugLevel >= 1)
        m_logger->info
          ("MDReader_HotSpotFX::RunWk: {} -- {}: Separator={} msec, {} Matches,"
           " {} Aggressions, {} Periods",
           a_curr_wk.front().m_mdDate, a_curr_wk.back().m_mdDate, m_clustMSec,
           m_wkMatches.size(), m_wkAggrs.size(), m_wkPeriods.size());
    }
    //-----------------------------------------------------------------------//
    // Now read the OrderBook Updates:                                       //
    //-----------------------------------------------------------------------//
    long nUpds = ReadWkOrderBookUpdates(baseFiles, a_ob_cb);

    //-----------------------------------------------------------------------//
    // Install Aggressions:                                                  //
    //-----------------------------------------------------------------------//
    // Again, do it only if Matches/Aggressions are processed:
    if (a_aggr_cb != nullptr)
    {
      // We can now map Matches to OrderBook Updates, and complete Aggressions
      // data:
      MapMatchesToUpdates();

      // Finally, invoke the Call-Back on Aggressions, mapping them to the corr
      // OrderBook updates which immediately follow the Aggressions:
      //
      for (Aggression const& aggr: m_wkAggrs)
      {
        utxx::time_val earlTS = aggr.m_updTSs[0];
        if (utxx::unlikely(earlTS.empty()))
          continue;

        // Find the UpdID for the "earlTS" which corresponds to the earlist rec-
        // orderd OrderBook impact of this Aggression. The Call-Back will use it
        // to find the corresp OrderBook Update w/o traversing TimeStamps:
        auto it = m_wkUpdates.find(earlTS);
        if (utxx::likely(it != m_wkUpdates.end()))
        {
          long   globalUpdID = it->second;
          assert(globalUpdID >= 0);

          (*a_aggr_cb)(aggr, globalUpdID, earlTS);
        }
      }
    }
    //-----------------------------------------------------------------------//
    // This Week is Done:                                                    //
    //-----------------------------------------------------------------------//
    // IMPORTANT: BaseUpdID made for the next week:
    m_baseUpdID += nUpds;

    m_wkMatches.clear();
    m_wkAggrs  .clear();
    m_wkPeriods.clear();
    m_wkUpdates.clear();

    if (m_debugLevel >= 1)
      // XXX: If we do not process Aggressions, the actual period of time in
      // "RunWk" may be much longer than 1 wk!
      m_logger->info
        ("MDReader_HotSpotFX::RunWk: COMPLETED: {} -- {}: {} WkUpdates, {} "
         "Total Updates",
         a_curr_wk.front().m_mdDate, a_curr_wk.back().m_mdDate,
         nUpds,            m_baseUpdID);
  }

  //=========================================================================//
  // "ReadWkTrades":                                                         //
  //=========================================================================//
  void MDReader_HotSpotFX::ReadWkTrades
    (vector<FileTS> const& a_base_files)
  const
  {
    assert(!a_base_files.empty() && m_wkAggrs.empty());

    //-----------------------------------------------------------------------//
    // For All Minute Files:                                                 //
    //-----------------------------------------------------------------------//
    for (FileTS const& ft: a_base_files)
    {
      // FileName and TimeStamps:
      string         trdFile = ft.m_fileName + ".trd";
      utxx::time_val minTS   = ft.m_mdTS;
      int            hh      = ft.m_mdHour;
      int            mm      = ft.m_mdMin;
      assert(!minTS.empty() && 0 <= hh && hh <= 23 && 0 <= mm && mm <= 59);

      // Open the Trades File. NB: It is OK if the file cannot be opened: For
      // some intra-day minutes, Trades may not exist:
      FILE*  f               = fopen(trdFile.c_str(), "r");
      if (utxx::unlikely(f  == nullptr))
        continue;

      //---------------------------------------------------------------------//
      // Read the Lines In:                                                  //
      //---------------------------------------------------------------------//
      for (int ln = 1; ; ++ln)
      {
        if (utxx::unlikely(fgets(m_lineBuff, sizeof(m_lineBuff), f) == nullptr))
          break;

        //-------------------------------------------------------------------//
        // Parse the Line:                                                   //
        //-------------------------------------------------------------------//
        // Get the TimeStamp and check that the Hour and Minute are as expctd:
        auto           tsp  = ReadTS(minTS, hh, mm);
        utxx::time_val ts   = tsp.first;
        int            cPos = tsp.second;

        long   diff = (ts - minTS).milliseconds();
        bool   ok1  = (0 <= diff && diff < 60000);

        // Get the Aggressor Side:
        char   aggr = m_lineBuff[cPos+1];
        bool   ok2  = (aggr == 'B' || aggr == 'S') &&
                      (m_lineBuff[cPos+2] == ',');

        // Get the Px:
        char const* end   = m_lineBuff + sizeof(m_lineBuff);
        char*       after = nullptr;
        double      px    = strtod(m_lineBuff + (cPos+3),  &after);
        char const* curr  = after;
        bool        ok3   = (curr < end && *curr == ',' && px > 0.0);

        // Finally, get the Qty:
        double qty  = strtod(curr+1, &after);
        curr        = after;
        bool   ok4  = (curr < end && *curr == '\n' && qty > 0.0);

        // Over-all success flag:
        bool   ok   = ok1 && ok2 && ok3 && ok4;
        if (utxx::unlikely(!ok))
        {
          if (m_debugLevel >= 2)
            m_logger->warn
              ("MDReader_HotSpotFX::ReadWTrades: {}: Invalid Line#{}",
               trdFile, ln);
          continue;
        }
        //-------------------------------------------------------------------//
        // Install a new Match:                                              //
        //-------------------------------------------------------------------//
        // Find the place to install it: normally at the end, or possibly a few
        // steps back fron the end, so do a direct linear search:
        Match* matches  = m_wkMatches.data();
        int    mLen     = int(m_wkMatches.size());
        int    afterPos = mLen - 1;
        for (; afterPos >= 0  && matches[afterPos].m_trTS > ts; --afterPos) ;

        assert(-1 <= afterPos && afterPos <= mLen - 1);
        vector<Match>::iterator  beforeIt  =
          m_wkMatches.begin() + (afterPos + 1);

        // XXX: For some reason, Matches in ".trd" files may come out of chro-
        // nological order, so accumulated them in a proper order first. So in
        // Matches, we can NOT just skip non-monotonic entries:
        Match mt;
        mt.m_trTS    = ts;
        mt.m_px      = px;
        mt.m_qty     = qty;
        mt.m_bidAggr = (aggr == 'B');
        mt.m_upd     = nullptr;   // Not yet known!

        // Save "mt":
        assert(mt.IsValid());
        (void) m_wkMatches.insert(beforeIt, mt);
      }
      // End of Lines-Reading Loop
      // Close the curr TradesFile:
      (void) fclose(f);
    }
    // All Trade Files Done!

    //-----------------------------------------------------------------------//
    // Group Matches into Aggressions (for the whole Week):                  //
    //-----------------------------------------------------------------------//
    int    nMatches = int(m_wkMatches.size());
    Match* matches  = m_wkMatches.data();

    for (int m = 0; m < nMatches; ++m)
    {
      Match*         mt = matches + m;
      utxx::time_val ts = mt->m_trTS;
      // Here TimeStamps MUST be monotonic, because they are already sorted:
      assert(!ts.empty()  && (m == 0 || ts >= matches[m-1].m_trTS));

      // Find the latest Aggression on the same AggrSide as the curr Match:
      Aggression*  aggr = FindAggression(*mt);

      if (aggr != nullptr)
      {
        assert(aggr->m_bidAggr  == mt->m_bidAggr);
        DEBUG_ONLY(utxx::time_val  upTo   =  aggr->m_trTSs[1];)
        assert(aggr->m_trTSs[0] <= upTo && upTo <= ts);

        // The Match and Aggression are separated by < 1.5 sec, so insert the
        // the Match into the Aggression:
        aggr->m_trTSs[1] = ts;
        assert(!(aggr->m_matches.empty()));
        aggr->m_matches.push_back(mt);

        // Update the AvgPx and TotalQty:
        aggr->m_avgPx  *= aggr->m_totQty;
        aggr->m_avgPx  += mt->m_px  * mt->m_qty;
        aggr->m_totQty += mt->m_qty;
        aggr->m_avgPx  /= aggr->m_totQty;

        // This Match is done:
        continue;
      }
      // If we got here: No existing Aggression has been updated (either not
      // found at all, or too far).  Install a new one:
      m_wkAggrs.push_back(Aggression());
      aggr = &(m_wkAggrs.back());

      // Initialize the new Aggression with a single (as yet) Match:
      aggr->m_trTSs[0]  = ts;
      aggr->m_trTSs[1]  = ts;
      aggr->m_bidAggr   = mt->m_bidAggr;
      aggr->m_avgPx     = mt->m_px;
      aggr->m_totQty    = mt->m_qty;
      // Install a ptr to this Match:
      aggr->m_matches.push_back(mt);
    }
    // Grouping Done!
  }

  //=========================================================================//
  // "MkPeriods":                                                            //
  //=========================================================================//
  void MDReader_HotSpotFX::MkPeriods() const
  {
    m_wkPeriods.clear();
    if (utxx::unlikely(m_wkAggrs.empty()))
      return;

    for (Aggression& aggr: m_wkAggrs)
    {
      // Install the range of OrderBook Update times which are potentially of
      // interest (ie may correspond to Matches within this Aggression):
      utxx::time_val from  = aggr.m_trTSs[0];
      utxx::time_val to    = aggr.m_trTSs[1];
      assert(!aggr.m_matches.empty() && !from.empty() && !to.empty() &&
              aggr.m_matches.front() != nullptr       &&
              aggr.m_matches.back () != nullptr       &&
              aggr.m_matches.front()->m_trTS == from  &&
              aggr.m_matches.back ()->m_trTS == to    && from <= to);
      // Widen the time range for potentially-related OrderBook updates:  XXX:
      // The range below is EXTREMELY WIDE, but for HotSpotFX, Trade Times may
      // be FAR AWAY from the corresp OrderBook Updates:
      from -= utxx::msecs(5000);
      to   += utxx::msecs(5000);

      // First of all, check for extreme cases:
      // The new interval is entirely beyond the end of the existing Periods:
      if (m_wkPeriods.empty() || m_wkPeriods.back().m_range[1] < from)
      {
        m_wkPeriods.emplace_back(from, to, aggr.m_matches);
        continue;
      }
      // ...or before all Periods (which is extremely unlikely):
      if (utxx::unlikely(to < m_wkPeriods.front().m_range[0]))
      {
        m_wkPeriods.emplace(m_wkPeriods.begin(), from, to, aggr.m_matches);
        continue;
      }

      // OTHERWISE: More complex case:
      // Traverse the Periods backwards, looking for possible overlaps of the
      // [from, to] interval with any Periods.   Stop when "aggrDone" flag is
      // set:
      // XXX: DO NOT extract a raw data ptr from "m_wkPeriods", as this vector
      // may be modified and re-allocated in the loop below:
      //
      bool aggrDone = false;

      for (int r = int(m_wkPeriods.size())-1; r >= 0; --r)
      {
        AggrPeriod&     curr     = m_wkPeriods [size_t(r)]; // Refs!
        utxx::time_val& cFrom    = curr.m_range[0];         //
        utxx::time_val& cTo      = curr.m_range[1];         //
        vector<Match*>& cMatches = curr.m_matches;          //
        assert(cFrom <  cTo);

        if (cTo < from)
        {
          // Then "curr" is entirely before the interval [from, to],  and all
          // further ranges will be so as well. No Periods can be updated any-
          // more:
          aggrDone = true;
          break;
        }

        // Otherwise: We are not yet too deep in prior Periods. Check if the
        // new interval falls exactly in between of existing Periods:
        if (r > 0)
        {
          AggrPeriod const&         prev  = m_wkPeriods [size_t(r-1)];
          DEBUG_ONLY(utxx::time_val pFrom = prev.m_range[0];)
          utxx::time_val            pTo   = prev.m_range[1];
          assert(pFrom < pTo && pTo < cFrom);

          if (pTo < from && to < cFrom)
          {
            // Yes, in between: Insert a new Period before "curr":
            m_wkPeriods.emplace
              (m_wkPeriods.begin()+r, from, to, aggr.m_matches);
            aggrDone = true;
            break;
          }
        }
        // IF WE GOT HERE: Check for OVERLAP between [from, to] and "curr":
        // First, possible overlaps with FWD (LATER) Periods:
        if (from <= cTo && cTo < to)
        {
          // "cTo" is in [from, to], so extend it upwards -> "to":
          cTo = to;
          AddMatches(aggr.m_matches, &cMatches);

          // This can result in a chain merge of later Periods, so go fwd:
          for (int f = r+1; f < int(m_wkPeriods.size()); )
          {
            AggrPeriod const& fwd = m_wkPeriods[size_t(f)];
            if (cTo < fwd.m_range[0])
              break;  // No more fwd merges -- break the "f" loop

            // Otherwise, "fwd" is merged with "curr", and "fwd" is removed:
            cTo =      fwd.m_range[1];
            AddMatches(fwd.m_matches, &cMatches);
            m_wkPeriods.erase(m_wkPeriods.begin()+f);

            // We already know that this "aggr" was processed: set "aggrDone":
            aggrDone = true;

            // Continue with possible fwd merges. NB: "f" is unchanged, but
            // now points to the next Period...
          }
        }
        // Then possible overlaps with BWD (EARLIER) Periods:
        if (from < cFrom && cFrom <= to)
        {
          // "cFrom" is in [from, to], so extend it downwards -> "from":
          cFrom = from;
          AddMatches(aggr.m_matches, &cMatches);

          // This can result in a chain merge of earlier Periods, so go bwd:
          for (int b = r-1; b >= 0; --b)
          {
            // If we got here, "r" must be valid (NB: it may be decremented in
            // this loop below!):
            assert(r >= 0);

            AggrPeriod const& bwd = m_wkPeriods[size_t(b)];
            if (bwd.m_range[1] < cFrom)
              break;  // No more bwd merges -- break the "b" loop

            // Otherwise, "bwd" is merged with "curr", and "bwd" removed:
            cFrom =    bwd.m_range[0];
            AddMatches(bwd.m_matches, &cMatches);
            m_wkPeriods.erase(m_wkPeriods.begin()+b);

            // We already know that this "aggr" was processed: set "aggrDone":
            aggrDone = true;

            // Continue with possible bwd merges. NB: Unlike the fwd case, "b"
            // needs to be decremented! IMPORTANT: "r" must also be decremented
            // because a prior Period was removed!
            --r;
          }
        }
        // If the Period [from,to] originating from "aggr"  was merged into
        // some existing Period (and the domino effect processed),   we are
        // done with this "aggr". Otherwise, continue scanning the existing
        // Periods:
        if (aggrDone)
          break;
      }
      // End of "r" loop
    }
    // End of "aggr" loop

    // Verify "m_wkPeriods" at the end:
    DEBUG_ONLY(
    AggrPeriod const* periods = m_wkPeriods.data();
    int              nPeriods = int(m_wkPeriods.size());

    for (int p = 0; p < nPeriods; ++p)
    {
      AggrPeriod const& curr = periods[p];
      utxx::time_val    from = curr.m_range[0];
      utxx::time_val    to   = curr.m_range[1];
      assert(from < to  &&  !curr.m_matches.empty());

      // All Matches must have non-decreasing TimeStamps within the range of
      // this "AggrPeriod":
      Match* const* mps = curr.m_matches.data();
      int           nm  = int(curr.m_matches.size());
      for (int m  = 0;  m < nm;  ++m)
      {
        Match* mp = mps[m];
        assert(mp != nullptr);
        if (m > 0)
          assert(from <= mp->m_trTS && mp->m_trTS <= to);
      }
      // Check the gap between the "prev" Period and the "curr" one:
      if (utxx::likely(p > 0))
      {
        AggrPeriod const& prev = periods[p-1];
        assert(prev.m_range[1] < curr.m_range[0]);
      }
    })
  }

  //=========================================================================//
  // "ReadWkOrderBookUpdates":                                               //
  //=========================================================================//
  long MDReader_HotSpotFX::ReadWkOrderBookUpdates
  (
    vector<FileTS> const&  a_base_files,
    OnOBUpdateCBT  const&  a_ob_cb
  )
  {
    //-----------------------------------------------------------------------//
    // For All Minute Files:                                                 //
    //-----------------------------------------------------------------------//
    // All OrderBook Updates are numbered through (within the Wk):
    long           wkUpdID = 0;
    utxx::time_val lastTS;

    for (FileTS const& ft: a_base_files)
    {
      //---------------------------------------------------------------------//
      // Initialization:                                                     //
      //---------------------------------------------------------------------//
      // FileName and TimeStamps:
      string         ordFile = ft.m_fileName + ".tks";
      utxx::time_val minTS   = ft.m_mdTS;
      int            hh      = ft.m_mdHour;
      int            mm      = ft.m_mdMin;
      assert(!minTS.empty() && 0 <= hh && hh <= 23 && 0 <= mm && mm <= 59);

      // Open the Orders File. XXX: Those files must exist (on Trading Dates)
      // for any (Hour, Min):
      FILE*  f               = fopen(ordFile.c_str(), "r");
      if (utxx::unlikely(f  == nullptr))
      {
        m_logger->warn
          ("MDReader_HotSpotFX::ReadWkOrderBookUpdates: Missing File: {}",
           ordFile);
        continue;
      }
      // NB: Each Minute has its own SnapShot initialization, so reset the Or-
      // derBook and Orders completely at the beginning of each Minute,   and
      // get back to the InitMode:
      ResetOrderBooksAndOrders();
      assert(m_dynInitMode);

      //---------------------------------------------------------------------//
      // Read the Lines In:                                                  //
      //---------------------------------------------------------------------//
      char const* end = m_lineBuff + sizeof(m_lineBuff);

      // We will also attribute OrderBook Updates being read to AggrPeriods, in
      // order to associate Matches with Cancels / Modifies:
      AggrPeriod* periods = m_wkPeriods.data();
      int        nPeriods = int(m_wkPeriods.size());
      AggrPeriod*   currP = nullptr;
      int              CP = 0;

      // BIG READING LOOP:
      for (int ln = 1; ; ++ln)
      {
        if (utxx::unlikely(fgets(m_lineBuff, sizeof(m_lineBuff), f) == nullptr))
          break;

        //-------------------------------------------------------------------//
        // Get the TimeStamp:                                                //
        //-------------------------------------------------------------------//
        // The line format is:
        // hh:mm:ss.sss,OrderID,[SCN],[12],Px,Qty
        auto           tsp  = ReadTS(minTS, hh, mm);
        utxx::time_val ts   = tsp.first;
        int            cPos = tsp.second;
        long           diff = (ts - minTS).milliseconds();
        bool           ok1  = (0 <= diff && diff < 60000);

        // Check the TimeStamp for monotonicity.  XXX: For OrderBook Updates,
        // Non-Monotonic TimeStamps are usually due to repeated data patches,
        // so we can skip them without worrying too much:
        if (utxx::unlikely(ts.empty() || ts < lastTS))
        {
          if (m_debugLevel >= 3)
            m_logger->warn
              ("MDReader_HotSpotFX::ReadWkOrderBookUpdatesTrades: {}: "
               "Non-Monotonic TimeStamp at Line#{}", ordFile, ln);
          continue;
        }

        //-------------------------------------------------------------------//
        // For this "ts", find the Period it belongs to:                     //
        //-------------------------------------------------------------------//
        // INVARIANT: periods[CP] is either the interval "ts" belongs to, or
        // is ahead of "ts", or CP == nPeriods (no more Periods available).
        // If "ts" is indeed in periods[CP], "currP" is set; otherwise, it is
        // NULL:
        // First of all, if "ts" is entirely beyond the curr Period, then we
        // need to advance "CP":
        for (; CP < nPeriods && ts > periods[CP].m_range[1]; ++CP) ;

        if (utxx::likely(CP < nPeriods))
        {
          assert(nPeriods > 0);

          // Thus, "ts" is now either before or within the "CP"s Period:
          utxx::time_val            pFrom = periods[CP].m_range[0];
          DEBUG_ONLY(utxx::time_val pTo   = periods[CP].m_range[1];)
          assert(pFrom < pTo && (ts < pFrom  || (pFrom <= ts && ts <= pTo)));

          if (ts < pFrom)
            // "CP" is not the curr Period yet:
            currP = nullptr;
          else
          {
            // "CP" is active now:
            assert(pFrom <= ts && ts <= pTo);
            currP = periods + CP;
          }
        }
        else
          // "CP" is not valid, so "currP" is not valid either:
          currP = nullptr;

        //-------------------------------------------------------------------//
        // Parse the rest of the Line:                                       //
        //-------------------------------------------------------------------//
        // Read the OrderID:
        OrderID     orderID = 0;
        char const* curr =
          utxx::fast_atoi<OrderID, false>(m_lineBuff + (cPos+1), end, orderID);
        bool ok2 = (curr != nullptr  &&   *curr == ',' && curr < end-1);

        // Read the Action Char:
        ++curr;
        char  action = *curr;
        ++curr;
        bool ok3 = (curr < end-1  && *curr  == ',' &&
                   (action == 'S' || action == 'N' || action == 'C' ||
                    action == 'M'));

        // Read the Side Char (unless Action is 'C' or 'M'):
        ++curr;
        char side = '\0'; // 1=Bid, 2=Ask
        bool ok4  = true;
        if (action != 'C' && action != 'M')
        {
          side   = *curr;
          ++curr;
          ok4   &= (side == '1' || side == '2');
        }
        ok4 &= (curr < end-1  && *curr  == ',');

        // Read the Px (again, unless Action is 'C' or 'M'): It must be > 0 in
        // this case:
        ++curr;
        char*  after = nullptr;
        double px    = NaN<double>;
        bool   ok5   = true;
        if (action != 'C' && action != 'M')
        {
          px   = strtod(curr, &after);
          curr = after;
          ok5 &= (px > 0.0);
        }
        ok5 &= (curr < end-1 && *curr == ',');

        // Finally, the Qty (unless Action=='C'; for 'M', NewQty is required!).
        // NB: We read the Qty as a "double" because in very rare cases it can
        // be fractional;  however, we will always round it up to an integer:
        ++curr;
        double qty = 0.0;
        bool   ok6 = true;
        if (action != 'C')
        {
          qty  = strtod(curr, &after);
          curr = after;
          ok6 &= (qty > 0.0);
        }
        ok6 &= (curr < end && *curr == '\n');

        // Check the line validity. If not valid, skip it with a warning:
        bool ok  = ok1 && ok2 && ok3 && ok4 && ok5 && ok6;
        if (utxx::unlikely(!ok))
        {
          if (m_debugLevel >= 2)
            m_logger->warn
              ("MDReader_HotSpotFX::ReadWkOrderBookUpdates: {}: Invalid "
               "Line#{}", ordFile, ln);
          continue;
        }
        //-------------------------------------------------------------------//
        // Invoke the Call-Back at THIS point:                               //
        //-------------------------------------------------------------------//
        // If the TimeStamp has moved forward, AND we are not in the InitMode,
        // the previous OrderBook state should be consistent, so  we *may* be
        // able to invoke the Call-Back NOW (and NOT after the update is carr-
        // ied out, because it may be an incomplete beginning of a new update):
        //
        bool movedFwd = !lastTS.empty() && ts > lastTS;

        if  (movedFwd && action != 'S'  && !m_dynInitMode)
        {
          // XXX: For efficiency, "ChecLevelkOrders" is currently not used:
          if (utxx::unlikely(!(m_ob1->IsConsistent())))
          {
           if (m_debugLevel >= 3)
            {
              double bidPx(m_ob1->GetBestBidPx());
              double askPx(m_ob1->GetBestAskPx());
              m_logger->warn
                ("MDReader_HotSpotFX::ReadWkOrderBookUpdates: {}, Line#{}: "
                 "Bid-Ask COLLISION: {} .. {}: {}", ordFile, ln, bidPx, askPx,
                 (m_fixCollisions ? "Fixing" : "Skipping"));
            }
            if (m_fixCollisions)
            {
              // WithOrdersLog=true:
              m_ob1->CorrectBook<true, QT, QR>();

              // Collision Fixed: Invoke the Call-Back (again with the Prev TS,
              // because the curr update was not applied yet!):
              long globalUpdID = m_baseUpdID + wkUpdID;
              a_ob_cb(*m_ob1,    globalUpdID,  lastTS);

              // Curr "lastTS" corresponds to the curr "globalUpdID". Record it
              // if we are in a Period of interest; "insert" will do nothing if
              // "globalUpdID" is not the first one at "lastTS":
              if (currP != nullptr)
                (void) m_wkUpdates.insert(make_pair(lastTS, globalUpdID));
              ++wkUpdID;
            }
            // Otherwise, the Book remains in collision, so no Call-Back invkn!
          }
          else
          {
            // Generic Case: No Collisions. Again, invoke the Call-Back (with
            // "lastTS") and record the Update ID and Time  (only if within a
            // Period of interest, and first Update at that time):
            long globalUpdID = m_baseUpdID + wkUpdID;
            a_ob_cb(*m_ob1,    globalUpdID,  lastTS);

            if (currP != nullptr)
              (void) m_wkUpdates.insert(make_pair(lastTS, globalUpdID));
            ++wkUpdID;
          }
        }
        // NB: ONLY NOW we can move "lastTS" fwd:
        lastTS = ts;

        //-------------------------------------------------------------------//
        // Now carry out THIS update:                                        //
        //-------------------------------------------------------------------//
        // IMPORTANT: As we use "ApplyOrderUpdate" only (not say "UpdateOrder-
        // Books", TimeStamps are NOT recorded in "*m_ob1"; the only TimeStamp
        // is therefore our "ts"/"lastTS" managed above!
        //
        // Side encoded as EntryType (would typically be valid for "New" only,
        // in other cases UNDEFINED):
        FIX::MDEntryTypeT entrySide =
          (side == '1') ? FIX::MDEntryTypeT::Bid   :
          (side == '2') ? FIX::MDEntryTypeT::Offer :
                          FIX::MDEntryTypeT::UNDEFINED;

        // We may also need OrderInfo for update recording:
        OrderBook::OrderInfo const* oi    = nullptr;
        double                      oiQty = 0.0;
        double                      oiPx  = 0.0;
        bool                        oiBid = false;

        switch (action)
        {
          case 'S':
            //---------------------------------------------------------------//
            // SnapShot:                                                     //
            //---------------------------------------------------------------//
            // Both Static and Dynamic Init Modes are 'true'. If DynInitMode
            // was already reset, it is again a data error: skip such a line
            // w/o warning:
            //
            if (!m_dynInitMode)
              continue;

            (void) ApplyOrderUpdate
            <
              true,   // IsSnapShot
              WithIncrUpdates, StaticInitMode,  ChangeIsPartFill,
              NewEncdAsChange, ChangeEncdAsNew, WithRelaxedOBs,
              IsFullAmt,       IsSparse,        QT,  QR
            >
            ( orderID, FIX::MDUpdateActionT::New, m_ob1, entrySide, PriceT(px),
              Qty<QT,QR>(qty), 0, 0
            );
            break;

          case 'N':
            //---------------------------------------------------------------//
            // New Order in Steady Mode:                                     //
            //---------------------------------------------------------------//
            // Once we got such a line, InitMode is over:
            m_dynInitMode = false;

            (void) ApplyOrderUpdate
            <
              false,  // !IsSnapShot
              WithIncrUpdates, StaticInitMode,  ChangeIsPartFill,
              NewEncdAsChange, ChangeEncdAsNew, WithRelaxedOBs,
              IsFullAmt,       IsSparse,        QT,  QR
            >
            ( orderID, FIX::MDUpdateActionT::New, m_ob1, entrySide, PriceT(px),
              Qty<QT,QR>(qty), 0, 0
            );
            break;

          case 'C':
            //---------------------------------------------------------------//
            // Order Deletion: in Steady Mode only:                          //
            //---------------------------------------------------------------//
            // Get the OrderInfo and Qty BEFORE the update:
            oi    = m_ob1->GetOrderInfo(orderID);
            oiQty = utxx::likely(oi != nullptr)
                    ? double(oi->GetQty<QT,QR>()) : 0.0;
            oiPx  = utxx::likely(oi != nullptr) ? double(oi->m_px) : 0.0;
            oiBid = oi->m_isBid;

            m_dynInitMode = false;

            (void) ApplyOrderUpdate
            <
              false,  // !IsSnapShot
              WithIncrUpdates, StaticInitMode,  ChangeIsPartFill,
              NewEncdAsChange, ChangeEncdAsNew, WithRelaxedOBs,
              IsFullAmt,       IsSparse,        QT,  QR
            >
            ( orderID, FIX::MDUpdateActionT::Delete, m_ob1, entrySide, PriceT(),
              Qty<QT,QR>(), 0, 0
            );
            break;

          case 'M':
          {
            //---------------------------------------------------------------//
            // Order Modification: in Steady Mode only:                      //
            //---------------------------------------------------------------//
            // Get the OrderInfo and Qty BEFORE the update:
            oi    = m_ob1->GetOrderInfo(orderID);
            oiQty = utxx::likely(oi != nullptr)
                    ? double(oi->GetQty<QT,QR>()) : 0.0;
            oiPx  = utxx::likely(oi != nullptr)   ? double(oi->m_px) : 0.0;
            oiBid = oi->m_isBid;

            m_dynInitMode = false;

            (void) ApplyOrderUpdate
            <
              false,  // !IsSnapShot
              WithIncrUpdates, StaticInitMode,  ChangeIsPartFill,
              NewEncdAsChange, ChangeEncdAsNew, WithRelaxedOBs,
              IsFullAmt,       IsSparse,        QT,  QR
            >
            ( orderID, FIX::MDUpdateActionT::Change, m_ob1, entrySide, PriceT(),
              Qty<QT,QR>(qty), 0, 0
            );
            break;
          }
        }
        // Action Switch Done

        //-------------------------------------------------------------------//
        // If within an "AggrPeriod": Collect Cancels / Modifies:            //
        //-------------------------------------------------------------------//
        if (currP != nullptr && (action == 'C' || action == 'M') &&
            oi    != nullptr)
        {
          OrderUpdate    ou;
          ou.m_updTS   = ts;
          ou.m_orderID = orderID;
          ou.m_isBid   = oiBid;
          ou.m_px      = oiPx;
          ou.m_wasQty  = oiQty;
          ou.m_nowQty  = (action == 'C') ? 0.0 : qty;
          ou.m_match   = nullptr;          // Not yet known!

          if (!ou.IsValid())
          {
            if (m_debugLevel >= 2)
              m_logger->warn
                ("MDReader_HotSpotFX::ReadWkOrderBookUpdates: Got Invalid Order"
                 "Update, SKIPPING: TS={}, OrderID={}, IsBid={}, Px={}, "
                 "WasQty={}, NowQty={}",
                 ou.m_updTS.milliseconds(), ou.m_orderID, ou.m_isBid, ou.m_px,
                 ou.m_wasQty, ou.m_nowQty);
          }
          else
            // OrderUpdate is OK:
            currP->m_updates.push_back(ou);
        }
      }
      // Line Reading Loop Done!
      fclose(f);
    }
    // OrderBook Files Loop Done
    // At the end of the Wk, the "wkUpdID" is the number of OrderBook updates
    // carried out within this Wk:
    return wkUpdID;
  }

  //=========================================================================//
  // "ReadTS":                                                               //
  //=========================================================================//
  // Helper method used by "ReadWkTrades: and "ReadWkOrderBookUpdates": Reads
  // and verifies a TimeStamp; returns EmptyTS if any check fails; also returns
  // the CommaPos:
  //
  pair<utxx::time_val, int>   MDReader_HotSpotFX::ReadTS
    (utxx::time_val a_min_ts, int a_hour, int a_min) const
  {
    // NB: "hour" and "min" are already known from the Caller, we only need to
    // verify them:
    assert(0 <= a_hour && a_hour <= 23 && 0 <= a_min && a_min <= 59);
    int hour = -1;
    int min  = -1;
    int sec  = -1;
    int msec = -1;
    int cPos = -1;

    bool ok1 = (utxx::atoi_left<int, 2, char>(m_lineBuff + 0, hour) ==
                m_lineBuff +  2)         &&
               (m_lineBuff   [2] == ':') && (hour  == a_hour);
    bool ok2 = (utxx::atoi_left<int, 2, char>(m_lineBuff + 3, min)  ==
                m_lineBuff +  5)         &&
               (m_lineBuff   [5] == ':') && (min   == a_min);
    bool ok3 = (utxx::atoi_left<int, 2, char>(m_lineBuff + 6, sec)  ==
                m_lineBuff +  8)         &&
               (m_lineBuff   [8] == '.'  || m_lineBuff[8] == ',')   &&
               (0 <= sec) && (sec <= 60);
    // XXX: Theoretically, Leap Seconds are allowed! Also, msecs can be missing,
    // so the above fld can be terminated by ',' as well as by '.'...
    bool ok4  = true;
    if (utxx::likely(m_lineBuff[8] == '.'))
    {
      // Got msecs:
      ok4  = (utxx::atoi_left<int, 3, char>(m_lineBuff + 9, msec) ==
              m_lineBuff + 12) && (m_lineBuff[12] == ',') && (0 <= msec);
      cPos = 12;
    }
    else
    {
      // No  msecs:
      msec = 0;
      cPos = 8;
    }

    // Return the MilliSec TimeStamp if all flags are OK, and the CommaPos:
    utxx::time_val ts;
    if (utxx::likely(ok1 && ok2 && ok3 && ok4))
      ts = a_min_ts + utxx::secs(sec) + utxx::msecs(msec);

    return make_pair(ts, cPos);
  }

  //=========================================================================//
  // "FindAggression":                                                       //
  //=========================================================================//
  // Find an Aggression to which the given Match may belong:
  //
  MDReader_HotSpotFX::Aggression*
  MDReader_HotSpotFX::FindAggression(Match const& a_mt) const
  {
    // Traverse "m_wkAggrs" backwards, looking for an Aggression at the same
    // side as "a_mt":
    utxx::time_val earliest = a_mt.m_trTS - utxx::secs(3);

    for (auto rit = m_wkAggrs.rbegin(); rit != m_wkAggrs.rend(); ++rit)
    {
      // If we are too far back, nothing else can be found:
      if (rit->m_trTSs[0] < earliest)
        break;

      // Skip entries on the wrong side:
      if (rit->m_bidAggr != a_mt.m_bidAggr)
        continue;

      // If same side and within a reasonable time interval:
      // XXX: We normally assume that Matches coming from the same Aggression
      // arrive in the right temporal order, and then they would come in the
      // right order of pxs as well:
      // (*) Bidder Aggressor => Matches on Ask Side => Pxs Non-Decreasing
      // (*) Asker  Aggressor => Matches on Bid Side => Pxs Non-Increasing:
      //
      assert(!(rit->m_matches.empty()));
      Match const* lm = rit->m_matches.back();
      assert(lm != nullptr);

      utxx::time_val latest = lm->m_trTS;
      assert(rit->m_trTSs[1] == latest);
      double         aggrPx = lm->m_px;

      if ( a_mt.m_trTS < latest                        ||
         ( a_mt.m_bidAggr && a_mt.m_px < aggrPx - Eps) ||
         (!a_mt.m_bidAggr && a_mt.m_px > aggrPx + Eps))
        continue;

      // If we got here, all conditions for including the "a_mt" Match into the
      // "*rit" Aggression are met, except (maybe) the positive temporal separ-
      // ation. So update the Histogram now:
      long   mSec    = (a_mt.m_trTS - latest).milliseconds();
      assert(mSec >= 0);
      double dmSec   = double(mSec);
      m_sepHist.Update(dmSec);

      if (mSec <= m_clustMSec)
        // Finally found it! The last criterion is met!
        return &(*rit);

      // Otherwise, search further (though it is very unlikely we would find a
      // suitable Aggression at the end, if this candidate Aggression failed)...
    }
    // Nothing found:
    return nullptr;
  }

  //=========================================================================//
  // "AddMatches":                                                           //
  //=========================================================================//
  void MDReader_HotSpotFX::AddMatches
    (vector<Match*> const& a_src, vector<Match*>* a_targ)
  {
    assert(a_targ != nullptr);
    // XXX: Normally, both "src" and "targ" are sorted in ascending order of
    // TimeStamps, but we do not make any assumtions of that -- just re-sort
    // the result. This is OK because both vectors are typically very short:
    //
    for (Match* s: a_src)
    {
      assert(s != nullptr);
      a_targ->push_back(s);
    }
    sort
    (
      a_targ->begin(), a_targ->end(),
      [](Match* a_left, Match* a_right)
      {
        assert(a_left != nullptr && a_right != nullptr);
        return a_left->m_trTS < a_right->m_trTS;
      }
    );
  }

  //=========================================================================//
  // "MapMatchesToUpdates":                                                  //
  //=========================================================================//
  void MDReader_HotSpotFX::MapMatchesToUpdates() const
  {
    //-----------------------------------------------------------------------//
    // Traverse all Periods and Matches within them:                         //
    //-----------------------------------------------------------------------//
    for (AggrPeriod& period: m_wkPeriods)
    for (Match*  mt: period. m_matches)
    {
      // The Match should be valid by construction:
      assert(mt != nullptr && mt->IsValid());

      // Find an OrderBook Update for this Match:
      // (*) Right Side
      // (*) Same  Px
      // (*) Suitable Qty
      // (*) Minimal Time Separation:
      long         minSep = INT_MAX;            // In milliseconds
      OrderUpdate* bestOU = nullptr;

      for (OrderUpdate& ou: period.m_updates)
      {
        // By construction, we only saved Valid OrderUpdates:
        assert(ou.IsValid());

        double deltaQty = ou.m_wasQty - ou.m_nowQty;
        assert(deltaQty > 0.0);

        if (ou.m_match !=  nullptr          ||  // Already linked to a Match
            ou.m_isBid ==  mt->m_bidAggr    ||  // NB: Must be opposite!
            fabs(ou.m_px - mt->m_px) > Eps  ||
            mt->m_qty  !=  deltaQty)
          continue;                             // Not compatible..

        // Otherwise, this OrderUpdate is a candidate: Compute the temporal
        // separation of the OrderUpdate and the Match:
        long dt = abs((mt->m_trTS - ou.m_updTS).milliseconds());
        if  (dt < minSep)
        {
          minSep = dt;
          bestOU = &ou;
        }
      }
      // Found? -- Link the Match and the OrderUpdate!
      if (bestOU != nullptr)
      {
        bestOU->m_match = mt;
        mt->m_upd       = bestOU;
        // Also update the Time Discrepancy HistoGram:
        m_discrHist.Update(double(minSep));
      }
      else
      if (m_debugLevel >= 2)
        m_logger->warn
          ("MDReader_HotSpotFX::MapMatchesToUpdates: No OrderUpdate found "
           "for Match: TS={}, Px={}, Qty={}, BidAggr={}",
           mt->m_trTS.milliseconds(), mt->m_px, mt->m_qty, mt->m_bidAggr);
    }
    // All Matches and Periods done!
    //-----------------------------------------------------------------------//
    // Complete Aggressions:                                                 //
    //-----------------------------------------------------------------------//
    // Install yet-unset OrderBook Updates Time Ranges, Total Qtys, Avg Pxs:
    //
    for (Aggression& aggr: m_wkAggrs)
    {
      utxx::time_val updFrom = utxx::secs(INT_MAX); // 2038!!!
      utxx::time_val updTo   = utxx::secs(0);

      // NB: Within an Aggressions, Matches are sorted wrt their own TradeTimes
      // but not necessarily wrt corresp OrderBook Update Times (and the latter
      // may sometimes be unavailable), so need to traverse all Matches:
      double totQty = 0.0;
      double avgPx  = 0.0;

      for (Match* mt: aggr.m_matches)
      {
        assert(mt != nullptr);
        OrderUpdate* ou = mt->m_upd;

        if (utxx::likely(ou != nullptr))
        {
          updFrom = min<utxx::time_val>(updFrom, ou->m_updTS);
          updTo   = max<utxx::time_val>(updTo,   ou->m_updTS);
        }
        // Also update the TotQty and Tot (later Avg) Px:
        assert   (mt->m_qty > 0 && mt->m_px > 0.0);
        totQty += mt->m_qty;
        avgPx  += mt->m_px * double(mt->m_qty);
      }
      // Have we got a valid Update TS range (that is, at least one linked Ord-
      // erBook Update)?
      if (utxx::likely(updFrom <= updTo))
      {
        aggr.m_updTSs[0] = updFrom;
        aggr.m_updTSs[1] = updTo;
      }
      // Also install the TotQty and AvgPx (VWAP):
      if (totQty > 0)
        avgPx /= double(totQty);
      aggr.m_totQty = totQty;
      aggr.m_avgPx  = avgPx;
    }
  }
} // End namespace QuantSupport
} // End namespace MAQUETTE
