/* io-proc.c - pokgame */
#include "pokgame.h"
#include "error.h"
#include "protocol.h"
#include "default.h"
#include "user.h"
#include <stdlib.h>
#include <string.h>

/* the IO procedure controls the flow of game state in the program; data flow could
   exist within this process or with a remote peer; the IO procedure must make sure
   it obtains a readers'/writer lock when reading or writing to the game state; the
   IO procedure does not control the render procedure; the graphics subsystem is
   handled by the caller; the IO procedure is, however, responsible for the update thread
   which it spawns for the version it runs */

/* constants */
#define WAIT_TIMEOUT 10000 /* wait 10 seconds before giving up on a read during the synchronous exchanges */
#define ERROR_WAIT "version failed to respond in enough time"

/* flags */
enum pok_io_result
{
    pok_io_result_finished, /* the version has finished normally and the engine should return to the default version */
    pok_io_result_quit,     /* the version has finished normally but the user wants to quit the application */
    pok_io_result_error,    /* the version didn't follow the protocol and one or more errors are on the exception stack */
    pok_io_result_waiting   /* the version did not fully specify a sequence, so */
};

/* structures */
struct pok_io_info
{
    uint32_t elapsed;
    struct pok_string string;
    struct pok_netobj_readinfo readInfo;

    bool_t protocolMode;           /* if non-zero, then the binary-based protocol is used, otherwise the text-based protocol is used */
    bool_t usingDefault;           /* if non-zero, then the game's graphics subsystem has the default parameters */
};

/* functions */
static enum pok_io_result run_game(struct pok_game_info* game);
static enum pok_io_result exch_intro(struct pok_game_info* game,struct pok_io_info* info);
static enum pok_io_result exch_inter(struct pok_game_info* game,struct pok_io_info* info);
static enum pok_io_result exch_gener(struct pok_game_info* game,struct pok_io_info* info);
static bool_t seq_greet(struct pok_game_info* game,struct pok_io_info* info);
static bool_t seq_mode(struct pok_game_info* game,struct pok_io_info* info);
static bool_t seq_label(struct pok_game_info* game,struct pok_io_info* info);
static bool_t seq_graphics_subsystem(struct pok_game_info* game,struct pok_io_info* info);
static bool_t seq_tile_manager(struct pok_game_info* game,struct pok_io_info* info);
static bool_t seq_sprite_manager(struct pok_game_info* game,struct pok_io_info* info);
static bool_t seq_player_character(struct pok_game_info* game,struct pok_io_info* info);
static bool_t seq_first_map(struct pok_game_info* game,struct pok_io_info* info);

int io_proc(struct pok_graphics_subsystem* sys,struct pok_game_info* game)
{
    bool_t madeDefault = game == NULL;
    struct pok_game_info* save = NULL;

    /* don't try to render game until we have something set up */
    pok_graphics_subsystem_game_render_state(sys,FALSE);

    /* create a default game (if none specified); it will have an external
       version procedure */
    if (madeDefault) {
        game = pok_make_default_game(sys);
        if (game == NULL)
            return 1;
        /* load the default game's textures */
        pok_game_load_textures(game);
    }

    /* run versions in a loop; a version ends when either the version callback
       or the 'run_game' function returns */
    do {
        if (game->versionCBack != NULL) {
            /* this game will be run by an external procedure; if it returns a pointer
               to a new version, then we save the current game and switch to the new
               version; when the new version ends, we will switch back to the original game */
            struct pok_game_info* ver;

            pok_game_register(game);              /* register game rendering functions */
            pok_graphics_subsystem_game_render_state(sys,TRUE); /* let render process begin rendering */
            pok_thread_start(game->updateThread); /* start up update process */
            ver = (*game->versionCBack)(game);    /* execute version callback */
            game->control = FALSE;                /* inform update procedure that we are finished */
            pok_thread_join(game->updateThread);  /* wait for update thread to end */
            pok_graphics_subsystem_game_render_state(sys,FALSE); /* turn off rendering */
            pok_game_unregister(game);            /* unregister rendering routines */

            /* process new version to play (if any); save the current game */
            if (ver != NULL) {
                save = game;
                game = ver;
            }
            else
                break;
        }
        else if (game->versionChannel != NULL) {
            /* the version is specifying a data channel; we can use this to engage the pokgame
               protocol with a peer; the peer could be a pokgame version process executing on
               a remote or local host */
            enum pok_io_result result;

            pok_game_register(game);              /* register game rendering functions */
            pok_graphics_subsystem_game_render_state(sys,TRUE); /* let render process begin rendering */
            pok_thread_start(game->updateThread); /* start up update process */
            result = run_game(game);              /* run the game, engaging the pokgame protocol with a peer */
            game->control = FALSE;                /* inform update procedure that we are finished */
            pok_thread_join(game->updateThread);  /* wait for update thread to end */
            pok_graphics_subsystem_game_render_state(sys,FALSE); /* turn off rendering */
            pok_game_unregister(game);            /* unregister rendering routines */

            /* free the game; it is not the default version so we destroy it*/
            pok_game_free(game);
            game = save;
            if (result == pok_io_result_quit)
                break;
        }
        else {
            pok_error(pok_error_warning,"game did not have IO source");
            break;
        }
    } while ( pok_graphics_subsystem_has_window(game->sys) );

    /* if we created a default version to play, then free its textures and then
       the game info structure */
    if (madeDefault) {
        pok_game_delete_textures(game);
        pok_game_free(game);
    }
    return 0;
}

enum pok_io_result run_game(struct pok_game_info* game)
{
    struct pok_io_info info;
    enum pok_io_result result;
    pok_string_init(&info.string);
    pok_netobj_readinfo_init(&info.readInfo);

    /* introductory exchange */
    result = exch_intro(game,&info);
    if (result != pok_io_result_finished)
        return result;
    if ( !info.protocolMode ) {
        /* text-mode works differently (it is a minimal implementation; transfer control
           to another module to handle it */
        pok_error(pok_error_fatal,"feature is unimplemented");
        return pok_io_result_finished;
    }

    /* intermediate exchange */
    result = exch_inter(game,&info);
    if (result != pok_io_result_finished)
        return result;

    /* enter a loop to handle the game IO operations; each iteration we check to see
       if we can perform a general exchange operation; this code expects asychro-
       nous IO operations so that it can move on to other things while IO takes place */
    while (TRUE) {
        /* general exchange operation */
        result = exch_gener(game,&info);
        if (result != pok_io_result_finished && result != pok_io_result_waiting)
            break;

        /* intermessage processing */

    }

    /* reset the window's title bar text back to its title for the default version
       as well as default parameters if we modified them */
    if (!info.usingDefault)
        /* this sets the title bar to 'default' as well */
        pok_graphics_subsystem_default(game->sys);
    else
        pok_graphics_subsystem_assign_title(game->sys,"default");

    pok_netobj_readinfo_delete(&info.readInfo);
    pok_string_delete(&info.string);
    return result;
}

enum pok_io_result exch_intro(struct pok_game_info* game,struct pok_io_info* info)
{
    /* perform the introductory exchange: this consists of the greetings sequence, the mode sequence
       and the associated label sequence */
    if ( !seq_greet(game,info) )
        return pok_io_result_error;
    if ( !seq_mode(game,info) )
        return pok_io_result_error;
    if ( !seq_label(game,info) )
        return pok_io_result_error;
    return pok_io_result_finished;
}

enum pok_io_result exch_inter(struct pok_game_info* game,struct pok_io_info* info)
{
    /* read bitmask determining which network objects are to be sent */
    byte_t bitmask;
    while ( !pok_data_stream_read_byte(game->versionChannel,&bitmask) ) {
        if ( !pok_exception_pop_ex(pok_ex_net,pok_ex_net_pending) )
            return pok_io_result_error;
        pok_timeout(&game->ioTimeout);
        info->elapsed += game->ioTimeout.elapsed;
        if (info->elapsed > WAIT_TIMEOUT) {
            pok_exception_new_format("exch_inter: " ERROR_WAIT);
            return pok_io_result_error;
        }
    }
    /* depending on the bitmask, netread static network objects */
    info->usingDefault = (bitmask & POKGAME_DEFAULT_GRAPHICS_MASK) == 0;
    if (!info->usingDefault)
        if ( !seq_graphics_subsystem(game,info) )
            return pok_io_result_error;
    if (bitmask & POKGAME_DEFAULT_TILES_MASK)
        if ( !seq_tile_manager(game,info) )
            return pok_io_result_error;
    if (bitmask & POKGAME_DEFAULT_SPRITES_MASK)
        if ( !seq_sprite_manager(game,info) )
            return pok_io_result_error;
    /* netwrite the player character object */
    if ( !seq_player_character(game,info) )
        return pok_io_result_error;
    /* netread the first map */
    if ( !seq_first_map(game,info) )
        return pok_io_result_error;
    return pok_io_result_finished;
}

enum pok_io_result exch_gener(struct pok_game_info* game,struct pok_io_info* info)
{

    return pok_io_result_finished;
}

static bool_t read_string(struct pok_game_info* game,struct pok_io_info* info)
{
    info->elapsed = 0;
    while ( !pok_data_stream_read_string_ex(game->versionChannel,&info->string) ) {
        if ( pok_exception_pop_ex(pok_ex_net,pok_ex_net_pending) )
            /* some data was received but not enough; reset timer */
            info->elapsed = 0;
        else if ( !pok_exception_pop_ex(pok_ex_net,pok_ex_net_wouldblock) ) {
            pok_string_clear(&info->string);
            return FALSE;
        }
        pok_timeout(&game->ioTimeout);
        info->elapsed += game->ioTimeout.elapsed;
        if (info->elapsed > WAIT_TIMEOUT) {
            pok_exception_new_format("exch_intro: " ERROR_WAIT);
            return FALSE;
        }
    }
    return TRUE;
}

bool_t seq_greet(struct pok_game_info* game,struct pok_io_info* info)
{
    /* exchange greetings */
    if ( !pok_data_stream_write_string(game->versionChannel,POKGAME_GREETING_SEQUENCE) )
        return FALSE;
    if ( !read_string(game,info) )
        return FALSE;
    if (strcmp(POKGAME_GREETING_SEQUENCE,info->string.buf) != 0) {
        pok_string_clear(&info->string);
        pok_exception_new_format("seq_greet: peer did not send valid protocol greeting sequence");
        return FALSE;
    }
    pok_string_clear(&info->string);
    return TRUE;
}

bool_t seq_mode(struct pok_game_info* game,struct pok_io_info* info)
{
    /* read the protocol mode from the peer */
    if ( !read_string(game,info) )
        return FALSE;
    if (strcmp(POKGAME_BINARYMODE_SEQUENCE,info->string.buf) == 0)
        info->protocolMode = TRUE;
    else if (strcmp(POKGAME_TEXTMODE_SEQUENCE,info->string.buf) == 0)
        info->protocolMode = FALSE;
    else {
        pok_string_clear(&info->string);
        pok_exception_new_format("seq_mode: peer did not send valid protocol mode sequence");
        return FALSE;
    }
    pok_string_clear(&info->string);
    return TRUE;
}

bool_t seq_label(struct pok_game_info* game,struct pok_io_info* info)
{
    size_t sz;
    const struct pok_user_info* userInfo = pok_user_get_info();
    if (userInfo == NULL) {
        pok_exception_new_format("seq_label: no user information available");
        return FALSE;
    }
    /* receive the version's associated label; this is used to give a title for the version
       so we assign it to the graphics subsystem; this will update the window title bar text */
    if ( !read_string(game,info) )
        return FALSE;
    pok_graphics_subsystem_assign_title(game->sys,info->string.buf);
    pok_string_assign(&game->versionLabel,info->string.buf);
    /* receive the version's guid; this helps to uniquely identify a version process; the guid
       is formatted as a human readible text string */
    if ( !read_string(game,info) )
        return FALSE;
    if (info->string.len != GUID_LENGTH) {
        pok_exception_new_format("seq_label: expected guid of length %d",GUID_LENGTH);
        return FALSE;
    }
    pok_string_concat(&game->versionLabel,info->string.buf);
    /* send our guid so the version can better uniquely identify us */
    if ( !pok_data_source_write(game->versionChannel,(byte_t*)userInfo->guid.buf,GUID_LENGTH,&sz) )
        return FALSE;
    return TRUE;
}

bool_t seq_graphics_subsystem(struct pok_game_info* game,struct pok_io_info* info)
{
    /* netread the graphics subsystem parameters; this will override the default parameters; we
       make this property (info->usingDefault) so that we can reapply them after this game */
    enum pok_network_result result;
    info->elapsed = 0;
    while (TRUE) {
        result = pok_graphics_subsystem_netread(game->sys,game->versionChannel,&info->readInfo);
        if (result != pok_net_incomplete)
            break;
        if (info->readInfo.pending)
            info->elapsed = 0;
        pok_timeout(&game->ioTimeout);
        info->elapsed += game->ioTimeout.elapsed;
        if (info->elapsed > WAIT_TIMEOUT) {
            pok_exception_new_format("exch_inter: " ERROR_WAIT);
            return FALSE;
        }
    }
    pok_netobj_readinfo_reset(&info->readInfo);
    if (result != pok_net_completed)
        return FALSE;
    return TRUE;
}

bool_t seq_tile_manager(struct pok_game_info* game,struct pok_io_info* info)
{
    enum pok_network_result result;
    struct pok_tile_manager* tman = pok_tile_manager_new(game->sys);
    /* netread tile manager */
    info->elapsed = 0;
    while (TRUE) {
        result = pok_tile_manager_netread(tman,game->versionChannel,&info->readInfo);
        if (result != pok_net_incomplete)
            break;
        if (info->readInfo.pending)
            info->elapsed = 0;
        pok_timeout(&game->ioTimeout);
        info->elapsed += game->ioTimeout.elapsed;
        if (info->elapsed > WAIT_TIMEOUT) {
            pok_exception_new_format("exch_inter: " ERROR_WAIT);
            pok_tile_manager_free(tman);
            return FALSE;
        }
    }
    pok_netobj_readinfo_reset(&info->readInfo);
    if (result != pok_net_completed) {
        pok_tile_manager_free(tman);
        return FALSE;
    }
    /* assign tile manager to the game; the game will now
       be responsible for freeing it */
    pok_game_static_replace(game,pok_static_obj_tile_manager,tman);
    return TRUE;
}

bool_t seq_sprite_manager(struct pok_game_info* game,struct pok_io_info* info)
{
    enum pok_network_result result;
    struct pok_sprite_manager* sman = pok_sprite_manager_new(game->sys);
    /* netread sprite manager */
    info->elapsed = 0;
    while (TRUE) {
        result = pok_sprite_manager_netread(sman,game->versionChannel,&info->readInfo);
        if (result != pok_net_incomplete)
            break;
        if (info->readInfo.pending)
            info->elapsed = 0;
        pok_timeout(&game->ioTimeout);
        info->elapsed += game->ioTimeout.elapsed;
        if (info->elapsed > WAIT_TIMEOUT) {
            pok_exception_new_format("exch_inter: " ERROR_WAIT);
            pok_sprite_manager_free(sman);
            return FALSE;
        }
    }
    pok_netobj_readinfo_reset(&info->readInfo);
    if (result != pok_net_completed) {
        pok_sprite_manager_free(sman);
        return FALSE;
    }
    /* assign sprite manager to the game; the game will now
       be responsible for freeing it */
    pok_game_static_replace(game,pok_static_obj_sprite_manager,sman);
    return TRUE;
}

bool_t seq_player_character(struct pok_game_info* game,struct pok_io_info* info)
{
    /* read a unique network object id that we can assign to the player character object */

    /* netwrite player character object */

    return TRUE;
}

bool_t seq_first_map(struct pok_game_info* game,struct pok_io_info* info)
{
    /* netread a unique network object id for our world object  */

    /* netwrite our world object */

    /* expect a 'pok_world_method_add_map' operation to netread the first map */

    return TRUE;
}
