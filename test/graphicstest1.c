/* graphicstest1.c - pokgame graphics test #1 */
#include "graphics.h"
#include "tileman.h"
#include "map-context.h"
#include "error.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <unistd.h>

extern const char* POKGAME_NAME;
extern const char* HOME;
extern void compute_chunk_render_info(struct pok_map_render_context* context,const struct pok_graphics_subsystem* sys);

static char* get_token(char** start,char delim);
struct pok_image* get_tile_image(int no); /* make these visable to other compilation units */
struct pok_map* get_map(int no);

/* test routines */
static const int TEST_ROUT_TOP = 2;
static void replace_routine(struct pok_graphics_subsystem* sys,int this,int that,void** contexts);
static void keyup_routine(enum pok_input_key key);
static void routineA(struct pok_graphics_subsystem* sys,struct pok_tile_manager* tman);
static void routineB(struct pok_graphics_subsystem* sys,struct pok_map_render_context* context);
static graphics_routine_t routines[] = {
    (graphics_routine_t) routineA,
    (graphics_routine_t) pok_map_render
};

/* global game objects for this test */
struct __
{
    struct pok_map_render_context* mcxt;
} globals;

int graphics_main_test1()
{
    int cycle, nxt;
    char inbuf[1024];
    struct pok_location loc;
    struct pok_point pnt;
    struct pok_map* map;
    struct pok_image* tileimg;
    struct pok_tile_manager* tman;
    struct pok_graphics_subsystem* sys;
    void* contexts[10];

    sys = pok_graphics_subsystem_new();
    pok_graphics_subsystem_default(sys);
    sys->keyup = keyup_routine;

    /* load up tiles for this test */
    tman = pok_tile_manager_new(sys);
    contexts[0] = tman;
    tileimg = get_tile_image(1);
    if (tileimg == NULL) {
        fprintf(stderr,"%s: couldn't load test tile image\n",POKGAME_NAME);
        return 1;
    }
    pok_tile_manager_load_tiles(tman,41,5,tileimg->pixels.data,TRUE);

    /* load up a test map */
    map =  get_map(1);
    if (map == NULL) {
        fprintf(stderr,"%s: couldn't read map\n",POKGAME_NAME);
        return 1;
    }
    globals.mcxt = pok_map_render_context_new(tman);
    loc.column = 5;
    loc.row = 5;
    pnt.X = 1;
    pnt.Y = 1;
    pok_map_render_context_set_map(globals.mcxt,map);
    assert( pok_map_render_context_center_on(globals.mcxt,&pnt,&loc) );
    contexts[1] = globals.mcxt;

    pok_graphics_subsystem_begin(sys);
    pok_graphics_subsystem_register(sys,routines[0],contexts[0]);

    /* read messages from stdin */
    cycle = nxt = 0;
    while (fgets(inbuf,sizeof(inbuf),stdin) != NULL) {
        int i, j;
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
        else if (strcmp(tok,"left") == 0)
            pok_map_render_context_move(globals.mcxt,pok_direction_left,FALSE);
        else if (strcmp(tok,"right") == 0)
            pok_map_render_context_move(globals.mcxt,pok_direction_right,FALSE);
        else if (strcmp(tok,"down") == 0)
            pok_map_render_context_move(globals.mcxt,pok_direction_down,FALSE);
        else if (strcmp(tok,"up") == 0)
            pok_map_render_context_move(globals.mcxt,pok_direction_up,FALSE);
        else if (strcmp(tok,"mapdebug") == 0) {
            struct pok_chunk_render_info info[4];
            compute_chunk_render_info(globals.mcxt,sys);
            printf("chunkSize{%d %d} focus{%d,%d} relpos{%d,%d} chunkpos{%d,%d} chunk{%d} viewing:\n",globals.mcxt->map->chunkSize.columns,
                globals.mcxt->map->chunkSize.rows,globals.mcxt->focus[0],globals.mcxt->focus[1],globals.mcxt->relpos.column,
                globals.mcxt->relpos.row,globals.mcxt->chunkpos.X,globals.mcxt->chunkpos.Y,globals.mcxt->map->chunk->data[0][0].data.tileid);
            for (j = 0;j < 3;++j) { /* rows */
                for (i = 0;i < 3;++i) /* columns */
                    printf("%d   ",globals.mcxt->viewingChunks[i][j]==NULL ? -1 : globals.mcxt->viewingChunks[i][j]->data[0][0].data.tileid);
                putchar('\n');
            }
            for (i = 0;i < 4;++i)
                if (info[i].chunk != NULL)
                    printf("px[%d] py[%d] across[%u] down[%u] loc{%d,%d} tile{%d}\n",info[i].px,info[i].py,info[i].across,info[i].down,
                        info[i].loc.column,info[i].loc.row,info[i].chunk->data[0][0].data.tileid);
        }
        else if (strcmp(tok,"keylisten") == 0) {
            puts("listening for keys");
            for (i = 0;i < 10;++i) {
                if (pok_graphics_subsystem_keyboard_query(sys,pok_input_key_ENTER,TRUE))
                    puts("ENTER");
                if (pok_graphics_subsystem_keyboard_query(sys,pok_input_key_ABUTTON,FALSE))
                    puts("A");
                if (pok_graphics_subsystem_keyboard_query(sys,pok_input_key_BBUTTON,FALSE))
                    puts("B");
                if (pok_graphics_subsystem_keyboard_query(sys,pok_input_key_DOWN,FALSE))
                    puts("DOWN");
                puts("---");
                sleep(1);
            }
        }
        else if (strcmp(tok,"offset") == 0) {
            if (globals.mcxt->offset[0] == -16)
                globals.mcxt->offset[0] = 0;
            else
                globals.mcxt->offset[0] = -16;
        }
        if (nxt != cycle) {
            replace_routine(sys,cycle,nxt,contexts);
            cycle = nxt;
        }
    }

    pok_graphics_subsystem_end(sys);

    pok_map_render_context_free(globals.mcxt);
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

struct pok_image* get_tile_image(int no)
{
    char name[256];
    struct pok_image* img = pok_image_new();
    sprintf(name,"test/img/sts%d.data",no);

    if ( !pok_image_fromfile_rgb(img,name) ) {
        const struct pok_exception* ex = pok_exception_pop();
        fprintf(stderr,"%s: error: failed to read image: id=%d kind=%d message=%s\n",POKGAME_NAME,ex->id,ex->kind,ex->message);
        pok_image_free(img);
        img = NULL;
    }

    return img;
}

struct pok_map* get_map(int no)
{
    char name[256];
    struct pok_map* map = pok_map_new();
    struct pok_data_source* fstream;

    sprintf(name,"test/maps/testmap%d",no);
    fstream = pok_data_source_new_file(name,pok_filemode_open_existing,pok_iomode_read);
    assert(fstream != NULL);
    if ( !pok_map_open(map,fstream) ) {
        const struct pok_exception* ex = pok_exception_pop();
        fprintf(stderr,"%s: error: failed to read map: id=%d kind=%d message=%s\n",POKGAME_NAME,ex->id,ex->kind,ex->message);
        pok_map_free(map);
        pok_data_source_free(fstream);
        return NULL;
    }
    pok_data_source_free(fstream);

    return map;
}

void replace_routine(struct pok_graphics_subsystem* sys,int this,int that,void** contexts)
{
    pok_graphics_subsystem_unregister(sys,routines[this],contexts[this]);
    pok_graphics_subsystem_register(sys,routines[that],contexts[that]);
}

void keyup_routine(enum pok_input_key key)
{
    /*printf("key up: %d\n",key);*/
    if (key == pok_input_key_UP)
        pok_map_render_context_move(globals.mcxt,pok_direction_up,FALSE);
    else if (key == pok_input_key_DOWN)
        pok_map_render_context_move(globals.mcxt,pok_direction_down,FALSE);
    else if (key == pok_input_key_LEFT)
        pok_map_render_context_move(globals.mcxt,pok_direction_left,FALSE);
    else if (key == pok_input_key_RIGHT)
        pok_map_render_context_move(globals.mcxt,pok_direction_right,FALSE);
}

void routineA(struct pok_graphics_subsystem* sys,struct pok_tile_manager* tman)
{
    /* test tiles */
    pok_image_render(tman->tileset[1],0,0);
    pok_image_render(tman->tileset[3],sys->dimension,sys->dimension);
    pok_image_render(tman->tileset[4],sys->dimension,sys->dimension*2);
    pok_image_render(tman->tileset[2],sys->dimension*3,sys->dimension*3);
    pok_image_render(tman->tileset[2],-sys->dimension/2,sys->dimension*2);
}
