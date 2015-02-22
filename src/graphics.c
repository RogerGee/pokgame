/* graphics.c - pokgame */
#include "graphics.h"
#include "error.h"
#include <stdlib.h>
#include <string.h>

/* implementation-specific api; we can assume that the implementation will run
   a graphical frame and other graphics operations on a separate thread; the
   below calls should guarentee concurrent access; mutual exclusion should be
   used when accessing data used by the implementation */
static bool_t impl_new(struct pok_graphics_subsystem* sys);
static void impl_free(struct pok_graphics_subsystem* sys);
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
void pok_graphics_subsystem_init(struct pok_graphics_subsystem* sys)
{
    sys->routinetop = 0;
    sys->impl = NULL;
}
void pok_graphics_subsystem_delete(struct pok_graphics_subsystem* sys)
{
    pok_graphics_subsystem_end(sys);
}
void pok_graphics_subsystem_default(struct pok_graphics_subsystem* sys)
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
}
enum pok_network_result pok_graphics_subsystem_netread(struct pok_graphics_subsystem* sys,struct pok_data_source* dsrc,
    struct pok_netobj_info* info)
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
    if (info->fieldProg == 0) {
        pok_data_stream_read_uint16(dsrc,&sys->dimension);
        result = pok_netobj_info_process(info);
    }
    if (info->fieldProg == 1) {
        pok_data_stream_read_uint16(dsrc,&sys->windowSize.columns);
        result = pok_netobj_info_process(info);
    }
    if (info->fieldProg == 2) {
        pok_data_stream_read_uint16(dsrc,&sys->windowSize.rows);
        result = pok_netobj_info_process(info);
    }
    if (info->fieldProg == 3) {
        pok_data_stream_read_uint16(dsrc,&sys->playerLocation.column);
        result = pok_netobj_info_process(info);
    }
    if (info->fieldProg == 4) {
        pok_data_stream_read_uint16(dsrc,&sys->playerLocation.row);
        result = pok_netobj_info_process(info);
    }
    if (info->fieldProg == 5) {
        pok_data_stream_read_uint16(dsrc,&sys->playerOffsetX);
        result = pok_netobj_info_process(info);
    }
    if (info->fieldProg == 6) {
        pok_data_stream_read_uint16(dsrc,&sys->playerOffsetY);
        result = pok_netobj_info_process(info);
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
void pok_graphics_subsystem_end(struct pok_graphics_subsystem* sys)
{
    if (sys->impl != NULL)
        impl_free(sys);
}
void pok_graphics_subsystem_register(struct pok_graphics_subsystem* sys,graphics_routine_t routine)
{
    size_t i = 0;
    impl_lock(sys);
    while (i<sys->routinetop && sys->routines[i]!=NULL)
        ++i;
    if (i < sys->routinetop)
        sys->routines[i] = routine;
    else if (i < MAX_GRAPHICS_ROUTINES)
        sys->routines[sys->routinetop++] = routine;
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
    if (i < sys->routinetop)
        sys->routines[i] = NULL;
    impl_unlock(sys);
}
