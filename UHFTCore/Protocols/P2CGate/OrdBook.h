// vim:ts=2:et
//===========================================================================//
//                       "Protocols/P2CGate/OrdBook.h":                      //
//===========================================================================//
#pragma once
#include "Basis/BaseTypes.hpp"
#include <cgate.h>
#pragma pack(push, 4)

namespace MAQUETTE
{
namespace P2CGate
{
namespace OrdBook
{
  struct orders
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    signed long long id_ord; // i8
    signed int sess_id; // i4
    struct cg_time_t moment; // t
    signed long long xstatus; // i8
    signed int status; // i4
    signed char action; // i1
    signed int isin_id; // i4
    signed char dir; // i1
    char price[11]; // d16.5
    signed int amount; // i4
    signed int amount_rest; // i4
    struct cg_time_t init_moment; // t
    signed int init_amount; // i4

    //-----------------------------------------------------------------------//
    // Static API required by "EConnector_MktData::UpdateOrderBooks":        //
    //-----------------------------------------------------------------------//
    // MDE is this struct itself, so there is only 1 such entry:
    using MDEntry = orders;
    int               GetNEntries()  const { return 1;     }
    MDEntry const&    GetEntry(int)  const { return *this; }

    // "GetEntryType": Actually the Side:
    FIX::MDEntryTypeT GetEntryType() const
    {
      return ((dir == 1)
              ? FIX::MDEntryTypeT::Bid
              : FIX::MDEntryTypeT::Offer);
    }

    // Symbol is not provided here -- only the SecID is:
    constexpr static char const*     GetSymbol(MDEntry const&) { return ""; }

    // There are no RptSeqs here:
    constexpr static SeqNum          GetRptSeq(MDEntry const&) { return 0;  }

    // UTC TimeStamp is to be constructed from components:
    constexpr static utxx::time_val  GetEventTS(MDEntry const&)
      { return utxx::time_val(); }
  };

  constexpr size_t sizeof_orders = 104;
  constexpr size_t orders_index = 0;


  struct info
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    signed long long infoID; // i8
    signed long long logRev; // i8
    signed int lifeNum; // i4
    struct cg_time_t moment; // t
  };

  constexpr size_t sizeof_info = 56;
  constexpr size_t info_index = 1;

} // End namespace OrdBook
} // End namaepace P2CGate
} // End namespace MAQUETTE
#pragma pack(pop)
