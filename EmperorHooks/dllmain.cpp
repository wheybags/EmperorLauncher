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

void myClipCursor(const RECT* rect)
{
  if (!IsDebuggerPresent())
    ClipCursor(rect);
}


typedef BOOL(__cdecl* SetWindowStyleAndDrainMessagesFuncType)(HWND hWnd, int width, int height, char windowedMode);
SetWindowStyleAndDrainMessagesFuncType setWindowStyleAndDrainMessagesOriginal = (SetWindowStyleAndDrainMessagesFuncType)0x004A7260;
BOOL __cdecl setWindowStyleAndDrainMessages(HWND hWnd, int width, int height, char windowedMode)
{
  SetWindowLongA(hWnd, GWL_STYLE, WS_POPUP);
  int left = GetSystemMetrics(SM_CXSCREEN) / 2 - width / 2;
  SetWindowPos(hWnd, HWND_TOPMOST, left, 0, width, height, SWP_SHOWWINDOW);

  RECT rc = {};
  GetWindowRect(*mainWindowHandleP, &rc);
  myClipCursor(&rc);
  
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
  return result;
}

void backgroundWindow()
{
  backgroundWindowThreadId = GetCurrentThreadId();

  // Register the window class.
  const char CLASS_NAME[] = "DuneIII background";

  WNDCLASSA wc = { };
  wc.lpfnWndProc = DefWindowProcA;
  wc.hInstance = GetModuleHandleA(nullptr);
  wc.lpszClassName = CLASS_NAME;
  RegisterClassA(&wc);

  HWND temp = CreateWindowExA(
    WS_EX_TOOLWINDOW,                              // Optional window styles.
    CLASS_NAME,                     // Window class
    nullptr,    // Window text
    WS_POPUP,            // Window style

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

void windowSwitchHandlerLoop()
{
  while (!*mainWindowHandleP || !backgroundWindow)
    Sleep(10);

  HWND lastForeground = nullptr;
  bool force = true;


  while (true)
  {
    Sleep(1000);

    HWND foreground = GetForegroundWindow();

    DWORD dwProcId = 0;
    if (!GetWindowThreadProcessId(foreground, &dwProcId))
    {
      printf("GetWindowThreadProcessId failed %d\n", GetLastError());
    }
    else
    {
      wchar_t* processPath = nullptr; 
      NTSTATUS status = GetProcessPathById(dwProcId, &processPath);
      if (status != S_OK)
      {
        printf("GetProcessPathById failed %d\n", status);
      }
      else
      {
        //printf("fg %d %d %S !!!\n", DWORD(foreground), dwProcId, processPath);

        // sometimes during loading dwm.exe / WerFault.exe take over as the "Game.exe is not responding" window
        bool isDwmExe = std::wstring_view(processPath).ends_with(L"\\dwm.exe");
        bool isWerFaultExe = std::wstring_view(processPath).ends_with(L"\\WerFault.exe");

        LocalFree(processPath);

        if (isDwmExe || isWerFaultExe)
          continue;
      }
    }

    if (foreground == backgroundWindowHandle)
    {
      printf("bg was fg, fixing\n");
      ShowWindow(*mainWindowHandleP, SW_SHOW);
      SetForegroundWindow(*mainWindowHandleP);
      lastForeground = *mainWindowHandleP;
      Sleep(1000 * 3);
      continue;
    }
    else if ((foreground == *mainWindowHandleP && lastForeground != *mainWindowHandleP) || force)
    {
      printf("SHOW START\n");

      printf("    topmost bg\n");
      SetWindowPos(backgroundWindowHandle, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
      printf("    show bg\n");
      ShowWindow(backgroundWindowHandle, SW_SHOWMAXIMIZED);
      SetForegroundWindow(backgroundWindowHandle);

      //Sleep(1000 * 3);

      printf("    topmost main\n");
      SetWindowPos(*mainWindowHandleP, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW);
      printf("    show main\n");
      ShowWindow(*mainWindowHandleP, SW_SHOW);
      SetForegroundWindow(*mainWindowHandleP);

      //Sleep(1000 * 3);

      printf("SHOW DONE\n");
      force = false;
    }
    else if (foreground != *mainWindowHandleP && lastForeground == *mainWindowHandleP)
    {
      printf("HIDE\n");
      SetWindowPos(*mainWindowHandleP, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
      ShowWindow(*mainWindowHandleP, SW_MINIMIZE);

      SetWindowPos(backgroundWindowHandle, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
      ShowWindow(backgroundWindowHandle, SW_MINIMIZE);
    }

    lastForeground = foreground;
  }
}

__declspec(dllexport) BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved)
{
  if (DetourIsHelperProcess()) {
    return TRUE;
  }

  if (dwReason == DLL_PROCESS_ATTACH) 
  {
    DWORD doFullscreen = 1;
    DWORD size = sizeof(DWORD);
    HRESULT result = RegGetValueA(HKEY_CURRENT_USER, "Software\\WestwoodRedirect\\Emperor\\LauncherCustomSettings", "DoFullscreen", RRF_RT_REG_DWORD, nullptr, &doFullscreen, &size);

    DetourRestoreAfterWith();

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);

    setupConsole();

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&)OutputDebugStringAReal, OutputDebugStringAWrap);
    DetourAttach(&(PVOID&)OutputDebugStringWReal, OutputDebugStringWWrap);
    //DetourAttach(&(PVOID&)TrueShowCursor, FakeShowCursor);
    if (doFullscreen)
      DetourAttach(&(PVOID&)setWindowStyleAndDrainMessagesOriginal, setWindowStyleAndDrainMessages);

    DetourAttach(&(PVOID&)doCdCheckOrig, doCdCheckPatched);
    DetourAttach(&(PVOID&)regSettingsOpenHkeyOrig, regSettingsOpenHkeyPatched);
    HookD3D7();
    patchDebugLog();
    patchLan();
    DetourTransactionCommit();

    patchD3D7ResolutionLimit();

    if (doFullscreen)
    {
      std::thread([]() { backgroundWindow(); }).detach();
      std::thread([]() { windowSwitchHandlerLoop(); }).detach();
      std::thread([]()
      {
        while (true)
        {
          Sleep(1000 * 1);
          RECT rect;
          if (GetWindowRect(*mainWindowHandleP, &rect))
            myClipCursor(&rect);
        }
      }).detach();
    }
  }

  return TRUE;
}