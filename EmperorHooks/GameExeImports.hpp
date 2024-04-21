#pragma once
#include <cstdint>
#include <Windows.h>
#include <winsock.h>

struct SomeNetworkManager;
struct GlobalStuff;
struct GraphicsContext;

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

extern void (*sub_477250)();
extern unsigned __int8(__thiscall* sub_4D66F0)(GlobalStuff* This);
extern int(__fastcall* sub_477170)(int a1);
extern void(__thiscall* GlobalStuff_someSetter)(GlobalStuff* This);
extern int (__thiscall* sub_5473C0)(DWORD* This, const char* a2, const char* a3);
extern void(__thiscall* resetDevice)(GraphicsContext* This);
extern int(__thiscall* CNetworkAdmin_setFrameLimitFromGlobalSettings)(CNetworkAdmin* This);

extern char* (__thiscall *ResourceManager_GetResourceStringOrig)(int* This, const char* name);
extern size_t (__cdecl* setupSoundCdPathsOrig)(const char* driveLetterPath, int cdIndex);
extern char (__cdecl* doCdCheckOrig)(int cdIndex, char a2);
extern char (__cdecl* regSettingsOpenHkeyOrig)(char* fullPath, int createKey);
extern int(__thiscall* SomeNetworkManager_SendLobbyMessageOrig)(SomeNetworkManager* This, __int16 a2, int a3, wchar_t* message, char a5);
extern int(__stdcall* wndProcDuneIIIOrig)(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
extern int(__thiscall* CNetworkAdmin_setFrameLimitOrig)(CNetworkAdmin* This, int value);
extern u_short(*CPortUtil_Get_Free_Random_Port_HOrig)();
extern bool(__cdecl* testPort)(u_short hostshort);
extern void(__cdecl* CPortUtil_Set_Port_RangeOrig)(unsigned __int16 port_h_min, unsigned __int16 port_h_max);
extern void(*CMangler_Pattern_QueryOrig)();
extern void(__cdecl* sendPortRangeUpdatePacketOrig)(void* p_user, int Id, unsigned __int16 port_h_min, unsigned __int16 port_h_max);

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
#define gNetworkAdmin1 IMP(CNetworkAdmin*, 0x007CB9D4)
#define onlineGameHostPort IMP(u_short, 0x00B74C68)