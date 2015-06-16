/* spriteman.c - pokgame */
#include "spriteman.h"
#include "error.h"
#include "protocol.h"
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
    sman->flags = pok_sprite_manager_no_alt;
    sman->spritecnt = 0;
    sman->imagecnt = 0;
    sman->spritesets = NULL;
    sman->spriteassoc = NULL;
    sman->flySprite = 0;
    sman->surfSprite = 0;
    sman->lavaSprite = 0;
    sman->_sheet = NULL;
}
void pok_sprite_manager_delete(struct pok_sprite_manager* sman)
{
    if (sman->imagecnt > 0 && sman->spritesets != NULL) {
        uint16_t i;
        struct pok_image* old = NULL;
        for (i = 0;i < sman->imagecnt;++i) {
            if (sman->spritesets[i] != old) {
                pok_image_free(sman->spritesets[i]);
                old = sman->spritesets[i];
            }
        }
        free(sman->spritesets);
    }
    if (sman->spriteassoc != NULL)
        free(sman->spriteassoc);
    if (sman->_sheet != NULL)
        pok_image_free(sman->_sheet);
}
static bool_t pok_sprite_manager_assoc(struct pok_sprite_manager* sman)
{
    /* configure the sprite association: there are 12 frames for sprites (some may
       be duplicates depending on the configuration) */
    uint16_t i;
    if (sman->spriteassoc != NULL)
        free(sman->spriteassoc);
    sman->spriteassoc = malloc(sizeof(struct pok_image**) * sman->spritecnt);
    if (sman->spriteassoc == NULL) {
        pok_exception_flag_memory_error();
        return FALSE;
    }
    for (i = 0;i < sman->spritecnt;++i)
        sman->spriteassoc[i] = sman->spritesets + i*12;
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
static bool_t pok_sprite_manager_from_data(struct pok_sprite_manager* sman,const byte_t* data,bool_t byRef)
{
    uint16_t i;
    size_t length;
    struct pok_image* img = NULL;
    length = sman->sys->dimension * sman->sys->dimension * sizeof(union alpha_pixel);
    for (i = 0;i < sman->imagecnt;++i) {
        uint16_t r = i % 12;
        if (((r != sprite_up_ani2 && r != sprite_down_ani2) || (sman->flags & pok_sprite_manager_updown_alt))
            && ((r != sprite_left_ani2 && r != sprite_right_ani2) || (sman->flags & pok_sprite_manager_leftright_alt)))
        {
            if (byRef)
                img = pok_image_new_byref_rgba(sman->sys->dimension,sman->sys->dimension,data);
            else
                img = pok_image_new_byval_rgba(sman->sys->dimension,sman->sys->dimension,data);
            if (img == NULL)
                return FALSE;
            data += length;
        }
        /* else use the previous image */
        sman->spritesets[i] = img;
    }
    return TRUE;
}
bool_t pok_sprite_manager_load(struct pok_sprite_manager* sman,uint16_t flags,uint16_t spriteCnt,const byte_t* data,bool_t byRef)
{
    /* read sprite data from memory; since sprite images must match the 'sman->sys->dimension' value, the data is
       assumed to be of the correct length for the given value of 'flags' and 'spriteCnt' */
    if (sman->spritesets != NULL) {
        pok_exception_new_ex(pok_ex_spriteman,pok_ex_spriteman_already);
        return FALSE;
    }
    /* allocate frames */
    sman->flags = flags;
    sman->imagecnt = spriteCnt * 12;
    sman->spritecnt = spriteCnt;
    sman->spritesets = malloc(sman->imagecnt * sizeof(struct pok_image*));
    if (sman->spritesets == NULL) {
        pok_exception_flag_memory_error();
        return FALSE;
    }
    /* load frames */
    if ( !pok_sprite_manager_from_data(sman,data,byRef) )
        return FALSE;
    return pok_sprite_manager_assoc(sman);
}
bool_t pok_sprite_manager_fromfile(struct pok_sprite_manager* sman,const char* file,uint16_t flags)
{
    /* load the sprite images from a file; the user specifies flags to indicate how many
       frames to expect */
    uint16_t per;
    uint32_t hlen;
    struct pok_image* img;
    if (sman->_sheet != NULL || sman->spritesets != NULL) {
        pok_exception_new_ex(pok_ex_spriteman,pok_ex_spriteman_already);
        return FALSE;
    }
    img = pok_image_new();
    if (img == NULL)
        return FALSE;
    if ( !pok_image_fromfile_rgba(img,file) ) {
        pok_image_free(img);
        return FALSE;
    }
    /* check image dimensions; it must support at least a single sprite set with minimal requirements (8 frames) */
    per = 2 * (2 + ((flags & pok_sprite_manager_updown_alt) == 0)
            + 2 + ((flags & pok_sprite_manager_leftright_alt) == 0));
    hlen = per * sman->sys->dimension;
    if (img->width != sman->sys->dimension || img->height < hlen || img->height % hlen != 0) {
        pok_exception_new_ex(pok_ex_spriteman,pok_ex_spriteman_bad_image_dimension);
        pok_image_free(img);
        return FALSE;
    }
    /* tell the sprite manager to load up images by reference from the sprite sheet */
    if ( !pok_sprite_manager_load(sman,flags,img->height / hlen,img->pixels.data,TRUE) ) {
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
        [2 bytes] flags specifying sprite frame configuration
        [2 bytes] number of sprite sets (there are 12 sprites per set (some may be redundant depending on flags))
        [n bytes] pok image containing all sprites; the image's dimensions must be 10*dim by dim*number-of-sets
        [2 bytes] sprite id of fly sprite
        [2 bytes] sprite id of surf sprite
        [2 bytes] sprite id of lava sprite
    */
    size_t i;
    uint32_t actualImageCnt;
    enum pok_network_result result = pok_net_already;
    switch (info->fieldProg) {
    case 0:
        pok_data_stream_read_uint16(dsrc,&sman->flags);
        if ((result = pok_netobj_readinfo_process(info)) != pok_net_completed)
            break;
    case 1:
        /* number of sprite sets */
        pok_data_stream_read_uint16(dsrc,&sman->spritecnt);
        if ((result = pok_netobj_readinfo_process(info)) != pok_net_completed)
            break;
        /* allocate sprite frame pointers */
        sman->imagecnt = sman->spritecnt * 12;
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
    case 2:
        /* pok_image: the actual number of subimages is dependent on the flags the user sent; we
           can infer the image dimensions using them */
        actualImageCnt = 2 * (2 + ((sman->flags & pok_sprite_manager_updown_alt) == 0)
            + 2 + ((sman->flags & pok_sprite_manager_leftright_alt) == 0));
        if ((result = pok_image_netread_ex(
                    sman->_sheet,
                    sman->sys->dimension * actualImageCnt,
                    sman->sys->dimension * (uint32_t)sman->spritecnt,
                    dsrc,info->next)) != pok_net_completed)
            break;
        if ( !pok_sprite_manager_from_data(sman,sman->_sheet->pixels.data,TRUE) )
            return pok_net_failed_internal;
        if ( !pok_sprite_manager_assoc(sman) )
            return pok_net_failed_internal;
    case 3:
        /* fly sprite */
        pok_data_stream_read_uint16(dsrc,&sman->flySprite);
        if ((result = pok_netobj_readinfo_process(info)) != pok_net_completed)
            break;
    case 4:
        /* surf sprite */
        pok_data_stream_read_uint16(dsrc,&sman->surfSprite);
        if ((result = pok_netobj_readinfo_process(info)) != pok_net_completed)
            break;
    case 5:
        /* lava sprite */
        pok_data_stream_read_uint16(dsrc,&sman->lavaSprite);
        if ((result = pok_netobj_readinfo_process(info)) != pok_net_completed)
            break;
    }
    return result;
}
