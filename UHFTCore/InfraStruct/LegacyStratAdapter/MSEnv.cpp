// vim:ts=2:et:sw=2
//===========================================================================//
//                                 "MSEnv.cpp":                              //
//             Strategy Environment and its API, for use with MS             //
//===========================================================================//
#ifndef  JSON_USE_MSGPACK
#define  JSON_USE_MSGPACK
#endif
#include <Common/DateTime.h>
#include <Common/Maths.h>
#include <Infrastructure/MiscUtils.h>
#include <Connectors/Json.h>
#include <Connectors/XConnector_MktData.h>
#include <Connectors/XConnector_OrderMgmt.h>
#include <Connectors/Configs_Conns.h>
#include <Connectors/Configs_DB.h>
#include <Connectors/Configs_FIX.h>
#include <StrategyAdaptor/EventTypes.h>
#include <Infrastructure/SecDefTypes.h>
#include <StrategyAdaptor/OrderBooksShM.hpp>
#include <StrategyAdaptor/MSEnv.h>
#include <Persistence/Configs_Conns-ODB.hxx>
#include <boost/algorithm/string.hpp>
#include <boost/process.hpp>
#include <odb/mysql/database.hxx>
#include <odb/transaction.hxx>
#include <odb/query.hxx>
#include <odb/result.hxx>
#include <Infrastructure/Logger.h>
#include <sys/wait.h>
#include <cstring>
#include <strings.h>
#include <sstream>
#include <cstdlib>
#include <cstdio>
#include <utility>
#include <algorithm>
#include <sys/time.h>
#include <fstream>

namespace
{
  using namespace MAQUETTE;
  using namespace Arbalete;
  using namespace std;
  using namespace boost::process;
  using namespace boost::process::initializers;
  using namespace boost::iostreams;

  //=========================================================================//
  // "FindOMC":                                                              //
  //=========================================================================//
  inline int FindOMC
  (
    char const*                      omc_name,
    vector<MSEnv::ConnDescr> const&  our_omcs
  )
  {
    assert(omc_name != NULL);
    int omcN = -1;

    for (int i = 0; i < int(our_omcs.size()); ++i)
      if (strcmp(omc_name, our_omcs[i].m_connName.c_str()) == 0)
      {
        omcN = i;
        break;
      }

    if (omcN < 0)
      throw runtime_error
            ("FindOMC: OMC="+ string(omc_name) +" not found locally");
    return omcN;
  }

  //=========================================================================//
  // "MapStratDataSegm":                                                     //
  //=========================================================================//
  // NB:
  // (*) "omc_name" is compulsory; "strat_name" and/or "symbol" may  be NULL;
  // (*) in the latter case  (or, in general, when the Strategy Data Segment
  //     cannot be opened and/or the corresp QSym cannot be found in it), the
  //     function returns "false";
  // (*) in the latter case, "data_segm" and "strat_qsym" are empty, so the
  //     "AOSIntruder" cannot be used (but that's OK -- we can do without it);
  // (*) "our_omcN" is always returned (unless there is an exception), so we
  //     know where to sent the request to:
  //
  inline bool MapStratDataSegm
  (
    EnvType                                        env_type,
    char const*                                    strat_name, // May be NULL
    char const*                                    omc_name,
    char const*                                    symbol,     // May be NULL
    vector<MSEnv::ConnDescr> const&                our_omcs,
    int*                                           our_omcN,   // Out (always)
    unique_ptr<BIPC::fixed_managed_shared_memory>* data_segm,  // Out (iff true)
    MSEnv::QSym*                                   strat_qsym, // Out (iff true)
    char const*                                    where
  )
  {
    assert(omc_name   != NULL && our_omcN != NULL && data_segm  != NULL &&
           strat_qsym != NULL && where    != NULL);

    // Initialise the Outputs:
    *our_omcN   = -1;
    data_segm->reset(NULL);
    *strat_qsym = MSEnv::QSym(); // Empty...

    //-----------------------------------------------------------------------//
    // Do local look-up for "omc_name":                                      //
    //-----------------------------------------------------------------------//
    *our_omcN = FindOMC(omc_name, our_omcs);
    assert(*our_omcN >= 0);      // Found, or an exception was thrown

    // The rest is only for non-NULL "strat_name":
    if (strat_name == NULL)
      return false;

    //-----------------------------------------------------------------------//
    // Open the Strategy's Data Segment:                                     //
    //-----------------------------------------------------------------------//
    string segmName = GetStrategyDataSegmName(string(strat_name), env_type);
    try
    {
      void* segm_addr = GetStrategyDataSegmMapAddr(segmName, env_type);

      data_segm->reset
        (new BIPC::fixed_managed_shared_memory
          (
            BIPC::open_only,
            segmName.c_str(),
            segm_addr // Same as string(strat_name)
        ));
    }
    catch (exception const& exc)
    {
      SLOG(WARNING) << "MSEnv::" << where << ": Cannot open StratDataSegm="
                   << segmName  << ": "  << exc.what()  << endl;
      return false;
    }

    //-----------------------------------------------------------------------//
    // Get the Strategy's OMC# from "omc_name":                              //
    //-----------------------------------------------------------------------//
    pair<MSEnv::StrVec*, size_t> omcNames =
      (*data_segm)->find<MSEnv::StrVec>("OMCNames");

    if (omcNames.first == NULL || omcNames.second != 1)
    {
      SLOG(WARNING) << "MSEnv::" << where << ": Cannot get OMCNames from "
                   << segmName  << endl;
      return false;
    }

    int strat_omcN = -1;

    for (int i = 0; i < int(omcNames.first->size()); ++i)
      if (strcmp(omc_name, (*omcNames.first)[i].c_str()) == 0)
      {
        strat_omcN = i;
        break;
      }
    if (strat_omcN == -1)
    {
      SLOG(WARNING) << "MSEnv::" << where << ": OMC=" << omc_name
                   << " not found in StrategySegm="  << segmName << endl;
      return false;
    }

    //-----------------------------------------------------------------------//
    // Determine the "QSym" from the Strategy's point of view:               //
    //-----------------------------------------------------------------------//
    // This is obviously for non-NULL "symbol" only:
    if (symbol == NULL)
      return false;

    pair<Mutex*, size_t>   mtx = (*data_segm)->find<Mutex>("QSymsMutex");

    if (mtx.first == NULL || mtx.second != 1)
    {
      SLOG(WARNING) << "MSEnv::" << where << ": Cannot get QSymsMutex from "
                   << segmName  << endl;
      return false;
    }
    pair<MSEnv::QSymVec*, size_t> qsv =
      (*data_segm)->find<MSEnv::QSymVec>("QSymsVec");

    if (qsv.first == NULL || qsv.second != 1)
    {
      SLOG(WARNING) << "MSEnv::" << where << ": Cannot get QSymsVec from "
                   << segmName  << endl;
      return false;
    }

    // Find the QSym by "symbol" and "strat_omcN":
    Events::SymKey key = Events::MkFixedStr<Events::SymKeySz>(symbol);
    {
      LockGuard lock(*mtx.first);

      for (MSEnv::QSymVec::const_iterator cit = qsv.first->begin();
           cit != qsv.first->end();  ++cit)
        if (cit->m_symKey == key && cit->m_omcN == strat_omcN)
        {
          *strat_qsym = *cit;
          break;
        }
    }
    // Not found?
    if (*strat_qsym == MSEnv::QSym())
    {
      SLOG(WARNING) << "MSEnv::"  << where      << ": QSym=(Symbol=" << symbol
                   << ", OMC#="  << strat_omcN << "): not found in Strategy="
                   << strat_name << endl;
      return false;
    }
    return true;
  }

  //=========================================================================//
  // "AOSIntruder":                                                          //
  //=========================================================================//
  // Intrusion into the "AOSMap" of another Strategy:
  //
  void AOSIntruder
  (
    BIPC::fixed_managed_shared_memory&    data_segm,
    char const*                           strat_name,
    function<void(MSEnv::AOSMap&)> const& action
  )
  {
    try
    {
      // Get the AOSMap Mutex, and lock it:
      pair<Mutex*, size_t> mtx   = data_segm.find<Mutex>("AOSMutex");
      if (mtx.first == NULL  || mtx.second  != 1)
      {
        string segmName = GetStrategyDataSegmName(string(strat_name), env_type);
        throw runtime_error("AOSIntruder: Cannot get AOSMutex from "+ segmName);
      }

      LockGuard lock(*mtx.first);

      // Get the AOSMap:
      pair<MSEnv::AOSMap*, size_t> aoss =
        data_segm.find<MSEnv::AOSMap>("AOSMap");

      if (aoss.first == NULL || aoss.second != 1)
      {
        string segmName = GetStrategyDataSegmName(string(strat_name), env_type);
        throw runtime_error("AOSIntruder: Cannot get AOSMap from "  + segmName);
      }

      // Invoke the User Action:
      action(*(aoss.first));
    }
    catch (exception const& exc)
    {
      SLOG(WARNING) << "AOSIntruder: " << exc.what() << endl;
      // XXX: At the moment, swallow the exception -- do not re-throw it...
    }
  }

  //=========================================================================//
  // "GetWorst{Bid,Ask}":                                                    //
  //=========================================================================//
/*
  double GetWorstBid(OrderBookSnapShot const& ob, long qty)
  {
    assert(qty > 0);

    // Go through the Bids, trying to accommodate the required "qty":
    for (int i = 0; i < ob.m_currBids; ++i)
    {
      long lQty = ob.m_bids[i].m_qty;
      if (qty <= lQty)
        // Qty can be accommodated at this level: return Price:
        return ob.m_bids[i].m_price;

      // Otherwise: decrease Qty by what is available at this level, and go
      // further into the depth:
      qty -= lQty;
    }
    // If we got here: Could not accommodate the whole Qty:
    assert(qty > 0);
    throw runtime_error
          ("GetWorstBid: Could not accommodate full Qty="   +
           to_string(qty) + ": Remaining=" + to_string(qty));
  }


  double GetWorstAsk(OrderBookSnapShot const& ob, long qty)
  {
    assert(qty > 0);

    // Go through the Asks, trying to accommodate the required "qty":
    for (int i = 0; i < ob.m_currAsks; ++i)
    {
      long lQty = ob.m_asks[i].m_qty;
      if (qty <= lQty)
        // Qty can be accommodated at this level: return Price:
        return ob.m_asks[i].m_price;

      // Otherwise: decrease Qty by what is available at this level, and go
      // further into the depth:
      qty -= lQty;
    }
    // If we got here: Could not accommodate the whole Qty:
    assert(qty > 0);
    throw runtime_error
          ("GetWorstAsk: Could not accommodate full Qty="   +
           to_string(qty) + ": Remaining=" + to_string(qty));
  }
*/
}

namespace MAQUETTE
{
  using namespace Arbalete;
  using namespace std;

  //=========================================================================//
  // "MSEnv::ConnDescr": Dtor:                                               //
  //=========================================================================//
  MSEnv::ConnDescr::~ConnDescr()
  {
    if (m_zConn != NULL)
      delete m_zConn;
    m_zConn = NULL;
  }

  //=========================================================================//
  // "MSEnv": Non-Default Ctor:                                              //
  //=========================================================================//
  MSEnv::MSEnv
  (
    vector<string> const& mdcs,
    vector<string> const& omcs,
    bool                  debug
  )
  : m_MDCs      (),
    m_OMCs      (),
    m_debug     (debug),
    m_RNG       (),
    m_Distr     (),
    m_DB        (),
    m_AccsByConn()
  {
    //-----------------------------------------------------------------------//
    // RNG: Initialise with microseconds:                                    //
    //-----------------------------------------------------------------------//
    timeval tv;
    gettimeofday(&tv,  NULL);
    m_RNG.seed  (tv.tv_usec);

    //-----------------------------------------------------------------------//
    // Make a DB connection with "Global" privileges:                        //
    //-----------------------------------------------------------------------//
    try
    {
      // Currently using "Config_MySQL_Global", as MS by definition has access
      // rights to each and every system component. Accessing the "Strategies"
      // DB:
      m_DB.reset(new odb::mysql::database
                     (Config_MySQL_Global.m_user,
                      Config_MySQL_Global.m_passwd, "Strategies"));
    }
    catch (exception const& exc)
    {
      throw runtime_error
            (string("MSEnv::Ctor: Cannot make DB connection to Strategies: ") +
             exc.what());
    }

    //-----------------------------------------------------------------------//
    // NB: No Down-Link ZeroMQ Sockets to be bound...
    // NB: No ShMem Allocators...
    // NB: No Data Structs or Mutexes in Shared Memory...
    // NB: No C3 Thread...
    //-----------------------------------------------------------------------//
    //=======================================================================//
    // MDCs:                                                                 //
    //=======================================================================//
    // Create maps of ShM segments of all MDCs, so that we can access their da-
    // ta any time:
    for (int i = 0; i < int(mdcs.size()); ++i)
    {
      for (int j = 0; j < int(m_MDCs.size()); ++j)
        if (m_MDCs[j].m_connName == mdcs[i])
          throw invalid_argument("MSEnv::Ctor: Repeated MDC Name: " + mdcs[i]);

      // Create the Descriptor and save it on the OMCs list; no ZeroMQ:
      m_MDCs.push_back(ConnDescr(mdcs[i]));

      // NB: We also need to map the ShM data Segment of that MDC into our addr
      // space, but will do it later to prevent a race condition -- at the MS
      // start-up time, the corresp Connector may not be running yet!
    }

    //=======================================================================//
    // OMCs:                                                                 //
    //=======================================================================//
    for (int i = 0; i < int(omcs.size()); ++i)
    {
      //---------------------------------------------------------------------//
      // Construct a "ConnDescr" for this OMC:                               //
      //---------------------------------------------------------------------//
      // Check that there are no duplicate names -- unlike "StratEnv", here
      // they are NOT allowed:
      for (int j = 0; j < int(m_OMCs.size()); ++j)
        if (m_OMCs[j].m_connName == omcs[i])
          throw invalid_argument("MSEnv::Ctor Repeated OMC Name: "+ omcs[i]);

      // Create the Descriptor and save it on the OMCs list; no ZeroMQ:
      m_OMCs.push_back(ConnDescr(omcs[i]));

      // Don't do any Subscriptions or ConnectorStatus -- MS checks the viabi-
      // lity of the Connector in a different way...

      //---------------------------------------------------------------------//
      // Extract all Accounts for this OMC:                                  //
      //---------------------------------------------------------------------//
      // (XXX: Currently, the DB Config is compiled-in, and is for MySQL only;
      // and only FIX Connectors are supported):
      //
      if (omcs[i].size() >= Events::ObjNameSz)
        throw runtime_error("MSEnv::Ctor: OMC Name Too Long: "+ omcs[i]);

      Events::ObjName key = Events::MkFixedStr<Events::ObjNameSz>(omcs[i]);

      if (omcs[i].substr(0, 9) != "Conn_FIX_")
        continue;   // XXX: No Accounts can be extracted (yet) for this Conn...

      // Create a DB Object accessing the Connector's DB:
      try
      {
        // XXX: Have to make extra DB connections (temporary) -- now to the
        // OMC's DBs:
        unique_ptr<odb::database> db
          (new odb::mysql::database
            (Config_MySQL_Global.m_user,
             Config_MySQL_Global.m_passwd, omcs[i]));

        // Extract the Accounts:
        //
        ConnAccInfosMap accMap;
        GetConnAccInfos(*db, omcs[i], &accMap);

        vector<AccShort> accVec;

        for (ConnAccInfosMap::const_iterator cit = accMap.begin();
             cit != accMap.end();  ++cit)
        {
          ConnAccInfo const& fai = cit->second;
          accVec.push_back({ fai.m_Account, fai.m_AccCrypt });
        }
        m_AccsByConn[key] = accVec; // XXX: Involves copying -- OK...
      }
      catch (exception const& exc)
      {
        // Log the error, but continue:
        SLOG(ERROR) << "MSEnv::Ctor: Cannot get AccountsByConn for " << omcs[i]
                   << ": " << exc.what() << endl;
      }
    }
  }

  //=========================================================================//
  // "SendIndication":                                                       //
  //=========================================================================//
  // XXX: "qsym" is used for DB recording only; do not rely on it, it may be
  // empty!
  void MSEnv::SendIndication
  (
    Events::Indication const& ind,
    int                       omcN,
    char const*               strat_name,
    QSym const&               qsym
  )
  {
    if (omcN < 0 || omcN >= int(m_OMCs.size()))
      throw invalid_argument
            ("MSEnv::SendIndication: Invalid OMC#: "+ to_string(omcN));

    //-----------------------------------------------------------------------//
    // Make an "OrdReq":                                                     //
    //-----------------------------------------------------------------------//
    XConnector_OrderMgmt::OrdReq req;
    req.m_isAdmin = false;
    req.m_U.m_ind = ind;

    // Get the "ReqQ" of that OMC.
    // XXX: In "MSEnv" (unlike "StratEnv"), "ReqQ" may not exist yet-- this is
    // to prevent race conditions on MS / Connectors start-up,  so  would have
    // to open it on-the-fly:
    //
    fixed_message_queue* reqQ = m_OMCs[omcN].m_reqQ.get();
    if (reqQ == NULL)
    {
      string const& rqName = GetOrderMgmtReqQName(m_OMCs[omcN].m_connName);
      try
      {
        m_OMCs[omcN].m_reqQ.reset
          (new fixed_message_queue
            (
              BIPC::open_only,
              rqName.c_str(),
              GetOrderMgmtReqQMapAddr(m_OMCs[omcN].m_connName, env_type)
          ));
        reqQ = m_OMCs[omcN].m_reqQ.get();
      }
      catch (exception const& exc)
      {
        SLOG(ERROR) << "MSEnv::SendIndication: Cannot open the OMC ReqQ="
                   << rqName << ": " << exc.what() << endl;
        throw;
      }
    }
    assert(reqQ != NULL);

    // Submit the "OrdReq". Any exceptions are propagated to the Caller (MS);
    // priorities are currently not used (0):
    //
    if (!reqQ->try_send(&req, sizeof(req), 0))
      // Queue full?
      throw runtime_error
            ("MSEnv::SendIndication: OMC ReqQ full: OMC#="+ to_string(omcN));

    //-----------------------------------------------------------------------//
    // Record the Indication sent, in the DB:                                //
    //-----------------------------------------------------------------------//
    // XXX: Inefficiency: creating tmp "string"s -- but OK for manual mode:
    DB_Strat::Indication sind
    (
      ind,
      string(strat_name),
      qsym.m_mdcN,
      Events::MkFixedStr<ObjNameSz,15>("(not recorded)"),
      omcN,
      Events::MkFixedStr<ObjNameSz>(m_OMCs[omcN].m_connName)
    );

    assert(m_DB.get() != NULL);
    try
    {
      odb::transaction trans(m_DB->begin());
      m_DB->persist<DB_Strat::Indication>(sind);
      trans.commit();
    }
    catch (exception const& exc)
    {
      SLOG(ERROR) << "MSEnv::SendIndication: DB Recording Error: "
                 << exc.what() << ": Will re-connect" << endl;

      // XXX: Do manual re-connection:
      m_DB.reset();
      assert(m_DB.get() == NULL);
      m_DB.reset(new odb::mysql::database
                     (Config_MySQL_Global.m_user,
                      Config_MySQL_Global.m_passwd, "Strategies"));
    }
  }

  //=========================================================================//
  // "NewOrderOnBehalf":                                                     //
  //=========================================================================//
  Events::OrderID MSEnv::NewOrderOnBehalf
  (
    char const*             strat_name,
    char const*             omc_name,
    char const*             symbol,
    bool                    is_buy,
    double                  price,
    long                    qty,
    Events::AccCrypt        acc_crypt,
    Events::UserData const* user_data,
    FIX::TimeInForceT       time_in_force,
    Date                    expire_date,
    double                  peg_diff,
    bool                    peg_to_other_side,
    long                    show_qty,
    long                    min_qty
  )
  {
    //-----------------------------------------------------------------------//
    // Segment Management:                                                   //
    //-----------------------------------------------------------------------//
    // Check the args:
    assert(strat_name != NULL && omc_name != NULL && symbol != NULL);

    if (price < 0 || qty <= 0)
      throw invalid_argument("MSEnv::NewOrderOnBehalf: Invalid Price / Qty");

    // Open the ShM data segment of that Sratregy and get our own OMC# corres-
    // ponding to the given OMC Connector Name:
    unique_ptr<BIPC::fixed_managed_shared_memory> dataSegm;
    QSym  qsym;
    int   omcN = -1;

    bool mapped =
      MapStratDataSegm
      (strat_name, omc_name, symbol, m_OMCs, &omcN, &dataSegm, &qsym,
       "NewOrderOnBehalf");

    //-----------------------------------------------------------------------//
    // Create a new Indication:                                              //
    //-----------------------------------------------------------------------//
    Events::Indication ind;

    ind.m_OrdID       = MkReqID(m_Distr(m_RNG));   // Random OrderID
    ind.m_Symbol      = symbol;
    if (is_buy)
      ind.SetBuyAF ();
    else
      ind.SetSellAF();
    ind.m_Price       = price;
    ind.m_Qty         = qty;
    ind.m_ShowQty     = show_qty;
    ind.m_MinQty      = min_qty;
    ind.m_AccCrypt    = acc_crypt;
    ind.m_ClientH     = MkHash48(strat_name);
    ind.m_TS          = CurrUTC();
    ind.m_IsManual    = true;
    ind.m_TimeInForce = time_in_force;
    ind.m_Expires     = DateTime(expire_date, Time());

    if (user_data != NULL)
      ind.m_UserData  = *user_data;

    if (Finite(peg_diff))
    {
      // This is a pegged order:
      ind.m_PegDiff   = peg_diff;
      if (peg_to_other_side)
        ind.SetPeggedOtherSideAF();
      else
        ind.SetPeggedThisSideAF ();
    }

    //-----------------------------------------------------------------------//
    // Inject the new "AOS" into the Strategy:                               //
    //-----------------------------------------------------------------------//
    if (mapped)
    {
      // The created AOS is marked as "manual":
      Events::AOS aos
        (ind.m_OrdID,    ind.m_AccCrypt,  ind.m_ClientH,     ind.m_Symbol,
         is_buy,         ind.m_Price,     ind.m_Qty,         ind.m_ShowQty,
         ind.m_MinQty,   ind.OrderType(), ind.m_TimeInForce, ind.m_TS,
         ind.m_IsManual, ind.m_UserData);
      assert(aos.m_orderType != FIX::OrderTypeT::Undefined);

      // NB: "AOSIntruder" logs, but otherwise tolerates, any exceptions:
      AOSIntruder
      (
        *dataSegm,
        strat_name,

        function<void(AOSMap&)>
        (
          [&qsym, &ind, &aos, strat_name](AOSMap& aoss) -> void
          {
            AOSMap::iterator it = aoss.find(qsym);

            if (it == aoss.end())
              throw runtime_error
                    ("MSEnv::NewOrderOnBehalf: No AOSes for " +
                     Events::ToString(qsym.m_symKey) + " in " +
                     string(strat_name) + ": Not subscribed?");

            // We are already under a lock, so do "insert":
            (void)(it->second.insert(make_pair(ind.m_OrdID, move(aos))));
          }
        )
      );
    }
    //-----------------------------------------------------------------------//
    // Set the Indication up to "omcN":                                      //
    //-----------------------------------------------------------------------//
    SendIndication(ind, omcN, strat_name, qsym);

    // "dataSegm" will be finalised automatically:
    return ind.m_OrdID;
  }

  //=========================================================================//
  // "CancelOrderOnBehalf":                                                  //
  //=========================================================================//
  void MSEnv::CancelOrderOnBehalf
  (
    char const*             strat_name,
    char const*             omc_name,
    char const*             symbol,
    Events::OrderID         order_id,
    Events::UserData const* user_data
  )
  {
    //-----------------------------------------------------------------------//
    // Segment Management:                                                   //
    //-----------------------------------------------------------------------//
    // Check the args:
    assert(strat_name != NULL && omc_name != NULL && symbol != NULL);

    unique_ptr<BIPC::fixed_managed_shared_memory> dataSegm;
    QSym   qsym;
    int    omcN = -1;

    bool mapped =
      MapStratDataSegm
      (strat_name, omc_name, symbol, m_OMCs, &omcN, &dataSegm, &qsym,
       "CancelOrderOnBehalf");

    //-----------------------------------------------------------------------//
    // Create an Indication:                                                 //
    //-----------------------------------------------------------------------//
    Events::Indication ind;

    ind.m_OrdID    = order_id;
    ind.m_Symbol   = symbol;
    ind.m_ClientH  = MkHash48(strat_name);
    ind.m_TS       = CurrUTC();
    ind.m_IsManual = true;
    if (user_data != NULL)
      ind.m_UserData = *user_data;

    //-----------------------------------------------------------------------//
    // Update (INTRUSIVELY) the Strategy's AOS with the Indication created:  //
    //-----------------------------------------------------------------------//
    if (mapped)
      AOSIntruder
      (
        *dataSegm,
        strat_name,

        function<void(AOSMap&)>
        (
          [&qsym, &ind, order_id, strat_name](AOSMap& aoss) -> void
          {
            // Outer Map:
            AOSMap::iterator it = aoss.find(qsym);
            if (it == aoss.end())
              throw runtime_error
                    ("MSEnv::CancelOrderOnBehalf: No AOSes for "+
                     Events::ToString(qsym.m_symKey) +" in "+ string(strat_name)
                     + ": Not subscribed?");

            // Inner Map:
            AOSMapI& aosI = it->second;
            AOSMapI::iterator itI = aosI.find(order_id);
            if (itI == aosI.end())
              throw runtime_error
                    ("MSEnv::CancelOrderOnBehalf: OrderID=" +
                     ToHexStr16(order_id) + " not found in AOSMap");

            // Otherwise: Do update and mark the AOS as "manual":
            Events::AOS& aos    = itI->second;

            aos.m_intendPx      = NaN<double>;
            aos.m_intendQty     = 0;
            aos.m_intendShowQty = 0;
            aos.m_intendMinQty  = 0;
            aos.m_intendTS      = ind.m_TS;
            aos.m_intendManual  = ind.m_IsManual;
            aos.m_intendID      = 0;    // Not yet known, assigned by the Conn

            aos.m_ultimPx       = NaN<double>;
            aos.m_ultimQty      = 0;
            aos.m_ultimShowQty  = 0;
            aos.m_ultimMinQty   = 0;
            aos.m_ultimTS       = ind.m_TS;
            aos.m_ultimManual   = ind.m_IsManual;
            aos.m_ultimID       = 0;    // Not yet known, assigned by the Conn

            aos.m_userData      = ind.m_UserData;
          }
        )
      );
    //-----------------------------------------------------------------------//
    // Send the Indication up to "omcN":                                     //
    //-----------------------------------------------------------------------//
    SendIndication(ind, omcN, strat_name, qsym);
  }

  //=========================================================================//
  // "ModifyOrderOnBehalf":                                                  //
  //=========================================================================//
  void MSEnv::ModifyOrderOnBehalf
  (
    char const*             strat_name,
    char const*             omc_name,
    char const*             symbol,
    Events::OrderID         order_id,
    double                  new_price,    // NaN  if not to change
    long                    new_qty,      // = 0  if not to change
    Events::UserData const* user_data,
    FIX::TimeInForceT       time_in_force,
    Date                    expire_date,
    long                    new_show_qty,
    long                    new_min_qty
  )
  {
    //-----------------------------------------------------------------------//
    // Segment Management:                                                   //
    //-----------------------------------------------------------------------//
    // Check the args:
    assert(strat_name != NULL && omc_name != NULL && symbol != NULL);

    unique_ptr<BIPC::fixed_managed_shared_memory> dataSegm;
    QSym   qsym;
    int    omcN = -1;

    bool mapped =
      MapStratDataSegm
      (strat_name, omc_name, symbol, m_OMCs, &omcN, &dataSegm, &qsym,
       "ModifyOrderOnBehalf");

    //-----------------------------------------------------------------------//
    // Create an Indication:                                                 //
    //-----------------------------------------------------------------------//
    Events::Indication ind;

    ind.m_OrdID       = order_id;
    ind.m_Symbol      = symbol;
    ind.m_Price       = new_price;
    ind.m_Qty         = new_qty;
    ind.m_ShowQty     = new_show_qty;
    ind.m_MinQty      = new_min_qty;
    ind.m_ClientH     = MkHash48(strat_name);
    ind.m_TS          = CurrUTC();
    ind.m_IsManual    = true;
    ind.m_TimeInForce = time_in_force;
    ind.m_Expires     = DateTime(expire_date, Time());
    if (user_data != NULL)
      ind.m_UserData  = *user_data;

    // The Modification (Amendment) flag -- without it, this Indication could
    // be indistinguishable from a New Limit Order:
    ind.SetOrderAmendReqAF();

    //-----------------------------------------------------------------------//
    // Update (INTRUSIVELY) the Strategy's AOS with the Indication created:  //
    //-----------------------------------------------------------------------//
    if (mapped)
      AOSIntruder
      (
        *dataSegm,
        strat_name,

        function<void(AOSMap&)>
        (
          [&qsym, &ind, order_id, new_price, new_qty, new_show_qty,
           new_min_qty, strat_name]
          (AOSMap& aoss) -> void
          {
            // Outer Map:
            AOSMap::iterator it = aoss.find(qsym);
            if (it == aoss.end())
              throw runtime_error
                    ("MSEnv::ModifyOrderOnBehalf: No AOSes for "+
                     Events::ToString(qsym.m_symKey) +" in "+ string(strat_name)
                     + ": Not subscribed?");

            // Inner Map:
            AOSMapI& aosI = it->second;
            AOSMapI::iterator itI = aosI.find(order_id);
            if (itI == aosI.end())
              throw runtime_error("MSEnv::ModifyOrderOnBehalf: OrderID=" +
                                  to_string(order_id) +" NOT FOUND in AOSMap");

            // Otherwise: Do update and mark the AOS as "manual":
            Events::AOS& aos   = itI->second;

            if (Finite(new_price))
            {
              aos.m_intendPx      = new_price;
              aos.m_ultimPx       = new_price;
            }
            if (new_qty > 0)
            {
              aos.m_intendQty     = new_qty;
              aos.m_ultimQty      = new_qty;
            }
            if (new_show_qty > 0)
            {
              aos.m_intendShowQty = new_show_qty;
              aos.m_ultimShowQty  = new_show_qty;
            }
            if (new_min_qty  > 0)
            {
              aos.m_intendMinQty  = new_min_qty;
              aos.m_ultimMinQty   = new_min_qty;
            }
            aos.m_intendTS      = ind.m_TS;
            aos.m_intendManual  = ind.m_IsManual;
            aos.m_intendID      = 0;    // Not yet known, assigned by the Conn

            aos.m_ultimTS       = ind.m_TS;
            aos.m_ultimManual   = ind.m_IsManual;
            aos.m_ultimID       = 0;    // Not yet known, assigned by the Conn

            aos.m_userData      = ind.m_UserData;
          }
        )
      );
    //-----------------------------------------------------------------------//
    // Send the Indication up to "omcN":                                     //
    //-----------------------------------------------------------------------//
    SendIndication(ind, omcN, strat_name, qsym);
  }

  //=========================================================================//
  // "CancelAllOrdersOnBehalf", for a given OMC and maybe Symbol:            //
  //=========================================================================//
  void MSEnv::CancelAllOrdersOnBehalf
  (
    char const*             strat_name,
    char const*             omc_name,
    char const*             symbol,  // If NULL or "", all Symbols cancelled
    Events::AccCrypt        acc_crypt,
    MassCancelSideT         sides,
    Events::UserData const* user_data
  )
  {
    //-----------------------------------------------------------------------//
    // Segment Management:                                                   //
    //-----------------------------------------------------------------------//
    // Check the args:
    assert(strat_name != NULL && omc_name != NULL);

    unique_ptr<BIPC::fixed_managed_shared_memory> dataSegm;
    QSym  qsym;
    int   omcN  = -1;

    bool mapped =
      MapStratDataSegm
      (strat_name, omc_name, symbol, m_OMCs, &omcN, &dataSegm, &qsym,
      "CancelAllOrdersOnBehalf(OMC/Symbol)");

    //-----------------------------------------------------------------------//
    // Create an Indication:                                                 //
    //-----------------------------------------------------------------------//
    Events::Indication ind;

    // NB: MassCancel requires the following flds to be 0 -- they are cleared
    // automatically by the "Indication" Ctor:
    assert(ind.m_OrdID == 0 && ind.m_Price == 0.0 && ind.m_Qty == 0);

    switch (sides)
    {
    case MassCancelSideT::SellT:
      ind.SetSellAF        ();
      ind.SetThisSideOnlyAF();
      break;
    case MassCancelSideT::BuyT:
      ind.SetBuyAF         ();
      ind.SetThisSideOnlyAF();
      break;
    case MassCancelSideT::BothT:
      ind.SetBothSidesAF   ();
      break;
    default:
      assert(false);
    }
    if (symbol != NULL && strlen(symbol) != 0)
      ind.m_Symbol = symbol;

    ind.m_AccCrypt = acc_crypt;
    ind.m_ClientH  = MkHash48(strat_name);
    ind.m_TS       = CurrUTC();
    ind.m_IsManual = true;
    if (user_data != NULL)
      ind.m_UserData = *user_data;

    //-----------------------------------------------------------------------//
    // INTRUSIVELY updated AOSes of all affected Orders:                     //
    //-----------------------------------------------------------------------//
    // XXX: This may be a bad idea -- do we really need to enumerate all poten-
    // tial targets of a "MassCancel"?
    if (mapped)
    {
      AOSIntruder
      (
        *dataSegm,
        strat_name,

        function<void(AOSMap&)>
        (
          [&qsym, &ind, sides](AOSMap& aoss) -> void
          {
            // Outer Map:
            AOSMap::iterator it = aoss.find(qsym);
            if (it != aoss.end())
            {
              AOSMapI& aosI = it->second;

              for (AOSMapI::iterator itI = aosI.begin();
                   itI != aosI.end(); ++itI)
              {
                Events::AOS& aos = itI->second;

                if (sides == MassCancelSideT::BothT
                   ||
                   (sides == MassCancelSideT::BuyT  &&  aos.m_isBuy)
                   ||
                   (sides == MassCancelSideT::SellT && !aos.m_isBuy))
                {
                  // Indicate the Cancellation request ("manual" mode):
                  aos.m_intendPx      = NaN<double>;
                  aos.m_intendQty     = 0;
                  aos.m_intendShowQty = 0;
                  aos.m_intendMinQty  = 0;
                  aos.m_intendTS      = ind.m_TS;
                  aos.m_intendManual  = ind.m_IsManual;
                  aos.m_intendID      = 0;  // Not yet known, assigned by Conn

                  aos.m_ultimPx       = NaN<double>;
                  aos.m_ultimQty      = 0;
                  aos.m_ultimShowQty  = 0;
                  aos.m_ultimMinQty   = 0;
                  aos.m_ultimTS       = ind.m_TS;
                  aos.m_ultimManual   = ind.m_IsManual;
                  aos.m_ultimID       = 0;  // Not yet known, assigned by Conn

                  aos.m_userData      = ind.m_UserData;
                }
              }
            }
          }
        )
      );
    }
    //-----------------------------------------------------------------------//
    // Send the Indication up to "omcN":                                     //
    //-----------------------------------------------------------------------//
    SendIndication(ind, omcN, strat_name, qsym);
  }

  //=========================================================================//
  // "CancelAllOrdersOnBehalf", for all OMCs and Symbols of the Strat:       //
  //=========================================================================//
  void MSEnv::CancelAllOrdersOnBehalf
  (
    char const*             strat_name,
    MassCancelSideT         sides,
    Events::UserData const* user_data
  )
  {
    for (int i = 0; i < int(m_OMCs.size()); ++i)
    {
      // NB: Because "symbol" is not specified, we MAY need to specify the acc;
      // so iterate over all Accounts configured for that Connector. If some of
      // the Accounts do not correspond to this "strat_name", never mind - will
      // simply get unsuccessful cancels...
      //
      char const* connName = m_OMCs[i].m_connName.c_str();

      vector<AccShort> accs;
      GetConnAccounts(connName, &accs);

      for (int j = 0; j < int(accs.size()); ++j)
        CancelAllOrdersOnBehalf
          (strat_name, connName, NULL, accs[j].m_accCrypt, sides, user_data);
    }
  }

  //=========================================================================//
  // "GetConnAccounts":                                                      //
  //=========================================================================//
  void MSEnv::GetConnAccounts
  (
    char const*       omc_name,
    vector<AccShort>* res
  )
  const
  {
    assert(omc_name != NULL && res != NULL);
    res->clear();

    // Construct the ByConnector Key:
    Events::ObjName key = Events::MkFixedStr<Events::ObjNameSz>(omc_name);

    // Get the "AccShort"s vector from the Connector:
    AccsByConn::const_iterator cit = m_AccsByConn.find(key);

    if (cit != m_AccsByConn.end())
      *res = cit->second;
  }

  //=========================================================================//
  // "SaveReport":                                                           //
  //=========================================================================//
  void MSEnv::SaveReport
  (
    char const*        generator_path,
    char const*        out_file,
    Arbalete::DateTime start_time,
    Arbalete::DateTime end_time
  )
  {
    static char const* dtime_format = "%Y-%m-%d_%H:%M:%S.%f";

    vector<string> args;
    args.push_back(string(generator_path));
    args.push_back(string(out_file));
    args.push_back(DateTimeToFStr(start_time, dtime_format));
    args.push_back(DateTimeToFStr(end_time,   dtime_format));

    // Invoke the generator and re-direct its stdout to the file specified:
    auto  c = execute(set_args(args),
                     close_stdin(),
                     inherit_env());
    auto code = wait_for_exit(c);
    if (!WIFEXITED(code) || WEXITSTATUS(code))
      throw runtime_error("Report generation error");
  }

# if 0
  // XXX: Currently de-configured: used "AllSecIDs"
  //=========================================================================//
  // "LiquidatePortfolioOnBehalf":                                           //
  //=========================================================================//
  void MSEnv::LiquidatePortfolioOnBehalf
  (
    EnvType                 env_type,
    char const*             strat_name,
    Events::UserData const* user_data
  )
  {
    //-----------------------------------------------------------------------//
    // Get the Strategy's Segment:                                           //
    //-----------------------------------------------------------------------//
    string segmName = GetStrategyDataSegmName(string(strat_name), env_type);
    unique_ptr<BIPC::fixed_managed_shared_memory>   dataSegm;
    try
    {
      dataSegm.reset
          (new BIPC::fixed_managed_shared_memory
            (
              BIPC::open_only,
              segmName.c_str(),
              StrategyDataSegmBaseAddr
          ));
    }
    catch (exception const& exc)
    {
      SLOG(ERROR) << "MSEnv::LiquidatePortfolioOnBehalf: Cannot open "
                    "StratDataSegm=" << segmName << ": " << exc.what()
                 << endl;
      return;
    }

    //-----------------------------------------------------------------------//
    // Get "Positions", "MDCNames" and "OMCNames" of that Strategy:          //
    //-----------------------------------------------------------------------//
    // Get the Positions Map Mutex, and lock it:
    pair<Mutex*, size_t> mtxPos = dataSegm->find<Mutex>("PositionsMutex");

    if (mtxPos.first == NULL || mtxPos.second != 1)
    {
      SLOG(ERROR) << "MSEnv::LiquidatePortfolioOnBehalf: Cannot get Positions"
                    "Mutex from "<< segmName << endl;
      return;
    }

    // Lock the segment against modifications by the Strategy:
    LockGuard lockP(*mtxPos.first);

    // Get the PositionsMap:
    pair<PositionsMap*, size_t> poss =
      dataSegm->find<PositionsMap>("PositionsMap");

    if (poss.first == NULL || poss.second != 1)
    {
      SLOG(ERROR) << "MSEnv::LiquidatePortfolioOnBehalf: Cannot get Positions"
                    "Map from " << segmName << endl;
      return;
    }

    // "MDCNames", "OMCNames": NB: no lock here as they are considered to be
    // constructed-once, then read-only:
    //
    pair<StrVec*, size_t> mdcNames = dataSegm->find<StrVec>("MDCNames");

    if (mdcNames.first == NULL || mdcNames.second != 1)
    {
      SLOG(ERROR) << "MSEnv::LiquidatePortfolioOnBehalf: Cannot get MDCNames "
                    "from " << segmName << endl;
      return;
    }

    pair<StrVec*, size_t> omcNames = dataSegm->find<StrVec>("OMCNames");

    if (omcNames.first == NULL || omcNames.second != 1)
    {
      SLOG(ERROR) << "MSEnv::LiquidatePortfolioOnBehalf: Cannot get OMCNames "
                    "from " << segmName << endl;
      return;
    }

    //-----------------------------------------------------------------------//
    // Iterate over the Positions:                                           //
    //-----------------------------------------------------------------------//
    for (PositionsMap::const_iterator cit = poss.first->begin();
         cit != poss.first->end();   ++cit)
    {
      //---------------------------------------------------------------------//
      // Get "QSym" (as viewed by the Strategy):                             //
      //---------------------------------------------------------------------//
      QSym         const& qsym = cit->first;
      PositionInfo const& pi   = cit->second;
      // assert(qsym == pi.m_qsym);

      long pos = pi.m_currPos;
      long qty = abs(pos);

      if (pos == 0)
        continue;   // Nothing to do here...

      SLOG(INFO) << "MSEnv::LiquidatePortfolioOnBehalf: "
                << Events::ToString(qsym.m_symKey) << ": " << pos << endl;

      //---------------------------------------------------------------------//
      // Access the MDC Segment of QSym for MktData:                         //
      //---------------------------------------------------------------------//
      double px = 0.0;              // Will use a MktOrder

      // Get the MDC Name corresponding to that QSym:
      //
      if (qsym.m_mdcN < 0 || qsym.m_mdcN >= int(mdcNames.first->size()))
        SLOG(WARNING) << "MSEnv::LiquidatePortfolioOnBehalf: Invalid MDC#="
                     << qsym.m_mdcN << " for Symbol="
                     << Events::ToString(qsym.m_symKey) << ", will use MktOrder"
                     << endl;
        // Thus, we cannot get the OrderBook for that QSym, and cannot determine
        // the liquidation price -- will use a MktOrder instead (px = 0), but it
        // may be unsupported by the OMC...
      else
      {
        //-------------------------------------------------------------------//
        // Yes, got the MDC Name: Now get the OrderBook for "symbol":        //
        //-------------------------------------------------------------------//
        ShMString const& mdcName    = (*mdcNames.first)[qsym.m_mdcN];

        // NB: Both "mdcName" and "symKey" below must be properly 0-terminated.
        // The following call may fill in the flds of the corresp MDC's  Conn-
        // Descr:
        OrderBookSnapShot const* ob =
          GetOrderBook(mdcName.data(), qsym.m_symKey.data());

        if (ob == NULL)
          SLOG(WARNING) << "MSEnv::LiquidatePortfolioOnBehalf: Cannot get Order"
                          "Book for " << qsym.m_symKey.data() << ", Will use "
                          "MktOrder"  << endl;
        else
          // NB: Exception here if the corresp Side is empty:
          try
          {
            px =
              (pos < 0)
              ? GetWorstAsk(*ob, qty)   // Buying  at highest Ask
              : GetWorstBid(*ob, qty);  // Selling at lowest  Bid
          }
          catch (exception const& exc)
          {
            SLOG(WARNING) << "MSEnv::LiquidatePortfolioOnBehalf: Error in "
                            "Limit Price computation: " << exc.what()
                         << ", will use MktOrder"       << endl;
            px = 0.0;
          }
      }
      // We now have the "px" (non-0 or otherwise).

      //---------------------------------------------------------------------//
      // Get the name of the Connector by Strategy-assigned OMC#:            //
      //---------------------------------------------------------------------//
      int omcN = qsym.m_omcN;
      if (omcN < 0 || omcN >= int(omcNames.first->size()))
      {
        SLOG(ERROR) << "MSEnv::LiquidatePortfolioOnBehalf: Symbol="
                   << Events::ToString(qsym.m_symKey) << ": Invalid OMC#="
                   << omcN << endl;
        continue;
      }
      char    omcName[64];
      memset( omcName, '\0', 64);
      strncpy(omcName, (*(omcNames.first))[omcN].data(), 64);

      char const*    csym = qsym.m_symKey.data();
      assert(strnlen(csym, Events::SymKeySz) < Events::SymKeySz);

      //---------------------------------------------------------------------//
      // Can now submit the Order:                                           //
      //---------------------------------------------------------------------//
      bool isBuy = (pos < 0);

      Events::OrderID ordID =
        NewOrderOnBehalf
        (strat_name, omcName, csym, isBuy,  px, qty, pi.m_accCrypt, user_data);

      // Output the msg:
      SLOG(INFO) << "MSEnv::LiquidatePortfolioOnBehalf: New OrderID="
                << ToHexStr16(ordID) << (pos < 0 ? ": Buy " : ": Sell ") << qty
                << " of " << Events::ToString(qsym.m_symKey)    << " @ " << px
                << endl;
    }
  }

  //=========================================================================//
  // "GetOrderBook":                                                         //
  //=========================================================================//
  // XXX: We have to do it on an ad-hoc basis: cannot construct ptrs to Order
  // Books in the "MSEnv" Ctor, as the corresp Connectors may not run at that
  // point:
  OrderBookSnapShot const* MSEnv::GetOrderBook
  (
    char const* mdc_name,
    char const* symbol
  )
  {
    //-----------------------------------------------------------------------//
    // Get the ShM segment of that MDC:                                      //
    //-----------------------------------------------------------------------//
    assert(mdc_name != NULL && symbol != NULL);

    // The Connector must be on our list:
    ConnDescr* cd = NULL;
    for (int i = 0; i < int(m_MDCs.size()); ++i)
      if (strcmp(m_MDCs[i].m_connName.c_str(), mdc_name) == 0)
      {
        cd = &(m_MDCs[i]);
        break;
      }
    if (cd == NULL)
    {
      SLOG(ERROR) << "GetOrderBook: UnRecognised MDC=" << mdc_name << endl;
      return NULL;
    }

    if (cd->m_segment.get() == NULL)
    {
      // The ShM segment of that MDC has not been opened yet, so do it now:
      string segmName = GetMktDataSegmName(string(mdc_name), env_type);
      try
      {
        cd->m_segment.reset
          (new BIPC::fixed_managed_shared_memory
            (
              BIPC::open_only,
              segmName.c_str(),
              MktDataSegmBaseAddr
          ));
      }
      catch (exception const& exc)
      {
        SLOG(ERROR) << "GetOrderBook: Cannot open the ShM Segment of MDC Segm="
                   << segmName << ": " << exc.what() << endl;
        return NULL;
      }
      // If we got here, we must have a valid segment:
      assert(cd->m_segment.get() != NULL);

      // But the OrderBooks map is empty yet -- will need to fill it in:
      assert(cd->m_orderBooks.empty());

      //---------------------------------------------------------------------//
      // First, get "AllSecIDs":                                             //
      //---------------------------------------------------------------------//
      pair<Events::SecID*, size_t> allSecIDs =
        cd->m_segment->find<Events::SecID>("AllSecIDs");

      if (allSecIDs.first == NULL || allSecIDs.second == 0)
      {
        SLOG(ERROR) << "GetOrderBook: Could not load AllSecIDs from MDC Segm="
                   << segmName << endl;
        return NULL;
      }

      //---------------------------------------------------------------------//
      // Traverse all SecIDs filling in the OrderBooks map:                  //
      //---------------------------------------------------------------------//
      for (int j = 0; j < int(allSecIDs.second); ++j)
      {
        // We will need the Symbol for this SecID: so obtain the corresp SecDefs
        // first. NB: Alternatively, the Symbol is recorded in the OrderBook it-
        // self, but the OrderBook is more likely to be empty than SecDefs:
        //
        Events::SecID  secID      = allSecIDs.first[j];
        Events::SymKey currSymbol = Events::EmptySymKey();

        string secDefsName = MkBookName<SecDef>(secID);
        // Open that area (XXX: it was allocated as a "long double" array, to
        // get proper 16-byte alignment of a variable-size buffer):
        pair<long double*, size_t> memSecDef =
          cd->m_segment->find<long double>(secDefsName.c_str());

        if (memSecDef.first != NULL && memSecDef.second != 0)
        {
          CircBuffSimple<SecDef> const* cbSecDef =
            (CircBuffSimple<SecDef> const*)(memSecDef.first);
          assert(cbSecDef != NULL);

          if (!(cbSecDef->IsEmpty()))
          {
            //---------------------------------------------------------------//
            // Got the SecDef:                                               //
            //---------------------------------------------------------------//
            SecDef const* def = cbSecDef->GetCurrPtr(); // A la "GetLatest"
            assert(def != NULL);
            currSymbol  = def->m_Symbol;
          }
        }
        // Otherwise, "currSymbol" remains empty for the moment -- will try to
        // get it from the OrderBook (see below)...

        // OrderBook ShM Object name:
        string obName = MkBookName<OrderBookSnapShot>(secID);

        // Open that area (XXX: it was allocated as a "long double" array, to
        // get proper 16-byte alignment of a variable-size buffer):
        pair<long double*, size_t> memOB =
          cd->m_segment->find<long double>(obName.c_str());

        if (memOB.first == NULL || memOB.second == 0)
        {
          SLOG(WARNING) << "GetOrderBook: No " << obName << " ShM object in "
                       << segmName << ", skipping this SecID" << endl;
          continue;
        }

        // Otherwise, we got the CircBuff for the OrderBook for this SecID:
        CircBuffSimple<OrderBookSnapShot> const* cbOB =
          (CircBuffSimple<OrderBookSnapShot> const*)(memOB.first);
        assert(cbOB != NULL);

        // If Symbol is not yet known, try to get it from the OrderBook:
        if (Events::IsEmpty(currSymbol) && !cbOB->IsEmpty())
        {
          OrderBookSnapShot const* ob = cbOB->GetCurrPtr(); // A la "GetLatest:
          assert(ob != NULL);
          currSymbol = ob->m_symbol;
        }

        // If the Symbol is STILL unknown, cannot do anything for this SecID:
        if (Events::IsEmpty(currSymbol))
          continue;

        // Otherwise: Got the Symbol, so install it in the map. XXX: We do not
        // check that "currSymbol" is unique, this is always assumed:
        //
        cd->m_orderBooks[currSymbol] = cbOB;  // Once again, cbOB != NULL
      }
    }
    //-----------------------------------------------------------------------//
    // Get the most recent SnapShot:                                         //
    //-----------------------------------------------------------------------//
    // Look-up the "m_orderBooks" map:
    //
    Events::SymKey symKey = Events::MkFixedStr<Events::SymKeySz>(symbol);

    map<Events::SymKey,
        CircBuffSimple<OrderBookSnapShot> const*>::const_iterator
      cit = cd->m_orderBooks.find(symKey);

    if (cit == cd->m_orderBooks.end() || cit->second->IsEmpty())
      // OrderBook is not found at all, or has no data generations:
      return NULL;

    // Otherwise:
    OrderBookSnapShot const* ob = cit->second->GetCurrPtr(); // A la "GetLatest"
    assert(ob != NULL);
    return ob;
  }
# endif
}
