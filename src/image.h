/* image.h - pokgame */
#ifndef POKGAME_IMAGE_H
#define POKGAME_IMAGE_H
#include "net.h"

/* exception kinds generated by this module */
enum pok_ex_image
{
    pok_ex_image_unrecognized_format, /* image data format was unrecognized */
    pok_ex_image_too_big, /* image resolution was too big */
    pok_ex_image_invalid_subimage, /* subimage does not exist */
    pok_ex_image_already_loaded, /* image was already initialized */
    pok_ex_image_incomplete_fromfile /* image being read from file was incomplete */
};

/* define 3- and 4-byte pixel structures (with and without alpha); the unions (which should
   NOT employ padding) are used to modify pixel data easily */
union pixel
{
    byte_t rgb[3]; /* this needs to be the first member */
    byte_t r;
    byte_t g;
    byte_t b;
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

/* pok_image: represents an array of pixels; the pixel data can be locally managed or used by
   reference if it is managed elsewhere; in either case, the pixel data can be (and thus should
   be able to be) modified by another context */
struct pok_image
{
    uint8_t flags;
    uint32_t width;
    uint32_t height;
    uint32_t texref;
    union { /* pixel data stored row by row */
        void* data;
        union pixel* dataRGB;
        union alpha_pixel* dataRGBA;
    } pixels;
};
struct pok_image* pok_image_new(); /* new empty image */
struct pok_image* pok_image_new_rgb_fill(uint32_t width,uint32_t height,union pixel fillPixel);
struct pok_image* pok_image_new_rgba_fill(uint32_t width,uint32_t height,union alpha_pixel fillPixel);
struct pok_image* pok_image_new_byval_rgb(uint32_t width,uint32_t height,const byte_t* dataRGB);
struct pok_image* pok_image_new_byval_rgba(uint32_t width,uint32_t height,const byte_t* dataRGBA);
struct pok_image* pok_image_new_byref_rgb(uint32_t width,uint32_t height,const byte_t* dataRGB);
struct pok_image* pok_image_new_byref_rgba(uint32_t width,uint32_t height,const byte_t* dataRGBA);
struct pok_image* pok_image_new_subimage(struct pok_image* src,uint32_t x,uint32_t y,uint32_t width,uint32_t height);
void pok_image_free(struct pok_image* img);
bool_t pok_image_save(struct pok_image* img,struct pok_data_source* dsrc);
bool_t pok_image_open(struct pok_image* img,struct pok_data_source* dsrc);
bool_t pok_image_fromfile_rgb(struct pok_image* img,const char* file);
bool_t pok_image_fromfile_rgb_ex(struct pok_image* img,const char* file,uint32_t width,uint32_t height);
bool_t pok_image_fromfile_rgba(struct pok_image* img,const char* file);
bool_t pok_image_fromfile_rgba_ex(struct pok_image* img,const char* file,uint32_t width,uint32_t height);
enum pok_network_result pok_image_netread(struct pok_image* img,struct pok_data_source* dsrc,struct pok_netobj_readinfo* info);
enum pok_network_result pok_image_netread_ex(struct pok_image* img,uint32_t width,uint32_t height,struct pok_data_source* dsrc,
    struct pok_netobj_readinfo* info);

/* these image constructors provide alternate input formats for image data; they are destroyed
   like any 'pok_image' using 'pok_image_free' */

/* using the Windows bitmap format */
struct pok_image* pok_image_bmp_new(const char* file);
void pok_image_bmp_save(struct pok_image* bmp);

/* using portable network graphics */
struct pok_image_png* pok_image_png_new(const char* file);
void pok_image_png_save(struct pok_image* png);

#endif
