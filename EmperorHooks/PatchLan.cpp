#include "pch.h"
#include <winsock.h>
#include <Windows.h>
#include <string>
#include <vector>
#include <detours.h>
#include "GameExeImports.hpp"

std::string sockaddrToString(const sockaddr_in* addr)
{
  return "family: " + std::to_string(addr->sin_family) + " " + std::string(inet_ntoa(addr->sin_addr)) + ":" + std::to_string(ntohs(addr->sin_port));
}

std::string sockaddrToString(const sockaddr* addr)
{
  if (addr->sa_family != AF_INET)
    return "ERR: NOT IPV4";
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

std::vector<std::pair<in_addr, sockaddr_in>> connectionsMapping;
sockaddr_in serverAddress = {};


enum class ConnectionType
{
  Server,
  Client,
};
ConnectionType connectionType = ConnectionType::Server;

#define netprintf(...) printf(__VA_ARGS__)
//#define netprintf(...) do {} while (false)


int (PASCAL FAR* sendtoReal)(
  _In_ SOCKET s,
  _In_reads_bytes_(len) const char FAR* buf,
  _In_ int len,
  _In_ int flags,
  _In_reads_bytes_opt_(tolen) const struct sockaddr FAR* to,
  _In_ int tolen) = nullptr;


int PASCAL FAR sendtoWrap(
  _In_ SOCKET s,
  _In_reads_bytes_(len) const char FAR* buf,
  _In_ int len,
  _In_ int flags,
  _In_reads_bytes_opt_(tolen) const struct sockaddr FAR* to,
  _In_ int tolen)
{
  netprintf("send packet to %s\n", sockaddrToString(to).c_str());
  netprintf("    SOCKET %s\n", socketToString(s).c_str());


  if (connectionType == ConnectionType::Server && to)
  {
    const sockaddr_in* addr = (const sockaddr_in*)to;
    auto it = std::find_if(connectionsMapping.begin(), connectionsMapping.end(), [&](const std::pair<in_addr, sockaddr_in>& pair) { return pair.first.S_un.S_addr == addr->sin_addr.S_un.S_addr; });
    if (it != connectionsMapping.end())
    {
      sockaddr_in* realAddr = &it->second;

      netprintf("    REDIR to %s\n", sockaddrToString(realAddr).c_str());

      int ret = sendtoReal(s, buf, len, flags, (struct sockaddr*)realAddr, sizeof(sockaddr_in));
      if (ret == SOCKET_ERROR)
        netprintf("socket error: %d\n", WSAGetLastError());

      return ret;
    }
  }

  if (connectionType == ConnectionType::Client)
  {
    netprintf("    CLIENT REDIR to %s\n", sockaddrToString(&serverAddress).c_str());

    int ret = sendtoReal(s, buf, len, flags, (struct sockaddr*)&serverAddress, sizeof(sockaddr_in));
    if (ret == SOCKET_ERROR)
      netprintf("socket error: %d\n", WSAGetLastError());

    return ret;
  }


  return sendtoReal(s, buf, len, flags, to, tolen);
}


int (PASCAL FAR* recvfromReal)(
  _In_ SOCKET s,
  _Out_writes_bytes_to_(len, return) __out_data_source(NETWORK) char FAR* buf,
  _In_ int len,
  _In_ int flags,
  _Out_writes_bytes_to_opt_(*fromlen, *fromlen) struct sockaddr FAR* from,
  _Inout_opt_ int FAR* fromlen) = nullptr;

int PASCAL FAR recvfromWrap(
  _In_ SOCKET s,
  _Out_writes_bytes_to_(len, return) __out_data_source(NETWORK) char FAR* buf,
  _In_ int len,
  _In_ int flags,
  _Out_writes_bytes_to_opt_(*fromlen, *fromlen) struct sockaddr FAR* from,
  _Inout_opt_ int FAR* fromlen)
{
  int bytesReceived = recvfromReal(s, buf, len, flags, from, fromlen);
  netprintf("recv from real: %s\n", sockaddrToString(from).c_str());
  netprintf("    SOCKET %s\n", socketToString(s).c_str());

  if (connectionType == ConnectionType::Server)
  {
    sockaddr_in* from_in = (sockaddr_in*)from;

    auto it = std::find_if(connectionsMapping.begin(), connectionsMapping.end(), [&](const std::pair<in_addr, sockaddr_in>& pair)
      {
        return pair.second.sin_family == from_in->sin_family &&
          pair.second.sin_addr.S_un.S_addr == from_in->sin_addr.S_un.S_addr &&
          pair.second.sin_port == from_in->sin_port;
      });

    if (it != connectionsMapping.end())
    {
      sockaddr_in* realAddr = &it->second;
      netprintf("    REDIR to fake: %s\n", sockaddrToString(realAddr).c_str());
      *from_in = *realAddr;
    }
    else if (bytesReceived != SOCKET_ERROR && bytesReceived == 128)
    {
      in_addr* ip = (in_addr*)(buf + 0x2C);

      char ipString[32] = {};
      strcpy(ipString, inet_ntoa(*ip));

      netprintf("MAGIC PACKET!!! ip: %s, real ip: %s\n", ipString, sockaddrToString(from_in).c_str());

      connectionsMapping.emplace_back(*ip, *from_in);
      from_in->sin_addr = *ip;
      from_in->sin_port = htons(4927);
    }
  }

  return bytesReceived;
}

int (PASCAL FAR * bindReal)(
  _In_ SOCKET s,
  _In_reads_bytes_(namelen) const struct sockaddr FAR * addr,
  _In_ int namelen) = nullptr;

int PASCAL FAR bindWrap(
  _In_ SOCKET s,
  _In_reads_bytes_(namelen) const struct sockaddr FAR * addr,
  _In_ int namelen)
{

  netprintf("-------------------------------------------BIND %s\n", sockaddrToString(addr).c_str());
  int ret = bindReal(s, addr, namelen);
  return ret;
}


int (PASCAL FAR * setsockoptReal)(
  _In_ SOCKET s,
  _In_ int level,
  _In_ int optname,
  _In_reads_bytes_opt_(optlen) const char FAR * optval,
  _In_ int optlen) = nullptr;

int PASCAL FAR setsockoptWrap(
  _In_ SOCKET s,
  _In_ int level,
  _In_ int optname,
  _In_reads_bytes_opt_(optlen) const char FAR * optval,
  _In_ int optlen)
{
  netprintf("setsockopt %d %d\n", level, optname);
  return setsockoptReal(s, level, optname, optval, optlen);
}

// original is __thiscall, but we can't declare a thiscall func outside a class, so we fake it with fastcall and an unused edx param
int __fastcall SomeNetworkManager_SendLobbyMessageWrap(SomeNetworkManager * This, DWORD edx, __int16 a2, int a3, wchar_t* message_, char a5)
{
  printf("GOT CHAT MESSAGE %S\n", message_);

  std::wstring commandReply;

  std::wstring_view message(message_);
  if (message == L"/connect")
  {
    connectionType = ConnectionType::Client;
    connectionsMapping.clear();
    commandReply = L"connecting!";

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.S_un.S_addr = inet_addr("86.246.78.252");
    serverAddress.sin_port = htons(4927);
  }
  else if (message == L"/disconnect")
  {
    connectionType = ConnectionType::Server;
    connectionsMapping.clear();
    commandReply = L"disconnected!";
  }

  if (!commandReply.empty())
    return SomeNetworkManager_SendLobbyMessageOrig(This, a2, a3, (std::wstring(message_) + L": " + commandReply).data(), a5);

  return SomeNetworkManager_SendLobbyMessageOrig(This, a2, a3, message_, a5);
}

void patchLan()
{
  HMODULE wsock = LoadLibraryA("wsock32.dll");

  sendtoReal = (int (PASCAL FAR*)(
    _In_ SOCKET s,
    _In_reads_bytes_(len) const char FAR * buf,
    _In_ int len,
    _In_ int flags,
    _In_reads_bytes_opt_(tolen) const struct sockaddr FAR * to,
    _In_ int tolen)) GetProcAddress(wsock, "sendto");

  recvfromReal = (int (PASCAL FAR*)(_In_ SOCKET s,
    _Out_writes_bytes_to_(len, return) __out_data_source(NETWORK) char FAR* buf,
    _In_ int len,
    _In_ int flags,
    _Out_writes_bytes_to_opt_(*fromlen, *fromlen) struct sockaddr FAR* from,
    _Inout_opt_ int FAR* fromlen)) GetProcAddress(wsock, "recvfrom");

  bindReal = (int (PASCAL FAR*)(
    _In_ SOCKET s,
    _In_reads_bytes_(namelen) const struct sockaddr FAR * addr,
    _In_ int namelen)) GetProcAddress(wsock, "bind");

  setsockoptReal = (int (PASCAL FAR*)(
    _In_ SOCKET s,
    _In_ int level,
    _In_ int optname,
    _In_reads_bytes_opt_(optlen) const char FAR * optval,
    _In_ int optlen)) GetProcAddress(wsock, "setsockopt");


  DetourAttach(&(PVOID&)sendtoReal, sendtoWrap);
  DetourAttach(&(PVOID&)recvfromReal, recvfromWrap);
  DetourAttach(&(PVOID&)bindReal, bindWrap);
  DetourAttach(&(PVOID&)setsockoptReal, setsockoptWrap);
  DetourAttach(&(PVOID&)SomeNetworkManager_SendLobbyMessageOrig, SomeNetworkManager_SendLobbyMessageWrap);
}