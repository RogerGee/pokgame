/* pok-util.c - pokgame utility library implementation */
#include "pok.h"
#include <stdlib.h>

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
    if (width <= max)
        size.columns = 1;
    else {
        while (width > max) {
            r = width % 2;
            width /= 2;
            width += r;
            (tog ? lefttop : rightbottom)->columns += r;
            tog = !tog;
            size.columns += 2;
        }
    }
    tog = TRUE;
    if (height <= max)
        size.rows = 1;
    else {
        while (height > max) {
            r = height % 2;
            height /= 2;
            height += r;
            (tog ? lefttop : rightbottom)->rows += r;
            tog = !tog;
            size.rows += 2;
        }
    }
    chunkSize->columns = width;
    chunkSize->rows = height;
    return size;
}

enum pok_direction pok_util_is_next_to(const struct pok_size* chunkSize,const struct pok_point* chunk1,const struct pok_point* chunk2,
    const struct pok_location* pos1, const struct pok_location* pos2)
{
    int dif;
    if (chunk1->X == chunk2->X && chunk1->Y == chunk2->Y) {
        /* case 1: within same chunk */
        if (pos1->column == pos2->column) {
            /* case 1.1: same column */
            dif = pos1->row - pos2->row;
            if (dif == -1)
                return pok_direction_up;
            else if (dif == 1)
                return pok_direction_down;
        }
        else if (pos1->row == pos2->row) {
            /* case 1.2: same row */
            dif = pos1->column - pos2->column;
            if (dif == -1)
                return pok_direction_left;
            else if (dif == 1)
                return pok_direction_right;
        }
    }
    else {
        /* case 2: within adjacent chunks */
        if (abs(chunk1->X - chunk2->X) == 1) {
            /* case 2.1: vertically adjacent chunks with shared column tile position */
            if (pos1->column == pos2->column) {
                if (pos1->row == 0 && pos2->row == chunkSize->rows - 1)
                    return pok_direction_up;
                else if (pos1->row == chunkSize->rows - 1 && pos2->row == 0)
                    return pok_direction_down;
            }
        }
        else if (abs(chunk1->Y - chunk2->Y) == 1) {
            /* case 2.2: horizontally adjacent chunks with shared row tile position */
            if (pos1->row == pos2->row) {
                if (pos1->column == 0 && pos2->column == chunkSize->columns - 1)
                    return pok_direction_left;
                else if (pos1->column == chunkSize->columns - 1 && pos2->column == 0)
                    return pok_direction_right;
            }
        }
    }
    return pok_direction_none;
}

