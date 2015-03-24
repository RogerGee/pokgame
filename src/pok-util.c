/* pok-util.c - pokgame utility library implementation */
#include "pok.h"

struct pok_size pok_util_compute_chunk_size(uint32_t width,uint32_t height,const uint32_t max,struct pok_size* chunkSize,
    struct pok_size* lefttop,struct pok_size* rightbottom)
{
    bool_t tog;
    uint16_t r;
    struct pok_size size;
    /* lefttop will refer to unused left-most columns in the left-most chunks and unused top-most rows in the top-most chunks; rightbottom
       will refer to unused right-most columns in the right-most chunks and unused bottom-most rows in the bottom-most chunks */
    lefttop->columns = lefttop->rows = 0;
    rightbottom->columns = rightbottom->rows = 0;
    size.columns = 0;
    size.rows = 0;
    /* find ideal chunk size by dividing dimensions in half; if the dimension is odd at any stage, then
       we add an extra unused column/row to certain "edge" chunks */
    tog = TRUE; /* toggle between lefttop and rightbottom */
    while (width > max) {
        r = width % 2;
        width /= 2;
        width += r;
        (tog ? lefttop : rightbottom)->columns += r;
        tog = !tog;
        size.columns += 2;
    }
    tog = TRUE;
    while (height > max) {
        r = height % 2;
        height /= 2;
        height += r;
        (tog ? lefttop : rightbottom)->rows += r;
        tog = !tog;
        size.rows += 2;
    }
    chunkSize->columns = width;
    chunkSize->rows = height;
    return size;
}
