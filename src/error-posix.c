/* error-posix.c - pokgame */
#include <pthread.h>

inline int pok_get_thread_id()
{
    return (int) pthread_self();
}

static pthread_mutex_t error_module_mutex = PTHREAD_MUTEX_INITIALIZER;

inline void pok_lock_error_module()
{
    if (pthread_mutex_lock(&error_module_mutex) != 0)
        pok_error(pok_error_fatal,"misuse of pok_lock_error_module()");
}

inline void pok_unlock_error_module()
{
    if (pthread_mutex_unlock(&error_module_mutex) != 0)
        pok_error(pok_error_fatal,"misuse of pok_unlock_error_module()");
}
