/*
  pok-stdenum.h - pokgame
   This file is included as part of the pokgame version API; it should not be included
   directly (include pok.h). It provides standard artwork enumerations that can be put
   to good use if your version uses the standard artwork.
 */
#ifndef POKGAME_POK_STDENUM_H
#define POKGAME_POK_STDENUM_H

/* pok_std_tile: enumerates some (not all) of the tiles defined in the
   standard tileset; currently, the standard tileset version is 'sts1' */
enum pok_std_tile
{
    pok_std_tile_grass = 1599,
    pok_std_tile_grass2 = 1629,
    pok_std_tile_tall_grass = 1635,
    pok_std_tile_tall_grass2 = 1636
};

/* pok_std_sprite: enumerates all of the sprites defined in the standard
   sprite set; currently, the standard tileset version is 'sss0' */
enum pok_std_sprite
{
    pok_std_sprite_male_player = 0,
    pok_std_sprite_scientist = 1
};

#endif
