// vims:ts=2
//===========================================================================//
//                          "Tests/TimeStampTest.cpp":                       //
//===========================================================================//
#include "Basis/TimeValUtils.hpp"
#include <iostream>

using namespace MAQUETTE;
using namespace std;

int main()
{
  char const*    str1 = "20150625-09:40:21";
  utxx::time_val ts1  = DateTimeToTimeValFIX<true>(str1);
  cout << str1 << ": " << (ts1.empty() ? "ERROR" : "OK") << endl;

  char const*    str2 = "20150625-09:40:21.999";
  utxx::time_val ts2  = DateTimeToTimeValFIX<true>(str2);
  cout << str2 << ": " << (ts2.empty() ? "ERROR" : "OK") << endl;

  return 0;
}
