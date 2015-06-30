/* map-context.h - pokgame */
#ifndef POKGAME_MAP_CONTEXT_H
#define POKGAME_MAP_CONTEXT_H
#include "map.h"
#include "tileman.h"
#include "graphics.h"

/* Notes about map rendering: a map is rendered ABOUT its position; the position starts at the player location
   specified in the graphics subsystem (the relative position of the player on the screen); thus a map is always
   centered where the player sprite is rendered; this also means a map never goes out of bounds, since the player
   cannot walk off the map; the render context will not draw areas of the map outside the map's edges, which means
   the clear fill color will show through; a map can be scrolled by changing the map render context's offset member;
   a map should first have its position updated, then it should be scrolled FROM to old position TO the new position */

/* pok_chunk_render_info: defines the application of a map chunk to the screen */
struct pok_chunk_render_info
{
    int32_t px, py;              /* screen location */
    uint16_t across, down;       /* number of visible columns and rows */
    struct pok_location loc;     /* location within chunk data to render */
    struct pok_point chunkPos;   /* chunk position relative to origin */
    struct pok_map_chunk* chunk; /* the chunk specified; NULL if unused */
};

/* pok_map_render_context: stores information useful for rendering a map and provides operations for changing a
   map's position; a map object should be managed through a context, not directly */
struct pok_map_render_context
{
    short focus[2];                            /* chunk occupied by relpos */
    int offset[2];                             /* offset by offset[0] x units, offset[1] y units (used for scrolling) */
    struct pok_map_chunk* viewingChunks[3][3]; /* chunks in the map that we care about */
    struct pok_location relpos;                /* relative position within current chunk */
    struct pok_point chunkpos;                 /* position of current chunk relative to map->origin */
    struct pok_map_chunk* chunk;               /* current chunk */
    struct pok_map* map;                       /* current map to draw */
    const struct pok_tile_manager* tman;       /* tile collection to use */
    struct pok_chunk_render_info info[4];      /* chunk render info for implementation */
    uint16_t granularity;                      /* controls map scroll granularity (how many updates per scroll cycle) */
    uint32_t tileAniTicks;                     /* tile animation counter */
    uint32_t scrollTicks;                      /* scroll animation tick counter */
    uint32_t grooveTicks;                      /* groove time tick counter */
    uint32_t scrollTicksAmt;                   /* number of ticks before scroll cycle */
    bool_t groove;                             /* true after a context has finished updating and for a period afterwards */
    bool_t changed;                            /* true if the map render context location has been changed */
    bool_t update;                             /* is the map render context being updated? */
};
struct pok_map_render_context* pok_map_render_context_new(const struct pok_tile_manager* tman);
void pok_map_render_context_free(struct pok_map_render_context* context);
void pok_map_render_context_init(struct pok_map_render_context* context,const struct pok_tile_manager* tman);
void pok_map_render_context_set_map(struct pok_map_render_context* context,struct pok_map* map);
void pok_map_render_context_align(struct pok_map_render_context* context);
bool_t pok_map_render_context_center_on(struct pok_map_render_context* context,
    const struct pok_point* chunkpos,
    const struct pok_location* relpos);
bool_t pok_map_render_context_set_position(struct pok_map_render_context* context,
    struct pok_map* map,
    const struct pok_point* chunkpos,
    const struct pok_location* relpos);
bool_t pok_map_render_context_move(struct pok_map_render_context* context,enum pok_direction dir,uint16_t skipTiles,bool_t checkPassable);
void pok_map_render_context_set_update(struct pok_map_render_context* context,enum pok_direction dir,uint16_t dimension);
bool_t pok_map_render_context_update(struct pok_map_render_context* context,uint16_t dimension,uint32_t ticks);
struct pok_tile* pok_map_render_context_get_adjacent_tile(struct pok_map_render_context* context,int x,int y);

/* render routine for maps */
void pok_map_render(const struct pok_graphics_subsystem* sys,struct pok_map_render_context* context);

#endif
