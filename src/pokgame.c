/* pokgame.c - pokgame */
#include "pokgame.h"
#include "error.h"
#include "user.h"
#include <dstructs/hashmap.h>
#include <stdlib.h>
#include <time.h>

#ifndef POKGAME_TEST

const char* POKGAME_NAME;

/* pokgame entry point */
int main(int argc,const char* argv[])
{
    struct pok_graphics_subsystem* sys;
    POKGAME_NAME = argv[0];

    /* C-library initialization */
    srand( time(NULL) );

    /* load all modules */
    pok_exception_load_module();
    pok_user_load_module();
    pok_netobj_load_module();
    pok_game_load_module();

    /* initialize a graphics subsystem for the game; this corresponds to the
       application's top-level window; start up the window and renderer before
       anything else; depending on the platform, the renderer may be run on
       another thread or require the main thread */
    sys = pok_graphics_subsystem_new();
    pok_graphics_subsystem_default(sys);
    if ( !pok_graphics_subsystem_begin(sys) )
        pok_error(pok_error_fatal,"could not begin graphics subsystem");

    if ( !sys->background ) {
        /* the platform cannot run the window on another thread, so start up the IO
           procedure on a background thread and run the window on this thread */


    }
    else
        /* begin the IO procedure; this is the entry point into the game */
        io_proc(sys);

    /* close graphics subsystem: if the graphics subsystem was working on another
       thread this will wait to join back up with said thread; otherwise the graphics
       subsystem has already finished and is waiting to be cleaned up */
    pok_graphics_subsystem_free(sys);

    /* unload all modules */
    pok_game_unload_module();
    pok_netobj_unload_module();
    pok_user_unload_module();
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
struct pok_game_info* pok_game_new(struct pok_graphics_subsystem* sys,struct pok_game_info* template)
{
    /* this function creates a new game state object; if 'template' is not NULL, then its static network
       objects are copied by reference into the new game info */
    struct pok_game_info* game;
    const struct pok_user_info* userInfo = pok_user_get_info();
    game = malloc(sizeof(struct pok_game_info));
    if (game == NULL)
        pok_error(pok_error_fatal,"failed memory allocation in pok_game_new()");
    /* initialize general parameters */
    pok_timeout_interval_reset(&game->ioTimeout,100);
    pok_timeout_interval_reset(&game->updateTimeout,10); /* needs to be pretty high-resolution for good performance */
    game->staticOwnerMask = template == NULL ? (2 << _pok_static_obj_top) - 1 : 0x00;
    game->control = TRUE;
    game->gameContext = pok_game_intro_context;
    game->versionProc = NULL;
    game->versionCBack = NULL;
    pok_string_init(&game->versionLabel);
    game->versionChannel = NULL;
    game->updateThread = pok_thread_new((pok_thread_entry)update_proc,game);
    if (game->updateThread == NULL)
        pok_error_fromstack(pok_error_fatal);
    /* assign graphics subsystem */
    game->sys = sys;
    /* initialize effects */
    pok_fadeout_effect_init(&game->fadeout);
    game->fadeout.keep = TRUE;
    if (template == NULL) {
        /* initialize tile and sprite image managers; we will own these objects */
        game->tman = pok_tile_manager_new(game->sys);
        if (game->tman == NULL)
            pok_error_fromstack(pok_error_fatal);
        game->sman = pok_sprite_manager_new(game->sys);
        if (game->sman == NULL)
            pok_error_fromstack(pok_error_fatal);
    }
    else {
        /* assign tile and sprite image managers from the existing game; the
           caller should take care not to delete the template game before this one */
        game->tman = template->tman;
        game->sman = template->sman;
    }
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
    /* initialize player character and add it to the character render context */
    game->player = pok_character_new();
    if (game->player == NULL)
        pok_error_fromstack(pok_error_fatal);
    game->player->isPlayer = TRUE;
    if (userInfo != NULL)
        game->player->spriteIndex = userInfo->sprite;
    game->playerEffect = pok_character_normal_effect;
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
    if (game->staticOwnerMask & 0x01)
        pok_sprite_manager_free(game->sman);
    if (game->staticOwnerMask & 0x02)
        pok_tile_manager_free(game->tman);
    pok_thread_free(game->updateThread);
    pok_string_delete(&game->versionLabel);
    if (game->versionProc != NULL)
        pok_process_free(game->versionProc);
    if (game->versionChannel != NULL)
        pok_data_source_free(game->versionChannel);
    free(game);
}
void pok_game_static_replace(struct pok_game_info* game,enum pok_static_obj_kind kind,void* obj)
{
    /* replace the specified static network object; the 'pok_game_info' instance will assume
       ownership of the object; free any owned object before assigning */
    switch (kind) {
    case pok_static_obj_tile_manager:
        if (game->staticOwnerMask & 0x01)
            pok_tile_manager_free(game->tman);
        else
            game->staticOwnerMask |= 0x01;
        game->tman = obj;
        break;
    case pok_static_obj_sprite_manager:
        if (game->staticOwnerMask & 0x02)
            pok_sprite_manager_free(game->sman);
        else
            game->staticOwnerMask |= 0x02;
        game->sman = obj;
        break;
    default:
#ifdef POKGAME_DEBUG
        pok_error(pok_error_fatal,"bad static network object kind to pok_game_static_replace()");
#endif
        return;
    }
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
void pok_game_load_textures(struct pok_game_info* game)
{
    pok_graphics_subsystem_create_textures(
        game->sys,
        2,
        game->tman->tileset, game->tman->tilecnt,
        game->sman->spritesets, game->sman->imagecnt );
}
void pok_game_delete_textures(struct pok_game_info* game)
{
    pok_graphics_subsystem_delete_textures(
        game->sys,
        2,
        game->tman->tileset, game->tman->tilecnt,
        game->sman->spritesets, game->sman->imagecnt );
}
