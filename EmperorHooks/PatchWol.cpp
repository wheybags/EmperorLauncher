#include "WolProxyBase.hpp"
#include "WolProxyServer.hpp"
#include "WolProxyClient.hpp"
#include "GameExeImports.hpp"
#include "Error.hpp"
#include <detours.h>


// Skips attempting to report results to the "gameres" (game results) server. Without this patch, it takes a considerable
// amount of time after every multiplayer match to time out, and the UI is broken / frozen during this time.
// original is __thiscall, but we can't declare a thiscall func outside a class, so we fake it with fastcall and an unused edx param
PFN_CWolWrapper_ReportResults CWolWrapper_ReportResults_Orig = CWolWrapper_ReportResults;
void __fastcall CWolWrapper_ReportResultsPatched(CWolWrapper* This, DWORD edx, int a1)
{}

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
    gameSpeed = ((WolProxyClient*)staticProxy)->getGameSpeedFromHost();


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

PFN_CPortUtil_Set_Port_Range CPortUtil_Set_Port_Range_Orig = CPortUtil_Set_Port_Range;
void CPortUtil_Set_Port_Range_Patched(unsigned __int16 port_h_min, unsigned __int16 port_h_max)
{
  portRange = { port_h_min, port_h_max };
  CPortUtil_Set_Port_Range_Orig(port_h_min, port_h_max);
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

void init(WolProxyBase* proxy)
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
  DetourAttach(&(PVOID&)CWolWrapper_ReportResults_Orig, CWolWrapper_ReportResultsPatched);
  DetourAttach(&(PVOID&)CNetworkAdmin_setFrameLimitOrig, CNetworkAdmin_setFrameLimitPatched);
  DetourAttach(&(PVOID&)CMangler_Pattern_Query_Orig, CMangler_Pattern_Query_Patched);
  DetourAttach(&(PVOID&)CPortUtil_Set_Port_Range_Orig, CPortUtil_Set_Port_Range_Patched);
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
  init(new WolProxyServer());
}

void patchWolAsClient(const std::string& serverAddr)
{
  proxylog("initialising as client, connecting to: %s\n", serverAddr.c_str());
  init(new WolProxyClient(serverAddr));
}