#include "graphics.h"
#include "tile.h"
#include "map.h"
#include "error.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

extern const char* POKGAME_NAME;
extern const char* HOME;

static char* get_token(char** start,char delim);
static struct pok_image* get_tile_image(int no,int cols,int rows);
static struct pok_map* get_map(int no);

/* test routines */
static const int TEST_ROUT_TOP = 2;
static void replace_routine(struct pok_graphics_subsystem* sys,int this,int that,void** contexts);
static void routineA(struct pok_graphics_subsystem* sys,struct pok_tile_manager* tman);
static void routineB(struct pok_graphics_subsystem* sys,struct pok_map_render_context* context);
static graphics_routine_t routines[] = {
    (graphics_routine_t) routineA,
    (graphics_routine_t) pok_map_render
};

int graphics_main_test()
{
    int cycle, nxt;
    char inbuf[1024];
    struct pok_location loc;
    struct pok_map* map;
    struct pok_image* tileimg;
    struct pok_tile_manager* tman;
    struct pok_map_render_context* mcxt;
    struct pok_graphics_subsystem* sys;
    void* contexts[10];

    sys = pok_graphics_subsystem_new();
    pok_graphics_subsystem_default(sys);

    /* load up tiles for this test */
    tman = pok_tile_manager_new(sys);
    contexts[0] = tman;
    tileimg = get_tile_image(1,1,164);
    if (tileimg == NULL) {
        fprintf(stderr,"%s: couldn't load test tile image\n",POKGAME_NAME);
        return 1;
    }
    pok_tile_manager_load_tiles(tman,41,5,tileimg->pixels.data,TRUE);

    /* load up a test map */
    map =  get_map(1);
    mcxt = pok_map_render_context_new(map,tman);
    loc.column = 0;
    loc.row = 0;
    pok_map_render_context_center_on(mcxt,&loc);
    contexts[1] = mcxt;

    pok_graphics_subsystem_begin(sys);
    pok_graphics_subsystem_register(sys,routines[0],contexts[0]);

    /* read messages from stdin */
    cycle = nxt = 0;
    while (fgets(inbuf,sizeof(inbuf),stdin) != NULL) {
        size_t len = strlen(inbuf);
        char* s = inbuf;
        char* tok = get_token(&s,' ');
        if (inbuf[len-1] == '\n')
            inbuf[len-1] = 0;
        if (strcmp(tok,"quit")==0 || strcmp(tok,"exit")==0)
            break;
        /* handle routine cycling */
        if (strcmp(tok,"next") == 0) {
            ++nxt;
            if (nxt >= TEST_ROUT_TOP)
                nxt = 0;
            
        }
        else if (strcmp(tok,"prev") == 0) {
            --nxt;
            if (nxt < 0)
                nxt = TEST_ROUT_TOP-1;
        }
        if (nxt != cycle) {
            replace_routine(sys,cycle,nxt,contexts);
            cycle = nxt;
        }
    }

    pok_graphics_subsystem_end(sys);

    pok_map_render_context_free(mcxt);
    pok_map_free(map);
    pok_tile_manager_free(tman);
    pok_image_free(tileimg);
    pok_graphics_subsystem_free(sys);

    return 0;
}

char* get_token(char** start,char delim)
{
    char* s, *tmp;
    if (*start == 0)
        return NULL;
    s = tmp = *start;
    while (*s && *s!=delim)
        ++s;
    if (*s) {
        *s = 0;
        *start = s+1;
    }
    else
        *start = s;
    return tmp;
}

struct pok_image* get_tile_image(int no,int cols,int rows)
{
    char name[256];
    struct pok_image* img = pok_image_new();
    sprintf(name,"test/img/sts%d.data",no);

    if ( !pok_image_load_rgb_ex(img,name,32*cols,1312*rows) ) {
        pok_image_free(img);
        img = NULL;
    }

    return img;
}

struct pok_map* get_map(int no)
{
    char name[256];
    struct pok_netobj_readinfo info;
    struct pok_map* map = pok_map_new();
    struct pok_data_source* fstream;

    sprintf(name,"test/maps/testmap%d",no);
    fstream = pok_data_source_new_file(name,pok_filemode_open_existing,pok_iomode_read);
    assert(fstream != NULL);
    pok_netobj_readinfo_init(&info);
    if (pok_map_netread(map,fstream,&info) == pok_net_failed) {
        printf("failed to read map: %s\n",pok_exception_pop()->message);
        pok_map_free(map);
        pok_netobj_readinfo_delete(&info);
        pok_data_source_free(fstream);
        return NULL;
    }
    pok_netobj_readinfo_delete(&info);
    pok_data_source_free(fstream);

    return map;
}

void replace_routine(struct pok_graphics_subsystem* sys,int this,int that,void** contexts)
{
    pok_graphics_subsystem_unregister(sys,routines[this]);
    pok_graphics_subsystem_register(sys,routines[that],contexts[that]);
}

void routineA(struct pok_graphics_subsystem* sys,struct pok_tile_manager* tman)
{
    /* test tiles */
    pok_image_render(tman->tileset[1],0,0);
    pok_image_render(tman->tileset[3],sys->dimension,sys->dimension);
    pok_image_render(tman->tileset[4],sys->dimension,sys->dimension*2);
    pok_image_render(tman->tileset[20],sys->dimension*3,sys->dimension*3);
}
