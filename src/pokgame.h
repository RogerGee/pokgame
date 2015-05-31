/* pokgame.h - pokgame */
#ifndef POKGAME_POKGAME_H
#define POKGAME_POKGAME_H
#include "net.h"
#include "graphics.h"
#include "tileman.h"
#include "spriteman.h"
#include "map-render.h"
#include <dstructs/hashmap.h>
#include <dstructs/treemap.h>

/* this structure stores all of the top-level game information */
struct pok_game_info
{
    /* timeouts for main game procedures (in thousandths of a second) */
    int ioTimeout;
    int updateTimeout;

    /* graphics */
    struct pok_graphics_subsystem* sys;

    /* tile images */
    struct pok_tile_manager* tman;

    /* sprite images */
    struct pok_sprite_manager* sman;

    /* maps */
    struct pok_map dummyMap; /* used as key to 'loadedMaps' */
    struct treemap* loadedMaps;
    struct pok_map_render_context* mapRC;
};

/* these functions provide mutual exclusion when an object is edited; the 'modify' functions
   should be called to ensure code may modify the specified object undisturbed; if the code
   need only read an object, then the 'lock' function should be called; these functions sleep
   the thread if necessary */
void pok_game_modify_enter(void* object); /* enter modify context */
void pok_game_modify_exit(void* object); /* exit modify context */
void pok_game_lock(void* object); /* ensure that 'object' is not being modified (read-only access) */
void pok_game_unlock(void* object); /* exit 'lock' context (read-only access) */

/* main pokgame procedures (the other procedure is rendering which is handled by the graphics subsystem) */
int io_proc(struct pok_game_info* info);
int update_proc(struct pok_game_info* info);

/* module load/unload */
void pok_game_load_module();
void pok_game_unload_module();

/* game initialization/closing */
struct pok_game_info* pok_game_new();
void pok_game_free(struct pok_game_info* game);

#endif
