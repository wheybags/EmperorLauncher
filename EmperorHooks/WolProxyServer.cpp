#include "WolProxyServer.hpp"
#include "Error.hpp"
#include "WolPort.hpp"
#include "WolIrcServer.hpp"
#include "CRC.hpp"
#include "GameExeImports.hpp"
#include "Log.hpp"

void WolProxyServer::initialise()
{
  proxylog("Creating proxy server socket\n");
  this->sock = socket(AF_INET, SOCK_DGRAM, 0);
  release_assert(sock != INVALID_SOCKET);

  sockaddr_in bindAddress = {};
  bindAddress.sin_family = AF_INET;
  bindAddress.sin_addr.S_un.S_addr = INADDR_ANY;
  bindAddress.sin_port = htons(wolPortH);

  release_assert(bind(this->sock, (sockaddr*)&bindAddress, sizeof(sockaddr_in)) == 0);

  this->servservAddr.S_un.S_addr = inet_addr("127.0.0.1");

  std::thread([]()
  {
    WolIrcServer wolServer;
    wolServer.run();
  }).detach();
}

void WolProxyServer::run()
{
  std::thread([this]()
  {
    this->sendKeepaliveLoop();
  }).detach();

  constexpr int buffSize = 5000;
  uint8_t buff[buffSize];

  while (true)
  {
    sockaddr from = {};
    int fromlen = sizeof(sockaddr);
    int bytesRead = recvfrom_orig(this->sock, (char*)&buff[0], buffSize, 0, &from, &fromlen);

    std::scoped_lock lock(this->mutex);

    if (bytesRead < int(sizeof(ForwardedPacketHeader)))
      continue;

    if (from.sa_family != AF_INET)
    {
      proxylog("got non ipv4 packet\n");
      continue;
    }

    sockaddr_in from4 = *((sockaddr_in*)&from);
    uint32_t crcReal = CRC::Calculate(buff + 4, bytesRead - 4, CRC::CRC_32());
    ForwardedPacketHeader* header = (ForwardedPacketHeader*)buff;

    if (crcReal != header->crc)
    {
      proxylog("crc mismatch\n");
      continue;
    }

    proxylog("ProxyServer::run: got packet from: %s, originalTo: %s\n", sockaddrToString(&from4).c_str(), sockaddrToString(&header->originalTo).c_str());

    ClientData* fromClient = nullptr;
    for (int32_t i = 0; i < int32_t(this->clients.size()); i++)
    {
      if (from4.sin_addr.S_un.S_addr == this->clients[i].realReplyAddr.sin_addr.S_un.S_addr && from4.sin_port == this->clients[i].realReplyAddr.sin_port)
      {
        fromClient = &this->clients[i];
        break;
      }
    }

    if (!fromClient)
    {
      Log("got connection from %s\n", sockaddrToString(&from4).c_str());
      fromClient = &clients.emplace_back();
      fromClient->realReplyAddr = from4;
    }

    fromClient->lastReceiveTime = GetTickCount64();


    if (header->type == PacketType::GamePacket)
    {
      if (fromClient->virtualPortRangeStartH != header->portRangeStartH || fromClient->virtualPortRangeEndH != header->portRangeEndH)
      {
        proxylog("updating port range for client %s:%d: %d-%d\n", inet_ntoa(from4.sin_addr), ntohs(from4.sin_port), header->portRangeStartH, header->portRangeEndH);
        fromClient->virtualPortRangeStartH = header->portRangeStartH;
        fromClient->virtualPortRangeEndH = header->portRangeEndH;
      }

      uint8_t* originalData = buff + sizeof(ForwardedPacketHeader);
      int originalDataSize = bytesRead - sizeof(ForwardedPacketHeader);

      // packet is for this server
      if (header->originalTo.sin_addr.S_un.S_addr == this->servservAddr.S_un.S_addr)
      {
        u_short destPortH = ntohs(header->originalTo.sin_port);
        ReceivedPacket& newPacket = this->receivedPacketsByPort[destPortH].emplace();
        newPacket.data = std::vector<uint8_t>(originalData, originalData + originalDataSize);
        newPacket.originalFrom = from4;
        newPacket.originalFrom.sin_port = htons(header->originalSourcePortH);
        newPacket.originalTo = header->originalTo;


        for (SocketData& socketData : this->sockets)
        {
          if (socketData.isAsyncSelectRegistered &&
            socketData.bindAddr.has_value() &&
            ntohs(socketData.bindAddr->sin_port) == destPortH &&
            !socketData.readEventSent)
          {
            socketData.sendReadEvent();
          }
        }
      }
      else // packet is for another client?
      {
        ClientData* destClient = nullptr;
        for (int32_t i = 0; i < int32_t(this->clients.size()); i++)
        {
          ClientData* temp = &this->clients[i];
          if (header->originalTo.sin_addr.S_un.S_addr == temp->realReplyAddr.sin_addr.S_un.S_addr && ntohs(header->originalTo.sin_port) >= temp->virtualPortRangeStartH && ntohs(header->originalTo.sin_port) <= temp->virtualPortRangeEndH)
          {
            destClient = temp;
            break;
          }
        }

        if (destClient)
        {
          header->originalSourceAddr = from4.sin_addr;
          header->crc = CRC::Calculate(buff + 4, bytesRead - 4, CRC::CRC_32());


          proxylog("forwarding packet, originalSourceAddr: %s, originalSourcePortH: %u, originalTo: %s\n",
            in_addr_to_string(header->originalSourceAddr).c_str(),
            header->originalSourcePortH,
            sockaddrToString(&header->originalTo).c_str());


          sendto_orig(this->sock, (const char*)buff, bytesRead, 0, (sockaddr*)&destClient->realReplyAddr, sizeof(sockaddr_in));
        }
        else
        {
          proxylog("got packet for unknown destination %s:%d\n", inet_ntoa(header->originalTo.sin_addr), ntohs(header->originalTo.sin_port));
        }
      }
    }
  }
}

void WolProxyServer::sendKeepaliveLoop()
{
  while (true)
  {
    {
      std::scoped_lock lock(this->mutex);
      for (int32_t i = 0; i < int32_t(this->clients.size()); )
      {
        if (GetTickCount64() - this->clients[i].lastReceiveTime > 1000 * 60)
        {
          Log("client %s:%d timed out\n", inet_ntoa(this->clients[i].realReplyAddr.sin_addr), ntohs(this->clients[i].realReplyAddr.sin_port));
          this->clients.erase(this->clients.begin() + i);
        }
        else
        {
          ForwardedPacketHeader keepalivePacket = {};
          keepalivePacket.type = PacketType::KeepAlive;
          keepalivePacket.crc = CRC::Calculate(((uint8_t*)&keepalivePacket) + 4, sizeof(ForwardedPacketHeader) - 4, CRC::CRC_32());
          sendto_orig(this->sock, (const char*)&keepalivePacket, sizeof(ForwardedPacketHeader), 0, (sockaddr*)&this->clients[i].realReplyAddr, sizeof(this->clients[i].realReplyAddr));

          i++;
        }
      }
    }

    Sleep(1000 * 10);
  }
}

int WolProxyServer::sendto_override(SOCKET s, const char* buf, int len, int flags, const struct sockaddr* to, int tolen)
{
  std::scoped_lock lock(this->mutex);

  sockaddr_in* to4 = (sockaddr_in*)to;
  uint16_t toPortH = ntohs(to4->sin_port);

  ClientData* client = nullptr;

  for (int32_t i = 0; i < int32_t(this->clients.size()); i++)
  {
    ClientData* temp = &this->clients[i];
    if (temp->realReplyAddr.sin_addr.S_un.S_addr == to4->sin_addr.S_un.S_addr && toPortH >= temp->virtualPortRangeStartH && toPortH <= temp->virtualPortRangeEndH)
    {
      client = &this->clients[i];
      break;
    }
  }

  if (!client)
  {
    proxylog("ProxyServer::sendto_override: no client found for %s:%d\n", inet_ntoa(to4->sin_addr), toPortH);
    return len;
  }

  int frameLimit = GameOptions_getGameSpeed(&someGlobalThing.optionsStorage->gameOptions);


  sockaddr_in sourceSockName = {};
  int size = sizeof(sockaddr_in);
  getsockname(s, (sockaddr*)&sourceSockName, &size);

  return this->forwardSend(ntohs(sourceSockName.sin_port), buf, len, flags, to, tolen, this->sock, client->realReplyAddr, frameLimit, this->servservAddr);
}