/* update-proc.c - pokgame */
#include "pokgame.h"
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
#define INITIAL_FADEIN_TIME     2000

/* globals */
static struct
{
    uint32_t mapTicksNormal;
    uint32_t mapTicksFast;
} globals;

/* functions */
static void set_defaults(struct pok_game_info* info);
static void set_tick_amounts(struct pok_game_info* info,struct pok_timeout_interval* t);
static void update_key_input(struct pok_game_info* info);

/* this procedure drives all the game logic; the return value has special meaning:
    0 - exit via in game event (e.g. the player selected a menu item)
    1 - exit because window was closed unexpectedly
*/
int update_proc(struct pok_game_info* info)
{
    int r = 0;
    uint32_t tileAniTicks = 0;

    /* setup parameters */
    set_defaults(info);
    set_tick_amounts(info,&info->updateTimeout);

    /* setup initial screen fade in (fade out with reverse set to true) */
    info->fadeout.delay = INITIAL_FADEIN_DELAY;
    pok_fadeout_effect_set_update(&info->fadeout,info->sys,INITIAL_FADEIN_TIME,pok_fadeout_black_screen,TRUE);

    /* game logic loop */
    do {
        bool_t skip;

        /* key input logic */
        update_key_input(info);

        /* perform input-sensitive update operations; if an update operation just completed, then skip the timeout */
        skip = pok_map_render_context_update(info->mapRC,info->sys->dimension,info->updateTimeout.elapsed)
            + pok_character_context_update(info->playerContext,info->sys->dimension,info->updateTimeout.elapsed);

        /* perform other update operations */
        pok_fadeout_effect_update(&info->fadeout,info->updateTimeout.elapsed);

        if (!skip) {
            /* update global counter and map context's tile animation counter */
            if (tileAniTicks >= 250) { /* tile animation ticks every 1/4 second */
                ++info->mapRC->tileAniTicks;
                tileAniTicks = 0;
            }

            /* perform timeout and update tile animation ticks */
            pok_timeout(&info->updateTimeout);
            tileAniTicks += info->updateTimeout.elapsed;
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

            /* don't do keyboard updates if the screen is faded out in this context */
            if (info->fadeout._base.update || info->fadeout.keep)
                return;

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
                    if (!info->playerContext->update) {
                        /* lock the graphics subsystem: this prevents rendering momentarily so that
                           we can update the player and the map at the same time without a race */
                        pok_graphics_subsystem_lock(info->sys);

                        /* update player context for animation (direction does not change) */
                        pok_game_modify_enter(info->playerContext);
                        pok_character_context_set_update(info->playerContext,direction,pok_character_normal_effect,info->sys->dimension);
                        pok_game_modify_exit(info->playerContext);

                        if (!info->mapRC->update) {
                            /* update map context */
                            pok_game_modify_enter(info->mapRC);
                            /* attempt to update the map focus; check for all possible collisions; the
                               map render context will handle impassable tile collisions */
                            if (/* TODO: check collisions... */ pok_map_render_context_move(info->mapRC,direction,TRUE)) {
                                /* prepare the map context to be updated */
                                pok_map_render_context_set_update(info->mapRC,direction,info->sys->dimension);
                                /* update the player character's location */
                                info->player->chunkPos = info->mapRC->chunkpos;
                                info->player->tilePos = info->mapRC->relpos;
                                info->playerContext->slowDown = FALSE;
                            }
                            else
                                /* the player ran into something; animate slower for effect */
                                info->playerContext->slowDown = TRUE;
                            pok_game_modify_exit(info->mapRC);
                        }

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
