#pragma once
#include <cstdint>
#include <Windows.h>

struct SomeNetworkManager;

extern void (*sub_477250)();
extern int (__thiscall* sub_5473C0)(DWORD* This, const char* a2, const char* a3);

extern char* (__thiscall *ResourceManager_GetResourceStringOrig)(int* This, const char* name);
extern size_t (__cdecl* setupSoundCdPathsOrig)(const char* driveLetterPath, int cdIndex);
extern char (__cdecl* doCdCheckOrig)(int cdIndex, char a2);
extern char (__cdecl* regSettingsOpenHkeyOrig)(char* fullPath, int createKey);
extern int(__thiscall* SomeNetworkManager_SendLobbyMessageOrig)(SomeNetworkManager* This, __int16 a2, int a3, wchar_t* message, char a5);

extern DWORD* dword_B7D098P;

extern int** resourceManagerP;
extern HWND* const mainWindowHandleP;
extern uint8_t* gDoQuitP;
extern char** currentCdDataPathP;
extern int* somethingThatControlsExitingCdCheckLoopP;