#pragma once
#include <cstdint>
#include <Windows.h>
#include <winsock.h>

struct SomeNetworkManager;
struct GlobalStuff;
struct GraphicsContext;
struct GameOptions;

struct IpAndPort
{
  in_addr ip;
  unsigned __int16 portInNetworkByteOrder;
  BYTE something[2];
};

struct SomeNetworkThing
{
  uint8_t* somePtr;
  DWORD unk[7];
};

struct CNetworkAdmin
{
  IpAndPort ipAndPort;
  DWORD unk;
  int frameLimit;
  float somethingRelatedToFrameLimit;
  DWORD unk6;
  DWORD latency;
  DWORD somethingRelatedToLatency;
  DWORD unk2;
  DWORD someFlag;
  DWORD someRandomNumber;
  DWORD someTickCount1;
  DWORD someTickCount2;
  DWORD unk3[10];
  uint8_t someBuffer[32];
  DWORD unk5[2];
  uint8_t someBuffer2[32];
  SomeNetworkThing something;
  DWORD unk4[66];
};

struct SomeIntSettingVtable
{
  void* probablyFree;
  void* saveIntToReg;
  void* readIntFromReg;
  void* unk0;
  void* unk2;
};

struct stdString
{
  DWORD maybeAllocator;
  char* data;
  DWORD length;
  DWORD maybeCapacity;
};


struct SomeIntSetting
{
  SomeIntSettingVtable* vtable;
  stdString registryValueName;
  stdString optionName;
  DWORD unk2;
  DWORD intValue;
};

struct Node
{
  Node* maybeNext;
  Node* someNode;
  Node* someNode2;
  stdString someString;
  SomeIntSetting* intSetting;
  DWORD unk;
};

struct SomeList
{
  DWORD unk;
  Node* node;
  DWORD unk2[2];
};


struct SomethingWithDataInIt
{
  DWORD* someVtable;
  SomeList someList;
};

struct GameOptions
{
  DWORD unk[5];
  SomethingWithDataInIt* somethingWithDataInIt;
};

struct COptionsStorage
{
  DWORD unk[17];
  char selectedIp[1];
  DWORD unk2[99];
  GameOptions gameOptions;
  DWORD unk4[12];
};

struct SomeGlobalThing
{
  COptionsStorage* optionsStorage;
  DWORD unkwrongSize[5];
};


typedef size_t(__cdecl* PFN_setupSoundCdPaths)(const char* driveLetterPath, int cdIndex);
typedef void(__cdecl* PFN_sub_477250)();
typedef unsigned __int8(__thiscall* PFN_sub_4D66F0)(GlobalStuff* This);
typedef int(__fastcall* PFN_sub_477170)(int a1);
typedef void(__thiscall* PFN_GlobalStuff_someSetter)(GlobalStuff* This);
typedef int(__thiscall* PFN_sub_5473C0)(DWORD* This, const char* a2, const char* a3);
typedef void(__thiscall* PFN_resetDevice)(GraphicsContext* This); // note: maybe wrong signature
typedef int(__thiscall* PFN_CNetworkAdmin_setFrameLimitFromGlobalSettings)(CNetworkAdmin* This);
typedef char* (__thiscall* PFN_ResourceManager_GetResourceStringOrig)(int* resourceManager, const char* name);
typedef char(__cdecl* PFN_doCdCheck)(int cdIndex, char a2);
typedef char(__cdecl* PFN_regSettingsOpenHkey)(char* fullPath, int createKey);
typedef int(__thiscall* PFN_SomeNetworkManager_SendLobbyMessage)(SomeNetworkManager* This, __int16 a2, int a3, wchar_t* message, char a5);
typedef int(__stdcall* PFN_wndProcDuneIII)(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
typedef BOOL(__cdecl* PFN_setWindowStyleAndDrainMessages)(HWND hWnd, int width, int height, char windowedMode);
typedef int(__thiscall* PFN_CNetworkAdmin_setFrameLimit)(CNetworkAdmin* This, int value);
typedef void(*PFN_CMangler_Pattern_Query)();
typedef int(__stdcall* PFN_WinMain)(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd);
typedef DWORD(__thiscall* PFN_GameOptions_getGameSpeed)(GameOptions* This);


#define setupSoundCdPaths ((PFN_setupSoundCdPaths)0x00477220)
#define sub_477250 ((PFN_sub_477250)0x477250)
#define sub_4D66F0 ((PFN_sub_4D66F0)0x4D66F0)
#define sub_477170 ((PFN_sub_477170)0x00477170)
#define GlobalStuff_someSetter ((PFN_GlobalStuff_someSetter)0x4D7230)
#define sub_5473C0 ((PFN_sub_5473C0)0x5473C0)
#define resetDevice ((PFN_resetDevice)0x00407F20)
#define CNetworkAdmin_setFrameLimitFromGlobalSettings ((PFN_CNetworkAdmin_setFrameLimitFromGlobalSettings)0x00509990)
#define ResourceManager_GetResourceString ((PFN_ResourceManager_GetResourceStringOrig)0x00405A50)
#define doCdCheck ((PFN_doCdCheck)0x00496580)
#define regSettingsOpenHkey ((PFN_regSettingsOpenHkey)0x0051E0E0)
#define SomeNetworkManager_SendLobbyMessage ((PFN_SomeNetworkManager_SendLobbyMessage)0x004D96A0)
#define wndProcDuneIII ((PFN_wndProcDuneIII)0x004A6560)
#define setWindowStyleAndDrainMessages ((PFN_setWindowStyleAndDrainMessages)0x004A7260)
#define CNetworkAdmin_setFrameLimit ((PFN_CNetworkAdmin_setFrameLimit)0x00406220)
#define CMangler_Pattern_Query ((PFN_CMangler_Pattern_Query)0x005AC370)
#define GameExeWinMain ((PFN_WinMain)0x004A73D0)
#define GameOptions_getGameSpeed ((PFN_GameOptions_getGameSpeed)0x00509A30)


#define IMP(type, address) (*((type*)address))
#define somethingThatControlsExitingCdCheckLoop IMP(int, 0x007CBC68)
#define currentCdDataPath IMP(char*, 0x007CBC50)
#define gDoQuit IMP(uint8_t, 0x007D75AC)
#define mainWindowHandle IMP(HWND, 0x007D75A8)
#define pResourceManager IMP(int*, 0x007D75B8)
#define dword_B7D098 IMP(DWORD, 0xB7D098)
#define dword_7CB9D4 IMP(DWORD, 0x7CB9D4)
#define dword_818458 IMP(DWORD, 0x818458)
#define dword_81845C IMP(DWORD, 0x81845C)
#define dword_7D75B0 IMP(DWORD, 0x7D75B0)
#define someGlobalNetworkObject IMP(SomeNetworkManager*, 0x00821D68)
#define somethingThatAffectsContextFree IMP(char, 0x00817C6D)
#define globalStuff IMP(GlobalStuff, 0x00818718)
#define maybeDontFreeContext IMP(char, 0x007D75AD)
#define gContext IMP(GraphicsContext*, 0x007D75B4)
#define gNetworkAdmin1 IMP(CNetworkAdmin*, 0x007CB9D4)
#define onlineGameHostPort IMP(u_short, 0x00B74C68)
#define someGlobalThing IMP(SomeGlobalThing, 0x007D75E8)