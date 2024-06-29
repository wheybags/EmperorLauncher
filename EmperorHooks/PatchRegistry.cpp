#include <windows.h>
#include <string>
#include <detours.h>

// All the registry functions used in Game.exe and WOLAPI.DLL:
//
// Doesn't need redirect:
// RegCloseKey
// RegEnumKeyExA
// RegEnumValueA
// RegQueryInfoKeyA
// RegQueryValueExA
// RegSetValueExA
//
// Needs redirect:
// RegCreateKeyA
// RegCreateKeyExA
// RegDeleteKeyA
// RegOpenKeyExA

static bool startsWithCaseInsensitive(const char* haystack, const char* needle)
{
  if (!*needle && !*haystack)
    return true;

  while (*needle)
  {
    if (tolower(*needle) != tolower(*haystack))
      return false;

    needle++;
    haystack++;
  }

  return true;
}

struct RedirectedPath
{
  HKEY hKey;
  std::string path;
};

RedirectedPath getRedirectedPath(HKEY hKey, const char* fullPath)
{
  RedirectedPath newPath = { hKey, fullPath };

  if (hKey == HKEY_LOCAL_MACHINE)
  {
    constexpr char prefix[] = "Software\\Westwood\\";
    if (startsWithCaseInsensitive(newPath.path.data(), prefix))
    {
      newPath.path.replace(0, sizeof(prefix) - 1, "Software\\WestwoodRedirect\\");
      newPath.hKey = HKEY_CURRENT_USER;
    }
  }

  return newPath;
}

typedef LSTATUS (__stdcall* PFN_RegCreateKeyA)(HKEY hKey, LPCSTR lpSubKey, PHKEY phkResult);
PFN_RegCreateKeyA RegCreateKeyAOrig = nullptr;
LSTATUS __stdcall RegCreateKeyAPatched(HKEY hKey, LPCSTR lpSubKey, PHKEY phkResult)
{
  RedirectedPath newPath = getRedirectedPath(hKey, lpSubKey);
  return RegCreateKeyAOrig(newPath.hKey, newPath.path.c_str(), phkResult);
}

typedef LSTATUS (__stdcall* PFN_RegCreateKeyExA)(HKEY hKey, LPCSTR lpSubKey, DWORD Reserved, LPSTR lpClass, DWORD dwOptions, REGSAM samDesired, const LPSECURITY_ATTRIBUTES lpSecurityAttributes, PHKEY phkResult, LPDWORD lpdwDisposition);
PFN_RegCreateKeyExA RegCreateKeyExAOrig = nullptr;
LSTATUS __stdcall RegCreateKeyExAPatched(HKEY hKey, LPCSTR lpSubKey, DWORD Reserved, LPSTR lpClass, DWORD dwOptions, REGSAM samDesired, const LPSECURITY_ATTRIBUTES lpSecurityAttributes, PHKEY phkResult, LPDWORD lpdwDisposition)
{
  RedirectedPath newPath = getRedirectedPath(hKey, lpSubKey);
  return RegCreateKeyExAOrig(newPath.hKey, newPath.path.c_str(), Reserved, lpClass, dwOptions, samDesired, lpSecurityAttributes, phkResult, lpdwDisposition);
}

typedef LSTATUS (__stdcall* PFN_RegDeleteKeyA)(HKEY hKey, LPCSTR lpSubKey);
PFN_RegDeleteKeyA RegDeleteKeyAOrig = nullptr;
LSTATUS __stdcall RegDeleteKeyAPatched(HKEY hKey, LPCSTR lpSubKey)
{
  RedirectedPath newPath = getRedirectedPath(hKey, lpSubKey);
  return RegDeleteKeyAOrig(newPath.hKey, newPath.path.c_str());
}

typedef LSTATUS (__stdcall* PFN_RegOpenKeyExA)(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult);
PFN_RegOpenKeyExA RegOpenKeyExAOrig = nullptr;
LSTATUS __stdcall RegOpenKeyExAPatched(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult)
{
  RedirectedPath newPath = getRedirectedPath(hKey, lpSubKey);
  return RegOpenKeyExAOrig(newPath.hKey, newPath.path.c_str(), ulOptions, samDesired, phkResult);
}


void patchRedirectRegistry()
{
  HMODULE advapi32 = LoadLibraryA("Advapi32.dll");
  RegCreateKeyAOrig = (PFN_RegCreateKeyA)GetProcAddress(advapi32, "RegCreateKeyA");
  RegCreateKeyExAOrig = (PFN_RegCreateKeyExA)GetProcAddress(advapi32, "RegCreateKeyExA");
  RegDeleteKeyAOrig = (PFN_RegDeleteKeyA)GetProcAddress(advapi32, "RegDeleteKeyA");
  RegOpenKeyExAOrig = (PFN_RegOpenKeyExA)GetProcAddress(advapi32, "RegOpenKeyExA");


  DetourTransactionBegin();
  DetourUpdateThread(GetCurrentThread());
  DetourAttach(&(PVOID&)RegCreateKeyAOrig, RegCreateKeyAPatched);
  DetourAttach(&(PVOID&)RegCreateKeyExAOrig, RegCreateKeyExAPatched);
  DetourAttach(&(PVOID&)RegDeleteKeyAOrig, RegDeleteKeyAPatched);
  DetourAttach(&(PVOID&)RegOpenKeyExAOrig, RegOpenKeyExAPatched);
  DetourTransactionCommit();
}
