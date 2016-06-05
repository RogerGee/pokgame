#include <Windows.h>

#define localtime_r(a,b) localtime_s(b,a)

inline int pok_get_thread_id()
{
    return (int) GetCurrentThreadId();
}

static HANDLE error_module_mutex = INVALID_HANDLE_VALUE;

inline void pok_lock_error_module()
{
    if (error_module_mutex == INVALID_HANDLE_VALUE) {
        error_module_mutex = CreateMutex(NULL, FALSE, NULL);
        if (error_module_mutex == NULL)
            pok_error(pok_error_fatal, "fail CreateMutex()");
    }
    WaitForSingleObject(error_module_mutex, INFINITE);
}

inline void pok_unlock_error_module()
{
    ReleaseMutex(error_module_mutex);
}
