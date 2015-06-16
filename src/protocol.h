/* protocol.h - pokgame 
    this file is exposed as part of the pokgame API */
#ifndef POKGAME_PROTOCOL_H
#define POKGAME_PROTOCOL_H

/* enumerators for network objects properties: do not change
   the order of elements in these enumerations */

enum pok_sprite_manager_flags
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
    pok_tile_warp_latent_cave_up, /* cave variants use different fadeout/in */
    pok_tile_warp_latent_cave_down,
    pok_tile_warp_latent_cave_left,
    pok_tile_warp_latent_cave_right,
    pok_tile_warp_spin, /* player "spins" away off the tile */
    pok_tile_warp_fall, /* player falls through the tile */
    pok_tile_warp_BOUND
};

enum pok_map_flags
{
    pok_map_flag_none = 0x00,
    pok_map_flag_dynamic = 0x01 /* make requests to obtain more chunks */
};

/* protocol method enumerators: represent the set of operations on network objects that can
   be performed during a 'netupdate'; each method expects zero or more arguments */

enum pok_map_chunk_methods
{
    pok_map_chunk_update_tile,   /* a tile was updated at a specified location */
    pok_map_chunk_update_region  /* a specified region was updated with a constant tile id */
};

enum pok_map_methods
{
    pok_map_method_add_chunk,   /* add a specified map chunk to the map with specified chunk position */
    pok_map_method_remove_chunk /* remove specified chunk from map */
};

/* protocol limits */

enum pok_limits
{
    pok_max_tile_images = 1024,
    pok_max_dimension = 128,
    pok_min_dimension = 8,
    pok_max_image_size = 5242880,
    pok_max_map_chunk_dimension = 128,
    pok_max_initial_chunks = 255
};

#endif
