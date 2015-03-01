/* tile.c - pokgame */
#include "tile.h"
#include "error.h"
#include <stdlib.h>

void pok_tile_manager_init(struct pok_tile_manager* tman,const struct pok_graphics_subsystem* sys)
{
    tman->flags = pok_tile_manager_flag_none;
    tman->sys = sys; /* store a reference to the subsystem; we do not own this object */
    tman->tilecnt = 0;
    tman->impassability = 1; /* black tile is impassable, everything else passable */
    tman->tileset = NULL;
    tman->tileani = NULL;
}
void pok_tile_manager_delete(struct pok_tile_manager* tman)
{
    if (tman->tilecnt > 0) {
        uint16_t i;
        for (i = 0;i < tman->tilecnt;++i)
            pok_image_free(tman->tileset[i]);
        free(tman->tileset);
    }
    if (tman->tileani!=NULL && (tman->flags & pok_tile_manager_flag_ani_byref))
        free(tman->tileani);
}
bool_t pok_tile_manager_load_tiles(struct pok_tile_manager* tman,uint16_t imgc,uint16_t impassability,byte_t* data,bool_t byRef)
{
    /* read tile data from memory; since tile images must match the 'dimension' value, the data is
       assumed to be of the correct length for the given value of 'imgc'; the pixel data also should
       not have an alpha channel */
    uint16_t i;
#ifdef POKGAME_DEBUG
    if (tman->tileset != NULL)
        pok_error(pok_error_fatal,"tileset is already loaded in pok_tile_manager_load_tiles()");
#endif
    tman->tilecnt = imgc+1;
    tman->impassability = impassability;
    tman->tileset = malloc(tman->tilecnt * sizeof(struct pok_image*));
    if (tman->tileset == NULL) {
        pok_exception_flag_memory_error();
        return FALSE;
    }
    /* the black tile is always the first tile */
    tman->tileset[0] = tman->sys->blacktile;
    /* load the other tiles */
    for (i = 1;i <= imgc;++i) {
        struct pok_image* img;
        if (byRef)
            img = pok_image_new_byref_rgb(tman->sys->dimension,tman->sys->dimension,data);
        else
            img = pok_image_new_byval_rgb(tman->sys->dimension,tman->sys->dimension,data);
        data += img->width * img->height * sizeof(union pixel);
        tman->tileset[i] = img;
    }
    return TRUE;
}
bool_t pok_tile_manager_load_ani(struct pok_tile_manager* tman,uint16_t anic,struct pok_tile_ani_data* data,bool_t byRef)
{
#ifdef POKGAME_DEBUG
    if (tman->tileani != NULL)
        pok_error(pok_error_fatal,"tile animation data is already loaded in pok_tile_manager_load_ani()");
    if (tman->tileset == NULL)
        pok_error(pok_error_fatal,"tile set must be loaded before call to pok_tile_manager_load_ani()");
    if (tman->tilecnt < anic)
        pok_error(pok_error_fatal,"too few animation data items passed to pok_tile_manager_load_ani()");
#endif
    if (byRef) {
        tman->tileani = data;
        tman->flags |= pok_tile_manager_flag_ani_byref;
    }
    else {
        uint16_t i;
        tman->tileani = malloc(sizeof(struct pok_tile_ani_data) * anic);
        if (tman->tileani == NULL) {
            pok_exception_flag_memory_error();
            return FALSE;
        }
        for (i = 0;i < anic;++i)
            tman->tileani[i] = data[i];
    }
    return TRUE;
}
enum pok_network_result pok_tile_manager_netread_tiles(struct pok_tile_manager* tman,struct pok_data_source* dsrc,
    struct pok_netobj_readinfo* info)
{
    /* read tile info substructure from data source:
        [2 bytes] number of tiles
        [2 bytes] impassability cutoff
        [n bytes] 'dimension x dimension' sized 'pok_image' structures */
    size_t i;
    enum pok_network_result result = pok_net_already;
    if (info->fieldProg == 0) {
        pok_data_stream_read_uint16(dsrc,&tman->tilecnt);
        result = pok_netobj_readinfo_process(info);
        if (result == pok_net_completed) {
            ++tman->tilecnt; /* account for first black tile */
            info->fieldCnt = tman->tilecnt-1; /* let field count hold number of image substructures to read */
            tman->tileset = malloc(sizeof(struct pok_image*) * tman->tilecnt);
            if (tman->tileset == NULL) {
                pok_exception_flag_memory_error();
                return pok_net_failed_internal;
            }
            /* initialize image substructures so that later calls can determine if allocated or not; the
               first tile is always set to the black tile */
            tman->tileset[0] = tman->sys->blacktile;
            for (i = 1;i < tman->tilecnt;++i)
                tman->tileset[i] = NULL;
        }
    }
    if (info->fieldProg == 1) {
        pok_data_stream_read_uint16(dsrc,&tman->impassability);
        result = pok_netobj_readinfo_process(info);
    }
    if (info->fieldProg == 2) {
        do {
            struct pok_image** imgp;
            imgp = tman->tileset + (tman->tilecnt - info->fieldCnt); /* grab ptr to image substructure */
            if (*imgp == NULL) { /* we have not attempted netread on this tile yet */
                *imgp = pok_image_new();
                if (info->next == NULL) {
                    if ((info->next = pok_netobj_readinfo_new()) == NULL)
                        return pok_net_failed_internal;
                }
                else {
                    pok_netobj_readinfo_delete(info->next); /* for correctness, we must call delete */
                    pok_netobj_readinfo_init(info->next);
                }
            }
            /* attempt to read from network; assume 'sys->dimension' is the image width/height */
            result = pok_image_netread_ex(*imgp,tman->sys->dimension,tman->sys->dimension,dsrc,info->next);
            if (result != pok_net_completed)
                break;
            --info->fieldCnt;
        } while (info->fieldCnt > 0);
        if (info->fieldCnt == 0)
            ++info->fieldProg;
    }
    return result;
}
static enum pok_network_result pok_tile_ani_data_netread(struct pok_tile_ani_data* ani,struct pok_data_source* dsrc,
    struct pok_netobj_readinfo* info)
{
    /* fields: [byte] ticks, [byte] indirections, [2-bytes] tile ID */
    enum pok_network_result result = pok_net_already;
    if (info->fieldProg == 0) {
        pok_data_stream_read_byte(dsrc,&ani->ticks);
        result = pok_netobj_readinfo_process(info);
    }
    if (info->fieldProg == 1) {
        pok_data_stream_read_byte(dsrc,&ani->indirc);
        result = pok_netobj_readinfo_process(info);
    }
    if (info->fieldProg == 2) {
        pok_data_stream_read_uint16(dsrc,&ani->tileid);
        result = pok_netobj_readinfo_process(info);
    }
    return result;
}
enum pok_network_result pok_tile_manager_netread_ani(struct pok_tile_manager* tman,struct pok_data_source* dsrc,struct pok_netobj_readinfo* info)
{
    /* read tile animation data substructure from data source:
        [2-bytes] animation data item count
        [4*n bytes] animation data items */
    enum pok_network_result result = pok_net_already;
    if (info->fieldProg == 0) {
        /* note: we read this just to ensure that the peer will send the
           correct number of substructures since that number is implied */
        pok_data_stream_read_uint16(dsrc,&info->fieldCnt);
        result = pok_netobj_readinfo_process(info);
        if (result == pok_net_completed) {
            if (info->fieldCnt < tman->tilecnt) {
                pok_exception_new_ex(pok_ex_tile,pok_ex_tile_domain_error);
                return pok_net_failed_protocol;
            }
            if ((info->next = pok_netobj_readinfo_new()) == NULL)
                return pok_net_failed_internal;
            tman->tileani = malloc(sizeof(struct pok_tile_ani_data) * info->fieldCnt);
            if (tman->tileani == NULL) {
                pok_exception_flag_memory_error();
                return pok_net_failed_internal;
            }
            info->depth[0] = 0; /* use this to iterate through fields */
        }
    }
    if (info->fieldProg == 1) {
        do {
            result = pok_tile_ani_data_netread(tman->tileani+info->depth[0],dsrc,info->next);
            if (result != pok_net_completed)
                break;
            ++info->depth[0];
            --info->fieldCnt;
            /* reset readinfo */
            pok_netobj_readinfo_delete(info->next);
            pok_netobj_readinfo_init(info->next);
        } while (info->fieldCnt > 0);
        if (info->fieldCnt == 0)
            ++info->fieldProg;
    }
    return result;
}
struct pok_image* pok_tile_manager_get_tile(struct pok_tile_manager* tman,uint16_t tileid,uint32_t aniticks)
{
    struct pok_image* img;
    if (tileid >= tman->tilecnt)
        tileid = 0;
    /* find animation tile for specified tile based on 'aniframe'; note
       that tile animation is optional and must first be loaded */
    if (tman->tileani!=NULL && aniticks>0) {
        struct pok_tile_ani_data* ani;
        /* think of the animation data as a linked list of tileset indeces; each
           animation sequence has some predetermined number of indirections; if
           this number is zero, then the tile does not animate; each tile animation
           has a 'ticks' value indicating the number of ticks before an indirection
           is applied; 'aniticks' must be some non-zero multiple of this value in
           order for an animation tile to be considered */
        ani = tman->tileani + tileid;
        if (ani->indirc>0 && ani->ticks>0 && aniticks%ani->ticks==0) {
            aniticks /= ani->ticks; /* number of times an indirection has been performed */
            aniticks %= ani->indirc; /* number of steps to get to desired indirection */
            while (aniticks > 0) { /* follow animation tiles to get desired tileid */
                ani = tman->tileani + ani->tileid;
                --aniticks;
            }
        }
        img = tman->tileset[ani->tileid];
    }
    else
        img = tman->tileset[tileid];
    return img;
}
