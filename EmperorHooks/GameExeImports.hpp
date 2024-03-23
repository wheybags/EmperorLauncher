#pragma once
#include <cstdint>
#include <Windows.h>

struct SomeNetworkManager;
struct GlobalStuff;
struct GraphicsContext;

extern void (*sub_477250)();
extern unsigned __int8(__thiscall* sub_4D66F0)(GlobalStuff* This);
extern int(__fastcall* sub_477170)(int a1);
extern void(__thiscall* GlobalStuff_someSetter)(GlobalStuff* This);
extern int (__thiscall* sub_5473C0)(DWORD* This, const char* a2, const char* a3);
extern void(__thiscall* resetDevice)(GraphicsContext* This);

extern char* (__thiscall *ResourceManager_GetResourceStringOrig)(int* This, const char* name);
extern size_t (__cdecl* setupSoundCdPathsOrig)(const char* driveLetterPath, int cdIndex);
extern char (__cdecl* doCdCheckOrig)(int cdIndex, char a2);
extern char (__cdecl* regSettingsOpenHkeyOrig)(char* fullPath, int createKey);
extern int(__thiscall* SomeNetworkManager_SendLobbyMessageOrig)(SomeNetworkManager* This, __int16 a2, int a3, wchar_t* message, char a5);
extern int(__stdcall* wndProcDuneIIIOrig)(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

extern DWORD* dword_B7D098P;

extern int** resourceManagerP;
extern HWND* const mainWindowHandleP;
extern uint8_t* gDoQuitP;
extern char** currentCdDataPathP;
extern int* somethingThatControlsExitingCdCheckLoopP;


#define IMP(type, address) (*((type*)address))

#define dword_7CB9D4 IMP(DWORD, 0x7CB9D4)
#define dword_818458 IMP(DWORD, 0x818458)
#define dword_81845C IMP(DWORD, 0x81845C)
#define dword_7D75B0 IMP(DWORD, 0x7D75B0)
#define someGlobalNetworkObject IMP(SomeNetworkManager*, 0x00821D68)
#define somethingThatAffectsContextFree IMP(char, 0x00817C6D)
#define globalStuff IMP(GlobalStuff, 0x00818718)
#define maybeDontFreeContext IMP(char, 0x007D75AD)
#define gContext IMP(GraphicsContext*, 0x007D75B4)