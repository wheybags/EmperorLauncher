#include <Windows.h>
#include <detours.h>
#include <cstdio>
#include <string>

#define release_assert(X) if (!(X)) do { MessageBoxA(nullptr, "assert failed", "assert failed", 0); abort(); } while (0)


HANDLE globalCommsFileMappingHandle = nullptr;
HANDLE globalCommsHandleMutex = nullptr;

const char* globalCommsHandleMutexGuid = "48BC11BD-C4D7-466b-8A31-C6ABBAD47B3E";
const char* globalCommsEventNameGuid = "D6E7FC97-64F9-4d28-B52C-754EDF721C6F";
const char* decryptedEmperorDatData = "UIDATA,3DDATA,MAPS";


void closeGlobalCommsHandle()
{
  if (globalCommsFileMappingHandle)
  {
    CloseHandle(globalCommsFileMappingHandle);
    globalCommsFileMappingHandle = nullptr;
  }

  if (globalCommsHandleMutex)
  {
    CloseHandle(globalCommsHandleMutex);
    globalCommsHandleMutex = nullptr;
  }
}

void createGlobalCommsFileMappingHandle()
{
  closeGlobalCommsHandle();

  SetLastError(0);
  globalCommsHandleMutex = CreateMutexA(0, 0, globalCommsHandleMutexGuid);
  release_assert(globalCommsHandleMutex && GetLastError() == 0);

  SECURITY_ATTRIBUTES attributes = {};
  attributes.nLength = 12;
  attributes.lpSecurityDescriptor = 0;
  attributes.bInheritHandle = 1;

  SetLastError(0);
  globalCommsFileMappingHandle = CreateFileMappingA((HANDLE)0xFFFFFFFF, &attributes, 4u, 0, strlen(decryptedEmperorDatData) + 1, 0);
  release_assert(globalCommsFileMappingHandle && GetLastError() == 0);
}


PROCESS_INFORMATION runGameExe()
{
  std::string hookDllPath;
  {
    hookDllPath.resize(10);
    while (true)
    {
      SetLastError(0);
      DWORD size = GetModuleFileNameA(nullptr, hookDllPath.data(), DWORD(hookDllPath.size()));
      DWORD err = GetLastError();

      if (err == ERROR_INSUFFICIENT_BUFFER)
      {
        hookDllPath.resize(hookDllPath.size() * 2);
        continue;
      }

      release_assert(err == 0);
      hookDllPath.resize(size);
      break;
    }

    hookDllPath.resize(hookDllPath.size() - (sizeof("EmperorLauncher.exe") - 1));
    hookDllPath += "EmperorHooks.dll";
  }

  std::string commandLine = "Game.exe -w";
  PROCESS_INFORMATION processInfo = {};

  STARTUPINFOA startupInfo = {};
  startupInfo.cb = sizeof(STARTUPINFOA);

  release_assert(DetourCreateProcessWithDllA(0, (char*)commandLine.c_str(), 0, 0, 1, 0, 0, 0, &startupInfo, &processInfo, hookDllPath.c_str(), nullptr));

  return processInfo;
}

void sendEmperorDatDataToGameProcessViaAnonymousMapping(HANDLE processHandle, DWORD idThread)
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

void waitForExit(PROCESS_INFORMATION* gameRunData, DWORD* exitCode)
{
  WaitForSingleObject(gameRunData->hProcess, 0xFFFFFFFF);
  if (exitCode)
    *exitCode = -1;
  if (exitCode)
    GetExitCodeProcess(gameRunData->hProcess, exitCode);
}


void writeGraphicsSettings(int screenWidth, int screenHeight)
{
  HKEY key = nullptr;
  HRESULT result = RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\WestwoodRedirect\\Emperor\\Options\\Graphics", 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr);
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
  setValue("LimitFrameRate", "0"); // this doesn't work, just btw
  setValue("LimitTo16BitTex", "0");

  RegCloseKey(key);
}

int getMainMonitorHeight()
{
  const POINT ptZero = { 0, 0 };
  HMONITOR monitor = MonitorFromPoint(ptZero, MONITOR_DEFAULTTOPRIMARY);
  MONITORINFO info;
  info.cbSize = sizeof(MONITORINFO);
  GetMonitorInfo(monitor, &info);
  return info.rcMonitor.bottom - info.rcMonitor.top;
}

int main(int argc, char** argv)
{
  DWORD doFullscreen = 1;
  std::string serverAddress;

  for (int i = 1; i < argc; i++)
  {
    if (strcmp(argv[i], "--windowed") == 0)
    {
      doFullscreen = 0;
    }
    else if (strcmp(argv[i], "--connect") == 0)
    {
      release_assert(i + 1 < argc);
      serverAddress = argv[i + 1];
      i++;
    }
    else
    {
      release_assert(false);
    }
  }

  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);

  int screenHeight = getMainMonitorHeight();

  if (!doFullscreen)
    screenHeight = std::max(int(screenHeight * 0.8), 600);

  // Lots of stuff in the game assumes a 4:3 screen ratio - I tried to patch this out but I couldn't figure it out for now :(
  int adjustedScreenWidth = int(float(screenHeight) * 4.0f / 3.0f);

  writeGraphicsSettings(adjustedScreenWidth, screenHeight);

  release_assert(RegSetKeyValueA(HKEY_CURRENT_USER, "Software\\WestwoodRedirect\\Emperor\\LauncherCustomSettings", "DoFullscreen", REG_DWORD, &doFullscreen, sizeof(DWORD)) == ERROR_SUCCESS);
  release_assert(RegSetKeyValueA(HKEY_CURRENT_USER, "Software\\WestwoodRedirect\\Emperor\\LauncherCustomSettings", "ServerAddress", REG_SZ, serverAddress.data(), DWORD(serverAddress.size())) == ERROR_SUCCESS);

  createGlobalCommsFileMappingHandle();

  PROCESS_INFORMATION runGameData = runGameExe();

  sendEmperorDatDataToGameProcessViaAnonymousMapping(runGameData.hProcess, runGameData.dwThreadId);

  DWORD ExitCode = 0;
  waitForExit(&runGameData, &ExitCode);

  closeGlobalCommsHandle();

  return ExitCode;
}
