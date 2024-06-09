#include "../EmperorHooks/WolServer.hpp"
#pragma comment (lib, "Ws2_32.lib")


int main(void)
{
  WSADATA wsaData;
  WSAStartup(MAKEWORD(1, 1), &wsaData);


  WolServer server;
  server.run();
  return 0;
}

