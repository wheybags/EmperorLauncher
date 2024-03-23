#include "pch.h"
#include "GameExeImports.hpp"
#include <stdio.h>

extern DWORD emperorLauncherDoFullscreen;


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
  else
  {
    // minimise on focus loss
    if (Msg == WM_ACTIVATEAPP && wParam == FALSE)
      ShowWindow(hWnd, SW_MINIMIZE);
  }

  return wndProcDuneIIIOrig(hWnd, Msg, wParam, lParam);
}