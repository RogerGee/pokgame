/* graphics.c - pokgame */
#include "graphics.h"
#include <stdlib.h>

struct pok_graphics_subsystem* pok_graphics_subsystem_new()
{
    struct pok_graphics_subsystem* sys;
    sys = malloc(sizeof(struct pok_graphics_subsystem));

    return sys;
}
void pok_graphics_subsystem_free(struct pok_graphics_subsystem* sys)
{
    pok_graphics_subsystem_delete(sys);
    free(sys);
}
void pok_graphics_subsystem_default(struct pok_graphics_subsystem* sys)
{
    /* apply default values to graphics parameters; these will be
       used by the default game upon startup */
    sys->dimension = 32;
    sys->windowSize.columns = 9;
    sys->windowSize.rows = 10;
    sys->playerLocation.column = 4;
    sys->playerLocation.row = 4;
    sys->playerOffsetX = 0;
    sys->playerOffsetY = -8;

}
void pok_graphics_subsystem_delete(struct pok_graphics_subsystem* sys)
{
    void* p = &*sys;
}
