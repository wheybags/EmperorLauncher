#include "WinsockApi.hpp"
#include "Error.hpp"
#include <memory>
#include <detours.h>
#include "Log.hpp"
#include "PatchWol.hpp"
#include <thread>
#include <vector>
#include <map>
#include <mutex>
#include <queue>
#include "CRC.hpp"
#include <optional>
#include "GameExeImports.hpp"
#include <combaseapi.h>
#include "WolServer.hpp"

//#define proxylog(format, ...) Log("\033[32m" format "\033[0m", __VA_ARGS__)
#define proxylog(format, ...) do {} while(0)

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
  uint32_t gameSpeed;
  PacketType type;
};

class Proxy;
static Proxy* staticProxy = nullptr;

static PFN_sendto sendto_orig = nullptr;
static PFN_recvfrom recvfrom_orig = nullptr;
static PFN_WSAAsyncSelect WSAAsyncSelect_orig = nullptr;
static PFN_connect connect_orig = nullptr;
static PFN_send send_orig = nullptr;
static PFN_recv recv_orig = nullptr;
static PFN_gethostbyname gethostbyname_orig = nullptr;


class Proxy
{
public:
  virtual void initialise() = 0;
  virtual void run() = 0;
  virtual bool isServer() = 0;

public:
  virtual int sendto_override(SOCKET s, const char* buf, int len, int flags, const struct sockaddr* to, int tolen) = 0;
  int recvfrom_override(SOCKET s, char* buf, int len, int flags, struct sockaddr* from, int* fromlen);
  int WSAAsyncSelect_override(SOCKET s, HWND hWnd, u_int wMsg, long lEvent);
  int connect_override(SOCKET s, const struct sockaddr* name, int namelen);
  int send_override(SOCKET s, const char* buf, int len, int flags);
  int recv_override(SOCKET s, char* buf, int len, int flags);
  struct hostent* gethostbyname_override(const char* name);

protected:
  int forwardSend(SOCKET s, const char* buf, int len, int flags, const struct sockaddr* to, int tolen, SOCKET forwardSock, sockaddr_in& forwardTo, uint32_t frameLimit=UINT_MAX);

  std::mutex mutex;
  std::map<u_short, std::queue<ReceivedPacket>> receivedPacketsByPort;
  in_addr servservAddr = {};

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
    return connect_orig(s, name, namelen);

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
    return send_orig(s, buf, len, flags);

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
    return recv_orig(s, buf, len, flags);

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

struct hostent* Proxy::gethostbyname_override(const char* name)
{
  if (strcmp(name, "servserv.westwood.com") == 0)
  {
    static thread_local hostent host = {};
    static thread_local in_addr addr = {};
    static thread_local in_addr* addrList[] = {&addr, nullptr};
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

int Proxy::forwardSend(SOCKET s, const char* buf, int len, int flags, const struct sockaddr* to, int tolen, SOCKET forwardSock, sockaddr_in& forwardTo, uint32_t frameLimit)
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
  header->gameSpeed = frameLimit;
  header->type = PacketType::GamePacket;

  uint8_t* packetData = newPacketBuff + sizeof(ForwardedPacketHeader);
  memcpy(packetData, buf, len);

  header->crc = CRC::Calculate(newPacketBuff + 4, finalPacketLen - 4, CRC::CRC_32());

  int retval = sendto_orig(forwardSock, (const char*)newPacketBuff, finalPacketLen, 0, (sockaddr*)&forwardTo, sizeof(sockaddr_in));
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
  bool isServer() override { return true; }

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

  this->servservAddr.S_un.S_addr = inet_addr("127.0.119.98");

  std::thread([]() {
    WolServer wolServer;
    wolServer.run();
  }).detach();
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
      Log("got connection from %s\n", sockaddrToString(&from4).c_str());
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
    sendto_orig(this->sock, (const char*)&keepalivePacket, sizeof(ForwardedPacketHeader), 0, (sockaddr*)&this->replyAddr, sizeof(this->replyAddr));

    Sleep(1000 * 10);
  }
}

int ProxyServer::sendto_override(SOCKET s, const char* buf, int len, int flags, const struct sockaddr* to, int tolen)
{
  release_assert(this->replyAddr.sin_family != AF_UNSPEC);
  int frameLimit = GameOptions_getGameSpeed(&someGlobalThing.optionsStorage->gameOptions);
  return this->forwardSend(s, buf, len, flags, to, tolen, this->sock, this->replyAddr, frameLimit);
}

class ProxyClient : public Proxy
{
public:
  explicit ProxyClient(std::string serverAddr) : serverAddrString(serverAddr) {}

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


void ProxyClient::initialise()
{
  this->sock = socket(AF_INET, SOCK_DGRAM, 0);
  release_assert(sock != INVALID_SOCKET);
  this->serverAddr.sin_family = AF_INET;
  this->serverAddr.sin_port = htons(proxyPortH);
  this->serverAddr.sin_addr.S_un.S_addr = inet_addr(this->serverAddrString.c_str());

  this->servservAddr = this->serverAddr.sin_addr;
}

void ProxyClient::sendKeepaliveLoop()
{
  while (true)
  {
    ForwardedPacketHeader keepalivePacket = {};
    keepalivePacket.type = PacketType::KeepAlive;
    keepalivePacket.crc = CRC::Calculate(((uint8_t*)&keepalivePacket) + 4, sizeof(ForwardedPacketHeader) - 4, CRC::CRC_32());
    sendto_orig(this->sock, (const char*)&keepalivePacket, sizeof(ForwardedPacketHeader), 0, (sockaddr*)&this->serverAddr, sizeof(this->serverAddr));

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

    this->gameSpeedFromHost = header->gameSpeed;
  }
}

int ProxyClient::sendto_override(SOCKET s, const char* buf, int len, int flags, const struct sockaddr* to, int tolen)
{
  return this->forwardSend(s, buf, len, flags, to, tolen, this->sock, this->serverAddr);
}

// This patch forces the in game speed setting to apply to multiplayer games. Unpatched, the game will simply run as fast as the
// slowest client. The game does some calculation to determine what frame limit to use depending on the speed of all the other
// clients, then sets it by calling CNetworkAdmin::setFrameLimit (this function). We override this function, and make sure that
// the frame limit is never set higher than the current speed setting. The host is in control, so on the host we read the game speed
// setting directly from the game options. We also send the game speed in all server->client packets, so the clients know what speed
// to use. I tried just setting the limit on the server only, and while it does work, it also causes nasty rubberbanding on the client.
// original is __thiscall, but we can't declare a thiscall func outside a class, so we fake it with fastcall and an unused edx param
PFN_CNetworkAdmin_setFrameLimit CNetworkAdmin_setFrameLimitOrig = CNetworkAdmin_setFrameLimit;
int __fastcall CNetworkAdmin_setFrameLimitPatched(CNetworkAdmin* This, DWORD edx, uint32_t value)
{
  uint32_t gameSpeed;
  if (staticProxy->isServer())
    gameSpeed = GameOptions_getGameSpeed(&someGlobalThing.optionsStorage->gameOptions);
  else
    gameSpeed = ((ProxyClient*)staticProxy)->getGameSpeedFromHost();


  // This switch is mostly copied from CNetworkAdmin::setFrameLimitFromGlobalSettings, but without a special case check
  // that seems to cause crashes when we use it from an unexpected place like this
  uint32_t maxFrameLimit = 0;
  switch (gameSpeed)
  {
  case 1u:
    maxFrameLimit = 8;
    break;
  case 2u:
    maxFrameLimit = 10;
    break;
  case 3u:
    maxFrameLimit = 12;
    break;
  case 4u:
    maxFrameLimit = 15;
    break;
  case 5u:
    maxFrameLimit = 20;
    break;
  case 6u:
    maxFrameLimit = 25;
    break;
  case 7u:
    maxFrameLimit = 30;
    break;
  default:
    maxFrameLimit = -1;
    break;
  }

  if (value > maxFrameLimit)
    value = maxFrameLimit;

  This->frameLimit = value;
  return This->frameLimit;
}

// skips the attempt to use port mangling
PFN_CMangler_Pattern_Query CMangler_Pattern_Query_Orig = CMangler_Pattern_Query;
void CMangler_Pattern_Query_Patched()
{
  return;
}

static int PASCAL sendto_override_static(SOCKET s, const char* buf, int len, int flags, const struct sockaddr* to, int tolen) { return staticProxy->sendto_override(s, buf, len, flags, to, tolen); }
static int PASCAL recvfrom_override_static(SOCKET s, char* buf, int len, int flags, struct sockaddr* from, int* fromlen) { return staticProxy->recvfrom_override(s, buf, len, flags, from, fromlen); }
static int PASCAL WSAAsyncSelect_override_static(SOCKET s, HWND hWnd, u_int wMsg, long lEvent) { return staticProxy->WSAAsyncSelect_override(s, hWnd, wMsg, lEvent); }
static int PASCAL connect_override_static(SOCKET s, const struct sockaddr* name, int namelen) { return staticProxy->connect_override(s, name, namelen); }
static int PASCAL send_override_static(SOCKET s, const char* buf, int len, int flags) { return staticProxy->send_override(s, buf, len, flags); }
static int PASCAL recv_override_static(SOCKET s, char* buf, int len, int flags) { return staticProxy->recv_override(s, buf, len, flags); }
static struct hostent* PASCAL gethostbyname_override_static(const char* name) { return staticProxy->gethostbyname_override(name); }


typedef HRESULT(__stdcall* PFN_DllRegisterServer)();

// WOL access is provided through a separately installed COM type library (WOLAPI.DLL).
// It's annoying to have to use the old broken installer though, so here we just register the type
// library ourselves. We use registry redirects to allow us to do this without admin privileges,
// and OaEnablePerUserTLibRegistration is a magic function that makes the redirects work for
// registering a type library.
// Technique from here: https://stackoverflow.com/q/44379353
void registerWolTypeLibrary()
{
  HMODULE wolApi = nullptr;
  wolApi = LoadLibraryA("WOL/WOLAPI.DLL");

  release_assert(wolApi);

  PFN_DllRegisterServer wolApiRegisterServer = (PFN_DllRegisterServer)GetProcAddress(wolApi, "DllRegisterServer");
  release_assert(wolApiRegisterServer);

  OaEnablePerUserTLibRegistration();

  HKEY key = nullptr;
  release_assert(RegCreateKeyA(HKEY_CURRENT_USER, "Software\\Classes", &key) == ERROR_SUCCESS);
  release_assert(RegOverridePredefKey(HKEY_CLASSES_ROOT, key) == ERROR_SUCCESS);
  RegCloseKey(key);

  release_assert(SUCCEEDED(wolApiRegisterServer()));

  release_assert(RegOverridePredefKey(HKEY_CLASSES_ROOT, nullptr) == ERROR_SUCCESS);

  CloseHandle(wolApi);
}

void init(Proxy* proxy)
{
  registerWolTypeLibrary();

  HMODULE wsock = LoadLibraryA("wsock32.dll");
  sendto_orig = (PFN_sendto)GetProcAddress(wsock, "sendto");
  recvfrom_orig = (PFN_recvfrom)GetProcAddress(wsock, "recvfrom");
  WSAAsyncSelect_orig = (PFN_WSAAsyncSelect)GetProcAddress(wsock, "WSAAsyncSelect");
  connect_orig = (PFN_connect)GetProcAddress(wsock, "connect");
  send_orig = (PFN_send)GetProcAddress(wsock, "send");
  recv_orig = (PFN_recv)GetProcAddress(wsock, "recv");
  gethostbyname_orig = (PFN_gethostbyname)GetProcAddress(wsock, "gethostbyname");

  DetourTransactionBegin();
  DetourUpdateThread(GetCurrentThread());
  DetourAttach(&(PVOID&)CNetworkAdmin_setFrameLimitOrig, CNetworkAdmin_setFrameLimitPatched);
  DetourAttach(&(PVOID&)CMangler_Pattern_Query_Orig, CMangler_Pattern_Query_Patched);
  DetourAttach(&(PVOID&)sendto_orig, sendto_override_static);
  DetourAttach(&(PVOID&)recvfrom_orig, recvfrom_override_static);
  DetourAttach(&(PVOID&)WSAAsyncSelect_orig, WSAAsyncSelect_override_static);
  DetourAttach(&(PVOID&)connect_orig, connect_override_static);
  DetourAttach(&(PVOID&)send_orig, send_override_static);
  DetourAttach(&(PVOID&)recv_orig, recv_override_static);
  DetourAttach(&(PVOID&)gethostbyname_orig, gethostbyname_override_static);
  DetourTransactionCommit();

  WSAData wSAData = {};
  // init api version 1.1, because that's what emperor uses
  WSAStartup(MAKEWORD(1, 1), &wSAData);

  staticProxy = proxy;
  staticProxy->initialise();

  std::thread([]()
  {
    staticProxy->run();
  }).detach();
}


void patchWolAsServer()
{
  proxylog("initialising as server\n");
  init(new ProxyServer());
}

void patchWolAsClient(const std::string& serverAddr)
{
  proxylog("initialising as client, connecting to: %s\n", serverAddr.c_str());
  init(new ProxyClient(serverAddr));
}