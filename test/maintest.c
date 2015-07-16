/* maintest.c -- pokgame main test bed; this
   test only tests render and update functionality,
   not the IO functionality */
#include "pokgame.h"
#include "error.h"
#include "pok.h"
#include <stdio.h>
#include <assert.h>

/* data */
struct pok_tile_ani_data ANIDATA[] = {
    {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0},
    {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0},
    {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0},
    {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0},
    {0,0,0,0}, {2,27,24,0}, {2,28,25,0}, {2,29,26,0}, {3,21,0,0},
    {3,22,0,0}, {3,23,0,0}, {3,0,21,0}, {3,0,22,0}, {3,0,23,0},
    {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0},
    {3,36,0,0}, {2,37,35,0}, {3,0,36,0}, {6,39,0,0}, {3,38,0,0},
    {6,41,0,0}, {3,40,0,0}
};

/* globals */
static struct pok_game_info* game;
static struct pok_character* friend;
static struct pok_character* dude1;

/* functions */
static void init();
static void load_maps();
static void load_characters();
static void aux_graphics_load();
static void aux_graphics_unload();
static int play_game();

/* entry point */
int main_test()
{
    struct pok_graphics_subsystem* sys;
    sys = pok_graphics_subsystem_new();
    if (sys == NULL)
        pok_error_fromstack(pok_error_fatal);
    sys->loadRoutine = aux_graphics_load;
    sys->unloadRoutine = aux_graphics_unload;
    game = pok_game_new(sys,NULL);
    init();
    printf("finished: %d\n",play_game());
    pok_game_free(game);
    pok_character_free(friend);
    pok_character_free(dude1);
    pok_graphics_subsystem_free(sys);
    return 0;
}

/* implementation */

void init()
{
    static const struct pok_location START_LOCATION = {4,6};
    fputs("doing init...",stdout);
    fflush(stdout);
    pok_graphics_subsystem_default(game->sys);
    if ( !pok_tile_manager_fromfile_tiles(game->tman,"test/img/sts0.data") )
        pok_error_fromstack(pok_error_fatal);
    game->tman->impassibility = 32;
    game->tman->terrain[pok_tile_terrain_ice].length = 1;
    game->tman->terrain[pok_tile_terrain_ice].list = malloc(sizeof(uint16_t));
    assert( game->tman->terrain[pok_tile_terrain_ice].list != NULL );
    game->tman->terrain[pok_tile_terrain_ice].list[0] = 35;
    game->tman->terrain[pok_tile_terrain_ledge_down].length = 1;
    game->tman->terrain[pok_tile_terrain_ledge_down].list = malloc(sizeof(uint16_t));
    assert( game->tman->terrain[pok_tile_terrain_ledge_down].list != NULL );
    game->tman->terrain[pok_tile_terrain_ledge_down].list[0] = 1;
    assert( pok_tile_manager_load_ani(game->tman,42,ANIDATA,TRUE) );
    if ( !pok_sprite_manager_fromfile(game->sman,"test/img/sss0.data",pok_sprite_manager_updown_alt) )
        pok_error_fromstack(pok_error_fatal);
    load_maps();
    load_characters();
    assert( pok_map_render_context_center_on(game->mapRC,&ORIGIN,&START_LOCATION) );
    pok_character_context_set_player(game->playerContext,game->mapRC);
    assert( pok_graphics_subsystem_begin(game->sys) );
    pok_game_load_textures(game);
    pok_game_register(game);
    puts("done");
}

void load_maps()
{
    /* load start map and focus on it */
    struct pok_map* map;
    map = pok_map_new();
    if (map == NULL)
        pok_error_fromstack(pok_error_fatal);
    map->mapNo = 1;
    if ( !pok_map_fromfile_csv(map,"test/maps/mapA.csv") )
        pok_error_fromstack(pok_error_fatal);
    pok_world_add_map(game->world,map);
    pok_map_render_context_set_map(game->mapRC,map);
    /* load map B */
    map = pok_map_new();
    if (map == NULL)
        pok_error_fromstack(pok_error_fatal);
    map->mapNo = 2;
    if ( !pok_map_fromfile_csv(map,"test/maps/mapB.csv") )
        pok_error_fromstack(pok_error_fatal);
    pok_world_add_map(game->world,map);
    /* load map C */
    map = pok_map_new();
    if (map == NULL)
        pok_error_fromstack(pok_error_fatal);
    map->mapNo = 3;
    if ( !pok_map_fromfile_csv(map,"test/maps/mapC.csv") )
        pok_error_fromstack(pok_error_fatal);
    pok_world_add_map(game->world,map);

    /* load warps for all maps */
    if ( !pok_world_fromfile_warps(game->world,"test/maps/warps") )
        pok_error_fromstack(pok_error_fatal);
}

void load_characters()
{
    friend = pok_character_new();
    friend->mapNo = 1;
    friend->tilePos = (struct pok_location){22,16};
    friend->direction = pok_direction_right;
    dude1 = pok_character_new();
    dude1->mapNo = 2;
    dude1->tilePos = (struct pok_location){4,4};
    assert( pok_character_render_context_add(game->charRC,friend) );
    assert( pok_character_render_context_add(game->charRC,dude1) );
}

void aux_graphics_load()
{
    pok_glyphs_load();
}

void aux_graphics_unload()
{
    pok_glyphs_unload();
}

int play_game()
{
    int retval;
    struct pok_thread* upthread;
    /* if the UI is running in the background, the use this thread to 
       run the update procedure */
    if (game->sys->background)
        return update_proc(game);
    /* otherwise run the UI on this thread and run the update procedure in the background */
    upthread = pok_thread_new((pok_thread_entry)update_proc,game);
    pok_thread_start(upthread);
    pok_graphics_subsystem_render_loop(game->sys);
    retval = pok_thread_join(upthread);
    pok_thread_free(upthread);
    return retval;
}
