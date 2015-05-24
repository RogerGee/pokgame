/* sprite.h - pokgame */
#ifndef POKGAME_SPRITE_H
#define POKGAME_SPRITE_H
#include "net.h"
#include "image.h"
#include "graphics.h"

/* the elements in this enum will map direction to sprite frame indeces */
enum pok_sprite_frame_direction
{
    sprite_up,
    sprite_up_ani,
    sprite_down,
    sprite_down_ani,
    sprite_left,
    sprite_left_ani,
    sprite_right,
    sprite_right_ani
};

/* the sprite manager is responsible for representing character sprite images objects; it
   provides a way to associate images into sprite sets */
struct pok_sprite_manager
{
    const struct pok_graphics_subsystem* sys;

    uint16_t spritecnt; /* number of logical sprites */
    uint16_t imagecnt; /* number of images allocated by sprite manager (spritecnt * 8) */

    /* set of sprite images available to the tile manager; these images must
       be square with dimensions equal to 'sys->dimension'; these are "character" sprites;
       version servers are expected to order sprites correctly to form a sprite association */
    struct pok_image** spriteset;

    /* the following substructures store an association between a character index and the sprite
       frames used to render it; every space in 'spriteassoc' stores a pointer to a set of images
       that make up a single character set */
    struct pok_image*** spriteassoc;
};
struct pok_sprite_manager* pok_sprite_manager_new(const struct pok_graphics_subsystem* sys);
void pok_sprite_manager_free(struct pok_sprite_manager* sman);
void pok_sprite_manager_init(struct pok_sprite_manager* sman,const struct pok_graphics_subsystem* sys);
void pok_sprite_manager_delete(struct pok_sprite_manager* sman);
bool_t pok_sprite_manager_save(struct pok_sprite_manager* sman,struct pok_data_source* dscr);
bool_t pok_sprite_manager_open(struct pok_sprite_manager* sman,struct pok_data_source* dscr);
void pok_sprite_manager_load(struct pok_sprite_manager* sman,uint16_t imgc,byte_t* data,bool_t byRef);
enum pok_network_result pok_sprite_manager_netread(struct pok_sprite_manager* sman,struct pok_data_source* dsrc,
    struct pok_netobj_readinfo* info);

#endif
