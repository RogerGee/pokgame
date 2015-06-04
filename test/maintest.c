/* maintest.c -- pokgame main test bed */
#include "pokgame.h"
#include "error.h"
#include <stdio.h>
#include <assert.h>
#include <unistd.h>

/* globals */
static struct pok_game_info* game;

/* functions */
static void init();
static void load_maps();

/* entry point */
int main_test()
{
    game = pok_game_new();
    init();
    update_proc(game);
    pok_game_free(game);
    return 0;
}

/* implementation */

void init()
{
    static const struct pok_point ORIGIN = {0,0};
    static const struct pok_location START_LOCATION = {4,6};
    fputs("doing init...",stdout);
    fflush(stdout);
    pok_graphics_subsystem_default(game->sys);
    if ( !pok_tile_manager_fromfile_tiles(game->tman,"test/img/sts1.data") )
        pok_error_fromstack(pok_error_fatal);
    game->tman->impassability = 32;
    if ( !pok_sprite_manager_fromfile(game->sman,"test/img/sss1.data") )
        pok_error_fromstack(pok_error_fatal);
    load_maps();
    assert( pok_map_render_context_center_on(game->mapRC,&ORIGIN,&START_LOCATION) );
    pok_character_context_set_player(game->playerContext,game->mapRC);
    assert( pok_graphics_subsystem_begin(game->sys) );
    pok_game_register(game);
    puts("done");
}

void load_maps()
{
    /* load maps; focus on start map */
    struct pok_map* map;
    map = pok_map_new();
    if (map == NULL)
        pok_error_fromstack(pok_error_fatal);
    if ( !pok_map_fromfile_csv(map,"test/maps/mapA.csv") )
        pok_error_fromstack(pok_error_fatal);
    pok_game_add_map(game,map,TRUE);
}
