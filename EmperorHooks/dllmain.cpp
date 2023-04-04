#include "pch.h"
#include <windows.h>
#include <detours.h>
#include <cstdint>
#include <cstdio>
#include <io.h>
#include <fcntl.h>
#include <intrin.h>
#include "HookD3D7.hpp"
#include "PatchD3D7ResolutionLimit.hpp"


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

bool heuristicIsThisAString(const char* maybeFmt)
{
  __try
  {
    for (int i = 0; i < 5; i++)
    {
      char ch = maybeFmt[i];
      if (ch == '\0')
        return false;

      bool isAscii = ch == '\t' || ch == '\r' || ch == '\n' || (ch >= ' ' && ch <= '~');
      if (!isAscii)
        return false;
    }

    return true;
  }
  __except (EXCEPTION_EXECUTE_HANDLER)
  {
    return false;
  }
}

// The problem with this function is that it's not one function, it's many functions.
// The original binary for this function is just a single byte ret instruction.
// My guess is that in the original source code, there were a bunch of debugging functions
// that were behind #ifdef DEBUG, and in release builds they were replaced with empty functions,
// also things like empty virtual functions to satisfy inheritance. They all end up compiling to
// a single ret, and then the linker says "oh look, I have duplicated functions" and merges them
// all. Unfortunatley, one of these empty functions (actually, I think, two of them) is the 
// debug logger. So in order to get debug logs, we need to use some heuristics on our parameters
// (or annotate every call site :s).
void MySometimesLogFunc(uint32_t firstArg, ...)
{
  static bool firstCall = true;
  if (firstCall)
  {
    firstCall = false;
    return;
  }

  char* fmt = nullptr;
  va_list args = nullptr;
  bool doFirstArg = false;

  if (firstArg > 1024)
  {
    fmt = (char*)firstArg;
    va_start(args, firstArg);
  }
  else
  {
    // Some logging calls have a different signature, with an integer before the format string.
    // I suspect these were some kind of tagged log call, so you could filter on the type tag enum.
    doFirstArg = true;
    va_start(args, firstArg);
    fmt = va_arg(args, char*);
  }

  if (fmt && heuristicIsThisAString(fmt))
  {
    if (doFirstArg)
      printf("%d: ", firstArg);

    vprintf(fmt, args);
    puts("");
  }
  
  if (args)
    va_end(args);
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

void (*EmptyFuncSometimesLog)(...) = (void (*)(...))0x401CB0;

bool isBorderlessWindowed = true;

HWND* const mainWindowHandleP = (HWND*)0x007D75A8;
uint8_t* gDoQuitP = (uint8_t*)0x007D75AC;

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
    SetWindowPos(hWnd, HWND_TOPMOST, GetSystemMetrics(SM_CXSCREEN) / 2 - useWidth / 2, 0, useWidth, useHeight, SWP_SHOWWINDOW);
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


bool focus = true;
uint64_t lastFocusCheckTick = 0;

int __stdcall wndProcDuneIII(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
  int ret = wndProcDuneIIIOriginal(hWnd, Msg, wParam, lParam);

  if (hWnd == *mainWindowHandleP && Msg == WM_ACTIVATEAPP)
  {
    if (wParam)
    {
      printf("GAINED FOCUS\n");
      focus = true;
    }
    else
    {
      printf("LOST FOCUS\n");
      focus = false;
      SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
    }
  }

  // We do this repeatedly because if we just do it once the taskbar sometimes stays on top. Not sure why, but this fixes it.
  if (GetTickCount64() - lastFocusCheckTick >= 1000 * 5)
  {
    lastFocusCheckTick = GetTickCount64();

    if (focus && *mainWindowHandleP && *mainWindowHandleP != INVALID_HANDLE_VALUE)
      SetWindowPos(*mainWindowHandleP, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW);
  }

  return ret;
}


__declspec(dllexport) BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved)
{
  if (DetourIsHelperProcess()) {
    return TRUE;
  }

  if (dwReason == DLL_PROCESS_ATTACH) 
  {
    DetourRestoreAfterWith();

    MessageBoxA(nullptr, "AAA", "AAA", 0);
    setupConsole();

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&)OutputDebugStringAReal, OutputDebugStringAWrap);
    DetourAttach(&(PVOID&)OutputDebugStringWReal, OutputDebugStringWWrap);
    DetourAttach(&(PVOID&)TrueShowCursor, FakeShowCursor);
    DetourAttach(&(PVOID&)EmptyFuncSometimesLog, MySometimesLogFunc);
    DetourAttach(&(PVOID&)setWindowStyleAndDrainMessagesOriginal, setWindowStyleAndDrainMessages);
    DetourAttach(&(PVOID&)wndProcDuneIIIOriginal, wndProcDuneIII);
    HookD3D7();
    DetourTransactionCommit();

    patchD3D7ResolutionLimit();
  }

  return TRUE;
}