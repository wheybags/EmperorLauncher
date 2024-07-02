#include "WolProxyBase.hpp"
#include "Error.hpp"
#include "CRC.hpp"

PFN_sendto sendto_orig = nullptr;
PFN_recvfrom recvfrom_orig = nullptr;
PFN_WSAAsyncSelect WSAAsyncSelect_orig = nullptr;
PFN_connect connect_orig = nullptr;
PFN_send send_orig = nullptr;
PFN_recv recv_orig = nullptr;
PFN_gethostbyname gethostbyname_orig = nullptr;

WolProxyBase* staticProxy = nullptr;
std::atomic<PortRange> portRange = PortRange{ 0, 0 };


int WolProxyBase::recvfrom_override(SOCKET s, char* buf, int len, int flags, struct sockaddr* from, int* fromlen)
{
  std::scoped_lock lock(this->mutex);

  SocketData* socketData = tryGetSocketData(s);

  if (socketData)
    socketData->readEventSent = false;

  sockaddr_in boundAddr = {};
  int size = sizeof(sockaddr_in);
  release_assert(getsockname(s, (sockaddr*)&boundAddr, &size) == 0);

  int portH = ntohs(boundAddr.sin_port);
  //proxylog("Proxy::recvfrom_override socket: %u, len: %d, flags: %d, port: %d\n", s, len, flags, portH);

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

    proxylog("Proxy::recvfrom_override socket: %u, len: %d, flags: %d, port: %d\n", s, len, flags, portH);
    proxylog("    -> SUCCESS %d\n", retval);
    WSASetLastError(0);
    return retval;
  }

  //proxylog("    -> WSAEWOULDBLOCK -1\n");
  WSASetLastError(WSAEWOULDBLOCK);
  return SOCKET_ERROR;

  //return this->recvfrom_orig(s, buf, len, flags, from, fromlen);
}

int WolProxyBase::WSAAsyncSelect_override(SOCKET s, HWND hWnd, u_int wMsg, long lEvent)
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

int WolProxyBase::connect_override(SOCKET s, const struct sockaddr* name, int namelen)
{
  int type = 0;
  int length = sizeof(int);
  getsockopt(s, SOL_SOCKET, SO_TYPE, (char*)&type, &length);

  if (type != SOCK_DGRAM)
    return connect_orig(s, name, namelen);

  std::scoped_lock lock(this->mutex);

  proxylog("Proxy::connect_override: socket: %u name: %s\n", s, sockaddrToString(name).c_str());

  release_assert(name->sa_family == AF_INET);
  this->getOrCreateSocketData(s).connectAddr = *((sockaddr_in*)name);

  WSASetLastError(0);
  return 0;
}

int WolProxyBase::send_override(SOCKET s, const char* buf, int len, int flags)
{
  int type = 0;
  int length = sizeof(int);
  getsockopt(s, SOL_SOCKET, SO_TYPE, (char*)&type, &length);

  if (type != SOCK_DGRAM)
    return send_orig(s, buf, len, flags);

  std::scoped_lock lock(this->mutex);

  sockaddr_in destination;
  {
    SocketData* socketData = this->tryGetSocketData(s);
    release_assert(socketData);
    destination = *socketData->connectAddr;
  }

  proxylog("Proxy::send_override: socket: %u, len: %d, flags: %d\n", s, len, flags);
  return this->sendto_override(s, buf, len, flags, (sockaddr*)&destination, sizeof(sockaddr_in));
}

int WolProxyBase::recv_override(SOCKET s, char* buf, int len, int flags)
{
  int type = 0;
  int length = sizeof(int);
  getsockopt(s, SOL_SOCKET, SO_TYPE, (char*)&type, &length);

  if (type != SOCK_DGRAM)
    return recv_orig(s, buf, len, flags);

  std::scoped_lock lock(this->mutex);

  sockaddr_in connectAddr;
  {
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

struct hostent* WolProxyBase::gethostbyname_override(const char* name)
{
  if (strcmp(name, "servserv.westwood.com") == 0)
  {
    std::scoped_lock lock(this->mutex);

    static thread_local hostent host = {};
    static thread_local in_addr addr = {};
    static thread_local in_addr* addrList[] = { &addr, nullptr };
    static thread_local char* aliasList[] = { nullptr };

    addr = this->servservAddr;

    host.h_name = (char*)"servserv.westwood.com";
    host.h_aliases = aliasList;
    host.h_addrtype = AF_INET;
    host.h_length = 4;
    host.h_addr_list = (char**)addrList;

    proxylog("Proxy::gethostbyname_override: %s -> %s\n", name, inet_ntoa(addr));

    return &host;
  }

  return gethostbyname_orig(name);
}

int WolProxyBase::forwardSend(uint16_t originalSourcePortH, const char* buf, int len, int flags, const struct sockaddr* to, int tolen, SOCKET forwardSock, sockaddr_in& forwardTo, uint32_t frameLimit, in_addr originalSourceAddr)
{
  constexpr int newPacketBuffSize = 5000;
  uint8_t newPacketBuff[newPacketBuffSize];

  int finalPacketLen = len + sizeof(ForwardedPacketHeader);

  release_assert(finalPacketLen < newPacketBuffSize);
  release_assert(to);
  release_assert(to->sa_family == AF_INET);

  sockaddr_in to4 = *((sockaddr_in*)to);


  proxylog("Proxy::forwardSend: originalSourcePort: %u, len: %d, flags: %d to: %s\n", originalSourcePortH, len, flags, sockaddrToString(to).c_str());

  ForwardedPacketHeader* header = (ForwardedPacketHeader*)newPacketBuff;
  header->originalTo = to4;
  header->originalSourcePortH = originalSourcePortH;
  header->originalSourceAddr = originalSourceAddr;
  header->gameSpeed = frameLimit;
  header->type = PacketType::GamePacket;

  PortRange range = portRange.load();
  header->portRangeStartH = range.startH;
  header->portRangeEndH = range.endH;

  uint8_t* packetData = newPacketBuff + sizeof(ForwardedPacketHeader);
  memcpy(packetData, buf, len);

  header->crc = CRC::Calculate(newPacketBuff + 4, finalPacketLen - 4, CRC::CRC_32());

  int retval = sendto_orig(forwardSock, (const char*)newPacketBuff, finalPacketLen, 0, (sockaddr*)&forwardTo, sizeof(sockaddr_in));
  if (retval == SOCKET_ERROR)
    return retval;

  release_assert(retval == finalPacketLen);
  return finalPacketLen - sizeof(ForwardedPacketHeader);
}

SocketData* WolProxyBase::tryGetSocketData(SOCKET s)
{
  std::scoped_lock lock(this->mutex);

  for (SocketData& sock : this->sockets)
  {
    if (sock.sock == s)
      return &sock;
  }

  return nullptr;
}

SocketData& WolProxyBase::getOrCreateSocketData(SOCKET s)
{
  std::scoped_lock lock(this->mutex);

  for (SocketData& sock : this->sockets)
  {
    if (sock.sock == s)
      return sock;
  }

  SocketData& retval = this->sockets.emplace_back();
  retval.sock = s;
  return retval;
}

void SocketData::sendReadEvent()
{
  proxylog("sending aysnc read event for sock %u\n", sock);
  PostMessageA(asyncSelectHWnd, asyncSelectWMsg, sock, FD_READ);
  readEventSent = true;
}

void SocketData::sendWriteEvent()
{
  proxylog("sending aysnc write event for sock %u\n", sock);
  PostMessageA(asyncSelectHWnd, asyncSelectWMsg, sock, FD_WRITE);
  readEventSent = true;
}

