/* tile.h - pokgame */
#ifndef POKGAME_TILE_H
#define POKGAME_TILE_H
#include "netobj.h"

/* exceptions generated by this module */
enum pok_ex_tile
{
    pok_ex_tile_bad_warp_kind /* bad warp kind was specified */
};

/* static tile representation */
struct pok_tile_data
{
    uint16_t tileid;                  /* tile image index */
    uint32_t warpMap;                 /* map number of warp destination */
    struct pok_point warpChunk;       /* chunk position of warp destination */
    struct pok_location warpLocation; /* location in chunk of warp destination */
    uint8_t warpKind;                 /* what kind of warp (enum pok_tile_warp_kind) */
};

/* tile structure; used by maps to represent the grid of spaces that make up
   the basic structure of the gameworld; this tile structure stores non-static
   information about the tile */
struct pok_tile
{
    struct pok_tile_data data;

    /* extra information */
    bool_t impass;           /* if non-zero, then otherwise passable tile is impassable */
    bool_t pass;             /* if non-zero, then otherwise impassable tile is passable */
};
void pok_tile_init(struct pok_tile* tile,uint16_t tileid);
void pok_tile_init_ex(struct pok_tile* tile,const struct pok_tile_data* tiledata);
bool_t pok_tile_save(struct pok_tile* tile,struct pok_data_source* dsrc);
bool_t pok_tile_open(struct pok_tile* tile,struct pok_data_source* dsrc);
enum pok_network_result pok_tile_netread(struct pok_tile* tile,struct pok_data_source* dsrc,
    struct pok_netobj_readinfo* info);
enum pok_network_result pok_tile_netwrite(struct pok_tile* tile,struct pok_data_source* dsrc,
    struct pok_netobj_writeinfo* info);

extern const struct pok_tile DEFAULT_TILE;

#endif
