// vim:ts=2:et
//===========================================================================//
//               "Connectors/ITCH/EConnector_ITCH_HotSpotFX.h":              //
//    "EConnector_TCP_ITCH" Impl for ITCH::DialectT::HotSpotFX_{Gen,Link}    //
//===========================================================================//
#pragma once

#include "Connectors/ITCH/EConnector_TCP_ITCH.h"
#include "Connectors/FIX/EConnector_FIX.h"
#include "Venues/HotSpotFX/Features_ITCH.h"
#include <type_traits>

namespace MAQUETTE
{
  //=========================================================================//
  // "EConnector_ITCH_HotSpotFX" Class:                                      //
  //=========================================================================//
  // NB: This class is STILL parameterised by "ITCH::DialectT", but only 2 vals
  // of the latter are accepted: HotSpotFX_{Gen,Link}:
  //
  template<ITCH::DialectT::type D>
  class    EConnector_ITCH_HotSpotFX final:
    public EConnector_TCP_ITCH<D, EConnector_ITCH_HotSpotFX<D>>
  {
  private:
    //=======================================================================//
    // Types:                                                                //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // Internal Types:                                                       //
    //-----------------------------------------------------------------------//
    // "TCP_Connector" and "EConnector_TCP_ITCH" must have full access to the
    // internals of this class (eg private Call-Backs):
    friend class   TCP_Connector         <EConnector_ITCH_HotSpotFX<D>>;
    friend class   EConnector_TCP_ITCH<D, EConnector_ITCH_HotSpotFX<D>>;
    using  TCPI  = EConnector_TCP_ITCH<D, EConnector_ITCH_HotSpotFX<D>>;

    // Constraints on the Dialect:
    static_assert(D == ITCH::DialectT::HotSpotFX_Gen ||
                  D == ITCH::DialectT::HotSpotFX_Link,
                  "EConnector_ITCH_HotSpotFX: Can be instantiated with "
                  "ITCH::DialectT::HotSpotFX_{Gen,Link} only");

    static_assert(!(TCPI::IsFullAmt && TCPI::WithFracQtys),
                  "EConnector_ITCH_HotSpotFX: FullAmount and FracQtys modes "
                  "are incompatible");

    //-----------------------------------------------------------------------//
    // OMC associated with this MDC: FIX for HotSpotFX:                      //
    //-----------------------------------------------------------------------//
    // XXX: This is not required yet, since there are no "ProcessTrade" calls
    // in ITCH -- we only receive OrderBook data...
    //
    using OMC =
      std::conditional_t
      <D == ITCH::DialectT::HotSpotFX_Gen,
       EConnector_FIX_HotSpotFX_Gen,
       EConnector_FIX_HotSpotFX_Link>;

  public:
    //-----------------------------------------------------------------------//
    // Native Qty Type:                                                      //
    //-----------------------------------------------------------------------//
    constexpr static QtyTypeT QT = QtyTypeT::QtyA;

    using QR   = typename std::conditional_t<TCPI::WithFracQtys, double, long>;
    using QtyN = Qty<QT,QR>;

  private:
    //=======================================================================//
    // Flags:                                                                //
    //=======================================================================//
    // For HotSpotFX, "WithIncrUpdates" is opposite to "IsFullAmt":
    constexpr static bool WithIncrUpdates = !TCPI::IsFullAmt;

    //=======================================================================//
    // Extra Data Flds:                                                      //
    //=======================================================================//
    // The following dynamic flag indicates whether this MDC operates in the
    // Extended Mode (ie provides MinQtys and LotSzs):
    bool const  m_withExtras;

  public:
    //=======================================================================//
    // Non-Default Ctor, Dtor:                                               //
    //=======================================================================//
    EConnector_ITCH_HotSpotFX
    (
      EPollReactor*                       a_reactor,
      SecDefsMgr*                         a_sec_defs_mgr,
      std::vector<std::string>    const*  a_only_symbols, // NULL=UseFullList
      RiskMgr*                            a_risk_mgr,
      boost::property_tree::ptree const&  a_params,
      EConnector_MktData*                 a_primary_mdc = nullptr
    );

    // Default Ctor is deleted:
    EConnector_ITCH_HotSpotFX() = delete;

    // Dtor is trivial and non-virtual, because this class is "final"; but it
    // still overrides the parent Dtor:
    ~EConnector_ITCH_HotSpotFX() noexcept override {}

    // The ConnectorName:
    static std::string
    GetConnectorName(boost::property_tree::ptree const& a_params);

  private:
    // This class is final, so the following method must finally be implemented:
    void EnsureAbstract()   const override {}

    //=======================================================================//
    // "ReadHandler":                                                        //
    //=======================================================================//
    // The actual Reader/Parser implementation for HotSpotFX ITCH. Invoked from
    // "EConnector_TCP_ITCH::ReadHandler" which is in turn
    //
    int ReadHandler
    (
      int             a_fd,       // Must be same as our "m_fd"
      char const*     a_buff,     // Points to dynamic buffer of Reactor
      int             a_size,     // Size of the chunk
      utxx::time_val  a_ts_recv
    );

    //=======================================================================//
    // Reader and Parser Utils:                                              //
    //=======================================================================//
    // XXX: They implement "integrated" Parser  and Processor  functionality.
    // This is NOT our standard architecture -- but in this case, such close
    // integration provides best performance and avoids unnecessary data for-
    // mat conversion and copying:
    //
    template<bool WithExtras>         // For optimisation only
    int ParseAndProcessSequencedPayLoad
    (
      char const*     a_msg_begin,
      char const*     a_chunk_end,
      utxx::time_val  a_ts_recv,
      utxx::time_val  a_ts_handl
    );

    template<bool WithExtras>         // For optmisation only
    bool ParseAndProcessMktSnapShot
    (
      char const*     a_msg_body,
      int             a_len,
      utxx::time_val  a_ts_exch,
      utxx::time_val  a_ts_recv,
      utxx::time_val  a_ts_handl
    );

    //=======================================================================//
    // Logging of Sent or Recvd Msgs:                                        //
    //=======================================================================//
    template<bool IsSend>
    void LogMsg (char const* a_data, int a_len) const;

    //=======================================================================//
    // Sender Utils:                                                         //
    //=======================================================================//
    // NB: "InitLogOn", "SendHeartBeat" and "GracefulStopInProgress" (which
    // actually sends a "LogOutReq") are required  as "TCP_Connector" Call-
    // Backs:
    void InitLogOn    ()                                const;
    void SendHeartBeat()                                const;
    void GracefulStopInProgress()                       const;

    // Dialect-Specific Subscription Mgmt: Call-Bacls required by the parent
    // "EConnector_TCP_ITCH" class: Effectively wrappers around the Protocol
    // Level:
    void SendMktDataSubscrReq  (SecDefD const& a_instr) const;
    void SendMktSnapShotReq    (SecDefD const& a_instr) const;
    void SendMktDataUnSubscrReq(SecDefD const& a_instr) const;
  };

  //=========================================================================//
  //  Aliases:                                                               //
  //=========================================================================//
  using EConnector_ITCH_HotSpotFX_Gen  =
        EConnector_ITCH_HotSpotFX<ITCH::DialectT::HotSpotFX_Gen>;

  using EConnector_ITCH_HotSpotFX_Link =
        EConnector_ITCH_HotSpotFX<ITCH::DialectT::HotSpotFX_Link>;
}
// End namespace MAQUETTE
