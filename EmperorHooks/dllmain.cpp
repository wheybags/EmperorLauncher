#include "pch.h"
#include <windows.h>
#include <detours.h>
#include <cstdint>
#include <cstdio>
#include <io.h>
#include <fcntl.h>
#include <intrin.h>
#include <thread>
#include "HookD3D7.hpp"
#include "PatchD3D7ResolutionLimit.hpp"
#include "GameExeImports.hpp"
#include "DoCdCheck.hpp"
#include "PatchDebugLog.hpp"
#include "PatchSettings.hpp"
#include "PatchLan.hpp"
#include "PatchWndProcDuneIII.hpp"
#include <string_view>
#include <winternl.h>
#include <Ntstatus.h>


int (WINAPI* TrueShowCursor)(BOOL bShow) = ShowCursor;
VOID(WINAPI* OutputDebugStringAReal) (_In_opt_ LPCSTR lpOutputString) = OutputDebugStringA;
VOID(WINAPI* OutputDebugStringWReal) (_In_opt_ LPCWSTR lpOutputString) = OutputDebugStringW;


VOID WINAPI OutputDebugStringAWrap(_In_opt_ LPCSTR lpOutputString)
{
  printf("OutputDebugStringA: %s", lpOutputString);
  OutputDebugStringAReal(lpOutputString);
}

VOID WINAPI OutputDebugStringWWrap(_In_opt_ LPCWSTR lpOutputString)
{
  wprintf(L"OutputDebugStringW: %s", lpOutputString);
  OutputDebugStringWReal(lpOutputString);
}


long count = 0;
int WINAPI FakeShowCursor(BOOL bShow)
{
  if (bShow)
    InterlockedIncrement(&count);
  else
    InterlockedDecrement(&count);
  return count;
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


volatile HWND backgroundWindowHandle = nullptr;
volatile DWORD backgroundWindowThreadId = 0;
volatile bool focus = true;
DWORD emperorLauncherDoFullscreen = 1;


void myClipCursor(const RECT* rect)
{
  if (!IsDebuggerPresent())
    ClipCursor(rect);
}


typedef BOOL(__cdecl* SetWindowStyleAndDrainMessagesFuncType)(HWND hWnd, int width, int height, char windowedMode);
SetWindowStyleAndDrainMessagesFuncType setWindowStyleAndDrainMessagesOriginal = (SetWindowStyleAndDrainMessagesFuncType)0x004A7260;
BOOL __cdecl setWindowStyleAndDrainMessages(HWND hWnd, int width, int height, char windowedMode)
{
  if (emperorLauncherDoFullscreen)
  {
    SetWindowLongA(hWnd, GWL_STYLE, WS_POPUP);
    SetParent(hWnd, backgroundWindowHandle);

    int left = GetSystemMetrics(SM_CXSCREEN) / 2 - width / 2;
    SetWindowPos(hWnd, nullptr, left, 0, width, height, SWP_SHOWWINDOW);

    BOOL result;
    MSG msg;
    for (result = PeekMessageA(&msg, 0, 0, 0, 0); result; result = PeekMessageA(&msg, 0, 0, 0, 0))
    {
      GetMessageA(&msg, 0, 0, 0);
      if (msg.message == WM_QUIT && (msg.hwnd == *mainWindowHandleP || (HWND)msg.wParam == *mainWindowHandleP))
        *gDoQuitP = 1;
      TranslateMessage(&msg);
      DispatchMessageA(&msg);
    }

    SetForegroundWindow(hWnd);

    return result;
  }
  else
  {
    BOOL retval = setWindowStyleAndDrainMessagesOriginal(hWnd, width, height, windowedMode);

    int left = GetSystemMetrics(SM_CXSCREEN) / 2 - width / 2;
    int top = GetSystemMetrics(SM_CYSCREEN) / 2 - height / 2;
    SetWindowPos(hWnd, nullptr, left, top, width, height, SWP_SHOWWINDOW);

    return retval;
  }
}

LRESULT __stdcall backgroundWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
  // minimise on focus loss
  if (Msg == WM_ACTIVATEAPP && wParam == FALSE)
    ShowWindow(hWnd, SW_MINIMIZE);

  // we need to forward these events to the game window or input gets screwy, see comment in wndProcDuneIIIPatched()
  if (*mainWindowHandleP && Msg == WM_ACTIVATEAPP)
  {
    SetFocus(*mainWindowHandleP);
    wndProcDuneIIIPatched(*mainWindowHandleP, Msg, wParam, lParam);
  }

  return DefWindowProcA(hWnd, Msg, wParam, lParam);
}


void createBackgroundWindow()
{
  backgroundWindowThreadId = GetCurrentThreadId();

  const char CLASS_NAME[] = "DuneIII background";

  WNDCLASSA wc = { };
  wc.lpfnWndProc = backgroundWndProc;
  wc.hInstance = GetModuleHandleA(nullptr);
  wc.lpszClassName = CLASS_NAME;
  RegisterClassA(&wc);

  HWND temp = CreateWindowExA(
      0,//WS_EX_TOOLWINDOW,                              // Optional window styles.
      CLASS_NAME,                     // Window class
      nullptr,    // Window text
      WS_CLIPCHILDREN | WS_POPUP,            // Window style

      // Size and position
      CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,

      NULL,       // Parent window
      NULL,       // Menu
      wc.hInstance,  // Instance handle
      NULL        // Additional application data
  );

  ShowWindow(temp, SW_MAXIMIZE);

  HBRUSH black = CreateSolidBrush(RGB(0, 0, 0));
  SetClassLongPtrA(temp, GCLP_HBRBACKGROUND, (LONG_PTR)black);
  InvalidateRect(temp, nullptr, 1);

  backgroundWindowHandle = temp;
}

void backgroundWindowLoop()
{
  BOOL bRet;

  MSG msg;
  while ((bRet = GetMessage(&msg, backgroundWindowHandle, 0, 0)) != 0)
  {
    if (bRet == -1)
    {
      break;
    }
    else
    {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }
}

struct SYSTEM_PROCESS_ID_INFORMATION
{
  HANDLE ProcessId;
  UNICODE_STRING ImageName;
};

constexpr SYSTEM_INFORMATION_CLASS SystemProcessIdInformation = SYSTEM_INFORMATION_CLASS(0x58);

NTSTATUS GetProcessPathById(DWORD ProcessId, wchar_t** output)
{
  NTSTATUS status = S_OK;

  HMODULE ntdll = nullptr;
  NTSTATUS(*NtQuerySystemInformation)(SYSTEM_INFORMATION_CLASS SystemInformationClass, PVOID SystemInformation, ULONG SystemInformationLength, PULONG ReturnLength) = nullptr;

  SYSTEM_PROCESS_ID_INFORMATION spii = { (HANDLE)ProcessId, { .Length = 0, .MaximumLength = 4096, .Buffer = nullptr } };


  ntdll = LoadLibraryA("ntdll.dll");
  if (!ntdll)
  {
    status = GetLastError();
    goto error;
  }

  NtQuerySystemInformation = (NTSTATUS (*)(SYSTEM_INFORMATION_CLASS SystemInformationClass, PVOID SystemInformation, ULONG SystemInformationLength, PULONG ReturnLength))GetProcAddress(ntdll, "NtQuerySystemInformation");
  if (!NtQuerySystemInformation)
  {
    status = GetLastError();
    goto error;
  }

  spii.ImageName.Buffer = (PWSTR)LocalAlloc(LMEM_FIXED, spii.ImageName.MaximumLength);
  if (!spii.ImageName.Buffer)
  {
    status = GetLastError();
    goto error;
  }

  status = NtQuerySystemInformation(SystemProcessIdInformation, &spii, sizeof(spii), 0);
  if (status == STATUS_INFO_LENGTH_MISMATCH)
  {
    LocalFree(spii.ImageName.Buffer);
    spii.ImageName.Buffer = (PWSTR)LocalAlloc(LMEM_FIXED, spii.ImageName.MaximumLength);
    if (!spii.ImageName.Buffer)
    {
      status = GetLastError();
      goto error;
    }
  }

  if (status != S_OK)
    goto error;

  *output = spii.ImageName.Buffer;
  goto end;

error:
  if (spii.ImageName.Buffer)
    LocalFree(spii.ImageName.Buffer);

end:
  if (ntdll)
    FreeLibrary(ntdll);

  return status;
}


__declspec(dllexport) BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved)
{
  if (DetourIsHelperProcess()) {
    return TRUE;
  }

  if (dwReason == DLL_PROCESS_ATTACH)
  {
    DWORD size = sizeof(DWORD);
    HRESULT result = RegGetValueA(HKEY_CURRENT_USER, "Software\\WestwoodRedirect\\Emperor\\LauncherCustomSettings", "DoFullscreen", RRF_RT_REG_DWORD, nullptr, &emperorLauncherDoFullscreen, &size);

    DetourRestoreAfterWith();

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);

    setupConsole();

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&)OutputDebugStringAReal, OutputDebugStringAWrap);
    DetourAttach(&(PVOID&)OutputDebugStringWReal, OutputDebugStringWWrap);
    //DetourAttach(&(PVOID&)TrueShowCursor, FakeShowCursor);
    DetourAttach(&(PVOID&)setWindowStyleAndDrainMessagesOriginal, setWindowStyleAndDrainMessages);

    DetourAttach(&(PVOID&)doCdCheckOrig, doCdCheckPatched);
    DetourAttach(&(PVOID&)regSettingsOpenHkeyOrig, regSettingsOpenHkeyPatched);
    DetourAttach(&(PVOID&)wndProcDuneIIIOrig, wndProcDuneIIIPatched);
    HookD3D7();
    patchDebugLog();
    //patchLan();
    DetourTransactionCommit();

    patchD3D7ResolutionLimit();

    if (emperorLauncherDoFullscreen)
    {
      createBackgroundWindow();
      std::thread([]() { backgroundWindowLoop(); }).detach();
    }

    std::thread([]()
    {
      while (true)
      {
        Sleep(1000 * 1);

        if (!*mainWindowHandleP)
          continue;

        RECT rect = {};
        if (!GetClientRect(*mainWindowHandleP, &rect))
          continue;

        POINT topLeft = {0, 0};
        if (!ClientToScreen(*mainWindowHandleP, &topLeft))
          continue;

        rect.left += topLeft.x;
        rect.right += topLeft.x;
        rect.top += topLeft.y;
        rect.bottom += topLeft.y;

        rect.left += 10;
        rect.top += 10;
        rect.right -= 10;
        rect.bottom -= 10;

        myClipCursor(&rect);
      }
    }).detach();
  }

  return TRUE;
}