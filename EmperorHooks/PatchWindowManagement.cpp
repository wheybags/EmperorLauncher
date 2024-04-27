#include "pch.h"
#include "GameExeImports.hpp"
#include <stdio.h>
#include <detours.h>
#include "Log.hpp"
#include <thread>

static volatile bool focus = true;
static bool emperorLauncherDoFullscreen = 1;
static volatile HWND backgroundWindowHandle = nullptr;
static volatile DWORD backgroundWindowThreadId = 0;


PFN_wndProcDuneIII wndProcDuneIIIOrig = wndProcDuneIII;
int __stdcall wndProcDuneIIIPatched(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
  if (emperorLauncherDoFullscreen)
  {
    // The original wndProc has code that sets up + tears down the renderer on focus gain / loss
    // This is not necessary on modern windows. Also, this was causing problems with nesting the
    // game window inside the black background window, as the game window itself didn't get those
    // messages, so the renderer was never enabled.
    // This function force runs the setup code once, when it receives its first event. Theoretically,
    // we could also block forwarding WM_ACTIVATEAPP messages. That seems to almost work, but there
    // are some weird issues with input after alt-tabbing in and out. So, we just force the event once.
    // We also forward in WM_ACTIVATEAPP events from the background / parent window

    static bool doOnce = false;
    if (!doOnce)
    {
      wndProcDuneIIIOrig(hWnd, WM_ACTIVATEAPP, TRUE, 0);
      doOnce = true;
    }
  }

  return wndProcDuneIIIOrig(hWnd, Msg, wParam, lParam);
}

PFN_setWindowStyleAndDrainMessages setWindowStyleAndDrainMessagesOrig = setWindowStyleAndDrainMessages;
BOOL __cdecl setWindowStyleAndDrainMessagesPatched(HWND hWnd, int width, int height, char windowedMode)
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
      if (msg.message == WM_QUIT && (msg.hwnd == mainWindowHandle || (HWND)msg.wParam == mainWindowHandle))
        gDoQuit = 1;
      TranslateMessage(&msg);
      DispatchMessageA(&msg);
    }

    SetForegroundWindow(hWnd);

    return result;
  }
  else
  {
    BOOL retval = setWindowStyleAndDrainMessagesOrig(hWnd, width, height, windowedMode);

    int left = GetSystemMetrics(SM_CXSCREEN) / 2 - width / 2;
    int top = GetSystemMetrics(SM_CYSCREEN) / 2 - height / 2;
    SetWindowPos(hWnd, nullptr, left, top, width, height, SWP_SHOWWINDOW);

    return retval;
  }
}

LRESULT __stdcall backgroundWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
  if (Msg == WM_ACTIVATEAPP)
  {
    Log("GOT WM_ACTIVATEAPP %d\n", wParam);
  }

  // we need to forward these events to the game window or input gets screwy, see comment in wndProcDuneIIIPatched()
  if (mainWindowHandle && Msg == WM_ACTIVATEAPP)
  {
    SetFocus(mainWindowHandle);
    Log("FORWARD WM_ACTIVATEAPP %d\n", wParam);
    SendMessageA(mainWindowHandle, Msg, wParam, lParam);
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

void captureCursorLoop()
{
  while (true)
  {
    Sleep(1000 * 1);

    if (!mainWindowHandle)
      continue;

    if (!emperorLauncherDoFullscreen)
    {
      if (mainWindowHandle != GetForegroundWindow())
        continue;
    }

    RECT rect = {};
    if (!GetClientRect(mainWindowHandle, &rect))
      continue;

    POINT topLeft = { 0, 0 };
    if (!ClientToScreen(mainWindowHandle, &topLeft))
      continue;

    rect.left += topLeft.x;
    rect.right += topLeft.x;
    rect.top += topLeft.y;
    rect.bottom += topLeft.y;

    rect.left += 10;
    rect.top += 10;
    rect.right -= 10;
    rect.bottom -= 10;

    if (!IsDebuggerPresent())
      ClipCursor(&rect);
  }
}

void patchWindowManagement(bool doFullscreen)
{
  DetourTransactionBegin();
  DetourUpdateThread(GetCurrentThread());
  DetourAttach(&(PVOID&)wndProcDuneIIIOrig, wndProcDuneIIIPatched);
  DetourAttach(&(PVOID&)setWindowStyleAndDrainMessagesOrig, setWindowStyleAndDrainMessagesPatched);
  DetourTransactionCommit();

  emperorLauncherDoFullscreen = doFullscreen;

  if (emperorLauncherDoFullscreen)
  {
    createBackgroundWindow();
    std::thread([]() { backgroundWindowLoop(); }).detach();
  }

  std::thread([]() { captureCursorLoop(); }).detach();
}