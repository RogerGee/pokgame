#include <GL/gl.h>

/* raster graphic rendering routine implemented with OpenGL */

void pok_image_render(struct pok_image* img,uint32_t x,uint32_t y)
{
    glRasterPos2i(x,y);
    if (img->flags & pok_image_flag_alpha)
        glDrawPixels(img->width,img->height,GL_RGBA,GL_UNSIGNED_BYTE,img->pixels.data);
    else
        glDrawPixels(img->width,img->height,GL_RGB,GL_UNSIGNED_BYTE,img->pixels.data);
}
