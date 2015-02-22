/* tile.h - pokgame */
#ifndef POKGAME_TILE_H
#define POKGAME_TILE_H
#include "graphics.h" /* gets 'image' and 'net' */

struct pok_tile_manager
{
    /* reference to graphics subsystem */
    const struct pok_graphics_subsystem* sys;

    /* set of tiles available to the tile manager; these images that must be
       square with dimensions equal to 'sys->dimension' */
    uint16_t tilecnt;
    struct pok_image** tileset;
};
void pok_tile_manager_init(struct pok_tile_manager* tman,const struct pok_graphics_subsystem* sys);
void pok_tile_manager_delete(struct pok_tile_manager* tman);
void pok_tile_manager_load_tiles(struct pok_tile_manager* tman,uint16_t imgc,byte_t* data,bool_t byRef);
enum pok_network_result pok_tile_manager_netread_tiles(struct pok_tile_manager* tman, struct pok_data_source* dsrc,
    struct pok_netobj_info* info);


#endif
