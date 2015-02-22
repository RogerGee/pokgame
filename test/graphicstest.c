#include "graphics.h"
#include <stdio.h>
#include <string.h>

int graphics_main_test()
{
    char inbuf[1024];
    struct pok_graphics_subsystem* sys = pok_graphics_subsystem_new();
    pok_graphics_subsystem_default(sys);
    pok_graphics_subsystem_begin(sys);

    /* read messages from stdin */
    while (fgets(inbuf,sizeof(inbuf),stdin) != NULL) {
        size_t len = strlen(inbuf);
        if (inbuf[len-1] == '\n')
            inbuf[len-1] = 0;
        if (strcmp(inbuf,"quit")==0 || strcmp(inbuf,"exit")==0)
            break;

    }

    pok_graphics_subsystem_free(sys);
    return 0;
}
