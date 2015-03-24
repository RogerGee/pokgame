/* sprite.h - pokgame */
#ifndef POKGAME_SPRITE_H
#define POKGAME_SPRITE_H
#include "graphics.h" /* gets 'net' and 'image' */

/* the elements in this enum will map directory to indeces for
   sprite frames */
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

/* the sprite manager is responsible for drawing sprites on the logical
   screen; it is the "lower-level" drawing interface for characters */
struct pok_sprite_manager
{
    const struct pok_graphics_subsystem* sys;

    /* set of sprite images available to the tile manager; these images must
       be square with dimensions equal to 'sys->dimension'; these are "character" sprites;
       servers are expected to order sprites correctly in order to form a sprite association */
    uint16_t spritecnt;
    struct pok_image** spriteset;

    /* the following substructures store an association between a character index and the sprite
       frames used to render it; every space in 'spriteassoc' stores a pointer to an image in
       'spriteset' that lies on an index that is either 0 or a multiple of 8 */
    struct pok_image** spriteassoc;
};
void pok_sprite_manager_init(struct pok_sprite_manager* tman,const struct pok_graphics_subsystem* sys);
void pok_sprite_manager_delete(struct pok_sprite_manager* tman);
void pok_sprite_manager_load(struct pok_sprite_manager* tman,uint16_t imgc,byte_t* data,bool_t byRef);
enum pok_network_result pok_sprite_manager_netread(struct pok_sprite_manager* tman,struct pok_data_source* dsrc,
    struct pok_netobj_readinfo* info);

#endif
