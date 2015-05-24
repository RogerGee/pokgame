/* protocol.h - pokgame 
    this file is exposed as part of the pokgame API */
#ifndef POKGAME_PROTOCOL_H
#define POKGAME_PROTOCOL_H

/* protocol flags/enumerators for network objects */

enum pok_tile_warp_kind
{ /* enumerates the different kinds of warps performed by a tile */
    pok_tile_warp_none, /* tile does not warp */
    pok_tile_warp_instant, /* warp happens after player walks onto the tile */
    pok_tile_warp_latent_up, /* warp happens after player attempts to move off in the specified direction */
    pok_tile_warp_latent_down,
    pok_tile_warp_latent_left,
    pok_tile_warp_latent_right,
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
    MAX_TILE_IMAGES = 1024,

    MAX_DIMENSION = 128,
    MIN_DIMENSION = 8,
    MAX_IMAGE_SIZE = 5242880,
    MAX_MAP_CHUNK_DIMENSION = 128,
    MAX_INITIAL_CHUNKS = 255
};

#endif
