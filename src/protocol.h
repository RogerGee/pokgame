/* protocol.h - pokgame 
    this file is exposed as part of the pokgame API */
#ifndef POKGAME_PROTOCOL_H
#define POKGAME_PROTOCOL_H

/* protocol flags/enumerators for network objects: do not change
   the order of elements in these enumerations */

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

enum pok_map_chunk_flags
{
    pok_map_chunk_flag_none = 0x00,
    pok_map_chunk_flag_byref = 0x01
};

enum pok_map_flags
{
    pok_map_flag_none = 0x00,
    pok_map_flag_dynamic = 0x01 /* make requests to obtain more chunks */
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
