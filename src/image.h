/* image.h - pokgame */
#ifndef POKGAME_IMAGE_H
#define POKGAME_IMAGE_H
#include "net.h"

/* define 4-byte pixel with alpha */
union _pixel
{
    struct {
        byte_t a;
        byte_t r;
        byte_t g;
        byte_t b;
    } argb;
    uint32_t value;
};

enum pok_image_flags
{
    pok_image_byref = 0x01, /* image data is not owned by image struct */
    pok_image_alpha = 0x02 /* image alpha channel should be employed */
};

struct pok_image
{
    uint16_t flags;
    uint32_t width;
    uint32_t height;
    union _pixel* data; /* pixel data stored row by row */
};
struct pok_image* pok_image_new(uint32_t width,uint32_t height,byte_t* data);
struct pok_image* pok_image_new_byref(uint32_t width,uint32_t height,byte_t* data);
struct pok_image* pok_image_new_fromnet(struct pok_data_source* dsrc);
void pok_image_free(struct pok_image* img);

struct pok_image_bmp
{
    struct pok_image _base;
};
struct pok_image_bmp* pok_image_bmp_new(const char* file);

#endif
