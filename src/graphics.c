/* graphics.c - pokgame */
#include "graphics.h"
#include "graphics-impl.h"
#include "error.h"
#include "protocol.h"
#include "opengl.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* the initial framerate */
#define INITIAL_FRAMERATE 60

/* define the black pixel used for the background (black tile); we don't want a true
   harsh black, but a 'lighter' black */
#define BLACK_COMPONENT 20
const union pixel BLACK_PIXEL = {{BLACK_COMPONENT, BLACK_COMPONENT, BLACK_COMPONENT}};
const float BLACK_PIXEL_FLOAT[3] = {BLACK_COMPONENT / (float)255.0, BLACK_COMPONENT / (float)255.0, BLACK_COMPONENT / (float)255.0};

/* define a structure to polymorphically represent each hook type */
struct pok_graphics_hook
{
    uint16_t top;
    graphics_routine_t routines[MAX_GRAPHICS_ROUTINES];
    void* contexts[MAX_GRAPHICS_ROUTINES];
};

void pok_graphics_hook_init(struct pok_graphics_hook* hook)
{
    int i;
    hook->top = 0;
    for (i = 0;i < MAX_GRAPHICS_ROUTINES;++i) {
        hook->routines[i] = NULL;
        hook->contexts[i] = NULL;
    }
}

struct pok_graphics_subsystem* pok_graphics_subsystem_new()
{
    struct pok_graphics_subsystem* sys;
    sys = malloc(sizeof(struct pok_graphics_subsystem));
    pok_graphics_subsystem_init(sys);
    return sys;
}
void pok_graphics_subsystem_free(struct pok_graphics_subsystem* sys)
{
    pok_graphics_subsystem_delete(sys);
    free(sys);
}
static void pok_graphics_subsystem_zeroset_parameters(struct pok_graphics_subsystem* sys)
{
    sys->dimension = 0;
    sys->windowSize.columns = 0;
    sys->windowSize.rows = 0;
    sys->playerLocation.column = 0;
    sys->playerLocation.row = 0;
    sys->_playerLocationInv.column = 0;
    sys->_playerLocationInv.row = 0;
    sys->playerOffsetX = 0;
    sys->playerOffsetY = 0;
    sys->wwidth = 0;
    sys->wheight = 0;
}
void pok_graphics_subsystem_init(struct pok_graphics_subsystem* sys)
{
    pok_graphics_subsystem_zeroset_parameters(sys);
    sys->routinetop = 0;
    sys->loadRoutine = NULL;
    sys->unloadRoutine = NULL;
    pok_graphics_hook_init((struct pok_graphics_hook*)&sys->keyupHook);
    pok_graphics_hook_init((struct pok_graphics_hook*)&sys->textentryHook);
    sys->blacktile = NULL;
    sys->impl = NULL;
    sys->framerate = INITIAL_FRAMERATE;
    sys->background = FALSE;
    pok_string_init(&sys->title);
}
void pok_graphics_subsystem_delete(struct pok_graphics_subsystem* sys)
{
    pok_graphics_subsystem_end(sys);
    pok_string_delete(&sys->title);
    if (sys->blacktile != NULL)
        pok_image_free(sys->blacktile);
}
static void pok_graphics_subsystem_after_assign(struct pok_graphics_subsystem* sys)
{
    /* computed inverted player location; these metrics are cached for the implementation
       so they do not have to be recomputed every time a rendering routine requires them;
       sys->playerLocation tells how many columns/rows are to the left/above the player; the
       sys->_playerLocationInv field tells how many columns/rows are to the right/below the player */
    sys->_playerLocationInv.column = sys->windowSize.columns - sys->playerLocation.column - 1;
    sys->_playerLocationInv.row = sys->windowSize.rows - sys->playerLocation.row - 1;
    sys->wwidth = sys->windowSize.columns * sys->dimension;
    sys->wheight = sys->windowSize.rows * sys->dimension;
}
void pok_graphics_subsystem_reset(struct pok_graphics_subsystem* sys)
{
    if (sys->impl != NULL)
        impl_free(sys);
    sys->routinetop = 0;
    sys->loadRoutine = NULL;
    sys->unloadRoutine = NULL;
    pok_graphics_hook_init((struct pok_graphics_hook*)&sys->keyupHook);
    pok_graphics_hook_init((struct pok_graphics_hook*)&sys->textentryHook);
    pok_graphics_subsystem_zeroset_parameters(sys);
    pok_string_assign(&sys->title,"pokgame: ");
    if (sys->blacktile != NULL) {
        pok_image_free(sys->blacktile);
        sys->blacktile = NULL;
    }
}
bool_t pok_graphics_subsystem_default(struct pok_graphics_subsystem* sys)
{
    /* apply default values to graphics parameters; these will be
       used by the default game upon startup */
    sys->dimension = DEFAULT_DIMENSION;
    sys->windowSize.columns = DEFAULT_WINDOW_WIDTH;
    sys->windowSize.rows = DEFAULT_WINDOW_HEIGHT;
    sys->playerLocation.column = DEFAULT_PLAYER_LOCATION_X;
    sys->playerLocation.row = DEFAULT_PLAYER_LOCATION_Y;
    sys->playerOffsetX = DEFAULT_PLAYER_OFFSET_X;
    sys->playerOffsetY = DEFAULT_PLAYER_OFFSET_Y;
    pok_graphics_subsystem_after_assign(sys);
    pok_graphics_subsystem_assign_title(sys,"default");
    if (sys->blacktile != NULL) {
        pok_image_free(sys->blacktile);
        sys->blacktile = NULL;
    }
    if ((sys->blacktile = pok_image_new_rgb_fillref(sys->dimension,sys->dimension,BLACK_PIXEL)) == NULL)
        return FALSE;
    return TRUE;
}
bool_t pok_graphics_subsystem_assign(struct pok_graphics_subsystem* sys,uint16_t dimension,uint16_t winCol,uint16_t winRow,
    uint16_t playerCol,uint16_t playerRow,uint16_t playerOffsetX,uint16_t playerOffsetY)
{
    /* check constraint on dimension */
    if (dimension > POK_MAX_DIMENSION) {
        pok_exception_new_ex(pok_ex_graphics,pok_ex_graphics_bad_dimension);
        return FALSE;
    }
    sys->dimension = dimension;
    /* check constraints on window size */
    if (winCol == 0 || winCol*dimension > MAX_WINDOW_PIXEL_WIDTH || winCol > POK_MAX_MAP_CHUNK_DIMENSION
        || winRow == 0 || winRow*dimension > MAX_WINDOW_PIXEL_WIDTH || winRow > POK_MAX_MAP_CHUNK_DIMENSION) {
        pok_exception_new_ex(pok_ex_graphics,pok_ex_graphics_bad_window_size);
        return FALSE;
    }
    sys->windowSize.columns = winCol;
    sys->windowSize.rows = winRow;
    /* check constraints on player location */
    if (playerCol >= winCol || playerRow >= winRow) {
        pok_exception_new_ex(pok_ex_graphics,pok_ex_graphics_bad_player_location);
        return FALSE;
    }
    sys->playerLocation.column = playerCol;
    sys->playerLocation.row = playerRow;
    /* check constraints on player offset */
    if (playerOffsetX >= dimension || playerOffsetY >= dimension) {
        pok_exception_new_ex(pok_ex_graphics,pok_ex_graphics_bad_player_offset);
        return FALSE;
    }
    sys->playerOffsetX = playerOffsetX;
    sys->playerOffsetY = playerOffsetY;
    pok_graphics_subsystem_after_assign(sys);
    if ((sys->blacktile = pok_image_new_rgb_fillref(sys->dimension,sys->dimension,BLACK_PIXEL)) == NULL)
        return FALSE;
    if (sys->impl != NULL)
        /* make changes to graphical frame */
        impl_reload(sys);
    return TRUE;
}
void pok_graphics_subsystem_assign_title(struct pok_graphics_subsystem* sys,const char* title)
{
    if (title == NULL || title[0] == 0)
        title = "unnamed";
    pok_string_assign(&sys->title,"pokgame: ");
    pok_string_concat(&sys->title,title);
    if (sys->impl != NULL)
        /* make changes to graphical frame */
        impl_reload(sys);
}
enum pok_network_result pok_graphics_subsystem_netread(struct pok_graphics_subsystem* sys,struct pok_data_source* dsrc,
    struct pok_netobj_readinfo* info)
{
    /* read graphics parameters from a data source:
        [2 bytes] tile/sprite dimension
        [2 bytes] window column count
        [2 bytes] window row count
        [2 bytes] player sprite location column offset
        [2 bytes] player sprite location row offset
        [2 bytes] player pixel offset X
        [2 bytes] player pixel offset Y
     */
    enum pok_network_result result = pok_net_already;
    switch (info->fieldProg) {
    case 0:
        pok_data_stream_read_uint16(dsrc,&sys->dimension);
        if ((result = pok_netobj_readinfo_process(info)) != pok_net_completed)
            break;
        if (sys->dimension<POK_MIN_DIMENSION || sys->dimension>POK_MAX_DIMENSION) {
            pok_exception_new_ex(pok_ex_graphics,pok_ex_graphics_bad_dimension);
            return pok_net_failed_protocol;
        }
        if ((sys->blacktile = pok_image_new_rgb_fill(sys->dimension,sys->dimension,BLACK_PIXEL)) == NULL)
            return pok_net_failed_internal; /* exception is inherited */
    case 1:
        pok_data_stream_read_uint16(dsrc,&sys->windowSize.columns);
        if ((result = pok_netobj_readinfo_process(info)) != pok_net_completed)
            break;
        if (sys->windowSize.columns==0 || sys->windowSize.columns*sys->dimension > MAX_WINDOW_PIXEL_WIDTH
                || sys->windowSize.columns > POK_MAX_MAP_CHUNK_DIMENSION) {
            pok_exception_new_ex(pok_ex_graphics,pok_ex_graphics_bad_window_size);
            return pok_net_failed_protocol;
        }
    case 2:
        pok_data_stream_read_uint16(dsrc,&sys->windowSize.rows);
        if ((result = pok_netobj_readinfo_process(info)) != pok_net_completed)
            break;
        if (sys->windowSize.rows==0 || sys->windowSize.rows*sys->dimension > MAX_WINDOW_PIXEL_HEIGHT
                || sys->windowSize.rows > POK_MAX_MAP_CHUNK_DIMENSION) {
            pok_exception_new_ex(pok_ex_graphics,pok_ex_graphics_bad_window_size);
            return pok_net_failed_protocol;
        }
    case 3:
        pok_data_stream_read_uint16(dsrc,&sys->playerLocation.column);
        if ((result = pok_netobj_readinfo_process(info)) != pok_net_completed)
            break;
        if (sys->playerLocation.column >= sys->windowSize.columns) {
            pok_exception_new_ex(pok_ex_graphics,pok_ex_graphics_bad_player_location);
            return pok_net_failed_protocol;
        }
    case 4:
        pok_data_stream_read_uint16(dsrc,&sys->playerLocation.row);
        if ((result = pok_netobj_readinfo_process(info)) != pok_net_completed)
            break;
        if (sys->playerLocation.row >= sys->windowSize.rows) {
            pok_exception_new_ex(pok_ex_graphics,pok_ex_graphics_bad_player_location);
            return pok_net_failed_protocol;
        }
    case 5:
        pok_data_stream_read_int16(dsrc,&sys->playerOffsetX);
        if ((result = pok_netobj_readinfo_process(info)) != pok_net_completed)
            break;
        if (sys->playerOffsetX >= sys->dimension) {
            pok_exception_new_ex(pok_ex_graphics,pok_ex_graphics_bad_player_offset);
            return pok_net_failed_protocol;
        }
    case 6:
        pok_data_stream_read_int16(dsrc,&sys->playerOffsetY);
        if ((result = pok_netobj_readinfo_process(info)) != pok_net_completed)
            break;
        if (sys->playerOffsetY >= sys->dimension) {
            pok_exception_new_ex(pok_ex_graphics,pok_ex_graphics_bad_player_offset);
            return pok_net_failed_protocol;
        }
        pok_graphics_subsystem_after_assign(sys);
        if (sys->impl != NULL)
            /* the graphics parameters changed, so reload the subsystem; this will
               cause the changes to take effect in the graphical frame */
            impl_reload(sys);
    }
    return result;
}
bool_t pok_graphics_subsystem_begin(struct pok_graphics_subsystem* sys)
{
    if (sys->impl == NULL) {
        if (!impl_new(sys))
            return FALSE;
        impl_map_window(sys);
        return TRUE;
    }
    pok_exception_new_ex(pok_ex_graphics,pok_ex_graphics_already_started);
    return FALSE;
}
bool_t pok_graphics_subsystem_create_textures(struct pok_graphics_subsystem* sys,int count, ...)
{
    if (count > 0) {
        int i;
        va_list list;
        struct texture_info* info;
        info = malloc(sizeof(struct texture_info) * count);
        if (info == NULL) {
            pok_exception_flag_memory_error();
            return FALSE;
        }
        va_start(list,count);
        for (i = 0;i < count;++i) {
            info[i].images = va_arg(list,struct pok_image**);
            info[i].count = va_arg(list,int);
        }
        /* give the implementation a chance to optimize raster graphics operations using textures;
           it may do nothing or may create the textures and free the image pixel data; the buffer of
           texture_info structures is deleted by the implementation */
        impl_load_textures(sys,info,count);
    }
    return TRUE;
}
bool_t pok_graphics_subsystem_delete_textures(struct pok_graphics_subsystem* sys,int count, ...)
{
    if (count > 0) {
        int i;
        va_list list;
        struct texture_info* info;
        info = malloc(sizeof(struct texture_info) * count);
        if (info == NULL) {
            pok_exception_flag_memory_error();
            return FALSE;
        }
        va_start(list,count);
        for (i = 0;i < count;++i) {
            info[i].images = va_arg(list,struct pok_image**);
            info[i].count = va_arg(list,int);
        }
        /* let the implementation delete the specified textures; since pokgame may load other
           artwork if a version sends it we need to be frugal with the video card memory space; the
           buffer of texture_info structures is deleted by the implementation */
        impl_delete_textures(sys,info,count);
    }
    return TRUE;
}
void pok_graphics_subsystem_game_render_state(struct pok_graphics_subsystem* sys,bool_t state)
{
    impl_set_game_state(sys,state);
}
void pok_graphics_subsystem_end(struct pok_graphics_subsystem* sys)
{
    if (sys->impl != NULL) {
        impl_free(sys);
        /*sys->impl = NULL;*/ /* performed by impl_free() */
    }
}
void pok_graphics_subsystem_register(struct pok_graphics_subsystem* sys,graphics_routine_t routine,void* context)
{
    size_t i = 0;
    impl_lock(sys);
    while (i<sys->routinetop && sys->routines[i]!=NULL)
        ++i;
    if (i < sys->routinetop) {
        sys->routines[i] = routine;
        sys->contexts[i] = context;
    }
    else if (i < MAX_GRAPHICS_ROUTINES) {
        sys->routines[sys->routinetop] = routine;
        sys->contexts[sys->routinetop++] = context;
    }
#ifdef POKGAME_DEBUG
    else
        pok_error(pok_error_fatal,"too many graphics routines");
#endif
    impl_unlock(sys);
}
void pok_graphics_subsystem_unregister(struct pok_graphics_subsystem* sys,graphics_routine_t routine,void* context)
{
    size_t i = 0;
    impl_lock(sys);
    while (i<sys->routinetop && sys->routines[i]!=routine && sys->contexts[i]!=context)
        ++i;
    if (i < sys->routinetop) {
        sys->routines[i] = NULL;
        sys->contexts[i] = NULL;
    }
    impl_unlock(sys);
}

/* OpenGL functionality for the graphics subsystem */

void gl_init(int32_t viewWidth,int32_t viewHeight)
{
    /* setup OpenGL to render pokgame according to the specified view dimensions */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glTexEnvf(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
    glPixelZoom(1,-1);
    glDrawBuffer(GL_BACK);
    glEnableClientState(GL_VERTEX_ARRAY);
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

void gl_create_textures(struct gl_texture_info* info,struct texture_info* texinfo,int count)
{
    /* create OpenGL texture objects from the specified images; a list of 'texture_info' structures is
       passed in; each contains a list of images to load as textures */
    int i;
    for (i = 0;i < count;++i) {
        int j;
        GLuint* names = malloc(sizeof(GLuint) * texinfo[i].count);
        if (names == NULL)
            pok_error(pok_error_fatal,"memory allocation failure in gl_create_textures()");
        glGenTextures(texinfo[i].count,names);
        for (j = 0;j < texinfo[i].count;++j) {
            struct pok_image* img = texinfo[i].images[j];
            /* the images could be non-unique, in which case we could have
               already loaded it as a texture and unloaded its pixels */
            if (img->texref == 0) {
                size_t index;
                /* search collection for unused index */
                for (index = 0;index < info->textureCount;++index)
                    if (info->textureNames[index] == 0)
                        break;
                if (index >= info->textureCount) {
                    /* append texture name to collection */
                    if (info->textureCount >= info->textureAlloc) {
                        size_t nalloc;
                        void* ndata;
                        nalloc = info->textureAlloc << 1;
                        ndata = realloc(info->textureNames,nalloc * sizeof(GLuint));
                        if (ndata == NULL) {
                            pok_error(pok_error_warning,"could not allocate memory in gl_create_textures()");
                            return;
                        }
                        info->textureNames = ndata;
                        info->textureAlloc = nalloc;
                    }
                    index = info->textureCount++;
                }
                info->textureNames[index] = names[j];
                /* create texture object */
                glBindTexture(GL_TEXTURE_2D,names[j]);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
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

void gl_delete_textures(struct gl_texture_info* existing,struct texture_info* info,int count)
{
    /* 'existing' is an internal list of texture names that are already loaded; see if the specified images'
       texture references exist in the list; if so, then place the names in a temporary list that
       will be passed to 'glDeleteTextures'; replace texture names with 0 in the existing list so the space;
       can be used again; according to the documentation, a texture name of 0 is silently ignored */
    int i, j;
    for (i = 0;i < count;++i) {
        size_t index = 0;
        GLsizei cnt = 0;
        GLuint names[100];
        for (j = 0;j < info[i].count;++j) {
            struct pok_image* img = info[i].images[j];
            if (img->texref != 0) {
                size_t start = index;
                if (start < existing->textureCount) {
                    do {
                        if (existing->textureNames[index] == img->texref)
                            break;
                        ++index;
                        /* since texture names are normally allocated sequentially, then we let 'index'
                           remember its position in the array since it is likely that the next position
                           is the next texture name */
                        if (index >= existing->textureCount)
                            index = 0;
                    } while (index != start);
                    if (index != start) {
                        if (cnt >= 100) {
                            glDeleteTextures(cnt,names);
                            cnt = 0;
                        }
                        names[cnt++] = img->texref;
                        existing->textureNames[index] = 0;
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
