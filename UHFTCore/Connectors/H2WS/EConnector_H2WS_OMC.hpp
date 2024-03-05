// vim:ts=2:et
//===========================================================================//
//                "Connectors/H2WS/EConnector_H2WS_OMC.hpp":                 //
//===========================================================================//
#pragma once

#include "Connectors/H2WS/EConnector_H2WS_OMC.h"
#include "Connectors/H2WS/EConnector_WS_OMC.hpp"
#include "Connectors/H2WS/H2Connector.hpp"
#include "Basis/IOUtils.hpp"
#include <boost/algorithm/string.hpp>

namespace MAQUETTE
{
  //=========================================================================//
  // Non-Default Ctor:                                                       //
  //=========================================================================//
  template<typename Derived, typename H2Conn>
  inline EConnector_H2WS_OMC<Derived, H2Conn>::EConnector_H2WS_OMC
  (
    boost::property_tree::ptree const&  a_params
  )
  : //-----------------------------------------------------------------------//
    // "EConnector_WS_OMC":                                                  //
    //-----------------------------------------------------------------------//
    EConnector_WS_OMC<Derived>(a_params),

    //-----------------------------------------------------------------------//
    // "EConnector_H2WS_OMC" Flds:                                           //
    //-----------------------------------------------------------------------//
    m_account       (a_params),
    m_numH2Conns    (a_params.get<unsigned>("NumH2Conns", 1)),
    m_activeH2Conns (),
    m_notActvH2Conns(),
    m_currH2Idx     (-1),     // Not yet selected: All H2Conns are NotActive
    m_currH2Conn    (nullptr) //
  {
    //-----------------------------------------------------------------------//
    // Create the H2 Connectors:                                             //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely(m_numH2Conns > MaxH2Conns))
      throw utxx::badarg_error
            ("EConnector_H2WS_OMC: Invalid NumH2Conns=", m_numH2Conns,
             ": Limit=", MaxH2Conns);

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

    for (size_t i = 0; i < m_numH2Conns; ++i)
    {
      boost::trim(ips[i % ips.size()]);
      m_notActvH2Conns.push_back
      (
        new H2Conn
        (
          static_cast<Derived*>(this),
          a_params,
          EConnector_OrdMgmt::m_maxReq12s,
          ips[i % ips.size()]
      ));
    }

    assert(m_notActvH2Conns.size() == m_numH2Conns &&
           m_activeH2Conns.empty());
    LOG_INFO(2,
      "EConnector_H2WS_OMC: Created {} H2Connector(s), yet NotActive",
      m_notActvH2Conns.size())
  }

  //=========================================================================//
  // Dtor:                                                                   //
  //=========================================================================//
  template<typename Derived, typename H2Conn>
  inline EConnector_H2WS_OMC<Derived, H2Conn>::~EConnector_H2WS_OMC() noexcept
  {
    // Catch all possible exns:
    try
    {
      // Destroy both Active and NotActive "H2Conn"s:
      //
      for (int i = 0; i < int(m_activeH2Conns.size()); ++i)
        if (m_activeH2Conns      [size_t(i)] != nullptr)
        {
          delete m_activeH2Conns [size_t(i)];
          m_activeH2Conns        [size_t(i)]  = nullptr;
        }
      m_activeH2Conns.clear();
      m_currH2Conn = nullptr;   // This is just a rotating alias

      for (int i = 0; i < int(m_notActvH2Conns.size()); ++i)
        if (m_notActvH2Conns     [size_t(i)] != nullptr)
        {
          delete m_notActvH2Conns[size_t(i)];
          m_notActvH2Conns       [size_t(i)]  = nullptr;
        }
    }
    catch(...){}
  }

  //=========================================================================//
  // Starting, Stopping and Properties:                                      //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "Start":                                                                //
  //-------------------------------------------------------------------------//
  template <typename Derived, typename H2Conn>
  inline void EConnector_H2WS_OMC<Derived, H2Conn>::Start()
  {
    // Start the WS leg (which invokes "TCPWS::Start"):
    OMCWS::Start();

    // First of all, at this point there should be no Active H2Conns:
    if (utxx::unlikely(!m_activeH2Conns.empty()))
    {
      LOG_WARN(2,
        "EConnector_H2WS_OMC::Start: Found {} Already-Active H2Conns",
        m_activeH2Conns.size())

      // But then, "H2Conn"s marked as Active must really be Active:
      CheckActiveH2Conns  ();
    }
    // And other "H2Conn"s must NOT be Active:
    CheckNotActiveH2Conns();

    // Start all "H2Conn"s which are yet NotActive. We do NOT move them into
    // the Active list immediately -- only when they actually become Active.
    // If they are already Starting or so, repeated "Start"s will be handled
    // accordingly:
    //
    for (H2Conn* h2conn: m_notActvH2Conns)
    {
      // XXX: Just start the resp TCP Connector; H2Conn state will be reset
      // later by "Derived":
      assert(h2conn != nullptr && !(h2conn->IsActive()));
      h2conn->Start();
    }
    LOG_INFO(2,
      "EConnector_H2WS_OMC::Start: Started {} H2Conns",
      m_notActvH2Conns.size())
  }

  //-------------------------------------------------------------------------//
  // "StopNow":                                                              //
  //-------------------------------------------------------------------------//
  template <typename Derived, typename H2Conn>
  template <bool FullStop>
  inline void EConnector_H2WS_OMC<Derived, H2Conn>::StopNow
    (char const* a_where, utxx::time_val a_ts_recv)
  {
    assert(a_where != nullptr);

    CHECK_ONLY
    (
      CheckActiveH2Conns   ();
      CheckNotActiveH2Conns();
    )
    // XXX: To be on a safe side, first stop the "H2Conn"s which are NotActive
    // (because they could be Starting or so):
    //
    for (H2Conn* h2conn: m_notActvH2Conns)
    {
      assert(h2conn != nullptr);
      h2conn->template StopNow<FullStop>(a_where, a_ts_recv);

      // It must really become "Inactive", not just !Active:
      assert(h2conn->IsInactive());
    }
    // Then stop the "H2Conn"s which are yet on the Active list (it is OK if
    // this list is already empty), and move them to the NotActive list:
    //
    for (H2Conn* h2conn: m_activeH2Conns)
    {
      assert(h2conn != nullptr && h2conn->IsActive());
      h2conn->template StopNow<FullStop>(a_where, a_ts_recv);

      // Again, it must really become "InActive", not just !Active:
      assert(h2conn->IsInactive());
      assert
        (std::find(m_notActvH2Conns.cbegin(), m_notActvH2Conns.cend(), h2conn)
         == m_notActvH2Conns.cend());
      m_notActvH2Conns.push_back(h2conn);
    }
    m_activeH2Conns.clear();
    // FIXME: We could get into an inconsistent state if an exception propagates
    // out of the above loop, but this is highly unlikely...

    // Finally, StopNow the WS. In addition, this will "RemovePusherTimer" and
    // Notify the Strategies:
    OMCWS::template StopNow<FullStop>(a_where, a_ts_recv);

    LOG_INFO(1,
      "EConnector_H2WS_OMC::StopNow({}): Connector STOPPED{}", a_where,
      FullStop ? "." : ", but will re-start")

    // Final integrity check:
    CHECK_ONLY(CheckH2ConnsIntegrity("StopNow");)
  }

  //-------------------------------------------------------------------------//
  // "H2ConnStopped":                                                        //
  //-------------------------------------------------------------------------//
  template <typename Derived, typename H2Conn>
  template <bool FullStop>
  inline void EConnector_H2WS_OMC<Derived, H2Conn>::H2ConnStopped
    (H2Conn const* a_h2conn,  char const* a_where)
  {
    assert(a_h2conn != nullptr && a_where != nullptr);

    //-----------------------------------------------------------------------//
    // Find this "a_h2conn" and move it from Active to NotActive list:       //
    //-----------------------------------------------------------------------//
    auto it =
      std::find(m_activeH2Conns.begin(), m_activeH2Conns.end(), a_h2conn);

    if (utxx::likely(it != m_activeH2Conns.end()))
    {
      // Remove it from the Active list:
      m_activeH2Conns.erase(it);

      // Attach it to the NotActive list -- it must not be there yet:
      assert
        (std::find(m_notActvH2Conns.cbegin(), m_notActvH2Conns.cend(), a_h2conn)
         ==   m_notActvH2Conns.cend());
      m_notActvH2Conns.push_back(const_cast<H2Conn*>(a_h2conn));
    }
    else
    {
      // This is strange:
      LOG_WARN(2,
        "EConnector_H2WS_OMC::H2ConnStopped({}): H2Conn={} Not Found on the "
        "Active list", a_where, a_h2conn->m_name)

      // But then, it must be on the NotActive list:
      CHECK_ONLY
      (
        if (utxx::unlikely
           (std::find(m_notActvH2Conns.cbegin(), m_notActvH2Conns.cend(),
                      a_h2conn) == m_notActvH2Conns.cend()))
          LOG_ERROR(1,
            "EConnector_H2WS_OMC::H2ConnStopped({}): H2Conn={} Not Found on "
            "the NotActive list EITHER", a_where, a_h2conn->m_name)
        // XXX: Still, nothing more specific to do in this case
      )
    }
    //-----------------------------------------------------------------------//
    // Continue with reduced number of "H2Conn"s:                            //
    //-----------------------------------------------------------------------//
    // Rotate them to get the curr one right:
    RotateH2Conns();

    // NB: The CurrH2Conn may be NULL if there are no Active H2Conns at all;
    // this is acceptable, as they may come and go...
    LOG_INFO(3,
      "EConnector_H2WS_OMC::H2ConnStopped({}): Stopped, Continuing with {} "
      "Active H2Conn(s)", a_where, m_activeH2Conns.size())

    // Final integrity check:
    CHECK_ONLY(CheckH2ConnsIntegrity("H2ConnStopped");)
  }

  //-------------------------------------------------------------------------//
  // "IsActive":                                                             //
  //-------------------------------------------------------------------------//
  template<typename Derived, typename H2Conn>
  inline bool EConnector_H2WS_OMC<Derived, H2Conn>::IsActive() const
  {
    // XXX: We consider the OMC to be Active if the WS leg is Active, and at
    // least 1 "H2Conn" is Active:
    if (!OMCWS::IsActive())
      return false;

    // To be on a safe side, check Active and NotActive "H2Conn"s for their
    // real status, moving them between the lists if necessary:
    CHECK_ONLY
    (
      CheckActiveH2Conns   ();
      CheckNotActiveH2Conns();
    )
    // In general, we should have
    //   m_activeH2Conns.empty() == (m_currH2Conn == NULL)
    // but tjis is not formally guaranteed, so:
    //
    if (utxx::unlikely(m_activeH2Conns.empty()))
      return false;   // Certtainly NOT Active;

    // If there are notionally-Active H2Conns, check the curr one:
    if (utxx::unlikely(m_currH2Conn == nullptr))
    {
      // Try to fix this condition by rotation:
      RotateH2Conns();

      // Still no good?
      if (utxx::unlikely(m_currH2Conn == nullptr))
      {
        LOG_WARN(2,
          "EConnector_H2WS_OMC::IsActive: There are {} notionally-Active "
          "H2Conns, but CurrH2Conn=NULL", m_activeH2Conns.size())
        return false;
      }
    }
    // If we got here: OK:
    return true;
  }

  //-------------------------------------------------------------------------//
  // "Stop" (graceful, external, virtual):                                   //
  //-------------------------------------------------------------------------//
  template<typename Derived, typename H2Conn>
  inline void EConnector_H2WS_OMC<Derived, H2Conn>::Stop()
  {
    // This is a "checking point":
    CHECK_ONLY
    (
      CheckActiveH2Conns   ();
      CheckNotActiveH2Conns();
    )
    // Then Stop ALL "H2Conn"s gracefully (FullStop=true), even thouse  which
    // are on the NotActive list (because they may be Starting etc). But thir
    // status does not change immediately, so they are NOT  moved between the
    // lists yet:
    //
    for (H2Conn* h2conn: m_activeH2Conns)
    {
      assert(h2conn != nullptr &&   h2conn->IsActive());
      (void) h2conn->template Stop<true>();
    }

    for (H2Conn* h2conn: m_notActvH2Conns)
    {
      assert(h2conn != nullptr && (!h2conn->IsActive()));
      (void) h2conn->template Stop<true>();
    }

    // Stop the WS leg gracefully (with FullStop) -- again, it does not Stop
    // immediately:
    OMCWS::Stop();
  }

  //=========================================================================//
  // Session Mgmt:                                                           //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "RotateH2Conns":                                                        //
  //-------------------------------------------------------------------------//
  // Used by Derived Connector to rotate "m_currH2Conn" to the next sending
  // socket (wrapping around the end):
  //
  template<typename Derived, typename H2Conn>
  inline void EConnector_H2WS_OMC<Derived, H2Conn>::RotateH2Conns() const
  {
    // Use by Derived OMC to move to the next "H2Conn". We only rotate Active
    // "H2Conn"s:
    int l = int(m_activeH2Conns.size());
    if (utxx::unlikely(l == 0))
    {
      LOG_WARN(2, "EConnector_H2WS_OMC::RotateH2Conns: No Active H2Conns")
      m_currH2Idx  = -1;
      m_currH2Conn = nullptr;
    }
    else
    {
      m_currH2Idx  = (m_currH2Idx + 1) % l;
      m_currH2Conn =  m_activeH2Conns[size_t(m_currH2Idx)];

      // It must indeed be Active:
      assert(m_currH2Conn != nullptr && m_currH2Conn->IsActive());
    }
  }

  //-------------------------------------------------------------------------//
  // "H2LogOnCompleted":                                                     //
  //-------------------------------------------------------------------------//
  template<typename Derived, typename H2Conn>
  inline void EConnector_H2WS_OMC<Derived, H2Conn>::H2LogOnCompleted
    (H2Conn const* a_h2conn)
  {
    assert(a_h2conn != nullptr && a_h2conn->IsActive());

    // This is a "checking point":
    CHECK_ONLY
    (
      CheckActiveH2Conns   ();
      CheckNotActiveH2Conns();
    )
    // Add this "H2Conn" to the Active list (it should NOT be there yet):
    auto cit =
      std::find(m_activeH2Conns.cbegin(), m_activeH2Conns.cend(), a_h2conn);

    if (utxx::likely(cit == m_activeH2Conns.cend()))
    {
      m_activeH2Conns.push_back(const_cast<H2Conn*>(a_h2conn));

      // At the same time, it MUST be on the NotActive list -- remove it from
      // there:
      auto   nit  =
        std::find(m_notActvH2Conns.begin(), m_notActvH2Conns.end(), a_h2conn);
      assert(nit != m_notActvH2Conns.end());

      m_notActvH2Conns.erase(nit);
    }
    else
    {
      // UnExpectedly, it is already on the Active list -- produce a warning:
      LOG_WARN(2,
        "EConnector_H2WS_OMC::H2LogOnCompleted: H2Conn={} is already on the "
        "Active list", a_h2conn->m_name)
    }
    // It is probably a good idea to perform Rotation:
    RotateH2Conns();

    // Check whether we can declare the whole OMC "Active". For that, we just
    // need the WS leg to be Active -- no need to wait for other "H2Conn"s:
    if (OMCWS::IsActive())
      H2WSTurnedActive();

    // Final integrity check:
    CHECK_ONLY(CheckH2ConnsIntegrity("H2LogOnCompleted");)
  }

  //-------------------------------------------------------------------------//
  // "WSLogOnCompleted":                                                     //
  //-------------------------------------------------------------------------//
  template<typename Derived, typename H2Conn>
  inline void EConnector_H2WS_OMC<Derived, H2Conn>::WSLogOnCompleted()
  {
    // IMPORTANT: Notify the TCPWS directly, NOT OMCWS -- otherwise Strategies
    // could be notified prematurely:
    TCPWS::LogOnCompleted();
    assert(TCPWS::IsActive());

    // Check if at lest one H2 is Active as well, in which case the whole OMC
    // becomes Active:
    CHECK_ONLY
    (
      CheckActiveH2Conns   ();
      CheckNotActiveH2Conns();
    )
    if (!(m_activeH2Conns.empty()))
      H2WSTurnedActive();
  }

  //-------------------------------------------------------------------------//
  // "H2WSTurnedActive":                                                     //
  //-------------------------------------------------------------------------//
  template<typename Derived, typename H2Conn>
  inline void EConnector_H2WS_OMC<Derived, H2Conn>::H2WSTurnedActive()
  {
    // HERE we create the PusherTask Timer:
    EConnector_OrdMgmt::template CreatePusherTask<Derived, Derived>
      (static_cast<Derived*>(this), static_cast<Derived*>(this));

    // Further action may be delegated to "Derived": XXX: Use a different method
    // name, otherwise infinite recursion may occur if the method is not implem-
    // ented there!
    static_cast<Derived*>(this)->OnTurningActive();

    // To be on a safe side, perform rotation of "H2Conn"s in this Session:
    RotateH2Conns();

    // Notify the Strategies:
    utxx::time_val now = utxx::now_utc();
    this->EConnector::ProcessStartStop(true, nullptr, 0, now, now);

    LOG_INFO(1,
      "EConnector_H2WS_OMC::H2WSTurnedActive: Connector is now ACTIVE")
  }

  //-------------------------------------------------------------------------//
  // "CheckActiveH2Conns":                                                   //
  //-------------------------------------------------------------------------//
  template<typename Derived, typename H2Conn>
  inline void EConnector_H2WS_OMC<Derived, H2Conn>::CheckActiveH2Conns()
  const
  {
    // "H2Conn"s on the Active list must really be Active:
    //
    for (auto it = m_activeH2Conns.begin(); it != m_activeH2Conns.end(); )
    {
      H2Conn* h2conn  = *it;
      assert (h2conn != nullptr);

      if (utxx::unlikely(!(h2conn->IsActive())))
      {
        // Found one which is NOT Active. Log a warning and move it into the
        // NotActive list:
        LOG_WARN(2,
          "EConnector_H2WS_OMC::CheckActiveH2Conns: H2Conn={} marked Active "
          "but is not really Active", h2conn->m_name)

        // XXX: Be careful: "it" and its successors will be invalidated when
        // erased from "m_activeH2Conns":
        if (it != m_activeH2Conns.begin())
        {
          auto prev = it - 1;
          m_activeH2Conns.erase(it);
          it = prev + 1;
        }
        else
        {
          m_activeH2Conns.erase(it);
          it = m_activeH2Conns.begin();
        }
        // Add this H2Conn to "m_notActvH2Conns". It must NOT be there yet:
        assert
          (std::find(m_notActvH2Conns.cbegin(), m_notActvH2Conns.cend(), h2conn)
          ==         m_notActvH2Conns.cend());
        m_notActvH2Conns.push_back   (h2conn);
      }
      else
        // Just go ahead:
        ++it;
    }
    // Final integrity check:
    CheckH2ConnsIntegrity("CheckActiveH2Conns");
  }

  //-------------------------------------------------------------------------//
  // "CheckNotActiveH2Conns":                                                //
  //-------------------------------------------------------------------------//
  template<typename Derived, typename H2Conn>
  inline void EConnector_H2WS_OMC<Derived, H2Conn>::CheckNotActiveH2Conns()
  const
  {
    // "H2Conn"s on the NotActive list must NOT be Active:
    //
    for (auto it = m_notActvH2Conns.begin(); it != m_notActvH2Conns.end(); )
    {
      H2Conn* h2conn  = *it;
      assert (h2conn != nullptr);

      if (utxx::unlikely(h2conn->IsActive()))
      {
        // Found one which is Active. Log a warning and move it into the Active
        // list:
        LOG_WARN(2,
          "EConnector_H2WS_OMC::CheckNotActiveConns: Found an Active H2Conn={}",
          h2conn->m_name)

        // XXX: Be careful: "it" and its successors will be invalidated when
        // erased from "m_notActvH2Conns":
        if (it != m_notActvH2Conns.begin())
        {
          auto prev = it - 1;
          m_notActvH2Conns.erase(it);
          it = prev + 1;
        }
        else
        {
          m_notActvH2Conns.erase(it);
          it = m_notActvH2Conns.begin();
        }
        // Add this H2Conn to "m_activeH2Conns". It must NOT be there yet:
        assert
          (std::find(m_activeH2Conns.cbegin(), m_activeH2Conns.cend(), h2conn)
          ==         m_activeH2Conns.cend());
        m_activeH2Conns.push_back   (h2conn);
      }
      else
        // Just go ahead:
        ++it;
    }
    // Final integrity check:
    CheckH2ConnsIntegrity("CheckActiveH2Conns");
  }

  //-------------------------------------------------------------------------//
  // "MoveH2ConnToActive":                                                   //
  //-------------------------------------------------------------------------//
  // NB: For external use only (NOT while iterating over H2Conn lists!):
  //
  template<typename Derived, typename H2Conn>
  inline void EConnector_H2WS_OMC<Derived, H2Conn>::MoveH2ConnToActive
    (H2Conn* a_h2conn)
  const
  {
    // The H2Conn must be Active:
    assert(a_h2conn != nullptr && a_h2conn->IsActive());

    // It must currently be on the NotActive list, and NOT be on the Active
    // list:
    auto itN =
      std::find(m_notActvH2Conns.begin(), m_notActvH2Conns.end(), a_h2conn);
    CHECK_ONLY
    (
      if (utxx::unlikely(itN == m_notActvH2Conns.end()))
        throw utxx::badarg_error
              ("EConnector_H2WS_OMC: MoveH2ConnToActive: Not Found in "
               "NotActv: ", a_h2conn->m_name);

      if (utxx::unlikely
         (std::find(m_activeH2Conns.cbegin(), m_activeH2Conns.cend(), a_h2conn)
          != m_activeH2Conns.cend()))
        throw utxx::badarg_error
              ("EConnector_H2WS_OMC: MoveH2ConnToActive: Already in Active: ",
               a_h2conn->m_name);
    )
    // If OK: Remove it from NotActive, add to Active:
    m_notActvH2Conns.erase   (itN);
    m_activeH2Conns.push_back(a_h2conn);

    // Final integrity check:
    CHECK_ONLY(CheckH2ConnsIntegrity("MoveH2ConnToActive");)
  }

  //-------------------------------------------------------------------------//
  // "MoveH2ConnToNotActive":                                                //
  //-------------------------------------------------------------------------//
  // NB: For external use only (NOT while iterating over H2Conn lists!):
  //
  template<typename Derived, typename H2Conn>
  inline void EConnector_H2WS_OMC<Derived, H2Conn>::MoveH2ConnToNotActive
    (H2Conn* a_h2conn)
  const
  {
    // The H2Conn must NOT be Active:
    assert(a_h2conn != nullptr && !(a_h2conn->IsActive()));

    // It must be on the Active list (as yet), and NOT be the NotActv list:
    auto itA =
      std::find(m_activeH2Conns.begin(), m_activeH2Conns.end(), a_h2conn);
    CHECK_ONLY
    (
      if (utxx::unlikely(itA == m_activeH2Conns.end()))
        throw utxx::badarg_error
              ("EConnector_H2WS_OMC: MoveH2ConnToNotActive: Not Found in "
               "Active: ", a_h2conn->m_name);

      if (utxx::unlikely
         (std::find(m_notActvH2Conns.cbegin(), m_notActvH2Conns.cend(),
                    a_h2conn) != m_notActvH2Conns.cend()))
        throw utxx::badarg_error
              ("EConnector_H2WS_OMC: MoveH2ConnToNotActive: Already in "
               "NotActv: ", a_h2conn->m_name);
    )
    // If OK: Remove it from Active, add to NotActv:
    m_activeH2Conns.erase     (itA);
    m_notActvH2Conns.push_back(a_h2conn);

    // XXX: If "a_h2conn" was actually "m_currH2Conn", do NOT rotate it yet, as
    // the CallER may still want to do something with the original ptr!

    // Final integrity check:
    CHECK_ONLY(CheckH2ConnsIntegrity("MoveH2ConnToNotActive");)
  }

  //-------------------------------------------------------------------------//
  // "CheckH2ConnsIntegrity":                                                //
  //-------------------------------------------------------------------------//
  // Integrity check of "m_activeH2Conns" and "m_notActvH2Conns":
  //
  template<typename Derived, typename H2Conn>
  void EConnector_H2WS_OMC<Derived, H2Conn>::CheckH2ConnsIntegrity
    (char const* a_where)
  const
  {
    assert(a_where != nullptr);

    for (int a = 0; a < 2; ++a)
    {
      auto const& currLst  = a ? m_activeH2Conns  : m_notActvH2Conns;
      auto const& othrLst  = a ? m_notActvH2Conns : m_activeH2Conns;

      char const* currName = a ? "ActiveH2Conns"  : "NotActvH2Conns";
      char const* othrName = a ? "NotActvH2Conns" : "ActiveH2Conns";

      for (auto cit = currLst.cbegin(); cit != currLst.cend(); ++cit)
      {
        H2Conn const* h2conn = *cit;
        assert(h2conn != nullptr);

        // First, all H2Conns on "currList" must be unique:
        auto nit = std::find(cit + 1,  currLst.end(), h2conn);
        if (utxx::unlikely  (nit !=    currLst.end()))
          throw utxx::logic_error
                ("EConnector_H2WS_OMC::CheckH2ConnsIntegrity(", a_where,
                 "): Duplication in ", currName, ": ", h2conn->m_name);

        // Second, all H2Conns on "currLst" must not be on "othrList":
        auto oit = std::find(othrLst.cbegin(), othrLst.cend(), h2conn);
        if (utxx::unlikely  (oit !=   othrLst.cend()))
          throw utxx::logic_error
                ("EConnector_H2WS_OMC::CheckH2ConnsIntegrity(", a_where,
                 "): H2Conn=", h2conn->m_name, " from ", currName,
                 " is also found on ", othrName);
      }
      // And finally, check the total number of H2Conns:
      if (utxx::unlikely(currLst.size() + othrLst.size() != m_numH2Conns))
        throw utxx::logic_error
              ("EConnector_H2WS_OMC::CheckH2ConnsIntegrity(",   a_where,
               "): Found ", currLst.size() + othrLst.size(),
               " H2Conns, must have ",     m_numH2Conns);
    }
  }
} // End namespace MAQUETTE
