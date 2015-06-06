#include "pokgame.h"
#if defined(POKGAME_WIN32)
/* must include Windows.h before gl.h using Microsoft C compiler */
#include <Windows.h>
#endif
#include <GL/gl.h>

void pok_map_render(const struct pok_graphics_subsystem* sys,struct pok_map_render_context* context)
{
    int i;
    /* obtain lock for the map context */
    pok_game_lock(context);
    /* compute dimensions of draw spaces */
    compute_chunk_render_info(context,sys);
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
    /* release lock for the map context */
    pok_game_unlock(context);
}
