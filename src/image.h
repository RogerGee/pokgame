/* image.h - pokgame */
#ifndef POKGAME_IMAGE_H
#define POKGAME_IMAGE_H
#include "net.h"

/* exception kinds generated by this module */
enum pok_ex_image
{
    pok_ex_image_unrecognized_format, /* image data format was unrecognized */
    pok_ex_image_protocol_error, /* network data did not adhere to the image protocol */
    pok_ex_image_too_big /* image resolution was too big */
};

/* define 3- and 4-byte pixel structures (with and without alpha); the unions (which should
   NOT employ padding are used to modify pixel data easily */
union pixel
{
    byte_t rgb[3];
};
union alpha_pixel
{
    byte_t rgba[4]; /* RED, GREEN, BLUE, ALPHA components */
    uint32_t value; /* aggregated RGBA value */
};

enum pok_image_flags
{
    pok_image_flag_none = 0x00,
    pok_image_flag_byref = 0x01, /* image data is not owned by image struct */
    pok_image_flag_alpha = 0x02 /* image contains an alpha channel */
};

struct pok_image
{
    uint16_t flags;
    uint32_t width;
    uint32_t height;
    union { /* pixel data stored row by row */
        void* data;
        union pixel* dataRGB;
        union alpha_pixel* dataRGBA;
    } pixels;
};
struct pok_image* pok_image_new();
struct pok_image* pok_image_new_byval_rgb(uint32_t width,uint32_t height,byte_t* dataRGB);
struct pok_image* pok_image_new_byval_rgba(uint32_t width,uint32_t height,byte_t* dataRGBA);
struct pok_image* pok_image_new_byref_rgb(uint32_t width,uint32_t height,byte_t* dataRGB);
struct pok_image* pok_image_new_byref_rgba(uint32_t width,uint32_t height,byte_t* dataRGBA);
void pok_image_free(struct pok_image* img);
enum pok_network_result pok_image_net_read(struct pok_image* img,struct pok_data_source* dsrc,struct pok_netobj_info* info);

struct pok_image_bmp
{
    byte_t comp;
    uint16_t bpp;
    struct pok_image _base;
};
struct pok_image_bmp* pok_image_bmp_new(const char* file);
void pok_image_bmp_free(struct pok_image_bmp* img);

struct pok_image_png
{
    struct pok_image _base;
};
struct pok_image_png* pok_image_png_new(const char* file);
void pok_image_bmp_free(struct pok_image_bmp* img);

#endif
