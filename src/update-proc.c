/* update-proc.c - pokgame */
#include "pokgame.h"
#include "error.h"

/* the update procedure runs the game engine logic; it changes a global configuration that
   is handled by the other two game procedures (IO and rendering); this procedure must obtain
   locks when attempting to read/modify game information (from the 'pok_game_info' structure)
   unless an operation is atomic in nature; the update procedure should exit before the rendering
   procedure */

/* constant update parameters */
#define DEFAULT_GRANULARITY      8
#define MAP_SCROLL_TIME          240 /* amount of time for complete map scroll update */

/* globals */
static struct
{
    uint32_t mapTicksNormal;
    uint32_t mapTicksFast;
} globals;

/* functions */
static void set_defaults(struct pok_game_info* info);
static void set_tick_amounts(struct pok_game_info* info,struct timeout_interval* t);
static void update_key_input(struct pok_game_info* info);

/* this procedure drives all the game logic; the return value has special meaning:
    0 - exit via in game event (e.g. the player selected a menu item)
    1 - exit because window was closed unexpectedly
*/
int update_proc(struct pok_game_info* info)
{
    int r = 0;
    uint32_t ticks = 0;
    struct timeout_interval t;

    /* setup parameters */
    timeout_interval_reset(&t,info->updateTimeout);
    set_defaults(info);
    set_tick_amounts(info,&t);

    /* game logic loop */
    do {
        bool_t skip;

        /* key input logic */
        update_key_input(info);

        /* perform update operations; we trust them to lock when necessary; if
           an update operation just completed, then skip the timeout */
        skip = pok_map_render_context_update(info->mapRC,info->sys->dimension)
            || pok_character_context_update(info->playerContext,info->sys->dimension);

        if (!skip) {
            /* update global counter and map context's tile animation counter */
            ++ticks;
            if (ticks % t.ticksFourthSecond == 0) /* tile animation ticks every 1/4 second */
                ++info->mapRC->tileAniTicks;

            /* check timeout change; then perform timeout */
            if (t.mseconds != info->updateTimeout) {
                timeout_interval_reset(&t,info->updateTimeout);
                set_tick_amounts(info,&t);
            }

            timeout(&t);
        }

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
    info->mapRC->granularity = DEFAULT_GRANULARITY;
    info->playerContext->granularity = DEFAULT_GRANULARITY;

}

void set_tick_amounts(struct pok_game_info* info,struct timeout_interval* t)
{
    /* compute tick amounts; note: map scroll and character animation need to be synced */
    globals.mapTicksNormal = MAP_SCROLL_TIME / t->mseconds / info->mapRC->granularity;
    globals.mapTicksFast = globals.mapTicksNormal / 1.75;
    info->mapRC->scrollTicksAmt = globals.mapTicksNormal;
    info->playerContext->aniTicksAmt = globals.mapTicksNormal;

    /* these need to be valid denominators */
    if (info->mapRC->scrollTicksAmt == 0)
        info->mapRC->scrollTicksAmt = 1;
    if (info->playerContext->aniTicksAmt == 0)
        info->playerContext->aniTicksAmt = 1;
}

void update_key_input(struct pok_game_info* info)
{
    static bool_t running = FALSE;
    enum pok_direction direction = pok_direction_none;

    pok_game_lock(info->sys);
    if ( pok_graphics_subsystem_is_running(info->sys) ) {
        pok_game_unlock(info->sys);

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
                    if (!info->playerContext->update) {
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
    else
        pok_game_unlock(info->sys);
}
