/* sprite.h - pokgame */
#ifndef POKGAME_SPRITE_H
#define POKGAME_SPRITE_H
#include "graphics.h" /* gets 'net' and 'image' */

struct pok_sprite_manager
{
    const struct pok_graphics_subsystem* sys;

    /* set of sprite images available to the tile manager; these images must
       be square with dimensions equal to 'sys->dimension'; these are "character" sprites */
    uint16_t spritecnt;
    struct pok_image** spriteset;
};
void pok_sprite_manager_init(struct pok_sprite_manager* tman,const struct pok_graphics_subsystem* sys);
void pok_sprite_manager_delete(struct pok_sprite_manager* tman);
void pok_sprite_manager_load_sprites(struct pok_sprite_manager* tman,uint16_t imgc,byte_t* data,bool_t byRef);
enum pok_network_result pok_sprite_manager_netread_sprites(struct pok_sprite_manager* tman,struct pok_data_source* dsrc,
    struct pok_netobj_info* info);

#endif
