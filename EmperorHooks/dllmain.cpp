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

bool isBorderlessWindowed = true;

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
  DWORD style; // ebp
  int useWidth; // esi
  int useHeight; // ebx
  BOOL hasWindow; // esi
  DWORD extendedStyle; // eax
  BOOL result; // eax
  struct tagRECT rc; // [esp+10h] [ebp-2Ch] BYREF
  struct tagMSG Msg; // [esp+20h] [ebp-1Ch] BYREF

  style = WS_POPUP;
  useWidth = GetSystemMetrics(SM_CXSCREEN);
  useHeight = GetSystemMetrics(SM_CYSCREEN);
  if (windowedMode)
  {
    useWidth = width;
    useHeight = height;
    if (!isBorderlessWindowed)
      style = WS_POPUPWINDOW | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_DLGFRAME;
  }
  SetWindowLongA(hWnd, GWL_STYLE, style);
  if (isBorderlessWindowed)
  {
    int left = GetSystemMetrics(SM_CXSCREEN) / 2 - useWidth / 2;
    SetWindowPos(hWnd, HWND_TOPMOST, left, 0, useWidth, useHeight, SWP_SHOWWINDOW);

    RECT rc = {};
    GetWindowRect(*mainWindowHandleP, &rc);
    myClipCursor(&rc);
  }
  else
  {
    SetRect(&rc, 0, 0, useWidth, useHeight);
    hasWindow = GetMenu(hWnd) != 0;
    extendedStyle = GetWindowLongA(hWnd, GWL_EXSTYLE);
    AdjustWindowRectEx(&rc, style, hasWindow, extendedStyle);
    MoveWindow(hWnd, 0, 0, rc.right - rc.left, rc.bottom - rc.top, 0);
    ShowWindow(hWnd, SW_SHOW);
    InvalidateRect(hWnd, 0, 0);
  }
  
  for (result = PeekMessageA(&Msg, 0, 0, 0, 0); result; result = PeekMessageA(&Msg, 0, 0, 0, 0))
  {
    GetMessageA(&Msg, 0, 0, 0);
    if (Msg.message == WM_QUIT && (Msg.hwnd == *mainWindowHandleP || (HWND)Msg.wParam == *mainWindowHandleP))
      *gDoQuitP = 1;
    TranslateMessage(&Msg);
    DispatchMessageA(&Msg);
  }
  return result;
}

typedef int(__stdcall* WndProcDuneIIIType)(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
WndProcDuneIIIType wndProcDuneIIIOriginal = (WndProcDuneIIIType)0x004A6560;


int __stdcall wndProcDuneIII(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
  int ret = wndProcDuneIIIOriginal(hWnd, Msg, wParam, lParam);


  if (hWnd == *mainWindowHandleP && Msg == WM_ACTIVATEAPP)
  {
    if (wParam || (!wParam && lParam == backgroundWindowThreadId))
    {
      printf("GAINED FOCUS\n");
      focus = true;
    }
    else
    {
      printf("LOST FOCUS\n");
      focus = false;
      SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
      ShowWindow(hWnd, SW_MINIMIZE);

      SetWindowPos(backgroundWindowHandle, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
      ShowWindow(backgroundWindowHandle, SW_MINIMIZE);
    }
  }

  static bool hadFocus = true;

  if (focus && !hadFocus)
  {
    RECT rc = {};
    GetWindowRect(*mainWindowHandleP, &rc);
    myClipCursor(&rc);

    std::thread([]()
    {
      printf("%p %p\n", *mainWindowHandleP, backgroundWindowHandle);
      Sleep(1000 * 2);
      ShowWindow(backgroundWindowHandle, SW_SHOWNOACTIVATE);
      Sleep(1000 * 2);
      SetWindowPos(backgroundWindowHandle, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
      Sleep(1000 * 2);
      SetWindowPos(*mainWindowHandleP, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW);
      ShowWindow(*mainWindowHandleP, SW_SHOW);
    }).detach();
  }

  hadFocus = focus;

  return ret;
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

__declspec(dllexport) BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved)
{
  if (DetourIsHelperProcess()) {
    return TRUE;
  }

  if (dwReason == DLL_PROCESS_ATTACH) 
  {
    DetourRestoreAfterWith();

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);

    MessageBoxA(nullptr, "AAA", "AAA", 0);
    setupConsole();

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&)OutputDebugStringAReal, OutputDebugStringAWrap);
    DetourAttach(&(PVOID&)OutputDebugStringWReal, OutputDebugStringWWrap);
    DetourAttach(&(PVOID&)TrueShowCursor, FakeShowCursor);
    DetourAttach(&(PVOID&)setWindowStyleAndDrainMessagesOriginal, setWindowStyleAndDrainMessages);
    DetourAttach(&(PVOID&)wndProcDuneIIIOriginal, wndProcDuneIII);
    DetourAttach(&(PVOID&)doCdCheckOrig, doCdCheckPatched);
    DetourAttach(&(PVOID&)regSettingsOpenHkeyOrig, regSettingsOpenHkeyPatched);
    HookD3D7();
    patchDebugLog();
    DetourTransactionCommit();

    patchD3D7ResolutionLimit();

    std::thread([]() { backgroundWindow(); }).detach();
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

  return TRUE;
}