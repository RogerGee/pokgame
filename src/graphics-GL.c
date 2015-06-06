/* graphics-GL.c - pokgame */

void gl_init(
    float black[],
    uint32_t viewWidth,
    uint32_t viewHeight)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glPixelZoom(1,-1);
    glDrawBuffer(GL_BACK);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0,viewWidth,viewHeight,0,-1.0,1.0);
    /* set the modelview matrix to identity; this may be adjusted by any graphics routine
       during the game loop; assume that the routines play nice and reset it */
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glViewport(0,0,viewWidth,viewHeight);
    /* set clear color: we need this to be the black pixel */
    glClearColor(black[0],black[1],black[2],0);
}

void pok_image_render(struct pok_image* img,int32_t x,int32_t y)
{
    if (img->pixels.data != NULL) {
        /* raster graphic rendering routine implemented with OpenGL */
        if (x < 0 || y < 0) {
            /* hack around clipping restrictions with glRasterPos; we
               must negate 'y' since the vertical coordinates were flipped */
            glRasterPos2i(0,0);
            glBitmap(0,0,0,0,(GLfloat)x,(GLfloat)-y,NULL);
        }
        else
            glRasterPos2i(x,y);
        if (img->flags & pok_image_flag_alpha)
            glDrawPixels(img->width,img->height,GL_RGBA,GL_UNSIGNED_BYTE,img->pixels.data);
        else
            glDrawPixels(img->width,img->height,GL_RGB,GL_UNSIGNED_BYTE,img->pixels.data);
    }
    else {
        int32_t X, Y;
        X = x + img->width;
        Y = y + img->height;
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D,img->texref);
        glBegin(GL_QUADS);
        {
            glTexCoord2i(0,0);
            glVertex2i(x,y);

            glTexCoord2i(1,0);
            glVertex2i(X,y);

            glTexCoord2i(1,1);
            glVertex2i(X,Y);

            glTexCoord2i(0,1);
            glVertex2i(x,Y);
        }
        glEnd();
        glDisable(GL_TEXTURE_2D);
    }
}
