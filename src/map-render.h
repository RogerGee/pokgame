/* map-render.h - pokgame */
#ifndef POKGAME_MAP_RENDER_H
#define POKGAME_MAP_RENDER_H
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
    struct pok_map_chunk* chunk; /* the chunk specified; NULL if unused */
};

/* pok_map_render_context: stores information useful for rendering a map and provides operations for changing a
   map's position; a map object should be managed through a context, not directly */
struct pok_map_render_context
{
    short focus[2];                            /* chunk occupied by relpos */
    int offset[2];                             /* offset by offset[0] x units, offset[1] y units (used for scrolling) */
    struct pok_map_chunk* viewingChunks[3][3]; /* chunks in the map that we care about */
    struct pok_location relpos;                /* relative position within focus chunk (cached for efficiency) */
    struct pok_point chunkpos;                 /* position of chunk relative to origin */
    struct pok_map* map;                       /* current map to draw */
    const struct pok_tile_manager* tman;       /* tile collection to use */
    uint32_t aniTicks;                         /* tile animation counter */
    struct pok_chunk_render_info info[4];      /* chunk render info for implementation */
};
struct pok_map_render_context* pok_map_render_context_new(const struct pok_tile_manager* tman);
void pok_map_render_context_free(struct pok_map_render_context* context);
void pok_map_render_context_init(struct pok_map_render_context* context,const struct pok_tile_manager* tman);
void pok_map_render_context_set_map(struct pok_map_render_context* context,struct pok_map* map);
void pok_map_render_context_align(struct pok_map_render_context* context);
bool_t pok_map_render_context_center_on(struct pok_map_render_context* map,const struct pok_point* chunkpos,const struct pok_location* relpos);
bool_t pok_map_render_context_update(struct pok_map_render_context* context,enum pok_direction dir);

/* render routine for maps */
void pok_map_render(const struct pok_graphics_subsystem* sys,struct pok_map_render_context* context);

#endif
