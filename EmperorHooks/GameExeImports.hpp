#pragma once
#include <cstdint>
#include <Windows.h>

extern void (*sub_477250)();
extern int (__thiscall* sub_5473C0)(DWORD* This, const char* a2, const char* a3);

extern void (*EmptyFuncSometimesLog)(...);
extern char* (__thiscall *ResourceManager_GetResourceString)(int* This, const char* name);
extern size_t (__cdecl* setupSoundCdPaths)(const char* driveLetterPath, int cdIndex);
extern char (__cdecl* doCdCheck)(int cdIndex, char a2);

extern DWORD* dword_B7D098P;

extern int** resourceManagerP;
extern HWND* const mainWindowHandleP;
extern uint8_t* gDoQuitP;
extern char** currentCdDataPathP;
extern int* somethingThatControlsExitingCdCheckLoopP;