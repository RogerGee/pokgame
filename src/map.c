/* map.c - pokgame */
#include "map.h"
#include "error.h"
#include "pok.h" /* gets protocol.h */
#include <stdlib.h>

/* include the map rendering function; define an interface that it can use to compute map render info */
struct chunk_render_info
{
    uint32_t px, py;
    uint16_t across, down;
    struct pok_location loc;
    struct pok_map_chunk* chunk;
};
void compute_chunk_render_info(struct chunk_render_info* chunks, /* this is 'extern' for debugging */
    const struct pok_graphics_subsystem* sys,struct pok_map_render_context* context);

#if defined(POKGAME_OPENGL)
#include "map-gl.c"
#endif

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
bool_t pok_map_load(struct pok_map* map,const struct pok_tile_data tiledata[],uint32_t columns,uint32_t rows)
{
    /* load a map based on a rectangular tile configuration */
    if (map->origin == NULL) {
        uint16_t i, j;
        struct pok_size mapArea;
        struct pok_size lefttop, rightbottom;
        const struct pok_tile_data* td[3];
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

/* pok_map_render_context */
struct pok_map_render_context* pok_map_render_context_new(struct pok_map* map,const struct pok_tile_manager* tman)
{
    struct pok_map_render_context* context;
    context = malloc(sizeof(struct pok_map_render_context));
    pok_map_render_context_init(context,map,tman);
    return context;
}
void pok_map_render_context_free(struct pok_map_render_context* context)
{
    free(context);
}
void pok_map_render_context_init(struct pok_map_render_context* context,struct pok_map* map,const struct pok_tile_manager* tman)
{
    int i, j;
    context->focus[0] = context->focus[1] = 0;
    context->offset[0] = context->offset[1] = 0;
    for (i = 0;i < 3;++i)
        for (j = 0;j < 3;++j)
            context->viewingChunks[i][j] = NULL;
    context->relpos.column = context->relpos.row = 0;
    context->chunkpos.X = map->originPos.X;
    context->chunkpos.Y = map->originPos.Y;
    map->chunk = map->origin;
    context->map = map;
    context->tman = tman;
    context->aniTicks = 0;
}
void pok_map_render_context_reset(struct pok_map_render_context* context,struct pok_map* newMap)
{
    int i, j;
    context->focus[0] = context->focus[1] = 0;
    context->offset[0] = context->offset[1] = 0;
    for (i = 0;i < 3;++i)
        for (j = 0;j < 3;++j)
            context->viewingChunks[i][j] = NULL;
    context->relpos.column = context->relpos.row = 0;
    context->chunkpos.X = newMap->originPos.X;
    context->chunkpos.Y = newMap->originPos.Y;
    newMap->chunk = newMap->origin;
    context->map = newMap;
    context->aniTicks = 0;
}
void pok_map_render_context_align(struct pok_map_render_context* context)
{
    /* compute surrounding chunks; place the current chunk in the center */
    context->focus[0] = context->focus[1] = 1;
    context->viewingChunks[1][1] = context->map->chunk;
    context->viewingChunks[1][0] = context->map->chunk->adjacent[pok_direction_up];
    context->viewingChunks[1][2] = context->map->chunk->adjacent[pok_direction_down];
    context->viewingChunks[0][1] = context->map->chunk->adjacent[pok_direction_left];
    context->viewingChunks[2][1] = context->map->chunk->adjacent[pok_direction_right];
    context->viewingChunks[0][0] = context->viewingChunks[1][0] != NULL ? context->viewingChunks[1][0]->adjacent[pok_direction_left] : NULL;
    context->viewingChunks[2][0] = context->viewingChunks[1][0] != NULL ? context->viewingChunks[1][0]->adjacent[pok_direction_right] : NULL;
    context->viewingChunks[0][2] = context->viewingChunks[1][2] != NULL ? context->viewingChunks[1][2]->adjacent[pok_direction_left] : NULL;
    context->viewingChunks[2][2] = context->viewingChunks[1][2] != NULL ? context->viewingChunks[1][2]->adjacent[pok_direction_right] : NULL;
}
/* bool_t pok_map_render_context_center_on(struct pok_map_render_context* context,const struct pok_location* location) */
/* { */
/*     int d[2]; */
/*     enum pok_direction dirs[2]; */
/*     struct pok_location pos; */
/*     struct pok_map_chunk* chunk; */
/*     /\* find the chunk that bounds 'location'; it may not exist, in which case the map's */
/*        current chunk remains unchanged and the function returns FALSE *\/ */
/*     pos = context->map->pos; */
/*     chunk = context->map->chunk; */
/*     /\* find direction that needs to be moved in each dimension *\/ */
/*     d[0] = pos.column > location->column */
/*         ? (dirs[0] = pok_direction_left, -context->map->chunkSize.columns) : (dirs[0] = pok_direction_right, context->map->chunkSize.columns);*/
/*     d[1] = pos.row > location->row */
/*         ? (dirs[1] = pok_direction_up, -context->map->chunkSize.rows) : (dirs[1] = pok_direction_down, context->map->chunkSize.rows); */
/*     /\* locate the chunk that contains the desired location; note that the map can be irregular (e.g. not rectangular) *\/ */
/*     do { */
/*         bool_t fA, fB; */
/*         struct pok_map_chunk* A, *B; */
/*         fA = FALSE; A = NULL; */
/*         fB = FALSE; B = NULL; */
/*         /\* advance along the width of the map if needed *\/ */
/*         if (pok_unsigned_diff(location->column,pos.column) >= context->map->chunkSize.columns) { */
/*             pos.column += d[0]; */
/*             A = chunk->adjacent[dirs[0]]; */
/*             fA = TRUE; */
/*         } */
/*         /\* advance along the height of the map if needed *\/ */
/*         if (pok_unsigned_diff(location->row,pos.row) >= context->map->chunkSize.rows) { */
/*             pos.row += d[1]; */
/*             B = chunk->adjacent[dirs[1]]; */
/*             fB = TRUE; */
/*         } */
/*         if (!fA && !fB) */
/*             break; /\* we found it! *\/ */
/*         if ((fA && A == NULL) || (fB && B == NULL)) */
/*             return FALSE; /\* we pushed outside the map *\/ */
/*         if (A != NULL && B != NULL) /\* choose the diagonal (column and row were not yet sufficient) *\/ */
/*             chunk = A->adjacent[dirs[1]]; /\* equivilent to B->adjacent[dirs[0]] (they are orthogonal) *\/ */
/*         else if (A != NULL) */
/*             chunk = A; */
/*         else */
/*             chunk = B; */
/*     } while (chunk != NULL); */
/*     if (chunk == NULL) */
/*         return FALSE; */
/*     context->map->pos = *location; */
/*     context->map->chunk = chunk; */
/*     /\* align the context on the new position *\/ */
/*     pok_map_render_context_align(context,TRUE); */
/*     return TRUE; */
/* } */
#include <stdio.h>
bool_t pok_map_render_context_center_on(struct pok_map_render_context* context,const struct pok_point* chunkpos,const struct pok_location* relpos)
{
    /* center the context on the specified chunk; if the chunk does not exist, then FALSE
       is returned and the context is not updated */
    int32_t x, y;
    int d1, d2, i1, i2;
    struct pok_map_chunk* chunk = context->map->chunk;
    x = chunkpos->X - context->chunkpos.X;
    y = chunkpos->Y - context->chunkpos.Y;
    if (x < 0) {
        d1 = pok_direction_left;
        i1 = 1;
    }
    else if (x > 0) {
        d1 = pok_direction_right;
        i1 = -1;
    }
    if (y < 0) {
        d2 = pok_direction_up;
        i2 = 1;
    }
    else if (y > 0) {
        d2 = pok_direction_down;
        i2 = -1;
    }
    do {
        if (x != 0 && chunk->adjacent[d1] != NULL) {
            chunk = chunk->adjacent[d1];
            x += i1;
        }
        else if (y != 0 && chunk->adjacent[d2] != NULL) {
            chunk = chunk->adjacent[d2];
            y += i2;
        }
        else
            return FALSE;
    } while (x != 0 || y != 0);
    /* operation was successful; realign the context and set the relative position */
    context->map->chunk = chunk;
    pok_map_render_context_align(context);
    context->relpos = *relpos;
    context->chunkpos = *chunkpos;
    return TRUE;
}
bool_t pok_map_render_context_update(struct pok_map_render_context* context,enum pok_direction dir)
{
    /* change the map position by 1 position unit in the specified direction; if this is not possible,
       then leave the map (and the render context) unchanged and return FALSE; otherwise update the relative
       position; if we leave the current chunk then update the context; if we walk into an area close to the
       edge of the 3x3 chunk grid, then re-align the grid around the edge chunk; note that maps can have an
       irregular size, so for the south and east directions the absence of a chunk bounds the map */
    if (dir == pok_direction_up) {
        if (context->relpos.row > 0) {
            --context->relpos.row;
            if (context->focus[1] == 0 && context->relpos.row <= context->map->chunkSize.rows/2)
                pok_map_render_context_align(context);
        }
        /* focus[1] will always be in range (focus[1] > 0 before the operation) */
        else if (context->viewingChunks[context->focus[0]][context->focus[1]-1] == NULL)
            return FALSE; /* there is no chunk to the north */
        else {
            /* focus on the new chunk */
            context->relpos.row = context->map->chunkSize.rows-1;
            --context->chunkpos.Y;
            --context->focus[1];
            context->map->chunk = context->map->chunk->adjacent[dir];
        }
    }
    else if (dir == pok_direction_down) {
        if (context->relpos.row < context->map->chunkSize.rows-1) {
            ++context->relpos.row;
            if (context->focus[1] == 2 && context->relpos.row >= context->map->chunkSize.rows/2)
                pok_map_render_context_align(context);
        }
        /* focus[1] will always be in range (focus[1] <= 1 before the operation) */
        else if (context->viewingChunks[context->focus[0]][context->focus[1]+1] == NULL)
            return FALSE; /* there is not a chunk to the south */
        else {
            /* focus on the new chunk */
            context->relpos.row = 0;
            ++context->chunkpos.Y;
            ++context->focus[1];
            context->map->chunk = context->map->chunk->adjacent[dir];
        }
    }
    else if (dir == pok_direction_left) {
        if (context->relpos.column > 0) {
            --context->relpos.column;
            if (context->focus[0] == 0 && context->relpos.column <= context->map->chunkSize.columns/2)
                pok_map_render_context_align(context);
        }
        /* focus[0] will always be in range (focus[0] >= 1 before the operation) */
        else if (context->viewingChunks[context->focus[0]-1][context->focus[1]] == NULL)
            return FALSE; /* there is no chunk to the west */
        else {
            /* focus on the new chunk */
            context->relpos.column = context->map->chunkSize.columns-1;
            --context->chunkpos.X;
            --context->focus[0];
            context->map->chunk = context->map->chunk->adjacent[dir];
        }
    }
    else if (dir == pok_direction_right) {
        if (context->relpos.column < context->map->chunkSize.columns-1) {
            ++context->relpos.column;
            if (context->focus[0] == 2 && context->relpos.column >= context->map->chunkSize.columns/2)
                pok_map_render_context_align(context);
        }
        /* focus[0] will always be in range (focus[0] <= 1 before the operation) */
        else if (context->viewingChunks[context->focus[0]+1][context->focus[1]] == NULL)
            return FALSE; /* there is no chunk to the east */
        else {
            /* focus on the new chunk */
            context->relpos.column = 0;
            ++context->chunkpos.X;
            ++context->focus[0];
            context->map->chunk = context->map->chunk->adjacent[dir];
        }
    }
    else
        return FALSE;
    return TRUE;
}

/* implementation of interface for map rendering routine */
void compute_chunk_render_info(struct chunk_render_info* info,
    const struct pok_graphics_subsystem* sys,struct pok_map_render_context* context)
{
    /* a map is painted as an application of at most 4 chunks; this routine computes which chunks are needed along with
       the bounding information specifying how each chunk is to be drawn; this routine corrects for small chunk dimensions
       (but the dimensions should be correct for any multi-chunk map, otherwise undefined behavior will result) */
    int i, d[2];
    uint16_t u, v;
    /* chunks: chunk1 is always the horizontally adjacent chunk (if any) and
       chunk2 is always the vertically adjacent chunk (if any) */
    for (i = 0;i < 4;++i)
        info[i].chunk = NULL;
    info[0].px = info[0].py = 0;
    info[0].across = sys->windowSize.columns;
    info[0].down = sys->windowSize.rows;
    info[0].chunk = context->map->chunk;
    i = (int)context->relpos.column - (int)sys->playerLocation.column;
    d[0] = context->focus[0]; d[1] = context->focus[1];
    if (i < 0) {
        /* viewing area exceeds chunk bounds on the left */
        u = -i; /* number of columns to the left */
        info[0].px += sys->dimension * u;
        info[0].across -= u;
        info[0].loc.column = 0;
        /* set dimensions for adjacent chunk; 'py', 'down' and 'loc.row' members to be set later */
        info[1].px = 0;
        info[1].across = u;
        info[1].loc.column = context->map->chunkSize.columns - u;
        info[1].chunk = context->viewingChunks[context->focus[0]-1][context->focus[1]]; /* immediate left */
        --d[0];
    }
    else {
        info[0].loc.column = i;
        u = context->map->chunkSize.columns - context->relpos.column - 1; /* how many columns to the right in chunk? */
        if (u < sys->_playerLocationInv.column) {
            /* viewing area exceeds chunk bounds on the right
               (note: viewing area can only exceed on the right given it did not exceed on the left) */
            v = sys->_playerLocationInv.column - u; /* how many columns to the right in viewing area? */
            info[0].across -= v;
            /* set dimensions for adjacent chunk; 'py', 'down' and 'loc.row' members to be set later */
            info[1].px = sys->dimension * info[0].across;
            info[1].across = v;
            info[1].loc.column = 0;
            info[1].chunk = context->viewingChunks[context->focus[0]+1][context->focus[1]]; /* immediate right */
            ++d[0];
        }
    }
    i = (int)context->relpos.row - (int)sys->playerLocation.row;
    if (i < 0) {
        /* viewing area exceeds chunk bounds above */
        u = -i; /* number of rows above */
        info[0].py += sys->dimension * u;
        info[0].down -= u;
        info[0].loc.row = 0;
        /* set dimensions for adjacent chunk */
        info[2].py = 0;
        info[2].down = u;        
        info[2].loc.row = context->map->chunkSize.rows - u;
        info[2].chunk = context->viewingChunks[context->focus[0]][context->focus[1]-1]; /* straight up */
        --d[1];
    }
    else {
        info[0].loc.row = i;
        u = context->map->chunkSize.rows - context->relpos.row - 1; /* how many rows below in chunk? */
        if (u < sys->_playerLocationInv.row) {
            /* viewing area exceeds chunk bounds below
               (note: viewing area can only exceed below given it did not exceed above) */
            v = sys->_playerLocationInv.row - u; /* how many rows below in viewing area? */
            info[0].down -= v;
            /* set dimensions for adjacent chunk */
            info[2].py = sys->dimension * info[0].down;
            info[2].down = v;
            info[2].loc.row = 0;
            info[2].chunk = context->viewingChunks[context->focus[0]][context->focus[1]+1]; /* straight down */
            ++d[1];
        }
    }
    if (info[1].chunk != NULL) {
        /* chunk 1 is identical to chunk 0 vertically */
        info[1].py = info[0].py;
        info[1].down = info[0].down;
        info[1].loc.row = info[0].loc.row;
    }
    if (info[2].chunk != NULL) {
        /* chunk 2 is identical to chunk 0 horizontally */
        info[2].px = info[0].px;
        info[2].across = info[0].across;
        info[2].loc.column = info[0].loc.column;
    }
    if (info[1].chunk!=NULL && info[2].chunk!=NULL) {
        /* we need to pull from a diagonal chunk; chunk 3 will be vertically identical
           to chunk 1 and horizontally identical to chunk 2 */
        info[3].px = info[1].px;
        info[3].across = info[1].across;
        info[3].loc.column = info[1].loc.column;
        info[3].py = info[2].py;
        info[3].down = info[2].down;
        info[3].loc.row = info[2].loc.row;
        info[3].chunk = context->viewingChunks[d[0]][d[1]];
    }
    /* correct chunk sizes */
    for (i = 0;i < 4;++i) {
        if (info[i].chunk != NULL) {
            if (info[i].across > context->map->chunkSize.columns)
                info[i].across = context->map->chunkSize.columns;
            if (info[i].down > context->map->chunkSize.rows)
                info[i].down = context->map->chunkSize.rows;
        }
    }
}
