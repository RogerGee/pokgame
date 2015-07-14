/* graphics.h - pokgame */
#ifndef POKGAME_GRAPHICS_H
#define POKGAME_GRAPHICS_H
#include "netobj.h"
#include "image.h"

/* constants */
enum pok_graphics_constants
{
    MAX_GRAPHICS_ROUTINES = 32,
    MAX_WINDOW_PIXEL_WIDTH = 1280,
    MAX_WINDOW_PIXEL_HEIGHT = 768,
    DEFAULT_DIMENSION = 32,
    DEFAULT_WINDOW_WIDTH = 10,
    DEFAULT_WINDOW_HEIGHT = 9,
    DEFAULT_PLAYER_LOCATION_X = 4,
    DEFAULT_PLAYER_LOCATION_Y = 4,
    DEFAULT_PLAYER_OFFSET_X = 0,
    DEFAULT_PLAYER_OFFSET_Y = -8
};

/* exception ids generated by this module */
enum pok_ex_graphics
{
    pok_ex_graphics_bad_dimension,
    pok_ex_graphics_bad_window_size,
    pok_ex_graphics_bad_player_location,
    pok_ex_graphics_bad_player_offset,
    pok_ex_graphics_already_started
};

/* define normal input keys */
enum pok_input_key
{
    pok_input_key_ABUTTON,
    pok_input_key_BBUTTON,
    pok_input_key_ENTER,
    pok_input_key_BACK,
    pok_input_key_DEL,
    pok_input_key_UP,
    pok_input_key_DOWN,
    pok_input_key_LEFT,
    pok_input_key_RIGHT,
    pok_input_key_unknown
};

/* graphics routine: every graphics routine is passed two parameters, the graphics
   subsystem and a context; the context generally is the object that stores the game's
   render state for a particular module (note: this is not a graphics library context; that
   can be obtained via the graphics subsystem) */
struct pok_graphics_subsystem;
typedef void (*graphics_routine_t)(const struct pok_graphics_subsystem* sys,void* context);
typedef void (*graphics_load_routine_t)();

/* hook routine types */
typedef void (*keyup_routine_t)(enum pok_input_key key,void* context);
typedef void (*textentry_routine_t)(char asciiValue,void* context);

/* define graphics subsystem object; this abstracts input/output to a graphical window frame */
struct _pok_graphics_subsystem_impl;
struct pok_graphics_subsystem
{
    /* init info: this information is set by default or sent by a game server */
    uint16_t dimension; /* tile and sprite dimension */
    struct pok_size windowSize; /* dimensionalized size of viewable window */
    struct pok_location playerLocation; /* dimensionalized window location of player sprite (must be within 'windowSize') */
    struct pok_location _playerLocationInv; /* (used by the implementation) */
    int16_t playerOffsetX, playerOffsetY; /* pixel offset for player sprite (must be within 'dimension') */

    /* BEGIN hooks: a hook is a procedure that is called on the graphics procedure such as when an input event 
       occurs or a frame is rendered */

    /* graphics rendering routines called by the subsystem: these hooks in order each time a frame is rendered; a
       graphics rendering routine can be added/removed using the 'register' methods */
    uint16_t routinetop;
    graphics_routine_t routines[MAX_GRAPHICS_ROUTINES];
    void* contexts[MAX_GRAPHICS_ROUTINES];

    /* graphics load/unload routines: these are special "one-time" hooks that are called when the subsystem begins and ends */
    graphics_load_routine_t loadRoutine;
    graphics_load_routine_t unloadRoutine;

    /* other hooks: these can be conveniently added using the 'pok_graphics_subsystem_append_hook' macro */
    struct {
        /* called when a keyup event occurs: only the game keys enumerated in 'enum pok_input_key' are specified */
        uint16_t top;
        keyup_routine_t routines[MAX_GRAPHICS_ROUTINES];
        void* contexts[MAX_GRAPHICS_ROUTINES];
    } keyupHook;
    struct {
        /* like keyup, except the specified key is mapped to ASCII for text entry input */
        uint16_t top;
        textentry_routine_t routines[MAX_GRAPHICS_ROUTINES];
        void* contexts[MAX_GRAPHICS_ROUTINES];
    } textentryHook;

    /* END hooks */

    /* misc info used by rendering contexts */
    struct pok_image* blacktile; /* solid black tile image */
    int32_t wwidth, wheight; /* non-dimensionalized window width and height (obtained from 'windowSize') */

    /* implementation-specific information */
    struct _pok_graphics_subsystem_impl* impl;

    /* misc other */
    int framerate; /* frames per second */
    bool_t background; /* if non-zero then the subsystem is executing on a separate thread */
    struct pok_string title; /* title bar text in graphical frame */
};
struct pok_graphics_subsystem* pok_graphics_subsystem_new();
void pok_graphics_subsystem_free(struct pok_graphics_subsystem* sys);
void pok_graphics_subsystem_init(struct pok_graphics_subsystem* sys);
void pok_graphics_subsystem_delete(struct pok_graphics_subsystem* sys);
void pok_graphics_subsystem_reset(struct pok_graphics_subsystem* sys);
bool_t pok_graphics_subsystem_default(struct pok_graphics_subsystem* sys);
bool_t pok_graphics_subsystem_assign(struct pok_graphics_subsystem* sys,uint16_t dimension,uint16_t winCol,uint16_t winRow,
    uint16_t playerCol,uint16_t playerRow,uint16_t playerOffsetX,uint16_t playerOffsetY);
void pok_graphics_subsystem_assign_title(struct pok_graphics_subsystem* sys,const char* title);
enum pok_network_result pok_graphics_subsystem_netread(struct pok_graphics_subsystem* sys,struct pok_data_source* dsrc,
    struct pok_netobj_readinfo* info);
bool_t pok_graphics_subsystem_begin(struct pok_graphics_subsystem* sys);
bool_t pok_graphics_subsystem_create_textures(struct pok_graphics_subsystem* sys,int count, ...);
bool_t pok_graphics_subsystem_delete_textures(struct pok_graphics_subsystem* sys,int count, ...);
void pok_graphics_subsystem_game_render_state(struct pok_graphics_subsystem* sys,bool_t state);
void pok_graphics_subsystem_end(struct pok_graphics_subsystem* sys);
void pok_graphics_subsystem_register(struct pok_graphics_subsystem* sys,graphics_routine_t routine,void* context); /* thread-safe */
void pok_graphics_subsystem_unregister(struct pok_graphics_subsystem* sys,graphics_routine_t routine,void* context); /* thread-safe */
bool_t pok_graphics_subsystem_keyboard_query(struct pok_graphics_subsystem* sys,enum pok_input_key key,bool_t refresh); /* thread-safe */
bool_t pok_graphics_subsystem_is_running(struct pok_graphics_subsystem* sys);
bool_t pok_graphics_subsystem_has_window(struct pok_graphics_subsystem* sys);
void pok_graphics_subsystem_lock(struct pok_graphics_subsystem* sys);
void pok_graphics_subsystem_unlock(struct pok_graphics_subsystem* sys);

/* other graphics-related routines */
void pok_image_render(struct pok_image* img,int32_t x,int32_t y);

/* globals */
extern const union pixel BLACK_PIXEL;
extern const float BLACK_PIXEL_FLOAT[];

/* this macro adds a hook routine to a hook */
#define pok_graphics_subsystem_append_hook(hook,routine,context) \
    hook.routines[hook.top] = routine; \
    hook.contexts[hook.top++] = context

#endif
