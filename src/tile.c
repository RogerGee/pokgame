/* tile.c - pokgame */
#include "tile.h"
#include "error.h"
#include <stdlib.h>

void pok_tile_manager_init(struct pok_tile_manager* tman,const struct pok_graphics_subsystem* sys)
{
    tman->sys = sys; /* store a reference to the subsystem; we do not own this object */
    tman->tilecnt = 0;
    tman->tileset = NULL;
}
void pok_tile_manager_delete(struct pok_tile_manager* tman)
{
    if (tman->tilecnt > 0) {
        uint16_t i;
        for (i = 0;i < tman->tilecnt;++i)
            pok_image_free(tman->tileset[i]);
        free(tman->tileset);
    }
}
void pok_tile_manager_load_tiles(struct pok_tile_manager* tman,uint16_t imgc,byte_t* data,bool_t byRef)
{
    /* read tile data from memory; since tile images must match the 'dimension' value, the data is
       assumed to be of the correct length for the given value of 'imgc'; the pixel data also should
       not have an alpha channel */
    uint16_t i;
#ifdef POKGAME_DEBUG
    if (tman->tileset != NULL)
        pok_error(pok_error_fatal,"tileset is already loaded in pok_tile_manager_load_tiles()");
#endif
    tman->tilecnt = imgc;
    tman->tileset = malloc(imgc * sizeof(struct pok_image*));
    for (i = 0;i < imgc;++i) {
        struct pok_image* img;
        if (byRef)
            img = pok_image_new_byref_rgb(tman->sys->dimension,tman->sys->dimension,data);
        else
            img = pok_image_new_byval_rgb(tman->sys->dimension,tman->sys->dimension,data);
        data += img->width * img->height * sizeof(union pixel);
        tman->tileset[i] = img;
    }
}
enum pok_network_result pok_tile_manager_netread_tiles(struct pok_tile_manager* tman,struct pok_data_source* dsrc,
    struct pok_netobj_info* info)
{
    /* read tile info substructure from data source:
        [2 bytes] number of tiles
        [n bytes] 'dimension x dimension' sized 'pok_image' structures */
    enum pok_network_result result = pok_net_already;
    if (info->fieldProg == 0) {
        pok_data_stream_read_uint16(dsrc,&tman->tilecnt);
        result = pok_netobj_info_process(info);
        if (result == pok_net_completed) {
            info->fieldCnt = tman->tilecnt; /* let field count hold number of image substructures to read */
            tman->tileset = malloc(sizeof(struct pok_image*) * tman->tilecnt);
            if (tman->tileset == NULL) {
                pok_exception_flag_memory_error();
                return pok_net_failed_internal;
            }
        }
    }
    if (info->fieldCnt > 0) {
        do {
            struct pok_image** imgp;
            info->fieldProg = 0; /* start field progress state at beginning for substructure */
            imgp = tman->tileset + (tman->tilecnt - info->fieldCnt); /* grab ptr to image substructure */
            *imgp = pok_image_new();
            /* attempt to read from network; assume dimension */
            result = pok_image_netread_ex(*imgp,tman->sys->dimension,tman->sys->dimension,dsrc,info);
            if (result != pok_net_completed)
                break;
            --info->fieldCnt;
        } while (info->fieldCnt > 0);
        info->fieldProg = 1;
    }
    return result;
}
