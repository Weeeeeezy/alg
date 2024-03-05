/ vim:ts=2:et
//===========================================================================//
//                   "Protocols/ITCH/Msgs_FastMatch.hpp":                    //
//                 ITCH Msg Types and Parsers for FastMatch                  //
//===========================================================================//
// XXX: Currently, this Protocol Engine supports the Connector (Client) Side
// only:
#pragma once

#include "Basis/BaseTypes.hpp"
#include "Protocols/ITCH/Features.h"
#include <utxx/config.h>
#include <utxx/enumv.hpp>
#include <utxx/error.hpp>
#include <spdlog/spdlog.h>
#include <map>

namespace MAQUETTE
{
namespace ITCH
{
  namespace FastMatch
  {
    //=======================================================================//
    // Msg Prefix:                                                           //
    //=======================================================================//
    // (Not part of the Msg Types below):
    // UDP: { uint32_t SeqNum, uint16_t PktLen, uint8_t MsgType }
    // TCP: {                  uint16_t PktLen, uint8_t MsgType }
    //=======================================================================//
    // FastMatch ITCH Msg Type Tags:                                         //
    //=======================================================================//
    // NB: This includes both "top-level" and "payload" types:
    //
    UTXX_ENUMV(MsgT, (char, '\0'),
      // Top-Level Msg Types:
      // Session-Level:
      (LogInReq,            'L')
      (LogInAccepted,       'A')
      (LogInRejected,       'J')
      (ServerHeartBeat,     'H')
      (EndOfSession,        'Z')
      (ClientHeartBeat,     'R')
      (LogOutReq,           'O')
      // Application-Level:
      (SequencedData,       'S')
      (UnSequencedData,     'U')
      // Payload Msg Types (encapsulated in "SequencedData"):
      (BookUpdate,          'B')
      (PriceAdd,            'P')  // Inside "BookUpdate"
      (PriceCancel,         'C')  // Inside "BookUpdate"
      (TradeUpdate,         'T')  // Separate
      (MidPointUpdate,      'M')  // Separate
      (MktOnCloseUpdate,    'O')  // Separate (XXX: Same tag as for LogOutReq)
      (MktDataSubscrReq,    'S')  //   XXX: Same tag as for SequencedData
      (MktDataSubscrResp,   'R')  //   XXX: Same tag as for ClientHeartBeat
      (InstrsDirReq,        'N')
      (InstrInfo,           'F')
      (Reject,              'J')  //   XXX: Same tag as for LogInRejected
    );

    //=======================================================================//
    // Error Codes:                                                          //
    //=======================================================================//
    UTXX_ENUMV(ErrT, (char, '\0'),
      // Business Error Codes:
      (OK,                  '0')
      (InvalidMsgType,      '1')
      (InvalidMsgLen,       '2')
      (InvalidInstr,        '3')
      (UnAuthorisedDest,    '4')
      (OtherError,          'Z')
      // Subscription Reject Reason Codes:
      (NotAuthorised,       'A')
      (UnSupportedInstr,    'S')
      (UnSupportedVerNum,   'V')
      (DuplicateReqID,      'D')
      (SubscrAlreadyActive, 'P')
      (ExchangeNotOpen,     'E')
      (InvalidActionType,   'F')
      (InvalidSubscrType,   'G')
      (InvalidDepth,        'H')
      (UnAuthorisedSess,    'j')
    );

    //=======================================================================//
    // Session-Level Msgs:                                                   //
    //=======================================================================//
    //=======================================================================//
    // "LogInReq":                                                           //
    //=======================================================================//
  }
  // End namespace FastMatch

  //=========================================================================//
  // "ReaderParserHelper": Partial Specialisation for FastMatch:             //
  //=========================================================================//
  template<typename Processor>
  struct ReadertParserHelper<DialectT::FastMatch, Processor>
  {
    //=======================================================================//
    // "ReadHandler":                                                        //
    //=======================================================================//
    template<bool IsMultiCast>
    static int ReadHandler
    (
      Processor*      a_proc,     // Typically EConnector_{UDP|TCP}_ITCH
      int             DEBUG_ONLY(a_fd),
      char const*     a_buff,     // Points to {stat|dyn} buffer of Reactor
      int             a_size,     // Size of the chunk
      utxx::time_val  a_ts_recv
    )
    {
      using namespace FastMatch;
      assert(a_fd >= 0         && a_buff != nullptr && a_size > 0 &&
             a_proc != nullptr && a_fd   == a_proc->m_fd);

      //---------------------------------------------------------------------//
      // Get the ITCH Msgs from the Byte Stream:                             //
      //---------------------------------------------------------------------//
      char const* msgBegin     = a_buff;
      char const* chunkEnd     = a_buff + a_size;
      assert(msgBegin < chunkEnd);
      do
      {
      }
    }
  };
} // End namespace ITCH
} // End namespace MAQUETTE
