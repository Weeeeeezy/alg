// vim:ts=2:et:sw=2
//===========================================================================//
//                                 "MSEnv.h":                                //
//       Minimalistic Strategy Environment for use in Management Server      //
//===========================================================================//
#ifndef MAQUETTE_STRATEGYADAPTOR_MSENV_H
#define MAQUETTE_STRATEGYADAPTOR_MSENV_H

#include "Common/DateTime.h"
#include "Common/Maths.h"
#include "Connectors/XConnector.h"
#include "Connectors/FIX_Msgs.h"
#include "StrategyAdaptor/StratMSEnvTypes.h"
#include "StrategyAdaptor/CircBuffSimple.hpp"
#include "StrategyAdaptor/OrderBooksShM.h"
#include "StrategyAdaptor/TradeBooksShM.h"
#include "Infrastructure/SecDefTypes.h

#include <odb/database.hxx>
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>
#include <atomic>

class Json;

namespace MAQUETTE
{
  //=========================================================================//
  // "MAQUETTE::MSEnv" Class:                                               //
  //=========================================================================//
  class MSEnv
  {
  public:
    //=======================================================================//
    // User-Visible Data Types:                                              //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // Common "MSEnv" and "StratEnv" types:                                  //
    //-----------------------------------------------------------------------//
    typedef StratMSEnvTypes::QSym           QSym;

    typedef StratMSEnvTypes::AOSMapI        AOSMapI;
    typedef StratMSEnvTypes::AOSMap         AOSMap;

    typedef StratMSEnvTypes::ShMString      ShMString;
    typedef StratMSEnvTypes::StrVec         StrVec;
    typedef StratMSEnvTypes::QSymVec        QSymVec;

    typedef StratMSEnvTypes::PositionInfo   PositionInfo;
    typedef StratMSEnvTypes::PositionsMap   PositionsMap;
    typedef StratMSEnvTypes::OrderBooksMap  OrderBooksMap;

    //=======================================================================//
    // "MAQUETTE::MSEnv::ConnDescr": Connector Descriptor (Internal):       //
    //=======================================================================//
    class ConnDescr
    {
    private:
      //---------------------------------------------------------------------//
      // Hidden Stuff:                                                       //
      //---------------------------------------------------------------------//
      ConnDescr()                                 = delete;
      ConnDescr(ConnDescr const&)                 = delete;
      ConnDescr& operator=(ConnDescr const&)      = delete;

    public:
      //---------------------------------------------------------------------//
      // Data Flds:                                                          //
      //---------------------------------------------------------------------//
      // Connector Name:
      std::string                                        m_connName;
      EnvType                                            m_envType;

      // Connector's ShMem segment: MDCs only. Must keep it here for the whole
      // duration of the MSEnv life cycle, otherwise OrderBook ptrs (see below)
      // would become invalid:
      std::unique_ptr<BIPC::fixed_managed_shared_memory> m_segment;

      std::map<Events::SymKey, CircBuffSimple<OrderBookSnapShot> const*>
                                                         m_orderBooks;
      // Connector's Queue (OMCs only):
      std::unique_ptr<fixed_message_queue>               m_reqQ;

      //---------------------------------------------------------------------//
      // Ctors / Dtor:                                                       //
      //---------------------------------------------------------------------//
      // Non-Default Ctor:
      //
      ConnDescr(EnvType env_type, std::string const& conn_name)
      : m_connName  (conn_name),
        m_envType   (env_type),
        m_zConn     (NULL),     // Will be created on demand
        m_segment   (),
        m_reqQ      ()          // Ditto
      {}

      // Move Ctor:
      //
      ConnDescr(ConnDescr&& right)
      : m_connName  (std::move(right.m_connName)),
        m_envType   (right.m_envType),
        m_zConn     (right.m_zConn),
        m_segment   (std::move(right.m_segment)),
        m_orderBooks(std::move(right.m_orderBooks)),
        m_reqQ      (std::move(right.m_reqQ))
      {
        right.m_zConn = NULL;   // This is a MUST -- not re-set automatically!
        right.m_segment.reset();
        right.m_orderBooks.clear();
        right.m_reqQ.reset();
      }

      // Move Assignment:
      //
      ConnDescr& operator=(ConnDescr&& right)
      {
        m_connName    = std::move(right.m_connName);
        right.m_connName.clear();

        m_envType     = right.m_envType;

        m_zConn       = right.m_zConn;
        right.m_zConn = NULL;

        m_segment     = std::move(right.m_segment);
        right.m_segment.reset();

        m_orderBooks  = std::move(right.m_orderBooks);
        right.m_orderBooks.clear();

        m_reqQ        = std::move(right.m_reqQ);
        right.m_reqQ.reset();
        return *this;
      }

      // Dtor (cannot be inlined -- see the implementation):
      //
      ~ConnDescr();
    };

    //=======================================================================//
    // "AccShort":                                                           //
    //=======================================================================//
    // XXX: Unlike "ConnAccInfo", the following struct is very minimalistic:
    // protocol-independent:
    //
    struct AccShort
    {
      std::string       m_accID;
      Events::AccCrypt  m_accCrypt;
    };

  private:
    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // Connectors:                                                           //
    //-----------------------------------------------------------------------//
    // MktData and OrderMgmt Connectors:
    std::vector<ConnDescr>                                    m_MDCs;
    std::vector<ConnDescr>                                    m_OMCs;
    bool                                                      m_debug;

    // Random Number Generator:
    mutable std::mt19937                                      m_RNG;
    mutable std::uniform_int_distribution<Events::OrderID>    m_Distr;

    // DB For Logging and Queries:
    std::unique_ptr<odb::database>                            m_DB;

    //-----------------------------------------------------------------------//
    // "AccShort"s for all configured OMCs:                                  //
    //-----------------------------------------------------------------------//
    // All Accounts by Connector Name:
    typedef Events::ObjName    ConnName;
    typedef std::map<ConnName, std::vector<AccShort>>         AccsByConn;
    AccsByConn                                                m_AccsByConn;

    //-----------------------------------------------------------------------//
    // Deleted / Hidden Stuff:                                               //
    //-----------------------------------------------------------------------//
    // "MSEnv" objects are completely un-copyable, un-assignable and un-compar-
    // able:
    MSEnv()               = delete;
    MSEnv(MSEnv const&)   = delete;
    MSEnv(MSEnv&&)        = delete;

    MSEnv& operator= (MSEnv const&);
    MSEnv& operator= (MSEnv&&);

    bool   operator==(MSEnv const&) const;
    bool   operator!=(MSEnv const&) const;

  public:
    //=======================================================================//
    // API for use by Strategies:                                            //
    //=======================================================================//
    //=======================================================================//
    // Non-Default Ctor, Dtor:                                               //
    //=======================================================================//
    // The following Ctor is for use by MS which may only submit requests to
    // OMCs on behalf of other Strategies. Primarily, it uses OMCs; MDCs are
    // required only for special tasks like Portfolio Liquidation:
    MSEnv
    (
      std::vector<std::string> const& mdcs,                 // Names of MDCs
      std::vector<std::string> const& omcs,                 // Names of OMCs
      bool                            debug = false
    );

    // Dtor -- currently trivial:
    ~MSEnv(){}

    //=======================================================================//
    // Order Management on Behalf of Real Strategies:                        //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "SendIndication":                                                     //
    //-----------------------------------------------------------------------//
    // Sending up an Indication to the OMC:
  private:
    void SendIndication
    (
      Events::Indication const& ind,
      int                       omcN,
      char const*               strat_name,
      QSym const&               qsym
    );

  public:
    //-----------------------------------------------------------------------//
    // "NewOrderOnBehalf":                                                   //
    //-----------------------------------------------------------------------//
    Events::OrderID NewOrderOnBehalf
    (
      char const*             strat_name,
      char const*             omc_name,
      char const*             symbol,
      bool                    is_buy,
      double                  price,
      long                    qty,
      Events::AccCrypt        acc_crypt,
      Events::UserData const* user_data         = NULL,
      FIX::TimeInForceT       time_in_force     = FIX::TimeInForceT::Day,
      Arbalete::Date          expire_date       = Arbalete::Date(),
      double                  peg_diff          = Arbalete::NaN<double>,
      bool                    peg_to_other_side = true,
      long                    show_qty          = 0,
      long                    min_qty           = 0
    );

    //-----------------------------------------------------------------------//
    // "CancelOrderOnBehalf":                                                //
    //-----------------------------------------------------------------------//
    void CancelOrderOnBehalf
    (
      char const*             strat_name,
      char const*             omc_name,
      char const*             symbol,
      Events::OrderID         order_id,
      Events::UserData const* user_data = NULL
    );

    //-----------------------------------------------------------------------//
    // "ModifyOrderOnBehalf":                                                //
    //-----------------------------------------------------------------------//
    void ModifyOrderOnBehalf
    (
      char const*             strat_name,
      char const*             omc_name,
      char const*             symbol,
      Events::OrderID         order_id,
      double                  new_price,      // = 0  if not to change
      long                    new_qty,        // = 0  if not to change
      Events::UserData const* user_data     = NULL,
      FIX::TimeInForceT       time_in_force = FIX::TimeInForceT::Day,
      Arbalete::Date          expire_date   = Arbalete::Date(),
      long                    new_show_qty  = 0,
      long                    new_min_qty   = 0
    );

    //-----------------------------------------------------------------------//
    // "CancelAllOrdersOnBehalf":                                            //
    //-----------------------------------------------------------------------//
    typedef StratMSEnvTypes::MassCancelSideT MassCancelSideT;

    // (1) For a specific "symbol" (which implies the OMC), and optionally, an
    //     Account:
    void CancelAllOrdersOnBehalf
    (
      char const*             strat_name,
      char const*             omc_name,
      char const*             symbol,
      Events::AccCrypt        acc_crypt = 0,
      MassCancelSideT         sides     = MassCancelSideT::BothT,
      Events::UserData const* user_data = NULL
    );

    // (2) For all Symbols and all OMCs of the specified Strategy:
    //
    void CancelAllOrdersOnBehalf
    (
      char const*             strat_name,
      MassCancelSideT         sides     = MassCancelSideT::BothT,
      Events::UserData const* user_data = NULL
    );

    //-----------------------------------------------------------------------//
    // "LiquidatePortfolioOnBehalf":                                         //
    //-----------------------------------------------------------------------//
    void LiquidatePortfolioOnBehalf
    (
      char const*             strat_name,
      Events::UserData const* user_data = NULL
    );

  private:
    OrderBookSnapShot const* GetOrderBook
    (
      char const* mdc_name,
      char const* symbol
    );

    //=======================================================================//
    // Accounting Info:                                                      //
    //=======================================================================//
  public:
    typedef StratMSEnvTypes::AccStateInfo  AccStateInfo;

    //=======================================================================//
    // Per-Connector Accounts For Manual Order Placing:                      //
    //=======================================================================//
    // Obtaining "AccountShort"s for a given OMC Name.
    // NB: If the argis invalid, an empty vector is returned -- no exceptions
    // are thrown:
    //
    void GetConnAccounts
    (
      char const*             omc_name,
      std::vector<AccShort>*  res
    )
    const;

    //=======================================================================//
    // "ExportFromMySQL": DB Data Export:                                    //
    //=======================================================================//
    // Supported file formats:
    //
    enum class ExportFormatT
    {
      CSV   = 0,
      XLS   = 1,
      HDF5  = 2,
      HTML  = 3
    };

    // XXX: This is for infrequent requests only -- a new DB connection is cre-
    // ated every time. MUST NOT be available for Strategies, because a Global
    // MySQL access is used:
    //
    static void ExportFromMySQL
    (
      char const*   db_name,
      char const*   sql_select,
      ExportFormatT format,
      char const*   out_file,
      char const*   report_title = ""
    );

    //=======================================================================//
    // Misc:                                                                 //
    //=======================================================================//
    void SaveReport
    (
     char const*        generator_path,
     char const*        out_file,
     Arbalete::DateTime start_time,
     Arbalete::DateTime nd_time
    );
  };
}
#endif  // MAQUETTE_STRATEGYADAPTOR_MSENV_H
