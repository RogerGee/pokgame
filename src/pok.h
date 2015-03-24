/* pok.h
    This file provides an Application Programming Interface for
   pokgame game servers.
*/
#ifndef POKGAME_API
#define POKGAME_API
#include <stdlib.h>
#include <stdint.h>
#include "protocol.h"
#include "types.h"

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
struct pok_size pok_util_compute_chunk_size(uint32_t width,uint32_t height,const uint32_t max,struct pok_size* chunkSize,
    struct pok_size* lefttop,struct pok_size* rightbottom);

#endif
