// vim:ts=2:et
//===========================================================================//
//                      "Protocols/P2CGate/OptOrderBook.h":                  //
//===========================================================================//
#pragma once
#include <cgate.h>
#pragma pack(push, 4)

namespace MAQUETTE
{
namespace P2CGate
{
namespace OptOrderBook
{
  struct orders
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    signed long long id_ord; // i8
    signed int sess_id; // i4
    char client_code[8]; // c7
    struct cg_time_t moment; // t
    signed long long xstatus; // i8
    signed int status; // i4
    signed char action; // i1
    signed int isin_id; // i4
    signed char dir; // i1
    char price[11]; // d16.5
    signed int amount; // i4
    signed int amount_rest; // i4
    char comment[21]; // c20
    signed char hedge; // i1
    signed char trust; // i1
    signed int ext_id; // i4
    char login_from[21]; // c20
    char broker_to[8]; // c7
    char broker_to_rts[8]; // c7
    struct cg_time_t date_exp; // t
    signed long long id_ord1; // i8
    char broker_from_rts[8]; // c7
    struct cg_time_t init_moment; // t
    signed int init_amount; // i4
  };

  constexpr size_t sizeof_orders = 204;
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

} // End namespace OptOrderBook
} // End namaepace P2CGate
} // End namespace MAQUETTE
#pragma pack(pop)
