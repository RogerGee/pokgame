/* default.c - pokgame */
#include "default.h"
#include "error.h"
#include "net.h"
#include "config.h"
#include "standard1.h" /* use standard game artwork version 1 */
#include <dstructs/treemap.h>
#include <stdlib.h>
#include <string.h>

/* structures and types */
enum portal_state
{
    portal_state_default, /* portal will always go to default version map */
    portal_state_userdef, /* portal is user-defined with url */
    portal_state_unconfig /* portal has no binding as of yet */
};

struct portal
{
    struct pok_point pos;
    char url[128];
    enum portal_state state;
    bool_t discov;
};

/* global data */
struct {
    struct treemap portals;
    struct pok_data_source* ds;
    struct pok_map* portalMap;
    bool_t portalModif;
} globals;

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
static struct pok_map* create_default_maps();
static struct pok_game_info* default_io_proc(struct pok_game_info* game);
static void load_portal_entries(struct pok_map* portalMap);
static void save_portal_entries(struct pok_map* portalMap);
static void open_portal_doorway(struct pok_map_chunk* A,struct pok_map_chunk* B,enum pok_direction AtoB);
static int test_player_near_doorway(const struct pok_character* player);
static struct pok_game_info* portal_hub(struct pok_game_info* game);

/* helpers */
static void intermsg_noop(struct pok_intermsg* intermsg);
static void intermsg_message_menu(struct pok_intermsg* intermsg,const char* prompt);
static void intermsg_input_menu(struct pok_intermsg* intermsg,const char* prompt);

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
    defmap = create_default_maps();
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
    struct pok_string* path;
    path = pok_get_install_root_path();
    pok_string_concat(path,POKGAME_DEFAULT_DIRECTORY POKGAME_STS_IMAGE);
    B( pok_tile_manager_fromfile_tiles_png(tman,path->buf) );
    pok_string_free(path);
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
    /* load sprite manager for the default game */
    struct pok_string* path;
    path = pok_get_install_root_path();
    pok_string_concat(path,POKGAME_DEFAULT_DIRECTORY POKGAME_SSS_IMAGE);
    B( pok_sprite_manager_fromfile_png(
            sman,
            path->buf,
            pok_sprite_manager_updown_alt) );
    pok_string_free(path);
}

struct pok_map* create_default_maps()
{
    /* create the first map; this will serve as a portal hub from which the
       player may enter the default version or connect to another version
       process */
    struct pok_map* map;
    N( map = pok_map_new() );
    B( pok_map_configure(map,&DEFAULT_MAP_CHUNK_SIZE,DEFAULT_MAP_CHUNK,DEFAULT_MAP_CHUNK_SIZE.columns * DEFAULT_MAP_CHUNK_SIZE.rows) );
    load_portal_entries(map);

    /* create the other maps in the default game */

    return map;
}

/* this is the IO procedure for the default game; the IO procedure calls
   different functions based on the current context of the game in its main
   loop; instead of passing data around to these functions, they store
   information in the data segment (i.e. static variables) OR they reference
   global data */

struct pok_game_info* default_io_proc(struct pok_game_info* game)
{
    struct pok_game_info* version = NULL;

    /* loop while the window is still open: this procedure will handle
       default game IO operations */
    while ( pok_graphics_subsystem_has_window(game->sys) ) {
        if (game->player->mapNo == 0) { /* inside initial portal hub map */
            if ((version = portal_hub(game)) != NULL)
                break; /* changing versions */

        }

        /* if an intermsg went unprocessed, then do a nop to cancel the
           request */
        if (game->updateInterMsg.ready && !game->updateInterMsg.processed) {
            game->updateInterMsg.processed = TRUE;
            intermsg_noop(&game->ioInterMsg);
        }

        /* time the thread out (don't compute elapsed time) */
        pok_timeout_no_elapsed(&game->ioTimeout);
    }

    /* while we have a chance, let's save the portal entries */
    save_portal_entries(pok_world_get_map(game->world,0));

    return version;
}

static void load_portal_entry_recursive(struct pok_map_chunk* chunk,struct pok_point pos)
{
    /* each entry in the portal file consists of a state value (1-byte) and
       a zero-terminated url string (which can be empty); their position
       relative to the origin chunk is defined recursively */

    int i;
    byte_t b;
    struct portal* portal;
    struct pok_map_chunk* newChunk;

    for (i = 0;i < 4;++i) {
        if (!pok_data_stream_read_byte(globals.ds,&b)) {
            if (!pok_exception_peek_ex(pok_ex_net,pok_ex_net_endofcomms))
                pok_error_fromstack(pok_error_warning);
            break;
        }

        /* if the byte was non-zero, then an entry exists in this position */
        if (b) {
            /* read entry from data source */
            portal = malloc(sizeof(struct portal));
            if (!pok_data_stream_read_byte(globals.ds,&b) ||
                !pok_data_stream_read_string(globals.ds,portal->url,sizeof(portal->url)))
            {
                free(portal);
                if (!pok_exception_peek_ex(pok_ex_net,pok_ex_net_endofcomms))
                    pok_error_fromstack(pok_error_warning);
                break;
            }
            portal->state = b;
            portal->discov = FALSE;

            /* create new map chunk */
            N( newChunk = pok_map_add_chunk(globals.portalMap,
                    &pos,
                    i,
                    DEFAULT_MAP_CHUNK,
                    DEFAULT_MAP_CHUNK_SIZE.columns * DEFAULT_MAP_CHUNK_SIZE.rows) );

            /* open doors between current and new chunk */
            open_portal_doorway(chunk,newChunk,i);

            /* add portal entry to map */
            portal->pos = pos;
            pok_direction_add_to_point(i,&portal->pos);
            treemap_insert(&globals.portals,portal);

            /* recursively load adjacencies */
            load_portal_entry_recursive(newChunk,portal->pos);
        }
    }
}

void load_portal_entries(struct pok_map* portalMap)
{
    struct portal* portal;
    struct pok_data_source* input;
    treemap_init(&globals.portals,(key_comparator)pok_point_compar,free);

    /* create portal for default version world */
    portal = malloc(sizeof(struct portal));
    portal->pos = portalMap->originPos;
    memset(portal->url,0,sizeof(portal->url));
    portal->state = portal_state_default;
    portal->discov = TRUE; /* when saving, this entry will be ignored */
    treemap_insert(&globals.portals,portal);

    /* load portal entries from file on disk */
    input = pok_data_source_new_file(POKGAME_CONTENT_DIRECTORY POKGAME_CONTENT_PORTAL_FILE,
        pok_filemode_open_existing,
        pok_iomode_read);
    if (input == NULL) {
        /* no portal file or some other kind of error (which we report) */
        if (!pok_exception_peek_ex(pok_ex_net,pok_ex_net_file_does_not_exist))
            pok_error_fromstack(pok_error_warning);
    }
    else {
        /* load globals for recursive call, then recursively load entries from
           input file */
        globals.ds = input;
        load_portal_entry_recursive(portalMap->origin,portalMap->originPos);
        pok_data_source_free(input);
    }

    globals.portalModif = FALSE;
}

static void save_portal_entry_recursive(struct pok_point pos)
{
    int i;
    struct portal* portal;
    struct pok_point test;

    for (i = 0;i < 4;++i) {
        test = pos;
        pok_direction_add_to_point(i,&test);
        portal = treemap_lookup(&globals.portals,&test);
        if (portal == NULL)
            /* zero byte means no entry in given direction */
            pok_data_stream_write_byte(globals.ds,0);
        else if (!portal->discov) { /* only process if not visited */
            /* write entry */
            pok_data_stream_write_byte(globals.ds,1);
            pok_data_stream_write_byte(globals.ds,portal->state);
            pok_data_stream_write_string(globals.ds,portal->url);
            portal->discov = TRUE;

            /* recursively write adjacencies */
            save_portal_entry_recursive(test);
        }
    }
}

void save_portal_entries(struct pok_map* portalMap)
{
    struct pok_data_source* output;

    /* only save portals file if they were modified */
    if (globals.portalModif) {
        output = pok_data_source_new_file(POKGAME_CONTENT_DIRECTORY POKGAME_CONTENT_PORTAL_FILE,
            pok_filemode_create_always,
            pok_iomode_write);
        if (output == NULL) {
            pok_error_fromstack(pok_error_warning);
        }
        else {
            /* load globals for recursive call, then recursively load entries from
               input file */
            globals.ds = output;
            globals.portalMap = portalMap;
            save_portal_entry_recursive(portalMap->originPos);
            pok_data_source_free(output);
        }
    }
}

void open_portal_doorway(struct pok_map_chunk* A,struct pok_map_chunk* B,enum pok_direction AtoB)
{
    const struct pok_location* doorA, *doorB;
    doorA = DEFAULT_MAP_DOOR_LOCATIONS + AtoB;
    doorB = DEFAULT_MAP_DOOR_LOCATIONS + pok_direction_opposite(AtoB);
    A->data[doorA->row][doorA->column].data.tileid = DEFAULT_MAP_PASSABLE_TILE;
    B->data[doorB->row][doorB->column].data.tileid = DEFAULT_MAP_PASSABLE_TILE;
}

int test_player_near_doorway(const struct pok_character* player)
{
    struct pok_location p;
    for (enum pok_direction i = 0;i < 4;++i) {
        p = DEFAULT_MAP_DOOR_LOCATIONS[i];
        pok_direction_add_to_location(pok_direction_opposite(i),&p);
        if (pok_location_compar(&player->tilePos,&p) == 0
            && player->direction == i)
        {
            return i;
        }
    }

    return -1;
}

struct pok_game_info* portal_hub(struct pok_game_info* game)
{
    /* check for interactions in the game world relating to the console; the
       console is the station in the portal hub (map=0) where the player can
       launch other versions */
    struct portal* portal;
    struct pok_point p;
    struct pok_string s;
    enum pok_direction dir;
    portal = treemap_lookup(&globals.portals,&game->player->chunkPos);
    pok_assert(portal != NULL);

    pok_game_modify_enter(&game->updateInterMsg);
    if (game->updateInterMsg.kind != pok_uninitialized_intermsg
        && game->updateInterMsg.ready)
    {
        if (game->updateInterMsg.kind == pok_keyinput_intermsg
            && game->updateInterMsg.payload.key == pok_input_key_ABUTTON)
        {
            if (pok_location_compar(&game->player->tilePos,&DEFAULT_MAP_CONSOLE_LOCATION) == 0) {
                game->updateInterMsg.processed = TRUE;
                if (portal->state == portal_state_default) {
                    intermsg_message_menu(&game->ioInterMsg,"This portal is configured for the default pokgame version. "
                        "Step onto the warp tile to enter the world.");
                }
                else if (portal->state == portal_state_unconfig) {
                    intermsg_input_menu(&game->ioInterMsg,"Configure portal:");
                }
                else if (portal->state == portal_state_userdef) {
                    pok_string_init(&s);
                    pok_string_assign(&s,"The portal is already configured at \"");
                    pok_string_concat(&s,portal->url);
                    pok_string_concat(&s,"\". Would you like to unconfigure? (y/n)");
                    intermsg_input_menu(&game->ioInterMsg,s.buf);
                    pok_string_delete(&s);
                }
            }
            else if (game->player->tilePos.column >= 1 && game->player->tilePos.row <= 6
                && game->player->tilePos.row == 4 && game->player->direction == pok_direction_up)
            {
                game->updateInterMsg.processed = TRUE;
                intermsg_message_menu(&game->ioInterMsg,"It's a complicated warping machine.\n"
                    "It looks like some physicists and computer scientists designed this...");
            }
            else {
                /* handle player interaction with doors */
                dir = test_player_near_doorway(game->player);
                if (dir != (enum pok_direction)-1) {
                    game->updateInterMsg.processed = TRUE;
                    intermsg_input_menu(&game->ioInterMsg,"Would you like to create a new portal? (y/n)");
                }
            }
        }
        else if (game->updateInterMsg.kind == pok_stringinput_intermsg) {
            
        }
    }
    pok_game_modify_exit(&game->updateInterMsg);

    return NULL;
}

/* helper function implementations */

void intermsg_noop(struct pok_intermsg* intermsg)
{
    pok_intermsg_setup(intermsg,pok_noop_intermsg,0);
    intermsg->ready = TRUE;
}

void intermsg_message_menu(struct pok_intermsg* intermsg,const char* prompt)
{
    pok_intermsg_setup(intermsg,pok_menu_intermsg,0);
    intermsg->modflags = pok_message_menu;
    pok_string_assign(intermsg->payload.string,prompt);
    intermsg->ready = TRUE;
}

void intermsg_input_menu(struct pok_intermsg* intermsg,const char* prompt)
{
    pok_intermsg_setup(intermsg,pok_menu_intermsg,0);
    intermsg->modflags = pok_input_menu;
    pok_string_assign(intermsg->payload.string,prompt);
    intermsg->ready = TRUE;
}
