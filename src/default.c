/* default.c - pokgame */
#include "default.h"
#include "error.h"
#include "config.h"
#include "standard1.h" /* use standard game artwork version 1 */
#include <stdlib.h>

/* these macros check an expression for failure; 'N' checks for NULL and 'B'
   checks for Boolean FALSE */
#define N(expr) \
    if ((expr) == NULL) \
        pok_error_fromstack(pok_error_fatal)
#define B(expr) \
    if (!(expr)) \
        pok_error_fromstack(pok_error_fatal)

/* functions */
static void setup_tileman(struct pok_tile_manager* tman);
static void setup_spriteman(struct pok_sprite_manager* sman);
static struct pok_map* create_default_map();
static struct pok_game_info* default_io_proc(struct pok_game_info* game);

struct pok_game_info* pok_make_default_game(struct pok_graphics_subsystem* sys)
{
    struct pok_map* defmap;
    struct pok_game_info* game;

    /* create a new game for the default version; register a callback to handle its
       input and output */
    game = pok_game_new(sys,NULL);
    game->versionCBack = default_io_proc;
    setup_tileman(game->tman);
    setup_spriteman(game->sman);
    defmap = create_default_map();
    B( pok_world_add_map(game->world,defmap) );

    /* prepare rendering contexts for initial scene */
    pok_map_render_context_set_map(game->mapRC,defmap);
    B( pok_map_render_context_center_on(game->mapRC,&ORIGIN,&DEFAULT_MAP_START_LOCATION) );
    pok_character_context_set_player(game->playerContext,game->mapRC); /* align the player context with map context */

    return game;
}

void setup_tileman(struct pok_tile_manager* tman)
{
    /* load tile manager for the default game */
    int i;
    B( pok_tile_manager_fromfile_tiles_png(tman,POKGAME_INSTALL_DIRECTORY POKGAME_DEFAULT_DIRECTORY POKGAME_STS_IMAGE) );
    tman->impassibility = DEFAULT_TILEMAN_IMPASSIBILITY;
    tman->flags |= pok_tile_manager_flag_terrain_byref;
    for (i = 0;i < POK_TILE_TERRAIN_TOP;++i) {
        /* the first element of a default terrain info list is the number of elements that proceed it */
        tman->terrain[i].length = DEFAULT_TILEMAN_TERRAIN_INFO[i][0];
        if (tman->terrain[i].length > 0)
            tman->terrain[i].list = DEFAULT_TILEMAN_TERRAIN_INFO[i] + 1;
    }
    B( pok_tile_manager_load_ani(tman,DEFAULT_TILEMAN_ANI_DATA_LENGTH,DEFAULT_TILEMAN_ANI_DATA,TRUE) );
}

void setup_spriteman(struct pok_sprite_manager* sman)
{
    B( pok_sprite_manager_fromfile_png(
            sman,
            POKGAME_INSTALL_DIRECTORY POKGAME_DEFAULT_DIRECTORY POKGAME_SSS_IMAGE,
            pok_sprite_manager_updown_alt) );
}

struct pok_map* create_default_map()
{
    /* create the default map; this will serve as a portal hub from which the player may enter the default version
       or connect to another version process */
    struct pok_map* map;
    N( map = pok_map_new() );
    B( pok_map_configure(map,&DEFAULT_MAP_CHUNK_SIZE,DEFAULT_MAP_CHUNK,DEFAULT_MAP_CHUNK_SIZE.columns * DEFAULT_MAP_CHUNK_SIZE.rows) );

    return map;
}

struct pok_game_info* default_io_proc(struct pok_game_info* game)
{
    /* loop while the window is still open */
    while ( pok_graphics_subsystem_has_window(game->sys) ) {


        /* time the thread out (don't compute elapsed time) */
        pok_timeout_no_elapsed(&game->ioTimeout);
    }
    return NULL;
}
