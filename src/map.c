/* map.c - pokgame */
#include "map.h"
#include "error.h"
#include "pok.h" /* gets protocol.h */
#include "parser.h"
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

struct chunk_adj_info
{
    uint16_t n;
    uint8_t a[MAX_INITIAL_CHUNKS];
    struct pok_map_chunk* c[MAX_INITIAL_CHUNKS];
};

struct chunk_recursive_info
{
    struct pok_data_source* dsrc;
    struct pok_size* chunkSize;
    bool_t complexTiles;
};

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
    chunk->flags = pok_map_chunk_flag_none;
    chunk->discov = FALSE;
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
    chunk->flags = byref ? pok_map_chunk_flag_byref : pok_map_chunk_flag_none;
    chunk->discov = FALSE;
    pok_netobj_default_ex(&chunk->_base,pok_netobj_mapchunk);
    return chunk;    
}
static void pok_map_chunk_free(struct pok_map_chunk* chunk,uint16_t rows)
{
    uint16_t i;
    if ((chunk->flags&pok_map_chunk_flag_byref) == 0 && chunk->data != NULL)
        for (i = 0;i < rows;++i)
            free(chunk->data[i]);
    free(chunk->data);
    pok_netobj_delete(&chunk->_base);
    /* recursively delete adjacent chunks */
    chunk->discov = TRUE;
    for (i = 0;i < 4;++i) {
        if (chunk->adjacent[i] != NULL) {
            /* destroy the reverse adjacency information */
            chunk->adjacent[i]->adjacent[ pok_direction_opposite(i) ] = NULL;
            if (!chunk->adjacent[i]->discov)
                pok_map_chunk_free(chunk->adjacent[i],rows);
        }
    }
    free(chunk);
}
static void pok_map_chunk_setstate(struct pok_map_chunk* chunk,bool_t state)
{
    int i;
    chunk->discov = state;
    for (i = 0;i < 4;++i)
        if (chunk->adjacent[i] != NULL && !(state ? chunk->adjacent[i]->discov : !chunk->adjacent[i]->discov))
            pok_map_chunk_setstate(chunk->adjacent[i],state);
}
static bool_t pok_map_chunk_save(struct pok_map_chunk* chunk,struct chunk_recursive_info* info)
{
    /* fields:
        [byte] bitmask specifying adjacencies to load (use 'enum pok_direction'+1 to mask)
        (recursively write the adjacencies)
        [n bytes] tile data
    */
    int i;
    uint8_t mask = 0x00;
    uint16_t r, c;
    /* compute and write adjacency mask */
    for (i = 0;i < 4;++i)
        if (chunk->adjacent[i] != NULL)
            mask |= 1 << i;
    if ( !pok_data_stream_write_byte(info->dsrc,mask) )
        return FALSE;
    /* recursively write non-visited adjacencies */
    for (i = 0;i < 4;++i) {
        if (chunk->adjacent[i] != NULL && !chunk->adjacent[i]->discov) {
            chunk->adjacent[i]->discov = TRUE;
            if ( !pok_map_chunk_save(chunk->adjacent[i],info) )
                return FALSE;
        }
    }
    /* write chunk data */
    for (r = 0;r < info->chunkSize->rows;++r) {
        for (c = 0;c < info->chunkSize->columns;++c) {
            if (info->complexTiles) {
                if ( !pok_tile_save(chunk->data[r] + c,info->dsrc) )
                    return FALSE;
            }
            else if ( !pok_data_stream_write_uint16(info->dsrc,chunk->data[r][c].data.tileid) )
                return FALSE;
        }
    }
    return TRUE;
}
static bool_t pok_map_chunk_open(struct pok_map_chunk* chunk,struct chunk_recursive_info* info)
{
    /* fields:
        [byte] bitmask specifying adjacencies to load (use 'enum pok_direction'+1 to mask)
        (recursively read the adjacencies)
        [n bytes] tile data

        This is a depth first way of reading chunk information. The representation is still a graph,
        but it does not have any cycles.
    */
    int i;
    byte_t adj;
    uint16_t c, r;
    /* read adjacency bitmask */
    if ( !pok_data_stream_read_byte(info->dsrc,&adj) )
        return FALSE;
    /* recursively create adjacencies */
    for (i = 0;i < 4;++i) {
        if ((adj & 1<<i) != 0) {
            int op = pok_direction_opposite(i),
                orthog1 = pok_direction_orthog1(i),
                orthog2 = pok_direction_orthog2(i);
            /* diagonals could imply existing adjacencies; for example:
                 X | Y
                 -----  if this is chunk X, then chunk Y or Z could already exist as they
                 Z | W  could have been created by W
            */
            if (chunk->adjacent[orthog1] != NULL && chunk->adjacent[orthog1]->adjacent[i] != NULL
                && chunk->adjacent[orthog1]->adjacent[i]->adjacent[orthog2] != NULL)
                chunk->adjacent[i] = chunk->adjacent[orthog1]->adjacent[i]->adjacent[orthog2];
            else if (chunk->adjacent[orthog2] != NULL && chunk->adjacent[orthog2]->adjacent[i] != NULL
                    && chunk->adjacent[orthog2]->adjacent[i]->adjacent[orthog1] != NULL)
                chunk->adjacent[i] = chunk->adjacent[orthog2]->adjacent[i]->adjacent[orthog1];
            if (chunk->adjacent[i] == NULL) {
                chunk->adjacent[i] = pok_map_chunk_new(info->chunkSize);
                chunk->adjacent[i]->adjacent[op] = chunk;
                if ( !pok_map_chunk_open(chunk->adjacent[i],info) )
                    return FALSE;
            }
            else
                chunk->adjacent[i]->adjacent[op] = chunk;
        }
    }
    /* base case: read chunk data */
    for (r = 0;r < info->chunkSize->rows;++r) {
        for (c = 0;c < info->chunkSize->columns;++c) {
            if (!info->complexTiles) {
                uint16_t id;
                if ( !pok_data_stream_read_uint16(info->dsrc,&id) )
                    return FALSE;
                pok_tile_init(chunk->data[r]+c,id);
            }
            else if ( !pok_tile_open(chunk->data[r]+c,info->dsrc) )
                return FALSE;
        }
    }
    return TRUE;
}
static enum pok_network_result pok_map_chunk_netread(struct pok_map_chunk* chunk,struct pok_data_source* dsrc,
    struct pok_netobj_readinfo* info,struct pok_size* size)
{
    /* fields:
        [...]     superclass
        [width*height*n bytes] tile structures
    */
    enum pok_network_result result = pok_net_already;
    if (info->fieldProg == 0) {
        result = pok_netobj_netread(&chunk->_base,dsrc,info);
        if (result==pok_net_completed && !pok_netobj_readinfo_alloc_next(info))
            return pok_net_failed_internal;
    }
    if (info->fieldProg == 1) {
        while (info->depth[0] < size->rows) {
            while (info->depth[1] < size->columns) {
                result = pok_tile_netread(chunk->data[info->depth[0]]+info->depth[1],dsrc,info->next);
                if (result != pok_net_completed)
                    goto fail;
                pok_netobj_readinfo_reset(info->next);
                ++info->depth[1];
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
    map->mapNo = 0;
    map->chunk = NULL;
    map->origin = NULL;
    map->chunkSize.columns = map->chunkSize.rows = 0;
    map->originPos.X = map->originPos.Y = 0;
    map->flags = pok_map_flag_none;
    pok_netobj_default_ex(&map->_base,pok_netobj_map);
}
void pok_map_delete(struct pok_map* map)
{
    pok_netobj_delete(&map->_base);
    map->chunk = NULL;
    if (map->origin != NULL) {
        pok_map_chunk_free(map->origin,map->chunkSize.rows);
        map->origin = NULL;
    }
}
bool_t pok_map_save(struct pok_map* map,struct pok_data_source* dsrc,bool_t complex)
{
    /* write the map representation to a file
        format:
        [1 byte] if non-zero then complex tile data is stored; otherwise simple
        [4 bytes] map number
        [2 bytes] chunk width
        [2 bytes] chunk height
        [4 bytes] origin chunk position X
        [4 bytes] origin chunk position Y
        [n bytes] origin chunk (and its adjacencies)
    */
    if (map->origin == NULL) {
        bool_t result;
        struct chunk_recursive_info info;
        info.dsrc = dsrc;
        info.chunkSize = &map->chunkSize;
        info.complexTiles = complex;
        if (!pok_data_stream_write_byte(dsrc,complex) || !pok_data_stream_write_uint32(dsrc,map->mapNo)
            || !pok_data_stream_write_uint16(dsrc,map->chunkSize.columns)
            || !pok_data_stream_write_uint16(dsrc,map->chunkSize.rows)
            || !pok_data_stream_write_int32(dsrc,map->originPos.X)
            || !pok_data_stream_write_int32(dsrc,map->originPos.Y))
            return FALSE;
        result = pok_map_chunk_save(map->origin,&info);
        pok_map_chunk_setstate(map->origin,FALSE); /* reset visit state for future operation */
        return result;
    }
    return FALSE;
}
bool_t pok_map_open(struct pok_map* map,struct pok_data_source* dsrc)
{
    /* read the map representation from a file (the representation produced by 'pok_map_save')
        format:
         [1 byte] if non-zero, then complex tile data is stored; otherwise simple tile data
         [4 bytes] map number
         [2 bytes] chunk width
         [2 bytes] chunk height
         [4 bytes] origin chunk position X
         [4 bytes] origin chunk position Y
         [n bytes] read origin chunk (and its adjacencies)
    */
    if (map->origin == NULL) {
        struct chunk_recursive_info info;
        /* read complex tile flag */
        if ( !pok_data_stream_read_byte(dsrc,&info.complexTiles) )
            return FALSE;
        /* read map number */
        if ( !pok_data_stream_read_uint32(dsrc,&map->mapNo) )
            return FALSE;
        /* read chunk dimensions */
        if (!pok_data_stream_read_uint16(dsrc,&map->chunkSize.columns) || !pok_data_stream_read_uint16(dsrc,&map->chunkSize.rows))
            return FALSE;
        /* ensure chunk dimensions are correct */
        if (map->chunkSize.columns == 0 || map->chunkSize.columns > MAX_MAP_CHUNK_DIMENSION
            || map->chunkSize.rows == 0 || map->chunkSize.rows > MAX_MAP_CHUNK_DIMENSION) {
            pok_exception_new_ex(pok_ex_map,pok_ex_map_bad_chunk_size);
            return FALSE;
        }
        /* read origin chunk position */
        if (!pok_data_stream_read_int32(dsrc,&map->originPos.X) || !pok_data_stream_read_int32(dsrc,&map->originPos.Y))
            return FALSE;
        /* read origin chunk; this will recursively read all chunks in the map */
        map->origin = pok_map_chunk_new(&map->chunkSize);
        if (map->origin == NULL)
            return FALSE;
        info.dsrc = dsrc;
        info.chunkSize = &map->chunkSize;
        if ( pok_map_chunk_open(map->origin,&info) ) {
            map->chunk = map->origin;
            /* measure the map by traversing from the origin */
            return TRUE;
        }
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
bool_t pok_map_load_simple(struct pok_map* map,const uint16_t tiledata[],uint32_t columns,uint32_t rows)
{
    /* load a map based on a rectangular tile configuration */
    if (map->origin == NULL) {
        uint16_t i, j;
        struct pok_size mapArea;
        struct pok_size lefttop, rightbottom;
        const uint16_t* td[3];
        struct chunk_insert_hint hint;
        mapArea = pok_util_compute_chunk_size(columns,rows,MAX_MAP_CHUNK_DIMENSION,&map->chunkSize,&lefttop,&rightbottom);
        chunk_insert_hint_init(&hint,mapArea.columns,mapArea.rows);
        td[0] = tiledata;
        for (i = 0;i < mapArea.rows;++i) {
            struct pok_size size;
            td[1] = td[0];
            for (j = 0;j < mapArea.columns;++j) {
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
                if (i+1 == mapArea.rows) {
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
                if (j+1 == mapArea.columns) {
                    for (o = map->chunkSize.columns-rightbottom.columns;o < map->chunkSize.rows;++o)
                        /* we have already blacked-out all the columns in the last 'l' rows */
                        for (k = 0;k < l;++k)
                            pok_tile_init(chunk->data[k]+o,0);
                    size.columns -= rightbottom.columns;
                }
                o = n;
                for (k = 0;k < size.rows;++m,++k) {
                    for (l = 0;l < size.columns;++n,++l)
                        pok_tile_init(chunk->data[m]+n,td[2][l]);
                    n = o;
                    td[2] += columns; /* move to next logical row in tile data */
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
bool_t pok_map_fromfile_space(struct pok_map* map,const char* filename)
{
    /* load space-separated tile data */
    if (map->origin == NULL) {
        bool_t result;
        struct pok_parser_info info;
        pok_parser_info_init(&info);
        info.dsrc = pok_data_source_new_file(filename,pok_filemode_open_existing,pok_iomode_read);
        if (info.dsrc == NULL)
            return FALSE;
        info.separator = ' ';
        result = pok_parse_map_simple(&info);
        if (result) {
            if (info.qwords[0] == 0 || info.qwords[1] == 0 || info.words_c[0] < info.qwords[0] * info.qwords[1]) {
                pok_exception_new_ex(pok_ex_map,pok_ex_map_bad_format);
                result = FALSE;
            }
            else
                result = pok_map_load_simple(map,info.words,info.qwords[0],info.qwords[1]);            
        }
        pok_data_source_free(info.dsrc);
        pok_parser_info_delete(&info);
        return result;
    }
    return FALSE;
}
bool_t pok_map_fromfile_csv(struct pok_map* map,const char* filename)
{
    /* load comma-separated tile data */
    if (map->origin == NULL) {
        bool_t result;
        struct pok_parser_info info;
        pok_parser_info_init(&info);
        info.dsrc = pok_data_source_new_file(filename,pok_filemode_open_existing,pok_iomode_read);
        if (info.dsrc == NULL)
            return FALSE;
        info.separator = ',';
        result = pok_parse_map_simple(&info);
        if (result) {
            if (info.qwords[0] == 0 || info.qwords[1] == 0 || info.words_c[0] != info.qwords[0] * info.qwords[1]) {
                pok_exception_new_ex(pok_ex_map,pok_ex_map_bad_format);
                result = FALSE;
            }
            else
                result = pok_map_load_simple(map,info.words,info.qwords[0],info.qwords[1]);
        }
        pok_data_source_free(info.dsrc);
        pok_parser_info_delete(&info);
        return result;
    }
    return FALSE;
}
static void pok_map_configure_adj(struct pok_map* map,struct chunk_adj_info* info)
{
    /* configure a map's chunks based on an adjacency list; the adjacency list specifies
       adjacencies in a breadth-first way; adjacencies may imply adjacencies not specified */
    int i, j, k;
    /* set origin chunk */
    map->chunk = map->origin = info->c[0];
    /* setup adjacencies */
    for (i = 0,j = 1;i < info->n;++i) {
        for (k = 0;k < 4;++k) {
            if (info->c[i]->adjacent[k] == NULL && (info->a[i] & (1 << k))) {
                int orthog1, orthog2, op;
                struct pok_map_chunk* chunk;
                op = pok_direction_opposite(k);
                orthog1 = pok_direction_orthog1(k);
                orthog2 = pok_direction_orthog2(k);
                /* see if the adjacency is implied by another preexisting adjacency; this should only be the
                   case if we have exhausted the list of unassigned chunks (j >= info->n) */
                if (info->c[i]->adjacent[orthog1] != NULL && info->c[i]->adjacent[orthog1]->adjacent[k] != NULL
                    && info->c[i]->adjacent[orthog1]->adjacent[k]->adjacent[orthog2] != NULL)
                    chunk = info->c[i]->adjacent[orthog1]->adjacent[k]->adjacent[orthog2];
                else if (info->c[i]->adjacent[orthog2] != NULL && info->c[i]->adjacent[orthog2]->adjacent[k] != NULL
                    && info->c[i]->adjacent[orthog2]->adjacent[k]->adjacent[orthog1] != NULL)
                    chunk = info->c[i]->adjacent[orthog2]->adjacent[k]->adjacent[orthog1];
                else
                    chunk = NULL;
                /* since the chunks are specified in a breadth-first manner, then this adjacency should refer to the
                   next assignable chunk in the list of unassigned chunks; if this list is exhausted then the previous
                   code should have found a chunk that was already assigned in a previous operation (chunk != NULL) */
                if (j < info->n) {
                    if (chunk != NULL) {
                        /* two chunks have been specified in the same place (peer made an error); delete the unassigned
                           chunk and continue (we want to handle this gracefully) */
                        pok_map_chunk_free(info->c[j++],map->chunkSize.rows);
                        continue;
                    }
                    chunk = info->c[j++];
                }
                else if (chunk == NULL) {
                    /* the peer should have specified a chunk in this direction, but they didn't; we gracefully
                       handle this problem by leaving the adjacency unassigned */
                    continue;
                }
                /* assign the adjacency; also assign the adjacency in the reverse direction */
                info->c[i]->adjacent[k] = chunk;
                chunk->adjacent[op] = info->c[i];
            }
        }
    }
}
enum pok_network_result pok_map_netread(struct pok_map* map,struct pok_data_source* dsrc,struct pok_netobj_readinfo* info)
{
    /* fields
        [super class]
        [2 bytes] map flags
        [4 bytes] map number
        [2 bytes] chunk size width
        [2 bytes] chunk size height
        [4 bytes] origin chunk position X
        [4 bytes] origin chunk position Y
        [1 byte] number of chunks
        [n bytes] chunk adjacency list (bitmask of 'enum pok_direction'+1 flags)
        [n chunks]
          [...] netread chunk

        The peer is expected to send less than or equal to MAX_INITIAL_CHUNKS. The chunks are preceeded by
        an adjacency list (of adjacency bitmasks). The list of adjacency bitmasks is parallel to the list of
        chunks
    */
    uint16_t i;
    struct chunk_adj_info* adj = (struct chunk_adj_info*) info->aux;
    enum pok_network_result result = pok_net_already;
    if (info->fieldProg == 0) /* super class */
        result = pok_netobj_netread(&map->_base,dsrc,info);
    if (info->fieldProg == 1) { /* map flags */
        pok_data_stream_read_uint16(dsrc,&map->flags);
        result = pok_netobj_readinfo_process(info);
    }
    if (info->fieldProg == 2) { /* map number */
        pok_data_stream_read_uint32(dsrc,&map->mapNo);
        result = pok_netobj_readinfo_process(info);
    }
    if (info->fieldProg == 3) { /* chunk size width */
        pok_data_stream_read_uint16(dsrc,&map->chunkSize.columns);
        result = pok_netobj_readinfo_process(info);
        if (result == pok_net_completed && 
            (map->chunkSize.columns == 0 || map->chunkSize.columns > MAX_MAP_CHUNK_DIMENSION)) {
            pok_exception_new_ex(pok_ex_map,pok_ex_map_bad_chunk_size);
            return pok_net_failed_protocol;
        }
    }
    if (info->fieldProg == 4) { /* chunk size height */
        pok_data_stream_read_uint16(dsrc,&map->chunkSize.rows);
        result = pok_netobj_readinfo_process(info);
        if (result == pok_net_completed && 
            (map->chunkSize.rows == 0 || map->chunkSize.rows > MAX_MAP_CHUNK_DIMENSION)) {
            pok_exception_new_ex(pok_ex_map,pok_ex_map_bad_chunk_size);
            return pok_net_failed_protocol;
        }
    }
    if (info->fieldProg == 5) { /* origin chunk position X */
        pok_data_stream_read_int32(dsrc,&map->originPos.X);
        result = pok_netobj_readinfo_process(info);
    }
    if (info->fieldProg == 6) { /* origin chunk position Y */
        pok_data_stream_read_int32(dsrc,&map->originPos.Y);
        result = pok_netobj_readinfo_process(info);
    }
    if (info->fieldProg == 7) { /* number of chunks */
        if (info->aux == NULL) {
            /* we need to store chunk information in a separate substructure; this will
               be automatically deallocated by 'info' upon delete */
            adj = malloc(sizeof(struct chunk_adj_info));
            if (adj == NULL) {
                pok_exception_flag_memory_error();
                return pok_net_failed_internal;
            }
            info->aux = adj;
        }
        pok_data_stream_read_uint16(dsrc,&adj->n);
        result = pok_netobj_readinfo_process(info);
        if (result == pok_net_completed) {
            if (adj->n == 0) { /* zero chunks specified */
                pok_exception_new_ex(pok_ex_map,pok_ex_map_zero_chunks);
                return pok_net_failed_protocol;
            }
            info->depth[0] = 0;
        }
    }
    if (info->fieldProg == 8) { /* adjacency list */
        while (info->depth[0] < adj->n) {
            pok_data_stream_read_byte(dsrc,adj->a + info->depth[0]);
            if ((result = pok_netobj_readinfo_process_depth(info,0)) != pok_net_completed)
                goto fail;
        }
        ++info->fieldProg;
        info->depth[0] = 0;
    }
    if (info->fieldProg == 9) { /* read chunks */
        while (info->depth[0] < adj->n) {
            if (adj->c[info->depth[0]]==NULL && (adj->c[info->depth[0]] = pok_map_chunk_new(&map->chunkSize)) == NULL)
                return pok_net_failed_internal;
            if (!pok_netobj_readinfo_alloc_next(info))
                return pok_net_failed_internal;
            result = pok_map_chunk_netread(adj->c[info->depth[0]],dsrc,info->next,&map->chunkSize);
            if (result != pok_net_completed)
                goto fail;
            ++info->depth[0];
        }
        /* all chunks successfully read here; setup map chunks according to adjacencies */
        pok_map_configure_adj(map,adj);
    }
fail:
    return result;
}
int pok_map_compar(struct pok_map* left,struct pok_map* right)
{
    return left->mapNo - right->mapNo;
}
