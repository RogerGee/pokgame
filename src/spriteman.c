/* spriteman.c - pokgame */
#include "spriteman.h"
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
    sman->spritesets = NULL;
    sman->spriteassoc = NULL;
    sman->_sheet = NULL;
}
void pok_sprite_manager_delete(struct pok_sprite_manager* sman)
{
    if (sman->imagecnt > 0 && sman->spritesets != NULL) {
        uint16_t i;
        for (i = 0;i < sman->imagecnt;++i)
            pok_image_free(sman->spritesets[i]);
        free(sman->spritesets);
    }
    if (sman->spriteassoc != NULL)
        free(sman->spriteassoc);
    if (sman->_sheet != NULL)
        pok_image_free(sman->_sheet);
}
static bool_t pok_sprite_manager_assoc(struct pok_sprite_manager* sman)
{
    /* configure the sprite association */
    uint16_t i;
    if (sman->spriteassoc != NULL)
        free(sman->spriteassoc);
    sman->spriteassoc = malloc(sizeof(struct pok_image**) * sman->spritecnt);
    if (sman->spriteassoc == NULL) {
        pok_exception_flag_memory_error();
        return FALSE;
    }
    for (i = 0;i < sman->spritecnt;++i)
        sman->spriteassoc[i] = sman->spritesets + i*10;
    return TRUE;
}
bool_t pok_sprite_manager_save(struct pok_sprite_manager* sman,struct pok_data_source* dscr)
{
    return FALSE;
}
bool_t pok_sprite_manager_open(struct pok_sprite_manager* sman,struct pok_data_source* dscr)
{
    return FALSE;
}
bool_t pok_sprite_manager_from_data(struct pok_sprite_manager* sman,uint16_t imgc,const byte_t* data,bool_t byRef)
{
    uint16_t i;
    size_t length;
    length = sman->sys->dimension * sman->sys->dimension * sizeof(union pixel);
    for (i = 0;i < imgc;++i) {
        struct pok_image* img;
        if (byRef)
            img = pok_image_new_byref_rgb(sman->sys->dimension,sman->sys->dimension,data);
        else
            img = pok_image_new_byval_rgb(sman->sys->dimension,sman->sys->dimension,data);
        if (img == NULL)
            return FALSE;
        data += length;
        sman->spritesets[i] = img;
    }
    return TRUE;
}
bool_t pok_sprite_manager_load(struct pok_sprite_manager* sman,uint16_t imgc,const byte_t* data,bool_t byRef)
{
    /* read sprite data from memory; since sprite images must match the 'sman->sys->dimension' value, the data is
       assumed to be of the correct length for the given value of 'imgc' */
    uint16_t i, r;
    if (sman->spritesets != NULL) {
        pok_exception_new_ex(pok_ex_spriteman,pok_ex_spriteman_already);
        return FALSE;
    }
    /* allocate frames */
    r = imgc % 10;
    sman->imagecnt = r==0 ? imgc : imgc+(10-r); /* nearest multiple of 10 >= 'imgc' */
    sman->spritecnt = sman->imagecnt / 10;
    sman->spritesets = malloc(sman->imagecnt * sizeof(struct pok_image*));
    if (sman->spritesets == NULL) {
        pok_exception_flag_memory_error();
        return FALSE;
    }
    /* load frames */
    if ( !pok_sprite_manager_from_data(sman,imgc,data,byRef) )
        return FALSE;
    for (i = imgc;i < sman->imagecnt;++i) {
        /* refer to the black tile image provided globally in case a bad number of sprite images
           were specified */
        sman->spritesets[i] = sman->sys->blacktile;
    }
    return pok_sprite_manager_assoc(sman);
}
bool_t pok_sprite_manager_fromfile(struct pok_sprite_manager* sman,const char* file)
{
    struct pok_image* img;
    if (sman->_sheet != NULL || sman->spritesets != NULL) {
        pok_exception_new_ex(pok_ex_spriteman,pok_ex_spriteman_already);
        return FALSE;
    }
    img = pok_image_new();
    if (img == NULL)
        return FALSE;
    if ( !pok_image_fromfile_rgb(img,file) ) {
        pok_image_free(img);
        return FALSE;
    }
    /* check image dimensions; it must support at least a single sprite set */
    if (img->width != sman->sys->dimension || img->height < sman->sys->dimension*10 || img->height % sman->sys->dimension != 0) {
        pok_exception_new_ex(pok_ex_spriteman,pok_ex_spriteman_bad_image_dimension);
        pok_image_free(img);
        return FALSE;
    }
    /* tell the sprite manager to load up images by reference from the sprite sheet */
    if ( !pok_sprite_manager_load(sman,img->height / sman->sys->dimension,img->pixels.data,TRUE) ) {
        pok_image_free(img);
        return FALSE;
    }
    sman->_sheet = img;
    return TRUE;
}
enum pok_network_result pok_sprite_manager_netread(struct pok_sprite_manager* sman,struct pok_data_source* dsrc,
    struct pok_netobj_readinfo* info)
{
    /* read sprite info substructure from data source:
        [2 bytes] number of sprite sets (there are 10 sprites per set)
        [n bytes] pok image containing all sprites; the image's dimensions must be 10*dim by dim*number-of-sets */
    enum pok_network_result result = pok_net_already;
    if (info->fieldProg == 0) {
        pok_data_stream_read_uint16(dsrc,&sman->spritecnt);
        result = pok_netobj_readinfo_process(info);
        if (result == pok_net_completed) {
            /* allocate sprite frame pointers */
            size_t i;
            sman->imagecnt = sman->spritecnt * 10;
            sman->spritesets = malloc(sizeof(struct pok_image*) * sman->imagecnt);
            if (sman->spritesets == NULL) {
                pok_exception_flag_memory_error();
                return pok_net_failed_internal;
            }
            for (i = 0;i < sman->imagecnt;++i)
                sman->spritesets[i] = NULL;
            /* allocate sprite sheet image */
            sman->_sheet = pok_image_new();
            if (sman->_sheet == NULL) {
                pok_exception_flag_memory_error();
                return pok_net_failed_internal;
            }
            /* setup info next */
            if ( !pok_netobj_readinfo_alloc_next(info) )
                return pok_net_failed_internal;
        }
    }
    if (info->fieldProg == 1) {
        result = pok_image_netread_ex(sman->_sheet,sman->sys->dimension * (uint32_t)10,sman->sys->dimension * (uint32_t)sman->spritecnt,
            dsrc,info->next);
        if (result == pok_net_completed) {
            if ( !pok_sprite_manager_from_data(sman,10*sman->spritecnt,sman->_sheet->pixels.data,TRUE) )
                return pok_net_failed_internal;
            if ( !pok_sprite_manager_assoc(sman) )
                return pok_net_failed_internal;
        }
    }
    return result;
}
