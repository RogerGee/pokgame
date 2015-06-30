/* graphics-GL.c - pokgame */

#ifdef POKGAME_WIN32

/* The Windows implementation of OpenGL does not have this functionality
   for texture mapping; so I work around it using another parameter which
   does the same thing in another way. */
#define GL_TEXTURE_BASE_LEVEL GL_TEXTURE_MAG_FILTER
#define GL_TEXTURE_MAX_LEVEL  GL_TEXTURE_MIN_FILTER
#define TEXPARAM GL_NEAREST

#else

#define TEXPARAM 0

#endif

void gl_init(
    int32_t viewWidth,
    int32_t viewHeight)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glTexEnvf(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
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
    glClearColor(BLACK_PIXEL_FLOAT[0],BLACK_PIXEL_FLOAT[1],BLACK_PIXEL_FLOAT[2],0.0);
}

void gl_create_textures(struct pok_graphics_subsystem* sys)
{
    int i;
    for (i = 0;i < sys->impl->texinfoCount;++i) {
        int j;
        GLuint* names = malloc(sizeof(GLuint) * sys->impl->texinfo[i].count);
        if (names == NULL)
            pok_error(pok_error_fatal,"memory allocation failure in gl_create_textures()");
        glGenTextures(sys->impl->texinfo[i].count,names);
        for (j = 0;j < sys->impl->texinfo[i].count;++j) {
            struct pok_image* img = sys->impl->texinfo[i].images[j];
            /* the images could be non-unique, in which case we could have
               already loaded it as a texture and unloaded its pixels */
            if (img->texref == 0) {
                size_t index;
                /* search collection for unused index */
                for (index = 0;index < sys->impl->textureCount;++index)
                    if (sys->impl->textureNames[index] == 0)
                        break;
                if (index >= sys->impl->textureCount) {
                    /* append texture name to collection */
                    if (sys->impl->textureCount >= sys->impl->textureAlloc) {
                        size_t nalloc;
                        void* ndata;
                        nalloc = sys->impl->textureAlloc << 1;
                        ndata = realloc(sys->impl->textureNames,nalloc * sizeof(GLuint));
                        if (ndata == NULL) {
                            pok_error(pok_error_warning,"could not allocate memory in gl_create_textures()");
                            return;
                        }
                        sys->impl->textureNames = ndata;
                        sys->impl->textureAlloc = nalloc;
                    }
                    index = sys->impl->textureCount++;
                }
                sys->impl->textureNames[index] = names[j];
                /* create texture object */
                glBindTexture(GL_TEXTURE_2D,names[j]);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_BASE_LEVEL,TEXPARAM);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,TEXPARAM);
                if (img->flags & pok_image_flag_alpha)
                    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,img->width,img->height,0,GL_RGBA,GL_UNSIGNED_BYTE,img->pixels.data);
                else
                    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,img->width,img->height,0,GL_RGB,GL_UNSIGNED_BYTE,img->pixels.data);
                /* assign the texture reference to the image */
                pok_image_unload(img);
                img->texref = names[j];
            }
        }
        free(names);
    }
}

void gl_delete_textures(struct pok_graphics_subsystem* sys,struct texture_info* info,int count)
{
    /* the 'sys->impl' object has an internal list of texture names; see if the specified images'
       texture references exist in the list; if so, then place the names in a temporary list that
       will be passed to 'glDeleteTextures'; replace texture names with 0 in the 'sys->impl' list;
       according to the documentation, a texture name of 0 is silently ignored */
    int i, j;
    for (i = 0;i < count;++i) {
        size_t index = 0;
        GLsizei cnt = 0;
        GLuint names[100];
        for (j = 0;j < info[i].count;++j) {
            struct pok_image* img = info[i].images[j];
            if (img->texref != 0) {
                size_t start = index;
                if (start < sys->impl->textureCount) {
                    do {
                        if (sys->impl->textureNames[index] == img->texref)
                            break;
                        ++index;
                        /* since texture names are normally allocated sequentially, then we let 'index'
                           remember its position in the array since it is likely that the next position
                           is the next texture name */
                        if (index >= sys->impl->textureCount)
                            index = 0;
                    } while (index != start);
                    if (index != start) {
                        if (cnt >= 100) {
                            glDeleteTextures(cnt,names);
                            cnt = 0;
                        }
                        names[cnt++] = img->texref;
                        sys->impl->textureNames[index] = 0;
                    }
                }
            }
        }
        glDeleteTextures(cnt,names);
    }
}

void pok_image_render(struct pok_image* img,int32_t x,int32_t y)
{
    int32_t X, Y;
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
    else if (img->texref == 0) {
        /* render a solid tile of a specified color; if the fill color is black, then don't render anything since
           the background will show through */
        if (img->fillref.r != BLACK_PIXEL.r || img->fillref.g != BLACK_PIXEL.g || img->fillref.b != BLACK_PIXEL.b) {
            X = x + img->width;
            Y = y + img->height;
            glColor3b(img->fillref.r,img->fillref.g,img->fillref.b);
            glBegin(GL_QUADS);
            {
                glVertex2i(x,y);
                glVertex2i(X,y);
                glVertex2i(X,Y);
                glVertex2i(x,Y);
            }
            glEnd();
        }
    }
    else {
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
