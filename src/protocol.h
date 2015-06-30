/*
  protocol.h - pokgame 
    This file is exposed as part of the pokgame version API; this file enumerates
    the many constant expressions required by the pokgame protocol; some version
    API calls may require parameters specified in this file; still, most of the
    functionality in this file is used by the implementation.

    Changing a constant or the order of enumerators will not change game behavior
    since the engine was compiled with the original constants/ordering. The version
    will only cause the engine to exhibit undefined behavior if it is compiled with
    an altered version of this file.
*/
#ifndef POKGAME_PROTOCOL_H
#define POKGAME_PROTOCOL_H
#include "types.h"

/* protocol sequence constants */
#define POKGAME_GREETING_SEQUENCE    "pokgame-greetings" /* greetings string */
#define POKGAME_BINARYMODE_SEQUENCE  "pokgame-binary"    /* indicates to use the binary protocol */
#define POKGAME_TEXTMODE_SEQUENCE    "pokgame-text"      /* indicates to use the text protocol */

/* protocol masks */
#define POKGAME_DEFAULT_GRAPHICS_MASK 0x01 /* mask for default settings bitmask sent during intermediate exchange */
#define POKGAME_DEFAULT_TILES_MASK    0x02
#define POKGAME_DEFAULT_SPRITES_MASK  0x04

/* enumerators for network objects properties: do not change
   the order of elements in these enumerations */

enum pok_sprite_manager_flag
{ /* sprite manager flags determine how animation frames are configured */
    pok_sprite_manager_no_alt = 0x00,          /* the sprite manager has no alternate animation frames */
    pok_sprite_manager_updown_alt = 0x01,    /* the sprite manager has an alternate up and down animation frame */
    pok_sprite_manager_leftright_alt = 0x02, /* the sprite manager has an alternate left and right animation frame */
};

enum pok_tile_warp_kind
{ /* enumerates the different kinds of warps performed by a tile */
    pok_tile_warp_none, /* tile does not warp */
    pok_tile_warp_instant, /* warp happens after player walks onto the tile */
    pok_tile_warp_cave_enter, /* same as warp_instant but with cave fadeout */
    pok_tile_warp_cave_exit, /* save as warp_instant but with cave fadein */
    pok_tile_warp_latent_up, /* warp happens after player attempts to move off in the specified direction */
    pok_tile_warp_latent_down, /* the game may draw an arrow indicating the direction */
    pok_tile_warp_latent_left,
    pok_tile_warp_latent_right,
    pok_tile_warp_latent_door_up, /* door variants prompt animation upon exit */
    pok_tile_warp_latent_door_down,
    pok_tile_warp_latent_door_left,
    pok_tile_warp_latent_door_right,
    pok_tile_warp_latent_cave_up, /* cave variants use different fadeout/in (needs to be last latent series) */
    pok_tile_warp_latent_cave_down,
    pok_tile_warp_latent_cave_left,
    pok_tile_warp_latent_cave_right,
    pok_tile_warp_spin, /* player "spins" away off the tile */
    pok_tile_warp_fall, /* player falls through the tile */
    pok_tile_warp_BOUND
};

enum pok_map_flag
{
    pok_map_flag_none = 0x00,
    pok_map_flag_dynamic = 0x01 /* make requests to obtain more chunks */
};

/* protocol method enumerators: represent the set of operations on network objects that can
   be performed during a 'netmethod_*' call; each method expects zero or more arguments */

enum pok_map_chunk_method
{
    pok_map_chunk_update_tile,   /* a tile was updated at a specified location */
    pok_map_chunk_update_region  /* a specified region was updated with a constant tile id */
};

enum pok_map_method
{
    pok_map_method_add_chunk,   /* add a specified map chunk to the map with specified chunk position */
    pok_map_method_remove_chunk /* remove specified chunk from map */
};

enum pok_world_method
{
    pok_world_method_add_map   /* add a new map to the world */
};

/* protocol method structures: these represent the parameters passed to a 'netmethod_send' call; the pokgame
   version API allows the user to employ these structures, though other wrappers are available */

union pok_map_chunk_method_params
{
    struct {
        uint16_t tileID; /* replacement tile id */
        uint16_t column; /* column of desired tile */
        uint16_t row;    /* row of desired tile */
    } update_tile;
    struct {
        uint16_t tileID; /* replacement fill tile id */
        uint16_t top;    /* describes the rectangle to fill */
        uint16_t bottom;
        uint16_t left;
        uint16_t right;
    } update_region;
};

union pok_map_method_params
{
    struct pok_location add_chunk;   /* position of chunk to add or remove */
    struct pok_location remove_chunk;
};

/* protocol limits */

enum pok_limit
{
    pok_max_tile_images = 1024,
    pok_max_dimension = 128,
    pok_min_dimension = 8,
    pok_max_image_size = 5242880,
    pok_max_map_chunk_dimension = 128,
    pok_min_map_chunk_dimension = 16,
    pok_max_initial_chunks = 255
};

#endif
