/* image.c - pokgame */
#include "image.h"
#include "error.h"
#include <stdlib.h>

#define MAX_IMAGE_DIMENSION 1024

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
void pok_image_free(struct pok_image* img)
{
    if ((img->flags&pok_image_flag_byref) == 0)
        free(img->pixels.data);
    free(img);
}
enum pok_network_result pok_image_net_read(struct pok_image* img,struct pok_data_source* dsrc,struct pok_netobj_info* info)
{
    /* read the image from a data source; incomplete transfers are flagged and the user can use a 'pok_netobj_info' object
       to re-call this function to attempt to read the remaining data */
    enum pok_network_result result = pok_net_completed;
    const struct pok_exception* ex;
    /* read flags, width and height */
    if (info->fieldProg == 0) {
        pok_data_stream_read_uint16(dsrc,&img->flags);
        result = pok_netobj_info_process(info);
    }
    if (info->fieldProg == 1) {
        pok_data_stream_read_uint32(dsrc,&img->width);
        result = pok_netobj_info_process(info);
    }
    if (info->fieldProg == 2) {
        pok_data_stream_read_uint32(dsrc,&img->height);
        result = pok_netobj_info_process(info);
    }
    if (info->fieldProg == 3) {
        byte_t* pixdata;
        size_t allocation = (img->flags&pok_image_flag_alpha) ? sizeof(union alpha_pixel) : sizeof(union pixel);
        if (info->depth==0 && img->pixels.data==NULL) {
            allocation *= img->width * img->height;
            img->pixels.data = malloc(allocation);
            if (img->pixels.data == NULL) {
                pok_exception_flag_memory_error();
                return pok_net_failed;
            }
        }
        pixdata = img->pixels.data;
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
            if ((result = pok_netobj_info_process_depth(info)) != pok_net_completed)
                break;
            if (info->depth[0] >= img->width) {
                ++info->depth[1];
                info->depth[0] = 0;
            }
        }
        result = pok_netobj_info_process(info);
    }
    return result;
}
