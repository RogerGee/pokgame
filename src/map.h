/* map.h - pokgame */
#ifndef POKGAME_MAP_H
#define POKGAME_MAP_H
#include "net.h"
#include "tile.h"
#include "graphics.h"

/* exceptions generated by this module */
enum pok_ex_map
{
    pok_ex_map_bad_chunk_size /* chunk size was outside an acceptable range */
    
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

    uint8_t flags; /* enum pok_map_chunk_flags */
    bool_t discov; /* (reserved) */
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
    struct pok_location pos; /* coordinate position relative to global map position */

    uint16_t flags; /* enum pok_map_flags */
};
struct pok_map* pok_map_new();
void pok_map_free(struct pok_map* map);
void pok_map_init(struct pok_map* map);
void pok_map_delete(struct pok_map* map);
bool_t pok_map_save(struct pok_map* map,struct pok_data_source* dsrc,bool_t complex);
bool_t pok_map_open(struct pok_map* map,struct pok_data_source* dsrc);
bool_t pok_map_load(struct pok_map* map,const struct pok_tile_data tiledata[],uint32_t columns,uint32_t rows);
enum pok_network_result pok_map_netread(struct pok_map* map,struct pok_data_source* dsrc,struct pok_netobj_readinfo* info);
bool_t pok_map_check_for_chunks(struct pok_map* map,const struct pok_location* location);

/* notes about map rendering: a map is rendered ABOUT its position; the position starts at the player location
   specified in the graphics subsystem (the relative position of the player on the screen); thus a map is always
   centered where the player sprite is rendered; this also means a map never goes out of bounds, since the player
   cannot walk off the map; the render context will not draw areas of the map outside the map's edges, which means
   the clear fill color will show through; a map can be scrolled by changing the map render context's offset member;
   a map should first have its position updated, then it should be scrolled FROM to old position TO the new position */

/* pok_map_render_context: stores information useful for rendering a map and provides operations for changing a
   map's position; a map object should be managed through a context, not directly */
struct pok_map_render_context
{
    short focus[2]; /* chunk occupied by relpos */
    int offset[2]; /* offset by offset[0] x units, offset[1] y units (used for scrolling) */
    struct pok_map_chunk* viewingChunks[3][3]; /* chunks in the map that we care about */
    struct pok_location relpos; /* relative position within focus chunk (cached for efficiency) */
    struct pok_map* map; /* current map to draw */
    const struct pok_tile_manager* tman; /* tile collection to use */
    uint32_t aniTicks; /* tile animation counter */
};
struct pok_map_render_context* pok_map_render_context_new(struct pok_map* map,const struct pok_tile_manager* tman);
void pok_map_render_context_free(struct pok_map_render_context* context);
void pok_map_render_context_init(struct pok_map_render_context* context,struct pok_map* map,const struct pok_tile_manager* tman);
void pok_map_render_context_reset(struct pok_map_render_context* context,struct pok_map* newMap);
void pok_map_render_context_align(struct pok_map_render_context* context,bool_t computeRelPos);
bool_t pok_map_render_context_center_on(struct pok_map_render_context* map,const struct pok_location* location);
bool_t pok_map_render_context_update(struct pok_map_render_context* context,enum pok_direction dir);

/* render routine for maps */
void pok_map_render(const struct pok_graphics_subsystem* sys,struct pok_map_render_context* context);

#endif
