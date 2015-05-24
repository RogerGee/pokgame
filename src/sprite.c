/* sprite.c - pokgame */
#include "sprite.h"
#include "error.h"
#include <stdlib.h>

struct pok_sprite_manager* pok_sprite_manager_new(const struct pok_graphics_subsystem* sys)
{
    struct pok_sprite_manager* sman;
    sman = malloc(sizeof(struct pok_sprite_manager));
    if (sman == NULL) {
        pok_exception_flag_memory_error();
        return NULL;
    }
    pok_sprite_manager_init(sman,sys);
    return sman;
}
void pok_sprite_manager_free(struct pok_sprite_manager* sman)
{
    pok_sprite_manager_delete(sman);
    free(sman);
}
void pok_sprite_manager_init(struct pok_sprite_manager* sman,const struct pok_graphics_subsystem* sys)
{
    sman->sys = sys; /* store a reference to the subsystem; we do not own this object */
    sman->spritecnt = 0;
    sman->imagecnt = 0;
    sman->spriteset = NULL;
    sman->spriteassoc = NULL;
}
void pok_sprite_manager_delete(struct pok_sprite_manager* sman)
{
    if (sman->imagecnt > 0 && sman->spriteset != NULL) {
        uint16_t i;
        for (i = 0;i < sman->imagecnt;++i)
            pok_image_free(sman->spriteset[i]);
        free(sman->spriteset);
        free(sman->spriteassoc);
    }
}
static void pok_sprite_manager_assoc(struct pok_sprite_manager* sman)
{
    /* configure the sprite association */

}
bool_t pok_sprite_manager_save(struct pok_sprite_manager* sman,struct pok_data_source* dscr)
{
    return FALSE;
}
bool_t pok_sprite_manager_open(struct pok_sprite_manager* sman,struct pok_data_source* dscr)
{
    return FALSE;
}
void pok_sprite_manager_load(struct pok_sprite_manager* sman,uint16_t imgc,byte_t* data,bool_t byRef)
{
    /* read sprite data from memory; since sprite images must match the 'sman->sys->dimension' value, the data is
       assumed to be of the correct length for the given value of 'imgc' */
    uint16_t i, r;
    size_t length;
#ifdef POKGAME_DEBUG
    if (sman->spriteset != NULL)
        pok_error(pok_error_fatal,"spriteset is already loaded in pok_sprite_manager_load_sprites()");
#endif
    r = imgc % 8;
    sman->imagecnt = r==0 ? imgc : imgc+(8-r); /* nearest multiple of 8 >= 'imgc' */
    sman->spritecnt = sman->imagecnt / 8;
    sman->spriteset = malloc(sman->imagecnt * sizeof(struct pok_image*));
    length = sman->sys->dimension * sman->sys->dimension * sizeof(union pixel);
    for (i = 0;i < imgc;++i) {
        struct pok_image* img;
        if (byRef)
            img = pok_image_new_byref_rgb(sman->sys->dimension,sman->sys->dimension,data);
        else
            img = pok_image_new_byval_rgb(sman->sys->dimension,sman->sys->dimension,data);
        data += length;
        sman->spriteset[i] = img;
    }
    for (;i < sman->imagecnt;++i) {
        /* refer to the black tile image provided globally in case a bad number of sprite images
           were specified */
        sman->spriteset[i] = sman->sys->blacktile;
    }
    pok_sprite_manager_assoc(sman);
}
enum pok_network_result pok_sprite_manager_netread(struct pok_sprite_manager* sman,struct pok_data_source* dsrc,
    struct pok_netobj_readinfo* info)
{
    /* read sprite info substructure from data source:
        [2 bytes] number of (character) sprite images
        [n bytes] 'dimension x dimension' sized 'pok_image' structures */
    size_t i;
    enum pok_network_result result = pok_net_already;
    if (info->fieldProg == 0) {
        pok_data_stream_read_uint16(dsrc,&sman->imagecnt);
        result = pok_netobj_readinfo_process(info);
        if (result == pok_net_completed) {
            uint16_t r = sman->imagecnt % 8;
            info->fieldCnt = sman->imagecnt; /* let field count hold number of image substructures to read */
            info->depth[0] = info->fieldCnt;
            if (r != 0)
                /* force a multiple of eight (it's okay if the peer didn't specify this since we'll use the
                   black tile for missing images) */
                sman->imagecnt += 8 - r;
            sman->spritecnt = sman->imagecnt / 8;
            sman->spriteset = malloc(sizeof(struct pok_image*) * sman->imagecnt);
            if (sman->spriteset == NULL) {
                pok_exception_flag_memory_error();
                return pok_net_failed_internal;
            }
            /* initialize image substructures so that later calls can determine if allocated or not */
            for (i = 0;i < sman->imagecnt;++i)
                sman->spriteset[i] = NULL;
        }
    }
    if (info->fieldProg == 1) {
        do {
            struct pok_image** imgp;
            imgp = sman->spriteset + (info->depth[0] - info->fieldCnt); /* grab ptr to image substructure */
            if (*imgp == NULL) {
                *imgp = pok_image_new();
                pok_netobj_readinfo_alloc_next(info);
            }
            /* attempt to read from network; assume dimension */
            result = pok_image_netread_ex(*imgp,sman->sys->dimension,sman->sys->dimension,dsrc,info->next);
            if (result != pok_net_completed)
                break;
            --info->fieldCnt;
        } while (info->fieldCnt > 0);
        if (info->fieldCnt == 0) { /* we're done */
            /* fill in any remaining spaces with references to the black tile */
            for (;info->depth[0] < sman->imagecnt;++info->depth[0])
                sman->spriteset[info->depth[0]] = sman->sys->blacktile;
            pok_sprite_manager_assoc(sman);
            ++info->fieldProg;
        }
    }
    return result;
}
