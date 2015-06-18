/* pokgame.c - pokgame */
#include "pokgame.h"
#include "error.h"
#include <dstructs/hashmap.h>
#include <stdlib.h>

#ifndef POKGAME_TEST

const char* POKGAME_NAME;

/* pokgame entry point */
int main(int argc,const char* argv[])
{
    POKGAME_NAME = argv[0];

    /* load all modules */
    pok_exception_load_module();
    pok_network_object_load_module();
    pok_game_load_module();

    /* unload all modules */
    pok_game_unload_module();
    pok_network_object_unload_module();
    pok_exception_unload_module();

    return 0;
}

#endif

/* pok game module - mutual exclusion */

struct gamelock;
static struct gamelock* glock; /* global lock */
static struct hashmap locks; /* void* --> void* */
static struct gamelock* gamelock_new(void* object);
static void gamelock_free(struct gamelock* lock);
static void gamelock_aquire(struct gamelock* lock);
static void gamelock_release(struct gamelock* lock);
static void gamelock_up(struct gamelock* lock);
static void gamelock_down(struct gamelock* lock);

/* include platform-specific code */
#if defined(POKGAME_POSIX)
#include "pokgame-posix.c"
#elif defined(POKGAME_WIN32)
#include "pokgame-win32.c"
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
    hashmap_delete_ex(&locks,(destructor)gamelock_free);
    gamelock_free(glock);
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

/* pok_timeout_interval */
void pok_timeout_interval_reset(struct pok_timeout_interval* t,uint32_t mseconds)
{
    t->mseconds = mseconds;
    t->useconds = mseconds * 1000;
    t->elapsed = 0;
}

/* pok_game_info */
struct pok_game_info* pok_game_new(struct pok_graphics_subsystem* sys)
{
    struct pok_game_info* game;
    game = malloc(sizeof(struct pok_game_info));
    if (game == NULL)
        pok_error(pok_error_fatal,"failed memory allocation in pok_game_new()");
    /* initialize general parameters */
    pok_timeout_interval_reset(&game->ioTimeout,100);
    pok_timeout_interval_reset(&game->updateTimeout,10); /* needs to be pretty high-resolution for good performance */
    game->control = TRUE;
    game->gameContext = pok_game_intro_context;
    game->versionProc = NULL;
    game->versionCBack = NULL;
    game->versionChannel = NULL;
    /* assign graphics subsystem */
    game->sys = sys;
    /* initialize effects */
    pok_fadeout_effect_init(&game->fadeout);
    game->fadeout.keep = TRUE;
    /* initialize tile and sprite image managers */
    game->tman = pok_tile_manager_new(game->sys);
    if (game->tman == NULL)
        pok_error_fromstack(pok_error_fatal);
    game->sman = pok_sprite_manager_new(game->sys);
    if (game->sman == NULL)
        pok_error_fromstack(pok_error_fatal);
    /* initialize maps */
    game->world = pok_world_new();
    if (game->world == NULL)
        pok_error_fromstack(pok_error_fatal);
    game->mapTrans = NULL;
    game->mapRC = pok_map_render_context_new(game->tman);
    if (game->mapRC == NULL)
        pok_error_fromstack(pok_error_fatal);
    /* initialize character render context */
    game->charRC = pok_character_render_context_new(game->mapRC,game->sman);
    /* initialize player and add it to the character render context */
    game->player = pok_character_new();
    if (game->player == NULL)
        pok_error_fromstack(pok_error_fatal);
    game->player->isPlayer = TRUE;
    game->playerContext = pok_character_render_context_add_ex(game->charRC,game->player);
    if (game->playerContext == NULL)
        pok_error_fromstack(pok_error_fatal);
    return game;
}
void pok_game_free(struct pok_game_info* game)
{
    pok_character_free(game->player);
    pok_character_render_context_free(game->charRC);
    pok_world_free(game->world);
    pok_map_render_context_free(game->mapRC);
    pok_sprite_manager_free(game->sman);
    pok_tile_manager_free(game->tman);
    free(game);
}
void pok_game_register(struct pok_game_info* game)
{
    /* the order of the graphics routines is important */
    pok_graphics_subsystem_register(game->sys,(graphics_routine_t)pok_map_render,game->mapRC);
    pok_graphics_subsystem_register(game->sys,(graphics_routine_t)pok_character_render,game->charRC);
    pok_graphics_subsystem_register(game->sys,(graphics_routine_t)pok_fadeout_effect_render,&game->fadeout);
}
void pok_game_unregister(struct pok_game_info* game)
{
    pok_graphics_subsystem_unregister(game->sys,(graphics_routine_t)pok_map_render,game->mapRC);
    pok_graphics_subsystem_unregister(game->sys,(graphics_routine_t)pok_character_render,game->charRC);
    pok_graphics_subsystem_unregister(game->sys,(graphics_routine_t)pok_fadeout_effect_render,&game->fadeout);
}
