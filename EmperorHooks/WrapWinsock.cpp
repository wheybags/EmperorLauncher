#include "pch.h"
#include <detours.h>
#include <winsock.h>
#include <string>
#include "Log.hpp"

#define netprintf(format, ...) Log("\033[33m" format "\033[0m", __VA_ARGS__)
//#define netprintf(...) do {} while (false)


static std::string sockaddrToString(const sockaddr_in* addr)
{
  return std::string(inet_ntoa(addr->sin_addr)) + ":" + std::to_string(ntohs(addr->sin_port));
}

static std::string sockaddrToString(const sockaddr* addr)
{
  if (addr->sa_family == AF_UNSPEC)
    return "empty";

  if (addr->sa_family != AF_INET)
    return "ERR: NOT IPV4: " + std::to_string(addr->sa_family);

  return sockaddrToString((const sockaddr_in*)addr);
}

static std::string socketToString(SOCKET s)
{
  if (s == INVALID_SOCKET)
    return "INVALID_SOCKET";

  sockaddr sa = {};
  int len = sizeof(sockaddr);

  if (getsockname(s, &sa, &len))
    return std::string("getsockname error ") + std::to_string(WSAGetLastError());

  int type = 0;
  int length = sizeof(int);
  getsockopt(s, SOL_SOCKET, SO_TYPE, (char*)&type, &length);

  return sockaddrToString(&sa) + " type: " + std::to_string(type);
}

static std::string wsaErrorToString(int err)
{
  switch (err)
  {
    case 0: return "SUCCESS";
    case WSAEINTR: return "WSAEINTR";
    case WSAEBADF: return "WSAEBADF";
    case WSAEACCES: return "WSAEACCES";
    case WSAEFAULT: return "WSAEFAULT";
    case WSAEINVAL: return "WSAEINVAL";
    case WSAEMFILE: return "WSAEMFILE";
    case WSAEWOULDBLOCK: return "WSAEWOULDBLOCK";
    case WSAEINPROGRESS: return "WSAEINPROGRESS";
    case WSAEALREADY: return "WSAEALREADY";
    case WSAENOTSOCK: return "WSAENOTSOCK";
    case WSAEDESTADDRREQ: return "WSAEDESTADDRREQ";
    case WSAEMSGSIZE: return "WSAEMSGSIZE";
    case WSAEPROTOTYPE: return "WSAEPROTOTYPE";
    case WSAENOPROTOOPT: return "WSAENOPROTOOPT";
    case WSAEPROTONOSUPPORT: return "WSAEPROTONOSUPPORT";
    case WSAESOCKTNOSUPPORT: return "WSAESOCKTNOSUPPORT";
    case WSAEOPNOTSUPP: return "WSAEOPNOTSUPP";
    case WSAEPFNOSUPPORT: return "WSAEPFNOSUPPORT";
    case WSAEAFNOSUPPORT: return "WSAEAFNOSUPPORT";
    case WSAEADDRINUSE: return "WSAEADDRINUSE";
    case WSAEADDRNOTAVAIL: return "WSAEADDRNOTAVAIL";
    case WSAENETDOWN: return "WSAENETDOWN";
    case WSAENETUNREACH: return "WSAENETUNREACH";
    case WSAENETRESET: return "WSAENETRESET";
    case WSAECONNABORTED: return "WSAECONNABORTED";
    case WSAECONNRESET: return "WSAECONNRESET";
    case WSAENOBUFS: return "WSAENOBUFS";
    case WSAEISCONN: return "WSAEISCONN";
    case WSAENOTCONN: return "WSAENOTCONN";
    case WSAESHUTDOWN: return "WSAESHUTDOWN";
    case WSAETOOMANYREFS: return "WSAETOOMANYREFS";
    case WSAETIMEDOUT: return "WSAETIMEDOUT";
    case WSAECONNREFUSED: return "WSAECONNREFUSED";
    case WSAELOOP: return "WSAELOOP";
    case WSAENAMETOOLONG: return "WSAENAMETOOLONG";
    case WSAEHOSTDOWN: return "WSAEHOSTDOWN";
    case WSAEHOSTUNREACH: return "WSAEHOSTUNREACH";
    case WSAENOTEMPTY: return "WSAENOTEMPTY";
    case WSAEPROCLIM: return "WSAEPROCLIM";
    case WSAEUSERS: return "WSAEUSERS";
    case WSAEDQUOT: return "WSAEDQUOT";
    case WSAESTALE: return "WSAESTALE";
    case WSAEREMOTE: return "WSAEREMOTE";
    case WSAEDISCON: return "WSAEDISCON";
    case WSASYSNOTREADY: return "WSASYSNOTREADY";
    case WSAVERNOTSUPPORTED: return "WSAVERNOTSUPPORTED";
    case WSANOTINITIALISED: return "WSANOTINITIALISED";
    case WSAHOST_NOT_FOUND: return "WSAHOST_NOT_FOUND";
    case WSATRY_AGAIN: return "WSATRY_AGAIN";
    case WSANO_RECOVERY: return "WSANO_RECOVERY";
    case WSANO_DATA: return "WSANO_DATA";
    case WSA_SECURE_HOST_NOT_FOUND: return "WSA_SECURE_HOST_NOT_FOUND";
    case WSA_IPSEC_NAME_POLICY_ERROR: return "WSA_IPSEC_NAME_POLICY_ERROR";
    default: return "UNKNOWN(" + std::to_string(err) + ")";
  }
}

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
// setsockopt
// shutdown                 wrapped 
// socket                   wrapped


typedef int (PASCAL* PFN_WSAAsyncSelect)(SOCKET s, HWND hWnd, u_int wMsg, long lEvent);
typedef int (PASCAL* PFN_listen)(SOCKET s, int backlog);
typedef SOCKET (PASCAL* PFN_accept)(SOCKET s, struct sockaddr* addr, int* addrlen);
typedef int (PASCAL* PFN_shutdown)(SOCKET s, int how);
typedef int (PASCAL* PFN_connect)(SOCKET s, const struct sockaddr* name, int namelen);
typedef SOCKET (PASCAL* PFN_socket)(int af, int type, int protocol);
typedef int (PASCAL* PFN_closesocket)(SOCKET s);
typedef int (PASCAL* PFN_sendto)(SOCKET s, const char* buf, int len, int flags, const struct sockaddr* to, int tolen);
typedef int (PASCAL* PFN_recvfrom)(SOCKET s, char* buf, int len, int flags, struct sockaddr* from, int* fromlen);
typedef int (PASCAL* PFN_bind)(SOCKET s, const struct sockaddr* addr, int namelen);
typedef int (PASCAL* PFN_recv)(SOCKET s, char* buf, int len, int flags);
typedef int (PASCAL* PFN_send)(SOCKET s, const char* buf, int len, int flags);
typedef int (PASCAL* PFN_select)(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, const struct timeval* timeout);

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

static int PASCAL select_wrap(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, const struct timeval* timeout)
{
  netprintf("select:\n");

  int ret = select_orig(nfds, readfds, writefds, exceptfds, timeout);
  int err = WSAGetLastError();

  netprintf("    -> %d %s\n", ret, wsaErrorToString(err).c_str());

  WSASetLastError(err);
  return ret;
}

void wrapWinsock()
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


  DetourTransactionCommit();
}