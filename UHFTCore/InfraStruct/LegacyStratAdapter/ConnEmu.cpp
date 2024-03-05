// vim:ts=2:et:sw=2
//===========================================================================//
//                               "ConnEmu.cpp":                              //
// MDC/OMC Emulator for Strategies running in the Dry-Run (Emulation) Mode:  //
//===========================================================================//
#include "Infrastructure/MiscUtils.h"
#include "Connectors/Configs_DB.h"
#include "StrategyAdaptor/ConnEmu.h"
#include "Persistence/SecDefsShM-ODB.hxx"

#include <odb/database.hxx>
#include <odb/mysql/database.hxx>
#include <odb/transaction.hxx>
#include <odb/query.hxx>
#include <odb/result.hxx>
#include <memory>
#include <algorithm>

namespace MAQUETTE
{
  using namespace Arbalete;
  using namespace MySQLDB;
  using namespace std;

  //=========================================================================//
  // "ConnEmu" Default Ctor:                                                 //
  //=========================================================================//
  ConnEmu::ConnEmu
  (
    EnvType       env_type,
    string const& strat_name,
    int           n_mdcs,
    int           debug_level
  )
  : m_nMDCs       (n_mdcs),
    m_SimulateFrom(),
    m_SimulateTo  (),
    m_DB          (NULL),
    m_DebugLevel  (debug_level),
    m_InstrsMap   (),
    m_SQLS        (NULL),
    m_RefsMap     (),
    m_ToStratBuff (64),
    m_SegmentName (),
    m_Segment     (),
    m_FromCache   (false),
    m_CacheAlloc  (),
    m_Cache       (NULL),
    m_CacheIdx    (-1)
  {
    //-----------------------------------------------------------------------//
    // Get the Environment:                                                  //
    //-----------------------------------------------------------------------//
    char const* segmName  = getenv("CONNEMU_USE_SEGMENT");
    char const* segmSzStr = getenv("CONNEMU_SEGMENT_SIZE_MB");
    char const* fromStr   = getenv("CONNEMU_SIMULATE_FROM");
    char const* toStr     = getenv("CONNEMU_SIMULATE_TO");

    if (env_type == EnvType::PROD)
      throw runtime_error
        ("ConnEmu Ctor: cannot run emulator in production environment!");

    if (segmName == NULL || segmSzStr == NULL ||
        fromStr  == NULL || toStr     == NULL)
      throw runtime_error("ConnEmu Ctor: CONNEMU_* env var(s) not set");

    m_SegmentName = string(segmName);

    // If the Segment does not exist, create it. Do NOT remove an existing seg-
    // ment -- it typically contains MktData loaded during previous calibration
    // runs:
    long sz = atol(segmSzStr) * 1024 * 1024;
    try
    {
      m_Segment.reset
        (new BIPC::fixed_managed_shared_memory
          (
            BIPC::open_or_create,
            m_SegmentName.c_str(),
            sz,
            // NB: It's OK to use m_SegmentName here instead of a ConnectorName
            // since we do not really get any data from a Connector!
            GetMktDataSegmMapAddr(m_SegmentName, env_type),
            BIPC::permissions(0660)
        ));
    }
    catch (exception const& exc)
    {
      SLOG(FATAL) << "ConnEmu::Ctor: Cannot open/create the ShM Segment="
                 << m_SegmentName << ", Size=" << sz << ": " << exc.what()
                 << endl;
    }
    assert(m_Segment.get() != NULL);

    m_SimulateFrom = DateTimeOfStr(string(fromStr));
    m_SimulateTo   = DateTimeOfStr(string(toStr));

    if (m_SimulateFrom >= m_SimulateTo)
      throw invalid_argument
            ("ConnEmu::Ctor: Invalid Time Range: SimulateFrom=" +
             DateTimeToStr(m_SimulateFrom)  + ", SimulateTo="   +
             DateTimeToStr(m_SimulateTo));

    //-----------------------------------------------------------------------//
    // Make a "MktData" MySQL Connection:                                    //
    //-----------------------------------------------------------------------//
    m_DB = mysql_init(NULL);
    if (m_DB == NULL)
      throw runtime_error("ConnEmu::Ctor: Cannot initialise MYSQL");

    MYSQL* mconn =
      mysql_real_connect
      (
        m_DB,
        NULL,   // Host -- will be localhost via a UNIX Domain Socket
        Config_MySQL_StratEnv.m_user.c_str(),
        Config_MySQL_StratEnv.m_passwd.c_str(),
        NULL,   // DataBase is not specified -- will be switched dynamically
        0,      // Port -- not required for local connections
        NULL,   // UNIX socket -- use the default
        0       // Client Flags
      );
    if (mconn != m_DB)
      throw runtime_error("ConnEmu::Ctor: Cannot connect to the DB: "+
                          string(mysql_error(m_DB)));

    SetUTCDiddle(m_SimulateFrom);
    //-----------------------------------------------------------------------//
    // XXX: Do NOT create the SQL Sequencer yet, as "InstrsMap" is yet empty //
    //-----------------------------------------------------------------------//
    //-----------------------------------------------------------------------//
    // ShM Cache Management:                                                 //
    //-----------------------------------------------------------------------//
    if (m_Segment == NULL)
      throw invalid_argument("ConnEmu::Ctor: ShM Segment is NULL");

    char const* ObjName = "ConnEmuCache";

    // Does the Cache exist yet?
    pair<CachedSDEQ*, size_t> cache =
      m_Segment->find<CachedSDEQ> (ObjName);

    if (cache.first == NULL || cache.second != 1)
    {
      // No, it does not exist, or is corrupt; remove it (for safety) first,
      // and re-create:
      (void) m_Segment->destroy<CachedSDEQ>(ObjName);

      // Create the Allocator:
      m_CacheAlloc.reset
        (new CachedSAllocator(m_Segment->get_segment_manager()));

      // Then create the Cache itself:
      m_Cache = m_Segment->construct<CachedSDEQ>(ObjName)(*m_CacheAlloc);

      // The Cache cannot be used as yet; we will only fill it in on this run:
      m_FromCache = false;
      m_CacheIdx  = -1;   // Will not be used...

      SLOG(INFO) << "ConnEmu::Ctor: Using data from the DB..." << endl;
    }
    else
    {
      // Yes, the Cache exists (eg from a previous calibration run); re-use it
      // until instructed to delete it:
      m_Cache     = cache.first;
      m_FromCache = true;
      m_CacheIdx  = 0;    // Start from 0

      SLOG(INFO) << "ConnEmu::Ctor: Using the ShM Cache..."    << endl;
    }
    // In any case:
    assert(m_Cache != NULL);
  }

  //=========================================================================//
  // "ConnEmu" Dtor:                                                         //
  //=========================================================================//
  ConnEmu::~ConnEmu()
  {
    // Remove the Sequencer (if exists):
    if (m_SQLS != NULL)
      delete m_SQLS;

    // Close the MySQL connection:
    if (m_DB != NULL)
      mysql_close(m_DB);

    // Remove the Cache if requested (eg at the end of Calibration). Actually,
    // we remove the whole segment:
    //
    if (getenv("CONNEMU_REMOVE_CACHE") != NULL)
      (void)BIPC::shared_memory_object::remove(m_SegmentName.c_str());

    m_Segment = NULL;
    m_Cache   = NULL;
  }

  //=========================================================================//
  // "ReBuildSequencer":                                                     //
  //=========================================================================//
  void ConnEmu::ReBuildSequencer()
  {
    // Remove the current Sequencer:
    if (m_SQLS != NULL)
    {
      delete m_SQLS;
      m_SQLS = NULL;
    }
    assert(m_SQLS == NULL);

    // Re-build the Sequencer if the "InstrsMap" is non-empty:
    if (!m_InstrsMap.empty())
      m_SQLS = new SQLSequencer(m_DB, m_InstrsMap, m_SimulateFrom);
  }

  //=========================================================================//
  // "GetSecDef":                                                            //
  //=========================================================================//
  SecDef const* ConnEmu::GetSecDef
  (
    Events::ObjName const& mdc_name,  // Same as DB NAme
    Events::SymKey  const& symbol,
    SecDefBuff*            output
  )
  const
  {
    assert(output != NULL);

    // NB: The following call does NOT throw exceptions but may return a NULL:
    SecDef const* secDefP = SecDefFromDB(mdc_name, symbol, output);

    // If non-NULL, send it to the Strategy as an event:
    if (secDefP != NULL)
    {
      Events::EventMsgHV secMsg
        (Events::EventMsgH<SecDef>(Events::EventT::SecDefUpdate, secDefP));

      assert(secMsg.m_ref == 0);
      m_ToStratBuff.push_back(secMsg);
    }

    // Return the ptr even if it is empty:
    return secDefP;
  }

  //=========================================================================//
  // "Subscribe":                                                            //
  //=========================================================================//
  void ConnEmu::Subscribe
  (
    Events::ObjName const& mdc_name,  // Same as DB Name
    Events::SymKey  const& symbol,
    Events::ClientRef      ref        // Same as MDC#
  )
  {
    //-----------------------------------------------------------------------//
    // XXX: Currently, all Symbols must be distinct:                         //
    //-----------------------------------------------------------------------//
    if (ref < 0 || ref >= m_nMDCs)
      throw invalid_argument
            ("ConnEmu::Subscribe: Unexpected Ref=" + to_string(ref));

    RefsMap::const_iterator rit = m_RefsMap.find(symbol);
    if (rit != m_RefsMap.end())
    {
      if (rit->second.first != ref || rit->second.second != mdc_name)
        throw invalid_argument
              ("ConnEmu::Subscribe: Repreated Symbol="     +
               Events::ToString(symbol) + ", but new Ref=" + to_string(ref) +
               " and/or new MDCName="   + Events::ToString(mdc_name)        +
               ": Not allowed with curr implementation");
      else
        return; // Nothing to do
    }
    else
      // Memoise this association: Symbol => (Ref, MDCName):
      m_RefsMap[symbol] = make_pair(ref, mdc_name);

    //-----------------------------------------------------------------------//
    // New Symbol:                                                           //
    //-----------------------------------------------------------------------//
    SQLSequencer::InstrsMap::iterator it = m_InstrsMap.find(mdc_name);

    // Is this MDC/DB already in the InstrsMap?
    if (it == m_InstrsMap.end())
    {
      // No -- install it now, unless it exceeds the declared "m_nMDCs":
      if (int(m_InstrsMap.size()) >= m_nMDCs)
        throw runtime_error
              ("ConnEmu::Subscribe: Extraneous MDC (too many): " +
               Events::ToString(mdc_name));

      // If within the limit:
      pair<SQLSequencer::InstrsMap::iterator, bool> ins =
        m_InstrsMap.insert(make_pair(mdc_name, vector<Events::SymKey>()));

      assert(ins.second);
      it = ins.first;
    }

    // In any case (even if globally-non-unique Symbols are permitted), Symbol
    // must be unique in its per-MDC vector:
    //
    vector<Events::SymKey>& symbols = it->second;

    if (find(symbols.begin(), symbols.end(), symbol) != symbols.end())
      throw runtime_error
            ("ConnEmu::Subscribe: Invariant Violation: Repeated Symbol="
            + Events::ToString(symbol) +" in "+ Events::ToString(mdc_name));

    // We can finally append it to the vector:
    symbols.push_back(symbol);

    //-----------------------------------------------------------------------//
    // IMPORTANT: Build / Re-Build the SQL Sequencer on "InstrsMap" update:  //
    //-----------------------------------------------------------------------//
    ReBuildSequencer();
  }

  //=========================================================================//
  // "Unsubscribe":                                                          //
  //=========================================================================//
  void ConnEmu::Unsubscribe
  (
    Events::ObjName const& mdc_name,  // Same as DB Name
    Events::SymKey  const& symbol
  )
  {
    //-----------------------------------------------------------------------//
    // Remove Symbol from "InstrsMap":                                       //
    //-----------------------------------------------------------------------//
    SQLSequencer::InstrsMap::iterator it = m_InstrsMap.find(mdc_name);
    if (it == m_InstrsMap.end())
    {
      SLOG(WARNING) << "ConnEmu::Unsubscribe(Emulated): MDC not found in "
                      "InstrsMap: " << Events::ToString(mdc_name) << endl;
      return;
    }
    // Get the vector of Symbols:
    vector<Events::SymKey>& symbols = it->second;

    // Remove the Symbol from that vector (if it is there):
    vector<Events::SymKey>::iterator vit =
      find(symbols.begin(), symbols.end(), symbol);

    if (vit == symbols.end())
    {
      SLOG(WARNING) << "ConnEmu:Unsubscribe: Symbol="
                   << Events::ToString(symbol)
                   << " not found in InstrsMap for "
                   << Events::ToString(mdc_name) << endl;
      return;
    }
    // Otherwise: Actually remove it:
    symbols.erase(vit);

    //-----------------------------------------------------------------------//
    // Possibly remove MDC Name from "InstrsMap":                            //
    //-----------------------------------------------------------------------//
    // If the vector becomes empty, remove the "mdc_name" as well -- it can be
    // re-installed on a next subscription:
    if (symbols.empty())
      m_InstrsMap.erase(mdc_name);

    // Sequencer needs to be rebuilt (or removed if there are no more instrum-
    // ents at all):
    ReBuildSequencer();

    //-----------------------------------------------------------------------//
    // Also remove Symbol from "RefsMap":                                    //
    //-----------------------------------------------------------------------//
    m_RefsMap.erase(symbol);
  }

  //=========================================================================//
  // "UnsubscribeAll":                                                       //
  //=========================================================================//
  void ConnEmu::UnsubscribeAll(Events::ObjName const& mdc_name)
  {
    // First of all, for all Symbosl within this Connector / DB, remove them
    // from "RefsMap":
    vector<Events::SymKey> const& symbols = m_InstrsMap[mdc_name];

    // If "mdc_name" is not currently present, produce a warining and return:
    if (symbols.empty())
    {
      SLOG(WARNING) << "ConnEmu::UnsubscribeAll: " << "MDC="
                   << Events::ToString(mdc_name)  << " is not in use" << endl;
      return;
    }
    for (int i = 0; i < int(symbols.size()); ++i)
      m_RefsMap.erase(symbols[i]);

    // Then remove this Connector / DB from the map, and rebuild (or remove)
    // the SQL Sequencer:
    m_InstrsMap.erase(mdc_name);
    ReBuildSequencer();
  }

  //=========================================================================//
  // "GetNextEventCommon":                                                   //
  //=========================================================================//
  // Returns "true" / "false" depending on whether the "msg" has been filled:
  //
  bool ConnEmu::GetNextEventCommon
  (
    Events::EventMsgHV* msg,
    DateTime*           currDT
  )
  const
  {
    assert(msg != NULL && currDT != NULL);

    // To be on a safe side, clear out the msg:
    memset(msg, '\0', sizeof(Events::EventMsgHV));

    // Check the SQL Sequencer -- even if not used directly here, it must be
    // available:
    if (m_SQLS == NULL)
      throw runtime_error("ConnEmu::GetNextEvenCommon: SQLSequencer is NULL: "
                          "No instruments subscribed?");

    // Is it the time to exit now?
    *currDT = CurrUTC();  // Simulated time
    if (*currDT >= m_SimulateTo)
    {
      msg->m_tag = Events::EventT::StratExit;
      // Other flds are irrelevant in this case...
      return true;
    }

    // If there are readily-available msgs in the buffer, return one:
    if (!m_ToStratBuff.empty())
    {
      *msg = m_ToStratBuff.front();
      m_ToStratBuff.pop_front();
      return true;
    }
    else
      return false;
  }

  //=========================================================================//
  // "GetNextEventFromCache":                                                //
  //=========================================================================//
  // Check if the ShM Cache is used, and the next event is available from the
  // Cache:
  bool ConnEmu::GetNextEventFromCache(DateTime deadline, CachedS* cs)
  const
  {
    assert(cs != NULL && m_Cache != NULL && m_CacheIdx >= 0);

    if (m_CacheIdx >= int(m_Cache->size()))
      // Simulation is done:
      return false;

    // Generic case: Got it, but still need to check the "deadline":
    *cs = (*m_Cache)[m_CacheIdx];

    assert(cs->m_ts != DateTime());
    if (cs->m_ts > deadline)
      // Can't use it:
      return false;

    // Otherwise: Will use it, and advance the index:
    ++m_CacheIdx;

    switch (cs->m_tag)
    {
      //---------------------------------------------------------------------//
      case SQLSequencer::ResultT::OrderBook:
      //---------------------------------------------------------------------//
      {
        // XXX: The following copying is not very efficient:
        //
        SQLSequencer::OrderBooksMap::iterator it =
          m_NextOrderBooks.find(cs->m_symbol);

        if (it != m_NextOrderBooks.end())
        {
          // Update the Book SnapShot:
          assert(it->second.get() != NULL);
          *(it->second) = cs->m_U.m_OB;
        }
        else
          // Create a new entry:
          m_NextOrderBooks[cs->m_symbol] =
            make_shared<OrderBookSnapShot>(cs->m_U.m_OB);
        return true;
      }

      //---------------------------------------------------------------------//
      case SQLSequencer::ResultT::Trade:
      //---------------------------------------------------------------------//
      {
        // Simply copy it over (XXX: not very efficient):
        m_NextTrade = cs->m_U.m_Tr;
        return true;
      }

      //---------------------------------------------------------------------//
      default:
      //---------------------------------------------------------------------//
        assert(false);
        return false;   // No return, actually
    }
  }

  //=========================================================================//
  // "GetNextEvent", without a deadline:                                     //
  //=========================================================================//
  void ConnEmu::GetNextEvent(Events::EventMsgHV* msg) const
  {
    // Common part: Is the next event available immediately from the buffer, or
    // we are at the end of simulation time?
    DateTime  currDT;
    if (GetNextEventCommon(msg, &currDT))
      return;

    // Otherwise:
    if (m_FromCache)
    {
      //---------------------------------------------------------------------//
      // Get it from ShM Cache:                                              //
      //---------------------------------------------------------------------//
      // If there are no more events, simulation is done:
      DateTime deadline = m_SimulateTo;
      CachedS  cs;
      if (!GetNextEventFromCache(deadline, &cs))
      {
        cs.m_tag = SQLSequencer::ResultT::Nothing;
        cs.m_ts  = deadline;
        // "ProcessMktDataEvent" will terminate the simulation
      }
      else
        assert(cs.m_ts != DateTime() && cs.m_ts <= deadline);

      // Otheriwise: the event data structs are already in "m_NextOrderBooks"
      // or "m_NextTrade":
      ProcessMktDataEvent(cs.m_tag, cs.m_symbol, deadline, msg);
    }
    else
    {
      //---------------------------------------------------------------------//
      // Get it from the DB:                                                 //
      //---------------------------------------------------------------------//
      // Try to get data from a DB  with increasing deadlines, until we get so-
      // mething.
      // XXX: what is the reasonable time step here?  Currently, the DB is for
      // tick data, so we start with 1 sec and increase the delays by the fac-
      // tor of 2 until we get something,   or reach the end of the simulation
      // time:
      Time     delay = TimeGener::seconds(1);
      assert(m_SQLS != NULL);

      while (true)
      {
        DateTime deadline = min<DateTime>(currDT + delay, m_SimulateTo);

        pair<SQLSequencer::ResultT, Events::SymKey> res =
          m_SQLS->GetNextEntry(deadline, &m_NextOrderBooks, &m_NextTrade);

        if (res.first == SQLSequencer::ResultT::Nothing)
        {
          SetUTCDiddle(deadline);
          // Double the delay and try again, unless we are already at the end:
          if (deadline >= m_SimulateTo)
            // "ProcessMktDataEvent" will issue an Exit signal:
            break;
          else
          {
            // Otherwise, go to the next iteration:
            delay += delay;
            continue;
          }
        }
        // Process the result and return it to the caller (in particular, the
        // following function fills in the Cache if required):
        assert(deadline <= m_SimulateTo);
        ProcessMktDataEvent(res.first, res.second, deadline, msg);
      }
    }
  }

  //=========================================================================//
  // "GetNextEvent", with a deadline:                                        //
  //=========================================================================//
  bool ConnEmu::GetNextEvent
  (
    Events::EventMsgHV* msg,
    Arbalete::DateTime  deadline
  )
  const
  {
    // Common part: Is the next event available immediately from the buffer, or
    // we are at the end of the simulation time:
    DateTime currDT;
    if (GetNextEventCommon(msg, &currDT))
      return true;

    // Otherwise:
    // Wait until the specified deadline (or the end of the simulated time):
    DateTime limit = min<DateTime>(deadline, m_SimulateTo);

    if (m_FromCache)
    {
      //---------------------------------------------------------------------//
      // Get it from ShM Cache:                                              //
      //---------------------------------------------------------------------//
      // If there are no more events, simulation is done:
      CachedS cs;
      if (!GetNextEventFromCache(limit, &cs))
      {
        if (limit < m_SimulateTo)
        {
          // Really got nothing up until the "limit":
          SetUTCDiddle(limit);
          return false;
        }
        else
          // Otherwise, still need to invoke "ProcessMktDataEvent", only to
          // terminate the simulation -- so fall through:
          cs.m_tag = SQLSequencer::ResultT::Nothing;
      }
      // Otheriwise: the event data structs are already in "m_NextOrderBooks"
      // or "m_NextTrade":
      ProcessMktDataEvent(cs.m_tag, cs.m_symbol, limit, msg);
      return true;
    }
    else
    {
      //---------------------------------------------------------------------//
      // Get it from the DB:                                                 //
      //---------------------------------------------------------------------//
      pair<SQLSequencer::ResultT, Events::SymKey> res =
        m_SQLS->GetNextEntry(limit, &m_NextOrderBooks, &m_NextTrade);

      if (res.first == SQLSequencer::ResultT::Nothing  && limit < m_SimulateTo)
      {
          // "limit" is not the ultimate end of the simulation time, so really
          // got nothing:
          SetUTCDiddle(limit);
          return false;
      }

      // In all other cases (including end of the simulation time): process it.
      // In particular, the following function updates the Cache if required:
      ProcessMktDataEvent(res.first, res.second, limit, msg);
      return true;
    }
  }

  //=========================================================================//
  // "ProcessMktDataEvent":                                                  //
  //=========================================================================//
  void ConnEmu::ProcessMktDataEvent
  (
    SQLSequencer::ResultT resT,
    Events::SymKey const& symbol,
    DateTime              deadline,
    Events::EventMsgHV*   msg
  )
  const
  {
    assert(msg != NULL);

    switch (resT)
    {
      //---------------------------------------------------------------------//
      case SQLSequencer::ResultT::Nothing:
      //---------------------------------------------------------------------//
      {
        // We must be at the end of simulation time: Produce the "Exit" msg:
        assert(deadline >= m_SimulateTo);
        msg->m_tag = Events::EventT::StratExit;
        SetUTCDiddle(m_SimulateTo);
        return;
      }
      //---------------------------------------------------------------------//
      case SQLSequencer::ResultT::OrderBook:
      //---------------------------------------------------------------------//
      {
        //-------------------------------------------------------------------//
        // There was an OrderBook update for the specified "symbol":         //
        //-------------------------------------------------------------------//
        // XXX: We return a direct ptr to the "OrderBook" as provided   by the
        // "SQLSequencer. But this ptr may not even be valid after next update;
        // we assume that the Client will consume it immediately:
        //
        // The "symbol" must be present in all our maps:
        //
        RefsMap::const_iterator crit = m_RefsMap.find(symbol);

        SQLSequencer::OrderBooksMap::iterator cobit =
          m_NextOrderBooks.find(symbol);

        if (crit == m_RefsMap.end() || cobit == m_NextOrderBooks.end())
          throw runtime_error
                ("ConnEmu::ProcessMktDataEvent: Symbol=" +
                 Events::ToString(symbol) + " not subscribe for?");

        Events::ClientRef ref =  crit->second.first;

        OrderBookSnapShot* ob = cobit->second.get();
        assert(ob != NULL);

        // XXX: Verify the OrderBook received -- due to peculiarities of data
        // extraction from the DB, it may contain inconsistencies:
        if (ob->m_currBids >= 1 &&
           (ob->m_bids[0].m_price <= 0.0 || ob->m_bids[0].m_qty <= 0))
          ob->m_currBids = 0;

        if (ob->m_currAsks >= 1 &&
           (ob->m_asks[0].m_price <= 0.0 || ob->m_asks[0].m_qty <= 0))
          ob->m_currAsks = 0;

        // TODO: More checks...

        DateTime now       = ob->m_eventTS;
        msg->m_tag         = Events::EventT::OrderBookUpdate;
        msg->m_ref         = ref;        // For an MDC event
        msg->m_U.m_bookPtr = (void const*)(ob);

        //-------------------------------------------------------------------//
        // Possibly put it into Cache:                                       //
        //-------------------------------------------------------------------//
        if (!m_FromCache)
        {
          CachedS cs;
          cs.m_tag    = resT;
          cs.m_symbol = symbol;
          cs.m_ts     = now;
          cs.m_U.m_OB = *ob;

          m_Cache->push_back(cs); // XXX: "bad_alloc" here if insufficient ShM
        }

        //-------------------------------------------------------------------//
        // Now check if this mkt move also makes any of our orders filled:   //
        //-------------------------------------------------------------------//
        // XXX: in the curr implementation, mkt impact of our positions is com-
        // pletely ignored.
        // XXX: we currently maintain just a single queue of Indications (rath-
        // er than per-Connector queues)  because all Symbols  in the Emulated
        // Mode are supposed to be unique:
        //
        for (int i = 0; i < int(m_Indications.size()); )
        {
          Events::Indication const& ind   = m_Indications[i];

          // If the Indication was not acted upon immediately, but rather put
          // into the queue, then it is either a Limit order, or (very rarely)
          // a Market order:
          assert(ind.IsNewLimitOrder() || ind.IsNewMarketOrder());

          bool isBuy = ind.IsBuy();

          // First of all, expunge the expired Indications (for any Symbols):
          if (ind.m_Expires != DateTime() && now > ind.m_Expires)
          {
            SLOG(INFO) << "ConnEmu::ProcessMktDataEvent: Order #"
                      << ToHexStr16(ind.m_OrdID) << " has EXPIRED" << endl;

            m_Indications.erase(m_Indications.begin() + i);
          }
          // If the Symbol is different, the Indication is not applicable:
          else
          if (ind.m_Symbol != symbol)
            ++i;

          // Limit Order? Check the prices:
          else
          if (ind.IsNewLimitOrder()                   &&
             (( isBuy      && ob->m_currAsks >= 1     &&
               ind.m_Price >= ob->m_asks[0].m_price)  ||
              (!isBuy      && ob->m_currBids >= 1     &&
               ind.m_Price <= ob->m_bids[0].m_price)))
          {
            // Convert it into a Fill Event (at the Indicated Price, since the
            // Indication arrived earlier) and remove the Indication:
            ReportFill(ind, ref, now, ind.m_Price);
            m_Indications.erase(m_Indications.begin() + i);
          }

          // Market Order which could not be filled immediately because the
          // corresp side of the Order Book was empty?
          else
          if (ind.IsNewMarketOrder()        &&
             ((isBuy && ob->m_currAsks >= 1)||(!isBuy && ob->m_currBids >= 1)))
          {
            // Similar to above, but use the Mkt Price, rather than Indicated
            // Price:
            double fillPx = (isBuy ? ob->m_asks : ob->m_bids)[0].m_price;
            ReportFill(ind, ref, now, fillPx);
          }
          else
            // In all other cases: Next iteration:
            ++i;
        }
        //-------------------------------------------------------------------//
        // Advance the timer to the timestamp of this event:                 //
        //-------------------------------------------------------------------//
        if (now > deadline)
          throw logic_error
                ("ConnEmu::ProcessMktDataEvent: OrderBookTS=" +
                 DateTimeToStr(now)      +   ", Deadline="    +
                 DateTimeToStr(deadline));

        SetUTCDiddle(now);
        return;
      }
      //---------------------------------------------------------------------//
      case SQLSequencer::ResultT::Trade:
      //---------------------------------------------------------------------//
      {
        // There was a Trade in the specified "symbol":
        // XXX: We return a direct ptr to the "NextTrade" as provided   by the
        // "SQLSequencer. But this ptr may not even be valid after next update;
        // we assume that the Client will consume it immediately:
        //
        // The "symbol" must be present in all our maps:
        //
        RefsMap::const_iterator crit = m_RefsMap.find(symbol);
        if (crit == m_RefsMap.end())
          throw runtime_error
                ("ConnEmu::ProcessMktDataEvent: Symbol=" +
                 Events::ToString(symbol) + " not subscribe for?");

        if (symbol != m_NextTrade.m_symbol)
          throw logic_error
                ("ConnEmu::ProcessMktDataEvent: SymbolFromSQLSeq=" +
                 Events::ToString(symbol) + ", SymbolFromTrade="   +
                 Events::ToString(m_NextTrade.m_symbol));

        Events::ClientRef ref = crit->second.first;

        DateTime now       = m_NextTrade.m_eventTS;
        msg->m_tag         = Events::EventT::TradeBookUpdate;
        msg->m_ref         = ref;         // For an MDC event
        msg->m_U.m_bookPtr = (void const*)(&m_NextTrade);

        //-------------------------------------------------------------------//
        // Possibly put it into Cache:                                       //
        //-------------------------------------------------------------------//
        if (!m_FromCache)
        {
          CachedS cs;

          cs.m_tag    = resT;
          cs.m_symbol = symbol;
          cs.m_ts     = now;
          cs.m_U.m_Tr = m_NextTrade;

          m_Cache->push_back(cs); // XXX: "bad_alloc" here if insufficient ShM
        }

        //-------------------------------------------------------------------//
        // Advance the timer to the timestamp of this event:                 //
        //-------------------------------------------------------------------//
        if (now > deadline)
          throw logic_error
                ("ConnEmu::ProcessMktDataEvent: OrderBookTS=" +
                 DateTimeToStr(now)      +   ", Deadline="    +
                 DateTimeToStr(deadline));

        SetUTCDiddle(now);
        return;
      }

      //---------------------------------------------------------------------//
      default: assert(false);
      //---------------------------------------------------------------------//
    }
  }

  //=========================================================================//
  // "SendIndication":                                                       //
  //=========================================================================//
  void ConnEmu::SendIndication(Events::Indication const& ind) const
  {
    //-----------------------------------------------------------------------//
    // What is the Indication type?                                          //
    //-----------------------------------------------------------------------//
    // If it is a Limit or Mkt Order, we need the corresp Order Book and Ref:
    //
    bool limitOrder = ind.IsNewLimitOrder();
    bool mktOrder   = ind.IsNewMarketOrder();
    assert(!(limitOrder && mktOrder));

    // Get the Client's Reference -- it must always be quited in responses:
    //
    RefsMap::const_iterator crit = m_RefsMap.find(ind.m_Symbol);
    if (crit == m_RefsMap.end())
      throw runtime_error
            ("ConnEmu::SendIndication: Symbol=" +
             Events::ToString(ind.m_Symbol) + " not subscribed for (no Ref)?");

    Events::ClientRef ref = crit->second.first;

    //-----------------------------------------------------------------------//
    if (limitOrder || mktOrder)
    //-----------------------------------------------------------------------//
    {
      SLOG(INFO) << "ConnEmu::SendIndication: New "
                << (limitOrder ? "Limit" : "Market") << " Order #"
                << ToHexStr16(ind.m_OrdID) << endl;

      SQLSequencer::OrderBooksMap::iterator cobit =
        m_NextOrderBooks.find(ind.m_Symbol);

      if (cobit == m_NextOrderBooks.end())
        throw runtime_error
              ("ConnEmu::SendIndication: Symbol=" +
               Events::ToString(ind.m_Symbol)+ " not subscribed for (no OB)?");

      OrderBookSnapShot const* ob = cobit->second.get();
      assert(ob != NULL);

      // So: Can the order be filled now?
      bool mktFill =
        mktOrder   &&
        (( ind.IsBuy() && ob->m_currAsks >= 1) ||
         (!ind.IsBuy() && ob->m_currBids >= 1));

      bool limitFill =
        limitOrder &&
        (( ind.IsBuy() && ob->m_currAsks >= 1 &&
           ind.m_Price >= ob->m_asks[0].m_price) ||
         (!ind.IsBuy() && ob->m_currBids >= 1 &&
           ind.m_Price <= ob->m_bids[0].m_price));

      if (limitFill && m_DebugLevel >= 2)
        SLOG(INFO) << "ConnEmu::SendIndication: OrderID="
                  << ToHexStr16(ind.m_OrdID)  << ": FILLED IMMEDIATELY: IsBuy="
                  << ind.IsBuy() << ", LimitPx=" << ind.m_Price
                  << (ind.IsBuy() ? ", MinAsk=": ", MaxBid=")
                  << (ind.IsBuy() ? ob->m_asks[0].m_price
                                  : ob->m_bids[0].m_price)
                  << endl;

      if (mktFill || limitFill)
      {
        // Then it is filled immediately (XXX: we assume zero market impact, so
        // the Order Book quantities are NOT affected by our fill), at the BEST
        // mkt prices. TODO: more accurate quantities and mkt impact management?
        //
        // We bought it at the best Ask price or sold it at the best Bid price
        // (NB: not the Indicated price, as the latter has arrived later!):
        //
        double fillPx = (ind.IsBuy() ? ob->m_asks : ob->m_bids)[0].m_price;
        ReportFill(ind, ref, ind.m_TS, fillPx);
      }
      else
      {
        // Cannot fill it in immediately, so queue it:
        m_Indications.push_back(ind);

        // XXX: In order to prevent the Indication from staying  in the queue
        // forever, set the Expiration time. XXX: there is no support for Tra-
        // ding Calendars here yet, so we simply add 24 hours to the curr time:
        //
        m_Indications.back().m_Expires = ind.m_TS + TimeGener::hours(24);

        // Report the order as "new" to the Client, -- same as "modified":
        ReportModify(ind, ref);
      }
    }
    else
    //-----------------------------------------------------------------------//
    if (ind.IsOrderCancelReq())
    //-----------------------------------------------------------------------//
    {
      SLOG(INFO) << "ConnEmu::SendIndication: CancelRequest for Order# "
                << ToHexStr16(ind.m_OrdID) << endl;

      // Find this OrderID and remove it from "m_Indications":
      bool removed = false;
      for (deque<Events::Indication>::iterator it = m_Indications.begin();
           it != m_Indications.end(); ++it)
        if (it->m_OrdID == ind.m_OrdID && !it->IsNewMarketOrder())
        {
          // NB: We do not cancel Mkt Orders (even if they are not filled imme-
          // diately for some reason) for consistency with our FIX engine beha-
          // viour.
          // Insert a response notifying the Strategy that the order has been
          // cancelled. NB: the 1st arg is the ORIGINAL indication:
          ReportCancel(*it, ref);
          m_Indications.erase(it);
          removed = true;
          break;
        }
      if (!removed)
        // The order has not been found -- respond with "CancelReject":
        ReportCancelReject(ind.m_OrdID, ind.IsBuy(), ref);
    }
    else
    //-----------------------------------------------------------------------//
    if (ind.IsOrderAmendReq())
    //-----------------------------------------------------------------------//
    {
      // Find this OrderID and replace its params:
      SLOG(INFO) << "ConnEmu::SendIndication: AmendRequest for Order# "
                << ToHexStr16(ind.m_OrdID) << endl;

      bool replaced = false;
      for (deque<Events::Indication>::iterator it = m_Indications.begin();
           it != m_Indications.end();  ++it)
        if (it->m_OrdID == ind.m_OrdID)
        {
          // Perform modifications:
          if (ind.m_Price > 0.0)
            it->m_Price = ind.m_Price;
          if (ind.m_Qty > 0)
            it->m_Qty   = ind.m_Qty;
          replaced = true;

          // Insert a response notifying the Strategy that the order has been
          // replaced:
          ReportModify(*it, ref);
          break;
        }
      if (!replaced)
        // The order has not been found -- respond with "CancelReject":
        ReportCancelReject(ind.m_OrdID, ind.IsBuy(), ref);
    }
    else
    //-----------------------------------------------------------------------//
    if (ind.IsMassCancelReq())
    //-----------------------------------------------------------------------//
    {
      // Find this OrderID and replace its params:
      SLOG(INFO) << "ConnEmu::SendIndication: MassCancelRequest..." << endl;

      // Go through all currently active Indications and cancel the applicable
      // ones:
      for (deque<Events::Indication>::iterator it = m_Indications.begin();
           it != m_Indications.end();)
      {
        // (1) Filtering by Client and Cancellation Target: because in the Emu-
        // lated mode,   there is only 1 Client, the following equality should
        // always hold:
        bool cond1 = (ind.m_ClientH == it->m_ClientH);

        // (2) Filetering by Side:
        bool cond2 =
          (ind.IsBothSidesAF()                  ||
          (ind.IsBuyAF      () &&  it->IsBuy()) ||
          (ind.IsSellAF     () && !it->IsBuy()));

        // NB: Our FIX (and emulated) protocol engines currently do not allow
        // us to cancel Mkt Orders directly, but in Mass Cancel, that's OK...
        if (cond1 && cond2)
        {
          // Really cancel it:
          ReportCancel(*it, ref);
          m_Indications.erase(it);
        }
        else
          ++it;
      }
    }
    // XXX: Other Indications (eg "OrderStatus") are ignored
  }

  //=========================================================================//
  // "ReportFill":                                                           //
  //=========================================================================//
  void ConnEmu::ReportFill
  (
    Events::Indication const& ind,
    Events::ClientRef         ref,  // This is an original MDC ref for Symbol
    DateTime                  now,
    double                    price
  )
  const
  {
    Events::AOS aos;
    aos.m_origID       = ind.m_OrdID;
    aos.m_origTS       = ind.m_TS;
    aos.m_accCrypt     = ind.m_AccCrypt;
    aos.m_clientH      = ind.m_ClientH;
    aos.m_symbol       = ind.m_Symbol;
    aos.m_isBuy        = ind.IsBuy();
    // Ignore all Intend / Acked / Confed / Ultim parts...
    // We can buy or sell at our indicated price -- XXX: assume full qty:
    aos.m_cumQty       = ind.m_Qty;
    aos.m_leavesQty    = 0;
    aos.m_lastQty      = ind.m_Qty;
    aos.m_avgPx        = price;
    aos.m_lastPx       = price;
    aos.m_exchFee      = 0;            // XXX: TODO !!!
    aos.m_brokFee      = 0;            // XXX: TODO !!!
    aos.m_tradeTS      = now;
    // The Event part:
    aos.m_eventTS      = now;
    aos.m_eventStatus  = Events::AOS::ReqStatusT::Filled;
    aos.m_eventID      = ind.m_OrdID;  // Same...
    aos.m_eventManual  = false;

    // Form the msg:
    Events::EventMsgHV fill(Events::EventT::OrderFilled, aos);
    // MDCs = OMCs, but refs are different:
    fill.m_ref = m_nMDCs + ref;

    // Push it into the buffer:
    m_ToStratBuff.push_back(fill);

    // Diagnostics:
    SLOG(INFO) << "ConnEmu::ReportFill: Order #" << ToHexStr16(ind.m_OrdID)
              << " FILLED" << endl;
  }

  //=========================================================================//
  // "ReportModify":                                                         //
  //=========================================================================//
  // NB: This method is invoked AFTER "ind" has been modified, so actually, it
  // exactly same for "New" orders:
  //
  void ConnEmu::ReportModify
  (
    Events::Indication const& ind,
    Events::ClientRef         ref
  )
  const
  {
    Events::AOS aos;
    aos.m_origID        = ind.m_OrdID;
    aos.m_origTS        = ind.m_TS;
    aos.m_accCrypt      = ind.m_AccCrypt;
    aos.m_clientH       = ind.m_ClientH;
    aos.m_symbol        = ind.m_Symbol;
    aos.m_isBuy         = ind.IsBuy();
    // Order status sections:
    // (Sections not explicitly filled in are zeroed-out):
    aos.m_confedPx      = ind.m_Price;
    aos.m_confedQty     = ind.m_Qty;
    aos.m_confedTS      = ind.m_TS;

    aos.m_ultimPx       = ind.m_Price;
    aos.m_ultimQty      = ind.m_Qty;
    aos.m_ultimTS       = ind.m_TS;
    // Nothing is filled yet:
    aos.m_cumQty        = 0;
    aos.m_leavesQty     = ind.m_Qty;
    aos.m_lastQty       = 0;
    aos.m_avgPx         = 0.0;
    aos.m_lastPx        = 0.0;
    aos.m_exchFee       = 0.0;          // XXX: TODO !!!
    aos.m_brokFee       = 0.0;          // XXX: TODO !!!
    aos.m_tradeTS       = DateTime();
    // The Event part:
    aos.m_eventTS       = ind.m_TS;
    aos.m_eventStatus   = Events::AOS::ReqStatusT::Confed;
    aos.m_eventID       = ind.m_OrdID;  // Same...
    aos.m_eventManual   = false;

    // Form the msg:
    Events::EventMsgHV fill(Events::EventT::MiscStatusChange, aos);
    // MDCs = OMCs, but refs are different:
    fill.m_ref = m_nMDCs + ref;

    // Push it into the buffer:
    m_ToStratBuff.push_back(fill);

    // Diagnostics:
    SLOG(INFO) << "ConnEmu::ReportModify: Order #" << ToHexStr16(ind.m_OrdID)
              << " AMENDED" << endl;
  }

  //=========================================================================//
  // "ReportCancel":                                                         //
  //=========================================================================//
  void ConnEmu::ReportCancel
  (
    Events::Indication const& ind,    // Original Indication being cancelled
    Events::ClientRef         ref
  )
  const
  {
    // Now the AOS:
    Events::AOS aos;
    aos.m_origID       = ind.m_OrdID,
    aos.m_origTS       = ind.m_TS;
    aos.m_accCrypt     = ind.m_AccCrypt;
    aos.m_clientH      = ind.m_ClientH;
    aos.m_symbol       = ind.m_Symbol;
    aos.m_isBuy        = ind.IsBuy();
    // Ignore all Intend / Acked / Confed / Ultim parts...
    aos.m_cumQty       = 0;    // Not filled!
    aos.m_leavesQty    = ind.m_Qty;
    aos.m_lastQty      = 0;
    aos.m_avgPx        = 0.0;
    aos.m_lastPx       = 0.0;
    aos.m_exchFee      = 0.0;         // XXX: Cancellation fee?
    aos.m_brokFee      = 0.0;         // XXX: ???
    aos.m_tradeTS      = DateTime();  // This is NOT a trade!
    // The Event part:
    aos.m_eventTS      = ind.m_TS;
    aos.m_eventStatus  = Events::AOS::ReqStatusT::Cancelled;
    aos.m_eventID      = ind.m_OrdID; // Same...
    aos.m_eventManual  = false;

    // Form the msg:
    Events::EventMsgHV canc(Events::EventT::OrderCancelled, aos);
    // MDCs = OMCs, but refs are different:
    canc.m_ref = m_nMDCs + ref;

    // Push it into the buffer:
    m_ToStratBuff.push_back(canc);

    // Diagnostics:
    SLOG(INFO) << "ConnEmu::ReportCancel: Order #" << ToHexStr16(ind.m_OrdID)
              << " CANCELLED" << endl;
  }

  //=========================================================================//
  // "ReportCancelReject":                                                   //
  //=========================================================================//
  void ConnEmu::ReportCancelReject
  (
    Events::OrderID    ord_id,         // Original Indication being cancelled
    bool               is_buy,
    Events::ClientRef  ref
  )
  const
  {
    // Return the "ErrInfo" to the Client:
    // XXX: Unlike the real FIX protocol implementation, here we send a "ver-
    // bal" error msg only, because if an order   to be cancelled / replaced
    // does not exist, then surely, either it's Client's error, or the order
    // has already been cancelled or filled, and the Client was notified:
    // Notionally, the corresp AOS is to be removed on the Client side (it is
    // probably removed already):
    //
    Events::EventMsgHV rej
      (ord_id, Events::EmptySymKey(), is_buy, "Order does not exist", true);

    // Push it into the buffer:
    m_ToStratBuff.push_back(rej);
    // Diagnostics:
    SLOG(INFO) << "ConnEmu::ReportCancelReject: Order #" << ToHexStr16(ord_id)
              << " Cancel/Amend REJECTED" << endl;
  }
}
