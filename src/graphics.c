/* graphics.c - pokgame */
#include "graphics.h"
#include "error.h"
#include "protocol.h"
#include <stdlib.h>
#include <string.h>

/* implementation-specific api; we can assume that the implementation will run
   a graphical frame and other graphics operations on a separate thread; the
   below calls should guarentee concurrent access; mutual exclusion should be
   used when accessing data used by the implementation */
static bool_t impl_new(struct pok_graphics_subsystem* sys);
static void impl_free(struct pok_graphics_subsystem* sys);
static void impl_reload(struct pok_graphics_subsystem* sys);
static void impl_set_game_state(struct pok_graphics_subsystem* sys,bool_t state);
static void impl_map_window(struct pok_graphics_subsystem* sys);
static void impl_unmap_window(struct pok_graphics_subsystem* sys);
static void impl_lock(struct pok_graphics_subsystem* sys);
static void impl_unlock(struct pok_graphics_subsystem* sys);

/* include platform-dependent code */
#if defined(POKGAME_POSIX) && defined(POKGAME_X11) && defined(POKGAME_OPENGL)
#include "graphics-X-GL.c"
#elif defined(POKGAME_WIN32) && defined(POKGAME_OPENGL)
#include "graphics-win32-gl.c"
#endif

static const union pixel blackPixel = {{0,0,0}};
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
}
void pok_graphics_subsystem_init(struct pok_graphics_subsystem* sys)
{
    pok_graphics_subsystem_zeroset_parameters(sys);
    sys->routinetop = 0;
    sys->keyup = NULL;
    sys->blacktile = NULL;
    sys->impl = NULL;
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
    if ((sys->blacktile = pok_image_new_rgb_fill(sys->dimension,sys->dimension,blackPixel)) == NULL)
        return FALSE;
    return TRUE;
}
bool_t pok_graphics_subsystem_assign(struct pok_graphics_subsystem* sys,uint16_t dimension,uint16_t winCol,uint16_t winRow,
    uint16_t playerCol,uint16_t playerRow,uint16_t playerOffsetX,uint16_t playerOffsetY,const char* title)
{
    /* check constraint on dimension */
    if (dimension > MAX_DIMENSION) {
        pok_exception_new_ex(pok_ex_graphics,pok_ex_graphics_bad_dimension);
        return FALSE;
    }
    sys->dimension = dimension;
    /* check constraints on window size */
    if (winCol == 0 || winCol*dimension > MAX_WINDOW_PIXEL_WIDTH || winCol > MAX_MAP_CHUNK_DIMENSION
        || winRow == 0 || winRow*dimension > MAX_WINDOW_PIXEL_WIDTH || winRow > MAX_MAP_CHUNK_DIMENSION) {
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
    if ((sys->blacktile = pok_image_new_rgb_fill(sys->dimension,sys->dimension,blackPixel)) == NULL)
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
            if (sys->dimension<MIN_DIMENSION || sys->dimension>MAX_DIMENSION) {
                pok_exception_new_ex(pok_ex_graphics,pok_ex_graphics_bad_dimension);
                return pok_net_failed_protocol;
            }
            if ((sys->blacktile = pok_image_new_rgb_fill(sys->dimension,sys->dimension,blackPixel)) == NULL)
                return pok_net_failed_internal; /* exception is inherited */
        }
    }
    if (info->fieldProg == 1) {
        pok_data_stream_read_uint16(dsrc,&sys->windowSize.columns);
        result = pok_netobj_readinfo_process(info);
        if (result==pok_net_completed && (sys->windowSize.columns==0 || sys->windowSize.columns*sys->dimension > MAX_WINDOW_PIXEL_WIDTH
                || sys->windowSize.columns > MAX_MAP_CHUNK_DIMENSION)) {
            pok_exception_new_ex(pok_ex_graphics,pok_ex_graphics_bad_window_size);
            return pok_net_failed_protocol;
        }
    }
    if (info->fieldProg == 2) {
        pok_data_stream_read_uint16(dsrc,&sys->windowSize.rows);
        result = pok_netobj_readinfo_process(info);
        if (result==pok_net_completed && (sys->windowSize.rows==0 || sys->windowSize.rows*sys->dimension > MAX_WINDOW_PIXEL_HEIGHT
                || sys->windowSize.rows > MAX_MAP_CHUNK_DIMENSION)) {
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
        pok_data_stream_read_uint16(dsrc,&sys->playerOffsetX);
        result = pok_netobj_readinfo_process(info);
        if (result==pok_net_completed && sys->playerOffsetX>=sys->dimension) {
            pok_exception_new_ex(pok_ex_graphics,pok_ex_graphics_bad_player_offset);
            return pok_net_failed_protocol;
        }
    }
    if (info->fieldProg == 6) {
        pok_data_stream_read_uint16(dsrc,&sys->playerOffsetY);
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
void pok_graphics_subsystem_game_render_state(struct pok_graphics_subsystem* sys,bool_t state)
{
    impl_set_game_state(sys,state);
}
void pok_graphics_subsystem_end(struct pok_graphics_subsystem* sys)
{
    if (sys->impl != NULL)
        impl_free(sys);
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
void pok_graphics_subsystem_unregister(struct pok_graphics_subsystem* sys,graphics_routine_t routine)
{
    size_t i = 0;
    impl_lock(sys);
    while (i<sys->routinetop && sys->routines[i]!=routine)
        ++i;
    if (i < sys->routinetop) {
        sys->routines[i] = NULL;
        sys->contexts[i] = NULL;
    }
    impl_unlock(sys);
}
