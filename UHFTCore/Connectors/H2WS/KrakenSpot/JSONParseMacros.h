#pragma once
#include "utxx/error.hpp"
//===========================================================================//
// JSON Parsing Macros:                                                      //
//===========================================================================//
//---------------------------------------------------------------------------//
// "GET_VAL": Skip a fields name:                                            //
//---------------------------------------------------------------------------//
#ifdef GET_VAL
#undef GET_VAL
#endif
#define GET_VAL()                                                              \
  {                                                                            \
    curr = strchr(curr, ':');                                                  \
    if (utxx::unlikely(curr == nullptr))                                       \
      UTXX_THROW_RUNTIME_ERROR("No field value!");                             \
    ++curr;                                                                    \
    while (*curr == ' ')                                                       \
      ++curr;                                                                  \
    if (*curr == '\'')                                                         \
      ++curr;                                                                  \
    if (*curr == '\"')                                                         \
      ++curr;                                                                  \
  }

//---------------------------------------------------------------------------//
// "SKP_FLD": Skip a fixed num of fields:                                   //
//---------------------------------------------------------------------------//
#ifdef SKP_FLD
#undef SKP_FLD
#endif
#define SKP_FLD(N)                                                             \
  {                                                                            \
    for (unsigned i = 0; i < N; ++i) {                                         \
      curr = strchr(curr, ',');                                                \
      if (utxx::unlikely(curr == nullptr))                                     \
        UTXX_THROW_RUNTIME_ERROR("Out of bounds!");                            \
      ++curr;                                                                  \
    }                                                                          \
  }

//---------------------------------------------------------------------------//
// "SKP_STR": Skip a fixed (known) string:                                   //
//---------------------------------------------------------------------------//
#ifdef SKP_STR
#undef SKP_STR
#endif
#define SKP_STR(Str)                                                           \
  {                                                                            \
    constexpr size_t strLen = sizeof(Str) - 1;                                 \
    if (utxx::unlikely(strncmp(Str, curr, strLen) != 0))                       \
      UTXX_THROW_RUNTIME_ERROR("\'", Str, "\' not found");                     \
    curr += strLen;                                                            \
    if (utxx::unlikely(int(curr - a_msg_body) > a_msg_len))                    \
      UTXX_THROW_RUNTIME_ERROR("Out of bounds:", curr);                        \
  }

//---------------------------------------------------------------------------//
// "GET_STR": "Var" will hold a 0-terminated string:                         //
//---------------------------------------------------------------------------//
#ifdef GET_STR
#undef GET_STR
#endif
#define GET_STR(Var)                                                           \
  char const *Var = curr;                                                      \
  curr = strchr(curr, '"');                                                    \
  assert(curr != nullptr && int(curr - a_msg_body) < a_msg_len);               \
  /* 0-terminate the Var string: */                                            \
  *const_cast<char *>(curr) = '\0';                                            \
  ++curr;

//---------------------------------------------------------------------------//
// "DBG_STR": Similar to "GET_STR", but the Var is for DebugMode only:       //
//---------------------------------------------------------------------------//
#ifdef DBG_STR
#undef DBG_STR
#endif
#define DBG_STR(Var)                                                           \
  DEBUG_ONLY(char const *Var = curr;)                                          \
  curr = strchr(curr, '"');                                                    \
  assert(curr != nullptr && int(curr - a_msg_body) < a_msg_len);               \
  /* 0-terminate the Var string, but only in DebugMode: */                     \
  DEBUG_ONLY(*const_cast<char *>(curr) = '\0';)                                \
  ++curr;

//---------------------------------------------------------------------------//
// "GET_BOOL":                                                               //
//---------------------------------------------------------------------------//
#ifdef GET_BOOL
#undef GET_BOOL
#endif
#define GET_BOOL(Var)                                                          \
  bool Var = (strncmp("true", curr, 4) == 0);                                  \
  assert(Var || (strncmp("false", curr, 5) == 0));                             \
  curr += (Var ? 4 : 5);

//---------------------------------------------------------------------------//
// "SKP_SPC": Skip white space:                                              //
//---------------------------------------------------------------------------//
#ifdef SKP_SPC
#undef SKP_SPC
#endif
#define SKP_SPC()                                                              \
  {                                                                            \
    while (isspace(*curr))                                                     \
      ++curr;                                                                  \
  }

//=========================================================================//
// Utils:                                                                  //
//=========================================================================//
//-------------------------------------------------------------------------//
// "SkipNFlds": Skip "a_n" comma-separated fields:                         //
//-------------------------------------------------------------------------//
// More precisely, skip "a_n" commas and return thr ptr to the char after the
// last skipped comma:
//
template <unsigned N, bool Strict = true>
inline char const *SkipNFlds(char const *a_curr) {
  if (Strict)
    assert(a_curr != nullptr);
  // XXX: The loop should be unrolled by the optimizer:
  for (unsigned i = 0; i < N; ++i) {
    a_curr = strchr(a_curr, ',');
    if (Strict)
      assert(a_curr != nullptr);
    ++a_curr;
  }
  return a_curr;
}
