/* map-render.c - pokgame */
#include "map-render.h"
#include "protocol.h"
#include <stdlib.h>

/* include the map rendering function; define an interface that it can use to compute map render info */
struct chunk_render_info
{
    uint32_t px, py;
    uint16_t across, down;
    struct pok_location loc;
    struct pok_map_chunk* chunk;
};
void compute_chunk_render_info(struct chunk_render_info* chunks, /* this is 'extern' for debugging */
    const struct pok_graphics_subsystem* sys,struct pok_map_render_context* context);

/* include platform-dependent code */
#if defined(POKGAME_OPENGL)
#include "map-render-gl.c"
#endif

/* pok_map_render_context */
struct pok_map_render_context* pok_map_render_context_new(struct pok_map* map,const struct pok_tile_manager* tman)
{
    struct pok_map_render_context* context;
    context = malloc(sizeof(struct pok_map_render_context));
    pok_map_render_context_init(context,map,tman);
    return context;
}
void pok_map_render_context_free(struct pok_map_render_context* context)
{
    free(context);
}
void pok_map_render_context_init(struct pok_map_render_context* context,struct pok_map* map,const struct pok_tile_manager* tman)
{
    int i, j;
    context->focus[0] = context->focus[1] = 0;
    context->offset[0] = context->offset[1] = 0;
    for (i = 0;i < 3;++i)
        for (j = 0;j < 3;++j)
            context->viewingChunks[i][j] = NULL;
    context->relpos.column = context->relpos.row = 0;
    context->chunkpos.X = map->originPos.X;
    context->chunkpos.Y = map->originPos.Y;
    map->chunk = map->origin;
    context->map = map;
    context->tman = tman;
    context->aniTicks = 0;
}
void pok_map_render_context_reset(struct pok_map_render_context* context,struct pok_map* newMap)
{
    int i, j;
    context->focus[0] = context->focus[1] = 0;
    context->offset[0] = context->offset[1] = 0;
    for (i = 0;i < 3;++i)
        for (j = 0;j < 3;++j)
            context->viewingChunks[i][j] = NULL;
    context->relpos.column = context->relpos.row = 0;
    context->chunkpos.X = newMap->originPos.X;
    context->chunkpos.Y = newMap->originPos.Y;
    newMap->chunk = newMap->origin;
    context->map = newMap;
    context->aniTicks = 0;
}
void pok_map_render_context_align(struct pok_map_render_context* context)
{
    /* compute surrounding chunks; place the current chunk in the center */
    context->focus[0] = context->focus[1] = 1;
    context->viewingChunks[1][1] = context->map->chunk;
    context->viewingChunks[1][0] = context->map->chunk->adjacent[pok_direction_up];
    context->viewingChunks[1][2] = context->map->chunk->adjacent[pok_direction_down];
    context->viewingChunks[0][1] = context->map->chunk->adjacent[pok_direction_left];
    context->viewingChunks[2][1] = context->map->chunk->adjacent[pok_direction_right];
    context->viewingChunks[0][0] = context->viewingChunks[1][0] != NULL ? context->viewingChunks[1][0]->adjacent[pok_direction_left] : NULL;
    context->viewingChunks[2][0] = context->viewingChunks[1][0] != NULL ? context->viewingChunks[1][0]->adjacent[pok_direction_right] : NULL;
    context->viewingChunks[0][2] = context->viewingChunks[1][2] != NULL ? context->viewingChunks[1][2]->adjacent[pok_direction_left] : NULL;
    context->viewingChunks[2][2] = context->viewingChunks[1][2] != NULL ? context->viewingChunks[1][2]->adjacent[pok_direction_right] : NULL;
}
/* bool_t pok_map_render_context_center_on(struct pok_map_render_context* context,const struct pok_location* location) */
/* { */
/*     int d[2]; */
/*     enum pok_direction dirs[2]; */
/*     struct pok_location pos; */
/*     struct pok_map_chunk* chunk; */
/*     /\* find the chunk that bounds 'location'; it may not exist, in which case the map's */
/*        current chunk remains unchanged and the function returns FALSE *\/ */
/*     pos = context->map->pos; */
/*     chunk = context->map->chunk; */
/*     /\* find direction that needs to be moved in each dimension *\/ */
/*     d[0] = pos.column > location->column */
/*         ? (dirs[0] = pok_direction_left, -context->map->chunkSize.columns) : (dirs[0] = pok_direction_right, context->map->chunkSize.columns);*/
/*     d[1] = pos.row > location->row */
/*         ? (dirs[1] = pok_direction_up, -context->map->chunkSize.rows) : (dirs[1] = pok_direction_down, context->map->chunkSize.rows); */
/*     /\* locate the chunk that contains the desired location; note that the map can be irregular (e.g. not rectangular) *\/ */
/*     do { */
/*         bool_t fA, fB; */
/*         struct pok_map_chunk* A, *B; */
/*         fA = FALSE; A = NULL; */
/*         fB = FALSE; B = NULL; */
/*         /\* advance along the width of the map if needed *\/ */
/*         if (pok_unsigned_diff(location->column,pos.column) >= context->map->chunkSize.columns) { */
/*             pos.column += d[0]; */
/*             A = chunk->adjacent[dirs[0]]; */
/*             fA = TRUE; */
/*         } */
/*         /\* advance along the height of the map if needed *\/ */
/*         if (pok_unsigned_diff(location->row,pos.row) >= context->map->chunkSize.rows) { */
/*             pos.row += d[1]; */
/*             B = chunk->adjacent[dirs[1]]; */
/*             fB = TRUE; */
/*         } */
/*         if (!fA && !fB) */
/*             break; /\* we found it! *\/ */
/*         if ((fA && A == NULL) || (fB && B == NULL)) */
/*             return FALSE; /\* we pushed outside the map *\/ */
/*         if (A != NULL && B != NULL) /\* choose the diagonal (column and row were not yet sufficient) *\/ */
/*             chunk = A->adjacent[dirs[1]]; /\* equivilent to B->adjacent[dirs[0]] (they are orthogonal) *\/ */
/*         else if (A != NULL) */
/*             chunk = A; */
/*         else */
/*             chunk = B; */
/*     } while (chunk != NULL); */
/*     if (chunk == NULL) */
/*         return FALSE; */
/*     context->map->pos = *location; */
/*     context->map->chunk = chunk; */
/*     /\* align the context on the new position *\/ */
/*     pok_map_render_context_align(context,TRUE); */
/*     return TRUE; */
/* } */
bool_t pok_map_render_context_center_on(struct pok_map_render_context* context,const struct pok_point* chunkpos,const struct pok_location* relpos)
{
    /* center the context on the specified chunk; if the chunk does not exist, then FALSE
       is returned and the context is not updated */
    int32_t x, y;
    int d1, d2, i1, i2;
    struct pok_map_chunk* chunk = context->map->chunk;
    x = chunkpos->X - context->chunkpos.X;
    y = chunkpos->Y - context->chunkpos.Y;
    if (x < 0) {
        d1 = pok_direction_left;
        i1 = 1;
    }
    else if (x > 0) {
        d1 = pok_direction_right;
        i1 = -1;
    }
    if (y < 0) {
        d2 = pok_direction_up;
        i2 = 1;
    }
    else if (y > 0) {
        d2 = pok_direction_down;
        i2 = -1;
    }
    do {
        if (x != 0 && chunk->adjacent[d1] != NULL) {
            chunk = chunk->adjacent[d1];
            x += i1;
        }
        else if (y != 0 && chunk->adjacent[d2] != NULL) {
            chunk = chunk->adjacent[d2];
            y += i2;
        }
        else
            return FALSE;
    } while (x != 0 || y != 0);
    /* operation was successful; realign the context and set the relative position */
    context->map->chunk = chunk;
    pok_map_render_context_align(context);
    context->relpos = *relpos;
    context->chunkpos = *chunkpos;
    return TRUE;
}
bool_t pok_map_render_context_update(struct pok_map_render_context* context,enum pok_direction dir)
{
    /* change the map position by 1 position unit in the specified direction; if this is not possible,
       then leave the map (and the render context) unchanged and return FALSE; otherwise update the relative
       position; if we leave the current chunk then update the context; if we walk into an area close to the
       edge of the 3x3 chunk grid, then re-align the grid around the edge chunk; note that maps can have an
       irregular size, so for the south and east directions the absence of a chunk bounds the map */
    if (dir == pok_direction_up) {
        if (context->relpos.row > 0) {
            --context->relpos.row;
            if (context->focus[1] == 0 && context->relpos.row <= context->map->chunkSize.rows/2)
                pok_map_render_context_align(context);
        }
        /* focus[1] will always be in range (focus[1] > 0 before the operation) */
        else if (context->viewingChunks[context->focus[0]][context->focus[1]-1] == NULL)
            return FALSE; /* there is no chunk to the north */
        else {
            /* focus on the new chunk */
            context->relpos.row = context->map->chunkSize.rows-1;
            --context->chunkpos.Y;
            --context->focus[1];
            context->map->chunk = context->map->chunk->adjacent[dir];
        }
    }
    else if (dir == pok_direction_down) {
        if (context->relpos.row < context->map->chunkSize.rows-1) {
            ++context->relpos.row;
            if (context->focus[1] == 2 && context->relpos.row >= context->map->chunkSize.rows/2)
                pok_map_render_context_align(context);
        }
        /* focus[1] will always be in range (focus[1] <= 1 before the operation) */
        else if (context->viewingChunks[context->focus[0]][context->focus[1]+1] == NULL)
            return FALSE; /* there is not a chunk to the south */
        else {
            /* focus on the new chunk */
            context->relpos.row = 0;
            ++context->chunkpos.Y;
            ++context->focus[1];
            context->map->chunk = context->map->chunk->adjacent[dir];
        }
    }
    else if (dir == pok_direction_left) {
        if (context->relpos.column > 0) {
            --context->relpos.column;
            if (context->focus[0] == 0 && context->relpos.column <= context->map->chunkSize.columns/2)
                pok_map_render_context_align(context);
        }
        /* focus[0] will always be in range (focus[0] >= 1 before the operation) */
        else if (context->viewingChunks[context->focus[0]-1][context->focus[1]] == NULL)
            return FALSE; /* there is no chunk to the west */
        else {
            /* focus on the new chunk */
            context->relpos.column = context->map->chunkSize.columns-1;
            --context->chunkpos.X;
            --context->focus[0];
            context->map->chunk = context->map->chunk->adjacent[dir];
        }
    }
    else if (dir == pok_direction_right) {
        if (context->relpos.column < context->map->chunkSize.columns-1) {
            ++context->relpos.column;
            if (context->focus[0] == 2 && context->relpos.column >= context->map->chunkSize.columns/2)
                pok_map_render_context_align(context);
        }
        /* focus[0] will always be in range (focus[0] <= 1 before the operation) */
        else if (context->viewingChunks[context->focus[0]+1][context->focus[1]] == NULL)
            return FALSE; /* there is no chunk to the east */
        else {
            /* focus on the new chunk */
            context->relpos.column = 0;
            ++context->chunkpos.X;
            ++context->focus[0];
            context->map->chunk = context->map->chunk->adjacent[dir];
        }
    }
    else
        return FALSE;
    return TRUE;
}

/* implementation of interface for map rendering routine */
void compute_chunk_render_info(struct chunk_render_info* info,
    const struct pok_graphics_subsystem* sys,struct pok_map_render_context* context)
{
    /* a map is painted as an application of at most 4 chunks; this routine computes which chunks are needed along with
       the bounding information specifying how each chunk is to be drawn; this routine corrects for small chunk dimensions
       (but the dimensions should be correct for any multi-chunk map, otherwise undefined behavior will result) */
    int i, d[2];
    uint16_t u, v;
    /* chunks: chunk1 is always the horizontally adjacent chunk (if any) and
       chunk2 is always the vertically adjacent chunk (if any) */
    for (i = 0;i < 4;++i)
        info[i].chunk = NULL;
    info[0].px = info[0].py = 0;
    info[0].across = sys->windowSize.columns;
    info[0].down = sys->windowSize.rows;
    info[0].chunk = context->map->chunk;
    i = (int)context->relpos.column - (int)sys->playerLocation.column;
    d[0] = context->focus[0]; d[1] = context->focus[1];
    if (i < 0) {
        /* viewing area exceeds chunk bounds on the left */
        u = -i; /* number of columns to the left */
        info[0].px += sys->dimension * u;
        info[0].across -= u;
        info[0].loc.column = 0;
        /* set dimensions for adjacent chunk; 'py', 'down' and 'loc.row' members to be set later */
        info[1].px = 0;
        info[1].across = u;
        info[1].loc.column = context->map->chunkSize.columns - u;
        info[1].chunk = context->viewingChunks[context->focus[0]-1][context->focus[1]]; /* immediate left */
        --d[0];
    }
    else {
        info[0].loc.column = i;
        u = context->map->chunkSize.columns - context->relpos.column - 1; /* how many columns to the right in chunk? */
        if (u < sys->_playerLocationInv.column) {
            /* viewing area exceeds chunk bounds on the right
               (note: viewing area can only exceed on the right given it did not exceed on the left) */
            v = sys->_playerLocationInv.column - u; /* how many columns to the right in viewing area? */
            info[0].across -= v;
            /* set dimensions for adjacent chunk; 'py', 'down' and 'loc.row' members to be set later */
            info[1].px = sys->dimension * info[0].across;
            info[1].across = v;
            info[1].loc.column = 0;
            info[1].chunk = context->viewingChunks[context->focus[0]+1][context->focus[1]]; /* immediate right */
            ++d[0];
        }
    }
    i = (int)context->relpos.row - (int)sys->playerLocation.row;
    if (i < 0) {
        /* viewing area exceeds chunk bounds above */
        u = -i; /* number of rows above */
        info[0].py += sys->dimension * u;
        info[0].down -= u;
        info[0].loc.row = 0;
        /* set dimensions for adjacent chunk */
        info[2].py = 0;
        info[2].down = u;        
        info[2].loc.row = context->map->chunkSize.rows - u;
        info[2].chunk = context->viewingChunks[context->focus[0]][context->focus[1]-1]; /* straight up */
        --d[1];
    }
    else {
        info[0].loc.row = i;
        u = context->map->chunkSize.rows - context->relpos.row - 1; /* how many rows below in chunk? */
        if (u < sys->_playerLocationInv.row) {
            /* viewing area exceeds chunk bounds below
               (note: viewing area can only exceed below given it did not exceed above) */
            v = sys->_playerLocationInv.row - u; /* how many rows below in viewing area? */
            info[0].down -= v;
            /* set dimensions for adjacent chunk */
            info[2].py = sys->dimension * info[0].down;
            info[2].down = v;
            info[2].loc.row = 0;
            info[2].chunk = context->viewingChunks[context->focus[0]][context->focus[1]+1]; /* straight down */
            ++d[1];
        }
    }
    if (info[1].chunk != NULL) {
        /* chunk 1 is identical to chunk 0 vertically */
        info[1].py = info[0].py;
        info[1].down = info[0].down;
        info[1].loc.row = info[0].loc.row;
    }
    if (info[2].chunk != NULL) {
        /* chunk 2 is identical to chunk 0 horizontally */
        info[2].px = info[0].px;
        info[2].across = info[0].across;
        info[2].loc.column = info[0].loc.column;
    }
    if (info[1].chunk!=NULL && info[2].chunk!=NULL) {
        /* we need to pull from a diagonal chunk; chunk 3 will be vertically identical
           to chunk 1 and horizontally identical to chunk 2 */
        info[3].px = info[1].px;
        info[3].across = info[1].across;
        info[3].loc.column = info[1].loc.column;
        info[3].py = info[2].py;
        info[3].down = info[2].down;
        info[3].loc.row = info[2].loc.row;
        info[3].chunk = context->viewingChunks[d[0]][d[1]];
    }
    /* correct chunk sizes */
    for (i = 0;i < 4;++i) {
        if (info[i].chunk != NULL) {
            if (info[i].across > context->map->chunkSize.columns)
                info[i].across = context->map->chunkSize.columns;
            if (info[i].down > context->map->chunkSize.rows)
                info[i].down = context->map->chunkSize.rows;
        }
    }
}
