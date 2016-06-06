/* pokgame.h - pokgame */
#ifndef POKGAME_POKGAME_H
#define POKGAME_POKGAME_H
#include "net.h"
#include "graphics.h"
#include "gamelock.h"
#include "tileman.h"
#include "spriteman.h"
#include "map-context.h"
#include "character-context.h"
#include "effect.h"
#include "menu.h"
#include "protocol.h"

/* pok_game_context: flag current game state */
enum pok_game_context
{
    pok_game_intro_context, /* the game is processing the intro screen */
    pok_game_pause_context, /* the game is doing nothing */
    pok_game_world_context,  /* the game is handling map logic */
    pok_game_warp_fadeout_context, /* the game is handling a warp fadeout */
    pok_game_warp_fadeout_cave_context, /* the game is handling a cave exit warp */
    pok_game_warp_latent_fadeout_context, /* the game is handling a latent warp fadeout */
    pok_game_warp_latent_fadeout_door_context, /* the game is handling a latent door exit warp */
    pok_game_warp_latent_fadeout_cave_context, /* the game is handling a latent cave exit warp */
    pok_game_warp_fadein_context, /* the game is handling a warp fadein */
    pok_game_warp_spin_context, /* the game is handling the initial spin warp sequence */
    pok_game_warp_spinup_context, /* the game is handling a spin warp (player moving up) */
    pok_game_warp_spindown_context, /* the game is handling a spin warp (player moving down) */
    pok_game_sliding_context, /* the player is sliding along ice tiles */
    pok_game_intermsg_context, /* the game is waiting for an intermsg reply */
    pok_game_menu_context /* the player is engaged in a menu activity */
};

/* intermessages: the update and io procedures use "intermessages" to perform
   remote operations; the game engine allocates two intermessage structures: one
   for the update proc and one for the io proc */

/* pok_intermsg_kind: flag intermsg kinds */
enum pok_intermsg_kind
{
    pok_uninitialized_intermsg,

    /* messages sent from the update proc to the io proc */

    pok_keyinput_intermsg,      /* key input in gameworld context */
    pok_stringinput_intermsg,   /* string input received from input message menu */
    pok_completed_intermsg,     /* simple message menu did complete */
    pok_selection_intermsg,     /* selection menu did complete (with selection, or -1 if cancel) */

    /* messages sent from the io proc to the update proc */

    pok_noop_intermsg,          /* no operation is to be performed in response */
    pok_menu_intermsg,          /* a menu is to be created */
};

struct pok_intermsg
{
    /* data payload of the intermsg; each member has an annotation describing
       the intermsg kind(s) for which the member is used */
    union {
        enum pok_input_key key;        /* pok_keyinput_intermsg */
        struct pok_string* string;     /* pok_stringinput_intermsg, pok_menu_intermsg */
        int32_t index;                 /* pok_selection_intermsg */
    } payload;
    uint32_t modflags; /* modifier flags: see protocol.h for intermsg modifier flags */

    bool_t ready;   /* flag if the intermsg is ready for consumption */
    int32_t delay; /* number of game ticks to wait before discarding the message */
    bool_t processed; /* flag indicating state of intermsg */
    enum pok_intermsg_kind kind; /* message kind */
};
void pok_intermsg_setup(struct pok_intermsg* im,enum pok_intermsg_kind kind,int32_t delay);
bool_t pok_intermsg_update(struct pok_intermsg* im,uint32_t ticks);
void pok_intermsg_discard(struct pok_intermsg* im);

struct pok_game_info;
typedef struct pok_game_info* (*pok_game_callback)(struct pok_game_info* info);

/* this structure stores all of the top-level game information */
struct pok_game_info
{
    /* bitmask for static network object ownership: */
    byte_t staticOwnerMask;

    /* controls the update procedure; if non-zero then the update procedure is allowed
       to execute; if it becomes FALSE then the update procedure is to exit; the update
       procedure may also exit for other reasons */
    volatile bool_t control;

    /* update thread handle */
    struct pok_thread* updateThread;

    /* version information */
    struct pok_process* versionProc; /* description of version process (if a local process is running the version) */
    struct pok_string versionLabel; /* version label of the form: "Text Label\0GUID\0" */
    pok_game_callback versionCBack; /* if non-NULL, then a procedure that executes the IO procedure for the version */
    struct pok_data_source* versionChannel; /* data source for version IO */

    /* timeouts for main game procedures (in thousandths of a second) */
    struct pok_timeout_interval ioTimeout;
    struct pok_timeout_interval updateTimeout;

    /* game control flags */
    enum pok_game_context gameContext; /* flag what the game is currently doing */
    enum pok_game_context contextStk[5]; /* previous contexts (not always required) */
    int contextStkTop;
    bool_t pausePlayerMap; /* if non-zero, then stop updating player and map */

    /* graphics: we do not own this object; it is managed by another context; it
       is stored here for convenience only */
    struct pok_graphics_subsystem* sys;

    /* effects */
    struct pok_fadeout_effect fadeout;
    struct pok_daycycle_effect daycycle;

    /* tile images */
    struct pok_tile_manager* tman;

    /* sprite images */
    struct pok_sprite_manager* sman;

    /* maps */
    struct pok_world* world; /* owned by the engine, not the version */
    struct pok_tile_data* mapTrans;
    struct pok_map_render_context* mapRC;

    /* character render context */
    struct pok_character_render_context* charRC;

    /* player character */
    struct pok_character* player; /* owned by the engine, not the version */
    enum pok_character_effect playerEffect;
    struct pok_character_context* playerContext; /* cached */

    /* intermessage structures: the update and io procs each get one */
    struct pok_intermsg updateInterMsg; /* gamelock for this object controls access to 'ioInterMsg' */
    struct pok_intermsg ioInterMsg;

    /* menu structures */
    struct pok_message_menu messageMenu;
    struct pok_input_menu inputMenu;
};

/* main pokgame procedures (the other procedure is graphics which is handled by the graphics subsystem) */
int io_proc(struct pok_graphics_subsystem* sys,struct pok_game_info* game);
int update_proc(struct pok_game_info* info);

/* game initialization/closing */
struct pok_game_info* pok_game_new(struct pok_graphics_subsystem* sys,struct pok_game_info* template);
void pok_game_free(struct pok_game_info* game);
void pok_game_static_replace(struct pok_game_info* game,enum pok_static_obj_kind kind,void* obj);
void pok_game_register(struct pok_game_info* game);
void pok_game_unregister(struct pok_game_info* game);
void pok_game_load_textures(struct pok_game_info* game);
void pok_game_delete_textures(struct pok_game_info* game);
void pok_game_context_push(struct pok_game_info* game);
void pok_game_context_pop(struct pok_game_info* game);

/* misc game operations */
void pok_game_activate_menu(struct pok_game_info* game,enum pok_menu_kind menuKind,struct pok_string* assignText);
void pok_game_deactivate_menus(struct pok_game_info* game);

#endif
