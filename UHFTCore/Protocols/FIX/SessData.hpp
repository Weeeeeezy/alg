// vim:ts=2:et
//===========================================================================//
//                        "Protocols/FIX/SessData.hpp":                      //
//         FIX Protocol Engine: Session-Specific Data Representation         //
//===========================================================================//
#pragma once

#include "Basis/BaseTypes.hpp"
#include "Protocols/FIX/Msgs.h"
#include <boost/core/noncopyable.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <cstring>

namespace MAQUETTE
{
namespace FIX
{
  //=========================================================================//
  // "SessData" Struct:                                                      //
  //=========================================================================//
  // Configuration and run-time data for a FIX Session:
  // (*) Applicable to both Client and Server Sessions;
  // (*) Used by both FIX Protocol and FIX Session Mgmt Layers;
  // (*) Does NOT include any Connector/Acceptor-specific data (eg IPs/Ports),
  //     as the FIX Session is created AFTER a TCP connection is established.
  // (*) Session Types (OrdMgmt/MktData or Server/Client) are NOT memoised
  //     here; rather, they are configured STATICALLY in FIX Protocol, FIX
  //     Session Manager and FIX Connectors.
  // The SessData objs are owned and managed by the SessMgr (or a SessMgr may
  // actually derive from SessData!), and are only exposed to/updated by  the
  // ProtocolLayer:
  //
  struct SessData
  {
    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    // NB: Below, we use the terms "Our" for *this* side of the protocol (which
    // runs the curr instance of this software),  and "Contr"  for the opposite
    // side. Either side could be the Initiator (Client, Connector) and the Ser-
    // ver (Acceptor). And depending on the curr msg direction, either side can
    // be the Sender and the Target (Receiver):
    //-----------------------------------------------------------------------//
    // Consts:                                                               //
    //-----------------------------------------------------------------------//
    // CompIDs, SubIDs of both Sides:
    Credential              m_ourCompID;      // 49
    Credential              m_ourSubID;       // 50
    Credential              m_contrCompID;    // 56
    Credential              m_contrSubID;     // 57
    // The followng credentials typically apply to the Client side, but not al-
    // ways. In any case, in a FIX Session, they are used by one side only:
    Credential              m_userName;       // 553
    Credential              m_passwd;         // 554
    Credential              m_newPasswd;      // 925
    Credential              m_account;        // 1
    Credential              m_clientID;       // For FIX <= 4.2 this is tag 109,
                                              // for FIX >= 4.3 this is tag 115,
                                              // which is OnBehalfOfCompID
    Credential              m_partyID;        // 448
    char                    m_partyIDSrc;     // 447  (only if PartyID is set)
    int                     m_partyRole;      // 452
    // Some Temporal Params:
    int                     m_heartBeatSec;   // 108
    int                     m_maxGapMSec;

    //-----------------------------------------------------------------------//
    // Dynamic Data:                                                         //
    //-----------------------------------------------------------------------//
    // NB: TxSN, RxSN are typically made persistent by the Session or Applicatn
    // Layer. In order to avoid duplication and possible inconsistencies  betwn
    // those Layers and the Protocol Layer, we do NOT keep separate  copies of
    // these counters in "SessData"; rather, provide ptrs to their external lo-
    // cations:
    SeqNum*                 m_txSN;           // For 34
    SeqNum*                 m_rxSN;
    // However, if external ptrs are NULL, local SNs will be used:
    SeqNum                  m_lTxSN;
    SeqNum                  m_lRxSN;

    mutable utxx::time_val  m_testReqTS;      // When TestReq   was sent
    mutable utxx::time_val  m_resendReqTS;    // When ResendReq was sent
    mutable OrderID         m_userReqID;      // Last user request id (923)

    //=======================================================================//
    // Methods:                                                              //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // Default Ctor: The resulting object is invalid:                        //
    //-----------------------------------------------------------------------//
    SessData()
    : m_ourCompID       (EmptyCredential),
      m_ourSubID        (EmptyCredential),
      m_contrCompID     (EmptyCredential),
      m_contrSubID      (EmptyCredential),
      m_userName        (EmptyCredential),
      m_passwd          (EmptyCredential),
      m_newPasswd       (EmptyCredential),
      m_account         (EmptyCredential),
      m_clientID        (EmptyCredential),
      m_partyID         (EmptyCredential),
      m_partyIDSrc      ('\0'),
      m_partyRole       (0),
      m_heartBeatSec    (0),
      m_maxGapMSec      (0),
      m_txSN            (nullptr),
      m_rxSN            (nullptr),
      m_lTxSN           (0),
      m_lRxSN           (0),
      // All other flds are not known from static configs -- set them to their
      // dynamic init vals:
      m_testReqTS       (),
      m_resendReqTS     (),
      m_userReqID       (0)
    {}

    //-----------------------------------------------------------------------//
    // Non-Default Ctor:                                                     //
    //-----------------------------------------------------------------------//
    // NB: The order of Ctor params is DIFFERENT from that of "SessData" flds --
    // this is to facilitate the use of default param vals:
    SessData
    (
      // The following params are always required:
      std::string const& a_our_comp_id,
      std::string const& a_contr_comp_id,
      SeqNum*            a_tx_sn,                      // NULL OK for MktData
      SeqNum*            a_rx_sn,                      //
      // Reasonable defaults for others:
      int                a_heart_beat_sec     = 30,    // 30 sec
      int                a_max_gap_msec       = 1000,  //  1 sec
      std::string const& a_our_sub_id         = std::string(),
      std::string const& a_contr_sub_id       = std::string(),
      std::string const& a_user_name          = std::string(),
      std::string const& a_passwd             = std::string(),
      std::string const& a_new_passwd         = std::string(),
      std::string const& a_account            = std::string(),
      std::string const& a_client_id          = std::string(),
      std::string const& a_party_id           = std::string(),
      char               a_party_id_src       = '\0',
      int                a_party_role         = 0
    )
    : m_ourCompID       (MkCredential(a_our_comp_id)),
      m_ourSubID        (MkCredential(a_our_sub_id)),
      m_contrCompID     (MkCredential(a_contr_comp_id)),
      m_contrSubID      (MkCredential(a_contr_sub_id)),
      m_userName        (MkCredential(a_user_name)),
      m_passwd          (MkCredential(a_passwd)),
      m_newPasswd       (MkCredential(a_new_passwd)),
      m_account         (MkCredential(a_account)),
      m_clientID        (MkCredential(a_client_id)),
      m_partyID         (MkCredential(a_party_id)),
      m_partyIDSrc      (a_party_id_src),
      m_partyRole       (a_party_role),
      m_heartBeatSec    (a_heart_beat_sec),
      m_maxGapMSec      (a_max_gap_msec),
      m_txSN            ((a_tx_sn != nullptr) ? a_tx_sn : &m_lTxSN),
      m_rxSN            ((a_rx_sn != nullptr) ? a_rx_sn : &m_lRxSN),
      m_lTxSN           (1), // Initialise them, though they may remain unused
      m_lRxSN           (1), //
      // All other flds are not known from static configs -- set them to their
      // dynamic init vals:
      m_testReqTS       (),
      m_resendReqTS     (),
      m_userReqID       (0)
    {
      // Verify the Data. In particular, TxSN and RxSN counters (external or
      // internal) must be properly initialised:
      if (utxx::unlikely
         (IsEmpty(m_ourCompID) || IsEmpty(m_contrCompID) || *m_txSN <= 0 ||
          *m_rxSN <= 0         || m_maxGapMSec <= 0))
        throw utxx::badarg_error("FIX::SessData::Ctor: Invalid arg(s)");
    }

    // Dtor is Auto-Generated...
  };
} // End namespace FIX
} // End namespace MAQUETTE
