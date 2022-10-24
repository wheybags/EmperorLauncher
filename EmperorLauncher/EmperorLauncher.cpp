#include <Windows.h>
#include <detours.h>

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


char __cdecl runCommand(ProcessRunData* gameRunData)
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


  if (!DetourCreateProcessWithDllA(0, CommandLine, 0, 0, 1, 0, 0, 0, &StartupInfo, &ProcessInformation, hookDllPath, nullptr))
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

int main()
{
  createGlobalCommsFileMappingHandle();

  ProcessRunData runGameData = {};
  strcat(runGameData.exeName, "Game.exe");
  runCommand(&runGameData);

  sendEmperorDatDataToGameProcessViaAnonymousMapping(runGameData.processHandle, runGameData.processThreadId);

  DWORD ExitCode = 0;
  waitForExit(&runGameData, &ExitCode);

  closeGlobalCommsHandle();

  return ExitCode;
}
