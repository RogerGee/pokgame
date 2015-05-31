/* pokgame/utils - spritemaker.c

    spritemaker builds sprite sheets iteratively from source images; a database is stored
    that associates each logical sprite set with an id; each sprite set contains 10 images,
    two for each of the four cardinal directions

    usage:
      spritemaker init <image-dimension>     // begin new spritemaker database (one must not preexist)
      spritemaker add <image-file> <cols> <rows>    // extract single sprite frame given column and row in source image; add
          <column> <row> <position[:sprite-set-id]> // the frame to the database using an optional id and required position
                                                    // note: if id is omitted, a new id number is allocated and printed out
      spritemaker compile <output-file> [rect|seq]      // compile sprite sheet from database
      spritemaker mv <position>:<old-sprite-set-id> <new-sprite-set-id> // move sprite frame to new position within database
      spritemaker rm <position>:<sprite-set-id> // remove sprite frame from database
*/
#include "lib.h"
#include <dstructs/dynarray.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

struct pixel_node
{
    uint8_t* pixel;
    unsigned char visited;
    struct pixel_node* adj[4];
};

/* sprites are offset from the standard tile images */
#define OFFSET -8

enum spritepos
{
    /* north (with 2 animation frames) */
    N,
    NA1,
    NA2,
    /* south (with 2 animation frames) */
    S,
    SA1,
    SA2,
    /* west (with 1 animation frame) */
    W,
    WA,
    /* east (with 1 animation frame) */
    E,
    EA
};
const char* spriteposStr[] = {
    "N",
    "NA1",
    "NA2",
    "S",
    "SA1",
    "SA2",
    "W",
    "WA",
    "E",
    "EA"
};

const char* programName;
static const char* const CONFIG_FILE = ".spritemaker.config";
static const char* const DBDIR = ".sprites";
static struct global_config
{
    int modified;
    size_t dimension;
    size_t topID;
} config;

/* load/unload configuration */
static void read_config();
static void write_config();

/* command handlers */
static void init(size_t dimension);
static void add(const char* file,int cols,int rows,int column,int row,int setID,int pos);
static void compile(const char* file,int format);
static void mv(int oldID,int pos,int newID);
static void rm(int setID,int pos);
static void ls();

/* database manipulation */
#define ENTER_DATABASE chdir(DBDIR)
#define EXIT_DATABASE chdir("..")
static void db_get_new_frame_id(char* name,int pos);
static int db_get_new_frame(char* name,int id,int pos);
static int db_get_frame(char* name,int id,int pos);
static int db_rename_frame(int fromID,int pos,int toID);
static int db_remove_frame(int id,int pos);

/* misc */
void erase_sprite(uint8_t* data);

int main(int argc,const char* argv[])
{
    int r;
    programName = argv[0];
    if (argc < 2) {
        fprintf(stderr,"%s: no arguments\n",argv[0]);
        return 1;
    }
    /* handle commands */
    argc -= 2;
    if (strcmp(argv[1],"init") == 0) {
        size_t dimension;
        if (argc < 1) {
            fprintf(stderr,"%s: init <dimension>\n",argv[0]);
            return 1;
        }
        dimension = atoi(argv[2]);
        if (dimension > 128 || dimension <= 0) {
            fprintf(stderr,"%s: init: dimension of %zu is invalid\n",argv[0],dimension);
            return 1;
        }
        init(dimension);
    }
    else if (strcmp(argv[1],"add") == 0) {
        int cols, rows, column, row, id, pos;
        char posStr[512];
        if (argc < 4) {
            fprintf(stderr,"%s: add <source-file> <column-count> <row-count> <column> <row> <POS[:ID]>\n",argv[0]);
            return 1;
        }
        cols = atoi(argv[3]);
        rows = atoi(argv[4]);
        column = atoi(argv[5]);
        row = atoi(argv[6]);
        r = sscanf(argv[7],"%[^:]:%d",posStr,&id);
        if (r == 1)
            id = -1;
        for (pos = 0;pos < 10;++pos)
            if (strcmp(spriteposStr[pos],posStr) == 0)
                break;
        if (pos >= 10) {
            fprintf(stderr,"%s: add: invalid position label: '%s'\n",argv[0],posStr);
            return 1;
        }
        read_config();
        add(argv[2],cols,rows,column,row,id,pos);
    }
    else if (strcmp(argv[1],"compile") == 0) {
        int option = COMPILED_RECTANGLE;
        if (argc < 1) {
            fprintf(stderr,"%s: compile <output-file> [rect|seq]\n",argv[0]);
            return 1;
        }
        if (argc >= 2) {
            if (strcmp(argv[3],"rect") == 0)
                option = COMPILED_RECTANGLE;
            else if (strcmp(argv[3],"seq") == 0)
                option = COMPILED_SEQUENTIAL;
            else {
                fprintf(stderr,"%s: compile: bad compile format: '%s'\n",argv[0],argv[3]);
                return 1;
            }
        }
        read_config();
        compile(argv[2],option);
    }
    else if (strcmp(argv[1],"mv") == 0) {
        int id, pos, newid;
        char posStr[512];
        if (argc < 2) {
            fprintf(stderr,"%s: mv <pos:id> <new-id>\n",argv[0]);
            return 1;
        }
        if (sscanf(argv[2],"%[^:]:%d",posStr,&id) != 2) {
            fprintf(stderr,"%s: mv: syntax error: '%s'\n",argv[0],argv[2]);
            return 1;
        }
        for (pos = 0;pos < 10;++pos)
            if (strcmp(spriteposStr[pos],posStr) == 0)
                break;
        if (pos >= 10) {
            fprintf(stderr,"%s: mv: invalid position label: '%s'\n",argv[0],posStr);
            return 1;
        }        
        newid = atoi(argv[3]);
        read_config();
        mv(id,pos,newid);
    }
    else if (strcmp(argv[1],"rm") == 0) {
        int id, pos;
        char posStr[512];
        if (argc < 1) {
            fprintf(stderr,"%s: rm <pos:id>\n",argv[0]);
            return 1;
        }
        if (sscanf(argv[2],"%[^:]:%d",posStr,&id) != 2) {
            fprintf(stderr,"%s: rm: syntax error: '%s'\n",argv[0],argv[2]);
            return 1;
        }
        for (pos = 0;pos < 10;++pos)
            if (strcmp(spriteposStr[pos],posStr) == 0)
                break;
        if (pos >= 10) {
            fprintf(stderr,"%s: rm: invalid position label: '%s'\n",argv[0],posStr);
            return 1;
        }
        read_config();
        rm(id,pos);
    }
    else if (strcmp(argv[1],"ls") == 0) {
        read_config();
        ls();
    }
    else {
        fprintf(stderr,"%s: unrecognized command '%s'\n",argv[0],argv[1]);
        return 1;
    }
    write_config();
    return 0;
}

void read_config()
{
    size_t i;
    FILE* fin;
    char buf[512];
    fin = fopen(CONFIG_FILE,"r");
    if (fin == NULL) {
        fprintf(stderr,"%s: cannot read config file: %s; is this a spritemaker directory?\n",programName,strerror(errno));
        exit(EXIT_FAILURE);
    }
    /* read 'dimension' field */
    if (fread(buf,sizeof(size_t),1,fin) != 1) {
        fprintf(stderr,"%s: fail fread()\n",programName);
        exit(EXIT_FAILURE);
    }
    config.dimension = 0;
    for (i = 0;i < sizeof(size_t);++i)
        config.dimension |= buf[i] << (8*i);
    /* read 'topID' field */
    if (fread(buf,sizeof(size_t),1,fin) != 1) {
        fprintf(stderr,"%s: fail fread()\n",programName);
        exit(EXIT_FAILURE);
    }
    config.topID = 0;
    for (i = 0;i < sizeof(size_t);++i)
        config.topID |= buf[i] << (8*i);
    config.modified = 0;
    fclose(fin);
}
void write_config()
{
    size_t i, iter;
    FILE* fout;
    char buf[512];
    if (config.modified) {
        fout = fopen(CONFIG_FILE,"w");
        if (fout == NULL) {
            fprintf(stderr,"%s: could not write to config file: %s\n",programName,strerror(errno));
            exit(EXIT_FAILURE);
        }
        iter = 0;
        /* write 'dimension' field */
        for (i = 0;i < sizeof(size_t);++i)
            buf[iter++] = (config.dimension >> (8*i)) & 0xff;
        /* write 'topID' field */
        for (i = 0;i < sizeof(size_t);++i)
            buf[iter++] = (config.topID >> (8*i)) & 0xff;
        fwrite(buf,iter,1,fout);
        fclose(fout);
    }
}

void init(size_t dimension)
{
    /* create database directory: it should not exist */
    if (mkdir(DBDIR,0777) == -1) {
        if (errno == EEXIST)
            fprintf(stderr,"%s: files exist; is this already a spritemaker directory?\n",programName);
        else
            fprintf(stderr,"%s: could not create database directory: %s\n",programName,strerror(errno));
        exit(EXIT_FAILURE);
    }
    config.dimension = dimension;
    config.topID = 1;
    config.modified = 1;
}

void add(const char* file,int cols,int rows,int column,int row,int setID,int pos)
{
    uint8_t* p;
    uint8_t* image;
    uint8_t* frame;
    char name[128];
    int r, s, t, u, iter;
    int width, height;
    /* validate coordinates */
    if (column<=0 || row<=0 || column >= cols || row >= rows) {
        fprintf(stderr,"%s: add: column and row do not exist within source image\n",programName);
        exit(EXIT_FAILURE);
    }
    /* allocate image buffers */
    width = config.dimension * cols;
    height = config.dimension * rows;
    image = malloc(width * height * 3); /* RGB */
    frame = malloc(config.dimension * config.dimension * 4); /* RGBA */
    if (image == NULL || frame == NULL) {
        fprintf(stderr,"%s: memory allocation fail\n",programName);
        exit(EXIT_FAILURE);
    }
    /* load source image */
    load_image_rgb(file,image,width,height,0);
    /* extract frame from image; add alpha information; erase background */
    p = image + 3 * ((int)config.dimension * (width * row + column) + width * OFFSET);
    for (r = 0,iter = 0;r < config.dimension;++r,p+=3*width) {
        for (s = 0,u = 0;s < config.dimension;++s) {
            /* read pixel */
            for (t = 0;t < 3;++t,++u)
                frame[iter++] = p[u];
            /* set alpha-component */
            frame[iter++] = 0xff;
        }
    }
    erase_sprite(frame);
    /* get database file name for frame image */
    ENTER_DATABASE;
    if (setID == -1) {
        /* create new frame */
        int id = config.topID;
        db_get_new_frame_id(name,pos);
        printf("created new sprite set with id=%d\n",id);
    }
    else if (!db_get_new_frame(name,setID,pos)) {
        fprintf(stderr,"%s: add: failed\n",programName);
        exit(EXIT_FAILURE);
    }
    /* save image */
    save_image_rgba(name,frame,config.dimension,config.dimension,1);
    EXIT_DATABASE;
    /* cleanup */
    free(image);
    free(frame);
}

static void compile_sequential(FILE* file,struct dynamic_array* array,size_t imgsize)
{
    size_t i;
    uint32_t height;
    /* write width and height (this image is for pokgame, so we add fields for width and height
       according to the 'pok_image' format */
    height = array->da_top * config.dimension;
    for (i = 0;i < 4;++i)
        fputc(((uint32_t)config.dimension >> (8*i)) & 0xff,file);
    for (i = 0;i < 4;++i)
        fputc((height >> (8*i)) & 0xff,file);
    /* write pixel information */
    for (i = 0;i < array->da_top;++i) {
        if (array->da_data[i] == NULL) {
            size_t j;
            for (j = 0;j < imgsize;++j)
                putc('\0',file);
        }
        else
            fwrite(array->da_data[i],1,imgsize,file);
    }
}

static void compile_rectangle(FILE* file,struct dynamic_array* array,size_t cnt)
{
    size_t i, j, k;
    size_t dimpx = config.dimension * 4;
    cnt *= 10;
    for (i = 0;i < cnt;i += 10) {
        for (j = 0;j < config.dimension;++j) {
            for (k = 0;k < 10;++k) {
                if (array->da_data[i + k] == NULL) {
                    size_t l;
                    for (l = 0;l < config.dimension;++l)
                        fwrite("\0\0\0\0",1,4,file);
                }
                else
                    fwrite(array->da_data[i + k]+j*dimpx,1,dimpx,file);
            }
        }
    }
}

void compile(const char* file,int format)
{
    FILE* f;
    size_t cnt, i, sz;
    struct dynamic_array array;
    dynamic_array_init(&array);
    /* grab all database images */
    ENTER_DATABASE;
    cnt = 0;
    sz = 4 * config.dimension * config.dimension;
    for (i = 1;i < config.topID;++i) {
        int j;
        short flag;
        char name[128];
        flag = 0;
        for (j = 0;j < 10;++j) {
            if ( db_get_frame(name,i,j) ) {
                uint8_t* pixels;
                flag = 1;
                pixels = malloc(sz);
                if (pixels == NULL) {
                    fprintf(stderr,"%s: memory allocation failure\n",programName);
                    exit(EXIT_FAILURE);
                }
                load_image_rgba(name,pixels,config.dimension,config.dimension,1);
                dynamic_array_pushback(&array,pixels);
            }
            else
                dynamic_array_pushback(&array,NULL);
        }
        if (!flag)
            array.da_top -= 10;
        else
            ++cnt;
    }
    EXIT_DATABASE;
    /* open output file and write compiled sprite sheet */
    f = fopen(file,"w");
    if (f == NULL) {
        fprintf(stderr,"%s: couldn't open output file: %s\n",programName,strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (format == COMPILED_SEQUENTIAL)
        compile_sequential(f,&array,sz);
    else
        compile_rectangle(f,&array,cnt);
    fclose(f);
    dynamic_array_delete_ex(&array,free);
}

void mv(int oldID,int pos,int newID)
{
    ENTER_DATABASE;
    if ( !db_rename_frame(oldID,pos,newID) )
        exit(EXIT_FAILURE);
    EXIT_DATABASE;
    puts("moved");
}

void rm(int setID,int pos)
{
    ENTER_DATABASE;
    if ( !db_remove_frame(setID,pos) )
        exit(EXIT_FAILURE);
    EXIT_DATABASE;
    puts("removed");
}

void ls()
{
    size_t i;
    char name[128];
    ENTER_DATABASE;
    for (i = 1;i < config.topID;++i) {
        int j;
        for (j = 0;j < 10;++j)
            if (db_get_frame(name,i,j))
                break;
        if (j < 10) {
            printf("%zu: ",i);
            for (j = 0;j < 10;++j) {
                if (db_get_frame(name,i,j))
                    fputs(spriteposStr[j],stdout);
                else
                    fputs("NONE",stdout);
                putchar('\t');
            }
            putchar('\n');
        }
    }
    EXIT_DATABASE;
}

void db_get_new_frame_id(char* name,int pos)
{
    sprintf(name,"%d_%d",(int)config.topID++,pos);
    config.modified = 1;
}
int db_get_new_frame(char* name,int id,int pos)
{
    sprintf(name,"%d_%d",id,pos);
    if (access(name,F_OK) == 0) {
        name[0] = 0;
        fprintf(stderr,"%s: frame with id=%d, pos=%s already exists: remove first\n",programName,id,spriteposStr[pos]);
        return 0;
    }
    return 1;
}
int db_get_frame(char* name,int id,int pos)
{
    sprintf(name,"%d_%d",id,pos);
    if (access(name,F_OK) == -1) {
        /* doesn't exist */
        name[0] = 0;
        return 0;
    }
    return 1;
}
int db_rename_frame(int fromID,int pos,int toID)
{
    char to[128];
    char from[128];
    sprintf(from,"%d_%d",fromID,pos);
    sprintf(to,"%d_%d",toID,pos);
    if (access(from,F_OK) == -1) {
        fprintf(stderr,"%s: source frame does not exist\n",programName);
        return 0;
    }
    if (access(to,F_OK) == 0) {
        fprintf(stderr,"%s: destination already exists: remove first\n",programName);
        return 0;
    }
    rename(from,to);
    return 1;
}
int db_remove_frame(int id,int pos)
{
    char name[128];
    sprintf(name,"%d_%d",id,pos);
    if (unlink(name) == -1) {
        fprintf(stderr,"%s: failed to remove image file: %s\n",programName,strerror(errno));
        return 0;
    }
    return 1;
}

static void pixel_traverse(struct pixel_node* node)
{
    /* search the graph of pixel nodes in a depth-first manner; do not explore pixels that are black;
       for every non-black pixel encountered, set its alpha component to fully transparent (invisible) */
    if (!node->visited) {
        node->visited = 1;
        if (node->pixel[0] != 0 || node->pixel[1] != 0 || node->pixel[2] != 0) {
            int i;
            node->pixel[3] = 0;
            for (i = 0;i < 4;++i)
                if (node->adj[i] != NULL)
                    pixel_traverse(node->adj[i]);
        }
    }
}

void erase_sprite(uint8_t* data)
{
    /* since a sprite is bounded by all-black pixels, we can interpret which pixels
       represent the background and set their alpha component to zero  */
    size_t i;
    size_t dim;
    size_t count;
    uint8_t* test;
    uint8_t white[4] = {0xff,0xff,0xff,0};
    struct pixel_node* pixels;
    test = data;
    /* allocate pixel node structures; increase its dimension by two in both directions */
    dim = config.dimension + 2;
    count = dim * dim;
    pixels = malloc(sizeof(struct pixel_node) * count);
    if (pixels == NULL) {
        fprintf(stderr,"%s: memory allocation failure\n",programName);
        exit(EXIT_FAILURE);
    }
    /* setup pixel data and adjacencies; we added to the image's dimensions so that the traversal algorithm
       can reach all areas of the image from its sides */
    for (i = 0;i < count;++i) {
        if (i < dim || (i+1) % dim == 0 || i % dim == 0 || count-i <= dim)
            pixels[i].pixel = white;
        else {
            pixels[i].pixel = data;
            data += 4;
        }
        pixels[i].visited = 0;
        pixels[i].adj[0] = i >= dim ? pixels + (i-dim) : NULL;
        pixels[i].adj[1] = i+dim < count ? pixels + (i+dim) : NULL;
        pixels[i].adj[2] = i == 0 ? NULL : pixels + (i-1);
        pixels[i].adj[3] = i+1 < count ? pixels + (i+1) : NULL;
    }
    pixel_traverse(pixels);
    free(pixels);
}
