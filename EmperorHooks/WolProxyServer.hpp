#pragma once
#include "WolProxyBase.hpp"

class WolProxyServer : public WolProxyBase
{
public:
  void initialise() override;
  void run() override;
  bool isServer() override { return true; }

private:
  int sendto_override(SOCKET s, const char* buf, int len, int flags, const struct sockaddr* to, int tolen) override;
  void sendKeepaliveLoop();

private:
  SOCKET sock = INVALID_SOCKET;

  struct ClientData
  {
    sockaddr_in realReplyAddr;
    uint64_t lastReceiveTime = 0;

    uint16_t virtualPortRangeStartH = 0;
    uint16_t virtualPortRangeEndH = 0;
  };

  std::vector<ClientData> clients;
};