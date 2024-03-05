// vim:ts=2:et
//===========================================================================//
//                           "Basis/BaseTypes.cpp":                          //
//                                Empty Consts                               //
//===========================================================================//
#include "Basis/BaseTypes.hpp"

namespace MAQUETTE
{
  //-------------------------------------------------------------------------//
  // Consts declared in "BaseTypes.hpp":                                     //
  //-------------------------------------------------------------------------//
  Ccy        const EmptyCcy        (MkCcy       (nullptr));
  SymKey     const EmptySymKey     (MkSymKey    (nullptr));
  Credential const EmptyCredential (MkCredential(nullptr));
  ObjName    const EmptyObjName    (MkObjName   (nullptr));
  ErrTxt     const EmptyErrTxt     (MkErrTxt    (nullptr));
  ExchOrdID  const EmptyExchOrdID; // Default Ctor -- all 0s
}
