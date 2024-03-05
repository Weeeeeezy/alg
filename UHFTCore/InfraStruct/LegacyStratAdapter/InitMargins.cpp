// vim:ts=2:et:sw=2
//===========================================================================//
//                            "InitMargins.cpp":                             //
//           Functions to Compute Initial Margin Requirements                //
//                     for Various Exchanges / Brokers                       //
//===========================================================================//
#include "Connectors/Configs_Conns.h"
#include "Connectors/Configs_DB.h"
#include "StrategyAdaptor/StratEnv.h"
#include "Infrastructure/SecDefTypes.h"
#include "Persistence/Configs_Conns-ODB.hxx"
#include "Persistence/SecDefTypes-ODB.hxx"
#include <Infrastructure/Logger.h>
#include <odb/database.hxx>
#include <odb/mysql/database.hxx>
#include <odb/transaction.hxx>
#include <boost/algorithm/string.hpp>
#include <memory>

namespace
{
  using namespace Arbalete;
  using namespace MAQUETTE;
  using namespace std;

  //=========================================================================//
  // QSym -> String (searching for a better place):                          //
  //=========================================================================//
  inline string ToString(StratEnv::QSym const& qsym)
  {
    return "("  + Events::ToString(qsym.m_symKey) +
           ", " + to_string(qsym.m_mdcN) +
           ", " + to_string(qsym.m_omcN) + ")";
  }

  //=========================================================================//
  // "InitMargin_FORTS_Gen":                                                 //
  //=========================================================================//
  // This is a template parameterised by the "SecDefs" map:
  //
  template<typename T>
  double InitMargin_FORTS_Gen
  (
    map<StratEnv::QSym, long>* poss,
    T const&                   defs
  )
  {
    assert(poss != NULL);
    double res = 0.0;

    // Go through the Futures / Options portfolio, creating Synthetic positions
    // where possible:
    for (map<StratEnv::QSym, long>::iterator it = poss->begin();
         it != poss->end();  ++it)
    {
      // Get the SecDef:
      long                       pos  = it->second;
      if (pos == 0)
        // NB: "pos" was non-0 initially, but can become 0 in Futures as a re-
        // sult of Synthetic positions construction:
        continue;

      StratEnv::QSym const&      qsym = it->first;
      typename T::const_iterator cit  = defs.find(qsym);

      if (cit == defs.end())
        throw runtime_error("InitMargin_FORTS: No SecDef for QSym=" +
                            ToString(qsym));

      SecDef const& secDef = *(cit->second);

      // Is it a Short Option Position? Then try to find its underlying Futures
      // in the same portfolio:
      if (secDef.m_CFICode[0] == 'O' && pos < 0)
      {
        Events::SymKey const& underSym  = secDef.m_UnderlyingSymbol;

        bool didSynthetic  = false;

        for (map<StratEnv::QSym, long>::iterator fit = poss->begin();
             fit != poss->end(); ++fit)
          if ((fit->first).m_symKey == underSym)
          {
            // Yes, the Futures has been found! We need to be Long in Futures,
            // and to have suff pos:
            long fpos = fit->second;
            if  (fpos <= 0)
              // No, we cannot really make a synthetic position:
              break;

            // Otherwise: The resulting synthetic qty:
            long   sqty = min<long>(fpos, abs(pos));
            assert(0 < fpos && 0 < sqty && sqty <= fpos);

            // Make a Synthetic position of the Futures and the Option
            res += double(sqty) * secDef.m_InitMarginSynthetic;

            // Then adjust the Futures pos:
            fit->second -= sqty;
            didSynthetic = true;
            break;
          }
        // So:
        if (!didSynthetic)
          // Could not construct a Synthetic pos, so we just hold a short
          // Option pos:
          res += fabs(double(pos)) * secDef.m_InitMarginOnSell;
      }
      else
      // Long Option pos, or a Futures:
      res += fabs(double(pos)) *
             (pos < 0
              ? secDef.m_InitMarginOnSell
              : secDef.m_InitMarginOnBuy);
    }
    return res;
  }
}

namespace MAQUETTE
{
  using namespace Arbalete;
  using namespace std;

  //=========================================================================//
  // "InitMargin_FORTS" (for use with "StratEnv"):                           //
  //=========================================================================//
  double InitMargin_FORTS
  (
    map<StratEnv::QSym,  long>* poss,
    StratEnv::SecDefsMap const& defs
  )
  {
    return InitMargin_FORTS_Gen<StratEnv::SecDefsMap>(poss, defs);
  }

  //=========================================================================//
  // "InitMargin" (For use with Reports Generator):                          //
  //=========================================================================//
  double InitMargin
  (
    vector<pair<string, long>> const& positions,
    Events::AccCrypt                  acc_crypt
  )
  {
    //-----------------------------------------------------------------------//
    // Build the Dictionary of notional "QSym"s:                             //
    //-----------------------------------------------------------------------//
    map<StratEnv::QSym, long> poss;

    for (int i = 0; i < int(positions.size()); ++i)
    {
      StratEnv::QSym        qsym (positions[i].first.c_str(), -1, -1);
      poss.insert(make_pair(qsym, positions[i].second));
    }

    //-----------------------------------------------------------------------//
    // Get the Connector (OMC Name) from this AccCrypt:                      //
    //-----------------------------------------------------------------------//
    string omcName;
    try
    {
      unique_ptr<odb::database> db
        (new odb::mysql::database
          (Config_MySQL_Global.m_user, Config_MySQL_Global.m_passwd,
           "MAQUETTE"));

      odb::transaction trans(db->begin());

      unique_ptr<ConnAccInfo> res
        (db->load<ConnAccInfo>(acc_crypt));

      omcName = res->m_Connector;
      trans.commit();
    }
    catch (exception const& exc)
    {
      LOG_ERROR("InitMargin: DB Exception in MAQUETTE.ConnAccInfo: %s\n",
                exc.what());
      return 0.0;
    }

    //-----------------------------------------------------------------------//
    // Get the "SecDef"s:                                                    //
    //-----------------------------------------------------------------------//
    map<StratEnv::QSym, unique_ptr<SecDef>> defs;
    try
    {
      unique_ptr<odb::database> db
        (new odb::mysql::database
          (Config_MySQL_Global.m_user, Config_MySQL_Global.m_passwd,
          omcName));

      odb::transaction  trans(db->begin());
      odb::result<SecDef> res(db->query<SecDef>());

      // Copy all SecDefs into "defs", allocating them synamically (this is
      // for compatibility with the generic version):
      for (odb::result<SecDef>::iterator it = res.begin(); it != res.end();
           ++it)
      {
        unique_ptr<SecDef> def(new SecDef(*it));
        StratEnv::QSym qsym;
        qsym.m_symKey = def->m_Symbol;

        (void) defs.insert(make_pair(qsym, move(def)));
      }
      trans.commit();
    }
    catch (exception const& exc)
    {
      LOG_ERROR("Initial Margin: DB Exception in %s.SecDef: %s\n",
                 omcName.c_str(), exc.what());
      return 0.0;
    }

    //-----------------------------------------------------------------------//
    // Get the Exchange:                                                     //
    //-----------------------------------------------------------------------//
    vector<string> nameParts;
    boost::split(nameParts, omcName, boost::is_any_of("_"));

    if (nameParts.size() < 3)
    {
      LOG_ERROR("InitMargin: Invalid OMC Name: %s\n", omcName.c_str());
      return 0.0;
    }

    if (nameParts[2] == "FORTS")
      //---------------------------------------------------------------------//
      // Yes, this is FORTS, so proceed:                                     //
      //---------------------------------------------------------------------//
      return InitMargin_FORTS_Gen
             <map<StratEnv::QSym, unique_ptr<SecDef>>>(&poss, defs);
    else
    {
      LOG_ERROR("InitMargin: UnRecognised Exchange: %s\n",
                nameParts[2].c_str());
      return 0.0;
    }
  }
}
