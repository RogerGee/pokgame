/* tileman.c - pokgame */
#include "tileman.h"
#include "error.h"
#include <stdlib.h>

/* pok_tile_manager */
struct pok_tile_manager* pok_tile_manager_new(const struct pok_graphics_subsystem* sys)
{
    struct pok_tile_manager* tman = malloc(sizeof(struct pok_tile_manager));
    if (tman == NULL) {
        pok_exception_flag_memory_error();
        return NULL;
    }
    pok_tile_manager_init(tman,sys);
    return tman;
}
void pok_tile_manager_free(struct pok_tile_manager* tman)
{
    pok_tile_manager_delete(tman);
    free(tman);
}
void pok_tile_manager_init(struct pok_tile_manager* tman,const struct pok_graphics_subsystem* sys)
{
    tman->flags = pok_tile_manager_flag_none;
    tman->sys = sys; /* store a reference to the subsystem; we do not own this object */
    tman->tilecnt = 0;
    tman->impassibility = 1; /* black tile is impassable, everything else passable */
    tman->tileset = NULL;
    tman->tileani = NULL;
    tman->waterTilesCnt = 0;
    tman->waterTiles = NULL;
    tman->lavaTilesCnt = 0;
    tman->lavaTiles = NULL;
    tman->waterfallTilesCnt = 0;
    tman->waterfallTiles = NULL;
    tman->_sheet = NULL;
}
void pok_tile_manager_delete(struct pok_tile_manager* tman)
{
    if (tman->tilecnt > 0) {
        uint16_t i;
        /* start at index 1 since the first tile is not owned by this manager; it is
           the black tile owned by the graphics subsystem */
        for (i = 1;i < tman->tilecnt;++i)
            if (tman->tileset[i] != NULL)
                pok_image_free(tman->tileset[i]);
        free(tman->tileset);
    }
    if (tman->tileani!=NULL && (tman->flags & pok_tile_manager_flag_ani_byref) == 0)
        free(tman->tileani);
    if (tman->waterTiles != NULL)
        free(tman->waterTiles);
    if (tman->lavaTiles != NULL)
        free(tman->lavaTiles);
    if (tman->waterfallTiles != NULL)
        free(tman->waterfallTiles);
    if (tman->_sheet != NULL)
        pok_image_free(tman->_sheet);
}
static void pok_tile_manager_compute_ani_ticks(struct pok_tile_manager* tman)
{
    /* compute total ticks involved in a tile animation's indirections */
    uint16_t i, j;
    struct pok_tile_ani_data* ani = tman->tileani;
    for (i = 0;i < tman->tilecnt;++i,++ani) {
        bool_t dir = TRUE;
        ani->totalTicks = 0;
        if (ani->ticks > 0) {
            struct pok_tile_ani_data* walker;
            j = tman->tilecnt; /* limit number of indirections */
            /* walk through indirections */
            walker = ani;
            do {
                ani->totalTicks += walker->ticks;
                if (dir) {
                    if (walker->forward == 0)
                        dir = FALSE;
                    else if (walker != ani && walker->forward == ani->forward)
                        break;
                }
                else if (walker->backward == 0 || walker->backward == ani->backward)
                    break;
                walker = tman->tileani + (dir ? walker->forward : walker->backward);
                --j;
            } while (j > 0);
        }
    }
}
bool_t pok_tile_manager_save(struct pok_tile_manager* tman,struct pok_data_source* dsrc)
{
    /* fields:
        [2 bytes] number of tiles
        [2 bytes] impassable tiles cutoff
        [n bytes] tile images
        [1 byte]  has tile animation info if non-zero
        [n bytes] optional tile animation info (if previous byte was non-zero)
           (for each animation info structure)
           [1 byte] ani ticks
           [2 bytes] forward tile id
           [2 bytes] backward tile id
        [2 bytes] number of water tiles
        [n bytes] water tile ids
        [2 bytes] number of lava tiles
    */
    if (tman->tileset != NULL) {
        uint16_t i;
        if (!pok_data_stream_write_uint16(dsrc,tman->tilecnt) || !pok_data_stream_write_uint16(dsrc,tman->impassibility))
            return FALSE;
        for (i = 0;i < tman->tilecnt;++i)
            if ( !pok_image_save(tman->tileset[i],dsrc) )
                return FALSE;
        if ( !pok_data_stream_write_byte(dsrc,tman->tileani != NULL) )
            return FALSE;
        if (tman->tileani != NULL)
            for (i = 0;i < tman->tilecnt;++i)
                if (!pok_data_stream_write_byte(dsrc,tman->tileani[i].ticks) || !pok_data_stream_write_uint16(dsrc,tman->tileani[i].forward)
                    || !pok_data_stream_write_uint16(dsrc,tman->tileani[i].backward))
                    return FALSE;
        return TRUE;
    }
    return FALSE;
}
bool_t pok_tile_manager_open(struct pok_tile_manager* tman,struct pok_data_source* dsrc)
{
    /* fields:
        [2 bytes] number of tiles
        [2 bytes] impassable tiles cutoff
        [n bytes] tile images
        [1 byte]  has tile animation info if non-zero
        [n bytes] optional tile animation info (if previous byte was non-zero)
           (for each animation info structure)
           [1 byte] ani ticks
           [2 bytes] forward tile id
           [2 bytes] backward tile id
    */
    if (tman->tileset == NULL) {
        uint16_t i;
        bool_t hasAni;
        if ( !pok_data_stream_read_uint16(dsrc,&tman->tilecnt) )
            return FALSE;
        if (tman->tilecnt == 0) {
            pok_exception_new_ex(pok_ex_tileman,pok_ex_tileman_zero_tiles);
            return FALSE;
        }
        if ( !pok_data_stream_read_uint16(dsrc,&tman->impassibility) )
            return FALSE;
        tman->tileset = malloc(sizeof(struct pok_image*) * tman->tilecnt);
        if (tman->tileset == NULL) {
            pok_exception_flag_memory_error();
            return FALSE;
        }
        for (i = 0;i < tman->tilecnt;++i) {
            tman->tileset[i] = pok_image_new();
            if (tman->tileset[i]==NULL ||  !pok_image_open(tman->tileset[i],dsrc))
                return FALSE;
        }
        if ( !pok_data_stream_read_byte(dsrc,&hasAni) )
            return FALSE;
        if (hasAni) {
            tman->tileani = malloc(sizeof(struct pok_tile_ani_data) * tman->tilecnt);
            if (tman->tileani == NULL) {
                pok_exception_flag_memory_error();
                return FALSE;
            }
            for (i = 0;i < tman->tilecnt;++i)
                if (!pok_data_stream_read_byte(dsrc,&tman->tileani[i].ticks) || !pok_data_stream_read_uint16(dsrc,&tman->tileani[i].forward)
                    || !pok_data_stream_read_uint16(dsrc,&tman->tileani[i].backward))
                    return FALSE;
            pok_tile_manager_compute_ani_ticks(tman);
        }
        return TRUE;
    }
    return FALSE;
}
static bool_t pok_tile_manager_from_data(struct pok_tile_manager* tman,uint16_t imgc,const byte_t* data,bool_t byRef)
{
    uint16_t i;
    /* load tiles from data: the first tile (black tile) is already set */
    for (i = 1;i <= imgc;++i) {
        struct pok_image* img;
        if (byRef)
            img = pok_image_new_byref_rgb(tman->sys->dimension,tman->sys->dimension,data);
        else
            img = pok_image_new_byval_rgb(tman->sys->dimension,tman->sys->dimension,data);
        if (img == NULL)
            return FALSE;
        data += img->width * img->height * sizeof(union pixel);
        tman->tileset[i] = img;
    }
    return TRUE;
}
bool_t pok_tile_manager_load_tiles(struct pok_tile_manager* tman,uint16_t imgc,uint16_t impassibility,const byte_t* data,bool_t byRef)
{
    /* read tile data from memory; since tile images must match the 'dimension' value, the data is
       assumed to be of the correct length for the given value of 'imgc'; the pixel data also should
       not have an alpha channel */
    if (tman->tileset != NULL) {
        pok_exception_new_ex(pok_ex_tileman,pok_ex_tileman_already);
        return FALSE;
    }
    tman->tilecnt = imgc+1;
    tman->impassibility = impassibility;
    tman->tileset = malloc(tman->tilecnt * sizeof(struct pok_image*));
    if (tman->tileset == NULL) {
        pok_exception_flag_memory_error();
        return FALSE;
    }
    /* the black tile is always the first tile */
    tman->tileset[0] = tman->sys->blacktile;
    /* load the other tiles */
    return pok_tile_manager_from_data(tman,imgc,data,byRef);
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
    pok_tile_manager_compute_ani_ticks(tman);
    return TRUE;
}
bool_t pok_tile_manager_fromfile_tiles(struct pok_tile_manager* tman,const char* file)
{
    struct pok_image* img;
    if (tman->_sheet != NULL || tman->tileset != NULL) {
        pok_exception_new_ex(pok_ex_tileman,pok_ex_tileman_already);
        return FALSE;
    }
    img = pok_image_new();
    if (img == NULL)
        return FALSE;
    if ( !pok_image_fromfile_rgb(img,file) ) {
        pok_image_free(img);
        return FALSE;
    }
    /* check image dimensions; it must specify complete tiles (and at least 1) */
    if (img->width != tman->sys->dimension || img->height < tman->sys->dimension || img->height % tman->sys->dimension != 0) {
        pok_exception_new_ex(pok_ex_tileman,pok_ex_tileman_bad_image_dimension);
        pok_image_free(img);
        return FALSE;
    }
    /* load tile images by reference (set impassibility to default value for now) */
    if ( !pok_tile_manager_load_tiles(tman,img->height / tman->sys->dimension,0,img->pixels.data,TRUE) ) {
        pok_image_free(img);
        return FALSE;
    }
    tman->_sheet = img;
    return TRUE;
}
static enum pok_network_result pok_tile_ani_data_netread(struct pok_tile_ani_data* ani,struct pok_data_source* dsrc,
    struct pok_netobj_readinfo* info)
{
    /* fields:
       [1 byte] ticks
       [2 bytes] forward tile id
       [2 bytes] backward tile id
    */
    enum pok_network_result result = pok_net_already;
    switch (info->fieldProg) {
    case 0:
        pok_data_stream_read_byte(dsrc,&ani->ticks);
        if ((result = pok_netobj_readinfo_process(info)) != pok_net_completed)
            break;
    case 1:
        pok_data_stream_read_uint16(dsrc,&ani->forward);
        if ((result = pok_netobj_readinfo_process(info)) != pok_net_completed)
            break;
    case 2:
        pok_data_stream_read_uint16(dsrc,&ani->backward);
        if ((result = pok_netobj_readinfo_process(info)) != pok_net_completed)
            break;
    }
    return result;
}
static enum pok_network_result pok_tile_manager_netread_ani(struct pok_tile_manager* tman,struct pok_data_source* dsrc,
    struct pok_netobj_readinfo* info)
{
    /* read tile animation data substructure from data source:
        [2 bytes] animation data item count
        [n bytes] animation data items */
    enum pok_network_result result = pok_net_already;
    switch (info->fieldProg) {
    case 0:
        /* note: we read this just to ensure that the peer will send the
           correct number of substructures since that number is implied and
           also to determine if the peer wants to send animation data */
        pok_data_stream_read_uint16(dsrc,&info->fieldCnt);
        if ((result = pok_netobj_readinfo_process(info)) != pok_net_completed)
            break;
        if (info->fieldCnt == 0) {
            /* this means that the animation data is not to be specified */
            info->fieldProg = 2;
            return pok_net_completed;
        }
        if (info->fieldCnt != tman->tilecnt) {
            /* not enough animation substructures were specified */
            pok_exception_new_ex(pok_ex_tileman,pok_ex_tileman_too_few_ani);
            return pok_net_failed_protocol;
        }
        tman->tileani = malloc(sizeof(struct pok_tile_ani_data) * info->fieldCnt);
        if (tman->tileani == NULL) {
            pok_exception_flag_memory_error();
            return pok_net_failed_internal;
        }
        info->depth[0] = 0; /* use this to iterate through fields */
    case 1:
        do {
            if ( !pok_netobj_readinfo_alloc_next(info) )
                return pok_net_failed_internal;
            result = pok_tile_ani_data_netread(tman->tileani+info->depth[0],dsrc,info->next);
            if (result != pok_net_completed)
                break;
            ++info->depth[0];
            --info->fieldCnt;
        } while (info->fieldCnt > 0);
        if (info->fieldCnt == 0) {
            pok_tile_manager_compute_ani_ticks(tman);
            ++info->fieldProg;
        }
    }
    return result;
}
static enum pok_network_result pok_tile_manager_netread_special_tiles(uint16_t* tileListCnt,
    uint16_t** tileList,
    struct pok_data_source* dsrc,
    struct pok_netobj_readinfo* info)
{
    /* read special tile lists from data source:
        [2 bytes] number of tiles to enumerate (if 0 then skip the remaining fields)
        [n bytes] the specified number of tile ids (each is 2 bytes)
    */
    enum pok_network_result result = pok_net_already;
    switch (info->fieldProg) {
    case 0:
        pok_data_stream_read_uint16(dsrc,tileListCnt);
        if ((result = pok_netobj_readinfo_process(info)) != pok_net_completed)
            break;
        *tileList = malloc(sizeof(uint16_t) * *tileListCnt);
        if (*tileList == NULL) {
            pok_exception_flag_memory_error();
            return pok_net_failed_internal;
        }
    case 1:
        for (info->fieldCnt = 0;info->fieldCnt < *tileListCnt;++info->fieldCnt) {
            pok_data_stream_read_uint16(dsrc,*tileList + info->fieldCnt);
            if ((result = pok_netobj_readinfo_process(info)) != pok_net_completed)
                break;
        }
    }
    return result;
}
enum pok_network_result pok_tile_manager_netread(struct pok_tile_manager* tman,struct pok_data_source* dsrc,
    struct pok_netobj_readinfo* info)
{
    /* read tile info substructure from data source:
        [2 bytes] number of tiles
        [2 bytes] impassibility cutoff
        [n bytes] pok image containing the specified number of tiles; the image's size
                  is assumed from the number of tiles (width=dimension height=dimension*number-of-tiles)
        [n bytes] tile animation data (optional if user sends zero as first field)
        [n bytes] water tiles (optional if user sends zero as first field)
        [n bytes] lava tiles (optional if user sends zero as first field)
        [n bytes] waterfall tiles (optional if user sends zero as first field)
    */
    size_t i;
    enum pok_network_result result = pok_net_already;
    switch (info->fieldProg) {
    case 0:
        /* number of tiles */
        pok_data_stream_read_uint16(dsrc,&tman->tilecnt);
        if ((result = pok_netobj_readinfo_process(info)) != pok_net_completed)
            break;
        ++tman->tilecnt; /* account for first black tile */
        tman->tileset = malloc(sizeof(struct pok_image*) * tman->tilecnt);
        if (tman->tileset == NULL) {
            pok_exception_flag_memory_error();
            return pok_net_failed_internal;
        }
        /* initialize image substructures; the first tile is always set to the black tile */
        tman->tileset[0] = tman->sys->blacktile;
        tman->_sheet = pok_image_new();
        if (tman->_sheet == NULL) {
            pok_exception_flag_memory_error();
            return pok_net_failed_internal;
        }
        for (i = 1;i < tman->tilecnt;++i)
            tman->tileset[i] = NULL;
    case 1:
        /* impassibility value */
        pok_data_stream_read_uint16(dsrc,&tman->impassibility);
        if ((result = pok_netobj_readinfo_process(info)) != pok_net_completed)
            break;
        if ( !pok_netobj_readinfo_alloc_next(info) )
            return pok_net_failed_internal;
    case 2: /* image containing tile structures */
        /* netread a single image that contains all the tiles */
        if ((result = pok_image_netread_ex(
                    tman->_sheet,
                    tman->sys->dimension,
                    (uint32_t)(tman->tilecnt-1)*tman->sys->dimension,
                    dsrc,
                    info->next)) != pok_net_completed)
            break;
            /* divide the tile sheet image into individual tile images; this takes up minimal memory since
               the individual images just refer to image data in the source sheet image; subtract 1 from
               tilecnt since we incremented it earlier to account for the black tile */
            if ( !pok_tile_manager_from_data(tman,tman->tilecnt-1,tman->_sheet->pixels.data,TRUE) )
                return pok_net_failed_internal;
            /* reset info next */
            if ( !pok_netobj_readinfo_alloc_next(info) )
                return pok_net_failed_internal;
    case 3:
        /* animation data */
        if ((result = pok_tile_manager_netread_ani(tman,dsrc,info->next)) != pok_net_completed)
            break;
        ++info->fieldProg;
        pok_netobj_readinfo_reset(info->next);
    case 4:
        /* water tiles */
        if ((result = pok_tile_manager_netread_special_tiles(&tman->waterTilesCnt,&tman->waterTiles,dsrc,info->next)) != pok_net_completed)
            break;
        ++info->fieldProg;
        pok_netobj_readinfo_reset(info->next);
    case 5:
        /* lava tiles */
        if ((result = pok_tile_manager_netread_special_tiles(&tman->lavaTilesCnt,&tman->lavaTiles,dsrc,info->next)) != pok_net_completed)
            break;
        ++info->fieldProg;
        pok_netobj_readinfo_reset(info->next);
    case 6:
        /* waterfall tiles */
        if ((result = pok_tile_manager_netread_special_tiles(&tman->waterfallTilesCnt,
                    &tman->waterfallTiles,dsrc,info->next)) != pok_net_completed)
            break;
        ++info->fieldProg;
    }
    return result;
}
struct pok_image* pok_tile_manager_get_tile(const struct pok_tile_manager* tman,uint16_t tileid,uint32_t aniticks)
{
    if (tileid >= tman->tilecnt)
        tileid = 0;
    /* find animation tile for specified tile based on 'aniframe'; note
       that tile animation is optional and must first be loaded */
    if (tman->tileani != NULL) {
        /* tile animation structures form a linked list of tileset position; a tile
           animation sequence oscillates back and forth */
        struct pok_tile_ani_data* ani = tman->tileani + tileid;
        if (ani->totalTicks > 0) {
            bool_t dir = TRUE;
            uint32_t rem = aniticks % ani->totalTicks;
            while (rem >= ani->ticks) {
                struct pok_tile_ani_data* prev;
                if (dir && ani->forward == 0)
                    dir = FALSE;
                prev = ani;
                tileid = dir ? ani->forward : ani->backward;
                ani = tman->tileani + tileid;
                rem -= prev->ticks;
            }
        }
    }
    return tman->tileset[tileid];
}
