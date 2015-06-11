/* graphics.c - pokgame */
#include "graphics.h"
#include "error.h"
#include "protocol.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* define the black pixel used for the background (black tile); we don't want a true
   harsh black, but a 'lighter' black */
#define BLACK_COMPONENT 20
const union pixel BLACK_PIXEL = {{BLACK_COMPONENT, BLACK_COMPONENT, BLACK_COMPONENT}};
const float BLACK_PIXEL_FLOAT[3] = {BLACK_COMPONENT / (float)255.0, BLACK_COMPONENT / (float)255.0, BLACK_COMPONENT / (float)255.0};

/* implementation-specific api; we can assume that the implementation will run
   a graphical frame and other graphics operations on a separate thread; the
   below calls should guarentee concurrent access; mutual exclusion should be
   used when accessing data used by the implementation */
struct texture_info
{
    int count;
    struct pok_image** images;
};
static bool_t impl_new(struct pok_graphics_subsystem* sys);
static void impl_free(struct pok_graphics_subsystem* sys);
static void impl_reload(struct pok_graphics_subsystem* sys);
static void impl_load_textures(struct pok_graphics_subsystem* sys,struct texture_info* info,int count);
static void impl_set_game_state(struct pok_graphics_subsystem* sys,bool_t state);
static void impl_map_window(struct pok_graphics_subsystem* sys);
static void impl_unmap_window(struct pok_graphics_subsystem* sys);
static void impl_lock(struct pok_graphics_subsystem* sys);
static void impl_unlock(struct pok_graphics_subsystem* sys);

/* include platform-dependent code */
#if defined(POKGAME_POSIX) && defined(POKGAME_X11) && defined(POKGAME_OPENGL)
#include "graphics-X-GL.c"
#elif defined(POKGAME_WIN32) && defined(POKGAME_OPENGL)
#include "graphics-win32-GL.c"
#elif defined(POKGAME_WIN32) && defined(POKGAME_WIN32_GDI)
#include "graphics-win32-gdi.c"
#endif

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
    sys->keyup = NULL;
    sys->blacktile = NULL;
    sys->impl = NULL;
    sys->framerate = INITIAL_FRAMERATE;
    pok_string_init(&sys->title);
    pok_string_assign(&sys->title,"pokgame: ");
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
    sys->keyup = NULL;
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
    pok_string_concat(&sys->title,"default");
    if ((sys->blacktile = pok_image_new_rgb_fill(sys->dimension,sys->dimension,BLACK_PIXEL)) == NULL)
        return FALSE;
    return TRUE;
}
bool_t pok_graphics_subsystem_assign(struct pok_graphics_subsystem* sys,uint16_t dimension,uint16_t winCol,uint16_t winRow,
    uint16_t playerCol,uint16_t playerRow,uint16_t playerOffsetX,uint16_t playerOffsetY,const char* title)
{
    /* check constraint on dimension */
    if (dimension > pok_max_dimension) {
        pok_exception_new_ex(pok_ex_graphics,pok_ex_graphics_bad_dimension);
        return FALSE;
    }
    sys->dimension = dimension;
    /* check constraints on window size */
    if (winCol == 0 || winCol*dimension > MAX_WINDOW_PIXEL_WIDTH || winCol > pok_max_map_chunk_dimension
        || winRow == 0 || winRow*dimension > MAX_WINDOW_PIXEL_WIDTH || winRow > pok_max_map_chunk_dimension) {
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
    pok_string_concat(&sys->title,title);
    if ((sys->blacktile = pok_image_new_rgb_fill(sys->dimension,sys->dimension,BLACK_PIXEL)) == NULL)
        return FALSE;
    return TRUE;
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
        [n bytes] game title string (null terminated)
     */
    enum pok_network_result result = pok_net_already;
    if (info->fieldProg == 0) {
        pok_data_stream_read_uint16(dsrc,&sys->dimension);
        result = pok_netobj_readinfo_process(info);
        if (result == pok_net_completed) {
            if (sys->dimension<pok_min_dimension || sys->dimension>pok_max_dimension) {
                pok_exception_new_ex(pok_ex_graphics,pok_ex_graphics_bad_dimension);
                return pok_net_failed_protocol;
            }
            if ((sys->blacktile = pok_image_new_rgb_fill(sys->dimension,sys->dimension,BLACK_PIXEL)) == NULL)
                return pok_net_failed_internal; /* exception is inherited */
        }
    }
    if (info->fieldProg == 1) {
        pok_data_stream_read_uint16(dsrc,&sys->windowSize.columns);
        result = pok_netobj_readinfo_process(info);
        if (result==pok_net_completed && (sys->windowSize.columns==0 || sys->windowSize.columns*sys->dimension > MAX_WINDOW_PIXEL_WIDTH
                || sys->windowSize.columns > pok_max_map_chunk_dimension)) {
            pok_exception_new_ex(pok_ex_graphics,pok_ex_graphics_bad_window_size);
            return pok_net_failed_protocol;
        }
    }
    if (info->fieldProg == 2) {
        pok_data_stream_read_uint16(dsrc,&sys->windowSize.rows);
        result = pok_netobj_readinfo_process(info);
        if (result==pok_net_completed && (sys->windowSize.rows==0 || sys->windowSize.rows*sys->dimension > MAX_WINDOW_PIXEL_HEIGHT
                || sys->windowSize.rows > pok_max_map_chunk_dimension)) {
            pok_exception_new_ex(pok_ex_graphics,pok_ex_graphics_bad_window_size);
            return pok_net_failed_protocol;
        }
    }
    if (info->fieldProg == 3) {
        pok_data_stream_read_uint16(dsrc,&sys->playerLocation.column);
        result = pok_netobj_readinfo_process(info);
        if (result==pok_net_completed && sys->playerLocation.column>=sys->windowSize.columns) {
            pok_exception_new_ex(pok_ex_graphics,pok_ex_graphics_bad_player_location);
            return pok_net_failed_protocol;
        }
    }
    if (info->fieldProg == 4) {
        pok_data_stream_read_uint16(dsrc,&sys->playerLocation.row);
        result = pok_netobj_readinfo_process(info);
        if (result==pok_net_completed && sys->playerLocation.row>=sys->windowSize.rows) {
            pok_exception_new_ex(pok_ex_graphics,pok_ex_graphics_bad_player_location);
            return pok_net_failed_protocol;
        }
    }
    if (info->fieldProg == 5) {
        pok_data_stream_read_int16(dsrc,&sys->playerOffsetX);
        result = pok_netobj_readinfo_process(info);
        if (result==pok_net_completed && sys->playerOffsetX>=sys->dimension) {
            pok_exception_new_ex(pok_ex_graphics,pok_ex_graphics_bad_player_offset);
            return pok_net_failed_protocol;
        }
    }
    if (info->fieldProg == 6) {
        pok_data_stream_read_int16(dsrc,&sys->playerOffsetY);
        result = pok_netobj_readinfo_process(info);
        if (result==pok_net_completed && sys->playerOffsetY>=sys->dimension) {
            pok_exception_new_ex(pok_ex_graphics,pok_ex_graphics_bad_player_offset);
            return pok_net_failed_protocol;
        }
    }
    if (info->fieldProg == 7) {
        pok_data_stream_read_string_ex(dsrc,&sys->title);
        result = pok_netobj_readinfo_process(info);
        if (result == pok_net_completed)
            pok_graphics_subsystem_after_assign(sys);
    }
    return result;
}
bool_t pok_graphics_subsystem_begin(struct pok_graphics_subsystem* sys)
{
    if (!impl_new(sys))
        return FALSE;
    impl_map_window(sys);
    return TRUE;
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
        /* give the implementation a chance to optimize raster graphics operations using textures; it
           may do nothing or may create the textures and free the image pixel data */
        impl_load_textures(sys,info,count);
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
