// vim:ts=2:et
//===========================================================================//
//                   "Acceptors/FIX_AcceptorSessMgr.h":                      //
//                     FIX Acceptor-Side Session Mgmt                        //
//===========================================================================//
// Parameterised by the FIX Protocol Dialect and the "Processor" (which performs
// Processing of Appl-Level Msgs).
// Features not supported yet (TODO):
// (*) Throttling of client requests;
// (*) Checking latencies of msgs submitted by clientsl
// (*) SeqNums gap mgmt in the moddle of a Session -- with TCP,  it is probbaly
//     not necessary; currently, onlu initial gaps (at LogOn) are managed, a la
//     TWIME:
//
#pragma once

#include "Acceptors/TCP_Acceptor.h"
#include "Protocols/FIX/Msgs.h"
#include "Protocols/FIX/SessData.h"
#include "Protocols/FIX/ProtoEngine.h"

namespace MAQUETTE
{
  //=========================================================================//
  // "FIX_AcceptorSessMgr" Class:                                            //
  //=========================================================================//
  template<FIX::DialectT::type D, typename Processor>
  class    FIX_AcceptorSessMgr:
    public TCP_Acceptor<FIX_AcceptorSessMgr<D, Processor>>
  {
  public:
    //=======================================================================//
    // Session Struct:                                                       //
    //=======================================================================//

  protected:
    using TCPA = TCP_Acceptor<FIX_AcceptorSessMgr<D, Processor>>;
    //=======================================================================//
    // Data Flds (FIX-specific):                                             //
    //=======================================================================//
    // FIX Protocol Session-Level Features:
    bool    const                  m_resetSeqNumsOnLogOn;

    //=======================================================================//
    // Non-Default Ctor, Dtor:                                               //
    //=======================================================================//
  };

} // End namespace MAQUETTE
