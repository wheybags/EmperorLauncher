#include <Windows.h>
#include <detours.h>
#include <cstdio>
#include <string>
#include <CommCtrl.h>
#include <map>
#include <optional>
#include "../EmperorHooks/Settings.hpp"

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



WNDPROC defWndProc = nullptr;
HWND window = nullptr;

HWND fullscreenCheckbox = nullptr;
HWND hostGameRadio = nullptr;
HWND connectToServerRadio = nullptr;
HWND serverAddressTextbox = nullptr;
HWND playButton = nullptr;

HWND pauseOnStartupCheckbox = nullptr;
HWND forceCursorVisibleCheckbox = nullptr;
HWND disableCursorCaptureCheckbox = nullptr;

bool playClicked = false;

Settings settings;


void refreshServerAddressTextboxEnabled()
{
  bool checked = SendMessage(connectToServerRadio, BM_GETCHECK, 0, 0) == BST_CHECKED;
  if (checked)
    EnableWindow(serverAddressTextbox, true);
  else
    EnableWindow(serverAddressTextbox, false);
}


void loadAndApplySettings()
{
  settings.readSettings();

  SendMessage(fullscreenCheckbox, BM_SETCHECK, settings.fullscreen ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessage(hostGameRadio, BM_SETCHECK, settings.hostGame ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessage(connectToServerRadio, BM_SETCHECK, settings.hostGame ? BST_UNCHECKED : BST_CHECKED, 0);
  SendMessageA(serverAddressTextbox, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(settings.serverAddress.c_str()));

  refreshServerAddressTextboxEnabled();
}


LRESULT onServerOrClientRadioClicked(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
  settings.hostGame = SendMessage(hostGameRadio, BM_GETCHECK, 0, 0) == BST_CHECKED;
  settings.writeSettings();

  refreshServerAddressTextboxEnabled();
  return CallWindowProc(defWndProc, hwnd, message, wParam, lParam);
}

LRESULT onFullscreenCheckboxClicked(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  settings.fullscreen = SendMessage(fullscreenCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
  settings.writeSettings();

  return CallWindowProc(defWndProc, hwnd, message, wParam, lParam);
}

std::string GetText(HWND hwnd)
{
  size_t size = SendMessageA(hwnd, WM_GETTEXTLENGTH, 0, 0);
  if (!size)
    return "";
  std::string text(size, '\0');
  SendMessageA(hwnd, WM_GETTEXT, text.size() + 1, reinterpret_cast<LPARAM>(text.c_str()));
  return text;
}

LRESULT onServerAddressTextChanged(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  settings.serverAddress = GetText(serverAddressTextbox);
  settings.writeSettings();
  return CallWindowProc(defWndProc, hwnd, message, wParam, lParam);
}

LRESULT onPauseOnStartupCheckboxClicked(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  settings.pauseOnStartup = SendMessage(pauseOnStartupCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
  settings.writeSettings();
  return CallWindowProc(defWndProc, hwnd, message, wParam, lParam);
}

LRESULT onForceCursorVisibleCheckboxClicked(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  settings.forceCursorVisible = SendMessage(forceCursorVisibleCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
  settings.writeSettings();
  return CallWindowProc(defWndProc, hwnd, message, wParam, lParam);
}

LRESULT onDisableCursorCaptureCheckboxClicked(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  settings.disableCursorCapture = SendMessage(disableCursorCaptureCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
  settings.writeSettings();
  return CallWindowProc(defWndProc, hwnd, message, wParam, lParam);
}

LRESULT OnWindowClose(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  PostQuitMessage(0);
  return CallWindowProc(defWndProc, hwnd, message, wParam, lParam);
}

LRESULT OnPlayClicked(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  playClicked = true;
  DestroyWindow(window);
  PostQuitMessage(0);
  return CallWindowProc(defWndProc, hwnd, message, wParam, lParam);
}


LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
  if (message == WM_CLOSE && hwnd == window) return OnWindowClose(hwnd, message, wParam, lParam);

  if (message == WM_COMMAND && HIWORD(wParam) == BN_CLICKED && reinterpret_cast<HWND>(lParam) == fullscreenCheckbox) return onFullscreenCheckboxClicked(hwnd, message, wParam, lParam);
  if (message == WM_COMMAND && HIWORD(wParam) == BN_CLICKED && reinterpret_cast<HWND>(lParam) == hostGameRadio) return onServerOrClientRadioClicked(hwnd, message, wParam, lParam);
  if (message == WM_COMMAND && HIWORD(wParam) == BN_CLICKED && reinterpret_cast<HWND>(lParam) == connectToServerRadio) return onServerOrClientRadioClicked(hwnd, message, wParam, lParam);
  if (message == WM_COMMAND && HIWORD(wParam) == EN_CHANGE && reinterpret_cast<HWND>(lParam) == serverAddressTextbox) return onServerAddressTextChanged(hwnd, message, wParam, lParam);
  if (message == WM_COMMAND && HIWORD(wParam) == BN_CLICKED && reinterpret_cast<HWND>(lParam) == playButton) return OnPlayClicked(hwnd, message, wParam, lParam);
  if (message == WM_COMMAND && HIWORD(wParam) == BN_CLICKED && reinterpret_cast<HWND>(lParam) == pauseOnStartupCheckbox) return onPauseOnStartupCheckboxClicked(hwnd, message, wParam, lParam);
  if (message == WM_COMMAND && HIWORD(wParam) == BN_CLICKED && reinterpret_cast<HWND>(lParam) == forceCursorVisibleCheckbox) return onForceCursorVisibleCheckboxClicked(hwnd, message, wParam, lParam);
  if (message == WM_COMMAND && HIWORD(wParam) == BN_CLICKED && reinterpret_cast<HWND>(lParam) == disableCursorCaptureCheckbox) return onDisableCursorCaptureCheckboxClicked(hwnd, message, wParam, lParam);

  return CallWindowProc(defWndProc, hwnd, message, wParam, lParam);
}


int main()
{
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);

  // main window
  int width = 550;
  int height = 350;
  int left = GetSystemMetrics(SM_CXSCREEN) / 2 - width / 2;
  int top = GetSystemMetrics(SM_CYSCREEN) / 2 - height / 2;
  window = CreateWindowEx(0, WC_DIALOG, L"EmperorLauncher", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, left, top, width, height, nullptr, nullptr, nullptr, nullptr);

  int ySpace = 30;
  int yMax = 0;

  // left side
  {
    int y = ySpace;
    int x = 30;

    CreateWindowEx(0, WC_STATIC, L"Settings", WS_CHILD | WS_VISIBLE, x, y, 200, 24, window, nullptr, nullptr, nullptr);
    y += ySpace;

    fullscreenCheckbox = CreateWindowEx(0, WC_BUTTON, L"Fullscreen", WS_CHILD | BS_AUTOCHECKBOX | WS_VISIBLE, x, y, 104, 24, window, nullptr, nullptr, nullptr);
    y += ySpace;
    y += ySpace;

    hostGameRadio = CreateWindowEx(0, WC_BUTTON, L"Host game / singleplayer", WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON, x, y, 200, 24, window, nullptr, nullptr, nullptr);
    y += ySpace;
    connectToServerRadio = CreateWindowEx(0, WC_BUTTON, L"Connect to server", WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON | WS_GROUP, x, y, 200, 24, window, nullptr, nullptr, nullptr);
    y += ySpace;

    serverAddressTextbox = CreateWindowEx(WS_EX_CLIENTEDGE, WC_EDIT, L"server ip here", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, x, y, 200, 24, window, nullptr, nullptr, nullptr);
    y += ySpace;

    yMax = std::max(yMax, y + ySpace);
  }

  // right side
  {
    int y = ySpace;
    int x = 320;

    CreateWindowEx(0, WC_STATIC, L"Debug settings", WS_CHILD | WS_VISIBLE, x, y, 200, 24, window, nullptr, nullptr, nullptr);
    y += ySpace;

    pauseOnStartupCheckbox = CreateWindowEx(0, WC_BUTTON, L"Pause on startup", WS_CHILD | BS_AUTOCHECKBOX | WS_VISIBLE, x, y, 200, 24, window, nullptr, nullptr, nullptr);
    y += ySpace;

    forceCursorVisibleCheckbox = CreateWindowEx(0, WC_BUTTON, L"Force cursor visible", WS_CHILD | BS_AUTOCHECKBOX | WS_VISIBLE, x, y, 200, 24, window, nullptr, nullptr, nullptr);
    y += ySpace;

    disableCursorCaptureCheckbox = CreateWindowEx(0, WC_BUTTON, L"Disable cursor capture", WS_CHILD | BS_AUTOCHECKBOX | WS_VISIBLE, x, y, 200, 24, window, nullptr, nullptr, nullptr);
    y += ySpace;


    yMax = std::max(yMax, y + ySpace);
  }

  playButton = CreateWindowEx(0, WC_BUTTON, L"Play", WS_CHILD | WS_VISIBLE, (width / 2) - 70, yMax, 104, 24, window, nullptr, nullptr, nullptr);

  defWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc)));

  loadAndApplySettings();

  ShowWindow(window, SW_SHOW);

  MSG message = { 0 };
  while (GetMessage(&message, nullptr, 0, 0))
  {
    TranslateMessage(&message);
    DispatchMessage(&message);
  }

  if (playClicked)
  {
    int screenHeight = getMainMonitorHeight();

    if (!settings.fullscreen)
      screenHeight = std::max(int(screenHeight * 0.8), 600);

    // Lots of stuff in the game assumes a 4:3 screen ratio - I tried to patch this out but I couldn't figure it out for now :(
    int adjustedScreenWidth = int(float(screenHeight) * 4.0f / 3.0f);

    writeGraphicsSettings(adjustedScreenWidth, screenHeight);

    createGlobalCommsFileMappingHandle();

    PROCESS_INFORMATION runGameData = runGameExe();

    sendEmperorDatDataToGameProcessViaAnonymousMapping(runGameData.hProcess, runGameData.dwThreadId);

    DWORD ExitCode = 0;
    waitForExit(&runGameData, &ExitCode);

    closeGlobalCommsHandle();

    return ExitCode;
  }

  return 0;
}
