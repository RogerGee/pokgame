/* update-proc.c - pokgame */
#include "pokgame.h"
#include "protocol.h"
#include "error.h"

/* the update procedure runs the game engine logic; it changes a global configuration that
   is handled by the other two game procedures (IO and graphics); this procedure must obtain
   locks when attempting to read/modify game information (from the 'pok_game_info' structure)
   that would cause the other procedures to demonstrate undefined behavior */

/* constant update parameters */

#define MAP_GRANULARITY          8   /* granularity of map scroll update and player move update */
#define MAP_SCROLL_TIME          240 /* number of ticks for complete map scroll update */
#define MAP_SCROLL_TIME_FAST     160 /* number of ticks for complete fast map scroll update */

#define INITIAL_FADEIN_DELAY     350 /* "initial fadein" happens before we show the game */
#define INITIAL_FADEIN_TIME     2000
#define WARP_FADEOUT_TIME        400 /* warp transitions */
#define WARP_FADEIN_TIME         400
#define WARP_FADEIN_DELAY        350

#define INTERMSG_DELAY          2400 /* number of ticks for intermsg response before canceling */

/* globals */
static struct
{
    uint32_t mapTicksNormal; /* ticks for normal map scroll */
    uint32_t mapTicksFast;   /* ticks for fast map scroll */
} globals;

/* functions */
static void set_defaults(struct pok_game_info* info);
static void set_tick_amounts(struct pok_game_info* info,struct pok_timeout_interval* t);
static void update_key_input(struct pok_game_info* info);
static void menu_keyup_hook(enum pok_input_key key,struct pok_game_info* info);
static void menu_textentry_hook(char c,struct pok_game_info* info);
static void character_update(struct pok_game_info* info);
static void menu_update(struct pok_game_info* info);
static bool_t check_collisions(struct pok_game_info* info);
static void player_move_logic(struct pok_game_info* info,enum pok_direction direction);
static void fadeout_logic(struct pok_game_info* info);
static void map_terrain_logic(struct pok_game_info* info);
static bool_t latent_warp_logic(struct pok_game_info* info,enum pok_direction direction);
static bool_t warp_logic(struct pok_game_info* info);
static void intermsg_logic(struct pok_game_info* info);
static enum pok_character_effect get_effect_from_terrain(struct pok_map_render_context* mapRC,
    struct pok_tile_manager* tman,
    enum pok_direction direction);

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
    info->pausePlayerMap = FALSE;

    /* setup graphics subsystem hooks */
    pok_graphics_subsystem_append_hook(info->sys->keyupHook,(keyup_routine_t)menu_keyup_hook,info);
    pok_graphics_subsystem_append_hook(info->sys->textentryHook,(textentry_routine_t)menu_textentry_hook,info);

    /* setup initial screen fade in (fade out with reverse set to true) */
    info->fadeout.delay = INITIAL_FADEIN_DELAY;
    pok_fadeout_effect_set_update(&info->fadeout,info->sys,INITIAL_FADEIN_TIME,pok_fadeout_black_screen,TRUE);

    /* game logic loop */
    do {
        bool_t skip = 0;

        /* key input logic */
        update_key_input(info);

        if (!info->pausePlayerMap) {
            /* perform input-sensitive update operations; if an update operation just completed, then skip the timeout;
               these must be performed at the same time before a frame is updated */
            pok_graphics_subsystem_lock(info->sys);
            skip = pok_map_render_context_update(info->mapRC,info->sys->dimension,info->updateTimeout.elapsed)
                +  pok_character_context_update(info->playerContext,info->sys->dimension,info->updateTimeout.elapsed);
            pok_graphics_subsystem_unlock(info->sys);
        }

        /* perform other updates */
        character_update(info);
        menu_update(info);

        /* perform game logic operations */
        fadeout_logic(info);
        map_terrain_logic(info);
        intermsg_logic(info);

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

    /* remove hooks from graphics subsystem */
    pok_graphics_subsystem_pop_hook(info->sys->textentryHook);
    pok_graphics_subsystem_pop_hook(info->sys->keyupHook);

    return r;
}

void set_defaults(struct pok_game_info* info)
{
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

            /* handle interaction key (A button) input; this allows the player to interact with
               the game world by pressing A next to something */
            if (pok_graphics_subsystem_keyboard_query(info->sys,pok_input_key_ABUTTON,FALSE)) {
                if (!info->playerContext->update) {
                    /* create a key input intermsg for the A button event and change the game context */
                    pok_intermsg_setup(&info->updateInterMsg,pok_keyinput_intermsg,INTERMSG_DELAY);
                    info->updateInterMsg.payload.key = pok_input_key_ABUTTON;
                    info->updateInterMsg.ready = TRUE;
                    info->gameContext = pok_game_intermsg_context;
                }
            }
            else {
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

                player_move_logic(info,direction);
            }
        }
    }
}

void menu_keyup_hook(enum pok_input_key key,struct pok_game_info* info)
{
    /* this function is executed on the rendering thread; it checks to see
       if a menu is active and focused and, if so, delivers the control key */
    if (info->gameContext == pok_game_menu_context) {
        if (info->messageMenu.base.active && info->messageMenu.base.focused)
            /* ignore the return value; we can check later to see if the menu has
               completed */
            pok_message_menu_ctrl_key(&info->messageMenu,key);

    }
}

void menu_textentry_hook(char c,struct pok_game_info* info)
{
    /* this function is executed on the rendering thread; it checks to see
       if a menu is active and focused and, if so, delivers the text entry
       character message to the menu; this only is done for the text input menu */
    if (info->gameContext == pok_game_menu_context /* && */) {

    }
}

void character_update(struct pok_game_info* info)
{
    size_t iter;
    /* process non-player character updates */
    pok_game_lock(info->charRC);
    for (iter = 0;iter < info->charRC->chars.da_top;++iter) {
        struct pok_character_context* context = (struct pok_character_context*) info->charRC->chars.da_data[iter];
        if (context != info->playerContext) {
            pok_character_context_update(
                context,
                info->sys->dimension,
                info->updateTimeout.elapsed );
        }
    }
    pok_game_unlock(info->charRC);
}

void menu_update(struct pok_game_info* info)
{
    /* if focused, update menu text contexts; if the menu finishes updating, then
       exit menu context and create an intermsg to handle menu completion; make sure
       to remove the menu's focus so that we keep one or less menus focused at a time */
    if (info->gameContext == pok_game_menu_context) {
        if (info->messageMenu.base.active && info->messageMenu.base.focused) {
            if (!pok_text_context_update(&info->messageMenu.text,info->updateTimeout.elapsed) && info->messageMenu.text.finished) {
                info->messageMenu.base.focused = FALSE;
                pok_intermsg_setup(&info->updateInterMsg,pok_completed_intermsg,INTERMSG_DELAY);
                info->updateInterMsg.ready = TRUE;
                info->gameContext = pok_game_intermsg_context;
            }
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

void player_move_logic(struct pok_game_info* info,enum pok_direction direction)
{
    /* process the player move logic; the player always moves in the 'pok_game_world_context' */
    if (direction != pok_direction_none) {
        if (direction == info->player->direction || info->mapRC->groove) { /* player facing update direction */
            if (!info->playerContext->update && !info->mapRC->update) {
                bool_t skip;
                bool_t groove = info->mapRC->groove;
                enum pok_character_effect effect = pok_character_normal_effect;

                /* lock the graphics subsystem: this prevents rendering momentarily so that
                   we can update the player and the map at the same time without a race */
                pok_graphics_subsystem_lock(info->sys);

                /* determine player update effect based on map terrain; this check occurs before the map is updated */
                if (info->playerEffect == pok_character_normal_effect)
                    effect = get_effect_from_terrain(info->mapRC,info->tman,direction);
                skip = effect == pok_character_jump_effect;

                /* update map context */
                pok_game_modify_enter(info->mapRC);
                /* test to see if the current tile has a latent warp; if so, this would change the
                   map and animate it to 1 tile offset the warp location; this will setup a transition */
                if ( !latent_warp_logic(info,direction) ) {
                    /* attempt to update the map focus; the map render context will
                       handle impassable tile collisions */
                    if ( pok_map_render_context_move(info->mapRC,direction,skip,TRUE) ) {
                        /* check for other collision possibilities while the context is updated */
                        if ( !check_collisions(info) ) {
                            /* undo the previous changes; this will work as if they never took place */
                            pok_map_render_context_move(info->mapRC,pok_direction_opposite(direction),skip,FALSE);
                            /* the player ran into something; animate slower for effect; reenter game world context
                               upon a collision as well */
                            info->playerContext->slowDown = TRUE;
                            info->gameContext = pok_game_world_context;
                        }
                        else { /* we can safely pass into the new location */
                            bool_t didWarp;
                            /* check for non-latent warps; these calls will setup a transition */
                            didWarp = warp_logic(info);
                            /* prepare the map context to be updated; if we skipped a column/row then double the
                               length of the map scroll animation */
                            pok_map_render_context_set_update(info->mapRC,direction,info->sys->dimension*(skip+1));
                            /* update the player character's location */
                            info->player->chunkPos = info->mapRC->chunkpos;
                            info->player->tilePos = info->mapRC->relpos;
                            info->playerContext->slowDown = FALSE;

                            /* determine if (after the map update) the tile terrain prompts a change in game context; we
                               only do this if the player is currently updating normally, hasn't run into anything and is
                               not currently warping */
                            if (info->playerEffect == pok_character_normal_effect && !didWarp) {
                                enum pok_character_effect eff;
                                eff = get_effect_from_terrain(info->mapRC,info->tman,direction);
                                if (eff == pok_character_slide_effect)
                                    info->gameContext = pok_game_sliding_context;
                                else
                                    info->gameContext = pok_game_world_context;
                            }
                        }
                    }
                    else {
                        /* the player ran into something; animate slower for effect; reset game context
                           to 'world' upon collision */
                        info->playerContext->slowDown = TRUE;
                        info->gameContext = pok_game_world_context;
                    }

                    /* update player context for animation (direction does not change); if
                       slowDown is true, then the player does not move (so don't set parameter) */
                    pok_game_modify_enter(info->playerContext);
                    pok_character_context_set_update(
                        info->playerContext,
                        direction,
                        effect,
                        info->playerContext->slowDown ? 0 : info->sys->dimension,
                        !groove );
                    pok_game_modify_exit(info->playerContext);
                }
                pok_game_modify_exit(info->mapRC);

                pok_graphics_subsystem_unlock(info->sys);
            }
        }
        else if (!info->playerContext->update) {
            /* update player context for animation; the player sprite is just moving in place; this
               still produces an animation, but the sprite doesn't offset; this is specified by
               passing 0 as the dimension parameter to 'pok_character_context_set_update' */
            pok_game_modify_enter(info->playerContext);
            info->playerContext->slowDown = FALSE;
            pok_character_context_set_update(info->playerContext,direction,info->playerEffect,0,TRUE);
            pok_game_modify_exit(info->playerContext);
        }
    }
}

void fadeout_logic(struct pok_game_info* info)
{
    bool_t result;
    struct pok_map* map;
    if ( pok_fadeout_effect_update(&info->fadeout,info->updateTimeout.elapsed) ) {
        if (info->gameContext == pok_game_intro_context)
            /* the intro context is just a fadeout that leads to the world context */
            info->gameContext = pok_game_world_context;
        else if (info->gameContext == pok_game_warp_fadeout_context || info->gameContext == pok_game_warp_fadeout_cave_context) {
            /* we just finished a fadeout for a warp; now finish the transition by setting a fadein effect; set the fadein
               effect first so that we avoid a race condition */
            info->fadeout.delay = WARP_FADEIN_DELAY;
            pok_fadeout_effect_set_update(
                &info->fadeout,
                info->sys,
                WARP_FADEIN_TIME,
                info->gameContext == pok_game_warp_fadeout_context ? pok_fadeout_black_screen : pok_fadeout_to_center,
                TRUE);
            /* complete the map warp */
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
            info->gameContext = pok_game_warp_fadein_context;
        }
        else if (info->gameContext == pok_game_warp_latent_fadeout_context || info->gameContext == pok_game_warp_latent_fadeout_cave_context
            || info->gameContext == pok_game_warp_latent_fadeout_door_context) {
            /* we just finished a fadeout for a latent warp; now finish the transition by setting a fadein effect; set
               the fadein effect first to avoid a race condition */
            info->fadeout.delay = WARP_FADEIN_DELAY;
            pok_fadeout_effect_set_update(
                &info->fadeout,
                info->sys,
                WARP_FADEIN_TIME,
                info->gameContext == pok_game_warp_latent_fadeout_cave_context ? pok_fadeout_to_center : pok_fadeout_black_screen,
                TRUE);
            /* complete the map warp */
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
                        pok_game_modify_enter(info->playerContext);
                        pok_character_context_set_player(info->playerContext,info->mapRC);
                        pok_game_modify_exit(info->playerContext);
                        if (info->gameContext != pok_game_warp_latent_fadeout_context) {
                            /* we do an animation if exiting a building or cave */
                            pok_game_modify_enter(info->playerContext);
                            info->playerContext->slowDown = FALSE;
                            pok_character_context_set_update(
                                info->playerContext,
                                direction,
                                pok_character_normal_effect,
                                info->sys->dimension,
                                TRUE );
                            pok_game_modify_exit(info->playerContext);
                            pok_game_modify_enter(info->mapRC);
                            pok_map_render_context_set_update(
                                info->mapRC,
                                direction,
                                info->sys->dimension );
                            pok_game_modify_exit(info->mapRC);
                        }
                        pok_graphics_subsystem_unlock(info->sys);
                        /* pause the map scrolling until the fadein effect completes */
                        info->pausePlayerMap = TRUE;
                    }
                }
                info->mapTrans = NULL;
            }
            info->gameContext = pok_game_warp_fadein_context;
        }
        else if (info->gameContext == pok_game_warp_fadein_context) {
            /* fade in to world from warp */
            info->gameContext = pok_game_world_context;
            info->pausePlayerMap = FALSE;
        }
    }
}

void map_terrain_logic(struct pok_game_info* info)
{
    if (info->gameContext == pok_game_sliding_context)
        player_move_logic(info,info->player->direction);
}

bool_t latent_warp_logic(struct pok_game_info* info,enum pok_direction direction)
{
    /* check to see if the current location has a latent warp; a latent warp is delayed; the
       player must first walk onto the tile, then walk in the specified direction */
    struct pok_tile_data* tdata;
    enum pok_tile_warp_kind kind;
    tdata = &info->mapRC->chunk->data[info->mapRC->relpos.row][info->mapRC->relpos.column].data;
    kind = tdata->warpKind;
    if ((((kind == pok_tile_warp_latent_up || kind == pok_tile_warp_latent_cave_up || kind == pok_tile_warp_latent_door_up)
            && direction == pok_direction_up)
        || ((kind == pok_tile_warp_latent_down || kind == pok_tile_warp_latent_cave_down || kind == pok_tile_warp_latent_door_down)
            && direction == pok_direction_down)
        || ((kind == pok_tile_warp_latent_left || kind == pok_tile_warp_latent_cave_left || kind == pok_tile_warp_latent_door_left)
            && direction == pok_direction_left)
        || ((kind == pok_tile_warp_latent_right || kind == pok_tile_warp_latent_cave_right || kind == pok_tile_warp_latent_door_right)
            && direction == pok_direction_right)) && direction == info->player->direction) {
        /* set warp effect (latent warps always fadeout) */
        pok_fadeout_effect_set_update(
            &info->fadeout,
            info->sys,
            WARP_FADEOUT_TIME,
            kind >= pok_tile_warp_latent_cave_up ? pok_fadeout_to_center : pok_fadeout_black_screen,
            FALSE );
        /* set game context; test to see if this is a cave warp or not */
        info->gameContext = kind >= pok_tile_warp_latent_cave_up
            ? pok_game_warp_latent_fadeout_cave_context : (kind >= pok_tile_warp_latent_door_up 
                ? pok_game_warp_latent_fadeout_door_context : pok_game_warp_latent_fadeout_context);
        /* cache a reference to the tile data; it will be used to change the map render context
           after the transition has finished */
        info->mapTrans = tdata;
        /* return true to mean that we applied a warp */
        return TRUE;
    }
    /* we did not find a warp */
    return FALSE;
}

bool_t warp_logic(struct pok_game_info* info)
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
        else
            return FALSE;
        /* cache a reference to the tile data; it will be used to change the map render context
           after the transition has finished */
        info->mapTrans = tdata;
        /* returning TRUE means we found a warp and have setup for a transition */
        return TRUE;
    }
    return FALSE;
}

static void intermsg_noop(struct pok_game_info* info)
{
    pok_game_deactivate_menus(info); /* clear menu effects */
    info->gameContext = pok_game_world_context;
}

void intermsg_logic(struct pok_game_info* info)
{
    if (info->gameContext == pok_game_intermsg_context) {
        pok_game_modify_enter(&info->updateInterMsg);
        if (info->ioInterMsg.ready && info->updateInterMsg.processed) {
            /* we got a response: process it immediately so that it's synchronized with the
               initial input operation */
            pok_intermsg_discard(&info->updateInterMsg);
            if (info->ioInterMsg.kind == pok_noop_intermsg) {
                /* this is a noop response (no action to be taken) */
                intermsg_noop(info);
            }
            else if (info->ioInterMsg.kind == pok_menu_intermsg) {
                /* begin a menu sequence; look at the modifier flags to determine which
                   kind of menu to create; we only create a single menu at a time until; this
                   lets the menus "stack up" on the screen; a future no-op intermsg will close
                   them all when the menu sequence is finished */
                pok_game_activate_menu(info,info->ioInterMsg.modflags,info->ioInterMsg.payload.string);
                info->gameContext = pok_game_menu_context;
            }
            /* it is our responsibility to discard the IO intermsg when we are done */
            pok_intermsg_discard(&info->ioInterMsg);
        }
        else if (pok_intermsg_update(&info->updateInterMsg,info->updateTimeout.elapsed)) {
            /* the timeout occurred: discard the intermsg and perform a no-op */
            pok_intermsg_discard(&info->updateInterMsg);
            intermsg_noop(info);
        }
        pok_game_modify_exit(&info->updateInterMsg);
    }
}

enum pok_character_effect get_effect_from_terrain(struct pok_map_render_context* mapRC,
    struct pok_tile_manager* tman,enum pok_direction direction)
{
    uint16_t i;
    uint16_t tileid;
    struct pok_tile* tile;
    /* obtain current tile id */
    tileid = mapRC->chunk->data[mapRC->relpos.row][mapRC->relpos.column].data.tileid;
    /* check ice tiles: ice tiles cause the player to slide */
    for (i = 0;i < tman->terrain[pok_tile_terrain_ice].length;++i)
        if (tman->terrain[pok_tile_terrain_ice].list[i] == tileid)
            return pok_character_slide_effect;
    /* check ledge tiles: ledge tiles cause the player to jump over the ledge tile; the
       player must be facing the correct direction; the ledge tile is assumed to be one tile
       in front of the player */
    if (direction > pok_direction_up) {
        tile = pok_map_render_context_get_adjacent_tile(
            mapRC,
            direction == pok_direction_left ? -1 : (direction == pok_direction_right ? 1 : 0),
            direction == pok_direction_down ? 1 : 0 );
        if (tile != NULL) {
            int d = pok_tile_terrain_ledge_down + (direction-1);
            for (i = 0;i < tman->terrain[d].length;++i)
                if (tman->terrain[d].list[i] == tile->data.tileid)
                    return pok_character_jump_effect;
        }
    }
    return pok_character_normal_effect;
}
