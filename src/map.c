/* map.c - pokgame */
#include "map.h"
#include "error.h"
#include "pok.h"
#include <stdlib.h>

/* structs used by the implementation */

struct chunk_insert_hint
{
    struct pok_location pos;
    struct pok_size dims;
    struct pok_map_chunk* column;
    struct pok_map_chunk* row;
};
static void chunk_insert_hint_init(struct chunk_insert_hint* hint,uint16_t cCnt,uint16_t rCnt)
{
    hint->pos.column = 0;
    hint->pos.row = 0;
    hint->dims.columns = cCnt;
    hint->dims.rows = rCnt;
    hint->column = NULL;
    hint->row = NULL;
}

/* pok_map_chunk */

static struct pok_map_chunk* pok_map_chunk_new(const struct pok_size* size)
{
    uint16_t i;
    struct pok_map_chunk* chunk;
    chunk = malloc(sizeof(struct pok_map_chunk));
    if (chunk == NULL) {
        pok_exception_flag_memory_error();
        return NULL;
    }
    chunk->data = malloc(sizeof(struct pok_tile*) * size->rows);
    if (chunk->data == NULL) {
        pok_exception_flag_memory_error();
        return NULL;
    }
    for (i = 0;i < size->rows;++i) {
        chunk->data[i] = malloc(sizeof(struct pok_tile) * size->columns);
        if (chunk->data[i] == NULL) {
            uint16_t j;
            for (j = 0;j < i;++j)
                free(chunk->data[j]);
            free(chunk->data);
            pok_exception_flag_memory_error();
            return NULL;
        }
    }
    for (i = 0;i < 4;++i)
        chunk->adjacent[i] = NULL;
    chunk->counter = 0;
    chunk->flags = pok_map_chunk_flag_none;
    pok_netobj_default_ex(&chunk->_base,pok_netobj_mapchunk);
    return chunk;
}
static struct pok_map_chunk* pok_map_chunk_new_ex(uint16_t rows,bool_t byref)
{
    /* this chunk only allocates space for pointers to row data and
       assumes the row data is allocated by another context; the caller
       can specify whether they want the chunk to destroy the row data
       using the byref flag */
    uint16_t i;
    struct pok_map_chunk* chunk;
    chunk = malloc(sizeof(struct pok_map_chunk));
    if (chunk == NULL) {
        pok_exception_flag_memory_error();
        return NULL;
    }
    chunk->data = malloc(sizeof(struct pok_tile*) * rows);
    for (i = 0;i < rows;++i)
        chunk->data[i] = NULL;
    for (i = 0;i < 4;++i)
        chunk->adjacent[i] = NULL;
    chunk->counter = 0;
    chunk->flags = byref ? pok_map_chunk_flag_byref : pok_map_chunk_flag_none;
    pok_netobj_default_ex(&chunk->_base,pok_netobj_mapchunk);
    return chunk;    
}
static void pok_map_chunk_free(struct pok_map_chunk* chunk,uint16_t rows)
{
    uint16_t i;
    if ((chunk->flags&pok_map_chunk_flag_byref) == 0)
        for (i = 0;i < rows;++i)
            free(chunk->data[i]);
    free(chunk->data);
    pok_netobj_delete(&chunk->_base);
    /* free adjacent chunks as well */
    for (i = 0;i < 4;++i) {
        if (chunk->adjacent[i] != NULL) {
            /* stop the chunk from trying to recursively free this chunk (we are already
               being freed in this context) */
            chunk->adjacent[i]->adjacent[pok_direction_opposite(i)] = NULL;
            pok_map_chunk_free(chunk->adjacent[i],rows);
        }
    }
    free(chunk);
}
static enum pok_network_result pok_map_chunk_netread(struct pok_map_chunk* chunk,struct pok_data_source* dsrc,
    struct pok_netobj_readinfo* info,struct pok_size* size)
{
    /* fields:
        [...]     superclass
        [1 bytes] flags
        [width*height*2 bytes] tile structures
    */
    enum pok_network_result result = pok_net_already;
    if (info->fieldProg == 0)
        result = pok_netobj_netread(&chunk->_base,dsrc,info);
    if (info->fieldProg == 1) {
        pok_data_stream_read_byte(dsrc,&chunk->flags);
        result = pok_netobj_readinfo_process(info);
        if (result==pok_net_completed && !pok_netobj_readinfo_alloc_next(info))
            return pok_net_failed_internal;
    }
    if (info->fieldProg == 2) {
        while (info->depth[0] < size->rows) {
            while (info->depth[1] < size->columns) {
                result = pok_tile_netread(chunk->data[info->depth[0]]+info->depth[1],dsrc,info->next);
                if (result != pok_net_completed)
                    goto fail;
                pok_netobj_readinfo_reset(info->next);
            }
            info->depth[1] = 0;
            ++info->depth[0];
        }
        ++info->fieldProg;
    }
fail:
    return result;
}
enum pok_network_result pok_map_chunk_netupdate(struct pok_map_chunk* chunk,struct pok_data_source* dsrc,
    const struct pok_netobj_readinfo* info)
{
    enum pok_network_result result = pok_net_already;

    return result;
}

/* pok_map */

struct pok_map* pok_map_new()
{
    struct pok_map* map;
    map = malloc(sizeof(struct pok_map));
    if (map == NULL) {
        pok_exception_flag_memory_error();
        return NULL;
    }
    pok_map_init(map);
    return map;
}
void pok_map_free(struct pok_map* map)
{
    pok_map_delete(map);
    free(map);
}
void pok_map_init(struct pok_map* map)
{
    map->chunk = NULL;
    map->origin = NULL;
    map->chunkSize.columns = map->chunkSize.rows = 0;
    map->pos.column = 0;
    map->pos.row = 0;
    map->flags = pok_map_flag_none;
}
void pok_map_delete(struct pok_map* map)
{
    map->chunk = NULL;
    pok_map_chunk_free(map->origin,map->chunkSize.rows);
    map->origin = NULL;
}
bool_t pok_map_save(struct pok_map* map,struct pok_data_source* dsrc)
{
    if (map->origin == NULL) {

    }
    return FALSE;
}
bool_t pok_map_open(struct pok_map* map,struct pok_data_source* dsrc)
{
    if (map->origin == NULL) {

    }
    return FALSE;
}
static void pok_map_insert_chunk(struct pok_map* map,struct pok_map_chunk* chunk,struct chunk_insert_hint* hint)
{
#ifdef POKGAME_DEBUG
    if (hint->pos.row >= hint->dims.rows)
        pok_error(pok_error_fatal,"map is already full in pok_map_insert_chunk");
#endif
    /* place the chunk into the map grid and "link" it in correctly with existing chunks; we assume the caller
       passes the same hint object in each call for the same map */
    if (hint->row == NULL) {
        hint->row = map->origin;
        hint->column = map->origin;
        ++hint->pos.column;
    }
    if (hint->pos.column >= hint->dims.columns) {
        /* move to next row */
        hint->row->adjacent[pok_direction_down] = chunk;
        hint->column = chunk;
        chunk->adjacent[pok_direction_up] = hint->row;
        hint->row = chunk;
        hint->pos.column = 0;
        ++hint->pos.row;
    }
    else {
        hint->column->adjacent[pok_direction_right] = chunk;
        if (hint->column->adjacent[pok_direction_up] != NULL) {
            chunk->adjacent[pok_direction_up] = hint->column->adjacent[pok_direction_up]->adjacent[pok_direction_right];
            chunk->adjacent[pok_direction_up]->adjacent[pok_direction_down] = chunk;
        }
        chunk->adjacent[pok_direction_left] = hint->column;
        hint->column = chunk;
        ++hint->pos.column;
    }
}
bool_t pok_map_load(struct pok_map* map,const struct pok_tile_data tiledata[],uint32_t columns,uint32_t rows)
{
    if (map->origin == NULL) {
        uint16_t i, j;
        struct pok_size lefttop, rightbottom;
        const struct pok_tile_data* td[3];
        struct chunk_insert_hint hint;
        map->mapSize = pok_util_compute_chunk_size(columns,rows,MAX_MAP_CHUNK_DIMENSION,&map->chunkSize,&lefttop,&rightbottom);
        chunk_insert_hint_init(&hint,map->mapSize.columns,map->mapSize.rows);
        td[0] = tiledata;
        for (i = 0;i < map->mapSize.rows;++i) {
            struct pok_size size;
            td[1] = td[0];
            for (j = 0;j < map->mapSize.columns;++j) {
                uint16_t k, l, m, n, o;
                struct pok_map_chunk* chunk = pok_map_chunk_new(&map->chunkSize);
                if (chunk == NULL)
                    return FALSE;
                td[2] = td[1];
                m = 0; n = 0; o = 0;
                /* black out unused rows */
                size.rows = map->chunkSize.rows;
                if (i == 0) {
                    for (;m < lefttop.rows;++m)
                        for (k = 0;k < map->chunkSize.columns;++k)
                            pok_tile_init(chunk->data[m]+k,0);
                    size.rows -= lefttop.rows;
                }
                if (i+1 == map->mapSize.rows) {
                    for (l = map->chunkSize.rows-rightbottom.rows;l < map->chunkSize.rows;++l)
                        for (k = 0;k < map->chunkSize.columns;++k)
                            pok_tile_init(chunk->data[l]+k,0);
                    size.rows -= rightbottom.rows;
                }
                /* black out unused columns */
                size.columns = map->chunkSize.columns;
                if (j == 0) {
                    for (;n < lefttop.columns;++n)
                        /* we have already blacked-out all the columns in the first 'm' rows */
                        for (k = m;k < map->chunkSize.rows;++k)
                            pok_tile_init(chunk->data[k]+n,0);
                    size.columns -= lefttop.columns;
                }
                if (j+1 == map->mapSize.columns) {
                    for (o = map->chunkSize.columns-rightbottom.columns;o < map->chunkSize.rows;++o)
                        /* we have already blacked-out all the columns in the last 'l' rows */
                        for (k = 0;k < l;++k)
                            pok_tile_init(chunk->data[k]+o,0);
                    size.columns -= rightbottom.columns;
                }
                o = n;
                for (k = 0;k < size.rows;++m,++k) {
                    for (l = 0;l < size.columns;++n,++l)
                        pok_tile_init_ex(chunk->data[m]+n,td[2]+l);
                    n = o;
                    td[2] += rows; /* move to next logical row in tile data */
                }
                /* advance to the next chunk in the row of chunks */
                td[1] += size.columns;
                /* add the chunk to the graph */
                if (map->origin == NULL) {
                    map->origin = chunk;
                    map->chunk = chunk;
                }
                else
                    pok_map_insert_chunk(map,chunk,&hint);
            }
            /* advance to the next row of chunks */
            td[0] += columns * size.rows;
        }
        return TRUE;
    }
    return FALSE;
}
enum pok_network_result pok_map_netread(struct pok_map* map,struct pok_data_source* dsrc,struct pok_netobj_readinfo* info)
{
    /* fields
        [2 bytes] map flags
        [2 bytes] chunk size width
        [2 bytes] chunk size height
        [2 bytes] number of chunks per row (columns)
        [2 bytes] number of chunks per column (rows)
        [n chunks]
          [...] netread chunk

        The peer is expected to send map chunks in row-major order corresponding to the
        dimensions sent in the fields before the map chunks.
    */
    uint16_t i;
    enum pok_network_result result = pok_net_already;
    if (info->fieldProg == 0) {
        pok_data_stream_read_uint16(dsrc,&map->flags);
        result = pok_netobj_readinfo_process(info);
    }
    if (info->fieldProg == 1) {
        pok_data_stream_read_uint16(dsrc,&map->chunkSize.columns);
        result = pok_netobj_readinfo_process(info);
    }
    if (info->fieldProg == 2) {
        pok_data_stream_read_uint16(dsrc,&map->chunkSize.rows);
        result = pok_netobj_readinfo_process(info);
    }
    if (info->fieldProg == 3) {
        pok_data_stream_read_uint16(dsrc,&info->depth[0]);
        result = pok_netobj_readinfo_process(info);
    }
    if (info->fieldProg == 4) {
        pok_data_stream_read_uint16(dsrc,&info->depth[1]);
        result = pok_netobj_readinfo_process(info);
    }
    if (info->fieldProg == 5) {
        if (info->aux == NULL) {
            /* we need to store a hint to help the insert method set up the graph of
               chunks; this will be automatically deallocated by 'info' upon delete */
            struct chunk_insert_hint* hint = malloc(sizeof(struct chunk_insert_hint));
            chunk_insert_hint_init(hint,info->depth[0],info->depth[1]); /* info->depth has initial dimensions for chunk grid */
            info->aux = hint;
        }
        while (info->depth[1]) {
            info->fieldCnt = info->depth[0];
            while (info->fieldCnt) {
                if (map->chunk == NULL) {
                    map->chunk = pok_map_chunk_new(&map->chunkSize);
                    if (!pok_netobj_readinfo_alloc_next(info))
                        return pok_net_failed_internal;
                }
                result = pok_map_chunk_netread(map->chunk,dsrc,info->next,&map->chunkSize);
                if (result != pok_net_completed)
                    goto fail;
                if (map->origin == NULL)
                    map->origin = map->chunk;
                else
                    pok_map_insert_chunk(map,map->chunk,(struct chunk_insert_hint*)info->aux);
                map->chunk = NULL; /* reset so that next iteration generates new chunk */
                --info->fieldCnt;
            }
            --info->depth[1];
        }
        /* all chunks successfully read here; set start chunk to origin */
        map->chunk = map->origin;
    fail: ;
    }
    return result;
}
bool_t pok_map_center_on(struct pok_map* map,const struct pok_location* location)
{
    int d[2];
    bool_t flag;
    enum pok_direction dirs[2];
    struct pok_location pos;
    struct pok_map_chunk* chunk;
    /* find the chunk that bounds 'location'; it may not exist, in which case the
       map is not updated and the function returns FALSE */
    pos = map->pos;
    chunk = map->chunk;
    /* find direction that needs to be moved in each dimension */
    d[0] = pos.column > location->column
        ? (dirs[0] = pok_direction_left, -map->chunkSize.columns) : (dirs[0] = pok_direction_right, map->chunkSize.columns);
    d[1] = pos.row > location->row
        ? (dirs[1] = pok_direction_up, -map->chunkSize.rows) : (dirs[1] = pok_direction_down, map->chunkSize.rows);
    do {
        flag = FALSE;
        if (pok_unsigned_diff(location->column,pos.column) >= map->chunkSize.columns) {
            pos.column += d[0];
            chunk = chunk->adjacent[dirs[0]];
            flag = TRUE;
        }
        if (pok_unsigned_diff(location->row,pos.row) >= map->chunkSize.rows) {
            pos.row += d[1];
            chunk = chunk->adjacent[dirs[1]];
            flag = TRUE;
        }
        /* we pushed outside the map */
        if (chunk == NULL)
            return FALSE;
    } while (flag);
    map->pos = pos;
    map->chunk = chunk;
    return TRUE;
}
bool_t pok_map_check_for_chunks(struct pok_map* map,const struct pok_location* location)
{
    return FALSE;
}
