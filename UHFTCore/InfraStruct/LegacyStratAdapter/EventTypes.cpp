// vim:ts=2:et:sw=2
//===========================================================================//
//                              "EventTypes.cpp":                            //
//                     Provides Zeroed-Out Fixed Strings:                    //
//===========================================================================//
#include "EventTypes.h"
#include <utxx/compiler_hints.hpp>
#include <cassert>

namespace
{
  using namespace Events;

  //-------------------------------------------------------------------------//
  // Flag for On-Demand Initialisation (on first call of either function:    //
  //-------------------------------------------------------------------------//
  bool NotInited = true;

  //-------------------------------------------------------------------------//
  // Empty Fixed Strings:                                                    //
  //-------------------------------------------------------------------------//
  ObjName emptyObjName;
  ErrTxt  emptyErrTxt;

  //-------------------------------------------------------------------------//
  // Initialiser:                                                            //
  //-------------------------------------------------------------------------//
  inline void ZeroOutConsts()
  {
    assert(NotInited);

    emptyObjName.fill('\0');
    emptyErrTxt.fill ('\0');

    NotInited = false;
  }
}

namespace Events
{
  //-------------------------------------------------------------------------//
  // Accessors:                                                              //
  //-------------------------------------------------------------------------//
  ObjName const& EmptyObjName()
  {
    if (utxx::unlikely(NotInited))
      ZeroOutConsts();
    return emptyObjName;
  }

  ErrTxt const&  EmptyErrTxt()
  {
    if (utxx::unlikely(NotInited))
      ZeroOutConsts();
    return emptyErrTxt;
  }
}
