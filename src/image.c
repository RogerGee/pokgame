/* image.c - pokgame */
#include "image.h"
#include "error.h"
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
    img->pixels.dataRGB = NULL;
    return img;
}
struct pok_image* pok_image_new_rgb_fill(uint32_t width,uint32_t height,union pixel fillPixel)
{
    size_t i, d, n;
    struct pok_image* img;
    if (width>MAX_IMAGE_DIMENSION || height>MAX_IMAGE_DIMENSION) {
        pok_exception_new_ex(pok_ex_image,pok_ex_image_too_big);
        return NULL;
    }
    img = malloc(sizeof(struct pok_image));
    d = width*height;
    n = d * sizeof(union pixel);
    img->width = width;
    img->height = height;
    img->flags = pok_image_flag_none;
    img->pixels.dataRGB = malloc(n);
    for (i = 0;i < d;++i)
        img->pixels.dataRGB[i] = fillPixel;
    return img;
}
struct pok_image* pok_image_new_rgba_fill(uint32_t width,uint32_t height,union alpha_pixel fillPixel)
{
    size_t i, d, n;
    struct pok_image* img;
    if (width>MAX_IMAGE_DIMENSION || height>MAX_IMAGE_DIMENSION) {
        pok_exception_new_ex(pok_ex_image,pok_ex_image_too_big);
        return NULL;
    }
    img = malloc(sizeof(struct pok_image));
    d = width * height;
    n = d * sizeof(union alpha_pixel);
    img->width = width;
    img->height = height;
    img->flags = pok_image_flag_none;
    img->pixels.dataRGBA = malloc(n);
    for (i = 0;i < d;++i)
        img->pixels.dataRGBA[i] = fillPixel;
    return img;
}
struct pok_image* pok_image_new_byval_rgb(uint32_t width,uint32_t height,byte_t* dataRGB)
{
    /* copy pixel data into structure; each pixel is 3-bytes long and can be cast to a
       'pixel' structure; we sincerely hope the caller has the data formatted correctly */
    size_t i, sz;
    struct pok_image* img;
    if (width>MAX_IMAGE_DIMENSION || height>MAX_IMAGE_DIMENSION) {
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
    img->flags = pok_image_flag_none;
    sz = width * height;
    img->pixels.dataRGB = malloc(sizeof(union alpha_pixel) * sz);
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
struct pok_image* pok_image_new_byval_rgba(uint32_t width,uint32_t height,byte_t* dataRGBA)
{
    /* copy pixel data into structure; each pixel is 4-bytes long and can be cast to an
       'alpha_pixel' structure; we sincerely hope the caller has the data formatted correctly */
    size_t i, sz;
    struct pok_image* img;
    if (width>MAX_IMAGE_DIMENSION || height>MAX_IMAGE_DIMENSION) {
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
    img->flags = pok_image_flag_alpha;
    sz = width * height;
    img->pixels.dataRGBA = malloc(sizeof(union alpha_pixel) * sz);
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
struct pok_image* pok_image_new_byref_rgb(uint32_t width,uint32_t height,byte_t* dataRGB)
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
    img->flags = pok_image_flag_byref;
    img->pixels.dataRGB = (union pixel*)dataRGB;
    return img;
}
struct pok_image* pok_image_new_byref_rgba(uint32_t width,uint32_t height,byte_t* dataRGBA)
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
    if ((img->flags&pok_image_flag_byref) == 0)
        free(img->pixels.data);
    free(img);
}
enum pok_network_result pok_image_netread(struct pok_image* img,struct pok_data_source* dsrc,struct pok_netobj_readinfo* info)
{
    /* read the image from a data source; incomplete transfers are flagged and the user can use a 'pok_netobj_readinfo' object
       to re-call this function to attempt to read the remaining data
        - data structure sent over network: 
           [1 byte] hasAlpha (true if non-zero)
           [4 bytes] width
           [4 bytes] height
           [n bytes] pixel-data, where n = widht*height * (4 if alpha channel, else 3) */
    enum pok_network_result result = pok_net_already;
    /* read flags, width and height */
    if (info->fieldProg == 0) {
        uint8_t alpha;
        pok_data_stream_read_byte(dsrc,&alpha);
        result = pok_netobj_readinfo_process(info);
        if (result == pok_net_completed)
            img->flags = alpha ? pok_image_flag_alpha : pok_image_flag_none;
    }
    if (info->fieldProg == 1) {
        pok_data_stream_read_uint32(dsrc,&img->width);
        result = pok_netobj_readinfo_process(info);
    }
    if (info->fieldProg == 2) {
        pok_data_stream_read_uint32(dsrc,&img->height);
        result = pok_netobj_readinfo_process(info);
    }
    if (info->fieldProg == 3) {
        byte_t* pixdata;
        size_t allocation = (img->flags&pok_image_flag_alpha) ? sizeof(union alpha_pixel) : sizeof(union pixel);
        if (info->depth==0 && img->pixels.data==NULL) {
            img->pixels.data = malloc(allocation * img->width * img->height);
            if (img->pixels.data == NULL) {
                pok_exception_flag_memory_error();
                return pok_net_failed;
            }
        }
        pixdata = (byte_t*)img->pixels.data + allocation * (info->depth[1]*img->width + info->depth[0]);
        while (info->depth[1] < img->height) {
            byte_t* recv;
            size_t amount;
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
        result = pok_netobj_readinfo_process(info);
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
           [n bytes] pixel-data, where n = widht*height * (4 if alpha channel, else 3) */
    enum pok_network_result result = pok_net_already;
    /* read flags */
    if (info->fieldProg == 0) {
        uint8_t alpha;
        img->width = width;
        img->height = height;
        pok_data_stream_read_byte(dsrc,&alpha);
        result = pok_netobj_readinfo_process(info);
        if (result == pok_net_completed)
            img->flags = alpha ? pok_image_flag_alpha : pok_image_flag_none;
    }
    if (info->fieldProg == 1) {
        byte_t* pixdata;
        size_t allocation = (img->flags&pok_image_flag_alpha) ? sizeof(union alpha_pixel) : sizeof(union pixel);
        if (info->depth==0 && img->pixels.data==NULL) {
            img->pixels.data = malloc(allocation * width * height);
            if (img->pixels.data == NULL) {
                pok_exception_flag_memory_error();
                return pok_net_failed;
            }
        }
        pixdata = (byte_t*)img->pixels.data + allocation * (info->depth[1]*width + info->depth[0]);
        while (info->depth[1] < height) {
            byte_t* recv;
            size_t amount;
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
        result = pok_netobj_readinfo_process(info);
    }
    return result;
}
