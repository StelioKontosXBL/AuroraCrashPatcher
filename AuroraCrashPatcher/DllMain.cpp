/*
 * Name:	AuroraCrashPatcher
 * Author:	Stelio Kontos
 * Desc:	Quick and dirty patch to prevent fatal crashing when
 *			downloading title assets (boxart, etc) through FSD and Aurora.
 *
 * Changelog:
 * v0.1-beta: 04/02/2021
 *		-	Initial commit.
 * v0.2-beta: 04/20/2021
 *		-	Changed base address to avoid conflict with nomni.
 *		-	Replaced hook method to more accurately identify and intercept the
 *			api calls causing the crash for everyone regardless of geographic locale.
 * v0.3-beta: 04/21/2021
 *		-	Added check to disable patching if Aurora/FSD update is detected.
 * v0.4-beta: 04/21/2021
 *		-	Changed killswitch method for Aurora devs to use once official patch is released.
 * v1.0: 04/22/2021
 * v1.1: 04/25/2021
 *		-	Fixed bug causing patch to not load for users not using a stealth server
 *			(props to rubensyama for helping track this down).
 */

#include "stdafx.h"
#include "Detour.h"

uint32_t g_flag = 0;
HANDLE g_hModule, g_hThread = 0;
Detour<INT> origHook;

INT HookProc(INT x, PCHAR h, HANDLE e, XNDNS **s) {
	if (!strcmp(h, "download.xbox.com")) {
		skDbgPrint("[sk] Blocked DNS Lookup for \"%s\"\n", h);
		strncpy(h, "stelio.kontos.nop", strlen(h));
	} else if (!strcmp(h, "aurora.crash.patched")) {
		g_flag = 0xDEADC0DE; //killswitch
	} else {
		skDbgPrint("[sk] Detected DNS Lookup for \"%s\"\n", h);
	}
	return ((INT(*)(INT, PCHAR, HANDLE, XNDNS **))origHook.SaveStub)(x, h, e, s);
}

DWORD WINAPI MainThread(LPVOID lpParameter) {
	auto Run = [] (uint32_t t) -> bool
	{
		static uint32_t p = 0;
		if (g_flag < 2) {
			if (t != p) {
				if (!t && !((uint32_t(*)(PVOID))0x800819D0)((PVOID)0x82000000))
					return true;
				if ((!t || t == 0xFFFE07D1 || t == 0xF5D20000) && ((uint32_t(*)(PVOID))0x800819D0)((PVOID)0x82000000)) {
					if (*(uint16_t*)0x82000000 != 0x4D5A)
						return true;
					g_flag = ByteSwap(*(uint32_t*)(0x82000008 + ByteSwap(*(uint32_t*)0x8200003C))) > 0x607F951E;
					if (!g_flag && !origHook.Addr) {
						if (origHook.SetupDetour(0x81741150, HookProc)) {
							DbgPrint("[sk] AuroraCrashPatcher v" SK_VERSION " by Stelio Kontos: ENABLED. [flag: 0x%X]\n", &g_flag);
						}
					}
				} else if (!p || p == 0xFFFE07D1 || p == 0xF5D20000) {
					if (origHook.Addr) {
						origHook.TakeDownDetour();
						DbgPrint("[sk] AuroraCrashPatcher v" SK_VERSION " DISABLED");
						g_flag = 0;
					}
				}
				p = t;
			}
			return true;
		} else {
			origHook.TakeDownDetour();
			if (g_flag == 0xDEADC0DE)
				SelfDestruct(g_hModule);
			return false;
		}
	};

	if (!MountSysDrives()) {
		skDbgPrint("[sk] Failed to mount system drives\n");
	}

	while (Run(((uint32_t(*)())0x816E03B8)())) {
		Sleep(100);
	}

	*(uint16_t *)((uint8_t *)g_hModule + 0x40) = 1;
	((void(*)(HANDLE, HANDLE))0x8007D190)(g_hModule, g_hThread);

	return 0;
}

BOOL APIENTRY DllMain(HANDLE hModule, DWORD dwReason, LPVOID lpReserved) {
	if (dwReason == DLL_PROCESS_ATTACH) {
		skDbgPrint("++++++++++ sk ~ DLL_PROCESS_ATTACH ++++++++++\n");
		g_hModule = hModule;
		ExCreateThread(&g_hThread, 0, 0, (PVOID)XapiThreadStartup, (LPTHREAD_START_ROUTINE)MainThread, 0, 0x2 | 0x1);
		XSetThreadProcessor(g_hThread, 0x4);
		ResumeThread(g_hThread);
	} else if (dwReason == DLL_PROCESS_DETACH) {
		skDbgPrint("========== sk ~ DLL_PROCESS_DETACH ==========\n");
	}

	return TRUE;
}
