#include "pch.h"
#include "WinsockApi.hpp"
#include "Error.hpp"
#include <memory>
#include <detours.h>
#include "Log.hpp"
#include "GamePacketProxy.hpp"
#include <thread>
#include <vector>
#include <map>
#include <mutex>
#include <queue>
#include "CRC.hpp"
#include <optional>


#define proxylog(format, ...) Log("\033[32m" format "\033[0m", __VA_ARGS__)

constexpr u_short proxyPortH = 1337;

struct ReceivedPacket
{
  std::vector<uint8_t> data;
  sockaddr_in originalFrom = {};
  sockaddr_in originalTo = {};
};

enum class PacketType : uint8_t
{
  GamePacket,
  KeepAlive,
};

struct ForwardedPacketHeader
{
  uint32_t crc;
  sockaddr_in originalTo;
  u_short originalSourcePortH;
  PacketType type;
};

class Proxy
{
public:
  void installHooks();
  virtual void initialise() = 0;
  virtual void run() = 0;

protected:
  virtual int forwardSend(SOCKET s, const char* buf, int len, int flags, const struct sockaddr* to, int tolen, SOCKET forwardSock, sockaddr_in& forwardTo);

  virtual int sendto_override(SOCKET s, const char* buf, int len, int flags, const struct sockaddr* to, int tolen) = 0;
  int recvfrom_override(SOCKET s, char* buf, int len, int flags, struct sockaddr* from, int* fromlen);
  int WSAAsyncSelect_override(SOCKET s, HWND hWnd, u_int wMsg, long lEvent);
  int connect_override(SOCKET s, const struct sockaddr* name, int namelen);
  int send_override(SOCKET s, const char* buf, int len, int flags);
  int recv_override(SOCKET s, char* buf, int len, int flags);

  PFN_sendto sendto_orig = nullptr;
  PFN_recvfrom recvfrom_orig = nullptr;
  PFN_WSAAsyncSelect WSAAsyncSelect_orig = nullptr;
  PFN_connect connect_orig = nullptr;
  PFN_send send_orig = nullptr;
  PFN_recv recv_orig = nullptr;

  static Proxy* staticProxy;
  static int PASCAL sendto_override_static(SOCKET s, const char* buf, int len, int flags, const struct sockaddr* to, int tolen) { return staticProxy->sendto_override(s, buf, len, flags, to, tolen); }
  static int PASCAL recvfrom_override_static(SOCKET s, char* buf, int len, int flags, struct sockaddr* from, int* fromlen) { return staticProxy->recvfrom_override(s, buf, len, flags, from, fromlen); }
  static int PASCAL WSAAsyncSelect_override_static(SOCKET s, HWND hWnd, u_int wMsg, long lEvent) { return staticProxy->WSAAsyncSelect_override(s, hWnd, wMsg, lEvent); }
  static int PASCAL connect_override_static(SOCKET s, const struct sockaddr* name, int namelen) { return staticProxy->connect_override(s, name, namelen); }
  static int PASCAL send_override_static(SOCKET s, const char* buf, int len, int flags) { return staticProxy->send_override(s, buf, len, flags); }
  static int PASCAL recv_override_static(SOCKET s, char* buf, int len, int flags) { return staticProxy->recv_override(s, buf, len, flags); }

  std::mutex mutex;
  std::map<u_short, std::queue<ReceivedPacket>> receivedPacketsByPort;

  struct SocketData
  {
    SOCKET sock = 0;
    std::optional<sockaddr_in> bindAddr;
    std::optional<sockaddr_in> connectAddr;

    bool isAsyncSelectRegistered = false;

    bool readEventSent = false;
    HWND asyncSelectHWnd = nullptr;
    u_int asyncSelectWMsg = 0;
    long asyncSelectLEvent = 0;

    void sendReadEvent()
    {
      proxylog("sending aysnc read event for sock %u\n", sock);
      PostMessageA(asyncSelectHWnd, asyncSelectWMsg, sock, FD_READ);
      readEventSent = true;
    }

    void sendWriteEvent()
    {
      proxylog("sending aysnc write event for sock %u\n", sock);
      PostMessageA(asyncSelectHWnd, asyncSelectWMsg, sock, FD_WRITE);
      readEventSent = true;
    }
  };

  std::vector<SocketData> sockets;

  SocketData* tryGetSocketData(SOCKET s)
  {
    for (SocketData& sock : this->sockets)
    {
      if (sock.sock == s)
        return &sock;
    }

    return nullptr;
  }

  SocketData& getOrCreateSocketData(SOCKET s)
  {
    for (SocketData& sock : this->sockets)
    {
      if (sock.sock == s)
        return sock;
    }

    SocketData& retval = this->sockets.emplace_back();
    retval.sock = s;
    return retval;
  }
};

Proxy* Proxy::staticProxy = nullptr;

void Proxy::installHooks()
{
  staticProxy = this;

  HMODULE wsock = LoadLibraryA("wsock32.dll");

  sendto_orig = (PFN_sendto)GetProcAddress(wsock, "sendto");
  recvfrom_orig = (PFN_recvfrom)GetProcAddress(wsock, "recvfrom");
  WSAAsyncSelect_orig = (PFN_WSAAsyncSelect)GetProcAddress(wsock, "WSAAsyncSelect");
  connect_orig = (PFN_connect)GetProcAddress(wsock, "connect");
  send_orig = (PFN_send)GetProcAddress(wsock, "send");
  recv_orig = (PFN_recv)GetProcAddress(wsock, "recv");

  DetourTransactionBegin();
  DetourUpdateThread(GetCurrentThread());

  DetourAttach(&(PVOID&)sendto_orig, sendto_override_static);
  DetourAttach(&(PVOID&)recvfrom_orig, recvfrom_override_static);
  DetourAttach(&(PVOID&)WSAAsyncSelect_orig, WSAAsyncSelect_override_static);
  DetourAttach(&(PVOID&)connect_orig, connect_override_static);
  DetourAttach(&(PVOID&)send_orig, send_override_static);
  DetourAttach(&(PVOID&)recv_orig, recv_override_static);

  DetourTransactionCommit();
}

int Proxy::recvfrom_override(SOCKET s, char* buf, int len, int flags, struct sockaddr* from, int* fromlen)
{
  std::scoped_lock lock(this->mutex);

  SocketData* socketData = tryGetSocketData(s);

  if (socketData)
    socketData->readEventSent = false;

  sockaddr_in boundAddr = {};
  int size = sizeof(sockaddr_in);
  release_assert(getsockname(s, (sockaddr*)&boundAddr, &size) == 0);

  int portH = ntohs(boundAddr.sin_port);
  proxylog("Proxy::recvfrom_override socket: %u, len: %d, flags: %d, port: %d\n", s, len, flags, portH);

  auto it = this->receivedPacketsByPort.find(portH);
  if (it != this->receivedPacketsByPort.end() && !it->second.empty())
  {
    ReceivedPacket& packet = it->second.front();

    release_assert(len >= int(it->second.front().data.size()));
    memcpy(buf, packet.data.data(), packet.data.size());

    if (from)
    {
      release_assert(*fromlen >= sizeof(sockaddr_in));
      memcpy(from, &packet.originalFrom, sizeof(sockaddr_in));
      *fromlen = sizeof(sockaddr_in);
    }

    int retval = int(packet.data.size());
    it->second.pop();

    if (!it->second.empty() && socketData && socketData->isAsyncSelectRegistered)
      socketData->sendReadEvent();

    proxylog("    -> SUCCESS %d\n", retval);
    WSASetLastError(0);
    return retval;
  }

  proxylog("    -> WSAEWOULDBLOCK -1\n");
  WSASetLastError(WSAEWOULDBLOCK);
  return SOCKET_ERROR;

  //return this->recvfrom_orig(s, buf, len, flags, from, fromlen);
}

int Proxy::WSAAsyncSelect_override(SOCKET s, HWND hWnd, u_int wMsg, long lEvent)
{
  proxylog("WSAAsyncSelect: socket: %u HWND: %p, wMsg: %u, lEvent: %ld\n", s, hWnd, wMsg, lEvent);

  release_assert(wMsg);

  sockaddr_in sourceSockName = {};
  int size = sizeof(sockaddr_in);
  getsockname(s, (sockaddr*)&sourceSockName, &size);
  release_assert(sourceSockName.sin_port);

  std::scoped_lock lock(this->mutex);

  SocketData& socketData = getOrCreateSocketData(s);
  socketData.isAsyncSelectRegistered = true;
  socketData.asyncSelectHWnd = hWnd;
  socketData.asyncSelectWMsg = wMsg;
  socketData.asyncSelectLEvent = lEvent;

  socketData.sendWriteEvent();

  WSASetLastError(0);
  return 0;
}

int Proxy::connect_override(SOCKET s, const struct sockaddr* name, int namelen)
{
  int type = 0;
  int length = sizeof(int);
  getsockopt(s, SOL_SOCKET, SO_TYPE, (char*)&type, &length);

  if (type != SOCK_DGRAM)
    return this->connect_orig(s, name, namelen);

  std::scoped_lock lock(this->mutex);

  proxylog("Proxy::connect_override: socket: %u name: %s\n", s, sockaddrToString(name).c_str());

  release_assert(name->sa_family == AF_INET);
  this->getOrCreateSocketData(s).connectAddr = *((sockaddr_in*)name);

  WSASetLastError(0);
  return 0;
}

int Proxy::send_override(SOCKET s, const char* buf, int len, int flags)
{
  int type = 0;
  int length = sizeof(int);
  getsockopt(s, SOL_SOCKET, SO_TYPE, (char*)&type, &length);

  if (type != SOCK_DGRAM)
    return this->send_orig(s, buf, len, flags);

  sockaddr_in destination;
  {
    std::scoped_lock lock(this->mutex);
    SocketData* socketData = this->tryGetSocketData(s);
    release_assert(socketData);
    destination = *socketData->connectAddr;
  }

  proxylog("Proxy::send_override: socket: %u, len: %d, flags: %d\n", s, len, flags);
  return this->sendto_override(s, buf, len, flags, (sockaddr*)&destination, sizeof(sockaddr_in));
}

int Proxy::recv_override(SOCKET s, char* buf, int len, int flags)
{
  int type = 0;
  int length = sizeof(int);
  getsockopt(s, SOL_SOCKET, SO_TYPE, (char*)&type, &length);

  if (type != SOCK_DGRAM)
    return this->recv_orig(s, buf, len, flags);

  sockaddr_in connectAddr;
  {
    std::scoped_lock lock(this->mutex);
    SocketData* socketData = this->tryGetSocketData(s);
    release_assert(socketData);
    connectAddr = *socketData->connectAddr;
  }

  proxylog("Proxy::recv_override: socket: %u, len: %d, flags: %d\n", s, len, flags);

  sockaddr_in from = {};
  int fromLen = sizeof(sockaddr_in);

  int ret = this->recvfrom_override(s, buf, len, flags, (sockaddr*)&from, &fromLen);
  // TODO: check that connectAddr matches here?
  return ret;
}

int Proxy::forwardSend(SOCKET s, const char* buf, int len, int flags, const struct sockaddr* to, int tolen, SOCKET forwardSock, sockaddr_in& forwardTo)
{
  constexpr int newPacketBuffSize = 5000;
  uint8_t newPacketBuff[newPacketBuffSize];

  int finalPacketLen = len + sizeof(ForwardedPacketHeader);

  release_assert(finalPacketLen < newPacketBuffSize);
  release_assert(to);
  release_assert(to->sa_family == AF_INET);

  sockaddr_in to4 = *((sockaddr_in*)to);
  sockaddr_in sourceSockName = {};
  int size = sizeof(sockaddr_in);
  getsockname(s, (sockaddr*)&sourceSockName, &size);

  proxylog("Proxy::forwardSend: socket: %u, bindPort: %d, len: %d, flags: %d to: %s\n", s, (int)htons(sourceSockName.sin_port), len, flags, sockaddrToString(to).c_str());

  ForwardedPacketHeader* header = (ForwardedPacketHeader*)newPacketBuff;
  header->originalTo = to4;
  header->originalSourcePortH = ntohs(sourceSockName.sin_port);
  header->type = PacketType::GamePacket;

  uint8_t* packetData = newPacketBuff + sizeof(ForwardedPacketHeader);
  memcpy(packetData, buf, len);

  header->crc = CRC::Calculate(newPacketBuff + 4, finalPacketLen - 4, CRC::CRC_32());

  int retval = this->sendto_orig(forwardSock, (const char*)newPacketBuff, finalPacketLen, 0, (sockaddr*)&forwardTo, sizeof(sockaddr_in));
  if (retval == SOCKET_ERROR)
    return retval;

  release_assert(retval == finalPacketLen);
  return finalPacketLen - sizeof(ForwardedPacketHeader);
}



class ProxyServer : public Proxy
{
public:
  void initialise() override;
  void run() override;

private:
  int sendto_override(SOCKET s, const char* buf, int len, int flags, const struct sockaddr* to, int tolen) override;
  void sendKeepaliveLoop();

private:
  SOCKET sock = INVALID_SOCKET;
  sockaddr_in replyAddr = {};
};

void ProxyServer::initialise()
{
  proxylog("Creating proxy server socket\n");
  this->sock = socket(AF_INET, SOCK_DGRAM, 0);
  release_assert(sock != INVALID_SOCKET);

  sockaddr_in bindAddress = {};
  bindAddress.sin_family = AF_INET;
  bindAddress.sin_addr.S_un.S_addr = INADDR_ANY;
  bindAddress.sin_port = htons(proxyPortH);

  release_assert(bind(this->sock, (sockaddr*)&bindAddress, sizeof(sockaddr_in)) == 0);
}

void ProxyServer::run()
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
      continue;

    proxylog("ProxyServer::run: got packet from: %s, originalTo: %s\n", sockaddrToString(&from4).c_str(), sockaddrToString(&header->originalTo).c_str());

    if (this->replyAddr.sin_family == AF_UNSPEC)
    {
      proxylog("got connection from %s\n", sockaddrToString(&from4).c_str());
      this->replyAddr = from4;
    }

    if (header->type != PacketType::GamePacket)
      continue;

    uint8_t* originalData = buff + sizeof(ForwardedPacketHeader);
    int originalDataSize = bytesRead - sizeof(ForwardedPacketHeader);

    std::scoped_lock lock(this->mutex);

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
}

void ProxyServer::sendKeepaliveLoop()
{
  while (true)
  {
    ForwardedPacketHeader keepalivePacket = {};
    keepalivePacket.type = PacketType::KeepAlive;
    keepalivePacket.crc = CRC::Calculate(&keepalivePacket + 4, sizeof(ForwardedPacketHeader) - 4, CRC::CRC_32());
    this->sendto_orig(this->sock, (const char*)&keepalivePacket, sizeof(ForwardedPacketHeader), 0, (sockaddr*)&this->replyAddr, sizeof(this->replyAddr));

    Sleep(1000 * 10);
  }
}

int ProxyServer::sendto_override(SOCKET s, const char* buf, int len, int flags, const struct sockaddr* to, int tolen)
{
  release_assert(this->replyAddr.sin_family != AF_UNSPEC);
  return this->forwardSend(s, buf, len, flags, to, tolen, this->sock, this->replyAddr);
}

class ProxyClient : public Proxy
{
public:
  explicit ProxyClient(std::string serverAddr) : serverAddrString(serverAddr) {}

  void initialise() override;
  void run() override;

private:
  int sendto_override(SOCKET s, const char* buf, int len, int flags, const struct sockaddr* to, int tolen) override;
  void sendKeepaliveLoop();

private:
  std::string serverAddrString;
  sockaddr_in serverAddr = {};
  SOCKET sock = INVALID_SOCKET;
};


void ProxyClient::initialise()
{
  this->sock = socket(AF_INET, SOCK_DGRAM, 0);
  release_assert(sock != INVALID_SOCKET);
  this->serverAddr.sin_family = AF_INET;
  this->serverAddr.sin_port = htons(proxyPortH);
  this->serverAddr.sin_addr.S_un.S_addr = inet_addr(this->serverAddrString.c_str());
}

void ProxyClient::sendKeepaliveLoop()
{
  while (true)
  {
    ForwardedPacketHeader keepalivePacket = {};
    keepalivePacket.type = PacketType::KeepAlive;
    keepalivePacket.crc = CRC::Calculate(((uint8_t*)&keepalivePacket) + 4, sizeof(ForwardedPacketHeader) - 4, CRC::CRC_32());
    this->sendto_orig(this->sock, (const char*)&keepalivePacket, sizeof(ForwardedPacketHeader), 0, (sockaddr*)&this->serverAddr, sizeof(this->serverAddr));

    Sleep(1000 * 10);
  }
}

void ProxyClient::run()
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
      continue;

    proxylog("ProxyClient::run: got packet from server: %s, originalTo: %s\n", sockaddrToString(&this->serverAddr).c_str(), sockaddrToString(&header->originalTo).c_str());

    if (header->type != PacketType::GamePacket)
      continue;

    uint8_t* originalData = buff + sizeof(ForwardedPacketHeader);
    int originalDataSize = bytesRead - sizeof(ForwardedPacketHeader);

    std::scoped_lock lock(this->mutex);

    u_short destPortH = ntohs(header->originalTo.sin_port);
    ReceivedPacket& newPacket = this->receivedPacketsByPort[destPortH].emplace();
    newPacket.data = std::vector<uint8_t>(originalData, originalData + originalDataSize);
    newPacket.originalFrom = this->serverAddr;
    newPacket.originalFrom.sin_port = htons(header->originalSourcePortH);
    newPacket.originalTo = header->originalTo;
  }
}

int ProxyClient::sendto_override(SOCKET s, const char* buf, int len, int flags, const struct sockaddr* to, int tolen)
{
  return this->forwardSend(s, buf, len, flags, to, tolen, this->sock, this->serverAddr);
}

void init(std::unique_ptr<Proxy> proxy)
{
  proxy->installHooks();

  WSAData wSAData = {};
  // init api version 1.1, because that's what emperor uses
  WSAStartup(MAKEWORD(1, 1), &wSAData);
  proxy->initialise();

  std::thread([proxy=std::move(proxy)]()
  {
    proxy->run();
  }).detach();
}


void installProxyServer()
{
  proxylog("initialising as server\n");
  init(std::unique_ptr<Proxy>(new ProxyServer()));
}

void installProxyClient(const std::string& serverAddr)
{
  proxylog("initialising as client, connecting to: %s\n", serverAddr.c_str());
  init(std::unique_ptr<Proxy>(new ProxyClient(serverAddr)));
}