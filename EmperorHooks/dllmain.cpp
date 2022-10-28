#include "pch.h"
#include <windows.h>
#include <detours.h>
#include <cstdint>
#include <cstdio>
#include <io.h>
#include <fcntl.h>
#include <intrin.h>


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
    DetourAttach(&(PVOID&)TrueShowCursor, FakeShowCursor);
    DetourAttach(&(PVOID&)EmptyFuncSometimesLog, MySometimesLogFunc);
    DetourTransactionCommit();
  }

  return TRUE;
}