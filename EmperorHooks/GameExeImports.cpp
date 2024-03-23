#include "pch.h"
#include "GameExeImports.hpp"

void (*sub_477250)() = (void (*)())0x477250;
unsigned __int8 (__thiscall* sub_4D66F0)(GlobalStuff* This) = (unsigned __int8(__thiscall *)(GlobalStuff * This))0x4D66F0;
int(__fastcall* sub_477170)(int a1) = (int(__fastcall*)(int a1)) 0x00477170;
void(__thiscall* GlobalStuff_someSetter)(GlobalStuff* This) = (void(__thiscall *)(GlobalStuff * This)) 0x4D7230;
int(__thiscall* sub_5473C0)(DWORD* This, const char* a2, const char* a3) = (int(__thiscall *)(DWORD * This, const char* a2, const char* a3))0x5473C0;
void(__thiscall* resetDevice)(GraphicsContext* This) = (void(__thiscall*)(GraphicsContext * This))0x00407F20; // note: maybe wrong signature

char* (__thiscall* ResourceManager_GetResourceStringOrig)(int* resourceManager, const char* name) = (char* (__thiscall*)(int* resourceManager, const char* name))0x00405A50;
size_t(__cdecl* setupSoundCdPathsOrig)(const char* driveLetterPath, int cdIndex) = (size_t(__cdecl*)(const char* driveLetterPath, int cdIndex))0x00477220;
char(__cdecl* doCdCheckOrig)(int cdIndex, char a2) = (char(__cdecl*)(int cdIndex, char a2))0x00496580;
char(__cdecl* regSettingsOpenHkeyOrig)(char* fullPath, int createKey) = (char(__cdecl*)(char* fullPath, int createKey))0x0051E0E0;
int(__thiscall* SomeNetworkManager_SendLobbyMessageOrig)(SomeNetworkManager* This, __int16 a2, int a3, wchar_t* message, char a5) = (int(__thiscall *)(SomeNetworkManager * This, __int16 a2, int a3, wchar_t* message, char a5))0x004D96A0;
extern int(__stdcall* wndProcDuneIIIOrig)(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) = (int(__stdcall*)(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)) 0x004A6560;

DWORD* dword_B7D098P = (DWORD*)0xB7D098;

int** resourceManagerP = (int**)0x007D75B8;
HWND* const mainWindowHandleP = (HWND*)0x007D75A8;
uint8_t* gDoQuitP = (uint8_t*)0x007D75AC;
char** currentCdDataPathP = (char**)0x007CBC50;
int* somethingThatControlsExitingCdCheckLoopP = (int*)0x007CBC68;