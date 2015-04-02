/* map.h - pokgame */
#ifndef POKGAME_MAP_H
#define POKGAME_MAP_H
#include "tile.h" /* gets graphics.h */

/* constants */
enum pok_map_constants
{
    MAX_MAP_CHUNK_DIMENSION = 128
};

/* exceptions generated by this module */
enum pok_ex_map
{
    pok_ex_map_domain_error /* value was out of an acceptable range */
};

/* map chunk: a map chunk is a mxn 2d array of pok_tile structures; it
   forms the basic building block of any map; most trivial statically
   sized maps will consist of only a single map chunk; a map chunk is
   a dynamic network object */
struct pok_map_chunk
{
    struct pok_netobj _base;

    struct pok_tile** data;
    struct pok_map_chunk* adjacent[4]; /* chunks adjacent to this chunk; index by pok_direction flag */

    uint8_t counter;
    uint8_t flags; /* enum pok_map_chunk_flags */
};
enum pok_network_result pok_map_chunk_netupdate(struct pok_map_chunk* chunk,struct pok_data_source* dsrc,
    const struct pok_netobj_readinfo* info);

/* map: a map is a grid of map chunks; each map chunk is sized the same; maps are
   linked together by their tile warp structures; this ultimately forms a graph-like
   structure; a map is a static network object */
struct pok_map
{
    /* map data */
    struct pok_map_chunk* chunk; /* current chunk */
    struct pok_map_chunk* origin; /* original chunk */
    struct pok_size chunkSize; /* dimensions of chunks */
    struct pok_size mapSize; /* dimensions of maps, in units of chunks */
    struct pok_location pos; /* coordinate of top-left position in 'chunk' relative to global map position */

    uint16_t flags; /* enum pok_map_flags */
};
struct pok_map* pok_map_new();
void pok_map_free(struct pok_map* map);
void pok_map_init(struct pok_map* map);
void pok_map_delete(struct pok_map* map);
bool_t pok_map_save(struct pok_map* map,struct pok_data_source* dsrc);
bool_t pok_map_open(struct pok_map* map,struct pok_data_source* dsrc);
bool_t pok_map_load(struct pok_map* map,const struct pok_tile_data tiledata[],uint32_t columns,uint32_t rows);
enum pok_network_result pok_map_netread(struct pok_map* map,struct pok_data_source* dsrc,struct pok_netobj_readinfo* info);
bool_t pok_map_center_on(struct pok_map* map,const struct pok_location* location);
bool_t pok_map_check_for_chunks(struct pok_map* map,const struct pok_location* location);
void pok_map_render(struct pok_map* map,const struct pok_graphics_subsystem* sys,const struct pok_tile_manager* tman);

#endif
