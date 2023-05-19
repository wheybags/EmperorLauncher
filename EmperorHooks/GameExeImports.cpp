#include "pch.h"
#include "GameExeImports.hpp"

void (*sub_477250)() = (void (*)())0x477250;
int(__thiscall* sub_5473C0)(DWORD* This, const char* a2, const char* a3) = (int(__thiscall *)(DWORD * This, const char* a2, const char* a3))0x5473C0;

char* (__thiscall* ResourceManager_GetResourceStringOrig)(int* resourceManager, const char* name) = (char* (__thiscall*)(int* resourceManager, const char* name))0x00405A50;
size_t(__cdecl* setupSoundCdPathsOrig)(const char* driveLetterPath, int cdIndex) = (size_t(__cdecl*)(const char* driveLetterPath, int cdIndex))0x00477220;
char(__cdecl* doCdCheckOrig)(int cdIndex, char a2) = (char(__cdecl*)(int cdIndex, char a2))0x00496580;
char(__cdecl* regSettingsOpenHkeyOrig)(char* fullPath, int createKey) = (char(__cdecl*)(char* fullPath, int createKey))0x0051E0E0;
int(__thiscall* SomeNetworkManager_SendLobbyMessageOrig)(SomeNetworkManager* This, __int16 a2, int a3, wchar_t* message, char a5) = (int(__thiscall *)(SomeNetworkManager * This, __int16 a2, int a3, wchar_t* message, char a5))0x004D96A0;

DWORD* dword_B7D098P = (DWORD*)0xB7D098;

int** resourceManagerP = (int**)0x007D75B8;
HWND* const mainWindowHandleP = (HWND*)0x007D75A8;
uint8_t* gDoQuitP = (uint8_t*)0x007D75AC;
char** currentCdDataPathP = (char**)0x007CBC50;
int* somethingThatControlsExitingCdCheckLoopP = (int*)0x007CBC68;