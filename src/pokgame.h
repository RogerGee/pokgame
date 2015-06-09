/* pokgame.h - pokgame */
#ifndef POKGAME_POKGAME_H
#define POKGAME_POKGAME_H
#include "net.h"
#include "graphics.h"
#include "tileman.h"
#include "spriteman.h"
#include "map-context.h"
#include "character-context.h"
#include "effect.h"
#include <dstructs/hashmap.h>
#include <dstructs/treemap.h>

/* timeout_interval: structure to represent game time */
struct pok_timeout_interval
{
    /* timeout duration */
    uint32_t mseconds;
    uint32_t useconds;

    /* how many ticks actually elapsed since the last timeout;
    a tick is defined as a single millisecond */
    uint32_t elapsed;
};
void pok_timeout_interval_reset(struct pok_timeout_interval* t,uint32_t mseconds);
void pok_timeout(struct pok_timeout_interval* interval);

/* pok_game_context: flag current game state */
enum pok_game_context
{
    pok_game_intro_context, /* the game is processing the intro screen */
    pok_game_world_context  /* the game is handling map logic */
};

/* this structure stores all of the top-level game information */
struct pok_game_info
{
    /* controls the io and update procedures */
    bool_t control;

    /* timeouts for main game procedures (in thousandths of a second) */
    struct pok_timeout_interval ioTimeout;
    struct pok_timeout_interval updateTimeout;

    /* flag what the game is currently doing */
    enum pok_game_context gameContext;

    /* graphics */
    struct pok_graphics_subsystem* sys;

    /* effects */
    struct pok_fadeout_effect fadeout;

    /* tile images */
    struct pok_tile_manager* tman;

    /* sprite images */
    struct pok_sprite_manager* sman;

    /* maps */
    struct pok_map dummyMap; /* used as key to 'loadedMaps' */
    struct treemap* loadedMaps;
    struct pok_map_render_context* mapRC;

    /* character render context */
    struct pok_character_render_context* charRC;

    /* player */
    struct pok_character* player;
    struct pok_character_context* playerContext; /* cached */
};

/* these functions provide mutual exclusion when an object is edited; the 'modify' functions
   should be called to ensure code may modify the specified object undisturbed; if the code
   need only read an object, then the 'lock' function should be called; these functions sleep
   the thread if necessary */
void pok_game_modify_enter(void* object); /* enter modify context */
void pok_game_modify_exit(void* object); /* exit modify context */
void pok_game_lock(void* object); /* ensure that 'object' is not being modified (read-only access) */
void pok_game_unlock(void* object); /* exit 'lock' context (read-only access) */

/* main pokgame procedures (the other procedure is graphics which is handled by the graphics subsystem) */
int io_proc(struct pok_game_info* info);
int update_proc(struct pok_game_info* info);

/* module load/unload */
void pok_game_load_module();
void pok_game_unload_module();

/* game initialization/closing */
struct pok_game_info* pok_game_new();
void pok_game_free(struct pok_game_info* game);
void pok_game_register(struct pok_game_info* game);
void pok_game_unregister(struct pok_game_info* game);
void pok_game_add_map(struct pok_game_info* game,struct pok_map* map,bool_t focus);

#endif
