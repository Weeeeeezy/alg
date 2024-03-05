// vim:ts=2:et
//===========================================================================//
//                    "Protocols/H2WS/LATOKEN-OMC.cpp":                      //
//                    Explicit Instances of "HandlerOMC"                     //
//===========================================================================//
#include "Protocols/H2WS/LATOKEN-OMC.hpp"

namespace MAQUETTE
{
namespace H2WS
{
namespace LATOKEN
{
  template class HandlerOMC<false>;
  template class HandlerOMC<true>;

} // End namespace LATOKEN
} // End namespace WS
} // End namespace MAQUETTE
