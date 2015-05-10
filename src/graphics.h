/* graphics.h - pokgame */
#ifndef POKGAME_GRAPHICS_H
#define POKGAME_GRAPHICS_H
#include "net.h"
#include "image.h"

/* constants */
enum pok_graphics_constants
{
    MAX_GRAPHICS_ROUTINES = 32,
    MAX_WINDOW_PIXEL_WIDTH = 1280,
    MAX_WINDOW_PIXEL_HEIGHT = 768,
    DEFAULT_DIMENSION = 32,
    DEFAULT_WINDOW_WIDTH = 9,
    DEFAULT_WINDOW_HEIGHT = 10,
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
    pok_ex_graphics_bad_player_offset
};

/* define normal input keys */
enum pok_input_key
{
    pok_input_key_ABUTTON,
    pok_input_key_BBUTTON,
    pok_input_key_ENTER,
    pok_input_key_ALT,
    pok_input_key_UP,
    pok_input_key_DOWN,
    pok_input_key_LEFT,
    pok_input_key_RIGHT
};

/* graphics routine: every graphics routine is passed two parameters, the graphics
   subsystem and a context; the context generally is the object that stores the game's
   render state for a particular module (note: this is not a graphics library context; that
   can be obtained via the graphics subsystem) */
struct pok_graphics_subsystem;
typedef void (*graphics_routine_t)(struct pok_graphics_subsystem* sys,void* context);

/* define graphics subsystem object; this abstracts input/output to a graphical window frame */
struct _pok_graphics_subsystem_impl;
struct pok_graphics_subsystem
{
    /* init info: this information is set by default or sent by a game server */
    uint16_t dimension; /* tile and sprite dimension */
    struct pok_size windowSize; /* dimensionalized size of viewable window */
    struct pok_location playerLocation; /* dimensionalized window location of player sprite (must be within 'windowSize') */
    struct pok_location _playerLocationInv; /* (used by the implementation) */
    uint16_t playerOffsetX, playerOffsetY; /* pixel offset for player sprite (must be within 'dimension') */

    /* graphics routines called by the subsystem */
    uint16_t routinetop;
    graphics_routine_t routines[MAX_GRAPHICS_ROUTINES];
    void* contexts[MAX_GRAPHICS_ROUTINES];

    /* misc info used by rendering contexts */
    struct pok_image* blacktile; /* solid black tile image */

    /* implementation-specific information */
    struct _pok_graphics_subsystem_impl* impl;

    /* misc other */
    struct pok_string title; /* title bar text in graphical frame */
};
struct pok_graphics_subsystem* pok_graphics_subsystem_new();
void pok_graphics_subsystem_free(struct pok_graphics_subsystem* sys);
void pok_graphics_subsystem_init(struct pok_graphics_subsystem* sys);
void pok_graphics_subsystem_delete(struct pok_graphics_subsystem* sys);
void pok_graphics_subsystem_reset(struct pok_graphics_subsystem* sys);
bool_t pok_graphics_subsystem_default(struct pok_graphics_subsystem* sys);
enum pok_network_result pok_graphics_subsystem_netread(struct pok_graphics_subsystem* sys,struct pok_data_source* dsrc,
    struct pok_netobj_readinfo* info);
bool_t pok_graphics_subsystem_begin(struct pok_graphics_subsystem* sys);
void pok_graphics_subsystem_game_render_state(struct pok_graphics_subsystem* sys,bool_t state);
void pok_graphics_subsystem_end(struct pok_graphics_subsystem* sys);
void pok_graphics_subsystem_register(struct pok_graphics_subsystem* sys,graphics_routine_t routine,void* context);
void pok_graphics_subsystem_unregister(struct pok_graphics_subsystem* sys,graphics_routine_t routine);
bool_t pok_graphics_subsystem_keyboard_query(struct pok_graphics_subsystem* sys,enum pok_input_key key,bool_t refresh);

/* other graphics-related routines */
void pok_image_render(struct pok_image* img,uint32_t x,uint32_t y);

#endif
