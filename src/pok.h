/* pok.h
    This file provides an Application Programming Interface for
   pokgame game version servers.
*/
#ifndef POKGAME_API
#define POKGAME_API
#include <stdlib.h>
#include <stdint.h>
#include "protocol.h"
#include "types.h"

/* POKGAME VERSION SERVER CORE LIBRARY */

/* pok_init: initialize pokgame version server library

   dimension        - tile/sprite dimension for graphics
   windowWidth      - number of tiles spanning game window's width
   windowHeight     - number of tiles spanning game window's height
   playerLocationX  - column in which to place player sprite on screen
   playerLocationY  - row in which to place player sprite on screen
   playerOffsetX    - pixel offset within column tile for player sprite
   playerOffsetY    - pixel offset within row tile for player sprite
   doServer         - if non-zero, then library uses network sockets to host game;
                      othewise the library uses the standard input/output

   'default' variant: lets the client specify graphics parameters
*/
void pok_init(
    uint16_t dimension,
    uint16_t windowWidth,
    uint16_t windowHeight,
    uint16_t playerLocationX,
    uint16_t playerLocationY,
    uint16_t playerOffsetX,
    uint16_t playerOffsetY,
    bool_t doServer);
void pok_init_default(bool_t doServer);

/* pok_close: close pokgame version server library */
void pok_close();

/* UTILITY LIBRARY */

/* pok_util_compute_chunk_size: computes a favorable chunk size given the specified dimensions of a map

   width, height - the dimensions of the 2d map object
   max           - the maximum chunk dimension to produce
   chunkSize     - the function will place the chunk size dimensions
   lefttop       - the function will place the number of columns and rows that are unused in the left-most/top-most
                   chunks
   rightbottom   - the function will place the number of columns and rows that are unused in the right-most/bottom-most
                   chunks

   return:       a size structure indicating the size of the map dimensionalized with the newly computed chunk size
*/
struct pok_size pok_util_compute_chunk_size(
    uint32_t width,
    uint32_t height,
    const uint32_t max,
    struct pok_size* chunkSize,
    struct pok_size* lefttop,
    struct pok_size* rightbottom);

#endif
