/* map-context.c - pokgame */
#include "map-context.h"
#include "protocol.h"
#include <stdlib.h>

/* this function computes chunk render information for the map rendering routine; this is 'extern' for debugging */
void compute_chunk_render_info(struct pok_map_render_context* context, const struct pok_graphics_subsystem* sys);

/* pok_map_render_context */
struct pok_map_render_context* pok_map_render_context_new(const struct pok_tile_manager* tman)
{
    struct pok_map_render_context* context;
    context = malloc(sizeof(struct pok_map_render_context));
    pok_map_render_context_init(context,tman);
    return context;
}
void pok_map_render_context_free(struct pok_map_render_context* context)
{
    free(context);
}
void pok_map_render_context_init(struct pok_map_render_context* context,const struct pok_tile_manager* tman)
{
    int i, j;
    context->focus[0] = context->focus[1] = 0;
    context->offset[0] = context->offset[1] = 0;
    for (i = 0;i < 3;++i)
        for (j = 0;j < 3;++j)
            context->viewingChunks[i][j] = NULL;
    context->relpos.column = context->relpos.row = 0;
    context->chunkpos.X = 0;
    context->chunkpos.Y = 0;
    context->map = NULL;
    context->tman = tman;
    for (i = 0;i < 4;++i)
        context->info[i].chunk = NULL;
    context->granularity = 1;
    context->tileAniTicks = 0;
    context->scrollTicks = 0;
    context->scrollTicksAmt = 1;
    context->groove = FALSE;
    context->changed = FALSE;
    context->update = FALSE;
}
void pok_map_render_context_set_map(struct pok_map_render_context* context,struct pok_map* map)
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
    context->changed = TRUE;
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
    context->changed = TRUE;
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
    if (x != 0 || y != 0) {
        /* walk along the chunk grid to get the correct chunk */
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
                /* the specified chunk did not exist */
                return FALSE;
        } while (x != 0 || y != 0);
        /* operation was successful */
        context->map->chunk = chunk;
        context->chunkpos = *chunkpos;
    }
    /* realign the context and set the relative position */
    pok_map_render_context_align(context); /* flags change */
    context->relpos = *relpos;
    return TRUE;
}
static bool_t is_impassable(const struct pok_tile_manager* tman,struct pok_map_chunk* chunk,uint16_t column,uint16_t row)
{
    if (chunk->data[row][column].data.tileid <= tman->impassability) {
        if ( !chunk->data[row][column].pass )
            return TRUE;
    }
    else if ( chunk->data[row][column].impass )
        return TRUE;
    return FALSE;
}
bool_t pok_map_render_context_move(struct pok_map_render_context* context,enum pok_direction dir,bool_t checkPassable)
{
    /* change the map position by 1 position unit in the specified direction; if this is not possible,
       then leave the map (and the render context) unchanged and return FALSE; otherwise update the relative
       position; if we leave the current chunk then update the context; if we walk into an area close to the
       edge of the 3x3 chunk grid, then re-align the grid around the edge chunk; note that maps can have an
       irregular size, so for the south and east directions the absence of a chunk bounds the map */
    uint16_t next;
    if (dir == pok_direction_up) {
        if (context->relpos.row > 0) {
            next = context->relpos.row - 1;
            if (checkPassable && is_impassable(context->tman,context->map->chunk,context->relpos.column,next))
                return FALSE;
            context->relpos.row = next;
            if (context->focus[1] == 0 && context->relpos.row <= context->map->chunkSize.rows/2)
                pok_map_render_context_align(context);
        }
        /* focus[1] will always be in range (focus[1] > 0 before the operation) */
        else if (context->viewingChunks[context->focus[0]][context->focus[1]-1] == NULL)
            return FALSE; /* there is no chunk to the north */
        else {
            /* focus on the new chunk */
            next = context->map->chunkSize.rows - 1;
            if (checkPassable && is_impassable(context->tman,context->map->chunk->adjacent[dir],context->relpos.column,next))
                return FALSE;
            context->relpos.row = next;
            --context->chunkpos.Y;
            --context->focus[1];
            context->map->chunk = context->map->chunk->adjacent[dir];
        }
    }
    else if (dir == pok_direction_down) {
        if (context->relpos.row < context->map->chunkSize.rows-1) {
            next = context->relpos.row + 1;
            if (checkPassable && is_impassable(context->tman,context->map->chunk,context->relpos.column,next))
                return FALSE;
            context->relpos.row = next;
            if (context->focus[1] == 2 && context->relpos.row >= context->map->chunkSize.rows/2)
                pok_map_render_context_align(context);
        }
        /* focus[1] will always be in range (focus[1] <= 1 before the operation) */
        else if (context->viewingChunks[context->focus[0]][context->focus[1]+1] == NULL)
            return FALSE; /* there is not a chunk to the south */
        else {
            /* focus on the new chunk */
            if (checkPassable && is_impassable(context->tman,context->map->chunk->adjacent[dir],context->relpos.column,0))
                return FALSE;
            context->relpos.row = 0;
            ++context->chunkpos.Y;
            ++context->focus[1];
            context->map->chunk = context->map->chunk->adjacent[dir];
        }
    }
    else if (dir == pok_direction_left) {
        if (context->relpos.column > 0) {
            next = context->relpos.column - 1;
            if (checkPassable && is_impassable(context->tman,context->map->chunk,next,context->relpos.row))
                return FALSE;
            context->relpos.column = next;
            if (context->focus[0] == 0 && context->relpos.column <= context->map->chunkSize.columns/2)
                pok_map_render_context_align(context);
        }
        /* focus[0] will always be in range (focus[0] >= 1 before the operation) */
        else if (context->viewingChunks[context->focus[0]-1][context->focus[1]] == NULL)
            return FALSE; /* there is no chunk to the west */
        else {
            /* focus on the new chunk */
            next = context->map->chunkSize.columns - 1;
            if (checkPassable && is_impassable(context->tman,context->map->chunk->adjacent[dir],next,context->relpos.row))
                return FALSE;
            context->relpos.column = next;
            --context->chunkpos.X;
            --context->focus[0];
            context->map->chunk = context->map->chunk->adjacent[dir];
        }
    }
    else if (dir == pok_direction_right) {
        if (context->relpos.column < context->map->chunkSize.columns-1) {
            next = context->relpos.column + 1;
            if (checkPassable && is_impassable(context->tman,context->map->chunk,next,context->relpos.row))
                return FALSE;
            context->relpos.column = next;
            if (context->focus[0] == 2 && context->relpos.column >= context->map->chunkSize.columns/2)
                pok_map_render_context_align(context);
        }
        /* focus[0] will always be in range (focus[0] <= 1 before the operation) */
        else if (context->viewingChunks[context->focus[0]+1][context->focus[1]] == NULL)
            return FALSE; /* there is no chunk to the east */
        else {
            /* focus on the new chunk */
            if (checkPassable && is_impassable(context->tman,context->map->chunk->adjacent[dir],0,context->relpos.row))
                return FALSE;
            context->relpos.column = 0;
            ++context->chunkpos.X;
            ++context->focus[0];
            context->map->chunk = context->map->chunk->adjacent[dir];
        }
    }
    else
        return FALSE;
    context->changed = TRUE;
    return TRUE;
}
void pok_map_render_context_set_update(struct pok_map_render_context* context,enum pok_direction dir,uint16_t dimension)
{
    /* set the scroll offset; this is in the opposite direction from the specified; e.g. when the map
       moves up it scrolls DOWN */
    switch (dir) {
    case pok_direction_up:
        context->offset[1] = -dimension;
        break;
    case pok_direction_down:
        context->offset[1] = dimension;
        break;
    case pok_direction_left:
        context->offset[0] = -dimension;
        break;
    case pok_direction_right:
        context->offset[0] = dimension;
        break;
    default:
        break;
    }
    context->scrollTicks = 0;
    context->groove = FALSE;
    context->update = TRUE;
}
bool_t pok_map_render_context_update(struct pok_map_render_context* context,uint16_t dimension,uint32_t ticks)
{
    /* check to see if map is being updated, and that enough time has elapsed for an update */
    context->scrollTicks += ticks;
    if (context->update) {
        if (context->scrollTicks >= context->scrollTicksAmt) {
            /* updating map render context by incrementing the map offset in
               the correct direction */
            int inc;
            int times;
            /* compute increment amount and the number of times to do it */
            inc = dimension / context->granularity;
            times = context->scrollTicks / context->scrollTicksAmt;
            if (inc == 0) /* granularity was too fine */
                inc = times;
            else
                inc *= times;
            /* reset scroll ticks for next time; keep leftover ticks if any to keep accurate time */
            context->scrollTicks %= context->scrollTicksAmt;
            /* update scroll offset */
            if (context->offset[0] < 0) {
                context->offset[0] += inc;
                if (context->offset[0] > 0)
                    context->offset[0] = 0;
            }
            else if (context->offset[0] > 0) {
                context->offset[0] -= inc;
                if (context->offset[0] < 0)
                    context->offset[0] = 0;
            }
            else if (context->offset[1] < 0) {
                context->offset[1] += inc;
                if (context->offset[1] > 0)
                    context->offset[1] = 0;
            }
            else if (context->offset[1] > 0) {
                context->offset[1] -= inc;
                if (context->offset[1] < 0)
                    context->offset[1] = 0;
            }
            /* check if completed */
            if (context->offset[0] == 0 && context->offset[1] == 0) {
                /* done: return TRUE to denote that the process finished */
                context->update = FALSE;
                context->groove = TRUE;
                return TRUE;
            }
        }
    }
    else if (context->groove && context->scrollTicks >= context->scrollTicksAmt * (context->granularity-1))
        /* we lost our groove */
        context->groove = FALSE;
    return FALSE;
}

/* implementation of check render info function for map rendering routine; the 'pok_chunk_render_info' structures
   created by this routine are used in many rendering contexts (not just for maps) */
void compute_chunk_render_info(struct pok_map_render_context* context,const struct pok_graphics_subsystem* sys)
{
    /* a map is painted as an application of at most 4 chunks; this routine computes which chunks are needed along with
       the bounding information specifying how each chunk is to be drawn; this routine corrects for small chunk dimensions
       (but the dimensions should be correct for any multi-chunk map, otherwise undefined behavior will result); the viewing
       space is expanded to include a column/row beyond the screen's border; this allows off-screen tiles to scroll correctly
       into the viewing area */
    int i, d[2];
    uint16_t u, v;
    const int32_t ZERO = - (int32_t)sys->dimension;
    /* chunks: chunk1 is always the horizontally adjacent chunk (if any) and
       chunk2 is always the vertically adjacent chunk (if any) */
    for (i = 0;i < 4;++i)
        context->info[i].chunk = NULL;
    /* compute initial chunk render info (offset so that we render a "border" around the viewing space) */
    context->info[0].px = ZERO;
    context->info[0].py = ZERO;
    context->info[0].across = sys->windowSize.columns + 2;
    context->info[0].down = sys->windowSize.rows + 2;
    context->info[0].chunkPos = context->chunkpos;
    context->info[0].chunk = context->map->chunk;
    d[0] = context->focus[0]; d[1] = context->focus[1];
    i = (int)context->relpos.column - (int)sys->playerLocation.column - 1; /* compute left column position within chunk */
    if (i < 0) {
        /* viewing area exceeds chunk bounds on the left */
        u = -i; /* number of columns to the left */
        context->info[0].px += sys->dimension * u;
        context->info[0].across -= u;
        context->info[0].loc.column = 0;
        /* set dimensions for adjacent chunk; 'py', 'down' and 'loc.row' members to be set later */
        context->info[1].px = ZERO;
        context->info[1].across = u;
        context->info[1].loc.column = context->map->chunkSize.columns - u;
        context->info[1].chunkPos = context->info[0].chunkPos; --context->info[1].chunkPos.X;
        context->info[1].chunk = context->viewingChunks[context->focus[0]-1][context->focus[1]]; /* immediate left */
        --d[0];
    }
    else {
        context->info[0].loc.column = i;
        u = context->map->chunkSize.columns - context->relpos.column - 1; /* how many columns to the right in chunk? */
        if (u < sys->_playerLocationInv.column + 1) {
            /* viewing area exceeds chunk bounds on the right
               (note: viewing area can only exceed on the right given it did not exceed on the left) */
            v = sys->_playerLocationInv.column + 1 - u; /* how many columns to the right in viewing area? */
            context->info[0].across -= v;
            /* set dimensions for adjacent chunk; 'py', 'down' and 'loc.row' members to be set later */
            context->info[1].px = context->info[0].px + sys->dimension * context->info[0].across;
            context->info[1].across = v;
            context->info[1].loc.column = 0;
            context->info[1].chunkPos = context->info[0].chunkPos; ++context->info[1].chunkPos.X;
            context->info[1].chunk = context->viewingChunks[context->focus[0]+1][context->focus[1]]; /* immediate right */
            ++d[0];
        }
    }
    i = (int)context->relpos.row - (int)sys->playerLocation.row - 1; /* compute top row position within chunk */
    if (i < 0) {
        /* viewing area exceeds chunk bounds above */
        u = -i; /* number of rows above */
        context->info[0].py += sys->dimension * u;
        context->info[0].down -= u;
        context->info[0].loc.row = 0;
        /* set dimensions for adjacent chunk */
        context->info[2].py = ZERO;
        context->info[2].down = u;
        context->info[2].loc.row = context->map->chunkSize.rows - u;
        context->info[2].chunkPos = context->info[0].chunkPos; --context->info[2].chunkPos.Y;
        context->info[2].chunk = context->viewingChunks[context->focus[0]][context->focus[1]-1]; /* straight up */
        --d[1];
    }
    else {
        context->info[0].loc.row = i;
        u = context->map->chunkSize.rows - context->relpos.row - 1; /* how many rows below in chunk? */
        if (u < sys->_playerLocationInv.row+1) {
            /* viewing area exceeds chunk bounds below
               (note: viewing area can only exceed below given it did not exceed above) */
            v = sys->_playerLocationInv.row + 1 - u; /* how many rows below in viewing area? */
            context->info[0].down -= v;
            /* set dimensions for adjacent chunk */
            context->info[2].py = context->info[0].py + sys->dimension * context->info[0].down;
            context->info[2].down = v;
            context->info[2].loc.row = 0;
            context->info[2].chunkPos = context->info[0].chunkPos; ++context->info[2].chunkPos.Y;
            context->info[2].chunk = context->viewingChunks[context->focus[0]][context->focus[1]+1]; /* straight down */
            ++d[1];
        }
    }
    if (context->info[1].chunk != NULL) {
        /* chunk 1 is identical to chunk 0 vertically */
        context->info[1].py = context->info[0].py;
        context->info[1].down = context->info[0].down;
        context->info[1].loc.row = context->info[0].loc.row;
    }
    if (context->info[2].chunk != NULL) {
        /* chunk 2 is identical to chunk 0 horizontally */
        context->info[2].px = context->info[0].px;
        context->info[2].across = context->info[0].across;
        context->info[2].loc.column = context->info[0].loc.column;
    }
    if (context->info[1].chunk!=NULL && context->info[2].chunk!=NULL) {
        /* we need to pull from a diagonal chunk; chunk 3 will be vertically identical
           to chunk 1 and horizontally identical to chunk 2 */
        context->info[3].px = context->info[1].px;
        context->info[3].across = context->info[1].across;
        context->info[3].loc.column = context->info[1].loc.column;
        context->info[3].py = context->info[2].py;
        context->info[3].down = context->info[2].down;
        context->info[3].loc.row = context->info[2].loc.row;
        context->info[3].chunkPos.X = context->info[0].chunkPos.X + d[0];
        context->info[3].chunkPos.Y = context->info[1].chunkPos.Y + d[1];
        context->info[3].chunk = context->viewingChunks[d[0]][d[1]];
    }
    /* correct chunk sizes */
    for (i = 0;i < 4;++i) {
        if (context->info[i].chunk != NULL) {
            if (context->info[i].across > context->map->chunkSize.columns)
                context->info[i].across = context->map->chunkSize.columns;
            if (context->info[i].down > context->map->chunkSize.rows)
                context->info[i].down = context->map->chunkSize.rows;
        }
    }
}

/* pok map rendering function */
void pok_map_render(const struct pok_graphics_subsystem* sys,struct pok_map_render_context* context)
{
    int i;
    if (context->changed) {
        /* compute dimensions of draw spaces */
        compute_chunk_render_info(context,sys);
        context->changed = FALSE;
    }
    /* draw each of the (possible) 4 chunks; make sure to perform scroll offset */
    for (i = 0;i < 4;++i) {
        if (context->info[i].chunk != NULL) {
            uint16_t h, row = context->info[i].loc.row;
            uint32_t y = context->info[i].py + context->offset[1];
            for (h = 0;h < context->info[i].down;++h,++row,y+=sys->dimension) {
                uint16_t w, col = context->info[i].loc.column;
                uint32_t x = context->info[i].px + context->offset[0];
                for (w = 0;w < context->info[i].across;++w,++col,x+=sys->dimension)
                    pok_image_render(
                        pok_tile_manager_get_tile(
                            context->tman,
                            context->info[i].chunk->data[row][col].data.tileid,
                            context->tileAniTicks ),
                        x,
                        y );
            }
        }
    }
}
