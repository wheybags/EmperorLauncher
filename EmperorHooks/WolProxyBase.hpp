#pragma once
#include <windows.h>
#include <mutex>
#include <map>
#include <optional>
#include <queue>
#include "WinsockApi.hpp"

//#define proxylog(format, ...) Log("\033[32m" format "\033[0m", __VA_ARGS__)
#define proxylog(format, ...) do {} while(0)

struct ReceivedPacket
{
  std::vector<uint8_t> data;
  sockaddr_in originalFrom = {};
  sockaddr_in originalTo = {};
};

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

  void sendReadEvent();
  void sendWriteEvent();
};

struct PortRange
{
  u_short startH;
  u_short endH;
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
  in_addr originalSourceAddr; // only used for host->client packets
  uint32_t gameSpeed; // only used for host->client packets
  PacketType type;
  uint16_t portRangeStartH = 0;
  uint16_t portRangeEndH = 0;
};


extern PFN_sendto sendto_orig;
extern PFN_recvfrom recvfrom_orig;
extern PFN_WSAAsyncSelect WSAAsyncSelect_orig;
extern PFN_connect connect_orig;
extern PFN_send send_orig;
extern PFN_recv recv_orig;
extern PFN_gethostbyname gethostbyname_orig;

class WolProxyBase;
extern WolProxyBase* staticProxy;
extern std::atomic<PortRange> portRange;


class WolProxyBase
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
  static int forwardSend(uint16_t originalSourcePortH, const char* buf, int len, int flags, const struct sockaddr* to, int tolen, SOCKET forwardSock, sockaddr_in& forwardTo, uint32_t frameLimit = UINT_MAX, in_addr originalSourceAddr = {});

  std::recursive_mutex mutex;
  std::map<u_short, std::queue<ReceivedPacket>> receivedPacketsByPort;
  in_addr servservAddr = {};

  std::vector<SocketData> sockets;

  SocketData* tryGetSocketData(SOCKET s);
  SocketData& getOrCreateSocketData(SOCKET s);
};
