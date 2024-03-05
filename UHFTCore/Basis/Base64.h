// vim:ts=2
//===========================================================================//
//                             "Basis/Base64.h":                             //
//             "UnChecked" Base64 Functions: No Memory Allocation            //
//===========================================================================//
#pragma  once
#include <cstdint>

namespace MAQUETTE
{
  //=========================================================================//
  // "Base64EnCodeUnChecked":                                                //
  //=========================================================================//
  // NB:
  // (*) "a_dst"     : the Caller is responsible for sufficient Dst capacity!
  // (*) "a_eq_trail": pad with '='(s);
  // (*) "a_url_mode": slightly different encoding as opposed to StdMode:
  // Returns the actual number of bytes stored in "a_dst":
  //
  int Base64EnCodeUnChecked
  (
    uint8_t const* a_src,
    int            a_len,
    char*          a_dst,  // The Caller is responsible for sufficient capacity!
    bool           a_eq_trail = true,
    bool           a_url_mode = false
  );

  //=========================================================================//
  // "Base64DecodeUnChecked":                                                //
  //=========================================================================//
  // Similar to "Base64EnCodeUnChecked" above:
  //
  int Base64DeCodeUnChecked
  (
    char const*    a_src,
    int            a_len,
    uint8_t*       a_dst,
    bool           a_url_mode = false
  );
} // End namespace MAQUETTE
