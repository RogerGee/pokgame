/* gamelock.c - pokgame */
#include "gamelock.h"
#include "error.h"
#include <dstructs/hashmap.h>
#include <stdlib.h>

static int gamelock_hash(const void** obj,int size)
{
    return (long long int)*obj % size;
}

/* global game object collections */
static struct gamelock* glock; /* global lock */
static struct hashmap locks; /* void* --> void* */

/* module load/unload functions */
void pok_gamelock_load_module()
{
    glock = gamelock_new(NULL);
    hashmap_init(&locks,20,(hash_function)gamelock_hash,(key_comparator)gamelock_compar);
}
void pok_gamelock_unload_module()
{
    hashmap_delete_ex(&locks,(destructor)gamelock_free);
    gamelock_free(glock);
}

/* pok_timeout_interval */
void pok_timeout_interval_reset(struct pok_timeout_interval* t,uint32_t mseconds)
{
    t->mseconds = mseconds;
    t->useconds = mseconds * 1000;
    t->nseconds = mseconds * 1000000;
    t->elapsed = 0;
}

/* global game locks */
void pok_game_modify_enter(void* object)
{
    /* see if a gamelock exists for the specified object */
    struct gamelock* lock;
    gamelock_up(glock);
    lock = hashmap_lookup(&locks,&object); /* thread safe */
    gamelock_down(glock);
    if (lock == NULL) {
        /* create a new lock */
        lock = gamelock_new(object);
        if (lock == NULL)
            pok_error(pok_error_fatal,"memory fail in pok_game_modify_enter()");
        gamelock_aquire(glock);
        hashmap_insert(&locks,lock);
        gamelock_release(glock);
    }
    gamelock_aquire(lock);
}
void pok_game_modify_exit(void* object)
{
    /* see if a gamelock exists for the specified object (it should) */
    struct gamelock* lock;
    gamelock_up(glock);
    lock = hashmap_lookup(&locks,&object); /* thread safe */
    gamelock_down(glock);
    if (lock != NULL)
        gamelock_release(lock);
}
void pok_game_lock(void* object)
{
    struct gamelock* lock;
    gamelock_up(glock);
    lock = hashmap_lookup(&locks,&object); /* thread safe */
    if (lock == NULL) {
        gamelock_down(glock);
        /* create a new lock */
        lock = gamelock_new(object);
        if (lock == NULL)
            pok_error(pok_error_fatal,"memory fail in pok_game_modify_enter()");
        gamelock_aquire(glock);
        hashmap_insert(&locks,lock);
        gamelock_release(glock);
        gamelock_up(glock);
    }
    gamelock_up(lock);
    gamelock_down(glock);
}
void pok_game_unlock(void* object)
{
    struct gamelock* lock;
    gamelock_up(glock);
    lock = hashmap_lookup(&locks,&object); /* thread safe */
    if (lock == NULL) {
        gamelock_down(glock);
        /* create a new lock */
        lock = gamelock_new(object);
        if (lock == NULL)
            pok_error(pok_error_fatal,"memory fail in pok_game_modify_enter()");
        gamelock_aquire(glock);
        hashmap_insert(&locks,lock);
        gamelock_release(glock);
        gamelock_up(glock);
    }
    gamelock_down(lock);
    gamelock_down(glock);
}

/* include platform specific code */
#if defined(POKGAME_POSIX)
#include "gamelock-posix.c"
#elif defined(POKGAME_WIN32)
#include "gamelock-win32.c"
#endif
