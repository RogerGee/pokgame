/* map.h - pokgame */
#ifndef POKGAME_MAP_H
#define POKGAME_MAP_H
#include "netobj.h"
#include "tile.h"
#include "protocol.h"
#include <dstructs/treemap.h>

/* exceptions generated by this module */
enum pok_ex_map
{
    pok_ex_map_bad_chunk_size, /* chunk size was outside an acceptable range */
    pok_ex_map_zero_chunks, /* peer specified a zero amount of chunks */
    pok_ex_map_too_many_chunks, /* peer specified too many chunks at initial load time */
    pok_ex_map_already, /* map object was already loaded */
    pok_ex_map_not_loaded, /* map object was not loaded so the operation could not complete */
    pok_ex_map_bad_format, /* map information was not formatted correctly */
    pok_ex_map_non_unique_chunk /* a new chunk was created at an already specified location */
};

/* pok_map_chunk: a map chunk is a mxn 2d array of pok_tile structures; it
   forms the basic building block of any map; most trivial statically
   sized maps will consist of only a single map chunk; a map chunk is
   a dynamic network object */
struct pok_map_chunk
{
    struct pok_netobj _base;

    struct pok_tile** data; /* data stored in [row][column] order */
    struct pok_map_chunk* adjacent[4]; /* chunks adjacent to this chunk; index by pok_direction flag */

    uint8_t flags; /* enum pok_map_chunk_flags */
    bool_t discov; /* (reserved) */
};
enum pok_network_result pok_map_chunk_netmethod_send(struct pok_map_chunk* chunk,
    struct pok_data_source* dsrc,
    struct pok_netobj_writeinfo* winfo,
    struct pok_netobj_upinfo* uinfo);
enum pok_network_result pok_map_chunk_netmethod_recv(struct pok_map_chunk* chunk,
    struct pok_data_source* dsrc,
    struct pok_netobj_readinfo* info,
    enum pok_map_chunk_method method);

/* pok_map: a map is a grid of map chunks; each map chunk is sized the same; maps are
   linked together by their tile warp structures; this ultimately forms a graph-like
   structure; a map is a dynamic network object */
struct pok_map
{
    struct pok_netobj _base;
    uint32_t mapNo;

    /* map data */
    struct pok_map_chunk* origin; /* original chunk */
    struct pok_size chunkSize; /* dimensions of chunks */

    struct treemap loadedChunks; /* maps chunk position to chunk for fast lookup */
    struct pok_point originPos; /* position of original chunk */
    uint16_t flags; /* enum pok_map_flags */
};
struct pok_map* pok_map_new();
void pok_map_free(struct pok_map* map);
void pok_map_init(struct pok_map* map);
void pok_map_delete(struct pok_map* map);
bool_t pok_map_configure(struct pok_map* map,const struct pok_size* chunkSize,const uint16_t firstChunk[],uint32_t length);
struct pok_map_chunk* pok_map_add_chunk(struct pok_map* map,
    const struct pok_point* adjacency,
    enum pok_direction direction,
    const uint16_t chunkTiles[],
    uint32_t length);
bool_t pok_map_save(struct pok_map* map,struct pok_data_source* dsrc,bool_t complex);
bool_t pok_map_open(struct pok_map* map,struct pok_data_source* dsrc);
bool_t pok_map_load_simple(struct pok_map* map,const uint16_t tiledata[],uint32_t columns,uint32_t rows);
bool_t pok_map_fromfile_space(struct pok_map* map,const char* filename);
bool_t pok_map_fromfile_csv(struct pok_map* map,const char* filename);
struct pok_map_chunk* pok_map_get_chunk(const struct pok_map* map,const struct pok_point* pos);
enum pok_network_result pok_map_netwrite(struct pok_map* map,
    struct pok_data_source* dsrc,
    struct pok_netobj_writeinfo* info);
enum pok_network_result pok_map_netmethod_send(struct pok_map* map,
    struct pok_data_source* dsrc,
    struct pok_netobj_writeinfo* winfo,
    struct pok_netobj_upinfo* uinfo);
enum pok_network_result pok_map_netmethod_recv(struct pok_map* map,
    struct pok_data_source* dsrc,
    struct pok_netobj_readinfo* info,
    enum pok_map_method method);

/* pok_world: a world is a top-level collection of maps; a world is a dynamic network object
   that provides an interface for managing maps and map chunks; a world handles net-reading
   maps when they are sent initially */
struct pok_world
{
    struct pok_netobj _base;

    struct treemap loadedMaps;
};
struct pok_world* pok_world_new();
void pok_world_free();
void pok_world_init(struct pok_world* world);
void pok_world_delete(struct pok_world* world);
static inline bool_t pok_world_add_map(struct pok_world* world,struct pok_map* map)
{ return treemap_insert(&world->loadedMaps,map) == 0; }
struct pok_map* pok_world_get_map(struct pok_world* world,uint32_t mapNo);
bool_t pok_world_fromfile_warps(struct pok_world* world,const char* filename);
enum pok_network_result pok_world_netwrite(struct pok_world* world,
    struct pok_data_source* dsrc,
    struct pok_netobj_writeinfo* info);
enum pok_network_result pok_world_netmethod_send(struct pok_world* world,
    struct pok_data_source* dsrc,
    struct pok_netobj_writeinfo* winfo,
    struct pok_netobj_upinfo* uinfo);
enum pok_network_result pok_world_netmethod_recv(struct pok_world* world,
    struct pok_data_source* dsrc,
    struct pok_netobj_readinfo* info,
    enum pok_world_method method);

#endif
