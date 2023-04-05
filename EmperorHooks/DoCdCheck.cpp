#include "pch.h"
#include <windows.h>
#include "GameExeImports.hpp"
#include "DoCdCheck.hpp"


char __cdecl doCdCheckPatched(int cdIndex, char a2)
{
  int cdIndex2; // edi
  char result; // al
  const char* cdResourceName; // eax
  int v5; // esi
  int v6; // ebp
  int cdIndexMinusOne; // edi
  int v8; // eax
  int v9; // eax
  int v10; // edi
  CHAR** v11; // esi
  CHAR* v12; // eax
  int v13; // eax
  int v14; // eax
  int v15; // eax
  int v16; // eax
  int v17; // eax
  int v18; // eax
  int v19; // eax
  int v20; // eax
  int v21; // edx
  int v22; // [esp+0h] [ebp-220h] BYREF
  CHAR RootPathName[4]; // [esp+10h] [ebp-210h] BYREF
  int v24; // [esp+14h] [ebp-20Ch]
  const char* driveLetterPath; // [esp+18h] [ebp-208h]
  int v26; // [esp+1Ch] [ebp-204h]
  CHAR VolumeNameBuffer[512]; // [esp+20h] [ebp-200h] BYREF

  cdIndex2 = cdIndex;
  if (!cdIndex)
    return 1;
  sub_477250();
  switch (cdIndex)
  {
  case 1:
    v24 = 1;
    break;
  case 2:
    v24 = 2;
    break;
  case 4:
    v24 = 3;
    break;
  case 8:
    v24 = 4;
    break;
  default:
    v24 = 0;
    break;
  }
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
    driveLetterPath = 0;
    break;
  }
  v5 = a2 != 0 ? 9 : 0;
  v26 = v5;
  v6 = v26;
  //while (1)
  //{
    if (cdIndex2 != 15)
    {
      //RootPathName[0] = *driveLetterPath;
      //RootPathName[1] = driveLetterPath[1];
      //strcpy(&RootPathName[2], "\\");
      //if (GetDriveTypeA(RootPathName) != DRIVE_CDROM)
        //return 0;
      //EmptyFuncSometimesLog("Volume %s is a CD-ROM Drive\n", RootPathName);
      //if (GetVolumeInformationA(RootPathName, VolumeNameBuffer, 511u, 0, 0, 0, 0, 0))
      {
        cdIndexMinusOne = cdIndex2 - 1;
        switch (cdIndexMinusOne)
        {
        case 0:
          v8 = sub_5473C0(dword_B7D098P, "CDCheck", "InstallLabel");
          goto LABEL_27;
        case 1:
          v8 = sub_5473C0(dword_B7D098P, "CDCheck", "ATLabel");
          goto LABEL_27;
        case 3:
          v8 = sub_5473C0(dword_B7D098P, "CDCheck", "HKLabel");
          goto LABEL_27;
        case 7:
          v8 = sub_5473C0(dword_B7D098P, "CDCheck", "ORLabel");
        LABEL_27:
          v6 = v8;
          break;
        default:
          EmptyFuncSometimesLog("Invalid CD Check ID");
          break;
        }
        //copyWstrToStrSimple(someVolumeNameUsedInCdCheck, *(wchar_t**)(v6 + 32));
        //LOBYTE(v9) = _strcmpi(VolumeNameBuffer, someVolumeNameUsedInCdCheck);
        //if (!v9)
        {
          if (!v24)
            return 1;
          switch (cdIndexMinusOne)
          {
          case 0:
            setupSoundCdPaths(driveLetterPath, 1);
            result = 1;
            break;
          case 1:
            setupSoundCdPaths(driveLetterPath, 2);
            result = 1;
            break;
          case 3:
            setupSoundCdPaths(driveLetterPath, 3);
            result = 1;
            break;
          case 7:
            setupSoundCdPaths(driveLetterPath, 4);
            result = 1;
            break;
          default:
            setupSoundCdPaths(driveLetterPath, 0);
            result = 1;
            break;
          }
          return result;
        }
      }
      //goto LABEL_41;
    }
  /*  v10 = 0;
    VolumeNameBuffer[0] = 0;
    if (dword_7CBC68 > 0)
      break;
  LABEL_41:
    Sleep(500u);
    v21 = v5--;
    v26 = v5;
    if (!v21)
      return 0;
    cdIndex2 = cdIndex;*/
  //}
  //v11 = (CHAR**)cd1DriveLetterPath;
  //while (1)
  //{
  //  v12 = *v11;
  //  RootPathName[0] = **v11;
  //  RootPathName[1] = v12[1];
  //  RootPathName[2] = 92;
  //  RootPathName[3] = 0;
  //  if (!GetVolumeInformationA(RootPathName, VolumeNameBuffer, 511u, 0, 0, 0, 0, 0) || &v22 == (int*)-32)
  //    goto LABEL_39;
  //  v13 = sub_5473C0(&dword_B7D098, "CDCheck", "ATLabel");
  //  copyWstrToStrSimple(someVolumeNameUsedInCdCheck, *(wchar_t**)(v13 + 32));
  //  LOBYTE(v14) = _strcmpi(VolumeNameBuffer, someVolumeNameUsedInCdCheck);
  //  if (!v14)
  //    goto LABEL_54;
  //  v15 = sub_5473C0(&dword_B7D098, "CDCheck", "HKLabel");
  //  copyWstrToStrSimple(someVolumeNameUsedInCdCheck, *(wchar_t**)(v15 + 32));
  //  LOBYTE(v16) = _strcmpi(VolumeNameBuffer, someVolumeNameUsedInCdCheck);
  //  if (!v16)
  //  {
  //    if (!v24)
  //      return 1;
  //    goto LABEL_51;
  //  }
  //  v17 = sub_5473C0(&dword_B7D098, "CDCheck", "ORLabel");
  //  copyWstrToStrSimple(someVolumeNameUsedInCdCheck, *(wchar_t**)(v17 + 32));
  //  LOBYTE(v18) = _strcmpi(VolumeNameBuffer, someVolumeNameUsedInCdCheck);
  //  if (!v18)
  //    break;
  //  v19 = sub_5473C0(&dword_B7D098, "CDCheck", "InstallLabel");
  //  copyWstrToStrSimple(someVolumeNameUsedInCdCheck, *(wchar_t**)(v19 + 32));
  //  LOBYTE(v20) = _strcmpi(VolumeNameBuffer, someVolumeNameUsedInCdCheck);
  //  if (!v20)
  //  {
  //  LABEL_54:
  //    if (v24)
  //      setupSoundCdPaths(*(const char**)&cd1DriveLetterPath[4 * v10], 0);
  //    return 1;
  //  }
  //LABEL_39:
  //  ++v10;
  //  ++v11;
  //  if (v10 >= dword_7CBC68)
  //  {
  //    v5 = v26;
  //    goto LABEL_41;
  //  }
  //}
  //if (v24)
  //{
  //LABEL_51:
  //  setupSoundCdPaths(*(const char**)&cd1DriveLetterPath[4 * v10], 0);
  setupSoundCdPaths(*cd1DriveLetterPathP, 0);
  //  return 1;
  //}
  return 1;
}