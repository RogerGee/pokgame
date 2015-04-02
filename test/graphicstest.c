#include "graphics.h"
#include "tile.h"
#include <stdio.h>
#include <string.h>

extern const char* POKGAME_NAME;
extern const char* HOME;

struct pok_image* get_tile_image();
void tile_test_routine(struct pok_graphics_subsystem* sys,struct pok_tile_manager* tman);

int graphics_main_test()
{
    char inbuf[1024];
    struct pok_image* tileimg;
    struct pok_tile_manager* tman;
    struct pok_graphics_subsystem* sys;

    sys = pok_graphics_subsystem_new();
    pok_graphics_subsystem_default(sys);

    tman = pok_tile_manager_new(sys);

    tileimg = get_tile_image();
    if (tileimg == NULL) {
        fprintf(stderr,"%s: couldn't load test tile image\n",POKGAME_NAME);
        return 1;
    }
    pok_tile_manager_load_tiles(tman,41,5,tileimg->pixels.data,TRUE);

    pok_graphics_subsystem_begin(sys);
    pok_graphics_subsystem_register(sys,(graphics_routine_t)tile_test_routine,tman);

    /* read messages from stdin */
    while (fgets(inbuf,sizeof(inbuf),stdin) != NULL) {
        size_t len = strlen(inbuf);
        if (inbuf[len-1] == '\n')
            inbuf[len-1] = 0;
        if (strcmp(inbuf,"quit")==0 || strcmp(inbuf,"exit")==0)
            break;

    }

    pok_tile_manager_free(tman);
    pok_image_free(tileimg);
    pok_graphics_subsystem_free(sys);

    return 0;
}

struct pok_image* get_tile_image()
{
    struct pok_string path;
    struct pok_image* img = pok_image_new();
    pok_string_init(&path);
    pok_string_assign(&path,HOME);
    pok_string_concat(&path,"/pictures/pokgame-img/sts1.data");

    if ( !pok_image_load_rgb_ex(img,path.buf,32,1312) ) {
        pok_image_free(img);
        img = NULL;
    }

    pok_string_delete(&path);
    return img;
}

void tile_test_routine(struct pok_graphics_subsystem* sys,struct pok_tile_manager* tman)
{
    pok_image_render(tman->tileset[1],0,0);
    pok_image_render(tman->tileset[3],sys->dimension,sys->dimension);
    pok_image_render(tman->tileset[4],sys->dimension,sys->dimension*2);
}
