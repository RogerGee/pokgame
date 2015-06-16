/* update-proc.c - pokgame */
#include "pokgame.h"
#include "protocol.h"
#include "error.h"

/* the update procedure runs the game engine logic; it changes a global configuration that
   is handled by the other two game procedures (IO and graphics); this procedure must obtain
   locks when attempting to read/modify game information (from the 'pok_game_info' structure)
   that would cause the other procedures to demonstrate undefined behavior; the update procedure
   should exit before the rendering procedure */

/* constant update parameters */
#ifdef POKGAME_WIN32
/* Windows doesn't have a very accurate timeout resolution like Linux, therefore different
   values are required to demonstrate the correct behavior */

#define MAP_GRANULARITY          8   /* granularity of map scroll update and player move update */
#define MAP_SCROLL_TIME          300 /* number of ticks for complete map scroll update */
#define MAP_SCROLL_TIME_FAST     180 /* number of ticks for complete fast map scroll update */

#else

#define MAP_GRANULARITY          8   /* granularity of map scroll update and player move update */
#define MAP_SCROLL_TIME          240 /* number of ticks for complete map scroll update */
#define MAP_SCROLL_TIME_FAST     160 /* number of ticks for complete fast map scroll update */

#endif

#define INITIAL_FADEIN_DELAY     250
#define INITIAL_FADEIN_TIME     1750
#define WARP_FADEOUT_TIME        200
#define WARP_FADEIN_TIME         200
#define WARP_FADEIN_DELAY        400

/* globals */
static struct
{
    uint32_t mapTicksNormal; /* ticks for normal map scroll */
    uint32_t mapTicksFast;   /* ticks for fast map scroll */
    bool_t pausePlayerMap;   /* if non-zero, then stop updating player and map */
} globals;

/* functions */
static void set_defaults(struct pok_game_info* info);
static void set_tick_amounts(struct pok_game_info* info,struct pok_timeout_interval* t);
static void update_key_input(struct pok_game_info* info);
static bool_t character_update(struct pok_game_info* info);
static void fadeout_update(struct pok_game_info* info);
static bool_t check_collisions(struct pok_game_info* info);
static bool_t latent_warp_logic(struct pok_game_info* info,enum pok_direction direction);
static void warp_logic(struct pok_game_info* info);

/* this procedure drives all the game logic; the return value has special meaning:
    0 - exit via in game event (e.g. the player selected a menu item)
    1 - exit because window was closed unexpectedly
*/
int update_proc(struct pok_game_info* info)
{
    int r = 0;
    uint32_t tileAniTicks = 0;
    uint64_t gameTime = 0;

    /* setup parameters */
    set_defaults(info);
    set_tick_amounts(info,&info->updateTimeout);
    globals.pausePlayerMap = FALSE;

    /* setup initial screen fade in (fade out with reverse set to true) */
    info->fadeout.delay = INITIAL_FADEIN_DELAY;
    pok_fadeout_effect_set_update(&info->fadeout,info->sys,INITIAL_FADEIN_TIME,pok_fadeout_black_screen,TRUE);

    /* game logic loop */
    do {
        bool_t skip;

        /* key input logic */
        update_key_input(info);

        if (!globals.pausePlayerMap) {
            /* perform input-sensitive update operations; if an update operation just completed, then skip the timeout */
            skip = pok_map_render_context_update(info->mapRC,info->sys->dimension,info->updateTimeout.elapsed)
                +  character_update(info);
        }

        /* perform fadeout update */
        fadeout_update(info);

        if (!skip) {
            /* update global counter and map context's tile animation counter */
            if (tileAniTicks >= 250) { /* tile animation ticks every 1/4 second */
                ++info->mapRC->tileAniTicks;
                tileAniTicks = 0;
            }

            /* perform timeout and update tile animation ticks */
            pok_timeout(&info->updateTimeout);
            tileAniTicks += info->updateTimeout.elapsed;
            gameTime += info->updateTimeout.elapsed;
        }
        else
            info->updateTimeout.elapsed = 0;

        if ( !pok_graphics_subsystem_has_window(info->sys) ) {
            r = 1;
            break;
        }
    } while (info->control);

    return r;
}

void set_defaults(struct pok_game_info* info)
{
    /* TODO: load defaults from init file */
    info->mapRC->granularity = MAP_GRANULARITY;
    info->playerContext->granularity = MAP_GRANULARITY;

}

void set_tick_amounts(struct pok_game_info* info,struct pok_timeout_interval* t)
{
    /* compute tick amounts; note: map scroll and character animation need to be synced */
    globals.mapTicksNormal = MAP_SCROLL_TIME / info->mapRC->granularity;
    globals.mapTicksFast = MAP_SCROLL_TIME_FAST / info->mapRC->granularity;

    /* these need to be valid denominators */
    if (globals.mapTicksNormal == 0)
        globals.mapTicksNormal = 1;
    if (globals.mapTicksFast == 0)
        globals.mapTicksFast = 1;

    /* set default tick amounts for game contexts */
    info->mapRC->scrollTicksAmt = globals.mapTicksNormal;
    info->playerContext->aniTicksAmt = globals.mapTicksNormal;
}

void update_key_input(struct pok_game_info* info)
{
    static bool_t running = FALSE;
    enum pok_direction direction = pok_direction_none;

    /* make sure the subsystem is running (window is up and game not paused) */
    if ( pok_graphics_subsystem_is_running(info->sys) ) {

        /* perform a no-op query to refresh the keyboard state information */
        pok_graphics_subsystem_keyboard_query(info->sys,-1,TRUE);

        if (info->gameContext == pok_game_world_context) {
            /* handle key logic for the world context; this context involves the
               player moving around the screen and potentially interacting with
               objects in the game world */

            /* update player and map based on keyboard input by checking the directional
               keys; make sure the map and player are not already updating already */
            if ( pok_graphics_subsystem_keyboard_query(info->sys,pok_input_key_UP,FALSE) )
                direction = pok_direction_up;
            else if ( pok_graphics_subsystem_keyboard_query(info->sys,pok_input_key_DOWN,FALSE) )
                direction = pok_direction_down;
            else if ( pok_graphics_subsystem_keyboard_query(info->sys,pok_input_key_LEFT,FALSE) )
                direction = pok_direction_left;
            else if ( pok_graphics_subsystem_keyboard_query(info->sys,pok_input_key_RIGHT,FALSE) )
                direction = pok_direction_right;

            /* check B key for fast scrolling (player running) */
            if ( pok_graphics_subsystem_keyboard_query(info->sys,pok_input_key_BBUTTON,FALSE) ) {
                if (!running) {
                    running = TRUE;
                    info->mapRC->scrollTicksAmt = globals.mapTicksFast;
                    info->playerContext->aniTicksAmt = globals.mapTicksFast;
                }
            }
            else if (running) {
                running = FALSE;
                info->mapRC->scrollTicksAmt = globals.mapTicksNormal;
                info->playerContext->aniTicksAmt = globals.mapTicksNormal;
            }

            if (direction != pok_direction_none) {
                if (direction == info->player->direction || info->mapRC->groove) { /* player facing update direction */
                    if (!info->playerContext->update && !info->mapRC->update) {
                        /* lock the graphics subsystem: this prevents rendering momentarily so that
                           we can update the player and the map at the same time without a race */
                        pok_graphics_subsystem_lock(info->sys);

                        /* update map context */
                        pok_game_modify_enter(info->mapRC);
                        /* test to see if the current tile has a latent warp; if so, this would change the
                           map and animate it to 1 tile offset the warp location; this will setup a transition */
                        if ( !latent_warp_logic(info,direction) ) {
                            /* attempt to update the map focus; the map render context will
                               handle impassable tile collisions */
                            if ( pok_map_render_context_move(info->mapRC,direction,TRUE) ) {
                                /* check for other collision possibilities while the context is updated */
                                if ( !check_collisions(info) ) {
                                    /* undo the previous changes; this will work as if they never took place */
                                    pok_map_render_context_move(info->mapRC,pok_direction_opposite(direction),FALSE);
                                    /* the player ran into something; animate slower for effect */
                                    info->playerContext->slowDown = TRUE;
                                }
                                else { /* we can safely pass into the new location */
                                    /* check for non-latent warps; these calls will setup a transition */
                                    warp_logic(info);
                                    /* prepare the map context to be updated */
                                    pok_map_render_context_set_update(info->mapRC,direction,info->sys->dimension);
                                    /* update the player character's location */
                                    info->player->chunkPos = info->mapRC->chunkpos;
                                    info->player->tilePos = info->mapRC->relpos;
                                    info->playerContext->slowDown = FALSE;
                                }
                            }
                            else
                                /* the player ran into something; animate slower for effect */
                                info->playerContext->slowDown = TRUE;

                            /* update player context for animation (direction does not change); if
                               slowDown is true, then the player does not move (so don't set parameter) */
                            pok_game_modify_enter(info->playerContext);
                            pok_character_context_set_update(
                                info->playerContext,
                                direction,
                                pok_character_normal_effect,
                                info->playerContext->slowDown ? 0 : info->sys->dimension );
                            pok_game_modify_exit(info->playerContext);
                        }
                        pok_game_modify_exit(info->mapRC);

                        pok_graphics_subsystem_unlock(info->sys);
                    }
                }
                else if (!info->playerContext->update) {
                    /* update player context for animation; the player sprite is just moving in place; this
                       still produces an animation, but the sprite doesn't offset; this is specified by
                       passing 0 as the dimension parameter to 'pok_character_set_update' */
                    pok_game_modify_enter(info->playerContext);
                    info->playerContext->slowDown = FALSE;
                    pok_character_context_set_update(info->playerContext,direction,pok_character_normal_effect,0);
                    pok_game_modify_exit(info->playerContext);
                }
            }
        }
    }
}

bool_t character_update(struct pok_game_info* info)
{
    if ( pok_character_context_update(info->playerContext,info->sys->dimension,info->updateTimeout.elapsed) ) {

        return TRUE;
    }
    return FALSE;
}

void fadeout_update(struct pok_game_info* info)
{
    bool_t result;
    struct pok_map* map;
    if ( pok_fadeout_effect_update(&info->fadeout,info->updateTimeout.elapsed) ) {
        if (info->gameContext == pok_game_intro_context)
            /* the intro context is just a fadeout that leads to the world context */
            info->gameContext = pok_game_world_context;
        else if (info->gameContext == pok_game_warp_fadeout_context || info->gameContext == pok_game_warp_fadeout_cave_context) {
            /* we just finished a fadeout for a warp; now finish the transition */
            if (info->mapTrans != NULL) {
                pok_game_lock(info->world);
                map = pok_world_get_map(info->world,info->mapTrans->warpMap);
                pok_game_unlock(info->world);
                /* if we can't find the map (or subsequently the chunk/position) then silently fail; the
                   player will find that they arrived where they left and be disappointed */
                if (map != NULL) {
                    pok_game_modify_enter(info->mapRC);
                    result = pok_map_render_context_set_position(info->mapRC,map,&info->mapTrans->warpChunk,&info->mapTrans->warpLocation);
                    pok_game_modify_exit(info->mapRC);
                    if (result) {
                        /* set the player character as well */
                        pok_game_modify_enter(info->playerContext);
                        pok_character_context_set_player(info->playerContext,info->mapRC);
                        pok_game_modify_exit(info->playerContext);
                    }
                }
                info->mapTrans = NULL;
            }
            /* set fadein effect */
            info->fadeout.delay = WARP_FADEIN_DELAY;
            pok_fadeout_effect_set_update(
                &info->fadeout,
                info->sys,
                WARP_FADEIN_TIME,
                info->gameContext == pok_game_warp_fadeout_context ? pok_fadeout_black_screen : pok_fadeout_to_center,
                TRUE );
            info->gameContext = pok_game_warp_fadein_context;
        }
        else if (info->gameContext == pok_game_warp_latent_fadeout_context || info->gameContext == pok_game_warp_latent_fadeout_cave_context) {
            /* we just finished a fadeout for a latent warp; now finish the transition */
            if (info->mapTrans != NULL) {
                /* update the map and player contexts; if the warp information is incorrect then silently fail */
                pok_game_lock(info->world);
                map = pok_world_get_map(info->world,info->mapTrans->warpMap);
                pok_game_unlock(info->world);
                if (map != NULL) {
                    pok_game_modify_enter(info->mapRC);
                    result = pok_map_render_context_set_position(info->mapRC,map,&info->mapTrans->warpChunk,&info->mapTrans->warpLocation);
                    pok_game_modify_exit(info->mapRC);
                    if (result) {
                        /* save exit direction and update player */
                        enum pok_direction direction = (info->mapTrans->warpKind - pok_tile_warp_latent_up) % 4;
                        pok_graphics_subsystem_lock(info->sys);
                        pok_game_modify_enter(info->mapRC);
                        pok_map_render_context_set_update(
                            info->mapRC,
                            direction,
                            info->sys->dimension );
                        pok_game_modify_exit(info->mapRC);
                        pok_game_modify_enter(info->playerContext);
                        pok_character_context_set_player(info->playerContext,info->mapRC);
                        info->playerContext->slowDown = FALSE;
                        pok_character_context_set_update(
                            info->playerContext,
                            direction,
                            pok_character_normal_effect,
                            info->sys->dimension);
                        pok_game_modify_exit(info->playerContext);
                        pok_graphics_subsystem_unlock(info->sys);
                        /* pause the map scrolling until the fadein effect completes */
                        globals.pausePlayerMap = TRUE;
                    }
                }
                info->mapTrans = NULL;
            }
            /* set fadein effect */
            info->fadeout.delay = WARP_FADEIN_DELAY;
            pok_fadeout_effect_set_update(
                &info->fadeout,
                info->sys,
                WARP_FADEIN_TIME,
                info->gameContext == pok_game_warp_latent_fadeout_cave_context ? pok_fadeout_to_center : pok_fadeout_black_screen,
                TRUE );
            info->gameContext = pok_game_warp_fadein_context;
        }
        else if (info->gameContext == pok_game_warp_fadein_context) {
            /* fade in to world from warp */
            info->gameContext = pok_game_world_context;
            globals.pausePlayerMap = FALSE;
        }
     }
 }

bool_t check_collisions(struct pok_game_info* info)
{
    size_t iter;
    bool_t status = TRUE;
    /* check against all player contexts in the character render context; this
       procedure assumes that the map render context is currently locked */
    pok_game_lock(info->charRC);
    for (iter = 0;iter < info->charRC->chars.da_top;++iter) {
        struct pok_character_context* ctx;
        ctx = info->charRC->chars.da_data[iter];
        if (ctx->character->mapNo == info->mapRC->map->mapNo
            && ctx->character->chunkPos.X == info->mapRC->chunkpos.X
            && ctx->character->chunkPos.Y == info->mapRC->chunkpos.Y
            && ctx->character->tilePos.column == info->mapRC->relpos.column
            && ctx->character->tilePos.row == info->mapRC->relpos.row)
        {
            status = FALSE;
            break;
        }
    }
    pok_game_unlock(info->charRC);

    return status;
}

bool_t latent_warp_logic(struct pok_game_info* info,enum pok_direction direction)
{
    /* check to see if the current location has a latent warp; a latent warp is delayed; the
       player must first walk onto the tile, then walk in the specified direction */
    struct pok_tile_data* tdata;
    tdata = &info->mapRC->chunk->data[info->mapRC->relpos.row][info->mapRC->relpos.column].data;
    if ((((tdata->warpKind == pok_tile_warp_latent_up || tdata->warpKind == pok_tile_warp_latent_cave_up)
            && direction == pok_direction_up)
        || ((tdata->warpKind == pok_tile_warp_latent_down || tdata->warpKind == pok_tile_warp_latent_cave_down)
            && direction == pok_direction_down)
        || ((tdata->warpKind == pok_tile_warp_latent_left || tdata->warpKind == pok_tile_warp_latent_cave_left)
            && direction == pok_direction_left)
        || ((tdata->warpKind == pok_tile_warp_latent_right || tdata->warpKind == pok_tile_warp_latent_cave_right)
            && direction == pok_direction_right)) && direction == info->player->direction) {
        struct pok_map* map;
        /* set warp effect (latent warps always fadeout) */
        pok_fadeout_effect_set_update(
            &info->fadeout,
            info->sys,
            WARP_FADEOUT_TIME,
            tdata->warpKind >= pok_tile_warp_latent_cave_up ? pok_fadeout_to_center : pok_fadeout_black_screen,
            FALSE );
        /* set game context; test to see if this is a cave warp or not */
        info->gameContext = tdata->warpKind >= pok_tile_warp_latent_cave_up
            ? pok_game_warp_latent_fadeout_cave_context : pok_game_warp_latent_fadeout_context;
        /* cache a reference to the tile data; it will be used to change the map render context
           after the transition has finished */
        info->mapTrans = tdata;
        /* return true to mean that we applied a warp */
        return TRUE;
    }
    /* we did not find a warp */
    return FALSE;
}

void warp_logic(struct pok_game_info* info)
{
    /* check to see if the current location has a non-latent warp */
    struct pok_tile_data* tdata;
    tdata = &info->mapRC->chunk->data[info->mapRC->relpos.row][info->mapRC->relpos.column].data;
    if (tdata->warpKind != pok_tile_warp_none && (tdata->warpKind < pok_tile_warp_latent_up
            || tdata->warpKind > pok_tile_warp_latent_cave_right)) {
        /* set warp effect and change game context depending on warp kind */
        if (tdata->warpKind == pok_tile_warp_instant || tdata->warpKind == pok_tile_warp_cave_exit) {
            pok_fadeout_effect_set_update(&info->fadeout,info->sys,WARP_FADEOUT_TIME,pok_fadeout_black_screen,FALSE);
            info->gameContext = pok_game_warp_fadeout_context;
        }
        else if (tdata->warpKind == pok_tile_warp_cave_enter) {
            pok_fadeout_effect_set_update(&info->fadeout,info->sys,WARP_FADEOUT_TIME,pok_fadeout_to_center,FALSE);
            info->gameContext = pok_game_warp_fadeout_cave_context;
        }
        /* cache a reference to the tile data; it will be used to change the map render context
           after the transition has finished */
        info->mapTrans = tdata;
    }
}
