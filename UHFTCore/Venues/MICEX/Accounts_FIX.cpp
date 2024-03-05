// vim:ts=2:et
//===========================================================================//
//                        "Venues/MICEX/Accounts_FIX.cpp":                   //
//                     Account Configs for MICEX FIX Connector               //
//===========================================================================//
#include "Venues/MICEX/Configs_FIX.hpp"

using namespace std;

namespace MAQUETTE
{
namespace MICEX
{
  //=========================================================================//
  // "FIXAccountConfigs":                                                    //
  //=========================================================================//
  // NB: In all cases,  we allow for 40000 Reqs / 5 min (300 sec); MICEX actu-
  // ally allows 45000, but we stay on the safe side. Non-FullAmt Orders:
  //
  //-------------------------------------------------------------------------//
  // Production Accounts:                                                    //
  //-------------------------------------------------------------------------//
  FIXAccountConfig const ALFA01_ProdAcc
    { "MD0000500469", "", "", "password", "MB0000509612", "", "", '\0', 0,
      300, 40000 };   // Bound to: ARobot11

  FIXAccountConfig const ALFA02_ProdAcc
    { "MD0000500815", "", "", "password", "MB0000509612", "", "", '\0', 0,
      300, 40000 };   // Bound to: ARobot11

  FIXAccountConfig const ALFA03_ProdAcc
    { "MD0000500826", "", "", "password", "MB0000509612", "", "", '\0', 0,
      300, 40000 };   // Bound to: ARobot1 and ARobot11

  FIXAccountConfig const ALFA04_ProdAcc
    { "MD0000500837", "", "", "password", "MB0000509612", "", "", '\0', 0,
      300, 40000 };   // Bound to: ARobot1

  FIXAccountConfig const ALFA05_ProdAcc
    { "MD0000500848", "", "", "password", "MB0000509612", "", "", '\0', 0,
      300, 40000 };   // Bound to: ARobot11

  FIXAccountConfig const ALFA06_ProdAcc
    { "MD0000500859", "", "", "password", "MB0000509612", "", "", '\0', 0,
      300, 40000 };   // Bound to: ARobot11

  FIXAccountConfig const ALFA07_ProdAcc
    { "MD000050086A", "", "", "password", "MB0000509612", "", "", '\0', 0,
      300, 40000 };   // Bound to: TBC

  FIXAccountConfig const ALFA08_ProdAcc
    { "MD000050087B", "", "", "password", "MB0000509612", "", "", '\0', 0,
      300, 40000 };   // Bound to: TBC

  FIXAccountConfig const ALFA09_ProdAcc
    { "MD000050088C", "", "", "password", "MB0000509612", "", "", '\0', 0,
      300, 40000 };   // Bound to: TBC

  // XXX: The following is an example of how the "PartyGroup" could be used:
  FIXAccountConfig const MPFX1_ProdAcc
    { "MD0010101998", "", "", "password", "MB0010106287", "",
      "SMP7807", 'D', 3,  300, 40000
    };

  //-------------------------------------------------------------------------//
  // Test Account(s):                                                        //
  //-------------------------------------------------------------------------//
  FIXAccountConfig const ALFA01_TestAcc
    { "MD0000450004", "", "", " ",        "MB00005CURR0", "", "", '\0', 0,
      //          ^-- Last digit could be 1..3 as well
      300, 40000 };

  //-------------------------------------------------------------------------//
  // Over-All Map:                                                           //
  //-------------------------------------------------------------------------//
  // XXX : We have to replicate all suitable Accounts across all Configs;
  // TODO: There are no EQ entries yet -- only FX:
  //
  map<string, FIXAccountConfig const> const FIXAccountConfigs
  {
    // Prod1 (XXX: NOT recommended at any time):
    { "ALFA01-FIX-MICEX-FX-Prod1",  ALFA01_ProdAcc },
    { "ALFA02-FIX-MICEX-FX-Prod1",  ALFA02_ProdAcc },
    { "ALFA03-FIX-MICEX-FX-Prod1",  ALFA03_ProdAcc },
    { "ALFA04-FIX-MICEX-FX-Prod1",  ALFA04_ProdAcc },
    { "ALFA05-FIX-MICEX-FX-Prod1",  ALFA05_ProdAcc },
    { "ALFA06-FIX-MICEX-FX-Prod1",  ALFA06_ProdAcc },
    { "ALFA07-FIX-MICEX-FX-Prod1",  ALFA06_ProdAcc },
    { "ALFA08-FIX-MICEX-FX-Prod1",  ALFA06_ProdAcc },
    { "ALFA09-FIX-MICEX-FX-Prod1",  ALFA06_ProdAcc },
    {  "MPFX1-FIX-MICEX-FX-Prod1",   MPFX1_ProdAcc },
    // Prod20:
    { "ALFA01-FIX-MICEX-FX-Prod20", ALFA01_ProdAcc },
    { "ALFA02-FIX-MICEX-FX-Prod20", ALFA02_ProdAcc },
    { "ALFA03-FIX-MICEX-FX-Prod20", ALFA03_ProdAcc },
    { "ALFA04-FIX-MICEX-FX-Prod20", ALFA04_ProdAcc },
    { "ALFA05-FIX-MICEX-FX-Prod20", ALFA05_ProdAcc },
    { "ALFA06-FIX-MICEX-FX-Prod20", ALFA06_ProdAcc },
    { "ALFA07-FIX-MICEX-FX-Prod20", ALFA06_ProdAcc },
    { "ALFA08-FIX-MICEX-FX-Prod20", ALFA06_ProdAcc },
    { "ALFA09-FIX-MICEX-FX-Prod20", ALFA06_ProdAcc },
    {  "MPFX1-FIX-MICEX-FX-Prod20",  MPFX1_ProdAcc },
    // Prod21:
    { "ALFA01-FIX-MICEX-FX-Prod21", ALFA01_ProdAcc },
    { "ALFA02-FIX-MICEX-FX-Prod21", ALFA02_ProdAcc },
    { "ALFA03-FIX-MICEX-FX-Prod21", ALFA03_ProdAcc },
    { "ALFA04-FIX-MICEX-FX-Prod21", ALFA04_ProdAcc },
    { "ALFA05-FIX-MICEX-FX-Prod21", ALFA05_ProdAcc },
    { "ALFA06-FIX-MICEX-FX-Prod21", ALFA06_ProdAcc },
    { "ALFA07-FIX-MICEX-FX-Prod21", ALFA06_ProdAcc },
    { "ALFA08-FIX-MICEX-FX-Prod21", ALFA06_ProdAcc },
    { "ALFA09-FIX-MICEX-FX-Prod21", ALFA06_ProdAcc },
    {  "MPFX1-FIX-MICEX-FX-Prod21",  MPFX1_ProdAcc },
    // Prod30:
    { "ALFA01-FIX-MICEX-FX-Prod30", ALFA01_ProdAcc },
    { "ALFA02-FIX-MICEX-FX-Prod30", ALFA02_ProdAcc },
    { "ALFA03-FIX-MICEX-FX-Prod30", ALFA03_ProdAcc },
    { "ALFA04-FIX-MICEX-FX-Prod30", ALFA04_ProdAcc },
    { "ALFA05-FIX-MICEX-FX-Prod30", ALFA05_ProdAcc },
    { "ALFA06-FIX-MICEX-FX-Prod30", ALFA06_ProdAcc },
    { "ALFA07-FIX-MICEX-FX-Prod30", ALFA06_ProdAcc },
    { "ALFA08-FIX-MICEX-FX-Prod30", ALFA06_ProdAcc },
    { "ALFA09-FIX-MICEX-FX-Prod30", ALFA06_ProdAcc },
    {  "MPFX1-FIX-MICEX-FX-Prod30",  MPFX1_ProdAcc },
    // Prod31:
    { "ALFA01-FIX-MICEX-FX-Prod31", ALFA01_ProdAcc },
    { "ALFA02-FIX-MICEX-FX-Prod31", ALFA02_ProdAcc },
    { "ALFA03-FIX-MICEX-FX-Prod31", ALFA03_ProdAcc },
    { "ALFA04-FIX-MICEX-FX-Prod31", ALFA04_ProdAcc },
    { "ALFA05-FIX-MICEX-FX-Prod31", ALFA05_ProdAcc },
    { "ALFA06-FIX-MICEX-FX-Prod31", ALFA06_ProdAcc },
    { "ALFA07-FIX-MICEX-FX-Prod31", ALFA06_ProdAcc },
    { "ALFA08-FIX-MICEX-FX-Prod31", ALFA06_ProdAcc },
    { "ALFA09-FIX-MICEX-FX-Prod31", ALFA06_ProdAcc },
    {  "MPFX1-FIX-MICEX-FX-Prod31",  MPFX1_ProdAcc },

    // UATL{1,2}, TestL{1,2}, UATI, TestI: All use same TestAcc:
    { "ALFA01-FIX-MICEX-FX-UATL1",     ALFA01_TestAcc },
    { "ALFA01-FIX-MICEX-FX-UATL2",     ALFA01_TestAcc },
    { "ALFA01-FIX-MICEX-FX-TestL1",    ALFA01_TestAcc },
    { "ALFA01-FIX-MICEX-FX-TestL2",    ALFA01_TestAcc },
    { "ALFA01-FIX-MICEX-FX-UATI",      ALFA01_TestAcc },
    { "ALFA01-FIX-MICEX-FX-TestI",     ALFA01_TestAcc }
  };
} // End namespace MICEX
} // End namespace MAQUETTE
