/* spriteman.h - pokgame */
#ifndef POKGAME_SPRITEMAN_H
#define POKGAME_SPRITEMAN_H
#include "netobj.h"
#include "image.h"
#include "graphics.h"

/* exceptions */
enum pok_ex_spriteman
{
    pok_ex_spriteman_already, /* the sprite manager was already configured for the specified operation */
    pok_ex_spriteman_bad_image_dimension /* specified image dimensions were incorrect */
};

/* the elements in this enum will map direction to sprite frame indeces */
enum pok_sprite_frame_direction
{
    sprite_up,
    sprite_up_ani1,
    sprite_up_ani2,
    sprite_down,
    sprite_down_ani1,
    sprite_down_ani2,
    sprite_left,
    sprite_left_ani1,
    sprite_left_ani2,
    sprite_right,
    sprite_right_ani1,
    sprite_right_ani2
};

#define pok_to_frame_direction(dir) (dir == pok_direction_up ? sprite_up : (dir == pok_direction_down ? sprite_down \
            : (dir == pok_direction_left ? sprite_left : sprite_right)))
#define pok_from_frame_direction(frame) (frame < 3 ? pok_direction_up : (frame < 6 ? pok_direction_down \
            : (frame < 9 ? pok_direction_left : pok_direction_right)))

/* the sprite manager is responsible for representing character sprite images objects; it
   provides a way to associate images into sprite sets */
struct pok_sprite_manager
{
    const struct pok_graphics_subsystem* sys;

    uint16_t flags; /* determines how the sprite frames are configured */
    uint16_t spritecnt; /* number of logical sprites */
    uint16_t imagecnt; /* number of images allocated by sprite manager (spritecnt * 8) */

    /* set of sprite images available to the tile manager; these images must
       be square with dimensions equal to 'sys->dimension'; these are "character" sprites;
       version servers are expected to order sprites correctly to form a sprite association */
    struct pok_image** spritesets;

    /* the following substructures store an association between a character index and the sprite
       frames used to render it; every space in 'spriteassoc' stores a pointer to a set of images
       that make up a single character set */
    struct pok_image*** spriteassoc;

    /* reserved for implementation */
    struct pok_image* _sheet;
};
struct pok_sprite_manager* pok_sprite_manager_new(const struct pok_graphics_subsystem* sys);
void pok_sprite_manager_free(struct pok_sprite_manager* sman);
void pok_sprite_manager_init(struct pok_sprite_manager* sman,const struct pok_graphics_subsystem* sys);
void pok_sprite_manager_delete(struct pok_sprite_manager* sman);
bool_t pok_sprite_manager_save(struct pok_sprite_manager* sman,struct pok_data_source* dsrc);
bool_t pok_sprite_manager_open(struct pok_sprite_manager* sman,struct pok_data_source* dsrc);
bool_t pok_sprite_manager_load(struct pok_sprite_manager* sman,uint16_t flags,uint16_t spriteCnt,const byte_t* data,bool_t byRef);
bool_t pok_sprite_manager_fromfile(struct pok_sprite_manager* sman,const char* file,uint16_t flags);
bool_t pok_sprite_manager_fromfile_png(struct pok_sprite_manager* sman,const char* file,uint16_t flags);
enum pok_network_result pok_sprite_manager_netread(struct pok_sprite_manager* sman,struct pok_data_source* dsrc,
    struct pok_netobj_readinfo* info);

#endif
