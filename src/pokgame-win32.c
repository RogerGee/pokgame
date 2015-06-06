#include <Windows.h>

struct gamelock
{
    void* object;
    int updown;
    HANDLE wait;
    HANDLE aquire;
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
    lock->wait = CreateMutex(NULL, FALSE, NULL);
    lock->aquire = CreateMutex(NULL, FALSE, NULL);
    if (lock->wait == NULL || lock->aquire == NULL) {
        pok_error(pok_error_fatal, "fail CreateMutex()");
        return NULL;
    }
    return lock;
}
void gamelock_free(struct gamelock* lock)
{
    CloseHandle(lock->wait);
    CloseHandle(lock->aquire);
    free(lock);
}
void gamelock_aquire(struct gamelock* lock)
{
    WaitForSingleObject(lock->wait,INFINITE);
    WaitForSingleObject(lock->aquire,INFINITE);
    ReleaseMutex(lock->wait);
}
void gamelock_release(struct gamelock* lock)
{
    ReleaseMutex(lock->aquire);
}
void gamelock_up(struct gamelock* lock)
{
    WaitForSingleObject(lock->wait,INFINITE);
    if (lock->updown++ == 0)
        WaitForSingleObject(lock->aquire,INFINITE);
    ReleaseMutex(lock->wait);
}
void gamelock_down(struct gamelock* lock)
{
    if (--lock->updown == 0)
        ReleaseMutex(lock->aquire);
}

/* implement 'timeout' from 'pokgame.h' */
void timeout(struct timeout_interval* interval)
{
    Sleep(interval->mseconds);
}
