#include "pch.h"
#include "GameExeImports.hpp"

void (*sub_477250)() = (void (*)())0x477250;
int(__thiscall* sub_5473C0)(DWORD* This, const char* a2, const char* a3) = (int(__thiscall *)(DWORD * This, const char* a2, const char* a3))0x5473C0;

void (*EmptyFuncSometimesLog)(...) = (void (*)(...))0x401CB0;
char* (__thiscall* ResourceManager_GetResourceString)(int* resourceManager, const char* name) = (char* (__thiscall*)(int* resourceManager, const char* name))0x00405A50;
size_t(__cdecl* setupSoundCdPaths)(const char* driveLetterPath, int cdIndex) = (size_t(__cdecl*)(const char* driveLetterPath, int cdIndex))0x00477220;
char(__cdecl* doCdCheck)(int cdIndex, char a2) = (char(__cdecl*)(int cdIndex, char a2))0x00496580;

DWORD* dword_B7D098P = (DWORD*)0xB7D098;

int** resourceManagerP = (int**)0x007D75B8;
HWND* const mainWindowHandleP = (HWND*)0x007D75A8;
uint8_t* gDoQuitP = (uint8_t*)0x007D75AC;
char** cd1DriveLetterPathP = (char**)0x007CBC50;