/* pokgame.c - pokgame */
#include "pokgame.h"
#include "error.h"
#include "user.h"
#include "config.h"
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#ifndef POKGAME_TEST

/* functions */
static void aux_graphics_load();
static void aux_graphics_unload();
static void configure_stderr();
static void log_termination();

const char* POKGAME_NAME;

/* pokgame entry point */
int main(int argc,const char* argv[])
{
    struct pok_graphics_subsystem* sys;
    POKGAME_NAME = argv[0];

    /* C-library initialization */
    srand( (unsigned int) time(NULL) );
    configure_stderr();

    /* load all modules */
    pok_exception_load_module();
    pok_user_load_module();
    pok_netobj_load_module();
    pok_gamelock_load_module();

    /* initialize a graphics subsystem for the game; this corresponds to the
       application's top-level window; start up the window and renderer before
       anything else; depending on the platform, the renderer may be run on
       another thread or require the main thread */
    sys = pok_graphics_subsystem_new();
    pok_graphics_subsystem_default(sys);
    sys->loadRoutine = aux_graphics_load;
    sys->unloadRoutine = aux_graphics_unload;
    if ( !pok_graphics_subsystem_begin(sys) )
        pok_error(pok_error_fatal,"could not begin graphics subsystem");

    if ( !sys->background ) {
        /* the platform cannot run the window on another thread, so start up the IO
           procedure on a background thread and run the window on this thread */
        struct pok_thread* iothread;
        iothread = pok_thread_new((pok_thread_entry)io_proc,sys);
        pok_thread_start(iothread);
        pok_graphics_subsystem_render_loop(sys);
        pok_thread_free(iothread);
    }
    else
        /* begin the IO procedure; this is the entry point into the game */
        io_proc(sys,NULL);

    /* close graphics subsystem: if the graphics subsystem was working on another
       thread this will wait to join back up with said thread; otherwise the graphics
       subsystem has already finished and is waiting to be cleaned up */
    pok_graphics_subsystem_free(sys);

    /* unload all modules */
    pok_gamelock_unload_module();
    pok_netobj_unload_module();
    pok_user_unload_module();
    pok_exception_unload_module();

    log_termination();
    return 0;
}

void aux_graphics_load()
{
    /* this routine is called by the graphics subsystem each time it loads the
       graphics functionality; we load auxilary graphics here (other than the game's
       artwork which can be specified by a game version or the default subsystem) */

    pok_glyphs_load();
}

void aux_graphics_unload()
{
    /* this routine is executed on the graphics thread when the graphics functionality
       is unloading */

    pok_glyphs_unload();
}

void configure_stderr()
{
    /* redirect 'stderr' to the log file; open it for appending and
       place a timestamp */
    time_t t;
    char buf[128];
    struct tm* tinfo;
    struct pok_string* path = pok_get_content_root_path();
    if (path == NULL)
        exit(1);
    pok_string_concat(path,POKGAME_CONTENT_LOG_FILE);
    if (freopen(path->buf,"a",stderr) == NULL)
        exit(1);
#ifndef POKGAME_DEBUG
    time(&t);
    tinfo = localtime(&t);
    strftime(buf,sizeof(buf),"%a %b %d %Y %I:%M:%S %p",tinfo);
    fprintf(stderr,"%s: begin: %s\n",POKGAME_NAME,buf);
#endif
    pok_string_free(path);
}

void log_termination()
{
#ifndef POKGAME_DEBUG
    time_t t;
    char buf[128];
    struct tm* tinfo;
    time(&t);
    tinfo = localtime(&t);
    strftime(buf,sizeof(buf),"%a %b %d %Y %I:%M:%S %p",tinfo);
    fprintf(stderr,"%s: end: %s\n",POKGAME_NAME,buf);
#endif
}

#endif /* POKGAME_TEST */

/* pok_intermsg */
void pok_intermsg_setup(struct pok_intermsg* im,enum pok_intermsg_kind kind,int32_t delay)
{
    im->ready = FALSE; /* delay readiness until caller can set content */
    im->kind = kind;
    im->processed = FALSE;
    im->delay = delay;
    im->modflags = 0;
    switch (kind) {
    case pok_stringinput_intermsg:
    case pok_menu_intermsg:
        im->payload.string = pok_string_new();
        break;
    case pok_keyinput_intermsg:
        im->payload.key = pok_input_key_unknown;
        break;
    case pok_selection_intermsg:
        im->payload.index = -1;
        break;
    default:
        break;
    }
}
bool_t pok_intermsg_update(struct pok_intermsg* im,uint32_t ticks)
{
    /* update elapsed time; if the timeout has occurred then return TRUE,
       FALSE otherwise */
    if (im->ready) {
        im->delay -= ticks;
        if (im->delay <= 0)
            return TRUE;
    }
    return FALSE;
}
void pok_intermsg_discard(struct pok_intermsg* im)
{
    switch (im->kind) {
    case pok_stringinput_intermsg:
    case pok_menu_intermsg:
        pok_string_free(im->payload.string);
        break;
    default:
        break;
    }
    im->processed = TRUE;
    im->delay = 0;
    im->kind = pok_uninitialized_intermsg;
    im->ready = FALSE;
}

static void pok_game_render_menus(const struct pok_graphics_subsystem* sys,struct pok_game_info* game)
{
    /* this function is a high-level entry to rendering the game's menus */
    if (game->messageMenu.base.active)
        pok_message_menu_render(&game->messageMenu);
    else if (game->inputMenu.base.active)
        pok_input_menu_render(&game->inputMenu);

    (void)sys;
}

/* pok_game_info */
struct pok_game_info* pok_game_new(struct pok_graphics_subsystem* sys,struct pok_game_info* template)
{
    /* this function creates a new game state object; if 'template' is not NULL, then its static network
       objects are copied by reference into the new game info */
    struct pok_game_info* game;
    const struct pok_user_info* userInfo = pok_user_get_info();
    game = malloc(sizeof(struct pok_game_info));
    if (game == NULL)
        pok_error(pok_error_fatal,"failed memory allocation in pok_game_new()");
    /* initialize general parameters */
    pok_timeout_interval_reset(&game->ioTimeout,100);
    pok_timeout_interval_reset(&game->updateTimeout,10); /* needs to be pretty high-resolution for good performance */
    game->staticOwnerMask = template == NULL ? (2 << _pok_static_obj_top) - 1 : 0x00;
    game->control = TRUE;
    game->gameContext = pok_game_intro_context;
    game->pausePlayerMap = FALSE;
    game->versionProc = NULL;
    game->versionCBack = NULL;
    pok_string_init(&game->versionLabel);
    game->versionChannel = NULL;
    game->updateThread = pok_thread_new((pok_thread_entry)update_proc,game);
    if (game->updateThread == NULL)
        pok_error_fromstack(pok_error_fatal);
    /* assign graphics subsystem */
    game->sys = sys;
    /* initialize effects */
    pok_fadeout_effect_init(&game->fadeout);
    game->fadeout.keep = TRUE;
    if (template == NULL) {
        /* initialize tile and sprite image managers; we will own these objects */
        game->tman = pok_tile_manager_new(game->sys);
        if (game->tman == NULL)
            pok_error_fromstack(pok_error_fatal);
        game->sman = pok_sprite_manager_new(game->sys);
        if (game->sman == NULL)
            pok_error_fromstack(pok_error_fatal);
    }
    else {
        /* assign tile and sprite image managers from the existing game; the
           caller should take care not to delete the template game before this one */
        game->tman = template->tman;
        game->sman = template->sman;
    }
    /* initialize maps */
    game->world = pok_world_new();
    if (game->world == NULL)
        pok_error_fromstack(pok_error_fatal);
    game->mapTrans = NULL;
    game->mapRC = pok_map_render_context_new(game->tman);
    if (game->mapRC == NULL)
        pok_error_fromstack(pok_error_fatal);
    /* initialize character render context */
    game->charRC = pok_character_render_context_new(game->mapRC,game->sman);
    /* initialize player character and add it to the character render context */
    game->player = pok_character_new();
    if (game->player == NULL)
        pok_error_fromstack(pok_error_fatal);
    game->player->isPlayer = TRUE;
    if (userInfo != NULL)
        game->player->spriteIndex = userInfo->sprite;
    game->playerEffect = pok_character_normal_effect;
    game->playerContext = pok_character_render_context_add_ex(game->charRC,game->player);
    if (game->playerContext == NULL)
        pok_error_fromstack(pok_error_fatal);
    pok_intermsg_setup(&game->updateInterMsg,pok_uninitialized_intermsg,0);
    pok_intermsg_setup(&game->ioInterMsg,pok_uninitialized_intermsg,0);
    pok_message_menu_init(&game->messageMenu,sys);
    pok_input_menu_init(&game->inputMenu,sys);
    return game;
}
void pok_game_free(struct pok_game_info* game)
{
    pok_input_menu_delete(&game->inputMenu);
    pok_message_menu_delete(&game->messageMenu);
    pok_intermsg_discard(&game->updateInterMsg);
    pok_intermsg_discard(&game->ioInterMsg);
    pok_character_free(game->player);
    pok_character_render_context_free(game->charRC);
    pok_world_free(game->world);
    pok_map_render_context_free(game->mapRC);
    if (game->staticOwnerMask & 0x01)
        pok_sprite_manager_free(game->sman);
    if (game->staticOwnerMask & 0x02)
        pok_tile_manager_free(game->tman);
    pok_thread_free(game->updateThread);
    pok_string_delete(&game->versionLabel);
    if (game->versionProc != NULL)
        pok_process_free(game->versionProc);
    if (game->versionChannel != NULL)
        pok_data_source_free(game->versionChannel);
    free(game);
}
void pok_game_static_replace(struct pok_game_info* game,enum pok_static_obj_kind kind,void* obj)
{
    /* replace the specified static network object; the 'pok_game_info' instance will assume
       ownership of the object; free any owned object before assigning */
    switch (kind) {
    case pok_static_obj_tile_manager:
        if (game->staticOwnerMask & 0x01)
            pok_tile_manager_free(game->tman);
        else
            game->staticOwnerMask |= 0x01;
        game->tman = obj;
        break;
    case pok_static_obj_sprite_manager:
        if (game->staticOwnerMask & 0x02)
            pok_sprite_manager_free(game->sman);
        else
            game->staticOwnerMask |= 0x02;
        game->sman = obj;
        break;
    default:
#ifdef POKGAME_DEBUG
        pok_error(pok_error_fatal,"bad static network object kind to pok_game_static_replace()");
#endif
        return;
    }
}
void pok_game_register(struct pok_game_info* game)
{
    /* the order of the graphics routines is important */
    pok_graphics_subsystem_register(game->sys,(graphics_routine_t)pok_map_render,game->mapRC);
    pok_graphics_subsystem_register(game->sys,(graphics_routine_t)pok_character_render,game->charRC);
    pok_graphics_subsystem_register(game->sys,(graphics_routine_t)pok_game_render_menus,game);
    pok_graphics_subsystem_register(game->sys,(graphics_routine_t)pok_fadeout_effect_render,&game->fadeout);
}
void pok_game_unregister(struct pok_game_info* game)
{
    pok_graphics_subsystem_unregister(game->sys,(graphics_routine_t)pok_map_render,game->mapRC);
    pok_graphics_subsystem_unregister(game->sys,(graphics_routine_t)pok_character_render,game->charRC);
    pok_graphics_subsystem_unregister(game->sys,(graphics_routine_t)pok_game_render_menus,game);
    pok_graphics_subsystem_unregister(game->sys,(graphics_routine_t)pok_fadeout_effect_render,&game->fadeout);
}
void pok_game_load_textures(struct pok_game_info* game)
{
    pok_graphics_subsystem_create_textures(
        game->sys,
        2,
        game->tman->tileset, game->tman->tilecnt,
        game->sman->spritesets, game->sman->imagecnt );
}
void pok_game_delete_textures(struct pok_game_info* game)
{
    pok_graphics_subsystem_delete_textures(
        game->sys,
        2,
        game->tman->tileset, game->tman->tilecnt,
        game->sman->spritesets, game->sman->imagecnt );
}
void pok_game_activate_menu(struct pok_game_info* game,enum pok_menu_kind menuKind,struct pok_string* assignText)
{
    /* activate specified menu; it is given focus by default; we don't care if a menu is already activated; in
       that case the menu will simply be reset and start over again */
    if (menuKind == pok_message_menu) {
        if (assignText != NULL)
            pok_message_menu_activate(&game->messageMenu,assignText->buf);
        else {
            /* activate with no text [ pok_message_menu_activate(&game->messageMenu,"") ] */
            pok_text_context_reset(&game->messageMenu.text);
            game->messageMenu.base.active = TRUE;
        }
    }
    else if (menuKind == pok_input_menu) {
        if (assignText != NULL)
            pok_input_menu_activate(&game->inputMenu,assignText->buf);
        else {
            /* activate with no text [ pok_input_menu_activate(&game->inputMenu,"") ] */
            pok_text_context_reset(&game->inputMenu.input.base);
            game->inputMenu.base.active = TRUE;
        }
    }
}
void pok_game_deactivate_menus(struct pok_game_info* game)
{
    pok_message_menu_deactivate(&game->messageMenu);
    pok_input_menu_deactivate(&game->inputMenu);
}
