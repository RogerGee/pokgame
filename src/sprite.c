/* sprite.c - pokgame */
#include "sprite.h"
#include "error.h"
#include <stdlib.h>

void pok_sprite_manager_init(struct pok_sprite_manager* sman,const struct pok_graphics_subsystem* sys)
{
    sman->sys = sys; /* store a reference to the subsystem; we do not own this object */
    sman->spritecnt = 0;
    sman->spriteset = NULL;
}
void pok_sprite_manager_delete(struct pok_sprite_manager* sman)
{
    if (sman->spritecnt > 0) {
        uint16_t i;
        for (i = 0;i < sman->spritecnt;++i)
            pok_image_free(sman->spriteset[i]);
        free(sman->spriteset);
    }
}
static void pok_sprite_manager_assoc(struct pok_sprite_manager* sman)
{
    /* configure the sprite association */
    
}
void pok_sprite_manager_load_sprites(struct pok_sprite_manager* sman,uint16_t imgc,byte_t* data,bool_t byRef)
{
    /* read sprite data from memory; since sprite images must match the 'sman->sys->dimension' value, the data is
       assumed to be of the correct length for the given value of 'imgc' */
    uint16_t i, r;
#ifdef POKGAME_DEBUG
    if (sman->spriteset != NULL)
        pok_error(pok_error_fatal,"spriteset is already loaded in pok_sprite_manager_load_sprites()");
#endif
    r = imgc % 8;
    sman->spritecnt = r==0 ? imgc : imgc+r;
    sman->spriteset = malloc(sman->spritecnt * sizeof(struct pok_image*));
    for (i = 0;i < imgc;++i) {
        struct pok_image* img;
        if (byRef)
            img = pok_image_new_byref_rgb(sman->sys->dimension,sman->sys->dimension,data);
        else
            img = pok_image_new_byval_rgb(sman->sys->dimension,sman->sys->dimension,data);
        data += img->width * img->height * sizeof(union pixel);
        sman->spriteset[i] = img;
    }
    for (;i < sman->spritecnt;++i) {
        /* refer to the black tile image provided globally */

    }
}
enum pok_network_result pok_sprite_manager_netread_sprites(struct pok_sprite_manager* sman,struct pok_data_source* dsrc,
    struct pok_netobj_readinfo* info)
{
    /* read sprite info substructure from data source:
        [2 bytes] number of (character) sprite images
        [n bytes] 'dimension x dimension' sized 'pok_image' structures */
    size_t i;
    enum pok_network_result result = pok_net_already;
    if (info->fieldProg == 0) {
        pok_data_stream_read_uint16(dsrc,&sman->spritecnt);
        result = pok_netobj_readinfo_process(info);
        if (result == pok_net_completed) {
            info->fieldCnt = sman->spritecnt; /* let field count hold number of image substructures to read */
            sman->spriteset = malloc(sizeof(struct pok_image*) * sman->spritecnt);
            if (sman->spriteset == NULL) {
                pok_exception_flag_memory_error();
                return pok_net_failed_internal;
            }
            /* initialize image substructures so that later calls can determine if allocated or not */
            for (i = 0;i < sman->spritecnt;++i)
                sman->spriteset[i] = NULL;
        }
    }
    if (info->fieldCnt > 0) { /* info->fieldProg == 1 */
        do {
            struct pok_image** imgp;
            info->fieldProg = 0; /* start field progress state at beginning for substructure */
            imgp = sman->spriteset + (sman->spritecnt - info->fieldCnt); /* grab ptr to image substructure */
            if (*imgp == NULL) {
                *imgp = pok_image_new();
                if (info->next == NULL)
                    info->next = pok_netobj_readinfo_new();
                else {
                    /* for correctness, delete the netobj_readinfo first */
                    pok_netobj_readinfo_delete(info->next);
                    pok_netobj_readinfo_init(info->next);
                }
            }
            /* attempt to read from network; assume dimension */
            result = pok_image_netread_ex(*imgp,sman->sys->dimension,sman->sys->dimension,dsrc,info->next);
            if (result != pok_net_completed)
                break;
            --info->fieldCnt;
        } while (info->fieldCnt > 0);
        if (info->fieldCnt == 0)
            ++info->fieldProg;
    }
    return result;
}
