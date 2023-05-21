#include "pch.h"

#define INITGUID
#define CINTERFACE
#include <Windows.h>
#include "dx7sdk_include/ddraw.h"
#include "dx7sdk_include/d3d.h"
#include <cstdio>
#include <detours.h>
#include <timeapi.h>
#include <profileapi.h>
#include <cstdint>
#include <mutex>
#include "Error.hpp"


LARGE_INTEGER lastFrameTime = {};
LARGE_INTEGER qpcFreq = {};
DWORD targetFps = 30;

// Limit FPS to 60, some effects in the game just don't work at high FPS
void* OriginalPointer_IDirect3DDevice7_EndScene = nullptr;
HRESULT(STDMETHODCALLTYPE* Real_IDirect3DDevice7_EndScene)(IDirect3DDevice7* This) = nullptr;
HRESULT STDMETHODCALLTYPE My_IDirect3DDevice7_EndScene(IDirect3DDevice7* This)
{
  LARGE_INTEGER nextFrameTime = {};
  nextFrameTime.QuadPart = lastFrameTime.QuadPart + qpcFreq.QuadPart / targetFps;

  LARGE_INTEGER now = {};
  QueryPerformanceCounter(&now);

  int64_t remainingTicks = int64_t(nextFrameTime.QuadPart) - int64_t(now.QuadPart);
  if (remainingTicks > 0)
  {
    DWORD remainingMs = DWORD(remainingTicks * 1000ULL / qpcFreq.QuadPart);
    if (remainingMs)
      Sleep(remainingMs);
  }

  HRESULT result = Real_IDirect3DDevice7_EndScene(This);

  QueryPerformanceCounter(&lastFrameTime);

  return result;
}


void* OriginalPointer_IDirect3D7_CreateDevice = nullptr;
HRESULT(STDMETHODCALLTYPE* Real_IDirect3D7_CreateDevice)(IDirect3D7* This, REFCLSID rclsid, LPDIRECTDRAWSURFACE7 lpDDS, LPDIRECT3DDEVICE7* lplpD3DDevice) = nullptr;
HRESULT STDMETHODCALLTYPE My_IDirect3D7_CreateDevice(IDirect3D7* This, REFCLSID rclsid, LPDIRECTDRAWSURFACE7 lpDDS, LPDIRECT3DDEVICE7* lplpD3DDevice)
{
  printf("!!!!!!!!!!!!!!!!! My_IDirect3D7_CreateDevice !!!!!!!!!!!!!!!!!!!\n");


  IDirect3DDevice7* device = *lplpD3DDevice;
  HRESULT result = Real_IDirect3D7_CreateDevice(This, rclsid, lpDDS, &device);
  *lplpD3DDevice = device;

  if (OriginalPointer_IDirect3DDevice7_EndScene)
    release_assert(device->lpVtbl->EndScene == OriginalPointer_IDirect3DDevice7_EndScene);

  if (device && Real_IDirect3DDevice7_EndScene == nullptr)
  {
    timeBeginPeriod(1);
    QueryPerformanceFrequency(&qpcFreq);
    QueryPerformanceCounter(&lastFrameTime);

    Real_IDirect3DDevice7_EndScene = device->lpVtbl->EndScene;
    OriginalPointer_IDirect3DDevice7_EndScene = device->lpVtbl->EndScene;

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&)Real_IDirect3DDevice7_EndScene, My_IDirect3DDevice7_EndScene);
    DetourTransactionCommit();
  }


  return result;
}


HRESULT(WINAPI* RealDirectDrawCreateEx)(GUID FAR* lpGuid, LPVOID* lplpDD, REFIID iid, IUnknown FAR* pUnkOuter) = nullptr;
HRESULT WINAPI MyDirectDrawCreateEx(GUID FAR* lpGuid, LPVOID* lplpDD, REFIID iid, IUnknown FAR* pUnkOuter)
{
  printf("!!!!!!!!!!!!!!!!! MyDirectDrawCreateEx !!!!!!!!!!!!!!!!!!!\n");

  IDirectDraw7* directDraw = *((IDirectDraw7**)lplpDD);
  HRESULT result = RealDirectDrawCreateEx(lpGuid, (void**)&directDraw, iid, pUnkOuter);
  *((IDirectDraw7**)lplpDD) = directDraw;

  if (directDraw)
  {
    IDirect3D7* direct3D = nullptr;
    directDraw->lpVtbl->QueryInterface(directDraw, IID_IDirect3D7, (void**)&direct3D);

    if (OriginalPointer_IDirect3D7_CreateDevice)
      release_assert(direct3D->lpVtbl->CreateDevice == OriginalPointer_IDirect3D7_CreateDevice);
    
    if (direct3D && Real_IDirect3D7_CreateDevice == nullptr)
    {
      Real_IDirect3D7_CreateDevice = direct3D->lpVtbl->CreateDevice;
      OriginalPointer_IDirect3D7_CreateDevice = direct3D->lpVtbl->CreateDevice;

      DetourTransactionBegin();
      DetourUpdateThread(GetCurrentThread());
      DetourAttach(&(PVOID&)Real_IDirect3D7_CreateDevice, My_IDirect3D7_CreateDevice);
      DetourTransactionCommit();

      direct3D->lpVtbl->Release(direct3D);
    }
  }

  return result;
}


void HookD3D7()
{
  // For some reason, this doesn't work if I use normal dynamic linking to fetch the original function pointer
  HMODULE ddraw = LoadLibraryA("DDRAW.DLL");
  RealDirectDrawCreateEx = (HRESULT(WINAPI*)(GUID FAR*, LPVOID*, REFIID, IUnknown FAR*)) GetProcAddress(ddraw, "DirectDrawCreateEx");

  DetourAttach(&(PVOID&)RealDirectDrawCreateEx, MyDirectDrawCreateEx);
}