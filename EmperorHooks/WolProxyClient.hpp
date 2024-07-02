#pragma once
#include "WolProxyBase.hpp"

class WolProxyClient : public WolProxyBase
{
public:
  explicit WolProxyClient(std::string serverAddr) : serverAddrString(serverAddr) {}

  void initialise() override;
  void run() override;
  bool isServer() override { return false; }

  int getGameSpeedFromHost() const { return this->gameSpeedFromHost.load(); }

private:
  int sendto_override(SOCKET s, const char* buf, int len, int flags, const struct sockaddr* to, int tolen) override;
  void sendKeepaliveLoop();

private:
  std::string serverAddrString;
  sockaddr_in serverAddr = {};
  SOCKET sock = INVALID_SOCKET;
  std::atomic<int> gameSpeedFromHost = 6;
};