#include "pch.h"
#include <windows.h>
#include <detours.h>


int (WINAPI* TrueShowCursor)(BOOL bShow) = ShowCursor;

long count = 0;
int WINAPI FakeShowCursor(BOOL bShow)
{
  if (bShow)
    InterlockedIncrement(&count);
  else
    InterlockedDecrement(&count);
  return count;
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
    DetourAttach(&(PVOID&)TrueShowCursor, FakeShowCursor);
    DetourTransactionCommit();
  }
  else if (dwReason == DLL_PROCESS_DETACH) 
  {
    /*DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourDetach(&(PVOID&)TrueSleep, TimedSleep);
    DetourTransactionCommit();*/
  }
  return TRUE;
}