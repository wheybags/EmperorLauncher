#include "pch.h"
#include <windows.h>
#include "GameExeImports.hpp"
#include "DoCdCheck.hpp"
#include <string>
#include <filesystem>


static char* __cdecl copyWstrToStrSimple(char* dest, wchar_t* src)
{
  wchar_t* src2; // edx
  wchar_t* src3; // esi
  char* v4; // ecx

  src2 = src;
  if (!*src)
    return dest;
  src3 = src;
  v4 = dest + 1;
  do
  {
    ++src2;
    *(v4 - 1) = *(BYTE*)src3;
    *v4++ = 0;
    src3 = src2;
  } while (*src2);
  return dest;
}

static bool isPathADiskRoot(const char* driveLetterPath)
{
  return strlen(driveLetterPath) == 3 && isalpha(driveLetterPath[0]) && driveLetterPath[1] == ':' && driveLetterPath[2] == '\\';
}

char __cdecl doCdCheckPatched(int cdIndex, char a2)
{
  if (!cdIndex)
    return 1;
  sub_477250();


  // campaign, we need a specific house CD
  if (cdIndex != 15)
  {
    int maxIterations = a2 != 0 ? 9 : 0;

    for (int i = 0; i <= maxIterations; i++)
    {
      int someCdDataObj = 0;
      switch (cdIndex - 1)
      {
        case 0:
          someCdDataObj = sub_5473C0(dword_B7D098P, "CDCheck", "InstallLabel");
          goto LABEL_27;
        case 1:
          someCdDataObj = sub_5473C0(dword_B7D098P, "CDCheck", "ATLabel");
          goto LABEL_27;
        case 3:
          someCdDataObj = sub_5473C0(dword_B7D098P, "CDCheck", "HKLabel");
          goto LABEL_27;
        case 7:
          someCdDataObj = sub_5473C0(dword_B7D098P, "CDCheck", "ORLabel");
        LABEL_27:
          break;
        default:
          EmptyFuncSometimesLog("Invalid CD Check ID");
          break;
      }

      const char* cdResourceName;
      const char* driveLetterPath;
      switch (cdIndex)
      {
        case 1:
          cdResourceName = "CD1";
          goto LABEL_14;
        case 2:
          cdResourceName = "CD2";
          goto LABEL_14;
        case 4:
          cdResourceName = "CD3";
          goto LABEL_14;
        case 8:
          cdResourceName = "CD4";
        LABEL_14:
          driveLetterPath = ResourceManager_GetResourceString(*resourceManagerP, cdResourceName);
          break;
        default:
          driveLetterPath = nullptr;
          break;
      }

      if (!driveLetterPath)
        return 1;

      bool ok = true;

      // if we are using a CD, make sure we have the right one
      if (isPathADiskRoot(driveLetterPath))
      {
        EmptyFuncSometimesLog("Volume %s is a CD-ROM Drive\n", driveLetterPath);

        CHAR VolumeNameBuffer[512];
        if (GetVolumeInformationA(driveLetterPath, VolumeNameBuffer, sizeof(VolumeNameBuffer) - 1, 0, 0, 0, 0, 0))
        {
          wchar_t* desiredVolumeName = *(wchar_t**)(someCdDataObj + 32);
          size_t len = wcslen(desiredVolumeName);

          std::string desiredVolumeNameNarrow(len, '\0');
          copyWstrToStrSimple(desiredVolumeNameNarrow.data(), desiredVolumeName);
          ok = _strcmpi(VolumeNameBuffer, desiredVolumeNameNarrow.data()) == 0;
        }
        else
        {
          ok = false;
        }
      }
      else
      {
        ok = std::filesystem::exists(std::filesystem::path(driveLetterPath) / "MUSIC.BAG");
      }

      if (ok)
      {
        switch (cdIndex - 1)
        {
          case 0:
            setupSoundCdPaths(driveLetterPath, 1);
            return 1;
          case 1:
            setupSoundCdPaths(driveLetterPath, 2);
            return 1;
          case 3:
            setupSoundCdPaths(driveLetterPath, 3);
            return 1;
          case 7:
            setupSoundCdPaths(driveLetterPath, 4);
            return 1;
          default:
            setupSoundCdPaths(driveLetterPath, 0);
            return 1;
        }
      }

      Sleep(500u);
    }

    return 0;
  }
  // non-campaign game, any game CD will do
  else
  {
    for (int i = 0; i < *somethingThatControlsExitingCdCheckLoopP; i++)
    {
      bool ok = true;
      if (isPathADiskRoot(*currentCdDataPathP))
      {
        CHAR VolumeNameBuffer[512];
        if (GetVolumeInformationA(*currentCdDataPathP, VolumeNameBuffer, sizeof(VolumeNameBuffer), 0, 0, 0, 0, 0))
        {
          bool isAtreidesDisk = false;
          {
            int someObj = sub_5473C0(dword_B7D098P, "CDCheck", "ATLabel");
            wchar_t* desiredVolumeName = *(wchar_t**)(someObj + 32);
            size_t len = wcslen(desiredVolumeName);

            std::string desiredVolumeNameNarrow(len, '\0');
            copyWstrToStrSimple(desiredVolumeNameNarrow.data(), desiredVolumeName);
            isAtreidesDisk = _strcmpi(VolumeNameBuffer, desiredVolumeNameNarrow.data()) == 0;
          }

          bool isHarkonnenDisk = false;
          {
            int someObj = sub_5473C0(dword_B7D098P, "CDCheck", "HKLabel");
            wchar_t* desiredVolumeName = *(wchar_t**)(someObj + 32);
            size_t len = wcslen(desiredVolumeName);

            std::string desiredVolumeNameNarrow(len, '\0');
            copyWstrToStrSimple(desiredVolumeNameNarrow.data(), desiredVolumeName);
            isHarkonnenDisk = _strcmpi(VolumeNameBuffer, desiredVolumeNameNarrow.data()) == 0;
          }

          bool isOrdosDisk = false;
          {
            int someObj = sub_5473C0(dword_B7D098P, "CDCheck", "ORLabel");
            wchar_t* desiredVolumeName = *(wchar_t**)(someObj + 32);
            size_t len = wcslen(desiredVolumeName);

            std::string desiredVolumeNameNarrow(len, '\0');
            copyWstrToStrSimple(desiredVolumeNameNarrow.data(), desiredVolumeName);
            isOrdosDisk = _strcmpi(VolumeNameBuffer, desiredVolumeNameNarrow.data()) == 0;
          }

          bool isInstallDisk = false;
          {
            int someObj = sub_5473C0(dword_B7D098P, "CDCheck", "InstallLabel");
            wchar_t* desiredVolumeName = *(wchar_t**)(someObj + 32);
            size_t len = wcslen(desiredVolumeName);

            std::string desiredVolumeNameNarrow(len, '\0');
            copyWstrToStrSimple(desiredVolumeNameNarrow.data(), desiredVolumeName);
            isInstallDisk = _strcmpi(VolumeNameBuffer, desiredVolumeNameNarrow.data()) == 0;
          }

          ok = isAtreidesDisk || isHarkonnenDisk || isOrdosDisk || isInstallDisk;
        }
        else
        {
          ok = false;
        }
      }
      else
      {
        ok = std::filesystem::exists(std::filesystem::path(*currentCdDataPathP) / "MUSIC.BAG") || 
             std::filesystem::exists(std::filesystem::path(*currentCdDataPathP) / "INSTALL" / "DATA" / "MUSIC" / "music.bag");
      }

      if (ok)
      {
        setupSoundCdPaths(*currentCdDataPathP, 0);
        return 1;
      }

      Sleep(500u);
    }
  }

  return 0;
}