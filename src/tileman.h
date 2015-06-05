/* tileman.h - pokgame */
#ifndef POKGAME_TILEMAN_H
#define POKGAME_TILEMAN_H
#include "net.h"
#include "image.h"
#include "graphics.h"
#include "tile.h"

enum pok_ex_tileman
{
    pok_ex_tileman_zero_tiles, /* zero tiles were specified (that obviously doesn't work) */
    pok_ex_tileman_too_few_ani, /* too few animation substructures were specified */
    pok_ex_tileman_already, /* tile manager was already configured for specified operation */
    pok_ex_tileman_bad_image_dimension /* image cannot load tile manager since it has incorrect dimensions */
};

enum pok_tile_manager_flags
{
    pok_tile_manager_flag_none = 0x00,
    pok_tile_manager_flag_ani_byref = 0x01
};

/* tile animation data structure; defines animation sequences for tiles */
struct pok_tile_ani_data
{
    byte_t ticks; /* animation frame waits this many 'ticks' */
    uint16_t forward; /* next tile frame in list */
    uint16_t backward; /* previous tile in list */
    uint16_t totalTicks; /* total ticks from this tile to itself */
};

/* pok_tile_manager: interface to manage tile images and animation */
struct pok_tile_manager
{
    /* flags bits (used internally; not sent over network) */
    byte_t flags;

    /* reference to graphics subsystem */
    const struct pok_graphics_subsystem* sys;

    /* set of tiles available to the tile manager; these images that must be
       square with dimensions equal to 'sys->dimension' */
    uint16_t tilecnt;
    uint16_t impassability; /* any tile index <= this value is initially considered impassable */
    struct pok_image** tileset;

    /* tile animation data links tiles together to form logical animation sequences; this
       configuration is optional; if loaded, any tileset[id] has some animation frame at
       tileset[tileani[id]] */
    struct pok_tile_ani_data* tileani;

    /* reserved for the implementation */
    struct pok_image* _sheet;
};
struct pok_tile_manager* pok_tile_manager_new(const struct pok_graphics_subsystem* sys);
void pok_tile_manager_free(struct pok_tile_manager* tman);
void pok_tile_manager_init(struct pok_tile_manager* tman,const struct pok_graphics_subsystem* sys);
void pok_tile_manager_delete(struct pok_tile_manager* tman);
bool_t pok_tile_manager_save(struct pok_tile_manager* tman,struct pok_data_source* dsrc);
bool_t pok_tile_manager_open(struct pok_tile_manager* tman,struct pok_data_source* dsrc);
bool_t pok_tile_manager_load_tiles(struct pok_tile_manager* tman,uint16_t imgc,uint16_t impassability,const byte_t* data,bool_t byRef);
bool_t pok_tile_manager_load_ani(struct pok_tile_manager* tman,uint16_t anic,struct pok_tile_ani_data* data,bool_t byRef);
bool_t pok_tile_manager_fromfile_tiles(struct pok_tile_manager* tman,const char* file);
enum pok_network_result pok_tile_manager_netread(struct pok_tile_manager* tman,struct pok_data_source* dsrc,
    struct pok_netobj_readinfo* info);
struct pok_image* pok_tile_manager_get_tile(const struct pok_tile_manager* tman,uint16_t tileid,uint32_t aniticks);

#endif
