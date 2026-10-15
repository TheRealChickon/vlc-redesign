/* SPDX-License-Identifier: GPL-2.0-or-later
 * MinGW's static C++ runtime deletes its exception TLS key in the Qt DLL's
 * global destructors. Windows has already stopped other threads at process
 * detach, so Winpthreads' key lock may have no remaining thread to release it.
 * Normal deletion remains unchanged. The OS reclaims TLS at process exit.
 */
#if defined(_WIN32) && defined(__MINGW32__)
#include <windows.h>
#include <pthread.h>

static volatile LONG process_detaching = 0;

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved);
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)instance;
    if (reason == DLL_PROCESS_DETACH && reserved != NULL)
        InterlockedExchange(&process_detaching, 1);
    return TRUE;
}

int __real_pthread_key_delete(pthread_key_t key);
int __wrap_pthread_key_delete(pthread_key_t key);
int __wrap_pthread_key_delete(pthread_key_t key)
{
    if (InterlockedCompareExchange(&process_detaching, 0, 0)) return 0;
    return __real_pthread_key_delete(key);
}
#endif
