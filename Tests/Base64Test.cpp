// vim:ts=2:et
//===========================================================================//
//                           "Tests/Base64Test.cpp":                         //
//===========================================================================//
#include "Basis/Base64.h"
#include <sys/random.h>
#include <iostream>
#include <cstring>
#include <cassert>

using namespace MAQUETTE;
using namespace std;

int main(int argc, char* argv[])
{
  // Get the Len param:
  int Len = -1;
  if (argc != 2 || (Len = atoi(argv[1])) < 0)
  {
    cerr << "PARAMETER: TestSeqLen (>= 0)" << endl;
    return 1;
  }

  uint8_t bin0[Len], bin1[Len];
  int     ALen = max<int>(Len*2, 4) + 1;
  char    asc [ALen];

  // Generate a random binary sequence:
  int   rc  = 0;
  do    rc  = int(getrandom(bin0, size_t(Len), 0));
  while(rc != Len);

  // Perform Base64-encoding:
  int    aLen = Base64EnCodeUnChecked(bin0, Len, asc);
  cout << "BinLen=" << Len << " -> ASCIILen=" << aLen << ", Limit="
       << (ALen-1)  << endl;

  assert(aLen < ALen);
  asc[aLen] = '\0';
  cout << asc << endl;

  // Decode it back:
  int bLen = Base64DeCodeUnChecked(asc, aLen, bin1);
  if (bLen != Len)
  {
    cerr << "ERROR: OrigSize=" << Len << ", DeCodedSize=" << bLen << endl;
    return 1;
  }

  // If sizes are OK, compare the sequences:
  if (memcmp(bin0, bin1, size_t(bLen)) != 0)
  {
    cerr << "ERROR: Orig and DeCided Seqs do not match" << endl;
    return 1;
  }
  // If OK:
  return 0;
}
