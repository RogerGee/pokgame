/* tile.c - pokgame */
#include "tile.h"
#include "error.h"
#include "protocol.h"
#include <stdlib.h>

/* pok_tile */
void pok_tile_init(struct pok_tile* tile,uint16_t tileid)
{
    tile->data.tileid = tileid;
    tile->data.warpMap = 0; /* this field doesn't matter if warpKind==pok_tile_warp_none */
    tile->data.warpLocation.column = 0;
    tile->data.warpLocation.row = 0;
    tile->data.warpKind = pok_tile_warp_none;
    tile->impass = FALSE;
}
void pok_tile_init_ex(struct pok_tile* tile,const struct pok_tile_data* tiledata)
{
    tile->data = *tiledata;
    tile->impass = FALSE;
}
bool_t pok_tile_save(struct pok_tile* tile,struct pok_data_source* dsrc)
{
    /* fields:
        [2 bytes] id
        [1 byte] warp kind
        [2 bytes] warp map
        [2 bytes] warp column
        [2 bytes] warp row
        [1 byte] impassable
    */
    return pok_data_stream_write_uint16(dsrc,tile->data.tileid)
        && pok_data_stream_write_byte(dsrc,tile->data.warpKind)
        && pok_data_stream_write_uint32(dsrc,tile->data.warpMap)
        && pok_data_stream_write_uint16(dsrc,tile->data.warpLocation.column)
        && pok_data_stream_write_uint16(dsrc,tile->data.warpLocation.row)
        && pok_data_stream_write_byte(dsrc,tile->impass);
}
bool_t pok_tile_open(struct pok_tile* tile,struct pok_data_source* dsrc)
{
    /* fields:
        [2 bytes] id
        [1 byte] warp kind
        [4 bytes] warp map number
        [2 bytes] warp column
        [2 bytes] warp row
        [1 byte] impassable
    */
    return pok_data_stream_read_uint16(dsrc,&tile->data.tileid)
        && pok_data_stream_read_byte(dsrc,&tile->data.warpKind)
        && pok_data_stream_read_uint32(dsrc,&tile->data.warpMap)
        && pok_data_stream_read_uint16(dsrc,&tile->data.warpLocation.column)
        && pok_data_stream_read_uint16(dsrc,&tile->data.warpLocation.row)
        && pok_data_stream_read_byte(dsrc,&tile->impass);
}
enum pok_network_result pok_tile_netread(struct pok_tile* tile,struct pok_data_source* dsrc,struct pok_netobj_readinfo* info)
{
    /* fields:
        [2 bytes] id
        [1 byte]  impassable
        [1 byte]  warp kind
        (the remaining fields are only expected if warp kind != none)
        [4 bytes] warp map number
        [2 bytes] warp column
        [2 bytes] warp row */
    enum pok_network_result result = pok_net_already;
    if (info->fieldProg == 0) {
        pok_data_stream_read_uint16(dsrc,&tile->data.tileid);
        result = pok_netobj_readinfo_process(info);
    }
    if (info->fieldProg == 1) {
        pok_data_stream_read_byte(dsrc,&tile->impass);
        result = pok_netobj_readinfo_process(info);
    }
    if (info->fieldProg == 2) {
        pok_data_stream_read_byte(dsrc,&tile->data.warpKind);
        result = pok_netobj_readinfo_process(info);
        if (result==pok_net_completed && tile->data.warpKind>=pok_tile_warp_BOUND) {
            pok_exception_new_ex(pok_ex_tile,pok_ex_tile_bad_warp_kind);
            return pok_net_failed_protocol;
        }
    }
    if (info->fieldProg == 3) {
        if (tile->data.warpKind != pok_tile_warp_none) {
            pok_data_stream_read_uint32(dsrc,&tile->data.warpMap);
            result = pok_netobj_readinfo_process(info);
        }
        else {
            tile->data.warpMap = 0; /* value doesn't matter since tile->warpKind==none */
            tile->data.warpLocation.column = 0;
            tile->data.warpLocation.row = 0;
            info->fieldProg = 5; /* skip other fields */
            return pok_net_completed;
        }
    }
    if (info->fieldProg == 4) {
        pok_data_stream_read_uint16(dsrc,&tile->data.warpLocation.column);
        result = pok_netobj_readinfo_process(info);
    }
    if (info->fieldProg == 5) {
        pok_data_stream_read_uint16(dsrc,&tile->data.warpLocation.row);
        result = pok_netobj_readinfo_process(info);
    }
    return result;
}
