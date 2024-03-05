// vim:ts=2:et
//===========================================================================//
//                            "Protocols/P2Cgate/VM.h":                      //
//===========================================================================//
#pragma once
#include <cgate.h>
#pragma pack(push, 4)

namespace MAQUETTE
{
namespace P2CGate
{
namespace VM
{
  struct fut_vm
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    signed int isin_id; // i4
    signed int sess_id; // i4
    char client_code[8]; // c7
    char vm[11]; // d16.5
    char vm_real[11]; // d16.5
  };

  constexpr size_t sizeof_fut_vm = 64;
  constexpr size_t fut_vm_index = 0;


  struct opt_vm
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    signed int isin_id; // i4
    signed int sess_id; // i4
    char client_code[8]; // c7
    char vm[11]; // d16.5
    char vm_real[11]; // d16.5
  };

  constexpr size_t sizeof_opt_vm = 64;
  constexpr size_t opt_vm_index = 1;


  struct fut_vm_sa
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    signed int isin_id; // i4
    signed int sess_id; // i4
    char settlement_account[13]; // c12
    char vm[15]; // d26.2
    char vm_real[15]; // d26.2
  };

  constexpr size_t sizeof_fut_vm_sa = 76;
  constexpr size_t fut_vm_sa_index = 2;


  struct opt_vm_sa
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    signed int isin_id; // i4
    signed int sess_id; // i4
    char settlement_account[13]; // c12
    char vm[15]; // d26.2
    char vm_real[15]; // d26.2
  };

  constexpr size_t sizeof_opt_vm_sa = 76;
  constexpr size_t opt_vm_sa_index = 3;

} // End namespace VM
} // End namaepace P2CGate
} // End namespace MAQUETTE
#pragma pack(pop)
