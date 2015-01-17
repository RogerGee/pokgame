/* graphics.h - pokgame */
#ifndef POKGAME_GRAPHICS_H
#define POKGAME_GRAPHICS_H
#include "image.h" /* gets net.h */

/* define size by number of columns and rows */
struct pok_size
{
    uint16_t columns; /* width */
    uint16_t rows; /* height */
};

/* define location by column and row position:
    columns are numbered from [0..n] left-to-right; rows
    are numbered from [0..n] from top-to-bottom */
struct pok_location
{
    uint16_t column; /* x */
    uint16_t row; /* y */
};

/* define graphics subsystem object */
struct pok_graphics_subsystem
{
    uint16_t dimension; /* tile and sprite dimension */
    struct pok_size windowSize; /* size of viewable window */
    struct pok_location playerLocation; /* location of player sprite */
    uint16_t playerOffsetX, playerOffsetY; /* pixel offset for player sprite */

    size_t tilecnt;
    struct pok_image** tileset;

    size_t spritecnt;
    struct pok_image** spriteset;
};
struct pok_graphics_subsystem* pok_graphics_subsystem_new();
void pok_graphics_subsystem_free(struct pok_graphics_subsystem* sys);
void pok_graphics_subsystem_default(struct pok_graphics_subsystem* sys);
void pok_graphics_subsystem_delete(struct pok_graphics_subsystem* sys);
void pok_graphics_subsystem_load_tiles(struct pok_graphics_subsystem* sys);

#endif
