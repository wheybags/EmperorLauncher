#include "pch.h"
#include "PatchD3D7ResolutionLimit.hpp"
#include <Windows.h>

// adapted from https://github.com/UCyborg/LegacyD3DResolutionHack/
// and dxwrapper: https://github.com/elishacloud/dxwrapper/blob/0daa30e6a586effcb8fb47ae3537ddedd5d9b78b/ddraw/IDirect3DX.cpp#L858

const void* memmem(const void* l, size_t l_len, const void* s, size_t s_len)
{
	register char* cur, * last;
	const char* cl = (const char*)l;
	const char* cs = (const char*)s;

	/* we need something to compare */
	if (!l_len || !s_len)
		return NULL;

	/* "s" must be smaller or equal to "l" */
	if (l_len < s_len)
		return NULL;

	/* special case where s_len == 1 */
	if (s_len == 1)
		return memchr(l, (int)*cs, l_len);

	/* the last position where it's possible to find "s" in "l" */
	last = (char*)cl + l_len - s_len;

	for (cur = (char*)cl; cur <= last; cur++)
		if (cur[0] == cs[0] && !memcmp(cur, cs, s_len))
			return cur;

	return NULL;
}

// This seems kinda fragile and might break at some point
static void patchModule(HMODULE hD3DIm)
{
	const BYTE wantedBytes[] = { 0xB8, 0x00, 0x08, 0x00, 0x00, 0x39 };

	PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)hD3DIm;
	PIMAGE_NT_HEADERS pNtHeader = (PIMAGE_NT_HEADERS)((char*)pDosHeader + pDosHeader->e_lfanew);
	DWORD dwCodeBase = (DWORD)hD3DIm + pNtHeader->OptionalHeader.BaseOfCode;
	DWORD dwCodeSize = pNtHeader->OptionalHeader.SizeOfCode;

	DWORD dwPatchBase = (DWORD)memmem((void*)dwCodeBase, dwCodeSize, wantedBytes, sizeof(wantedBytes));
	if (dwPatchBase)
	{
		dwPatchBase++;

		DWORD dwOldProtect = 0;
		VirtualProtect((LPVOID)dwPatchBase, 4, PAGE_EXECUTE_READWRITE, &dwOldProtect);
		*(DWORD*)dwPatchBase = -1;
		VirtualProtect((LPVOID)dwPatchBase, 4, dwOldProtect, &dwOldProtect);
	}
}

void patchD3D7ResolutionLimit()
{
	patchModule(LoadLibraryA("d3dim.dll"));
	patchModule(LoadLibraryA("d3dim700.dll"));
}