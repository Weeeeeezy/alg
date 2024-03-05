// vim:ts=2:et
//===========================================================================//
//                      "Tests/SpecialFPValsTest.cpp":                       //
//        Representation of NaNs and Infinities as "double" and "long"       //
//===========================================================================//
#include <iostream>
#include <cmath>
#include <climits>

using namespace std;

int main()
{
  // NaN Rep:
  long          x = long(0xfff0000000000001L);
  double const* f = reinterpret_cast<double const*>(&x);

  // NegInf Rep:
  long          y = long(0xfff0000000000000L);
  double const* g = reinterpret_cast<double const*>(&y);

  // PosInf Rep:
  long          z = long(0x7ff0000000000000L);
  double const* h = reinterpret_cast<double const*>(&z);

  cout << "NaN: x=" << x << ", f=" << (*f) << ",\tIsNaN=" << isnan(*f) << endl;
  cout << "-oo: y=" << y << ", g=" << (*g) << ",\tIsInf=" << isinf(*g) << endl;
  cout << "+oo: z=" << z << ", h=" << (*h) << ",\tIsInf=" << isinf(*h) << endl;
  return 0;
}
