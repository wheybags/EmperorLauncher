#include "../EmperorHooks/WolIrcServer.hpp"
#pragma comment (lib, "Ws2_32.lib")


int main(void)
{
  WSADATA wsaData;
  WSAStartup(MAKEWORD(1, 1), &wsaData);


  WolIrcServer server;
  server.run();
  return 0;
}

