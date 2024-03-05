// vim:ts=2:et
//===========================================================================//
//                 "Connectors/ITCH/EConnector_TCP_ITCH.h":                  //
//     MktData Connector Implementing a Dialect of ITCH Protocol over TCP    //
//===========================================================================//
#pragma once

#include "Protocols/ITCH/Features.h"
#include "Connectors/ITCH/Configs.h"
#include "Connectors/EConnector_MktData.h"
#include "Connectors/TCP_Connector.h"

namespace MAQUETTE
{
  //=========================================================================//
  // "EConnector_TCP_ITCH" Class:                                            //
  //=========================================================================//
  // Class Design Considerations:
  // (*) Similar to "EConnector_FAST", "EConnector_TCP_ITCH" contains only Pro-
  //     tocol-independent code, so it must not be "final" -- it will be exten-
  //     ded with the actual ITCH Dialect-specific implementations.
  // (*) This is dissimilar to "EConnector_FIX": the latter does not need to be
  //     extendable because all FIX Dialects have very similar  generic syntax,
  //     so the differences between FIX Dialects  do not  require any specific
  //     code to be written -- they can be managed solely via static "Protocol-
  //     Features".
  // (*) However, unlike "EConnector_FAST", "EConnector_TCP_ITCH" is implement-
  //     ed on top of "TCP_Connector" and must therefore provide call-backs (eg
  //     "ReadHandler") to the latter. Because such call-backs are highly ITCH
  //     Dialect-specific, they can only be implemented bu Derived classes. Ie,
  //     "EConnector_TCP_ITCH" needs to be parameterised by its Derived  class
  //     (similar to "TCP_Connector").
  // (*) Similar to "EConnector_FIX", "EConnector_TCP_ITCH" is still parameter-
  //     ised by "ITCH::DialectT", because it is convenient in some places (eg
  //     for static ProtocolFeatures and access to Configs). XXX:  In GCC,  we
  //     would be able to get this param from "Derived" class rather than pro-
  //     vide it explicitly -- but that does NOT work swith CLang!
  // (*) NB: "TCP_Connector" is instantiated  with the furthest-down "Derived"
  //     class, NOT with this class! Only the "Derived" class ultimately provi-
  //     des all (Dialect-specific) call-backs needed for "TCP_Connector":
  //
  template<ITCH::DialectT::type D, typename Derived>
  class    EConnector_TCP_ITCH:
    public EConnector_MktData,
    public TCP_Connector<Derived>
  {
  protected:
    //=======================================================================//
    // Flags:                                                                //
    //=======================================================================//
    // The following Flags are presumably valid for all TCP ITCH Dialects and
    // impls:
    constexpr static bool IsMultiCast      = false; // It's TCP!
    constexpr static bool WithOrdersLog    = true;
    constexpr static bool WithIncrUpdates  = true;
    constexpr static bool StaticInitMode   = false; // Only applicable to UDP
    constexpr static bool ChangeIsPartFill = true;
    constexpr static bool NewEncdAsChange  = false; // Very rare anyway
    constexpr static bool ChangeEncdAsNew  = false; // ditto
    constexpr static bool IsSparse         = false;

    // The following Flags are Dialect-dependent:
    constexpr static bool IsFullAmt        =
      ITCH::ProtocolFeatures<D>::s_hasFullAmount;

    constexpr static bool WithFracQtys     =
      ITCH::ProtocolFeatures<D>::s_hasFracQtys;

    constexpr static  bool WithRelaxedOBs  =
      ITCH::ProtocolFeatures<D>::s_hasRelaxedOBs;

    // Left for the "Derived" class:
    // "IsSnapShot", "WithIncrUpdates"

    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    using  TCPC = TCP_Connector<Derived>;

    // Ptr to the Config (it may actually be a derived class of "TCP_ITCH_
    // Config"):
    TCP_ITCH_Config const*  m_config;

    //=======================================================================//
    // Non-Default Ctor, Dtor:                                               //
    //=======================================================================//
    // NB: They are "protected" as must not be invoked directly -- only via
    // "Derived":
    //
    EConnector_TCP_ITCH
    (
      EConnector_MktData*                a_primary_mdc, // May be NULL
      boost::property_tree::ptree const& a_params
    );

    // Default Ctor is deleted:
    EConnector_TCP_ITCH() = delete;

    // The Dtor is trivial, but it must still be "virtual" -- this class is NOT
    // "final" yet:
    virtual ~EConnector_TCP_ITCH() noexcept override {}

  public:
    //=======================================================================//
    // Virtual methods from "EConnector" and "EConnector_MktData":           //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "Start", "Stop", Properties:                                          //
    //-----------------------------------------------------------------------//
    // They are made "public" because no longer overridden by Derived classes:
    //
    void Start()          override;
    void Stop ()          override;

    // The following is needed for technical reasons (ambiguity resolution in
    // multiple inheritance):
    bool IsActive() const override;

  protected:
    //=======================================================================//
    // Call-Backs for the static interface of "TCP_Connector":               //
    //=======================================================================//
    // "InitGracefulStop": XXX: For an MDC, it is a NoOp:
    void InitGracefulStop()  const    {}

    // "ServerInactivityTimeOut":
    void ServerInactivityTimeOut      (utxx::time_val a_ts_recv);

    // "StopNow":
    template<bool FullStop>
    void StopNow(char const* a_where,  utxx::time_val a_ts_recv);

    // "InitSession":
    // For ITCH, no special session initialisation is required, so this method
    // signals completion immediately:
    //
    void InitSession()  { TCPC::SessionInitCompleted(); }

    // NB: the remaining "TCP_Connector" call-backs are Dialect-specific, so
    // implemented in the "Derived" class:
    // (*) "ReadHandler"
    // (*) "InitLogOn"
    // (*) "GracefulStopInProgress"
    // (*) "SendHeartBeat"

    //=======================================================================//
    // Session-Level Mechanisms:                                             //
    //=======================================================================//
    // Call-Forward into "TCP_Connector":
    void LogOnCompleted(utxx::time_val a_ts_recv);

    // Sending of Session-Level Msgs. In ITCH, Msg does not need to be serial-
    // ized -- it is just put on the wire "as is"!
    template<typename Msg>
    void SendMsg(Msg const& a_msg)   const;

    // NB: "LogMsg" is Dialect-specific, so implemented by the "Derived" class

    //=======================================================================//
    // Processing of Session-Level Msgs:                                     //
    //=======================================================================//
    void ProcessLogInAccepted    ();
    void ProcessLogInRejected    (char const* a_reason,  int a_len);
    void ProcessServerHeartBeat  ();
    void ProcessErrorNotification(char const* a_err_msg, int a_len);
    void ProcessEndOfSession     ();

    //=======================================================================//
    // Processing of Application-Level Msgs:                                 //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "NewOrder":                                                           //
    //-----------------------------------------------------------------------//
    template<QtyTypeT QT, typename QR>
    void ProcessNewOrder
    (
      SymKey const&     a_symbol,
      OrderID           a_id,
      bool              a_is_bid,
      PriceT            a_px,
      Qty<QT,QR>        a_qty,
      utxx::time_val    a_ts_exch,
      utxx::time_val    a_ts_recv,
      utxx::time_val    a_ts_handl
    );

    //-----------------------------------------------------------------------//
    // "ModifyOrder":                                                        //
    //-----------------------------------------------------------------------//
    template<QtyTypeT QT, typename QR>
    void ProcessModifyOrder
    (
      SymKey const&     a_symbol,
      OrderID           a_id,
      Qty<QT,QR>        a_new_qty,
      utxx::time_val    a_ts_exch,
      utxx::time_val    a_ts_recv,
      utxx::time_val    a_ts_handl
    );

    //-----------------------------------------------------------------------//
    // "CancelOrder":                                                        //
    //-----------------------------------------------------------------------//
    template<QtyTypeT QT, typename QR>
    void ProcessCancelOrder
    (
      SymKey const&     a_symbol,
      OrderID           a_id,
      utxx::time_val    a_ts_exch,
      utxx::time_val    a_ts_recv,
      utxx::time_val    a_ts_handl
    );
  };
} // End namespace MAQUETTE
