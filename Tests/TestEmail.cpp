#include "Basis/IOSendEmail.hpp"

using namespace MAQUETTE::IO;

int main(int, char **) {
  IOSendEmail sender("jonas@l2quant.com");

  sender.Send("Test Message",
              "Hi there,\n\nThis is a simple test message.\n\nBest,\nJonas");

  sender.Send("Test Message 2", "I hope this still works");

  return 0;
}
