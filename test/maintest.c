/* maintest.c -- pokgame main test bed; this
   test only tests render and update functionality,
   not the main IO functionality (though we provide some
   basic functionality to test certain features) */
#include "pokgame.h"
#include "error.h"
#include "user.h"
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
static struct pok_character_context* friend1;
static struct pok_character_context* friend2;

/* functions */
static void init();
static void fail_from_stack();
static void load_maps();
static void load_characters();
static void aux_graphics_load();
static void aux_graphics_unload();
static int play_game();
static int game_io(void*);
static void conn_version();

/* entry point */
int main_test()
{
    struct pok_graphics_subsystem* sys;
    sys = pok_graphics_subsystem_new();
    if (sys == NULL)
        pok_error_fromstack(pok_error_fatal);
    pok_graphics_subsystem_default(sys);
    sys->loadRoutine = aux_graphics_load;
    sys->unloadRoutine = aux_graphics_unload;
    game = pok_game_new(sys,NULL);
    init();
    printf("finished: %d\n",play_game());
    pok_game_delete_textures(game);
    pok_character_free(friend1->character);
    pok_character_free(friend2->character);
    pok_game_free(game);
    pok_graphics_subsystem_free(sys);
    return 0;
}

/* implementation */

void init()
{
    static const struct pok_location START_LOCATION = {4,6};
    fputs("doing init...",stdout);
    fflush(stdout);
    if ( !pok_tile_manager_fromfile_tiles(game->tman,"test/img/sts0.data") )
        fail_from_stack();
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
        fail_from_stack();
    load_maps();
    load_characters();
    assert( pok_map_render_context_center_on(game->mapRC,&ORIGIN,&START_LOCATION) );
    pok_character_context_set_player(game->playerContext,game->mapRC);
    assert( pok_graphics_subsystem_begin(game->sys) );
    pok_game_load_textures(game);
    pok_game_register(game);
    puts("done");
}

void fail_from_stack()
{
    puts("failed");
    pok_error_fromstack(pok_error_fatal);
}

void load_maps()
{
    struct pok_map* map;
    struct pok_data_source* fstream;

    /* load start map and focus on it */
    map = pok_map_new();
    if (map == NULL)
        fail_from_stack();
    map->mapNo = 1;
    if ( !pok_map_fromfile_csv(map,"test/maps/mapA.csv") )
        fail_from_stack();
    pok_world_add_map(game->world,map);
    pok_map_render_context_set_map(game->mapRC,map);

    /* load map B */
    map = pok_map_new();
    if (map == NULL)
        fail_from_stack();
    map->mapNo = 2;
    if ( !pok_map_fromfile_csv(map,"test/maps/mapB.csv") )
        fail_from_stack();
    pok_world_add_map(game->world,map);

    /* load map C */
    map = pok_map_new();
    if (map == NULL)
        fail_from_stack();
    map->mapNo = 3;
    if ( !pok_map_fromfile_csv(map,"test/maps/mapC.csv") )
        fail_from_stack();
    pok_world_add_map(game->world,map);

    /* load map D */
    map = pok_map_new();
    if (map == NULL)
        fail_from_stack();
    fstream = pok_data_source_new_file("test/maps/testmap1",pok_filemode_open_existing,pok_iomode_read);
    if (fstream == NULL)
        fail_from_stack();
    if ( !pok_map_open(map,fstream) )
        fail_from_stack();
    map->mapNo = 4; /* need to set afterwards */
    pok_data_source_free(fstream);
    pok_world_add_map(game->world,map);

    /* load warps for all maps */
    if ( !pok_world_fromfile_warps(game->world,"test/maps/warps") )
        fail_from_stack();
}

void load_characters()
{
    struct pok_character* ch;
    ch = pok_character_new();
    ch->mapNo = 1;
    ch->tilePos = (struct pok_location){22,16};
    ch->direction = pok_direction_right;
    friend1 = pok_character_render_context_add_ex(game->charRC,ch);
    assert(friend1);
    ch = pok_character_new();
    ch->mapNo = 2;
    ch->tilePos = (struct pok_location){4,4};
    friend2 = pok_character_render_context_add_ex(game->charRC,ch);
    assert(friend2);
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
    int testver = 0;
    int retval = 0;
    struct pok_thread* upthread;
    /* create a new thread on which to run the update thread */
    upthread = pok_thread_new((pok_thread_entry)update_proc,game);
    pok_thread_start(upthread);
    /* run the test io procedure */
    if (game->sys->background) {
        /* rendering is done on background thread, so we can use
           this thread for the test io procedure */
        testver = game_io(game);
    }
    else {
        /* the graphics subsystem requires that the rendering is done
           on this main thread; so create a new thread for the IO proc */
        struct pok_thread* gameThread;
        gameThread = pok_thread_new(game_io,game);
        pok_thread_start(gameThread);
        pok_graphics_subsystem_render_loop(game->sys);
        testver = pok_thread_join(gameThread);
        pok_thread_free(gameThread);
    }
    retval = pok_thread_join(upthread);
    pok_thread_free(upthread);

    if (testver) {
        /* begin testing version */
        conn_version();
    }

    return retval;
}

/* implement a minimal IO procedure to test game functionality */
static void noop()
{
    /* take no action; reply with no-op intermsg response */
    game->updateInterMsg.processed = TRUE;
    pok_intermsg_setup(&game->ioInterMsg,pok_noop_intermsg,0);
    game->ioInterMsg.ready = TRUE;
}
static void message_menu(const char* contents)
{
    game->updateInterMsg.processed = TRUE; /* we are responding to update intermsg */
    pok_intermsg_setup(&game->ioInterMsg,pok_menu_intermsg,0);
    game->ioInterMsg.modflags = pok_message_menu;
    pok_string_assign(game->ioInterMsg.payload.string,contents);
    game->ioInterMsg.ready = TRUE;
}
static void input_menu(const char* prompt)
{
    game->updateInterMsg.processed = TRUE; /* we are responding to update intermsg */
    pok_intermsg_setup(&game->ioInterMsg,pok_menu_intermsg,0);
    game->ioInterMsg.modflags = pok_input_menu;
    pok_string_assign(game->ioInterMsg.payload.string,prompt);
    game->ioInterMsg.ready = TRUE;
}
static inline bool_t next_to(struct pok_location* loc)
{
    enum pok_direction dir;
    return (dir = pok_util_is_next_to(
        &game->mapRC->map->chunkSize,
        &game->player->chunkPos,
        &ORIGIN, /* assume chunk pos */
        &game->player->tilePos,
        loc)) != pok_direction_none && pok_direction_opposite(dir) == game->player->direction;
}
static inline bool_t next_to_character(struct pok_character* character,enum pok_direction* outDir)
{
    return (*outDir = pok_util_is_next_to(
            &game->mapRC->map->chunkSize,
            &game->player->chunkPos,
            &character->chunkPos,
            &game->player->tilePos,
            &character->tilePos)) != pok_direction_none && pok_direction_opposite(*outDir) == game->player->direction;
}
int game_io(void* p)
{
    static int context = 0;
    int ret = 0;
    enum pok_direction dir;
    struct pok_location loc;
    struct pok_string stringbuilder;
    pok_string_init(&stringbuilder);
    while ( pok_graphics_subsystem_has_window(game->sys) ) {
        /* check to make sure update intermsg was not discarded */
        pok_game_modify_enter(&game->updateInterMsg);
        if (game->updateInterMsg.kind != pok_uninitialized_intermsg && game->updateInterMsg.ready) {
            /* process game world interactions */
            if (game->updateInterMsg.kind == pok_keyinput_intermsg && game->updateInterMsg.payload.key == pok_input_key_ABUTTON) {
                if (game->player->mapNo == 1) {
                    /* character James */
                    if ( next_to_character(friend1->character,&dir) ) {
                        if (friend1->character->direction != dir)
                            pok_character_context_set_update(friend1,dir,pok_character_normal_effect,0,TRUE);
                        message_menu("Hi, my name is James B. Grossweiner. I am training to be a "
                            "pokgame master! Unfortunately, poks don't exist yet so I will have to hold up until "
                            "I can catch 'em (not \"all\" of them, obviously). I am told that the \"powers that be\" "
                            "are trying to implement features as quickly as possible. However, as with all good things, "
                            "it takes time. It will be a " POK_TEXT_COLOR_BLUE "fantastic" POK_TEXT_COLOR_BLACK " day when "
                            "version 1 comes out!");
                        goto finish;
                    }

                    /* Map sign */
                    loc.column = 32;
                    loc.row = 18;
                    if ( next_to(&loc) ) {
                        message_menu(POK_TEXT_COLOR_BLUE "Testimatica" POK_TEXT_COLOR_BLACK " --- "
                            "founded by the pokgame author for testing purposes; " POK_TEXT_COLOR_RED "only authorized persons "
                            "may plant cabbage in designated areas!");
                        goto finish;
                    }

                    /* center house sign */
                    loc.column = 21;
                    loc.row = 8;
                    if ( next_to(&loc) ) {
                        message_menu("Bart's House");
                        goto finish;
                    }

                    /* talking shrubbery */
                    loc.column = 41;
                    loc.row = 15;
                    if ( next_to(&loc) ) {
                        context = 1;
                        input_menu("Hello, I am a talking shrub. I am something of a local legend... Anyway, enough about "
                            "me! What is your name?");
                        goto finish;
                    }

                }
                else if (game->player->mapNo == 2) {
                    /* character Bart */
                    if ( next_to_character(friend2->character,&dir) ) {
                        if (friend2->character->direction != dir)
                            pok_character_context_set_update(friend2,dir,pok_character_normal_effect,0,TRUE);
                        message_menu("Hi!\n\nThe name's Bart! This is my abode. I know, it's not much, but I "
                            "am a PIONEER! Soon there will be fuller worlds filled with WONDER! From dirt floors "
                            "will rise kingdoms!");
                        goto finish;
                    }

                }

                context = 0;
                noop();
            }
            else if (game->updateInterMsg.kind == pok_stringinput_intermsg) {
                if (context == 1) { /* talking shrubbery response */
                    pok_string_assign(&stringbuilder,"Nice to meet you, ");
                    pok_string_concat_obj(&stringbuilder,game->updateInterMsg.payload.string);
                    pok_string_concat_char(&stringbuilder,'!');
                    message_menu(stringbuilder.buf);
                    goto finish;
                }

            }
            else {
                context = 0;
                noop();
            }
        }
    finish:
        pok_game_modify_exit(&game->updateInterMsg);

        /* test spin animation */
        uint32_t mapNo = game->player->mapNo;
        struct pok_location* pos = &game->player->tilePos;
        static bool_t spinAttempt = FALSE;
        if (pok_location_test(pos,5,3)) {
            if (!spinAttempt && !game->playerContext->update) {
                pok_game_modify_enter(game->playerContext);
                pok_character_context_set_update(
                    game->playerContext,
                    pok_direction_down,
                    pok_character_spin_effect,
                    game->playerContext->aniTicksAmt * 4,
                    FALSE );
                pok_game_modify_exit(game->playerContext);
                spinAttempt = TRUE;
            }
        }
        else {
            spinAttempt = FALSE;
        }

        /* test simple version connection */
        if (mapNo == 1 && pok_location_test(pos,0,0)) {
            game->control = FALSE; /* let the update procedure end normally */
            pok_graphics_subsystem_game_render_state(game->sys,FALSE);
            pok_game_unregister(game);
            ret = 1;
            break;
        }

        pok_timeout_no_elapsed(&game->ioTimeout);
    }
    pok_string_delete(&stringbuilder);
    return ret;
}

void conn_version()
{
    struct pok_process* version;
    struct pok_game_info* newgame;

    newgame = pok_game_new(game->sys,game);
    version = pok_process_new("test/v1","a\0b\0\0",NULL);
    if (version == NULL) {
        pok_error_fromstack(pok_error_fatal);
    }
    newgame->versionProc = version;
    newgame->versionChannel = pok_process_stdio(version);

    /* the io_proc will play the version specified by the process */
    pok_user_load_module();
    pok_netobj_load_module();

    if (io_proc(newgame->sys,newgame) != 0) {
        pok_process_shutdown(version,5,newgame->versionChannel);
        pok_process_free(version);
        pok_error_fromstack(pok_error_fatal);
    }
    pok_process_shutdown(version,PROCESS_TIMEOUT_INFINITE,newgame->versionChannel);
    pok_game_free(newgame);

    pok_netobj_unload_module();
    pok_user_unload_module();
}
