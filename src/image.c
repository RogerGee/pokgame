/* image.c - pokgame */
#include "image.h"
#include "error.h"
#include "protocol.h"
#include <png.h>
#include <stdlib.h>

struct pok_image* pok_image_new()
{
    struct pok_image* img;
    img = malloc(sizeof(struct pok_image));
    if (img == NULL) {
        pok_exception_flag_memory_error();
        return NULL;
    }
    img->width = 0;
    img->height = 0;
    img->texref = 0;
    img->fillref.r = img->fillref.g = img->fillref.b = 0;
    img->pixels.dataRGB = NULL;
    img->flags = pok_image_flag_none;
    return img;
}
struct pok_image* pok_image_new_rgb_fill(uint32_t width,uint32_t height,union pixel fillPixel)
{
    size_t i, d, n;
    struct pok_image* img;
    d = width*height;
    n = d * sizeof(union pixel);
    if (d > POK_MAX_IMAGE_SIZE) {
        pok_exception_new_ex(pok_ex_image,pok_ex_image_too_big);
        return NULL;
    }
    img = malloc(sizeof(struct pok_image));
    if (img == NULL) {
        pok_exception_flag_memory_error();
        return NULL;
    }
    img->width = width;
    img->height = height;
    img->texref = 0;
    img->fillref.r = img->fillref.g = img->fillref.b = 0;
    img->flags = pok_image_flag_none;
    img->pixels.dataRGB = malloc(n);
    for (i = 0;i < d;++i)
        img->pixels.dataRGB[i] = fillPixel;
    return img;
}
struct pok_image* pok_image_new_rgb_fillref(uint32_t width,uint32_t height,union pixel fillPixel)
{
    struct pok_image* img;
    img = malloc(sizeof(struct pok_image));
    if (img == NULL) {
        pok_exception_flag_memory_error();
        return NULL;
    }
    img->width = width;
    img->height = height;
    img->texref = 0;
    img->fillref = fillPixel;
    img->flags = pok_image_flag_none;
    img->pixels.data = NULL;
    return img;
}
struct pok_image* pok_image_new_rgba_fill(uint32_t width,uint32_t height,union alpha_pixel fillPixel)
{
    size_t i, d, n;
    struct pok_image* img;
    d = width * height;
    n = d * sizeof(union alpha_pixel);
    if (n > POK_MAX_IMAGE_SIZE) {
        pok_exception_new_ex(pok_ex_image,pok_ex_image_too_big);
        return NULL;
    }
    img = malloc(sizeof(struct pok_image));
    if (img == NULL) {
        pok_exception_flag_memory_error();
        return NULL;
    }
    img->width = width;
    img->height = height;
    img->texref = 0;
    img->fillref.r = img->fillref.g = img->fillref.b = 0;
    img->flags = pok_image_flag_none;
    img->pixels.dataRGBA = malloc(n);
    for (i = 0;i < d;++i)
        img->pixels.dataRGBA[i] = fillPixel;
    return img;
}
struct pok_image* pok_image_new_byval_rgb(uint32_t width,uint32_t height,const byte_t* dataRGB)
{
    /* copy pixel data into structure; each pixel is 3-bytes long and can be cast to a
       'pixel' structure; we sincerely hope the caller has the data formatted correctly */
    size_t i, sz, imgSz;
    struct pok_image* img;
    sz = width * height;
    imgSz = sz * sizeof(union pixel);
    if (imgSz > POK_MAX_IMAGE_SIZE) {
        pok_exception_new_ex(pok_ex_image,pok_ex_image_too_big);
        return NULL;
    }
    img = malloc(sizeof(struct pok_image));
    if (img == NULL) {
        pok_exception_flag_memory_error();
        return NULL;
    }
    img->width = width;
    img->height = height;
    img->texref = 0;
    img->fillref.r = img->fillref.g = img->fillref.b = 0;
    img->flags = pok_image_flag_none;
    img->pixels.dataRGB = malloc(imgSz);
    if (img->pixels.data == NULL) {
        free(img);
        pok_exception_flag_memory_error();
        return NULL;
    }
    for (i = 0;i < sz;++i) {
        size_t j;
        for (j = 0;j < 3;++j)
            img->pixels.dataRGB[i].rgb[j] = *dataRGB++;
    }
    return img;
}
struct pok_image* pok_image_new_byval_rgba(uint32_t width,uint32_t height,const byte_t* dataRGBA)
{
    /* copy pixel data into structure; each pixel is 4-bytes long and can be cast to an
       'alpha_pixel' structure; we sincerely hope the caller has the data formatted correctly */
    size_t i, sz, imgSz;
    struct pok_image* img;
    sz = width * height;
    imgSz = sz * sizeof(union alpha_pixel);
    if (imgSz > POK_MAX_IMAGE_SIZE) {
        pok_exception_new_ex(pok_ex_image,pok_ex_image_too_big);
        return NULL;
    }
    img = malloc(sizeof(struct pok_image));
    if (img == NULL) {
        pok_exception_flag_memory_error();
        return NULL;
    }
    img->width = width;
    img->height = height;
    img->texref = 0;
    img->fillref.r = img->fillref.g = img->fillref.b = 0;
    img->flags = pok_image_flag_alpha;
    img->pixels.dataRGBA = malloc(imgSz);
    if (img->pixels.data == NULL) {
        free(img);
        pok_exception_flag_memory_error();
        return NULL;
    }
    for (i = 0;i < sz;++i) {
        size_t j;
        for (j = 0;j < 4;++j)
            img->pixels.dataRGBA[i].rgba[j] = *dataRGBA++;
    }
    return img;
}
struct pok_image* pok_image_new_byref_rgb(uint32_t width,uint32_t height,const byte_t* dataRGB)
{
    /* just refer to pixel data; each pixel is 3-bytes long and can be cast to a
       'pixel' structure; we sincerely hope the caller has the data formatted correctly */
    struct pok_image* img;
    img = malloc(sizeof(struct pok_image));
    if (img == NULL) {
        pok_exception_flag_memory_error();
        return NULL;
    }
    img->width = width;
    img->height = height;
    img->texref = 0;
    img->fillref.r = img->fillref.g = img->fillref.b = 0;
    img->flags = pok_image_flag_byref;
    img->pixels.dataRGB = (union pixel*)dataRGB;
    return img;
}
struct pok_image* pok_image_new_byref_rgba(uint32_t width,uint32_t height,const byte_t* dataRGBA)
{
    /* just refer to pixel data; each pixel is 3-bytes long and can be cast to a
       'pixel' structure; we sincerely hope the caller has the data formatted correctly */
    struct pok_image* img;
    img = malloc(sizeof(struct pok_image));
    if (img == NULL) {
        pok_exception_flag_memory_error();
        return NULL;
    }
    img->width = width;
    img->height = height;
    img->texref = 0;
    img->fillref.r = img->fillref.g = img->fillref.b = 0;
    img->flags = pok_image_flag_byref | pok_image_flag_alpha;
    img->pixels.dataRGBA = (union alpha_pixel*)dataRGBA;
    return img;
}
struct pok_image* pok_image_new_subimage(struct pok_image* src,uint32_t x,uint32_t y,uint32_t width,uint32_t height)
{
    /* create a new image that contains a subimage of the specified source image */
    uint32_t r, c, i, j;
    struct pok_image* img;
    if (x>=src->width || y>=src->height || x+width>src->width || y+height>src->height) {
        /* subrange does not exist */
        pok_exception_new_ex(pok_ex_image,pok_ex_image_invalid_subimage);
        return NULL;
    }
    img = malloc(sizeof(struct pok_image));
    if (img == NULL) {
        pok_exception_flag_memory_error();
        return NULL;
    }
    img->width = width;
    img->height = height;
    img->texref = 0;
    img->fillref.r = img->fillref.g = img->fillref.b = 0;
    img->flags = src->flags & ~pok_image_flag_byref; /* preserve all but byref flag */
    if (img->flags & pok_image_flag_alpha) {
        img->pixels.dataRGBA = malloc(sizeof(union alpha_pixel) * width * height);
        if (img->pixels.dataRGBA == NULL) {
            free(img);
            pok_exception_flag_memory_error();
            return NULL;
        }
        for (i = 0,r = 0,j = y*src->width+x;r < height;++r,j+=src->width) {
            uint32_t k = j; /* j refers to start of subimage row */
            for (c = 0;c < width;++c,++i,++k)
                img->pixels.dataRGBA[i] = src->pixels.dataRGBA[k];
        }
    }
    else {
        img->pixels.dataRGB = malloc(sizeof(union pixel) * width * height);
        if (img->pixels.dataRGB == NULL) {
            free(img);
            pok_exception_flag_memory_error();
            return NULL;
        }
        for (i = 0,r = 0,j = y*src->width+x;r < height;++r,j+=src->width) {
            uint32_t k = j; /* j refers to start of subimage row */
            for (c = 0;c < width;++c,++i,++k)
                img->pixels.dataRGB[i] = src->pixels.dataRGB[k];
        }
    }
    return img;
}
void pok_image_free(struct pok_image* img)
{
    if ((img->flags&pok_image_flag_byref) == 0 && img->pixels.data != NULL)
        free(img->pixels.data);
    free(img);
}
void pok_image_unload(struct pok_image* img)
{
    /* the 'unload' function discards image data but preserves the image object; this
       is useful when the image has been loaded as a texture and no longer requires its
       pixel data */
    if ((img->flags & pok_image_flag_byref) == 0 && img->pixels.data != NULL)
        free(img->pixels.data);
    img->pixels.data = NULL;
}
bool_t pok_image_save(struct pok_image* img,struct pok_data_source* dsrc)
{
    size_t dummy;
    if (img->pixels.data == NULL) {
        pok_exception_new_ex(pok_ex_image,pok_ex_image_unrecognized_format);
        return FALSE;
    }
    return pok_data_stream_write_byte(dsrc,img->flags&pok_image_flag_alpha?0x1:0x00)
        && pok_data_stream_write_uint32(dsrc,img->width)
        && pok_data_stream_write_uint32(dsrc,img->height)
        && pok_data_source_write(dsrc,img->pixels.data,
            (img->flags&pok_image_flag_alpha ? sizeof(union alpha_pixel) : sizeof (union pixel)) * img->width * img->height,&dummy);
}
static bool_t pok_image_fromfile_generic(struct pok_image* img,struct pok_data_source* dsrc,size_t bytecnt)
{
    size_t bytesout;
    img->pixels.data = malloc(bytecnt);
    if (img->pixels.data == NULL) {
        pok_exception_flag_memory_error();
        return FALSE;
    }
    if ( !pok_data_source_read_to_buffer(dsrc,img->pixels.data,bytecnt,&bytesout) ) {
        free(img->pixels.data);
        img->pixels.data = NULL;
        return FALSE;
    }
    if (bytesout != bytecnt) {
        free(img->pixels.data);
        img->pixels.data = NULL;
        pok_exception_new_ex(pok_ex_image,pok_ex_image_incomplete_fromfile);
        return FALSE;
    }
    return TRUE;
}
bool_t pok_image_open(struct pok_image* img,struct pok_data_source* dsrc)
{
    uint8_t alpha;
    size_t imgSz;
    if (img->pixels.data != NULL) {
        pok_exception_new_ex(pok_ex_image,pok_ex_image_already_loaded);
        return FALSE;
    }
    /* read image alpha status, width and height; check image dimensions */
    if (!pok_data_stream_read_byte(dsrc,&alpha) || !pok_data_stream_read_uint32(dsrc,&img->width)
        || !pok_data_stream_read_uint32(dsrc,&img->height))
        return FALSE;
    img->flags = alpha ? pok_image_flag_alpha : pok_image_flag_none;
    imgSz = (alpha ? sizeof (union alpha_pixel) : sizeof (union pixel)) * img->width * img->height;
    if (imgSz > POK_MAX_IMAGE_SIZE) {
        pok_exception_new_ex(pok_ex_image,pok_ex_image_too_big);
        return FALSE;
    }
    /* allocate and read pixels */
    if ( !pok_image_fromfile_generic(img,dsrc,imgSz) )
        return FALSE;
    return TRUE;
}
bool_t pok_image_fromfile_rgb(struct pok_image* img,const char* file)
{
    /* read rgb image from file; first 8 bytes specify image width and height */
    size_t imgSz;
    struct pok_data_source* fin;
    if (img->pixels.data != NULL) {
        pok_exception_new_ex(pok_ex_image,pok_ex_image_already_loaded);
        return FALSE;
    }
    fin = pok_data_source_new_file(file,pok_filemode_open_existing,pok_iomode_read);
    if (fin == NULL)
        return FALSE;
    if (!pok_data_stream_read_uint32(fin,&img->width) || !pok_data_stream_read_uint32(fin,&img->height)) {
        pok_data_source_free(fin);
        return FALSE;
    }
    imgSz = img->width * img->height * sizeof(union pixel);
    if (imgSz > POK_MAX_IMAGE_SIZE) {
        pok_data_source_free(fin);
        pok_exception_new_ex(pok_ex_image,pok_ex_image_too_big);
        return FALSE;
    }
    if ( !pok_image_fromfile_generic(img,fin,imgSz) ) {
        pok_data_source_free(fin);
        return FALSE;
    }
    pok_data_source_free(fin);
    return TRUE;
}
bool_t pok_image_fromfile_rgb_ex(struct pok_image* img,const char* file,uint32_t width,uint32_t height)
{
    /* read rgb image from file using specified width and height; the file input is just interpreted as pixel data */
    size_t imgSz;
    struct pok_data_source* fin;
    imgSz = width * height * sizeof(union pixel);
    if (imgSz > POK_MAX_IMAGE_SIZE) {
        pok_exception_new_ex(pok_ex_image,pok_ex_image_too_big);
        return FALSE;
    }
    if (img->pixels.data != NULL) {
        pok_exception_new_ex(pok_ex_image,pok_ex_image_already_loaded);
        return FALSE;
    }
    fin = pok_data_source_new_file(file,pok_filemode_open_existing,pok_iomode_read);
    if (fin == NULL)
        return FALSE;
    if ( !pok_image_fromfile_generic(img,fin,imgSz) ) {
        pok_data_source_free(fin);
        return FALSE;
    }
    img->width = width;
    img->height = height;
    pok_data_source_free(fin);
    return TRUE;
}
bool_t pok_image_fromfile_rgba(struct pok_image* img,const char* file)
{
    /* read rgba image from file; first 8 bytes specify image width and height */
    size_t imgSz;
    struct pok_data_source* fin;
    if (img->pixels.data != NULL) {
        pok_exception_new_ex(pok_ex_image,pok_ex_image_already_loaded);
        return FALSE;
    }
    fin = pok_data_source_new_file(file,pok_filemode_open_existing,pok_iomode_read);
    if (fin == NULL)
        return FALSE;
    if (!pok_data_stream_read_uint32(fin,&img->width) || !pok_data_stream_read_uint32(fin,&img->height)) {
        pok_data_source_free(fin);
        return FALSE;
    }
    imgSz = img->width * img->height * sizeof(union alpha_pixel);
    if (imgSz > POK_MAX_IMAGE_SIZE) {
        pok_data_source_free(fin);
        pok_exception_new_ex(pok_ex_image,pok_ex_image_too_big);
        return FALSE;
    }
    if ( !pok_image_fromfile_generic(img,fin,imgSz) ) {
        pok_data_source_free(fin);
        return FALSE;
    }
    img->flags = pok_image_flag_alpha;
    pok_data_source_free(fin);
    return TRUE;
}
bool_t pok_image_fromfile_rgba_ex(struct pok_image* img,const char* file,uint32_t width,uint32_t height)
{
    /* read rgba image from file using specified width and height; the file input is just interpreted as pixel data */
    size_t imgSz;
    struct pok_data_source* fin;
    imgSz = width * height * sizeof(union alpha_pixel);
    if (imgSz > POK_MAX_IMAGE_SIZE) {
        pok_exception_new_ex(pok_ex_image,pok_ex_image_too_big);
        return FALSE;
    }
    if (img->pixels.data != NULL) {
        pok_exception_new_ex(pok_ex_image,pok_ex_image_already_loaded);
        return FALSE;
    }
    fin = pok_data_source_new_file(file,pok_filemode_open_existing,pok_iomode_read);
    if (fin == NULL)
        return FALSE;
    if ( !pok_image_fromfile_generic(img,fin,imgSz) ) {
        pok_data_source_free(fin);
        return FALSE;
    }
    img->width = width;
    img->height = height;
    img->flags = pok_image_flag_alpha;
    pok_data_source_free(fin);
    return TRUE;
}
enum pok_network_result pok_image_netread(struct pok_image* img,struct pok_data_source* dsrc,struct pok_netobj_readinfo* info)
{
    /* read the image from a data source; incomplete transfers are flagged and the user can use a 'pok_netobj_readinfo' object
       to re-call this function to attempt to read the remaining data
       - data structure sent over network: 
       [1 byte] hasAlpha (true if non-zero)
       [4 bytes] width
       [4 bytes] height
       [n bytes] pixel-data, where n = width*height * (4 if alpha channel, else 3) */
    byte_t* recv;
    uint8_t alpha;
    size_t amount;
    byte_t* pixdata;
    size_t allocation;
    enum pok_network_result result = pok_net_already;
    switch (info->fieldProg) {
    case 0:
        /* read alpha flag */
        if (img->pixels.data != NULL) {
            pok_exception_new_ex(pok_ex_image,pok_ex_image_already_loaded);
            return pok_net_failed;
        }
        pok_data_stream_read_byte(dsrc,&alpha);
        if ((result = pok_netobj_readinfo_process(info)) != pok_net_completed)
            break;
        img->flags = alpha ? pok_image_flag_alpha : pok_image_flag_none;
    case 1:
        /* read width */
        pok_data_stream_read_uint32(dsrc,&img->width);
        if ((result = pok_netobj_readinfo_process(info)) != pok_net_completed)
            break;
    case 2:
        /* read height */
        pok_data_stream_read_uint32(dsrc,&img->height);
        if ((result = pok_netobj_readinfo_process(info)) != pok_net_completed)
            break;
    case 3:
        /* read pixel-data */
        allocation = (img->flags&pok_image_flag_alpha) ? sizeof(union alpha_pixel) : sizeof(union pixel);
        if (img->pixels.data == NULL) {
            amount = allocation * img->width * img->height;
            if (amount > POK_MAX_IMAGE_SIZE) {
                pok_exception_new_ex(pok_ex_image,pok_ex_image_too_big);
                return pok_net_failed;
            }
            img->pixels.data = malloc(amount);
            if (img->pixels.data == NULL) {
                pok_exception_flag_memory_error();
                return pok_net_failed;
            }
        }
        pixdata = (byte_t*)img->pixels.data + allocation * (info->depth[1]*img->width + info->depth[0]);
        while (info->depth[1] < img->height) {
            recv = pok_data_source_read(dsrc,allocation,&amount);
            if (recv != NULL) {
                if (amount < allocation) {
                    pok_data_source_unread(dsrc,amount);
                    return pok_net_incomplete;
                }
                for (;amount > 0;--amount)
                    *pixdata++ = *recv++;
            }
            if ((result = pok_netobj_readinfo_process_depth(info,0)) != pok_net_completed)
                break;
            if (info->depth[0] >= img->width) {
                ++info->depth[1];
                info->depth[0] = 0;
            }
        }
        if ((result = pok_netobj_readinfo_process(info)) != pok_net_completed)
            break;
    }
    return result;
}
enum pok_network_result pok_image_netread_ex(struct pok_image* img,uint32_t width,uint32_t height,struct pok_data_source* dsrc,
    struct pok_netobj_readinfo* info)
{
    /* read the image from a data source; incomplete transfers are flagged and the user can use a 'pok_netobj_readinfo' object
       to re-call this function to attempt to read the remaining data; this varient assumes the specified width and height
       and should be used when the image dimension is implied between peers
       - data structure sent over network: 
       [1 byte] alpha
       [n bytes] pixel-data, where n = width*height * (4 if alpha channel, else 3) */

    uint8_t alpha;
    size_t amount;
    byte_t* pixdata;
    size_t allocation;
    enum pok_network_result result = pok_net_already;
    /* read flags */
    switch (info->fieldProg) {
    case 0:
        /* read alpha flag */
        if (img->pixels.data != NULL) {
            pok_exception_new_ex(pok_ex_image,pok_ex_image_already_loaded);
            return pok_net_failed;
        }
        img->width = width;
        img->height = height;
        pok_data_stream_read_byte(dsrc,&alpha);
        if ((result = pok_netobj_readinfo_process(info)) != pok_net_completed)
            break;
        img->flags = alpha ? pok_image_flag_alpha : pok_image_flag_none;
    case 1:
        /* read pixel data */
        allocation = (img->flags&pok_image_flag_alpha) ? sizeof(union alpha_pixel) : sizeof(union pixel);
        if (img->pixels.data == NULL) {
            amount = allocation * width * height;
            if (amount > POK_MAX_IMAGE_SIZE) {
                pok_exception_new_ex(pok_ex_image,pok_ex_image_too_big);
                return pok_net_failed;
            }
            img->pixels.data = malloc(amount);
            if (img->pixels.data == NULL) {
                pok_exception_flag_memory_error();
                return pok_net_failed;
            }
        }
        pixdata = (byte_t*)img->pixels.data + allocation * (info->depth[1]*width + info->depth[0]);
        while (info->depth[1] < height) {
            byte_t* recv;
            recv = pok_data_source_read(dsrc,allocation,&amount);
            if (recv != NULL) {
                if (amount < allocation) {
                    pok_data_source_unread(dsrc,amount);
                    return pok_net_incomplete;
                }
                for (;amount > 0;--amount)
                    *pixdata++ = *recv++;
            }
            if ((result = pok_netobj_readinfo_process_depth(info,0)) != pok_net_completed)
                break;
            if (info->depth[0] >= width) {
                ++info->depth[1];
                info->depth[0] = 0;
            }
        }
        if ((result = pok_netobj_readinfo_process(info)) != pok_net_completed)
            break;
    }
    return result;
}

/* PNG functionality; we link against libpng for this */

/* customize libpng's io routines using our routines defined in net.h */
static void read_data(png_structp pngptr,png_bytep data,png_size_t length)
{
    size_t bytesRead;
    struct pok_data_source* dsrc;
    dsrc = png_get_io_ptr(pngptr);
    pok_data_source_read_to_buffer(dsrc,data,length,&bytesRead);
}
static void write_data(png_structp pngptr,png_bytep data,png_size_t length)
{
    size_t bytesWritten;
    struct pok_data_source* dsrc;
    dsrc = png_get_io_ptr(pngptr);
    pok_data_source_write(dsrc,data,length,&bytesWritten);
}
static void flush_data(png_structp png_ptr)
{
}

struct pok_image* pok_image_png_new(const char* file)
{
    size_t bytesRead;
    png_structp pngptr;
    png_infop infoptr;
    png_bytep sig;
    png_uint_32 i;
    png_uint_32 imgwidth;
    png_uint_32 imgheight;
    png_uint_32 bitdepth;
    png_uint_32 channels;
    png_uint_32 colorType;
    png_bytep ptr;
    png_bytepp rowptrs;
    struct pok_image* img;
    struct pok_data_source* dsrc;

    /* allocate image */
    img = malloc(sizeof(struct pok_image));
    if (img == NULL) {
        pok_exception_flag_memory_error();
        return NULL;
    }

    /* open data source to file */
    dsrc = pok_data_source_new_file(file,pok_filemode_open_existing,pok_iomode_read);
    if (dsrc == NULL) {
        pok_image_free(img);
        return NULL;
    }

    /* verify that file data is png image format by reading signiture bytes*/
    sig = pok_data_source_read(dsrc,8,&bytesRead);
    if (sig == NULL)
        goto fail;
    if (bytesRead != 8 || !png_check_sig(sig,8)) {
        pok_exception_new_ex(pok_ex_image,pok_ex_image_unrecognized_format);
        goto fail;
    }

    /* allocate libpng structures */
    pngptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,NULL,NULL,NULL);
    if (pngptr == NULL) {
        pok_exception_flag_memory_error();
        goto fail;
    }
    infoptr = png_create_info_struct(pngptr);
    if (infoptr == NULL) {
        pok_exception_flag_memory_error();
        goto fail;
    }

    /* setup libpng */
    png_set_read_fn(pngptr,dsrc,read_data); /* custom read function */
    if (setjmp(png_jmpbuf(pngptr)) != 0) { /* error handler */
        pok_exception_new_format("libpng exception in pok_image_png_new()");
        png_destroy_read_struct(&pngptr,&infoptr,NULL);
        goto fail;
    }

    /* read header information */
    png_set_sig_bytes(pngptr,8);
    png_read_info(pngptr,infoptr);

    /* make sure image is correctly formatted */
    imgwidth = png_get_image_width(pngptr,infoptr);
    imgheight = png_get_image_height(pngptr,infoptr);
    bitdepth = png_get_bit_depth(pngptr,infoptr);
    channels = png_get_channels(pngptr,infoptr);
    colorType = png_get_color_type(pngptr,infoptr);
    switch (colorType) {
    case PNG_COLOR_TYPE_PALETTE:
        png_set_palette_to_rgb(pngptr);
        channels = 3;
        colorType = PNG_COLOR_TYPE_RGB;
        break;
    case PNG_COLOR_TYPE_GRAY:
        if (bitdepth < 8)
            png_set_expand_gray_1_2_4_to_8(pngptr);
        bitdepth = 8;
        colorType = PNG_COLOR_TYPE_RGB;
        break;
    }
    if (bitdepth != 8 || (colorType != PNG_COLOR_TYPE_RGB && colorType != PNG_COLOR_TYPE_RGBA)) {
        pok_exception_new_ex(pok_ex_image,pok_ex_image_bad_color_format);
        png_destroy_read_struct(&pngptr,&infoptr,NULL);
        goto fail;
    }

    /* setup image object */
    img->width = imgwidth;
    img->height = imgheight;
    if (colorType == PNG_COLOR_TYPE_RGB)
        img->pixels.dataRGB = malloc(sizeof(union pixel) * imgheight * imgwidth);
    else { /* PNG_COLOR_TYPE_RGBA */
        img->flags |= pok_image_flag_alpha;
        img->pixels.dataRGBA = malloc(sizeof(union alpha_pixel) * imgheight * imgwidth);
    }
    if (img->pixels.data == NULL) {
        pok_exception_flag_memory_error();
        png_destroy_read_struct(&pngptr,&infoptr,NULL);
        goto fail;
    }
    rowptrs = malloc(sizeof(png_bytep) * imgheight);
    if (rowptrs == NULL) {
        pok_exception_flag_memory_error();
        png_destroy_read_struct(&pngptr,&infoptr,NULL);
        goto fail;
    }
    ptr = img->pixels.data;
    for (i = 0;i < imgheight;++i) {
        /* note: we have already asserted that bitdepth is 8 */
        rowptrs[i] = ptr;
        ptr += imgwidth * channels;
    }

    /* read image data */
    png_read_image(pngptr,rowptrs);

    png_destroy_read_struct(&pngptr,&infoptr,NULL);
    pok_data_source_free(dsrc);
    free(rowptrs);
    return img;

fail:
    pok_image_free(img);
    pok_data_source_free(dsrc);
    return NULL;

}
void pok_image_png_save(struct pok_image* img,const char* file)
{
}
