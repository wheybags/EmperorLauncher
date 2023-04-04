#include <Windows.h>
#include <detours.h>
#include <cstdio>
#include <string>

#define release_assert(X) if (!(X)) abort()


HANDLE globalCommsFileMappingHandle = nullptr;
HANDLE globalCommsHandleMutex = nullptr;

const char* globalCommsHandleMutexGuid = "48BC11BD-C4D7-466b-8A31-C6ABBAD47B3E";
const char* globalCommsEventNameGuid = "D6E7FC97-64F9-4d28-B52C-754EDF721C6F";
const char* decryptedEmperorDatData = "UIDATA,3DDATA,MAPS";


HANDLE closeGlobalCommsHandle()
{
  HANDLE result; // eax

  if (globalCommsFileMappingHandle)
  {
    CloseHandle(globalCommsFileMappingHandle);
    globalCommsFileMappingHandle = 0;
  }
  result = globalCommsHandleMutex;
  if (globalCommsHandleMutex)
  {
    result = (HANDLE)CloseHandle(globalCommsHandleMutex);
    globalCommsHandleMutex = 0;
  }
  return result;
}

void createGlobalCommsFileMappingHandle()
{
  DWORD emperorDatSize = strlen(decryptedEmperorDatData) + 1;
  HANDLE handleTemp; // eax
  struct _SECURITY_ATTRIBUTES FileMappingAttributes; // [esp+8h] [ebp-28h] BYREF
  

  closeGlobalCommsHandle();
  globalCommsHandleMutex = 0;
  globalCommsFileMappingHandle = 0;
  globalCommsHandleMutex = CreateMutexA(0, 0, globalCommsHandleMutexGuid);
  if (globalCommsHandleMutex && GetLastError() != ERROR_ALREADY_EXISTS)
  {
    //QQQSomeStruct2::constructor(&emperorDatPath, aEmperor_dat, 0);
    //if (QQQSomeStruct2::accessible(&emperorDatPath, 0))
    {
      //emperorDatSize = QQQSomeStruct2::getFileSize(&emperorDatPath);
      FileMappingAttributes = {};
      FileMappingAttributes.nLength = 12;
      FileMappingAttributes.lpSecurityDescriptor = 0;
      FileMappingAttributes.bInheritHandle = 1;
      handleTemp = CreateFileMappingA((HANDLE)0xFFFFFFFF, &FileMappingAttributes, 4u, 0, emperorDatSize, 0);
      globalCommsFileMappingHandle = handleTemp;
      if (!handleTemp)
      {
      LABEL_7:
        CloseHandle(handleTemp);
        globalCommsFileMappingHandle = 0;
        goto LABEL_8;
      }
      if (GetLastError() == ERROR_ALREADY_EXISTS)
      {
        handleTemp = globalCommsFileMappingHandle;
        goto LABEL_7;
      }
    }
  LABEL_8:
    (void)0;
    //std::ios_base::~ios_base((std::ios_base*)&emperorDatPath);
  }
}

struct ProcessRunData
{
  char maybeLaunchDir[256];
  char exeName[256];
  char maybeLaunchArgs[256];
  HANDLE processHandle;
  DWORD processId;
  HANDLE processHThread;
  DWORD processThreadId;
};


char __cdecl runCommand(ProcessRunData* gameRunData, bool hook)
{
  DWORD lastError; // eax
  DWORD dwProcessId; // edx
  HANDLE hThread; // eax
  DWORD dwThreadId; // ecx
  struct _PROCESS_INFORMATION ProcessInformation; // [esp+10h] [ebp-358h] BYREF
  struct _STARTUPINFOA StartupInfo; // [esp+20h] [ebp-348h] BYREF
  CHAR CommandLine[516]; // [esp+64h] [ebp-304h] BYREF
  CHAR Buffer[256]; // [esp+268h] [ebp-100h] BYREF

  memset(&StartupInfo, 0, sizeof(StartupInfo));
  memset(CommandLine, 0, 512u);
  CommandLine[512] = 0;
  StartupInfo.cb = sizeof(_STARTUPINFOA);
  strcpy(CommandLine, gameRunData->exeName);
  strcat(CommandLine, gameRunData->maybeLaunchArgs);


  BOOL runResult = FALSE;
  if (hook)
  {
    char hookDllPath[MAX_PATH + 1];
    SetLastError(0);
    GetModuleFileNameA(nullptr, hookDllPath, MAX_PATH + 1);
    release_assert(GetLastError() == 0);

    int lastSlash = -1;
    for (int i = 0; hookDllPath[i] != '\0'; i++)
    {
      if (hookDllPath[i] == '\\')
        lastSlash = i;
    }
    release_assert(lastSlash != -1);

    hookDllPath[lastSlash + 1] = 0;
    strcat(hookDllPath, "EmperorHooks.dll");

    runResult = DetourCreateProcessWithDllA(0, CommandLine, 0, 0, 1, 0, 0, 0, &StartupInfo, &ProcessInformation, hookDllPath, nullptr);
  }
  else
  {
    runResult = CreateProcessA(0, CommandLine, 0, 0, 1, 0, 0, 0, &StartupInfo, &ProcessInformation);
  }

  if (!runResult)
  {
    lastError = GetLastError();
    FormatMessageA(0x1000u, 0, lastError, 0x400u, Buffer, 0x100u, 0);
    release_assert(false);
  }
  dwProcessId = ProcessInformation.dwProcessId;
  hThread = ProcessInformation.hThread;
  gameRunData->processHandle = ProcessInformation.hProcess;
  dwThreadId = ProcessInformation.dwThreadId;
  gameRunData->processId = dwProcessId;
  gameRunData->processHThread = hThread;
  gameRunData->processThreadId = dwThreadId;
  return 1;
}

void __cdecl sendEmperorDatDataToGameProcessViaAnonymousMapping(HANDLE processHandle, DWORD idThread)
{
  void* commsMapping = MapViewOfFileEx(globalCommsFileMappingHandle, 0xF001Fu, 0, 0, 0, 0);
  release_assert(commsMapping);
  memcpy(commsMapping, decryptedEmperorDatData, strlen(decryptedEmperorDatData) + 1);
  UnmapViewOfFile(commsMapping);

  HANDLE gameReadyEvent = CreateEventA(0, 0, 0, globalCommsEventNameGuid);
  release_assert(gameReadyEvent && GetLastError() != ERROR_ALREADY_EXISTS);

  HANDLE Handles[2];
  Handles[0] = gameReadyEvent;
  Handles[1] = processHandle;
  if (!WaitForMultipleObjects(2u, Handles, 0, 0x493E0u) && globalCommsFileMappingHandle)
    PostThreadMessageA(idThread, 0xBEEFu, 0, (LPARAM)globalCommsFileMappingHandle);
  CloseHandle(gameReadyEvent);
}

void waitForExit(ProcessRunData* gameRunData, DWORD* exitCode)
{
  WaitForSingleObject(gameRunData->processHandle, 0xFFFFFFFF);
  if (exitCode)
    *exitCode = -1;
  if (exitCode)
    GetExitCodeProcess(gameRunData->processHandle, exitCode);
}


void writeGraphicsSettings(int screenWidth, int screenHeight)
{
  HKEY key = nullptr;
  HRESULT result = RegCreateKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Westwood\\Emperor\\Options\\Graphics", 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr);
  if (result != S_OK)
    result = RegCreateKeyExA(HKEY_CURRENT_USER, "SOFTWARE\\Classes\\VirtualStore\\MACHINE\\SOFTWARE\\WOW6432Node\\Westwood\\Emperor\\Options\\Graphics", 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr);
  release_assert(result == S_OK && key);

  auto setValue = [&](const char* valueName, const std::string& value)
  {
    release_assert(RegSetKeyValueA(key, nullptr, valueName, REG_SZ, value.c_str(), DWORD(value.size() + 1)) == ERROR_SUCCESS);
  };

  setValue("ScreenWidth", std::to_string(screenWidth));
  setValue("ScreenHeight", std::to_string(screenHeight));

  // Just copied from the registry after cranking all settings in the in game settings dialogue
  setValue("GraphicsLOD", "4");
  setValue("ColorDepth", "32");
  setValue("Shadows", "1");
  setValue("ModelLOD", "2");
  setValue("TextureLOD", "0");
  setValue("TerrainLOD", "2");
  setValue("EffectLOD", "2");
  setValue("MultiTexture", "1");
  setValue("HardwareTL", "1");
  setValue("AltDevice", "0");
  setValue("ShadowQuality", "2");
  setValue("LimitFrameRate", "0");
  setValue("LimitTo16BitTex", "0");

  RegCloseKey(key);
}

void getMainMonitorDimensions(int& screenWidth, int& screenHeight)
{
  const POINT ptZero = { 0, 0 };
  HMONITOR monitor = MonitorFromPoint(ptZero, MONITOR_DEFAULTTOPRIMARY);
  MONITORINFO info;
  info.cbSize = sizeof(MONITORINFO);
  GetMonitorInfo(monitor, &info);
  screenWidth = info.rcMonitor.right - info.rcMonitor.left;
  screenHeight = info.rcMonitor.bottom - info.rcMonitor.top;
}


int main(int argc, char** argv)
{
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);

  int screenWidth = 0;
  int screenHeight = 0;
  getMainMonitorDimensions(screenWidth, screenHeight);

  // Lots of stuff in the game assumes a 4:3 screen ratio - I tried to patch this out but I couldn't figure it out for now :(
  int adjustedScreenWidth = int(float(screenHeight) * 4.0f / 3.0f);

  writeGraphicsSettings(adjustedScreenWidth, screenHeight);

  bool hook = true;
  for (int i = 0; i < argc; i++)
  {
    if (strcmp(argv[i], "--no-hook") == 0)
      hook = false;
  }

  createGlobalCommsFileMappingHandle();

  ProcessRunData runGameData = {};
  strcat(runGameData.exeName, "Game.exe");
  strcat(runGameData.maybeLaunchArgs, " -w"); // we need windowed mode - higher resolutions don't work in fullscreen
  runCommand(&runGameData, hook);

  sendEmperorDatDataToGameProcessViaAnonymousMapping(runGameData.processHandle, runGameData.processThreadId);

  DWORD ExitCode = 0;
  waitForExit(&runGameData, &ExitCode);

  closeGlobalCommsHandle();

  return ExitCode;
}
