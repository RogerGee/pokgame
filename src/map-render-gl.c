#include <GL/gl.h>

void pok_map_render(const struct pok_graphics_subsystem* sys,struct pok_map_render_context* context)
{
    int i;
    struct chunk_render_info info[4];
    /* setup the modelview matrix to translate the map by the offset specified */
    glTranslated(context->offset[0],context->offset[1],0.0);
    /* compute dimensions of draw spaces */
    compute_chunk_render_info(info,sys,context);
    /* draw each of the (possible) 4 chunks */
    for (i = 0;i < 4;++i) {
        if (info[i].chunk != NULL) {
            uint16_t w, h;
            for (h = 0;h < info[i].down;++h,++info[i].loc.row,info[i].py+=sys->dimension) {
                uint16_t col = info[i].loc.column;
                uint32_t x = info[i].px;
                for (w = 0;w < info[i].across;++w,++col,x+=sys->dimension)
                    pok_image_render(
                        pok_tile_manager_get_tile(context->tman,info[i].chunk->data[info[i].loc.row][col].data.tileid,context->aniTicks),
                        x,
                        info[i].py );
            }
        }
    }
    /* reset the matrix for future operations */
    glLoadIdentity();
}
