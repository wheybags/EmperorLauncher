#include "../EmperorHooks/WolServer.hpp"
#pragma comment (lib, "Ws2_32.lib")


int main(void)
{
  WolServer server;
  server.run();
  return 0;
}

