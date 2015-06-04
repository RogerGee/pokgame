/* pokgame.c - pokgame */
#include "pokgame.h"
#include "error.h"
#include <stdlib.h>

#ifndef POKGAME_TEST

/* pokgame entry point */
int main(int argc,const char* argv[])
{
    /* load all modules */
    pok_exception_load_module();
    pok_game_load_module();

    /* unload all modules */
    pok_game_unload_module();
    pok_exception_unload_module();

    return 0;
}

#endif

/* pok game module - mutual exclusion */

struct gamelock;
static struct gamelock* glock; /* global lock */
static struct hashmap locks; /* void* --> void* */
static struct gamelock* gamelock_new(void* object);
static void gamelock_aquire(struct gamelock* lock);
static void gamelock_release(struct gamelock* lock);
static void gamelock_up(struct gamelock* lock);
static void gamelock_down(struct gamelock* lock);

/* include platform-specific code */
#if defined(POKGAME_POSIX)
#include "pokgame-posix.c"
#endif

static int gamelock_hash(const void** obj,int size)
{
    return (long long int)*obj % size;
}

static int gamelock_compar(const struct gamelock* left,const struct gamelock* right)
{
    long long int result = (long long int)left->object - (long long int)right->object;
    return result < 0 ? -1 : (result > 0 ? 1 : 0);
}

void pok_game_load_module()
{
    glock = gamelock_new(NULL);
    hashmap_init(&locks,20,(hash_function)gamelock_hash,(key_comparator)gamelock_compar);
}
void pok_game_unload_module()
{
    hashmap_delete_ex(&locks,free);
    free(glock);
}

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

/* pok game module - intialization and closing */

struct pok_game_info* pok_game_new()
{
    struct pok_game_info* game;
    game = malloc(sizeof(struct pok_game_info));
    if (game == NULL)
        pok_error(pok_error_fatal,"failed memory allocation in pok_game_new()");
    /* initialize general parameters */
    game->ioTimeout = 100;
    game->updateTimeout = 1000 / INITIAL_FRAMERATE / 2; /* 2 frames for every update cycle */
    game->control = TRUE;
    /* initialize graphics subsystem */
    game->sys = pok_graphics_subsystem_new();
    if (game->sys == NULL)
        pok_error_fromstack(pok_error_fatal);
    /* initialize tile and sprite image managers */
    game->tman = pok_tile_manager_new(game->sys);
    if (game->tman == NULL)
        pok_error_fromstack(pok_error_fatal);
    game->sman = pok_sprite_manager_new(game->sys);
    if (game->sman == NULL)
        pok_error_fromstack(pok_error_fatal);
    /* initialize maps */
    game->loadedMaps = treemap_new((key_comparator)pok_map_compar,(destructor)pok_map_free);
    if (game->loadedMaps == NULL)
        pok_error(pok_error_fatal,"failed memory allocation in pok_game_new()");
    game->mapRC = pok_map_render_context_new(game->tman);
    if (game->mapRC == NULL)
        pok_error_fromstack(pok_error_fatal);
    pok_map_init(&game->dummyMap);
    /* initialize character render context */
    game->charRC = pok_character_render_context_new(game->mapRC,game->sman);
    /* initialize player and add it to the character render context */
    game->player = pok_character_new();
    if (game->player == NULL)
        pok_error_fromstack(pok_error_fatal);
    game->playerContext = pok_character_render_context_add_ex(game->charRC,game->player);
    if (game->playerContext == NULL)
        pok_error_fromstack(pok_error_fatal);
    return game;
}
void pok_game_free(struct pok_game_info* game)
{
    pok_character_free(game->player);
    pok_character_render_context_free(game->charRC);
    pok_map_render_context_free(game->mapRC);
    treemap_free(game->loadedMaps);
    pok_sprite_manager_free(game->sman);
    pok_tile_manager_free(game->tman);
    pok_graphics_subsystem_free(game->sys);
    free(game);
}
void pok_game_register(struct pok_game_info* game)
{
    /* the order of the graphics routines is important */
    pok_graphics_subsystem_register(game->sys,(graphics_routine_t)pok_map_render,game->mapRC);
    pok_graphics_subsystem_register(game->sys,(graphics_routine_t)pok_character_render,game->charRC);
}
void pok_game_unregister(struct pok_game_info* game)
{
    pok_graphics_subsystem_unregister(game->sys,(graphics_routine_t)pok_map_render,game->mapRC);
    pok_graphics_subsystem_unregister(game->sys,(graphics_routine_t)pok_character_render,game->charRC);
}
void pok_game_add_map(struct pok_game_info* game,struct pok_map* map,bool_t focus)
{
    treemap_insert(game->loadedMaps,map);
    if (focus)
        pok_map_render_context_set_map(game->mapRC,map);
}

void timeout_interval_reset(struct timeout_interval* t,int mseconds)
{
    t->mseconds = mseconds;
    t->useconds = mseconds * 1000;
    t->ticksEighthSecond = 125 / t->mseconds;
    t->ticksFourthSecond = 250 / t->mseconds;
    t->ticksHalfSecond = 500 / t->mseconds;
    t->ticksSecond = 1000 / t->mseconds;

    /* these need to be a valid denominator */
    if (t->ticksEighthSecond == 0)
        t->ticksEighthSecond = 1;
    if (t->ticksFourthSecond == 0)
        t->ticksFourthSecond = 1;
    if (t->ticksHalfSecond == 0)
        t->ticksHalfSecond = 1;
    if (t->ticksSecond == 0)
        t->ticksSecond = 1;
}
