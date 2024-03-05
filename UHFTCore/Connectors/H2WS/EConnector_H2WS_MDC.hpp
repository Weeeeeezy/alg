// vim:ts=2:et
//===========================================================================//
//                "Connectors/H2WS/EConnector_H2WS_MDC.hpp":                 //
//===========================================================================//
#pragma once

#include "Connectors/H2WS/EConnector_H2WS_MDC.h"
#include "Connectors/H2WS/EConnector_WS_MDC.hpp"
#include "Connectors/H2WS/H2Connector.hpp"
#include "Basis/IOUtils.hpp"
#include <boost/algorithm/string.hpp>

namespace MAQUETTE
{
  //=========================================================================//
  // Non-Default Ctor:                                                       //
  //=========================================================================//
  template<typename Derived, typename H2Conn>
  inline EConnector_H2WS_MDC<Derived, H2Conn>::EConnector_H2WS_MDC
  (
    EConnector_MktData*                 a_primary_mdc,
    boost::property_tree::ptree const&  a_params
  )
  : //-----------------------------------------------------------------------//
    // "EConnector_WS_MDC":                                                  //
    //-----------------------------------------------------------------------//
    EConnector_WS_MDC<Derived>
    (
      a_primary_mdc,
      a_params
    ),
    //-----------------------------------------------------------------------//
    // "EConnector_H2WS_MDC" Flds:                                           //
    //-----------------------------------------------------------------------//
    m_h2conns   (),
    m_currH2Idx (-1),
    m_currH2Conn(nullptr),  // As yet
    m_allInds   (nullptr)   // Stub only: always NULL
  {
    //-----------------------------------------------------------------------//
    // Create the H2 Connectors:                                             //
    //-----------------------------------------------------------------------//
    size_t nH2 = (a_params.get<size_t>("NumH2Conns", 1));
    if (utxx::unlikely(nH2 <= 0 || nH2 > MaxH2Conns))
      throw utxx::badarg_error
            ("EConnector_H2WS_MDC: Invalid NumH2Conns=", nH2, ": Limit=",
             MaxH2Conns);

    //-----------------------------------------------------------------------//
    // H2 Connectors can be explicitly bound to Interfaces' IPs              //
    //-----------------------------------------------------------------------//
    // NB: Newly-created H2Conns are first attached to the NotActive list!
    auto ipList = a_params.get<std::string>("IPs", "");

    std::vector<std::string> ips;
    if (ipList == "")
      ips.push_back("");
    else
      boost::split(ips, ipList, boost::is_any_of(","));

    for (size_t i = 0; i < nH2; ++i)
    {
      boost::trim(ips[i % ips.size()]);
      m_h2conns.push_back
      (
        new H2Conn
        (
          static_cast<Derived*>(this),
          a_params,
          0,
          ips[i % ips.size()]
      ));
    }
    assert(m_h2conns.size() == nH2);

    //-----------------------------------------------------------------------//
    // Initialize the Rotation:                                              //
    //-----------------------------------------------------------------------//
    m_currH2Idx  = 0;
    m_currH2Conn = m_h2conns[0];
    assert(m_currH2Conn != nullptr);
  }

  //=========================================================================//
  // Dtor:                                                                   //
  //=========================================================================//
  // NB: Identical to "~EConnector_H2WS_OMC":
  //
  template<typename Derived, typename H2Conn>
  inline EConnector_H2WS_MDC<Derived, H2Conn>::~EConnector_H2WS_MDC() noexcept
  {
    // Catch all possible exns:
    try
    {
      // Destroy the "H2Conn"s:
      for (int i = 0; i < int(m_h2conns.size()); ++i)
        if (m_h2conns     [size_t(i)] != nullptr)
        {
          delete m_h2conns[size_t(i)];
          m_h2conns       [size_t(i)]  = nullptr;
        }
      m_h2conns.clear();
      m_currH2Conn = nullptr;      // This is just a rotating alias
    }
    catch(...){}
  }

  //=========================================================================//
  // Starting, Stopping and Properties:                                      //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "Start":                                                                //
  //-------------------------------------------------------------------------//
  // NB: Identical to "EConnector_H2WS_OMC::Start":
  //
  template <typename Derived, typename H2Conn>
  inline void EConnector_H2WS_MDC<Derived, H2Conn>::Start()
  {
    // Start the WS leg (which invoked "TCPWS::Start" and resets the state):
    MDCWS::Start();

    // Start all "H2Conn"s:
    for (H2Conn* h2conn: m_h2conns)
      // XXX: Just start the resp TCP Connector; H2Conn state will be reset la-
      // ter by "Derived":
      h2conn->Start();
  }

  //-------------------------------------------------------------------------//
  // "StopNow":                                                              //
  //-------------------------------------------------------------------------//
  // NB: Very similar to "EConnector_H2WS_OMC::StopNow":
  //
  template <typename Derived, typename H2Conn>
  template <bool FullStop>
  inline void EConnector_H2WS_MDC<Derived, H2Conn>::StopNow
    (char const* a_where, utxx::time_val a_ts_recv)
  {
    assert(a_where != nullptr);

    // Stop the "H2Conn"s (NB: "m_h2conns" may even be empty at this point):
    for (H2Conn* h2conn: m_h2conns)
      h2conn->template StopNow<FullStop>(a_where, a_ts_recv);

    // Stop the WS. It will Notify all Strategies:
    MDCWS::template StopNow<FullStop>(a_where, a_ts_recv);

    LOG_INFO(1,
      "EConnector_H2WS_MDC({}): Connector STOPPED{}", a_where,
      FullStop ? "." : ", but will re-start")
  }

  //-------------------------------------------------------------------------//
  // "H2ConnStopped":
  //-------------------------------------------------------------------------//
  // NB: Very similar to "EConnector_H2WS_OMC::H2ConnStopped":
  //
  template <typename Derived, typename H2Conn>
  template <bool FullStop>
  inline void EConnector_H2WS_MDC<Derived, H2Conn>::H2ConnStopped
  (
    H2Conn const* CHECK_ONLY(a_h2conn),
    char   const* a_where
  )
  {
    CHECK_ONLY
    (
      assert(a_h2conn != nullptr && a_where != nullptr);

      auto it = std::find(m_h2conns.begin(), m_h2conns.end(), a_h2conn);
      if (utxx::unlikely(it == m_h2conns.end()))
        throw utxx::logic_error
              ("EConnector_H2WS_MDC::H2ConnStopped: H2Conn=", a_h2conn->m_name,
               " Not Found");
    )
    if (utxx::unlikely(m_h2conns.empty()))
      // XXX: No active "H2Conn"s remain -- will have to stop the entire MDC:
      StopNow<FullStop>("All H2Conns Stopped", utxx::now_utc());

    // Otherwise, continue with reduced number of "H2Conn"s. Rotate them to get
    // the curr one right:
    RotateH2Conns();

    LOG_INFO(3,
      "EConnector_H2WS_MDC::H2ConnStopped({}): Stopped, Continuing with {} "
      "operational H2Conn(s)", a_where, m_h2conns.size())
  }

  //-------------------------------------------------------------------------//
  // "IsActive":                                                             //
  //-------------------------------------------------------------------------//
  // NB: Very similar to "EConnector_H2WS_OMC::IsActive":
  //
  template<typename Derived, typename H2Conn>
  inline bool EConnector_H2WS_MDC<Derived, H2Conn>::IsActive() const
  {
    // XXX: We consider the MDC to be Active if the WS leg is Active, and at
    // least 1 H2 is Active:
    if (!MDCWS::IsActive())
      return false;

    for (H2Conn const* h2conn: m_h2conns)
      if (h2conn->IsActive())
        return true;

    // If we got here, all "H2Conn"s are NOT Active, nor is the whole MDC:
    return false;
  }

  //-------------------------------------------------------------------------//
  // "Stop" (graceful, external, virtual):                                   //
  //-------------------------------------------------------------------------//
  // NB: Very similar to "EConnector_H2WS_OMC::Stop":
  //
  template<typename Derived, typename H2Conn>
  inline void EConnector_H2WS_MDC<Derived, H2Conn>::Stop()
  {
    // Stop the "H2Conn"s gracefully (FullStop=true):
    for (H2Conn* h2conn: m_h2conns)
      (void) h2conn->template Stop<true>();

    // Stop the WS leg gracefully (with FullStop):
    MDCWS::Stop();
  }

  //=========================================================================//
  // Session Mgmt:                                                           //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "RotateH2Conns":                                                        //
  //-------------------------------------------------------------------------//
  // Used by Derived Connector to rotate "m_connH2" to the next sending socket
  // (wrapping around the end):
  //
  template<typename Derived, typename H2Conn>
  inline void EConnector_H2WS_MDC<Derived, H2Conn>::RotateH2Conns() const
  {
    // Use by Derived MDC to move to the next "H2Conn".
    // NB: Normally, all "H2Conn"s in "m_h2Conns" should be "Active", but that
    // is NOT guaranteed during Start/Stop periods, so we make no formal assum-
    // ptions about that. If an InActive "H2Conn" is found, try to Rotate furt-
    // her and find a good one -- otherwise an attempt to use it may result in
    // an exception:
    int l = int(m_h2conns.size());
    if (utxx::unlikely(l == 0))
    {
      LOG_WARN(2, "EConnector_H2WS_MDC::RotateH2Conns: No Active H2Conns")
      m_currH2Idx  = -1;
      m_currH2Conn = nullptr;
      return;     // Cannot do anything
    }

    // XXX: The "for" loop is a formal guarantee that our search for a valid
    // "H2Conn" is bounded. Normally, just 1 iteration is enough:
    //
    for (int i = 0; i < l; ++i)
    {
      m_currH2Idx  = (m_currH2Idx + 1) % l;
      m_currH2Conn =  m_h2conns[size_t(m_currH2Idx)];
      assert(m_currH2Conn != nullptr);

      // Accept this choice if it is Active, otherwise go further trying to
      // find a good one (which may still not be available):
      if (utxx::likely(m_currH2Conn->IsActive()))
        break;
    }
  }

  //-------------------------------------------------------------------------//
  // "H2LogOnCompleted":                                                     //
  //-------------------------------------------------------------------------//
  // NB: Identical to "EConnector_H2WS_OMC::H2LogOnCompleted":
  //
  template<typename Derived, typename H2Conn>
  inline void EConnector_H2WS_MDC<Derived, H2Conn>::H2LogOnCompleted
    (H2Conn const* a_h2conn)
  {
    assert(a_h2conn != nullptr && a_h2conn->IsActive());

    // Add this "H2Conn" to the over-all list if it is not there yet:
    auto cit =  std::find(m_h2conns.cbegin(), m_h2conns.cend(), a_h2conn);
    if  (cit == m_h2conns.cend())
      m_h2conns.push_back(const_cast<H2Conn*>(a_h2conn));

    // Check whether we can declare the whole MDC "Active". For that, we just
    // need the WS leg to be Active -- no need to wait for other "H2Conn"s:
    if (MDCWS::IsActive())
      H2WSTurnedActive();
  }

  //-------------------------------------------------------------------------//
  // "WSLogOnCompleted":                                                     //
  //-------------------------------------------------------------------------//
  // NB: Identical to EConnector_H2WS_OMC::WSLogOnCompleted":
  //
  template<typename Derived, typename H2Conn>
  inline void EConnector_H2WS_MDC<Derived, H2Conn>::WSLogOnCompleted()
  {
    // IMPORTANT: notify the TCPWS directly, NOT MDCWS -- otherwise Strategies
    // could be notified prematurely:
    //
    TCPWS::LogOnCompleted();
    assert(TCPWS::IsActive());

    // Check if at lest one H2 is Active as well, in which case the whole MDC
    // becomes Active:
    for (H2Conn const* h2conn: m_h2conns)
    {
      assert(h2conn != nullptr);
      if (h2conn->IsActive())
      {
        H2WSTurnedActive();
        break;
      }
    }
  }

  //-------------------------------------------------------------------------//
  // "H2WSTurnedActive":                                                     //
  //-------------------------------------------------------------------------//
  template<typename Derived, typename H2Conn>
  inline void EConnector_H2WS_MDC<Derived, H2Conn>::H2WSTurnedActive()
  {
    // Further action may be delegated to "Derived": XXX: Use a different method
    // name, otherwise infinite recursion may occur if the method is not implem-
    // ented there!
    static_cast<Derived*>(this)->OnTurningActive();

    // Notify the Strategies:
    utxx::time_val now = utxx::now_utc();
    this->EConnector::ProcessStartStop(true, nullptr, 0, now, now);

    LOG_INFO(1,
      "EConnector_H2WS_MDC::H2WSTurnedActive: Connector is now ACTIVE")
  }
} // End namespace MAQUETTE
