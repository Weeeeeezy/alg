// vim:ts=2:et
//===========================================================================//
//           "Connectors/H2WS/LATOKEN/EConnector_WS_LATOKEN_MDC.h":          //
//                          WS-Based MDC for LATOKEN                         //
//===========================================================================//
#pragma once

#include "Connectors/H2WS/EConnector_WS_MDC.h"
#include "Protocols/H2WS/LATOKEN-OMC.h"
#include "Protocols/FIX/Msgs.h"
#include <map>

namespace MAQUETTE
{
  //=========================================================================//
  // "EConnector_WS_LATOKEN_MDC" Class:                                      //
  //=========================================================================//
  class    EConnector_WS_LATOKEN_MDC final:
    public EConnector_WS_MDC<EConnector_WS_LATOKEN_MDC>
  {
  public:
    constexpr static char Name[] = "WS-LATOKEN-MDC";
    //=======================================================================//
    // Types and Consts:                                                     //
    //=======================================================================//
    // The Native Qty type: LATOKEN uses Fractional QtyA:
    constexpr static QtyTypeT QT   = QtyTypeT::QtyA;
    using                     QR   = double;
    using                     QtyN = Qty<QT,QR>;

  private:
    //=======================================================================//
    // Configuration:                                                        //
    //=======================================================================//
    // For parsing Orders, we re-use the LATOKEN OMC JSON Parser:
    // IsKafkaOrMDA=true here:
    using  Handler = H2WS::LATOKEN::HandlerOMC<true>;
    using  MDCWS   = EConnector_WS_MDC<EConnector_WS_LATOKEN_MDC>;

    // "TCP_Connector", "EConnector_WS_MDC" and "H2WS::WSProtoEngine" must have
    // full access to this class:
    friend class     TCP_Connector      <EConnector_WS_LATOKEN_MDC>;
    friend class     EConnector_WS_MDC  <EConnector_WS_LATOKEN_MDC>;
    friend class     H2WS::WSProtoEngine<EConnector_WS_LATOKEN_MDC>;

    //-----------------------------------------------------------------------//
    // Flags required by "EConnector_WS_MDC":                                //
    //-----------------------------------------------------------------------//
    // MktDepth is conceptually unlimited:
    constexpr static int  MktDepth         = 0;

    // Trades info is available (in general; need to be explicitly subscribed
    // for):
    constexpr static bool HasTrades        = true;

    // We do NOT receive MktData SeqNums:
    constexpr static bool HasSeqNums       = false;

    // XXX: Sparse OrderBooks are not yet used for LATOKEN, though it may be a
    // very good idea:
    constexpr static bool IsSparse         = false;

    //-----------------------------------------------------------------------//
    // Flags for "EConnector_MktData" templated methods:                     //
    //-----------------------------------------------------------------------//
    constexpr static bool IsMultiCast      = false; // It's TCP/WS
    constexpr static bool WithIncrUpdates  = true;  // Yes, not only SnapShots
    constexpr static bool WithOrdersLog    = true;  // WSMDA is FOB/FOL-based
    constexpr static bool WithRelaxedOBs   = false; // Be strict
    constexpr static bool StaticInitMode   = false; // Appl to MultiCast only
    constexpr static bool ChangeIsPartFill = true;  // As normal
    constexpr static bool NewEncdAsChange  = false; // New != Change in LATOKEN
    constexpr static bool ChangeEncdAsNew  = false; // ditto
    constexpr static bool IsFullAmt        = false; // Needless to say

    //-----------------------------------------------------------------------//
    // "GetWSConfig":                                                        //
    //-----------------------------------------------------------------------//
    static H2WSConfig const& GetWSConfig(MQTEnvT a_env);

    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    mutable rapidjson::Reader             m_reader;
    mutable Handler                       m_handler;

  public:
    //=======================================================================//
    // Non-Default Ctor, Dtor:                                               //
    //=======================================================================//
    EConnector_WS_LATOKEN_MDC
    (
      EPollReactor*                       a_reactor,       // Non-NULL
      SecDefsMgr*                         a_sec_defs_mgr,  // Non-NULL
      std::vector<std::string>    const*  a_only_symbols,  // NULL=UseFullList
      RiskMgr*                            a_risk_mgr,      // Depends...
      boost::property_tree::ptree const&  a_params,
      EConnector_MktData*                 a_primary_mdc = nullptr
    );

    // The Dtor is trivial, and non-virtual (because this class is "final"):
    ~EConnector_WS_LATOKEN_MDC() noexcept override {}

  private:
    // This class is final, so the following method must finally be implemented:
    void EnsureAbstract()   const override {}

    //=======================================================================//
    // Msg Processing ("H2WS::WSProtoEngine" Call-Back):                     //
    //=======================================================================//
    //  "ProcessWSMsg" returns "true" to continue receiving msgs, "false" to
    //  stop:
    bool ProcessWSMsg
    (
      char const*     a_msg_body,
      int             a_msg_len,
      bool            a_last_msg,
      utxx::time_val  a_ts_recv,
      utxx::time_val  a_ts_handl
    );

    //=======================================================================//
    // Msg Parsing and Processing Helpers:                                   //
    //=======================================================================//
    // "ProcessSnapShot":
    // Parsing and Processing the Full Orders Book (FOB). Returns "true" iff the
    // msg was valid:
    bool ProcessSnapShot
    (
      char const*     a_fob,
      OrderBook*      a_ob,
      utxx::time_val  a_ts_recv,
      utxx::time_val  a_ts_handl
    );

    // "ProcessOrder":
    // Parsing and Processing an individual Order from FOB or FOL. Returns
    // "true" on success, "false" otherwise:
    bool ProcessOrder
    (
      char const*     a_order,
      OrderBook*      a_ob,
      std::map<OrderID, H2WS::LATOKEN::MsgOMC>*
                      a_snap_shot_ords     // NULL  if not aSnapShot (FOB)
    );

    //=======================================================================//
    // "TCP_Connector" Call-Backs (others are in "EConnector_WS_MDC"):       //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // Session-Level Methods:                                                //
    //-----------------------------------------------------------------------//
    //  "InitSession": Performs HTTP connection and WS upgrade:
    void InitSession() const;

    //  "InitLogOn" : Although there is no LogOn per se, it performs Protocol-
    //  level MktData subscription (which is NOT done in "InitSession"):
    void InitLogOn  ();
  };
} // End namespace MAQUETTE
