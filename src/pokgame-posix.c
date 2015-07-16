/* pokgame-posix.c - pokgame */
#include <pthread.h>
#include <unistd.h>

#ifdef POKGAME_OSX
#include <sys/time.h>
#include <dispatch/dispatch.h>
#define CLOCK_MONOTONIC 0

/* clock_gettime is not implemented on OSX */
int clock_gettime(int id,struct timespec* t) {
    struct timeval now;
    int rv = gettimeofday(&now,NULL);
    if (rv) return rv;
    t->tv_sec  = now.tv_sec;
    t->tv_nsec = now.tv_usec * 1000;
    return 0;
    (void)id;
}

/* OSX doesn't implement POSIX semaphores completely so we wrap other semaphore calls */
typedef struct {
    dispatch_semaphore_t sem;
    int counter;
} sem_t;
static int sem_init(sem_t* sem,int pshared,unsigned int value)
{
    sem->sem = dispatch_semaphore_create(value);
    sem->counter = value;
    return 0;
    (void)pshared;
}
static int sem_wait(sem_t* sem)
{
    dispatch_semaphore_wait(sem->sem,DISPATCH_TIME_FOREVER);
    --sem->counter;
    return 0;
}
static int sem_post(sem_t* sem)
{
    dispatch_semaphore_signal(sem->sem);
    ++sem->counter;
    return 0;
}
static int sem_getvalue(sem_t* sem,int* sval)
{
    *sval = sem->counter;
    return 0;
}

#elif defined(POKGAME_LINUX)
#include <semaphore.h>

#endif

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
    sem_t readOnly;        /* this semaphore allows more than 1 process to read the object */
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
    /* aquire writer's lock */
    CHECK_SUCCESS( sem_wait(&lock->modify) );
}
void gamelock_release(struct gamelock* lock)
{
    /* release writer's lock */
    CHECK_SUCCESS( sem_post(&lock->modify) );
}
void gamelock_up(struct gamelock* lock)
{
    /* aquire readers' lock; make the following region executes atomically; the
       mutex will also act as a queue for clients wishing to aquire read lock */
    CHECK_SUCCESS( pthread_mutex_lock(&lock->atom) );
    {
        /* since this is a critical region, then we can accurately read the semaphore's value */
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
    /* release readers' lock; make sure the following region executes atomically; the
       mutex will also act as a queue for clients wishing to release read lock */
    CHECK_SUCCESS( pthread_mutex_lock(&lock->atom) );
    {
        /* since this is a critical region, then we can accurately read the semaphore's value */
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
