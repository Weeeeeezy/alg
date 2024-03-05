// vim:ts=2:et
//===========================================================================//
//                         "Tests/MisAlligned.cpp":                          //
//        Provoking a BusError on Accessing MisAligned Integer Data          //
//===========================================================================//
#include <iostream>

int main()
{
#if defined(__GNUC__)
# if defined(__i386__)
    /* Enable Alignment Checking on x86 */
    __asm__("pushf\norl $0x40000,(%esp)\npopf");
# elif defined(__x86_64__)
     /* Enable Alignment Checking on x86_64 */
    __asm__("pushf\norl $0x40000,(%rsp)\npopf");
# endif
#endif
  char buff[128];

  long* l = reinterpret_cast<long*>(buff + 1);

  *l = 24459308806L;
  std::cout << (*l) << std::endl;

  return 0;
}
