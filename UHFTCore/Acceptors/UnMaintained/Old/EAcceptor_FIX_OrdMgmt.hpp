// vim:ts=2:et
//===========================================================================//
//                         "EAcceptor_FIX_OrdMgmt.hpp":                      //
//                 Template aliases for "EAcceptor_FIX_OrdMgmt":             //
//===========================================================================//
# ifdef  __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wunused-parameter"
# endif
# ifdef  __GNUC__
# pragma GCC   diagnostic push
# pragma GCC   diagnostic ignored "-Wunused-parameter"
# endif

#include "FixEngine/Acceptor/EAcceptor_FIX_OrdMgmt.h"

// Implementations for "EAcceptor_FIX_OrdMgmt":
#ifndef  MQT_FIX_SERVER_ORDMGMT
#define  MQT_FIX_SERVER_ORDMGMT
#ifndef  MQT_FIX_SERVER
#define  MQT_FIX_SERVER
#endif
#endif

#include "FixEngine/Acceptor/InitFini.hpp"
#include "FixEngine/Acceptor/UtilsMacros.hpp"
#include "FixEngine/Acceptor/ReaderParsers.hpp"
#include "FixEngine/Acceptor/Processors1.hpp"
#include "FixEngine/Acceptor/Senders1.hpp"

// Implementations for "EAcceptor_FIX_OrdMgmt" proper:
#include "FixEngine/Acceptor/Senders2.hpp"
#include "FixEngine/Acceptor/Processors2.hpp"

// Protocol Dialects:
#include "FixEngine/Acceptor/Features_AlfaFIX2.h"
#include "FixEngine/Acceptor/Features_FXBA.h"
#include "FixEngine/Acceptor/Features_MICEX.h"

namespace AlfaRobot
{
  template<typename Conn>
  using  EAcceptor_FIX_OrdMgmt_AlfaFIX2 =
         EAcceptor_FIX_OrdMgmt<FIX::DialectT::AlfaFIX2, Conn>;

  template<typename Conn>
  using  EAcceptor_FIX_OrdMgmt_FXBA     =
         EAcceptor_FIX_OrdMgmt<FIX::DialectT::FXBA,     Conn>;

  template<typename Conn>
  using  EAcceptor_FIX_OrdMgmt_MICEX    =
         EAcceptor_FIX_OrdMgmt<FIX::DialectT::MICEX,    Conn>;
}
// End namespace AlfaRobot

# ifdef  __clang__
# pragma clang diagnostic pop
# endif
# ifdef  __GNUC__
# pragma GCC   diagnostic pop
# endif

// TODO YYY
#undef MQT_FIX_SERVER_ORDMGMT
