/* pokgame-posix.c - pokgame */
#include <pthread.h>

struct gamelock
{
    void* object;
    int updown;
    pthread_mutex_t mutex;
};

struct gamelock* gamelock_new(void* object)
{
    struct gamelock* lock;
    lock = malloc(sizeof(struct gamelock));
    if (lock == NULL) {
        pok_exception_flag_memory_error();
        return NULL;
    }
    lock->object = object;
    lock->updown = 0;
    pthread_mutex_init(&lock->mutex,NULL);
    return lock;
}
void gamelock_aquire(struct gamelock* lock)
{
    pthread_mutex_lock(&lock->mutex);
}
void gamelock_release(struct gamelock* lock)
{
    pthread_mutex_unlock(&lock->mutex);
}
void gamelock_up(struct gamelock* lock)
{
    if (lock->updown == 0)
        pthread_mutex_lock(&lock->mutex);
    ++lock->updown;
}
void gamelock_down(struct gamelock* lock)
{
    --lock->updown;
    if (lock->updown == 0)
        pthread_mutex_unlock(&lock->mutex);
}
