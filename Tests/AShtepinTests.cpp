// vim:ts=2:et
//===========================================================================//
//                          "Tools/AShtepinTests.cpp":                       //
//===========================================================================//
#include "Basis/MiscUtils/MinMax.hpp"
#include "Basis/MiscUtils/SumCalcer.hpp"
#include "Basis/MiscUtils/MeanCalcer.hpp"
#include "Basis/PxsQtys.h"
#include "stdlib.h"
#include <time.h>

using namespace MAQUETTE;
using namespace std;

//===========================================================================//
// "main":                                                                   //
//===========================================================================//
int main() {

  MeanCalcer<double, 50> rb(6);
  // SumCalcer<double, 50> rb(6);
  srand(unsigned(time(nullptr)));

  for (int i = 0; i < 12; ++i)
  {
    auto x = i; //(rand() % 100 - 50);
    (void) rb.PushBack(x);
    // std::cout << "Sum=" << rb.GetSum() << "   ";
    std::cout << "Mean=" << rb.GetMean() << "   ";
    // std::cout << "Min=" << rb.GetMin() << "  Max=" << rb.GetMax() << "   ";
    rb.Print();
  }

  rb.Fill(13);
  rb.Print();

  return 0;
}
