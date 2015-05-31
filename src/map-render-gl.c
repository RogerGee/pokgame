#include "pokgame.h"
#include <GL/gl.h>

void pok_map_render(const struct pok_graphics_subsystem* sys,struct pok_map_render_context* context)
{
    int i;
    /* setup the modelview matrix to translate the map by the offset specified */
    glTranslated(context->offset[0],context->offset[1],0.0);
    /* compute dimensions of draw spaces */
    compute_chunk_render_info(context->info,sys,context);
    /* obtain lock for the map context */
    pok_game_lock(context);
    /* draw each of the (possible) 4 chunks */
    for (i = 0;i < 4;++i) {
        if (context->info[i].chunk != NULL) {
            uint16_t h, row = context->info[i].loc.row;
            uint32_t y = context->info[i].py;
            for (h = 0;h < context->info[i].down;++h,++row,y+=sys->dimension) {
                uint16_t w, col = context->info[i].loc.column;
                uint32_t x = context->info[i].px;
                for (w = 0;w < context->info[i].across;++w,++col,x+=sys->dimension)
                    pok_image_render(
                        pok_tile_manager_get_tile(
                            context->tman,
                            context->info[i].chunk->data[row][col].data.tileid,
                            context->aniTicks ),
                        x,
                        y );
            }
        }
    }
    /* release lock for the map context */
    pok_game_unlock(context);
    /* reset the matrix for future operations */
    glLoadIdentity();
}
