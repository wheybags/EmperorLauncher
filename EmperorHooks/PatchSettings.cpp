#include "pch.h"
#include "GameExeImports.hpp"
#include <string>
#include <detours.h>

static PFN_regSettingsOpenHkey regSettingsOpenHkeyOrig = regSettingsOpenHkey;

// redirect HKEY_LOCAL_MACHINE\\Software\\Westwood -> HKEY_CURRENT_USER\\Software\\WestwoodRedirect
// Using HKEY_LOCAL_MACHINE requires admin on moder windows. There is some automagical redirection that should work,
// but I think linking in the modern patch dll is messing with the heuristics windows uses to activate it - so we just
// force our own.
static char __cdecl regSettingsOpenHkeyPatched(char* fullPath, int createKey)
{
  std::string newPath(fullPath);

  constexpr char prefix[] = "HKEY_LOCAL_MACHINE\\Software\\Westwood\\";
  size_t prefixIndex = newPath.find(prefix);
  if (prefixIndex != std::string::npos)
    newPath.replace(prefixIndex, sizeof(prefix) - 1, "HKEY_CURRENT_USER\\Software\\WestwoodRedirect\\");

  return regSettingsOpenHkeyOrig(newPath.data(), createKey);
}

void patchRedirectRegistry()
{
  DetourTransactionBegin();
  DetourUpdateThread(GetCurrentThread());
  DetourAttach(&(PVOID&)regSettingsOpenHkeyOrig, regSettingsOpenHkeyPatched);
  DetourTransactionCommit();
}
