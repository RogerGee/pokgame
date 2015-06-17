#include <Windows.h>

#define MAX_SEMAPHORE 10

struct gamelock
{
    void* object;
    HANDLE readOnly;
    HANDLE modify;
    HANDLE atom;
    INT updown;
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
    lock->readOnly = CreateSemaphore(NULL, MAX_SEMAPHORE, MAX_SEMAPHORE, NULL);
    lock->modify = CreateSemaphore(NULL, 1, 1, NULL);
    lock->atom = CreateMutex(NULL, FALSE, NULL);
    lock->updown = 0;
    if (lock->readOnly == NULL || lock->modify == NULL)
        pok_error(pok_error_fatal, "fail CreateSemaphore()");
    if (lock->atom == NULL)
        pok_error(pok_error_fatal, "fail CreateMutex()");
    return lock;
}
void gamelock_free(struct gamelock* lock)
{
    CloseHandle(lock->readOnly);
    CloseHandle(lock->modify);
    CloseHandle(lock->atom);
    free(lock);
}
void gamelock_aquire(struct gamelock* lock)
{
    WaitForSingleObject(lock->modify, INFINITE);
}
void gamelock_release(struct gamelock* lock)
{
    ReleaseSemaphore(lock->modify, 1, NULL);
}
void gamelock_up(struct gamelock* lock)
{
    WaitForSingleObject(lock->atom, INFINITE);
    if (lock->updown++ == 0)
        /* get inside the modify context */
        WaitForSingleObject(lock->modify, INFINITE);
    ReleaseMutex(lock->atom);
    WaitForSingleObject(lock->readOnly, INFINITE);
}
void gamelock_down(struct gamelock* lock)
{
    WaitForSingleObject(lock->atom, INFINITE);
    if (--lock->updown == 0)
        /* we are the last reader; get outside modify context */
        ReleaseSemaphore(lock->modify, 1, NULL);
    ReleaseMutex(lock->atom);
    ReleaseSemaphore(lock->readOnly, 1, NULL);
}

/* implement 'pok_timeout' from 'pokgame.h' */
void pok_timeout(struct pok_timeout_interval* interval)
{
    /* Windows NT clock interval is 15 ms; I expect the
       elapsed time computed here will be roughly that if
       the timeout period <= 15 ms */
    LARGE_INTEGER before;
    LARGE_INTEGER after;
    static LARGE_INTEGER freq = { 0, 0 };
    if (freq.QuadPart == 0)
        QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&before);
    do {
        Sleep(1);
        QueryPerformanceCounter(&after);
        interval->elapsed = (uint32_t) ((after.QuadPart - before.QuadPart) * 1000 / freq.QuadPart);
    } while (interval->elapsed < interval->mseconds);
}
