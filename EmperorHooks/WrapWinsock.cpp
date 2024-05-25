#include <windows.h>
#include <detours.h>
#include <string>
#include "Log.hpp"
#include "WinsockApi.hpp"

#define netprintf(format, ...) Log("\033[33m" format "\033[0m", __VA_ARGS__)


// A list of all winsock functions imported by Game.exe and WOLAPI.DLL:
// WSAAsyncGetHostByName
// WSAAsyncSelect           wrapped
// WSACancelAsyncRequest
// WSACleanup               not interesting
// WSAGetLastError          not interesting
// WSASetLastError          not interesting
// WSAStartup               not interesting
// __WSAFDIsSet             not interesting
// accept                   wrapped
// bind                     wrapped
// closesocket              wrapped
// connect                  wrapped
// gethostbyname
// gethostname              not interesting
// getpeername              not interesting
// getsockname              not interesting
// getsockopt               not interesting
// htonl                    not interesting
// htons                    not interesting
// inet_addr                not interesting
// inet_ntoa                not interesting
// ioctlsocket
// listen                   wrapped
// ntohl                    not interesting
// ntohs                    not interesting
// recv                     wrapped
// recvfrom                 wrapped
// select
// send                     wrapped
// sendto                   wrapped
// setsockopt               wrapped
// shutdown                 wrapped
// socket                   wrapped

static PFN_WSAAsyncSelect WSAAsyncSelect_orig = nullptr;
static PFN_listen listen_orig = nullptr;
static PFN_accept accept_orig = nullptr;
static PFN_shutdown shutdown_orig = nullptr;
static PFN_socket socket_orig = nullptr;
static PFN_closesocket closesocket_orig = nullptr;
static PFN_sendto sendto_orig = nullptr;
static PFN_recvfrom recvfrom_orig = nullptr;
static PFN_bind bind_orig = nullptr;
static PFN_connect connect_orig = nullptr;
static PFN_recv recv_orig = nullptr;
static PFN_send send_orig = nullptr;
static PFN_select select_orig = nullptr;
static PFN_setsockopt setsockopt_orig = nullptr;
static PFN_ioctlsocket ioctlsocket_orig = nullptr;

static int PASCAL WSAAsyncSelect_wrap(SOCKET s, HWND hWnd, u_int wMsg, long lEvent)
{
  netprintf("WSAAsyncSelect: socket: %u HWND: %p, wMsg: %u, lEvent: %ld\n", s, hWnd, wMsg, lEvent);

  int ret = WSAAsyncSelect_orig(s, hWnd, wMsg, lEvent);
  int err = WSAGetLastError();

  netprintf("    -> %d %s\n", ret, wsaErrorToString(err).c_str());

  WSASetLastError(err);
  return ret;
}

static int PASCAL listen_wrap(SOCKET s, int backlog)
{
  netprintf("listen: socket: %u backlog: %d\n", s, backlog);

  int ret = listen_orig(s, backlog);
  int err = WSAGetLastError();

  netprintf("    -> %d %s\n", ret, wsaErrorToString(err).c_str());

  WSASetLastError(err);
  return ret;
}

static SOCKET PASCAL accept_wrap(SOCKET s, struct sockaddr* addr, int* addrlen)
{
  netprintf("accept: socket: %u addr: %s\n", s, sockaddrToString(addr).c_str());

  SOCKET ret = accept_orig(s, addr, addrlen);
  int err = WSAGetLastError();

  netprintf("    -> %u %s\n", ret, wsaErrorToString(err).c_str());

  WSASetLastError(err);
  return ret;
}

static int PASCAL shutdown_wrap(SOCKET s, int how)
{
  netprintf("shutdown: socket: %u how: %d\n", s, how);

  int ret = shutdown_orig(s, how);
  int err = WSAGetLastError();

  netprintf("    -> %d %s\n", ret, wsaErrorToString(err).c_str());

  WSASetLastError(err);
  return ret;
}

static SOCKET PASCAL socket_wrap(int af, int type, int protocol)
{
  std::string typeStr;
  if (type == SOCK_STREAM)
    typeStr = "TCP";
  else if (type == SOCK_DGRAM)
    typeStr = "UDP";
  else
    typeStr = "OTHER(" + std::to_string(type) + ")";

  netprintf("socket: af: %d, type: %s, protocol: %d\n", af, typeStr.c_str(), protocol);

  SOCKET ret = socket_orig(af, type, protocol);
  int err = WSAGetLastError();

  netprintf("    -> %u %s\n", ret, wsaErrorToString(err).c_str());

  WSASetLastError(err);
  return ret;
}

static int PASCAL closesocket_wrap(SOCKET s)
{
  netprintf("closesocket: socket: %u\n", s);

  int ret = closesocket_orig(s);
  int err = WSAGetLastError();

  netprintf("    -> %d %s\n", ret, wsaErrorToString(err).c_str());

  WSASetLastError(err);
  return ret;
}

static int PASCAL sendto_wrap(SOCKET s, const char* buf, int len, int flags, const struct sockaddr* to, int tolen)
{
  netprintf("sendto: socket: %u, len: %d, flags: %d to: %s\n", s, len, flags, sockaddrToString(to).c_str());

  int ret = sendto_orig(s, buf, len, flags, to, tolen);
  int err = WSAGetLastError();

  netprintf("    -> %d %s\n", ret, wsaErrorToString(err).c_str());

  WSASetLastError(err);
  return ret;
}

static int PASCAL recvfrom_wrap(SOCKET s, char* buf, int len, int flags, struct sockaddr* from, int* fromlen)
{
  netprintf("recvfrom: socket: %u, len: %d, flags: %d\n", s, len, flags);

  int ret = recvfrom_orig(s, buf, len, flags, from, fromlen);
  int err = WSAGetLastError();

  netprintf("    -> from: %s\n", sockaddrToString(from).c_str());
  netprintf("    -> %d %s\n", ret, wsaErrorToString(err).c_str());


  WSASetLastError(err);
  return ret;
}

static int PASCAL bind_wrap(SOCKET s, const struct sockaddr* addr, int namelen)
{
  netprintf("bind: socket: %u addr: %s\n", s, sockaddrToString(addr).c_str());

  int ret = bind_orig(s, addr, namelen);
  int err = WSAGetLastError();

  netprintf("    -> %d %s\n", ret, wsaErrorToString(err).c_str());

  WSASetLastError(err);
  return ret;
}

static int PASCAL connect_wrap(SOCKET s, const struct sockaddr* name, int namelen)
{
  netprintf("connect: socket: %u addr: %s\n", s, sockaddrToString(name).c_str());

  int ret = connect_orig(s, name, namelen);
  int err = WSAGetLastError();

  netprintf("    -> %d %s\n", ret, wsaErrorToString(err).c_str());

  WSASetLastError(err);
  return ret;
}

static int PASCAL recv_wrap(SOCKET s, char* buf, int len, int flags)
{
  netprintf("recv: socket: %u, len: %d, flags: %d\n", s, len, flags);

  int ret = recv_orig(s, buf, len, flags);
  int err = WSAGetLastError();

  netprintf("    -> %d %s\n", ret, wsaErrorToString(err).c_str());

  WSASetLastError(err);
  return ret;
}

static int PASCAL send_wrap(SOCKET s, const char* buf, int len, int flags)
{
  netprintf("send: socket: %u, len: %d, flags: %d\n", s, len, flags);

  int ret = send_orig(s, buf, len, flags);
  int err = WSAGetLastError();

  netprintf("    -> %d %s\n", ret, wsaErrorToString(err).c_str());

  WSASetLastError(err);
  return ret;
}

static std::string fd_set_tostring(const fd_set* set)
{
  if (!set)
    return "null";

  std::string retval = "[";
  for (u_int i = 0; i < set->fd_count; i++)
    retval += std::to_string(set->fd_array[i]) + ",";

  if (retval.size() > 1)
    retval.resize(retval.size() - 1);

  retval += "]";

  return retval;
}

static int PASCAL select_wrap(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, const struct timeval* timeout)
{
  netprintf("select:\n");
  netprintf(" readfds: %s\n", fd_set_tostring(readfds).c_str());
  netprintf(" writefds: %s\n", fd_set_tostring(writefds).c_str());
  netprintf(" exceptfds: %s\n", fd_set_tostring(exceptfds).c_str());

  int ret = select_orig(nfds, readfds, writefds, exceptfds, timeout);
  int err = WSAGetLastError();

  netprintf("    -> readfds: %s\n", fd_set_tostring(readfds).c_str());
  netprintf("    -> writefds: %s\n", fd_set_tostring(writefds).c_str());
  netprintf("    -> exceptfds: %s\n", fd_set_tostring(exceptfds).c_str());
  netprintf("    -> %d %s\n", ret, wsaErrorToString(err).c_str());

  WSASetLastError(err);
  return ret;
}

static std::string sockopt_to_string(int level, int opt)
{
  if (level == SOL_SOCKET)
  {
    switch (opt)
    {
      case SO_DEBUG: return "SO_DEBUG";
      case SO_ACCEPTCONN: return "SO_ACCEPTCONN";
      case SO_REUSEADDR: return "SO_REUSEADDR";
      case SO_KEEPALIVE: return "SO_KEEPALIVE";
      case SO_DONTROUTE: return "SO_DONTROUTE";
      case SO_BROADCAST: return "SO_BROADCAST";
      case SO_USELOOPBACK: return "SO_USELOOPBACK";
      case SO_LINGER: return "SO_LINGER";
      case SO_OOBINLINE: return "SO_OOBINLINE";
      case SO_DONTLINGER: return "SO_DONTLINGER";
      case SO_SNDBUF: return "SO_SNDBUF";
      case SO_RCVBUF: return "SO_RCVBUF";
      case SO_SNDLOWAT: return "SO_SNDLOWAT";
      case SO_RCVLOWAT: return "SO_RCVLOWAT";
      case SO_SNDTIMEO: return "SO_SNDTIMEO";
      case SO_RCVTIMEO: return "SO_RCVTIMEO";
      case SO_ERROR: return "SO_ERROR";
      case SO_TYPE: return "SO_TYPE";
    }
  }

  return "unknown(" + std::to_string(opt) + ")";
}

static int PASCAL setsockopt_wrap(SOCKET s, int level, int optname, const char* optval, int optlen)
{
  netprintf("setsockopt: socket: %u, level: %d, optname: %s\n", s, level, sockopt_to_string(level, optname).c_str());

  int ret = setsockopt_orig(s, level, optname, optval, optlen);
  int err = WSAGetLastError();

  netprintf("    -> %d %s\n", ret, wsaErrorToString(err).c_str());

  WSASetLastError(err);
  return ret;
}

static int PASCAL ioctlsocket_wrap(SOCKET s, long cmd, u_long* argp)
{
  if (cmd == FIONBIO)
    netprintf("ioctlsocket: socket: %u, cmd: FIONBIO, val: %lu\n", s, cmd, *argp);
  else
    netprintf("ioctlsocket: socket: %u, cmd: %d\n", s, cmd);

  int ret = ioctlsocket_orig(s, cmd, argp);
  int err = WSAGetLastError();

  netprintf("    -> %d %s\n", ret, wsaErrorToString(err).c_str());

  WSASetLastError(err);
  return ret;
}

void wrapWinsockWithLogging()
{
  HMODULE wsock = LoadLibraryA("wsock32.dll");

  WSAAsyncSelect_orig = (PFN_WSAAsyncSelect)GetProcAddress(wsock, "WSAAsyncSelect");
  listen_orig = (PFN_listen)GetProcAddress(wsock, "listen");
  accept_orig = (PFN_accept)GetProcAddress(wsock, "accept");
  shutdown_orig = (PFN_shutdown)GetProcAddress(wsock, "shutdown");
  socket_orig = (PFN_socket)GetProcAddress(wsock, "socket");
  closesocket_orig = (PFN_closesocket)GetProcAddress(wsock, "closesocket");
  sendto_orig = (PFN_sendto)GetProcAddress(wsock, "sendto");
  recvfrom_orig = (PFN_recvfrom)GetProcAddress(wsock, "recvfrom");
  bind_orig = (PFN_bind)GetProcAddress(wsock, "bind");
  connect_orig = (PFN_connect)GetProcAddress(wsock, "connect");
  recv_orig = (PFN_recv)GetProcAddress(wsock, "recv");
  send_orig = (PFN_send)GetProcAddress(wsock, "send");
  select_orig = (PFN_select)GetProcAddress(wsock, "select");
  setsockopt_orig = (PFN_setsockopt)GetProcAddress(wsock, "setsockopt");
  ioctlsocket_orig = (PFN_ioctlsocket)GetProcAddress(wsock, "ioctlsocket");


  DetourTransactionBegin();
  DetourUpdateThread(GetCurrentThread());

  DetourAttach(&(PVOID&)WSAAsyncSelect_orig, WSAAsyncSelect_wrap);
  DetourAttach(&(PVOID&)listen_orig, listen_wrap);
  DetourAttach(&(PVOID&)accept_orig, accept_wrap);
  DetourAttach(&(PVOID&)shutdown_orig, shutdown_wrap);
  DetourAttach(&(PVOID&)socket_orig, socket_wrap);
  DetourAttach(&(PVOID&)closesocket_orig, closesocket_wrap);
  DetourAttach(&(PVOID&)sendto_orig, sendto_wrap);
  DetourAttach(&(PVOID&)recvfrom_orig, recvfrom_wrap);
  DetourAttach(&(PVOID&)bind_orig, bind_wrap);
  DetourAttach(&(PVOID&)connect_orig, connect_wrap);
  DetourAttach(&(PVOID&)recv_orig, recv_wrap);
  DetourAttach(&(PVOID&)send_orig, send_wrap);
  DetourAttach(&(PVOID&)select_orig, select_wrap);
  DetourAttach(&(PVOID&)setsockopt_orig, setsockopt_wrap);
  DetourAttach(&(PVOID&)ioctlsocket_orig, ioctlsocket_wrap);

  DetourTransactionCommit();
}