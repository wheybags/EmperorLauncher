#include "pch.h"
#include "GameExeImports.hpp"

int __stdcall wndProcDuneIIIPatched(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
  // The original wndProc has code that sets up + tears down the renderer on focus gain / loss
  // This is not necessary on modern windows. Also, this was causing problems with nesting the
  // game window inside the black background window, as the game window itself didn't get those
  // messages, so the renderer was never enabled.
  // This function runs the setup code once, when it receives its first event. We also block
  // sending the focus gain / loss events to make sure we never tear it down, or double init.
  // Other than that, we just forward to the original.

  static bool doOnce = false;
  if (!doOnce)
  {
    int v7 = 0;

    // copied from original handler for WM_ACTIVATEAPP
    somethingThatAffectsContextFree = 1;
    sub_4D66F0(&globalStuff);
    sub_477170(v7);
    GlobalStuff_someSetter(&globalStuff);
    ShowCursor(0);
    if (gContext)
      resetDevice(gContext); // note: maybe wrong signature
    maybeDontFreeContext = 1;
    dword_7D75B0 = GetTickCount();


    // copied from original handler for WM_ACTIVATE
    sub_4D66F0(&globalStuff);


    doOnce = true;
  }

  if (Msg == WM_ACTIVATE || Msg == WM_ACTIVATEAPP)
    return DefWindowProcA(hWnd, Msg, wParam, lParam);


  return wndProcDuneIIIOrig(hWnd, Msg, wParam, lParam);
}