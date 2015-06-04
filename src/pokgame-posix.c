/* pokgame-posix.c - pokgame */
#include <pthread.h>
#include <unistd.h>

struct gamelock
{
    void* object;
    int updown;
    pthread_mutex_t wait;
    pthread_mutex_t aquire;
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
    pthread_mutex_init(&lock->wait,NULL);
    pthread_mutex_init(&lock->aquire,NULL);
    return lock;
}
void gamelock_aquire(struct gamelock* lock)
{
    pthread_mutex_lock(&lock->wait);
    pthread_mutex_lock(&lock->aquire);
    pthread_mutex_unlock(&lock->wait);
}
void gamelock_release(struct gamelock* lock)
{
    pthread_mutex_unlock(&lock->aquire);
}
void gamelock_up(struct gamelock* lock)
{
    pthread_mutex_lock(&lock->wait);
    if (lock->updown++ == 0)
        pthread_mutex_lock(&lock->aquire);
    pthread_mutex_unlock(&lock->wait);
}
void gamelock_down(struct gamelock* lock)
{
    if (--lock->updown == 0)
        pthread_mutex_unlock(&lock->aquire);
}

/* implement 'timeout' from 'pokgame.h' */
void timeout(struct timeout_interval* interval)
{
    usleep(interval->useconds);
}
