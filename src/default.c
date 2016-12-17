/* default.c - pokgame */
#include "default.h"
#include "error.h"
#include "net.h"
#include "config.h"
#include "standard1.h" /* use standard game artwork version 1 */
#include <dstructs/treemap.h>
#include <stdlib.h>
#include <string.h>

/* macros ***********************************************************************/

/* these macros check an expression for failure; 'N' checks for NULL and 'B'
   checks for Boolean FALSE */
#define N(expr) \
    if ((expr) == NULL) \
        pok_error_fromstack(pok_error_fatal)
#define B(expr) \
    if (!(expr)) \
        pok_error_fromstack(pok_error_fatal)

/********************************************************************************/

/* portal structures and types **************************************************/
enum portal_state
{
    portal_state_default, /* portal will always go to default version map */
    portal_state_userdef, /* portal is user-defined with url */
    portal_state_unconfig /* portal has no binding as of yet */
};

enum portal_flags
{
    portal_flag_build_up = 0x01,
    portal_flag_build_down = 0x02,
    portal_flag_build_left = 0x04,
    portal_flag_build_right = 0x08
};

struct portal
{
    struct pok_point pos;
    char url[128];
    enum portal_state state;

    /* used by implementation (not saved) */
    struct pok_map_chunk* chunk;
    bool_t discov;
    int flags;
};
/********************************************************************************/

/* global data: we try to prevent putting things here, but truly only a single
   game instance should be using this module at a time */
struct {
    struct treemap portals;
    struct pok_data_source* ds;
    struct pok_map* portalMap;
    bool_t portalModif;
} globals;

/* functions */
static void setup_tileman(struct pok_tile_manager* tman);
static void setup_spriteman(struct pok_sprite_manager* sman);
static struct pok_map* create_default_maps();
static struct pok_game_info* default_io_proc(struct pok_game_info* game);
static void load_portal_entries(struct pok_map* portalMap);
static void save_portal_entries(struct pok_map* portalMap);
static void open_portal_doorway(struct pok_map_chunk* A,struct pok_map_chunk* B,enum pok_direction AtoB);
static int test_player_near_doorway(const struct pok_character* player,const struct pok_map_chunk* chunk);
static struct pok_game_info* portal_hub(struct pok_game_info* game);
static void build_new_portal(const struct pok_point* adjpos,enum pok_direction direction);

/* helpers */
static void* pok_malloc(size_t amt);
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
    B( pok_map_render_context_set_position(game->mapRC,defmap,&ORIGIN,&DEFAULT_MAP_START_LOCATION) );
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
            portal = pok_malloc(sizeof(struct portal));
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
            portal->flags = 0;

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
            portal->chunk = newChunk;
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
    portal = pok_malloc(sizeof(struct portal));
    portal->pos = portalMap->originPos;
    memset(portal->url,0,sizeof(portal->url));
    portal->state = portal_state_default;
    portal->discov = TRUE; /* when saving, this entry will be ignored */
    portal->flags = 0;
    portal->chunk = portalMap->origin;
    treemap_insert(&globals.portals,portal);

    /* load portal entries from file on disk */
    input = pok_data_source_new_file(POKGAME_CONTENT_DIRECTORY POKGAME_CONTENT_PORTAL_FILE,
        pok_filemode_open_existing,
        pok_iomode_read);
    if (input == NULL) {
        /* no portal file or some other kind of error (we report the error if it
           is something besides the file not existing) */
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

int test_player_near_doorway(const struct pok_character* player,const struct pok_map_chunk* portalChunk)
{
    struct pok_location p, q;
    pok_assert(portalChunk != NULL);

    for (enum pok_direction i = 0;i < 4;++i) {
        /* see if the player is standing and facing a doorway */
        p = q = DEFAULT_MAP_DOOR_LOCATIONS[i];
        pok_direction_add_to_location(pok_direction_opposite(i),&q);
        if (pok_location_compar(&player->tilePos,&q) == 0
            && player->direction == i)
        {
            /* make sure the door tile is there (meaning there is an unopened
               door in the specified direction) */
            if (portalChunk->data[p.row][p.column].data.tileid == DEFAULT_MAP_DOOR_TILE)
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
                dir = test_player_near_doorway(game->player,portal->chunk);
                if (dir != (enum pok_direction)-1) {
                    /* set flag for potentially adding a new portal room */
                    game->updateInterMsg.processed = TRUE;
                    intermsg_input_menu(&game->ioInterMsg,"Would you like to create a new portal? (y/n)");
                    portal->flags |= (1 << dir);
                }
            }
        }
        else if (game->updateInterMsg.kind == pok_stringinput_intermsg) {
            /* process responses from the user in the default map; the
               'portal->flags' member lets us determine the context */

            /* handle building new portals; no more than one of the
               'portal_build*' flags should be set */
            for (int i = 0;i <= pok_direction_right;++i) {
                enum portal_flags f = (i << 1);
                if (portal->flags & f) {
                    build_new_portal(&portal->pos,i);

                    /* realign the map render context to account for the
                       addition; we need to lock it out in case the update
                       process is using it; however we do not need to perform
                       mutual exclusion against the render process because of
                       how the render context is designed */
                    pok_game_modify_enter(game->mapRC);
                    pok_map_render_context_align(game->mapRC);
                    pok_game_modify_exit(game->mapRC);

                    /* unset flag bit */
                    portal->flags &= ~f;
                }
            }
        }
    }
    pok_game_modify_exit(&game->updateInterMsg);

    return NULL;
}

void build_new_portal(const struct pok_point* adjpos,enum pok_direction direction)
{
    /* this function creates a new portal chunk in the default (a.k.a. portal)
       map; the position specified is an existing portal adjacent to the new
       one; the new portal should be created at a position in the specified
       'direction' from the adjacent position */

    struct portal* portal;
    struct pok_map_chunk* chunk;

    /* create a new map chunk object for the portal */
    N( chunk = pok_map_add_chunk(
        globals.portalMap,
        adjpos,
        direction,
        DEFAULT_MAP_CHUNK,DEFAULT_MAP_CHUNK_SIZE.columns*DEFAULT_MAP_CHUNK_SIZE.rows) );

    /* create the portal entry object */
    portal = pok_malloc(sizeof(struct portal));
    portal->pos = *adjpos;
    portal->url[0] = 0;
    portal->state = portal_state_unconfig;
    portal->discov = FALSE;
    portal->chunk = chunk;

    /* insert the portal into the collection */
    treemap_insert(&globals.portals,portal);
}

/* helper function implementations */

void* pok_malloc(size_t amt)
{
    /* a 'malloc' wrapper that terminates the program */
    void* p = malloc(amt);
    if (p == NULL) {
        pok_exception_flag_memory_error();
        pok_error_fromstack(pok_error_fatal);
    }
    return p;
}

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
