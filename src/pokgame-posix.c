/* pokgame-posix.c - pokgame */
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#define SEMAPHORE_MAX 10

#ifdef POKGAME_DEBUG
#define CHECK_SUCCESS(call) \
    if (call != 0) \
        pok_error(pok_error_fatal,"fail '"#call"'")
#else
#define CHECK_SUCCESS(call) call
#endif

struct gamelock
{
    void* object;          /* we need to provide synchronization for this object */
    sem_t readOnly;        /* this semaphore allows more than 2 process to read the object */
    sem_t modify;          /* this mutex allows 1 process to modify the object */
    pthread_mutex_t atom;  /* this mutex makes certain operations atomic */
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
    sem_init(&lock->readOnly,0,SEMAPHORE_MAX);
    sem_init(&lock->modify,0,1);
    pthread_mutex_init(&lock->atom,NULL);
    return lock;
}
void gamelock_free(struct gamelock* lock)
{
    free(lock);
}
inline void gamelock_aquire(struct gamelock* lock)
{
    CHECK_SUCCESS( sem_wait(&lock->modify) );
}
void gamelock_release(struct gamelock* lock)
{
    CHECK_SUCCESS( sem_post(&lock->modify) );
}
void gamelock_up(struct gamelock* lock)
{
    /* make the following region executes atomically */
    CHECK_SUCCESS( pthread_mutex_lock(&lock->atom) );
    {
        int value;
        CHECK_SUCCESS( sem_getvalue(&lock->readOnly,&value) );
        /* if no one has entered the readOnly context, then seal off the modify context
           before entering the readOnly context */
        if (value == SEMAPHORE_MAX) {
            CHECK_SUCCESS( sem_wait(&lock->modify) );
            CHECK_SUCCESS( sem_wait(&lock->readOnly) );
            CHECK_SUCCESS( pthread_mutex_unlock(&lock->atom) );
            return;
        }
    }
    CHECK_SUCCESS( pthread_mutex_unlock(&lock->atom) );
    /* default: enter read only context (this could but probably won't block) */
    CHECK_SUCCESS( sem_wait(&lock->readOnly) );
}
void gamelock_down(struct gamelock* lock)
{
    /* make sure the following region executes atomically */
    CHECK_SUCCESS( pthread_mutex_lock(&lock->atom) );
    {
        int value;
        CHECK_SUCCESS( sem_post(&lock->readOnly) );
        CHECK_SUCCESS( sem_getvalue(&lock->readOnly,&value) );
        /* if we are the last to leave the readOnly context,
           then yield access to the modify context */
        if (value == SEMAPHORE_MAX)
            CHECK_SUCCESS( sem_post(&lock->modify) );
    }
    CHECK_SUCCESS( pthread_mutex_unlock(&lock->atom) );
}

/* implement 'pok_timeout' from 'pokgame.h' */
void pok_timeout(struct pok_timeout_interval* interval)
{
    struct timespec before, after;
    clock_gettime(CLOCK_MONOTONIC,&before);
    usleep(interval->useconds);
    clock_gettime(CLOCK_MONOTONIC,&after);
    interval->elapsed = ((uint64_t)1000000000 * (after.tv_sec - before.tv_sec) + (after.tv_nsec - before.tv_nsec)) / 1000000;
}

void pok_timeout_no_elapsed(struct pok_timeout_interval* interval)
{
    /* this variant just does the sleep; it does not compute and set the elapsed time */
    usleep(interval->useconds);
}
