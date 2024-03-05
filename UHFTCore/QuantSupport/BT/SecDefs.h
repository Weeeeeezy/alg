// vim:ts=2:et
//===========================================================================//
//                    "QuantSupport/BT/SecDefs.h":                           //
//===========================================================================//

#pragma once

#include "Basis/SecDefs.h"


namespace MAQUETTE
{

  namespace History
  {
    /**
     * Return SecDefs vector by market name
     */
    std::vector<SecDefS> const& SecDefs(std::string const& market);
  }

} // namespace MAQUETTE
