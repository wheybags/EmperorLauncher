#include <windows.h>
#include <detours.h>
#include "HookD3D7.hpp"
#include "PatchD3D7ResolutionLimit.hpp"
#include "GameExeImports.hpp"
#include "DoCdCheck.hpp"
#include "PatchDebugLog.hpp"
#include "PatchRegistry.hpp"
#include "PatchWindowManagement.hpp"
#include "WrapWinsock.hpp"
#include "Log.hpp"
#include "PatchWol.hpp"
#include "Error.hpp"


int (WINAPI* TrueShowCursor)(BOOL bShow) = ShowCursor;
VOID(WINAPI* OutputDebugStringAReal) (_In_opt_ LPCSTR lpOutputString) = OutputDebugStringA;
VOID(WINAPI* OutputDebugStringWReal) (_In_opt_ LPCWSTR lpOutputString) = OutputDebugStringW;


VOID WINAPI OutputDebugStringAWrap(_In_opt_ LPCSTR lpOutputString)
{
  // intel gpu drivers spam this for some reason
  if (lpOutputString && strncmp(lpOutputString, "Peiran message result", 21) == 0)
    return;

  Log("OutputDebugStringA: %s", lpOutputString);
  OutputDebugStringAReal(lpOutputString);
}

VOID WINAPI OutputDebugStringWWrap(_In_opt_ LPCWSTR lpOutputString)
{
  // intel gpu drivers spam this for some reason
  if (lpOutputString && wcsncmp(lpOutputString, L"Peiran message result", 21) == 0)
    return;

  Log("OutputDebugStringW: %S", lpOutputString);
  OutputDebugStringWReal(lpOutputString);
}


long count = 0;
int WINAPI FakeShowCursor(BOOL bShow)
{
  if (bShow)
    return InterlockedIncrement(&count);
  else
    return InterlockedDecrement(&count);
}


void setupConsole()
{
  if (AttachConsole(ATTACH_PARENT_PROCESS))
  {
    (void)freopen("CONIN$", "rb", stdin);
    (void)freopen("CONOUT$", "wb", stdout);
    (void)freopen("CONOUT$", "wb", stderr);
  }
}


void runHooks()
{
  //MessageBoxA(nullptr, "AAAAA", "AAAA", 0);

  DWORD doFullscreen = 0;
  {
    DWORD size = sizeof(DWORD);
    HRESULT result = RegGetValueA(HKEY_CURRENT_USER, "Software\\WestwoodRedirect\\Emperor\\LauncherCustomSettings", "DoFullscreen", RRF_RT_REG_DWORD, nullptr, &doFullscreen, &size);
  }

  std::string serverAddress;
  {
    DWORD buffSize = 1024;
    char buff[1024];
    HRESULT result = RegGetValueA(HKEY_CURRENT_USER, "Software\\WestwoodRedirect\\Emperor\\LauncherCustomSettings", "ServerAddress", RRF_RT_REG_SZ, nullptr, &buff, &buffSize);
    release_assert(SUCCEEDED(result));
    serverAddress = buff;
  }

  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);

  setupConsole();

  DetourTransactionBegin();
  DetourUpdateThread(GetCurrentThread());
  DetourAttach(&(PVOID&)OutputDebugStringAReal, OutputDebugStringAWrap);
  DetourAttach(&(PVOID&)OutputDebugStringWReal, OutputDebugStringWWrap);
  //DetourAttach(&(PVOID&)TrueShowCursor, FakeShowCursor);
  DetourTransactionCommit();

  patchDebugLog();
  HookD3D7();
  patchCdCheck();
  patchRedirectRegistry();
  patchD3D7ResolutionLimit();
  patchWindowManagement(doFullscreen);

  //wrapWinsockWithLogging();

  if (!serverAddress.empty())
    patchWolAsClient(serverAddress);
  else
    patchWolAsServer();
}


// defer all our custom setup to just before WinMain rather than directly in DllMain, because there are a
// lot of restrictions on what you're allowed to do inside DllMain
PFN_WinMain WinMainOrig = GameExeWinMain;
int __stdcall WinMainPatched(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
  runHooks();
  return WinMainOrig(hInstance, hPrevInstance, lpCmdLine, nShowCmd);
}


__declspec(dllexport) BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved)
{
  if (DetourIsHelperProcess()) {
    return TRUE;
  }

  if (dwReason == DLL_PROCESS_ATTACH)
  {
    DetourRestoreAfterWith();

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&)WinMainOrig, WinMainPatched);
    DetourTransactionCommit();
  }

  return TRUE;
}