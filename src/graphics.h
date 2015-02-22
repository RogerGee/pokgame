/* graphics.h - pokgame */
#ifndef POKGAME_GRAPHICS_H
#define POKGAME_GRAPHICS_H
#include "image.h" /* gets net.h */

/* constants */
enum pok_graphics_constants
{
    MAX_GRAPHICS_ROUTINES = 32,
    DEFAULT_DIMENSION = 32,
    DEFAULT_WINDOW_WIDTH = 9,
    DEFAULT_WINDOW_HEIGHT = 10,
    DEFAULT_PLAYER_LOCATION_X = 4,
    DEFAULT_PLAYER_LOCATION_Y = 4,
    DEFAULT_PLAYER_OFFSET_X = 0,
    DEFAULT_PLAYER_OFFSET_Y = -8
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

/* define size by number of columns and rows */
struct pok_size
{
    uint16_t columns; /* width */
    uint16_t rows; /* height */
};

/* define location by column and row position:
    columns are numbered from [0..n] left-to-right; rows
    are numbered from [0..n] from top-to-bottom */
struct pok_location
{
    uint16_t column; /* x */
    uint16_t row; /* y */
};

/* graphics routine */
struct pok_graphics_subsystem;
typedef void (*graphics_routine_t)(struct pok_graphics_subsystem* sys);

/* define graphics subsystem object; this abstracts input/output to a graphical window frame */
struct _pok_graphics_subsystem_impl;
struct pok_graphics_subsystem
{
    /* init info: this information is set by default or sent by a game server */
    uint16_t dimension; /* tile and sprite dimension */
    struct pok_size windowSize; /* size of viewable window */
    struct pok_location playerLocation; /* location of player sprite */
    uint16_t playerOffsetX, playerOffsetY; /* pixel offset for player sprite */

    /* graphics routines called by the subsystem */
    uint16_t routinetop;
    graphics_routine_t routines[MAX_GRAPHICS_ROUTINES];

    /* implementation-specific information */
    struct _pok_graphics_subsystem_impl* impl;
};
struct pok_graphics_subsystem* pok_graphics_subsystem_new();
void pok_graphics_subsystem_free(struct pok_graphics_subsystem* sys);
void pok_graphics_subsystem_init(struct pok_graphics_subsystem* sys);
void pok_graphics_subsystem_delete(struct pok_graphics_subsystem* sys);
void pok_graphics_subsystem_default(struct pok_graphics_subsystem* sys);
enum pok_network_result pok_graphics_subsystem_netread(struct pok_graphics_subsystem* sys,struct pok_data_source* dsrc,
    struct pok_netobj_info* info);
bool_t pok_graphics_subsystem_begin(struct pok_graphics_subsystem* sys);
void pok_graphics_subsystem_end(struct pok_graphics_subsystem* sys);
void pok_graphics_subsystem_register(struct pok_graphics_subsystem* sys,graphics_routine_t routine);
void pok_graphics_subsystem_unregister(struct pok_graphics_subsystem* sys,graphics_routine_t routine);
bool_t pok_graphics_subsystem_keyboard_query(struct pok_graphics_subsystem* sys,enum pok_input_key key,bool_t refresh);

#endif
