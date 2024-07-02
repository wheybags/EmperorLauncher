#pragma once
#include "WolProxyClient.hpp"
#include "Error.hpp"
#include "CRC.hpp"
#include "WolCommon.hpp"


void WolProxyClient::initialise()
{
  this->sock = socket(AF_INET, SOCK_DGRAM, 0);
  release_assert(sock != INVALID_SOCKET);
  this->serverAddr.sin_family = AF_INET;
  this->serverAddr.sin_port = htons(wolPortH);
  this->serverAddr.sin_addr.S_un.S_addr = inet_addr(this->serverAddrString.c_str());

  this->servservAddr = this->serverAddr.sin_addr;
}

void WolProxyClient::sendKeepaliveLoop()
{
  while (true)
  {
    {
      std::scoped_lock lock(this->mutex);
      ForwardedPacketHeader keepalivePacket = {};
      keepalivePacket.type = PacketType::KeepAlive;
      keepalivePacket.crc = CRC::Calculate(((uint8_t*)&keepalivePacket) + 4, sizeof(ForwardedPacketHeader) - 4, CRC::CRC_32());
      sendto_orig(this->sock, (const char*)&keepalivePacket, sizeof(ForwardedPacketHeader), 0, (sockaddr*)&this->serverAddr, sizeof(this->serverAddr));
    }

    Sleep(1000 * 10);
  }
}

void WolProxyClient::run()
{
  std::thread([this]()
  {
    this->sendKeepaliveLoop();
  }).detach();


  constexpr int buffSize = 5000;
  uint8_t buff[buffSize];

  while (true)
  {
    int serverAddrSize = sizeof(this->serverAddr);
    int bytesRead = recvfrom_orig(this->sock, (char*)&buff[0], buffSize, 0, (sockaddr*)&this->serverAddr, &serverAddrSize);

    proxylog("client got something %d\n", bytesRead);

    if (bytesRead < int(sizeof(ForwardedPacketHeader)))
      continue;

    uint32_t crcReal = CRC::Calculate(buff + 4, bytesRead - 4, CRC::CRC_32());
    ForwardedPacketHeader* header = (ForwardedPacketHeader*)buff;

    if (crcReal != header->crc)
    {
      proxylog("crc mismatch\n");
      continue;
    }

    proxylog("ProxyClient::run: got packet from: %s:%d, originalTo: %s\n", in_addr_to_string(header->originalSourceAddr).c_str(), header->originalSourcePortH, sockaddrToString(&header->originalTo).c_str());

    if (header->type != PacketType::GamePacket)
      continue;

    uint8_t* originalData = buff + sizeof(ForwardedPacketHeader);
    int originalDataSize = bytesRead - sizeof(ForwardedPacketHeader);

    std::scoped_lock lock(this->mutex);

    u_short destPortH = ntohs(header->originalTo.sin_port);
    ReceivedPacket& newPacket = this->receivedPacketsByPort[destPortH].emplace();
    newPacket.data = std::vector<uint8_t>(originalData, originalData + originalDataSize);
    newPacket.originalFrom.sin_family = AF_INET;
    newPacket.originalFrom.sin_addr = header->originalSourceAddr;
    newPacket.originalFrom.sin_port = htons(header->originalSourcePortH);
    newPacket.originalTo = header->originalTo;

    this->gameSpeedFromHost = header->gameSpeed;
  }
}

int WolProxyClient::sendto_override(SOCKET s, const char* buf, int len, int flags, const struct sockaddr* to, int tolen)
{
  sockaddr_in sourceSockName = {};
  int size = sizeof(sockaddr_in);
  getsockname(s, (sockaddr*)&sourceSockName, &size);

  std::scoped_lock lock(this->mutex);

  return WolProxyBase::forwardSend(ntohs(sourceSockName.sin_port), buf, len, flags, to, tolen, this->sock, this->serverAddr);
}
