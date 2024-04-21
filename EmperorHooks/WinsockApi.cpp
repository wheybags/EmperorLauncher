#include "pch.h"
#include "WinsockApi.hpp"

std::string sockaddrToString(const sockaddr_in* addr)
{
  return std::string(inet_ntoa(addr->sin_addr)) + ":" + std::to_string(ntohs(addr->sin_port));
}

std::string sockaddrToString(const sockaddr* addr)
{
  if (addr->sa_family == AF_UNSPEC)
    return "empty";

  if (addr->sa_family != AF_INET)
    return "ERR: NOT IPV4: " + std::to_string(addr->sa_family);

  return sockaddrToString((const sockaddr_in*)addr);
}

std::string socketToString(SOCKET s)
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

std::string wsaErrorToString(int err)
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