/* error-posix.c - pokgame */
#include <pthread.h>

static inline int pok_get_thread_id()
{
    return (int) pthread_self();
}
