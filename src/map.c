/* map.c - pokgame */
#include "map.h"
#include "error.h"
#include "pok.h"
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
    uint8_t a[POK_MAX_INITIAL_CHUNKS];
    struct pok_point p[POK_MAX_INITIAL_CHUNKS];
    struct pok_map_chunk* c[POK_MAX_INITIAL_CHUNKS];
};

static void chunk_adj_info_init(struct chunk_adj_info* info)
{
    for (int i = 0;i < POK_MAX_INITIAL_CHUNKS;++i)
        info->c[i] = NULL;
}

struct chunk_recursive_info
{
    struct pok_data_source* dsrc;
    struct pok_map* map;
    bool_t complexTiles;
};

struct chunk_key
{
    struct pok_point pos;
    struct pok_map_chunk* chunk;
};

static bool_t chunk_key_create(struct pok_map* map,struct pok_map_chunk* chunk,const struct pok_point* point)
{
    struct chunk_key* key = malloc(sizeof(struct chunk_key));
    if (key == NULL) {
        pok_exception_flag_memory_error();
        return FALSE;
    }
    key->pos = *point;
    key->chunk = chunk;
    if (treemap_insert(&map->loadedChunks,key) != 0) {
        /* a chunk key already exists with the same position */
        pok_exception_new_ex(pok_ex_map,pok_ex_map_non_unique_chunk);
        return FALSE;
    }
    return TRUE;
}

/* pok_map_chunk */
enum pok_map_chunk_flags
{
    pok_map_chunk_flag_none = 0x00,
    pok_map_chunk_flag_byref = 0x01
};

static struct pok_map_chunk* pok_map_chunk_new(struct pok_map* map,const struct pok_point* position)
{
    uint16_t i, j;
    struct pok_map_chunk* chunk;
    chunk = malloc(sizeof(struct pok_map_chunk));
    if (chunk == NULL) {
        pok_exception_flag_memory_error();
        return NULL;
    }
    chunk->data = malloc(sizeof(struct pok_tile*) * map->chunkSize.rows);
    if (chunk->data == NULL) {
        pok_exception_flag_memory_error();
        return NULL;
    }
    for (i = 0;i < map->chunkSize.rows;++i) {
        chunk->data[i] = malloc(sizeof(struct pok_tile) * map->chunkSize.columns);
        if (chunk->data[i] == NULL) {
            for (j = 0;j < i;++j)
                free(chunk->data[j]);
            free(chunk->data);
            pok_exception_flag_memory_error();
            return NULL;
        }
        for (j = 0;j < map->chunkSize.columns;++j)
            chunk->data[i][j] = DEFAULT_TILE;
    }
    for (i = 0;i < 4;++i)
        chunk->adjacent[i] = NULL;
    chunk->flags = pok_map_chunk_flag_none;
    chunk->discov = FALSE;
    /* add the chunk to the map's treemap (if 'position' is specified); if this fails, then destroy the chunk */
    if (position != NULL && !chunk_key_create(map,chunk,position)) {
        for (i = 0;i < map->chunkSize.rows;++i)
            free(chunk->data[i]);
        free(chunk->data);
        return NULL; /* exception is inherited */
    }
    pok_netobj_default_ex(&chunk->_base,pok_netobj_mapchunk);
    return chunk;
}
static struct pok_map_chunk* pok_map_chunk_new_ex(struct pok_map* map,const struct pok_point* position,bool_t byref)
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
    chunk->data = malloc(sizeof(struct pok_tile*) * map->chunkSize.rows);
    for (i = 0;i < map->chunkSize.rows;++i)
        chunk->data[i] = NULL;
    for (i = 0;i < 4;++i)
        chunk->adjacent[i] = NULL;
    chunk->flags = byref ? pok_map_chunk_flag_byref : pok_map_chunk_flag_none;
    chunk->discov = FALSE;
    /* add the chunk to the map's treemap; if this fails, then destroy the chunk */
    if ( !chunk_key_create(map,chunk,position) ) {
        free(chunk->data);
        return NULL; /* exception is inherited */
    }
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
    for (r = 0;r < info->map->chunkSize.rows;++r) {
        for (c = 0;c < info->map->chunkSize.columns;++c) {
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
static bool_t pok_map_chunk_open(struct pok_map_chunk* chunk,struct chunk_recursive_info* info,struct pok_point point)
{
    /* fields:
        [byte] bitmask specifying adjacencies to load (use 'enum pok_direction'+1 to mask)
        (recursively read the adjacencies)
        [n bytes] tile data

        This is a depth first way of reading chunk information. The representation is still a graph,
        but it does not have any cycles. The chunk position is assigned into the pok_map's treemap here.
    */
    int i;
    byte_t adj;
    uint16_t c, r;
    static struct pok_point ptmp;
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
            if (chunk->adjacent[i] == NULL) { /* create new chunk */
                /* compute the chunk's position */
                ptmp = point;
                if (i == pok_direction_up)
                    --ptmp.Y;
                else if (i == pok_direction_down)
                    ++ptmp.Y;
                else if (i == pok_direction_left)
                    --ptmp.X;
                else /*if (i == pok_direction_right)*/
                    ++ptmp.X;
                chunk->adjacent[i] = pok_map_chunk_new(info->map,&ptmp);
                chunk->adjacent[i]->adjacent[op] = chunk;
                /* recursively build chunk; ptmp is a static variable whose value
                   is copied onto the stack */
                if ( !pok_map_chunk_open(chunk->adjacent[i],info,ptmp) )
                    return FALSE;
            }
            else
                chunk->adjacent[i]->adjacent[op] = chunk;
        }
    }
    /* base case: read chunk data */
    for (r = 0;r < info->map->chunkSize.rows;++r) {
        for (c = 0;c < info->map->chunkSize.columns;++c) {
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
    switch (info->fieldProg) {
    case 0:
        /* superclass: pok_netobj */
        if ((result = pok_netobj_netread(&chunk->_base,dsrc,info)) != pok_net_completed)
            break;
        if ( !pok_netobj_readinfo_alloc_next(info) )
            return pok_net_failed_internal;
    case 1:
        /* tile structures */
        while (info->depth[0] < size->rows) {
            while (info->depth[1] < size->columns) {
                result = pok_tile_netread(chunk->data[info->depth[0]]+info->depth[1],dsrc,info->next);
                if (result != pok_net_completed)
                    return result;
                pok_netobj_readinfo_reset(info->next);
                ++info->depth[1];
            }
            info->depth[1] = 0;
            ++info->depth[0];
        }
        ++info->fieldProg;
    }
    return result;
}
static enum pok_network_result pok_map_chunk_netwrite(struct pok_map_chunk* chunk,
    struct pok_data_source* dsrc,
    struct pok_netobj_writeinfo* winfo)
{
    enum pok_network_result result = pok_net_completed;

    return result;    
}
enum pok_network_result pok_map_chunk_netmethod_send(struct pok_map_chunk* chunk,
    struct pok_data_source* dsrc,
    struct pok_netobj_writeinfo* winfo,
    struct pok_netobj_upinfo* uinfo)
{
    enum pok_network_result result = pok_net_completed;

    return result;
}
enum pok_network_result pok_map_chunk_netmethod_recv(struct pok_map_chunk* chunk,
    struct pok_data_source* dsrc,
    struct pok_netobj_readinfo* info,
    enum pok_map_chunk_method method)
{
    enum pok_network_result result = pok_net_completed;

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
    map->origin = NULL;
    map->chunkSize.columns = map->chunkSize.rows = 0;
    map->originPos.X = map->originPos.Y = 0;
    map->flags = pok_map_flag_none;
    treemap_init(&map->loadedChunks,(key_comparator)pok_point_compar,free); /* does not delete chunks, just the keys */
    pok_netobj_default_ex(&map->_base,pok_netobj_map);
}
void pok_map_delete(struct pok_map* map)
{
    pok_netobj_delete(&map->_base);
    if (map->origin != NULL) {
        /* this will recursively delete the adjacent chunks */
        pok_map_chunk_free(map->origin,map->chunkSize.rows);
        map->origin = NULL;
    }
    treemap_delete(&map->loadedChunks);
}
bool_t pok_map_configure(struct pok_map* map,const struct pok_size* chunkSize,const uint16_t firstChunk[],uint32_t length)
{
    /* configure an empty map with the specified chunk size; create the first chunk from the specified tile data; the
       'length' argument specifies the length of the tile data array; the algorithm will repeat the tiles if too few
       are specified (this allows fills and patterns to be applied to a chunk) */
    if (map->origin == NULL) {
        uint16_t i, j;
        uint32_t k = 0;
        /* validate chunk size */
        if (chunkSize->columns < POK_MIN_MAP_CHUNK_DIMENSION || chunkSize->columns > POK_MAX_MAP_CHUNK_DIMENSION
            || chunkSize->rows < POK_MIN_MAP_CHUNK_DIMENSION || chunkSize->rows > POK_MAX_MAP_CHUNK_DIMENSION) {
            pok_exception_new_ex(pok_ex_map,pok_ex_map_bad_chunk_size);
            return FALSE;
        }
        /* create origin chunk */
        map->chunkSize = *chunkSize;
        map->origin = pok_map_chunk_new(map,&ORIGIN);
        /* assign chunk data; the tile data may repeat */
        if (length > 0)
            for (i = 0;i < chunkSize->rows;++i)
                for (j = 0;j < chunkSize->columns;++j)
                    map->origin->data[i][j].data.tileid = firstChunk[k++ % length];
        return TRUE;
    }
    pok_exception_new_ex(pok_ex_map,pok_ex_map_already);
    return FALSE;
}
struct pok_map_chunk* pok_map_add_chunk(struct pok_map* map,
    const struct pok_point* adjacency,
    enum pok_direction direction,
    const uint16_t chunkTiles[],
    uint32_t length)
{
    /* add a new chunk to the map that is adjacent to the chunk at position
       'adjacency' in the specified 'direction'; assign the specified tile
       information to the new chunk */
    if (map->origin != NULL) {
        uint16_t i, j;
        uint32_t k = 0;
        struct pok_point pos;
        struct pok_map_chunk* chunk;
        /* compute the position for the new chunk */
        pos = *adjacency;
        pok_direction_add_to_point(direction,&pos);
        /* create the new chunk; this adds it to the map */
        chunk = pok_map_chunk_new(map,&pos);
        if (chunk != NULL) {
            /* assign chunk data; the tile data may repeat */
            if (length > 0)
                for (i = 0;i < map->chunkSize.rows;++i)
                    for (j = 0;j < map->chunkSize.columns;++j)
                        chunk->data[i][j].data.tileid = chunkTiles[k++ % length];
        }
        return chunk;
    }
    pok_exception_new_ex(pok_ex_map,pok_ex_map_not_loaded);
    return NULL;
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
        info.map = map;
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
    pok_exception_new_ex(pok_ex_map,pok_ex_map_not_loaded);
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
        if (map->chunkSize.columns < POK_MIN_MAP_CHUNK_DIMENSION || map->chunkSize.columns > POK_MAX_MAP_CHUNK_DIMENSION
            || map->chunkSize.rows < POK_MIN_MAP_CHUNK_DIMENSION || map->chunkSize.rows > POK_MAX_MAP_CHUNK_DIMENSION) {
            pok_exception_new_ex(pok_ex_map,pok_ex_map_bad_chunk_size);
            return FALSE;
        }
        /* read origin chunk position */
        if (!pok_data_stream_read_int32(dsrc,&map->originPos.X) || !pok_data_stream_read_int32(dsrc,&map->originPos.Y))
            return FALSE;
        /* read origin chunk; this will recursively read all chunks in the map */
        map->origin = pok_map_chunk_new(map,&map->originPos);
        if (map->origin == NULL)
            return FALSE;
        info.dsrc = dsrc;
        info.map = map;
        if ( pok_map_chunk_open(map->origin,&info,map->originPos) )
            return TRUE;
        return FALSE;
    }
    pok_exception_new_ex(pok_ex_map,pok_ex_map_already);
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
        mapArea = pok_util_compute_chunk_size(columns,rows,POK_MAX_MAP_CHUNK_DIMENSION,&map->chunkSize,&lefttop,&rightbottom);
        chunk_insert_hint_init(&hint,mapArea.columns,mapArea.rows);
        td[0] = tiledata;
        for (i = 0;i < mapArea.rows;++i) {
            struct pok_size size;
            td[1] = td[0];
            for (j = 0;j < mapArea.columns;++j) {
                uint16_t k, l, m, n, o;
                struct pok_point position;
                struct pok_map_chunk* chunk;
                position.X = i;
                position.Y = j;
                chunk = pok_map_chunk_new(map,&position);
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
                if (map->origin == NULL)
                    map->origin = chunk;
                else
                    pok_map_insert_chunk(map,chunk,&hint);
            }
            /* advance to the next row of chunks */
            td[0] += columns * size.rows;
        }
        return TRUE;
    }
    pok_exception_new_ex(pok_ex_map,pok_ex_map_already);
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
    pok_exception_new_ex(pok_ex_map,pok_ex_map_already);
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
    pok_exception_new_ex(pok_ex_map,pok_ex_map_already);
    return FALSE;
}
struct pok_map_chunk* pok_map_get_chunk(struct pok_map* map,const struct pok_point* pos)
{
    struct chunk_key* key;
    key = treemap_lookup(&map->loadedChunks,pos);
    if (key == NULL)
        return NULL;
    return key->chunk;
}
static bool_t pok_map_configure_adj(struct pok_map* map,struct chunk_adj_info* info)
{
    /* configure a map's chunks based on an adjacency list; the adjacency list specifies
       adjacencies in a breadth-first way; adjacencies may imply adjacencies not specified */
    int i, j, k;
    /* set origin chunk */
    map->origin = info->c[0];
    info->p[0] = map->originPos;
    if ( !chunk_key_create(map,map->origin,&map->originPos) )
        return FALSE;
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
                    struct pok_point pos;
                    if (chunk != NULL) {
                        /* two chunks have been specified in the same place (peer made an error); delete the unassigned
                           chunk and continue (we want to handle this gracefully) */
                        pok_map_chunk_free(info->c[j++],map->chunkSize.rows);
                        continue;
                    }
                    /* compute chunk position */
                    pos = info->p[i];
                    if (k == pok_direction_up)
                        --pos.Y;
                    else if (k == pok_direction_down)
                        ++pos.Y;
                    else if (k == pok_direction_left)
                        --pos.X;
                    else /* if (k == pok_direction_right) */
                        ++pos.X;
                    info->p[j] = pos; /* save position for later */
                    chunk = info->c[j++];
                    /* add the new chunk to the map's treemap */
                    if ( !chunk_key_create(map,chunk,&pos) )
                        return FALSE;
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
    return TRUE;
}
static enum pok_network_result pok_map_netread(struct pok_map* map,struct pok_data_source* dsrc,struct pok_netobj_readinfo* info)
{
    /* fields
        [super class]
        [2 bytes] map flags
        [4 bytes] map number
        [2 bytes] chunk size width
        [2 bytes] chunk size height
        [4 bytes] origin chunk position X
        [4 bytes] origin chunk position Y
        [2 bytes] number of chunks
        [n bytes] chunk adjacency list (bitmask where each bit index corresponds to pok_direction flag)
        [n chunks]
          [...] netread chunk

        The peer is expected to send less than or equal to pok_max_initial_chunks. The chunks are preceeded by
        an adjacency list (of adjacency bitmasks). The list of adjacency bitmasks is parallel to the list of
        chunks
    */
    struct chunk_adj_info* adj = (struct chunk_adj_info*) info->aux;
    enum pok_network_result result = pok_net_already;
    switch (info->fieldProg) {
    case 0:
        /* super class */
        if ((result = pok_netobj_netread(&map->_base,dsrc,info)) != pok_net_completed)
            break;
    case 1:
        /* map flags */
        pok_data_stream_read_uint16(dsrc,&map->flags);
        if ((result = pok_netobj_readinfo_process(info)) != pok_net_completed)
            break;
    case 2:
        /* map number */
        pok_data_stream_read_uint32(dsrc,&map->mapNo);
        if ((result = pok_netobj_readinfo_process(info)) != pok_net_completed)
            break;
    case 3:
        /* chunk size width */
        pok_data_stream_read_uint16(dsrc,&map->chunkSize.columns);
        if ((result = pok_netobj_readinfo_process(info)) != pok_net_completed)
            break;
        if (map->chunkSize.columns < POK_MIN_MAP_CHUNK_DIMENSION || map->chunkSize.columns > POK_MAX_MAP_CHUNK_DIMENSION) {
            pok_exception_new_ex(pok_ex_map,pok_ex_map_bad_chunk_size);
            return pok_net_failed_protocol;
        }
    case 4:
        /* chunk size height */
        pok_data_stream_read_uint16(dsrc,&map->chunkSize.rows);
        if ((result = pok_netobj_readinfo_process(info)) != pok_net_completed)
            break;
        if (map->chunkSize.rows < POK_MIN_MAP_CHUNK_DIMENSION || map->chunkSize.rows > POK_MAX_MAP_CHUNK_DIMENSION) {
            pok_exception_new_ex(pok_ex_map,pok_ex_map_bad_chunk_size);
            return pok_net_failed_protocol;
        }
    case 5:
        /* origin chunk position X */
        pok_data_stream_read_int32(dsrc,&map->originPos.X);
        if ((result = pok_netobj_readinfo_process(info)) != pok_net_completed)
            break;
    case 6:
        /* origin chunk position Y */
        pok_data_stream_read_int32(dsrc,&map->originPos.Y);
        if ((result = pok_netobj_readinfo_process(info)) != pok_net_completed)
            break;
    case 7:
        /* number of chunks */
        if (adj == NULL) {
            /* we need to store chunk information in a separate substructure; this will
               be automatically deallocated by 'info' upon delete */
            adj = malloc(sizeof(struct chunk_adj_info));
            if (adj == NULL) {
                pok_exception_flag_memory_error();
                return pok_net_failed_internal;
            }
            chunk_adj_info_init(adj);
            info->aux = adj;
        }
        pok_data_stream_read_uint16(dsrc,&adj->n);
        if ((result = pok_netobj_readinfo_process(info)) != pok_net_completed)
            break;
        if (adj->n == 0) { /* zero chunks specified */
            pok_exception_new_ex(pok_ex_map,pok_ex_map_zero_chunks);
            return pok_net_failed_protocol;
        }
        if (adj->n > POK_MAX_INITIAL_CHUNKS) {
            pok_exception_new_ex(pok_ex_map,pok_ex_map_too_many_chunks);
            return pok_net_failed_protocol;
        }
        info->depth[0] = 0;
    case 8:
        /* adjacency list */        
        while (info->depth[0] < adj->n) {
            pok_data_stream_read_byte(dsrc,adj->a + info->depth[0]);
            if ((result = pok_netobj_readinfo_process_depth(info,0)) != pok_net_completed)
                return result;
        }
        ++info->fieldProg;
        info->depth[0] = 0;
    case 9:
        /* read chunks */
        while (info->depth[0] < adj->n) {
            int i = info->depth[0];
            if (adj->c[i] == NULL
                && (adj->c[i] = pok_map_chunk_new(map,NULL)) == NULL)
            {
                return pok_net_failed_internal;
            }
            if (!pok_netobj_readinfo_alloc_next(info))
                return pok_net_failed_internal;
            if ((result = pok_map_chunk_netread(adj->c[i],dsrc,info->next,&map->chunkSize))
                != pok_net_completed)
            {
                return result;
            }
            ++info->depth[0];
        }
        /* all chunks successfully read here; setup map chunks according to adjacencies */
        if ( !pok_map_configure_adj(map,adj) )
            return pok_net_failed_internal;
    }
    return result;
}
enum pok_network_result pok_map_netwrite(struct pok_map* map,
    struct pok_data_source* dsrc,
    struct pok_netobj_writeinfo* info)
{
    enum pok_network_result result = pok_net_completed;

    return result;
}
enum pok_network_result pok_map_netmethod_send(struct pok_map* map,
    struct pok_data_source* dsrc,
    struct pok_netobj_writeinfo* winfo,
    struct pok_netobj_upinfo* uinfo)
{
    enum pok_network_result result = pok_net_completed;

    return result;
}
enum pok_network_result pok_map_netmethod_recv(struct pok_map* map,
    struct pok_data_source* dsrc,
    struct pok_netobj_readinfo* info,
    enum pok_map_method method)
{
    enum pok_network_result result = pok_net_completed;

    return result;
}
static int pok_map_compar(struct pok_map* left,struct pok_map* right)
{
    return left->mapNo - right->mapNo;
}

/* pok_world */
struct pok_world* pok_world_new()
{
    struct pok_world* world = malloc(sizeof(struct pok_world));
    if (world == NULL) {
        pok_exception_flag_memory_error();
        return NULL;
    }
    pok_world_init(world);
    return world;
}
void pok_world_free(struct pok_world* world)
{
    pok_world_delete(world);
    free(world);
}
void pok_world_init(struct pok_world* world)
{
    pok_netobj_default_ex(&world->_base,pok_netobj_world);
    treemap_init(&world->loadedMaps,(key_comparator)pok_map_compar,(destructor)pok_map_free);
}
void pok_world_delete(struct pok_world* world)
{
    treemap_delete(&world->loadedMaps);
    pok_netobj_delete(&world->_base);
}
struct pok_map* pok_world_get_map(struct pok_world* world,uint32_t mapNo)
{
    /* we have to use a 'struct pok_map' as the key since 'pok_map::mapNo'
       is NOT the first member to the structure */
    struct pok_map dummyMap;
    dummyMap.mapNo = mapNo;
    return treemap_lookup(&world->loadedMaps,&dummyMap);
}
bool_t pok_world_fromfile_warps(struct pok_world* world,const char* filename)
{
    /* read warps from the specified file and apply them to the map; silently fail
       if any specified map does not exist within the world at this point */
    struct pok_data_source* fin;
    fin = pok_data_source_new_file(filename,pok_filemode_open_existing,pok_iomode_read);
    if (fin != NULL) {
        bool_t result;
        struct pok_parser_info parser;
        pok_parser_info_init(&parser);
        parser.dsrc = fin;
        if ((result = pok_parse_map_warps(&parser))) {
            size_t i, j;
            for (i = 0,j = 0;i < parser.bytes_c[0];++i,j += 10) {
                struct pok_map* map;
                struct pok_point chunkpos;
                struct pok_location relpos;
                chunkpos.X = parser.qwords[j+2];
                chunkpos.Y = parser.qwords[j+3];
                relpos.column = parser.qwords[j+6];
                relpos.row = parser.qwords[j+7];
                if ((map = pok_world_get_map(world,parser.qwords[j])) != NULL) {
                    struct pok_map_chunk* chunk;
                    if ((chunk = pok_map_get_chunk(map,&chunkpos)) != NULL) {
                        if (relpos.column < map->chunkSize.columns && relpos.row < map->chunkSize.rows) {
                            struct pok_tile_data* data;
                            data = &chunk->data[relpos.row][relpos.column].data;
                            data->warpKind = parser.bytes[i];
                            data->warpMap = parser.qwords[j+1];
                            data->warpChunk.X = parser.qwords[j+4];
                            data->warpChunk.Y = parser.qwords[j+5];
                            data->warpLocation.column = parser.qwords[j+8];
                            data->warpLocation.row = parser.qwords[j+9];
                        }
                    }
                }
            }
        }
        pok_parser_info_delete(&parser);
        pok_data_source_free(fin);
        return result;
    }
    return FALSE;
}
enum pok_network_result pok_world_netwrite(struct pok_world* world,
    struct pok_data_source* dsrc,
    struct pok_netobj_writeinfo* info)
{
    enum pok_network_result result = pok_net_completed;

    return result;
}
enum pok_network_result pok_world_netmethod_send(struct pok_world* world,
    struct pok_data_source* dsrc,
    struct pok_netobj_writeinfo* winfo,
    struct pok_netobj_upinfo* uinfo)
{
    enum pok_network_result result = pok_net_completed;

    return result;
}
static enum pok_network_result world_netmethod_add_map(struct pok_world* world,
    struct pok_data_source* dsrc,
    struct pok_netobj_readinfo* info)
{
    /* this method is very simple: we try to netread a new map object and
       register it; then we add it to the world */
    enum pok_network_result result;
    struct pok_map* map;

    /* create a map object for use by the operation */
    if (info->aux == NULL) {
        map = pok_map_new();
        if (map == NULL)
            return pok_net_failed_internal;
        /* save for future calls (in case we have to return early) */
        info->aux = map;
    }
    else {
        /* recall the map object from a previous call */
        map = (struct pok_map*)info->aux;
    }

    /* allocate another readinfo for the operation; we do this so we can store
       both 'map' and another aux structure in case the transfer is incomplete */
    if (!pok_netobj_readinfo_alloc_next(info))
        return pok_net_failed_internal;

    /* attempt to netread the map */
    result = pok_map_netread(map,dsrc,info->next);
    if (result == pok_net_completed) {
        /* remove this reference so it is not freed */
        info->aux = NULL;

        /* add the new map to the world object */
        if (!pok_world_add_map(world,map)) {
            pok_exception_new_format("could not add map '%u' to world",map->mapNo);
            return pok_net_failed_protocol;
        }
    }
    else if (result != pok_net_incomplete) {
        /* the operation failed: destroy the map object properly */
        info->aux = NULL;
        pok_map_free(map);
    }
    return result;
}
enum pok_network_result pok_world_netmethod_recv(struct pok_world* world,
    struct pok_data_source* dsrc,
    struct pok_netobj_readinfo* info,
    enum pok_world_method method)
{
    enum pok_network_result result = pok_net_failed_protocol;

    /* delegate control to the appropriate function to handle the method kind */
    switch (method) {
    case pok_world_method_add_map:
        return world_netmethod_add_map(world,dsrc,info);
    default: /* error: bad method kind */
        pok_exception_new_format("bad method to world object: %d",method);
    }

    return result;
}
