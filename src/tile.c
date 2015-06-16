/* tile.c - pokgame */
#include "tile.h"
#include "error.h"
#include "protocol.h"
#include <stdlib.h>

/* pok_tile */
void pok_tile_init(struct pok_tile* tile,uint16_t tileid)
{
    tile->data.tileid = tileid;
    /* most of these fields are ignored if warpKind==pok_tile_warp_none */
    tile->data.warpMap = 0;
    tile->data.warpChunk.X = 0;
    tile->data.warpChunk.Y = 0;
    tile->data.warpLocation.column = 0;
    tile->data.warpLocation.row = 0;
    tile->data.warpKind = pok_tile_warp_none;
    tile->impass = FALSE;
    tile->pass = FALSE;
}
void pok_tile_init_ex(struct pok_tile* tile,const struct pok_tile_data* tiledata)
{
    tile->data = *tiledata;
    tile->impass = FALSE;
    tile->pass = FALSE;
}
bool_t pok_tile_save(struct pok_tile* tile,struct pok_data_source* dsrc)
{
    /* fields:
        [2 bytes] id
        [1 byte] warp kind
        [4 bytes] warp map number
        [4 bytes] warp chunk X
        [4 bytes] warp chunk Y
        [2 bytes] warp location column
        [2 bytes] warp location row
    */
    return pok_data_stream_write_uint16(dsrc,tile->data.tileid)
        && pok_data_stream_write_byte(dsrc,tile->data.warpKind)
        && pok_data_stream_write_uint32(dsrc,tile->data.warpMap)
        && pok_data_stream_write_int32(dsrc,tile->data.warpChunk.X)
        && pok_data_stream_write_int32(dsrc,tile->data.warpChunk.Y)
        && pok_data_stream_write_uint16(dsrc,tile->data.warpLocation.column)
        && pok_data_stream_write_uint16(dsrc,tile->data.warpLocation.row);
}
bool_t pok_tile_open(struct pok_tile* tile,struct pok_data_source* dsrc)
{
    /* fields:
        [2 bytes] id
        [1 byte] warp kind
        [4 bytes] warp map number
        [4 bytes] warp chunk X
        [4 bytes] warp chunk Y
        [2 bytes] warp location column
        [2 bytes] warp location row
    */
    return pok_data_stream_read_uint16(dsrc,&tile->data.tileid)
        && pok_data_stream_read_byte(dsrc,&tile->data.warpKind)
        && pok_data_stream_read_uint32(dsrc,&tile->data.warpMap)
        && pok_data_stream_read_int32(dsrc,&tile->data.warpChunk.X)
        && pok_data_stream_read_int32(dsrc,&tile->data.warpChunk.Y)
        && pok_data_stream_read_uint16(dsrc,&tile->data.warpLocation.column)
        && pok_data_stream_read_uint16(dsrc,&tile->data.warpLocation.row);
}
enum pok_network_result pok_tile_netread(struct pok_tile* tile,struct pok_data_source* dsrc,struct pok_netobj_readinfo* info)
{
    /* fields:
        [2 bytes] id
        [1 byte]  warp kind
        (the remaining fields are only expected if warp kind != none)
        [4 bytes] warp map number
        [4 bytes] warp chunk X
        [4 bytes] warp chunk Y
        [2 bytes] warp column
        [2 bytes] warp row */
    enum pok_network_result result = pok_net_already;
    switch (info->fieldProg) {
    case 0:
        pok_data_stream_read_uint16(dsrc,&tile->data.tileid);
        if ((result = pok_netobj_readinfo_process(info)) != pok_net_completed)
            break;
    case 1:
        pok_data_stream_read_byte(dsrc,&tile->data.warpKind);
        if ((result = pok_netobj_readinfo_process(info)) != pok_net_completed)
            break;
        if (tile->data.warpKind >= pok_tile_warp_BOUND) {
            pok_exception_new_ex(pok_ex_tile,pok_ex_tile_bad_warp_kind);
            return pok_net_failed_protocol;
        }
    case 2:
        if (tile->data.warpKind != pok_tile_warp_none) {
            pok_data_stream_read_uint32(dsrc,&tile->data.warpMap);
            if ((result = pok_netobj_readinfo_process(info)) != pok_net_completed)
                break;
        }
        else {
            /* these values will be ignored since tile->warpKind==none */
            tile->data.warpMap = 0;
            tile->data.warpChunk.X = 0;
            tile->data.warpChunk.Y = 0;
            tile->data.warpLocation.column = 0;
            tile->data.warpLocation.row = 0;
            info->fieldProg = 5; /* skip other fields */
            return pok_net_completed;
        }
    case 3:
        pok_data_stream_read_int32(dsrc,&tile->data.warpChunk.X);
        if ((result = pok_netobj_readinfo_process(info)) != pok_net_completed)
            break;
    case 4:
        pok_data_stream_read_int32(dsrc,&tile->data.warpChunk.Y);
        if ((result = pok_netobj_readinfo_process(info)) != pok_net_completed)
            break;
    case 5:
        pok_data_stream_read_uint16(dsrc,&tile->data.warpLocation.column);
        if ((result = pok_netobj_readinfo_process(info)) != pok_net_completed)
            break;
    case 6:
        pok_data_stream_read_uint16(dsrc,&tile->data.warpLocation.row);
        if ((result = pok_netobj_readinfo_process(info)) != pok_net_completed)
            break;
    }
    return result;
}
