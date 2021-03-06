/* map-context.c - pokgame */
#include "map-context.h"
#include "protocol.h"
#include "error.h"
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
    context->chunk = NULL;
    context->map = NULL;
    context->tman = tman;
    for (i = 0;i < 4;++i)
        context->info[i].chunk = NULL;
    context->granularity = 4;
    context->tileAniTicks = 0;
    context->scrollTicks = 0;
    context->grooveTicks = 0;
    context->scrollTicksAmt = 1;
    context->groove = FALSE;
    context->changed = FALSE;
    context->update = FALSE;
}
void pok_map_render_context_set_map(struct pok_map_render_context* context,struct pok_map* map)
{
    /* set map and reset the other information */
    int i, j;
    context->focus[0] = context->focus[1] = 1;
    context->offset[0] = context->offset[1] = 0;
    for (i = 0;i < 3;++i)
        for (j = 0;j < 3;++j)
            context->viewingChunks[i][j] = NULL;
    context->relpos.column = context->relpos.row = 0;
    context->chunkpos.X = map->originPos.X;
    context->chunkpos.Y = map->originPos.Y;
    context->chunk = map->origin;
    context->map = map;
    pok_map_render_context_align(context);
}
void pok_map_render_context_align(struct pok_map_render_context* context)
{
    /* compute surrounding chunks; place the current chunk in the center */
    context->focus[0] = context->focus[1] = 1;
    context->viewingChunks[1][1] = context->chunk;
    context->viewingChunks[1][0] = context->chunk->adjacent[pok_direction_up];
    context->viewingChunks[1][2] = context->chunk->adjacent[pok_direction_down];
    context->viewingChunks[0][1] = context->chunk->adjacent[pok_direction_left];
    context->viewingChunks[2][1] = context->chunk->adjacent[pok_direction_right];
    context->viewingChunks[0][0] = context->viewingChunks[1][0] != NULL ? context->viewingChunks[1][0]->adjacent[pok_direction_left]
        : (context->viewingChunks[0][1] != NULL ? context->viewingChunks[0][1]->adjacent[pok_direction_up] : NULL);
    context->viewingChunks[2][0] = context->viewingChunks[1][0] != NULL ? context->viewingChunks[1][0]->adjacent[pok_direction_right]
        : (context->viewingChunks[2][1] != NULL ? context->viewingChunks[2][1]->adjacent[pok_direction_up] : NULL);
    context->viewingChunks[0][2] = context->viewingChunks[1][2] != NULL ? context->viewingChunks[1][2]->adjacent[pok_direction_left]
        : (context->viewingChunks[0][1] != NULL ? context->viewingChunks[0][1]->adjacent[pok_direction_down] : NULL);
    context->viewingChunks[2][2] = context->viewingChunks[1][2] != NULL ? context->viewingChunks[1][2]->adjacent[pok_direction_right]
        : (context->viewingChunks[2][1] != NULL ? context->viewingChunks[2][1]->adjacent[pok_direction_down] : NULL);
    context->changed = TRUE;
}
bool_t pok_map_render_context_center_on(struct pok_map_render_context* context,const struct pok_point* chunkpos,const struct pok_location* relpos)
{
    /* center the context on the specified chunk and location; if the chunk does not exist, 
       then FALSE is returned and the context is not updated */
    struct pok_map_chunk* chunk = pok_map_get_chunk(context->map,chunkpos);
    if (chunk == NULL)
        return FALSE;
    /* make sure the position within the chunk is valid */
    if (relpos->column >= context->map->chunkSize.columns || relpos->row >= context->map->chunkSize.rows)
        return FALSE;
    /* realign the context and set relative position */
    context->chunk = chunk;
    context->chunkpos = *chunkpos;
    context->relpos = *relpos;
    /* the next call sets 'context->changed'; this needs to be the final operation (for thread-safety) */
    pok_map_render_context_align(context);
    return TRUE;
}
bool_t pok_map_render_context_set_position(struct pok_map_render_context* context,
    struct pok_map* map,
    const struct pok_point* chunkpos,
    const struct pok_location* relpos)
{
    struct pok_map* oldMap;
    /* perform 'set_map' then 'center_on'; if 'center_on' fails, restore the
      previous configuration; alignment only occurs once */
    oldMap = context->map;
    context->map = map;
    if ( !pok_map_render_context_center_on(context,chunkpos,relpos) ) {
        context->map = oldMap;
        return FALSE;
    }
    /* 'context->changed' was flagged in a previous function call */
    return TRUE;
}
static bool_t is_impassable(const struct pok_tile_manager* tman,struct pok_map_chunk* chunk,uint16_t column,uint16_t row)
{
    /* check to see if the data specifies a warp for the tile in question; a
       warp location is always passable */
    if (chunk->data[row][column].data.warpKind != pok_tile_warp_none)
        return FALSE;
    if (chunk->data[row][column].data.tileid <= tman->impassibility) {
        /* make sure there is not an exception to impassibility rule */
        if ( !chunk->data[row][column].pass )
            return TRUE;
    }
    else if ( chunk->data[row][column].impass ) /* check for exception */
        return TRUE;
    return FALSE;
}
bool_t pok_map_render_context_move(struct pok_map_render_context* context,enum pok_direction dir,uint16_t skipTiles,bool_t checkPassable)
{
    /* change the map position by skipTiles+1 position units in the specified direction; if this is not possible,
       then leave the map (and the render context) unchanged and return FALSE; otherwise update the relative
       position; if we leave the current chunk then update the context; if we walk into an area close to the
       edge of the 3x3 chunk grid, then re-align the grid around the edge chunk; note that maps can have an
       irregular size, so for the south and east directions the absence of a chunk bounds the map */
    uint16_t next;
    uint16_t offset = skipTiles + 1;
    if (dir == pok_direction_up) {
        if (context->relpos.row > skipTiles) {
            next = context->relpos.row - offset;
            if (checkPassable && is_impassable(context->tman,context->chunk,context->relpos.column,next))
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
            next = context->map->chunkSize.rows - offset;
            if (checkPassable && is_impassable(context->tman,context->chunk->adjacent[dir],context->relpos.column,next))
                return FALSE;
            context->relpos.row = next;
            --context->chunkpos.Y;
            --context->focus[1];
            context->chunk = context->chunk->adjacent[dir];
        }
    }
    else if (dir == pok_direction_down) {
        if (context->relpos.row < context->map->chunkSize.rows-offset) {
            next = context->relpos.row + offset;
            if (checkPassable && is_impassable(context->tman,context->chunk,context->relpos.column,next))
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
            if (skipTiles < context->map->chunkSize.rows
                && checkPassable && is_impassable(context->tman,context->chunk->adjacent[dir],context->relpos.column,skipTiles))
                return FALSE;
            context->relpos.row = skipTiles;
            ++context->chunkpos.Y;
            ++context->focus[1];
            context->chunk = context->chunk->adjacent[dir];
        }
    }
    else if (dir == pok_direction_left) {
        if (context->relpos.column > skipTiles) {
            next = context->relpos.column - offset;
            if (checkPassable && is_impassable(context->tman,context->chunk,next,context->relpos.row))
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
            next = context->map->chunkSize.columns - offset;
            if (checkPassable && is_impassable(context->tman,context->chunk->adjacent[dir],next,context->relpos.row))
                return FALSE;
            context->relpos.column = next;
            --context->chunkpos.X;
            --context->focus[0];
            context->chunk = context->chunk->adjacent[dir];
        }
    }
    else if (dir == pok_direction_right) {
        if (context->relpos.column < context->map->chunkSize.columns-offset) {
            next = context->relpos.column + offset;
            if (checkPassable && is_impassable(context->tman,context->chunk,next,context->relpos.row))
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
            if (skipTiles < context->map->chunkSize.columns
                && checkPassable && is_impassable(context->tman,context->chunk->adjacent[dir],skipTiles,context->relpos.row))
                return FALSE;
            context->relpos.column = skipTiles;
            ++context->chunkpos.X;
            ++context->focus[0];
            context->chunk = context->chunk->adjacent[dir];
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
    /* reset ticks to maintain smooth rendering when a new animation sequence is
       requested; we may lose some game time but it will be more consistent */
    context->scrollTicks = 0;
    context->groove = FALSE;
    context->update = TRUE;
}
bool_t pok_map_render_context_update(struct pok_map_render_context* context,uint16_t dimension,uint32_t ticks)
{
    /* check to see if map is being updated, and that enough time has elapsed for an update */
    if (context->update) {
        context->scrollTicks += ticks;
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
                context->grooveTicks = 0;
                return TRUE;
            }
        }
    }
    else if (context->groove) {
        context->grooveTicks += ticks;
        if (context->grooveTicks >= context->scrollTicksAmt * (context->granularity - 1)) {
            /* we lost our groove */
            context->groove = FALSE;
        }
    }
    return FALSE;
}
struct pok_tile* pok_map_render_context_get_adjacent_tile(struct pok_map_render_context* context,int x,int y)
{
    struct pok_map_chunk* chunk = context->chunk;
    struct pok_location pos = context->relpos;
    const struct pok_size* chunkSize = &context->map->chunkSize;
    if (x > 0) {
        if (pos.column < chunkSize->columns - 1)
            ++pos.column;
        else {
            chunk = context->viewingChunks[context->focus[0]+1][context->focus[1]];
            if (chunk == NULL)
                return NULL;
            pos.column = 0;
        }
    }
    else if (x < 0) {
        if (pos.column > 0)
            --pos.column;
        else {
            chunk = context->viewingChunks[context->focus[0]-1][context->focus[1]];
            if (chunk == NULL)
                return NULL;
            pos.column = chunkSize->columns-1;
        }
    }
    if (y > 0) {
        if (pos.row < chunkSize->rows - 1)
            ++pos.row;
        else {
            chunk = context->viewingChunks[context->focus[0]+1][context->focus[1]+1];
            if (chunk == NULL)
                return NULL;
            pos.row = 0;
        }
    }
    else if (y < 0) {
        if (pos.row > 0)
            --pos.row;
        else {
            chunk = context->viewingChunks[context->focus[0]][context->focus[1]-1];
            if (chunk == NULL)
                return NULL;
            pos.row = chunkSize->rows-1;
        }
    }
    return chunk->data[pos.row] + pos.column;
}

/* Implementation of check render info function for map rendering routine. The
 * 'pok_chunk_render_info' structures created by this routine are used in many
 * rendering contexts (not just for maps).
 */
void compute_chunk_render_info(struct pok_map_render_context* context,
    const struct pok_graphics_subsystem* sys)
{
    /* A map is painted as an application of at most 4 chunks. This routine
     * computes which chunks are needed along with the bounding information
     * specifying how each chunk is to be drawn. This routine corrects for small
     * chunk dimensions (but the dimensions should be correct for any
     * multi-chunk map, otherwise undefined behavior will result). The viewing
     * space is expanded to include 2 columns and/or rows beyond the screen's
     * border; this allows off-screen tiles to scroll correctly into the viewing
     * area.
     *
     * The position in context->info is important. Chunk 0 is always the current
     * chunk (i.e. the one focused by the map). Chunk 1 is the horizontally
     * adjacent chunk (i.e. to the left or right). Chunk 2 is the vertically
     * adjacent chunk (i.e. to the north or south). Chunk 3 is the diagonally
     * adjacent chunk. Metrics for the chunk info are computer regardless of if
     * a chunk is actually present. Therefore always check that
     * context->info[N]->chunk is not NULL.
     */

    int i, d[2], f[2];
    uint16_t u, v;
    const int32_t ZERO = - (int32_t)sys->dimension * 2;

    /* Reset the chunks. */
    for (i = 0;i < 4;++i) {
        context->info[i].chunk = NULL;
    }

    /* Compute initial chunk render info. We offset so that we render a "border"
     * around the viewing space for potential animations. This border is twice
     * the dimension so that we can offset by at most 2 tiles in any direction.
     */
    context->info[0].px = ZERO;
    context->info[0].py = ZERO;
    context->info[0].across = sys->windowSize.columns + 4;
    context->info[0].down = sys->windowSize.rows + 4;
    context->info[0].chunkPos = context->chunkpos;
    context->info[0].chunk = context->chunk;

    /* Let 'd' hold the position of the diagonal chunk to consider given the
     * layout of the other chunks.
     */
    d[0] = f[0] = context->focus[0];
    d[1] = f[1] = context->focus[1];

    /*** Handle chunks left or right ***/

    /* Compute left column bound. If it is negative then we have exceeded chunk
     * bounds on the left and need to grab the left chunk.
     */
    i = (int)context->relpos.column - (int)sys->playerLocation.column - 2;
    if (i < 0) {
        /* Viewing area exceeds chunk bounds on the left. Get the number of
         * columns to the left.
         */
        u = -i;

        /* Adjust the current chunk bounds to account for the left chunk we're
         * bringing into the picture.
         */
        context->info[0].px += sys->dimension * u;
        context->info[0].across -= u;
        context->info[0].loc.column = 0;

        /* Offset diagonal position to the immediate left. */
        d[0] -= 1;

        /* Set available dimensions for adjacent chunk. We need to set these
         * even if the chunk is NULL so that they can be used for a potential
         * diagonal chunk. The 'py', 'down' and 'loc.row' members are to be set
         * later.
         */
        context->info[1].px = ZERO;
        context->info[1].across = u;
        context->info[1].loc.column = context->map->chunkSize.columns - u;
        context->info[1].chunkPos = context->info[0].chunkPos;
        context->info[1].chunkPos.X -= 1;
        context->info[1].chunk = context->viewingChunks[f[0] - 1][f[1]];
    }
    else {
        /* Adjust column start bound for first chunk. */
        context->info[0].loc.column = i;

        /* Compute how many columns are to the right in this chunk. */
        u = context->map->chunkSize.columns - context->relpos.column - 1;
        if (u < sys->_playerLocationInv.column+2) {
            /* Viewing area exceeds chunk bounds on the right. Note: viewing
             * area can only exceed on the right given it did not exceed on the
             * left.
             */

            /* Figure out how many columns are to the right in viewing area. */
            v = sys->_playerLocationInv.column + 2 - u;

            /* Adjust first chunk bounds accordingly. */
            context->info[0].across -= v;

            /* Offset diagonal position to the immediate right. */
            d[0] += 1;

            /* Set available dimensions for adjacent chunk and grab it. We need
             * to set these even if the chunk is NULL so that they can be used
             * for a potential diagonal chunk. The 'py', 'down' and 'loc.row'
             * members are to be set later.
             */
            context->info[1].px = context->info[0].px + sys->dimension * context->info[0].across;
            context->info[1].across = v;
            context->info[1].loc.column = 0;
            context->info[1].chunkPos = context->info[0].chunkPos;
            context->info[1].chunkPos.X += 1;
            context->info[1].chunk = context->viewingChunks[f[0] + 1][f[1]];
        }
    }

    /*** Handle chunks above or below ***/

    /* Computer top column bound. If it is negative then we have exceeded chunk
     * bounds above the current chunk and need to grab the chunk to the north.
     */
    i = (int)context->relpos.row - (int)sys->playerLocation.row - 2;
    if (i < 0) {
        /* Viewing area exceeds chunk bounds above. Get the number of rows to
         * above.
         */
        u = -i;

        /* Adjust the current chunk bounds to account for the north chunk we're
         * bringing into the picture.
         */
        context->info[0].py += sys->dimension * u;
        context->info[0].down -= u;
        context->info[0].loc.row = 0;

        /* Offset diagonal position straight up. */
        d[1] -= 1;

        /* Set dimensions for adjacent chunk. We need to set these even if the
         * chunk is NULL so that they can be used for a potential diagonal
         * chunk. Some members to be set at a later time.
         */
        context->info[2].py = ZERO;
        context->info[2].down = u;
        context->info[2].loc.row = context->map->chunkSize.rows - u;
        context->info[2].chunkPos = context->info[0].chunkPos;
        context->info[2].chunkPos.Y -= 1;
        context->info[2].chunk = context->viewingChunks[f[0]][f[1] - 1];
    }
    else {
        context->info[0].loc.row = i;
        u = context->map->chunkSize.rows - context->relpos.row - 1; /* how many rows below in chunk? */
        if (u < sys->_playerLocationInv.row+2) {
            /* Viewing area exceeds chunk bounds below. Note: viewing area can
             * only exceed below given it did not exceed above.
             */

            /* Figure out how many rows are below in viewing area. */
            v = sys->_playerLocationInv.row + 2 - u;

            /* Adjust first chunk bounds accordingly. */
            context->info[0].down -= v;

            /* Offset diagonal position straight down. */
            d[1] += 1;

            /* Set dimensions for adjacent chunk. We need to set these even if
             * the chunk is NULL so that they can be used for a potential
             * diagonal chunk. Some members to be set at a later time.
             */
            context->info[2].py = context->info[0].py + sys->dimension * context->info[0].down;
            context->info[2].down = v;
            context->info[2].loc.row = 0;
            context->info[2].chunkPos = context->info[0].chunkPos;
            context->info[2].chunkPos.Y += 1;
            context->info[2].chunk = context->viewingChunks[f[0]][f[1] + 1];
        }
    }

    /* Assign similar information for chunks with common dimensions. */
    if (context->info[1].chunk != NULL) {
        /* Chunk 1 is identical to chunk 0 vertically. */
        context->info[1].py = context->info[0].py;
        context->info[1].down = context->info[0].down;
        context->info[1].loc.row = context->info[0].loc.row;
    }
    if (context->info[2].chunk != NULL) {
        /* Chunk 2 is identical to chunk 0 horizontally. */
        context->info[2].px = context->info[0].px;
        context->info[2].across = context->info[0].across;
        context->info[2].loc.column = context->info[0].loc.column;
    }

    /* We need to pull for a diagonal chunk if the offset from focus changed in
     * both directions (and if a chunk actually exists there).
     */
    if (d[0] != f[0] && d[1] != f[1] && context->viewingChunks[d[0]][d[1]] != NULL) {
        /* Chunk 3 will be vertically identical to chunk 1 and horizontally
         * identical to chunk 2.
         */
        context->info[3].px = context->info[1].px;
        context->info[3].across = context->info[1].across;
        context->info[3].loc.column = context->info[1].loc.column;
        context->info[3].py = context->info[2].py;
        context->info[3].down = context->info[2].down;
        context->info[3].loc.row = context->info[2].loc.row;
        context->info[3].chunkPos.X = context->info[0].chunkPos.X + d[0];
        context->info[3].chunkPos.Y = context->info[0].chunkPos.Y + d[1];
        context->info[3].chunk = context->viewingChunks[d[0]][d[1]];
    }

    /* Correct chunk sizes to make sure we're within bounds. */
    for (i = 0;i < 4;++i) {
        if (context->info[i].chunk != NULL) {
            if (context->info[i].across > context->map->chunkSize.columns) {
                context->info[i].across = context->map->chunkSize.columns;
            }
            if (context->info[i].down > context->map->chunkSize.rows) {
                context->info[i].down = context->map->chunkSize.rows;
            }
        }
    }
}

/* pok map rendering function */
void pok_map_render(const struct pok_graphics_subsystem* sys,struct pok_map_render_context* context)
{
    int i;
    if (context->changed) {
        /* Compute dimensions of draw spaces if the context was changed. This
         * flag helps provide thread safety as well since another thread may
         * change the context before toggling 'context->changed' to make the
         * changes go into effect).
         */
        compute_chunk_render_info(context,sys);
        context->changed = FALSE;
    }
    /* Draw each of the (possible) 4 chunks, and make sure to perform scroll
     * offset.
     */
    for (i = 0;i < 4;++i) {
        if (context->info[i].chunk != NULL) {
            uint16_t h, row = context->info[i].loc.row;
            uint32_t y = context->info[i].py + context->offset[1];
            for (h = 0;h < context->info[i].down;++h,++row,y+=sys->dimension) {
                uint16_t w, col = context->info[i].loc.column;
                uint32_t x = context->info[i].px + context->offset[0];
                for (w = 0;w < context->info[i].across;++w,++col,x+=sys->dimension) {
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
}
