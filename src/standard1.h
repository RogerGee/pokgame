/* standard1.h */
#ifndef POKGAME_STANDARD1_H
#define POKGAME_STANDARD1_H
#include "tileman.h"
#include "pok-stdenum.h"

/* the following globals define important settings for the default version; their values rely
   on a specific version of the game's standard artwork (e.g. tileset, spriteset, ETC.) */

/* standard files */
#define POKGAME_STS_IMAGE "sts1.png"
#define POKGAME_SSS_IMAGE "sss1.png"

/* tile manager impassibility: defines which tiles are by default impassable */
extern const uint16_t DEFAULT_TILEMAN_IMPASSIBILITY;

/* tile manager animation tiles: specifies which tiles animate and how */
extern const uint16_t DEFAULT_TILEMAN_ANI_DATA_LENGTH;
extern struct pok_tile_ani_data DEFAULT_TILEMAN_ANI_DATA[];

/* tile manager terrain info: defines special terrain tiles; the first element contains the number of elements
   to follow (which are tile ids) */
extern uint16_t* DEFAULT_TILEMAN_TERRAIN_INFO[];

/* default map: default map provides the player with an interface to enter the default game
   or to run/connect to a version; the default map chunk is designed to be tiled */
extern const struct pok_location DEFAULT_MAP_START_LOCATION;
extern const struct pok_location DEFAULT_MAP_WARP_LOCATION;
extern const struct pok_location DEFAULT_MAP_CONSOLE_LOCATION;
extern const struct pok_location DEFAULT_MAP_DOOR_LOCATIONS[];
extern const struct pok_size DEFAULT_MAP_CHUNK_SIZE;
extern const uint16_t DEFAULT_MAP_CHUNK[];
extern const uint16_t DEFAULT_MAP_PASSABLE_TILE;
extern const uint16_t DEFAULT_MAP_DOOR_TILE;

#endif
